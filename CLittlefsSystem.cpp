/*!
    \file
    \brief Class for working with LittleFS.
    \authors Bliznets R.A. (r.bliznets@gmail.com)
    \version 1.0.0.0
    \date 12.03.2026
    \details Implementation of methods for working with LittleFS file system on ESP32.
             Includes functions for initialization, file management and transactions.
*/

#include "CLittlefsSystem.h"
#include "CJsonReadStream.h"
#include "esp_littlefs.h"
#include "esp_log.h"
#include <cstdio>
#include <dirent.h>
#include <string.h>
#include <stdexcept>
#include <map>
#include <sys\stat.h>
#include "esp_task_wdt.h"
#include <filesystem>

static const char *TAG = "spiffs"; ///< Tag for logging

// Static list of LittleFS event handlers
// Stores pointers to functions that will be called at the beginning/end of write operations
// Used to notify external modules about file system state
std::list<onLittlefsWork *> CLittlefsSystem::mWriteQueue;

/*!
 * @brief Call all registered event handlers
 * @param lock true - start transaction (write), false - end transaction
 *
 * Sequentially calls each handler from mWriteQueue with the passed flag
 * Guarantees calling all unique handlers (without duplication)
 *
 * \note Method blocks access to file system until all calls are completed
 * \warning Should not be used inside event handlers
 */
void CLittlefsSystem::writeEvent(bool lock)
{
    for (auto const &event : mWriteQueue)
    {
        event(lock);
    }
}

/*!
 * @brief Register new event handler
 * @param event Pointer to handler function
 *
 * Adds function to mWriteQueue list only if it doesn't exist yet
 * Prevents handler duplication
 *
 * \note Uniqueness check is performed before adding
 * \warning Must ensure handler memory is properly freed
 */
void CLittlefsSystem::addWriteEvent(onLittlefsWork *event)
{
    // Check for handler presence in list before adding
    for (auto &e : mWriteQueue)
    {
        if (e == event)
        {
            return; // Handler already exists - exit
        }
    }
    mWriteQueue.push_back(event);
}

/*!
 * @brief Remove event handler
 * @param event Pointer to handler function
 *
 * Removes all occurrences of specified handler from mWriteQueue list
 * Uses safe removal with std::erase_if
 *
 * \note Method guarantees removal of all handler duplicates
 * \warning After removal, handler memory must be properly freed
 */
void CLittlefsSystem::removeWriteEvent(onLittlefsWork *event)
{
    // Remove all occurrences of handler from list
    std::erase_if(mWriteQueue, [event](const auto &item)
                  { return item == event; });
}

/*!
 * @brief Initialize LittleFS file system
 * @param check File system integrity check flag (true - check, false - don't check)
 * @return true - if initialization was successful, otherwise false
 *
 * Performs the following actions:
 * 1. Register LittleFS in Virtual File System (VFS)
 * 2. Check file system integrity (when check=true)
 * 3. Get and log partition information
 * 4. Restore file system if necessary
 *
 * \note When check=true performs long operation (may cause Watchdog timeout)
 * \warning Formatting file system will result in loss of all data
 */
bool CLittlefsSystem::init(bool check)
{
    // LittleFS mount configuration
    // - base_path: root directory for LittleFS access
    // - max_files: maximum number of simultaneously opened files
    // - format_if_mount_failed: automatic formatting on mount failure
    esp_vfs_littlefs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = nullptr,
        .format_if_mount_failed = true,
        .read_only = false,
        .dont_mount = false,
        .grow_on_mount = false};

    // Register LittleFS in VFS
    // Returns ESP_OK on successful registration
    esp_err_t ret = esp_vfs_littlefs_register(&conf);
    if (ret != ESP_OK)
    {
        // Handle mount errors
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find LittleFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize LittleFS (%s)", esp_err_to_name(ret));
        }
        return false;
    }

    // Additional transaction completion check
    check |= endTransaction(true);

    // Check file system integrity
    if (check)
    {
    }

    // Get partition information
    size_t total = 0, used = 0;
    ret = esp_littlefs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        // Warning about long operation when Watchdog is enabled
#ifdef CONFIG_ESP_TASK_WDT_INIT
        ESP_LOGW(TAG, "Long time operation, but WD is enabled.");
#endif
        // Attempt recovery through formatting
        ESP_LOGE(TAG, "Failed to get LittleFS partition information (%s). Formatting...", esp_err_to_name(ret));
        ret = esp_littlefs_format(conf.partition_label);
        return (ret == ESP_OK);
    }
    else
    {
        // Log partition size
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return true;
}

/*!
 * @brief Free LittleFS resources
 *
 * Performs the following actions:
 * 1. Unregister LittleFS from VFS system
 * 2. Free associated resources
 *
 * \note After calling this method, access to file system is impossible
 * \warning Must ensure all files are closed before calling
 */
void CLittlefsSystem::free()
{
    // Unregister LittleFS
    // nullptr indicates unregistration of all partitions
    esp_vfs_littlefs_unregister(nullptr);
}

// Вспомогательная функция для рекурсивного обхода
void CLittlefsSystem::processDirectory(const std::string &path, bool log, bool isRecovery)
{
    DIR *dp = opendir(path.c_str());
    if (dp == nullptr)
        return;

    struct dirent *entry;
    while ((entry = readdir(dp)))
    {
        std::string name = entry->d_name;
        if (name == "." || name == "..")
            continue;

        std::string fullPath = path + "/" + name;

        // Проверяем, является ли объект директорией
        struct stat st;
        if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
        {
            // Рекурсивный вход в папку
            processDirectory(fullPath, log, isRecovery);
            continue;
        }

        // Логика обработки файлов (ваша исходная логика)
        size_t len = name.length();
        if (len < 2)
            continue;

        char lastChar = name[len - 1];
        std::string baseNamePath = path + "/" + name.substr(0, len - 1);

        if (isRecovery)
        {
            // Режим восстановления: просто удаляем временные файлы $ и !
            if (lastChar == '$' || lastChar == '!')
            {
                if (log)
                    ESP_LOGW(TAG, "Recovery: remove %s", fullPath.c_str());
                std::remove(fullPath.c_str());
            }
        }
        else
        {
            // Активная транзакция
            if (lastChar == '!')
            {
                if (log)
                    ESP_LOGI(TAG, "Finalizing !: remove %s and %s", baseNamePath.c_str(), fullPath.c_str());
                std::remove(baseNamePath.c_str());
                std::remove(fullPath.c_str());
            }
            else if (lastChar == '$')
            {
                if (log)
                    ESP_LOGI(TAG, "Finalizing $: rename %s to %s", fullPath.c_str(), baseNamePath.c_str());
                std::remove(baseNamePath.c_str()); // Удаляем старый оригинал
                std::rename(fullPath.c_str(), baseNamePath.c_str());
            }
        }
    }
    closedir(dp);
}

bool CLittlefsSystem::endTransaction(bool log)
{
    const char *root = "/spiffs";
    const char *marker = "/spiffs/$";
    bool res = false;

    FILE *f = std::fopen(marker, "r");
    if (f == nullptr)
    {
        // Режим восстановления (маркера нет, чистим мусор)
        processDirectory(root, log, true);
        res = true;
    }
    else
    {
        // Активная транзакция
        std::fclose(f);

        // 1 проход: обрабатываем удаление (!)
        // 2 проход: обрабатываем переименование ($)
        // В рекурсивном исполнении проще сделать один проход,
        // так как LittleFS атомарно переименовывает файлы.
        processDirectory(root, log, false);

        if (log)
            ESP_LOGI(TAG, "Transaction complete, removing marker");
        std::remove(marker);
        res = true;
    }

    return res;
}

/*!
 * @brief Write buffer data to file
 * @param fileName File name for writing
 * @param data Pointer to data for writing
 * @param size Data size in bytes
 * @return true - if write was successful, otherwise false
 *
 * Performs atomic write operation with file system locking:
 * 1. Notify event handlers about start of transaction
 * 2. Open file in append mode
 * 3. Perform data write
 * 4. Notify event handlers about end of transaction
 *
 * \note File opens in "a" mode for appending
 * \warning Doesn't guarantee atomicity on power failures
 */
bool CLittlefsSystem::writeBuffer(const char *fileName, uint8_t *data, uint32_t size)
{
    // Notify handlers about start of operation
    writeEvent(true);

    // Open file for appending
    FILE *f = std::fopen(fileName, "a");
    if (f == nullptr)
    {
        // Notify handlers about end of operation
        writeEvent(false);
        ESP_LOGE(TAG, "Failed to open file %s", fileName);
        return false;
    }
    else
    {
        // Write data to file
        if (std::fwrite(data, 1, size, f) != size)
        {
            std::fclose(f);
            writeEvent(false);
            ESP_LOGE(TAG, "Failed to write to file %s(%ld)", fileName, size);
            return false;
        }
        else
        {
            std::fclose(f);
            // Notify handlers about end of operation
            writeEvent(false);
            return true;
        }
    }
}

/**
 * @brief Clears content of files in specified directory (at end of transaction)
 *
 * Function opens all files in directory and marks them for erasure at transaction end.
 * Files with extensions '!' and '$' are ignored (considered service files).
 *
 * @param dirName Path to directory whose file content needs to be cleared
 * @return uint16_t Number of successfully cleared files
 */
uint16_t CLittlefsSystem::clearDir(const char *dirName)
{
    uint16_t res = 0;     // Counter for successfully cleared files
    struct dirent *entry; // Pointer to directory entry
    DIR *dp;              // Pointer to directory descriptor

    // writeEvent(true); // Signal start of write operation

    dp = opendir(dirName); // Open directory for reading
    if (dp != nullptr)
    {
        std::list<std::string> transFiles;
        while ((entry = readdir(dp)))
        {
            std::string str = entry->d_name;
            if (str == "." || str == "..")
                continue;

            std::string fullPath = dirName;
            fullPath += "/" + str;
            // Проверяем, является ли объект директорией
            struct stat st;
            if (stat(fullPath.c_str(), &st) == 0 && S_ISDIR(st.st_mode))
            {
                continue;
            }

            if ((str.length() > 1) && (str[str.length() - 1] == '$'))
            {
                transFiles.push_back(str.substr(0, str.length() - 1));
            }
        }
        closedir(dp);
        dp = opendir(dirName);
        while ((entry = readdir(dp)))
        {
            std::string str = entry->d_name;
            auto it = std::find(transFiles.begin(), transFiles.end(), str);
            if (it != transFiles.end())
                continue;

            // Check that file name is longer than 1 character and doesn't end with '!' or '$'
            if ((str.length() > 1) && (str[str.length() - 1] != '!') && (str[str.length() - 1] != '$'))
            {
                std::string fname = dirName;
                fname += "/" + str + "!";
                FILE *f = std::fopen(fname.c_str(), "w"); // Open file in write mode (truncates file to 0 bytes)
                if (f != nullptr)
                {
                    std::fclose(f); // Close file
                    res++;          // Increment cleared file counter
                }
            }
        }
        closedir(dp); // Close directory after reading
    }

    // writeEvent(false); // Signal end of write operation
    return res; // Return number of successfully cleared files
}

/*!
 * @brief Process JSON commands for LittleFS
 * @param cmd JSON object with command at file system root
 * @param[out] answer json with response.
 *
 * Processes the following commands:
 * - ls: Get list of files in directory
 * - rd: Read file content
 * - rm: Delete file
 * - trans: Transaction management (end/cancel)
 * - old/new: Rename file
 * - wr: Append data to file
 * - ct: Create file with text
 * - at: Append text to end of file
 * - rt: Read text file
 *
 * \note All file operations are wrapped in writeEvent() for synchronization
 */
void CLittlefsSystem::command(json &cmd, json &answer)
{
    if (cmd.contains("spiffs") && cmd["spiffs"].is_object())
    {
        answer["spiffs"] = json::object();

        // Command to get list of files in directory
        if (cmd["spiffs"].contains("ls") && cmd["spiffs"]["ls"].is_string())
        {
            std::string fname = cmd["spiffs"]["ls"].template get<std::string>();
            std::string fname2 = "/spiffs";
            answer["spiffs"]["root"] = fname;

            int offset = 0;
            int count = -1;
            // Get pagination parameters
            if (cmd["spiffs"].contains("offset") && cmd["spiffs"]["offset"].is_number_unsigned())
            {
                offset = cmd["spiffs"]["offset"].template get<int>();
            }
            if (cmd["spiffs"].contains("count") && cmd["spiffs"]["count"].is_number_unsigned())
            {
                count = cmd["spiffs"]["count"].template get<int>();
            }

            // Form complete path
            if (fname != "")
                fname2 += "/" + fname;

            struct dirent *entry;
            DIR *dp;
            std::map<std::string, uint32_t> dirs;
            writeEvent(true); // Lock file system

            // Open directory
            dp = opendir(fname2.c_str());
            if (dp == nullptr)
            {
                writeEvent(false);
                ESP_LOGE(TAG, "Failed to open dir %s", fname2.c_str());
                answer["spiffs"]["error"] = "Failed to open dir " + fname2;
            }
            else
            {
                fname2 += "/";
                answer["spiffs"]["files"] = json::array();

                // Scan directory contents
                while ((entry = readdir(dp)))
                {
                    char *result;
                    if ((result = std::strchr(entry->d_name, '/')) == nullptr)
                    {
                        offset--;
                        if (offset < 0)
                        {
                            if (count != 0)
                            {
                                count--;
                                struct stat buf;
                                int res = stat((fname2 + entry->d_name).c_str(), &buf);
                                int32_t sz = -1;
                                if (res == 0)
                                {
                                    sz = buf.st_size;
                                }
                                json fl;
                                fl["name"] = entry->d_name;
                                fl["size"] = sz;
#if (CONFIG_SPIFFS_USE_MTIME == 1)
                                char tmp[32];
                                strftime(tmp, 32, "%Y.%m.%d %H:%M:%S", localtime(&buf.st_mtime));
                                fl["modify"] = tmp;
#endif
                                answer["spiffs"]["files"].push_back(fl);
                            }
                        }
                    }
                    else
                    {
                        *result = 0;
                        fname = std::string(entry->d_name);
                        if (!dirs.contains(fname))
                        {
                            dirs[fname] = 1;
                        }
                        else
                        {
                            dirs[fname]++;
                        }
                    }
                }
                closedir(dp);
                writeEvent(false); // Unlock file system

                // Add information about subdirectories
                for (const auto &[key, value] : dirs)
                {
                    offset--;
                    if (offset < 0)
                    {
                        if (count != 0)
                        {
                            count--;
                            json fl;
                            fl["name"] = key;
                            fl["count"] = value;
                            answer["spiffs"]["files"].push_back(fl);
                        }
                        else
                            break;
                    }
                }
            }
        }
        // Command to read binary file
        else if (cmd["spiffs"].contains("rd") && cmd["spiffs"]["rd"].is_string())
        {
            std::string fname = cmd["spiffs"]["rd"].template get<std::string>();
            std::string str = "/spiffs/" + fname;
            FILE *f = std::fopen(str.c_str(), "r");

            if (f == nullptr)
            {
                ESP_LOGW(TAG, "Failed to open file %s", fname.c_str());
                answer["spiffs"]["error"] = "Failed to open file " + fname;
            }
            else
            {
                answer["spiffs"]["fr"] = fname;
                int offset = 0;
                // Get read parameters
                if (cmd["spiffs"].contains("offset") && cmd["spiffs"]["offset"].is_number_unsigned())
                {
                    offset = cmd["spiffs"]["offset"].template get<int>();
                }
                if (offset != 0)
                {
                    answer["spiffs"]["offset"] = offset;
                }
                int size = CONFIG_DATAFORMAT_DEFAULT_DATA_SIZE / 2;
                if (cmd["spiffs"].contains("size") && cmd["spiffs"]["size"].is_number_unsigned())
                {
                    size = cmd["spiffs"]["size"].template get<int>();
                }

                uint8_t *data = new uint8_t[size];
                str = "";

                // Read data from specified offset
                std::fseek(f, offset, SEEK_SET);
                size = std::fread(data, 1, size, f);
                std::fclose(f);

                // Convert binary data to HEX string
                char tmp[4];
                for (size_t i = 0; i < size; i++)
                {
                    std::sprintf(tmp, "%02x", data[i]);
                    str += tmp;
                }
                delete[] data;
                answer["spiffs"]["data"] = str;
            }
        }
        // Command to delete file
        else if (cmd["spiffs"].contains("rm") && cmd["spiffs"]["rm"].is_string())
        {
            std::string fname = cmd["spiffs"]["rm"].template get<std::string>();
            std::string str = "/spiffs/" + fname;
            writeEvent(true);

            if (std::filesystem::exists(str.c_str()))
            {
                std::remove(str.c_str());
            }
            else
            {
                answer["spiffs"]["warning"] = "File do not exist";
            }
            writeEvent(false);
            answer["spiffs"]["fd"] = fname;
        }
        // Command for transaction management
        else if (cmd["spiffs"].contains("trans") && cmd["spiffs"]["trans"].is_string())
        {
            std::string fname = cmd["spiffs"]["trans"].template get<std::string>();

            if (fname == "end")
            {
                writeEvent(true);
                if (cmd["spiffs"].contains("clear") && cmd["spiffs"]["clear"].is_array())
                {
                    for (auto &[key, val] : cmd["spiffs"]["clear"].items())
                    {
                        if (val.is_string())
                        {
                            std::string str = "/spiffs/";
                            str += val.template get<std::string>();
                            clearDir(str.c_str());
                        }
                    }
                }
                FILE *f = std::fopen("/spiffs/$", "w");
                std::fclose(f);
                endTransaction();
                writeEvent(false);
                answer["spiffs"]["trans"] = "end";
            }
            else if (fname == "cancel")
            {
                writeEvent(true);
                std::remove("/spiffs/$");
                endTransaction();
                writeEvent(false);
                answer["spiffs"]["trans"] = "cancel";
            }
            else
            {
                answer["spiffs"]["error"] = "Wrong transaction command: " + fname;
            }
        }
        // Command to rename file
        else if (cmd["spiffs"].contains("old") && cmd["spiffs"]["old"].is_string() && cmd["spiffs"].contains("new") && cmd["spiffs"]["new"].is_string())
        {
            std::string fname = cmd["spiffs"]["old"].template get<std::string>();
            std::string fname2 = cmd["spiffs"]["new"].template get<std::string>();
            std::string str = "/spiffs/" + fname;
            std::string str2 = "/spiffs/" + fname2;
            writeEvent(true);

            if (std::filesystem::exists(str.c_str()))
            {
                if (std::filesystem::exists(str2.c_str()))
                {
                    std::remove(str2.c_str());
                    if (std::rename(str.c_str(), str2.c_str()) != 0)
                    {
                        writeEvent(false);
                        ESP_LOGW(TAG, "Failed to rename file %s to %s", fname.c_str(), fname2.c_str());
                        answer["spiffs"]["error"] = "Failed to rename file " + fname + " to " + fname2;
                    }
                    else
                    {
                        writeEvent(false);
                        answer["spiffs"]["fold"] = fname;
                        answer["spiffs"]["fnew"] = fname2;
                    }
                }
                else
                {
                    if (std::rename(str.c_str(), str2.c_str()) != 0)
                    {
                        writeEvent(false);
                        ESP_LOGW(TAG, "Failed to rename file %s to %s", fname.c_str(), fname2.c_str());
                        answer["spiffs"]["error"] = "Failed to rename file " + fname + " to " + fname2;
                    }
                    else
                    {
                        writeEvent(false);
                        answer["spiffs"]["fold"] = fname;
                        answer["spiffs"]["fnew"] = fname2;
                    }
                }
            }
            else if (std::filesystem::exists(str2.c_str()))
            {
                writeEvent(false);
                answer["spiffs"]["warning"] = "Old file do not exist";
                answer["spiffs"]["fnew"] = fname2;
            }
            else
            {
                writeEvent(false);
                answer["spiffs"]["error"] = "Failed to rename file " + fname + " to " + fname2;
            }
        }
        // Command to write binary data to file
        else if (cmd["spiffs"].contains("wr") && cmd["spiffs"]["wr"].is_string())
        {
            std::string fname = cmd["spiffs"]["wr"].template get<std::string>();
            std::string str = "/spiffs/" + fname;
            if (cmd["spiffs"].contains("data") && cmd["spiffs"]["data"].is_string())
            {
                std::string hexString = cmd["spiffs"]["data"].template get<std::string>();
                std::vector<uint8_t> data(hexString.length() / 2);

                // Convert HEX string to binary data
                for (size_t i = 0; i < hexString.length(); i += 2)
                {
                    std::string byteStr = hexString.substr(i, 2);
                    try
                    {
                        // Convert the two-character hex string to an integer with base 16
                        uint8_t byteValue = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
                        data[i >> 1] = byteValue;
                    }
                    catch (const std::invalid_argument &e)
                    {
                        answer["spiffs"]["error"] = "Invalid hex character in string: " + byteStr;
                        return;
                    }
                    catch (const std::out_of_range &e)
                    {
                        answer["spiffs"]["error"] = "Hex value out of range for uint8_t: " + byteStr;
                        return;
                    }
                }
                writeEvent(true);
                FILE *f = std::fopen(str.c_str(), "a");

                if (f == nullptr)
                {
                    writeEvent(false);
                    ESP_LOGW(TAG, "Failed to open file %s", fname.c_str());
                    answer["spiffs"]["error"] = "Failed to open file " + fname;
                }
                else
                {
                    int offset = 0;
                    long fsize = std::ftell(f);
                    // Get write parameters
                    if (cmd["spiffs"].contains("offset") && cmd["spiffs"]["offset"].is_number_unsigned())
                    {
                        offset = cmd["spiffs"]["offset"].template get<int>();
                    }

                    // Truncate file if necessary
                    if (offset < fsize)
                    {
                        std::fclose(f);
                        truncate(str.c_str(), offset);
                        f = std::fopen(str.c_str(), "a");
                        fsize = offset;
                        answer["spiffs"]["rewrite"] = true;
                    }

                    // Check offset correctness
                    if (offset != fsize)
                    {
                        ESP_LOGW(TAG, "Wrong offset of file %s(%d)", fname.c_str(), offset);
                        answer["spiffs"]["error"] = "Wrong offset of file " + fname + "(" + std::to_string(fsize) + ")";
                    }
                    else
                    {
                        // Write data to file
                        if (std::fwrite(data.data(), 1, data.size(), f) != data.size())
                        {
                            ESP_LOGW(TAG, "Failed to write to file %s(%d)", fname.c_str(), data.size());
                            answer["spiffs"]["error"] = "Failed to write to file " + fname;
                        }
                        else
                        {
                            answer["spiffs"]["fw"] = fname;
                            if (offset != 0)
                            {
                                answer["spiffs"]["offset"] = offset;
                            }
                            answer["spiffs"]["size"] = data.size();
                        }
                    }
                    std::fclose(f);
                    writeEvent(false);
                }
            }
            else
            {
                answer["spiffs"]["error"] = "No data to write for " + fname;
            }
        }
        // Command to create text file
        else if (cmd["spiffs"].contains("ct") && cmd["spiffs"]["ct"].is_string() && cmd["spiffs"].contains("text") && cmd["spiffs"]["text"].is_string())
        {
            std::string fname = cmd["spiffs"]["ct"].template get<std::string>();
            std::string fname2 = cmd["spiffs"]["text"].template get<std::string>();
            std::string str = "/spiffs/" + fname;
            writeEvent(true);
            FILE *f = std::fopen(str.c_str(), "w");

            if (f == nullptr)
            {
                writeEvent(false);
                ESP_LOGW(TAG, "Failed to open file %s", fname.c_str());
                answer["spiffs"]["error"] = "Failed to open file " + fname;
            }
            else
            {
                // Write text to file
                if (std::fwrite(fname2.c_str(), 1, fname2.length(), f) != fname2.length())
                {
                    ESP_LOGW(TAG, "Failed to write to file %s(%d)", fname.c_str(), fname2.length());
                    answer["spiffs"]["error"] = "Failed to write to file " + fname;
                }
                else
                {
                    answer["spiffs"]["tc"] = fname;
                    answer["spiffs"]["size"] = std::to_string(std::ftell(f));
                }
                std::fclose(f);
                writeEvent(false);
            }
        }
        // Command to append text to end of file
        else if (cmd["spiffs"].contains("at") && cmd["spiffs"]["at"].is_string() && cmd["spiffs"].contains("text") && cmd["spiffs"]["text"].is_string())
        {
            std::string fname = cmd["spiffs"]["at"].template get<std::string>();
            std::string fname2 = cmd["spiffs"]["text"].template get<std::string>();
            std::string str = "/spiffs/" + fname;
            writeEvent(true);
            FILE *f = std::fopen(str.c_str(), "a");

            if (f == nullptr)
            {
                writeEvent(false);
                ESP_LOGW(TAG, "Failed to open file %s", fname.c_str());
                answer["spiffs"]["error"] = "Failed to open file " + fname;
            }
            else
            {
                // Append text to end of file
                if (std::fwrite(fname2.c_str(), 1, fname2.length(), f) != fname2.length())
                {
                    ESP_LOGW(TAG, "Failed to write to file %s(%d)", fname.c_str(), fname2.length());
                    answer["spiffs"]["error"] = "Failed to write to file " + fname;
                }
                else
                {
                    answer["spiffs"]["ta"] = fname;
                    answer["spiffs"]["size"] = std::to_string(std::ftell(f));
                }
                std::fclose(f);
                writeEvent(false);
            }
        }
        // Command to read text file
        else if (cmd["spiffs"].contains("rt") && cmd["spiffs"]["rt"].is_string())
        {
            std::string fname = cmd["spiffs"]["rt"].template get<std::string>();
            std::string str = "/spiffs/" + fname;
            FILE *f = std::fopen(str.c_str(), "r");

            if (f == nullptr)
            {
                ESP_LOGW(TAG, "Failed to open file %s", fname.c_str());
                answer["spiffs"]["error"] = "Failed to open file " + fname;
            }
            else
            {
                answer["spiffs"]["tr"] = fname;
                int offset = 0;
                // Get read parameters
                if (cmd["spiffs"].contains("offset") && cmd["spiffs"]["offset"].is_number_unsigned())
                {
                    offset = cmd["spiffs"]["offset"].template get<int>();
                }
                if (offset != 0)
                {
                    answer["spiffs"]["offset"] = offset;
                }
                int size = CONFIG_DATAFORMAT_DEFAULT_DATA_SIZE;
                if (cmd["spiffs"].contains("size") && cmd["spiffs"]["size"].is_number_unsigned())
                {
                    size = cmd["spiffs"]["size"].template get<int>();
                }
                uint8_t *data = new uint8_t[size + 1];
                // Read text from specified offset
                std::fseek(f, offset, SEEK_SET);
                size = std::fread(data, 1, size, f);
                std::fclose(f);
                data[size] = 0;
                std::string str((const char *)data);
                delete[] data;

                answer["spiffs"]["text"] = str;
            }
        }
        else if (cmd["spiffs"].contains("gc") && cmd["spiffs"]["gc"].is_number_unsigned())
        {
            esp_err_t err;
            size_t total_bytes;
            size_t used_bytes;
            err = esp_littlefs_info(nullptr, &total_bytes, &used_bytes);
            if (err == ESP_OK)
            {
                answer["spiffs"]["total"] = total_bytes;
                answer["spiffs"]["used"] = used_bytes;
            }
        }
    }
}