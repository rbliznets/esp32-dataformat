/*!
    \file
    \brief Класс для работы с SPIFFS.
    \authors Близнец Р.А. (r.bliznets@gmail.com)
    \version 0.1.0.0
    \date 12.12.2023
*/

#include "CSpiffsSystem.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include <cstdio>
#include <dirent.h>
#include <stdexcept>

static const char *TAG = "spiffs";

void CSpiffsSystem::init(bool check)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = nullptr,
        .max_files = 15,
        .format_if_mount_failed = true};
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        }
        else if (ret == ESP_ERR_NOT_FOUND)
        {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        }
        else
        {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return;
    }

    check |= endTransaction();
    if (check)
    {
        ESP_LOGI(TAG, "SPIFFS checking...");
        ret = esp_spiffs_check(conf.partition_label);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return;
        }
        else
        {
            ESP_LOGI(TAG, "SPIFFS_check() successful");
        }
    }

    esp_spiffs_gc(conf.partition_label, 0x100000);

    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        esp_spiffs_format(conf.partition_label);
        return;
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
}

void CSpiffsSystem::free()
{
    esp_vfs_spiffs_unregister(nullptr);
}

bool CSpiffsSystem::endTransaction()
{
    struct dirent *entry;
    DIR *dp;
    bool res = false;
    dp = opendir("/spiffs");
    if (dp == nullptr)
    {
        ESP_LOGE(TAG, "Failed to open dir /spiffs");
        res = true;
    }
    else
    {
        std::string str = "/spiffs/";
        while ((entry = readdir(dp)))
        {
            std::string fname = entry->d_name;
            if (fname[fname.length() - 1] == '$')
            {
                res = true;
                std::remove((str + fname).c_str());
                ESP_LOGW(TAG, "Delete %s", fname.c_str());
            }
            else if (fname[fname.length() - 1] == '!')
            {
                res = true;
                std::remove((str + fname.substr(0, fname.length() - 1)).c_str());
                std::rename((str + fname).c_str(), (str + fname.substr(0, fname.length() - 1)).c_str());
                ESP_LOGW(TAG, "Rename %s", fname.c_str());
            }
        }
        closedir(dp);
    }
    return res;
}

std::string CSpiffsSystem::command(CJsonParser *cmd)
{
    std::string answer = "";
    int t2;
    if (cmd->getObject(1, "spiffs", t2))
    {
        std::string fname;
        std::string fname2;
        if (cmd->getField(t2, "ls"))
        {
            answer = "\"spiffs\":{";
            struct dirent *entry;
            DIR *dp;
            dp = opendir("/spiffs");
            if (dp == nullptr)
            {
                ESP_LOGE(TAG, "Failed to open dir /spiffs");
                answer += "\"error\":\"Failed to open dir /spiffs\"";
            }
            else
            {
                std::string str = "/spiffs/";
                answer += "\"files\":[";
                bool point = false;
                while ((entry = readdir(dp)))
                {
                    FILE *f = std::fopen((str + entry->d_name).c_str(), "r");
                    int32_t sz = -1;
                    if (f != nullptr)
                    {
                        std::fseek(f, 0, SEEK_END);
                        sz = std::ftell(f);
                        std::fclose(f);
                    }
                    if (point)
                        answer += ',';
                    else
                        point = true;
                    answer = answer + "{\"name\":\"" + entry->d_name + "\",\"size\":" + std::to_string(sz) + "}";
                }
                closedir(dp);
                answer += ']';
            }
            answer += '}';
        }
        else if (cmd->getString(t2, "rd", fname))
        {
            answer = "\"spiffs\":{";
            std::string str = "/spiffs/" + fname;
            FILE *f = std::fopen(str.c_str(), "r");
            if (f == nullptr)
            {
                ESP_LOGW(TAG, "Failed to open file %s", fname.c_str());
                answer += "\"error\":\"Failed to open file " + fname + "\"";
            }
            else
            {
                answer += "\"fr\":\"" + fname + "\",";
                int offset = 0;
                cmd->getInt(t2, "offset", offset);
                answer += "\"offset\":" + std::to_string(offset) + ",\"data\":\"";
                int size = 96;
                cmd->getInt(t2, "size", size);
                uint8_t *data = new uint8_t[size];
                std::fseek(f, offset, SEEK_SET);
                size = std::fread(data, 1, size, f);
                std::fclose(f);
                char tmp[3];
                for (size_t i = 0; i < size; i++)
                {
                    std::sprintf(tmp, "%02x", data[i]);
                    answer += tmp;
                }
                delete[] data;
                answer += "\"";
            }
            answer += '}';
        }
        else if (cmd->getString(t2, "rm", fname))
        {
            answer = "\"spiffs\":{";
            std::string str = "/spiffs/" + fname;
            std::remove(str.c_str());
            answer += "\"fd\":\"" + fname + "\"}";
        }
        else if ((cmd->getString(t2, "old", fname)) && (cmd->getString(t2, "new", fname2)))
        {
            answer = "\"spiffs\":{";
            std::string str = "/spiffs/" + fname;
            std::string str2 = "/spiffs/" + fname2;
            if (std::rename(str.c_str(), str2.c_str()) != 0)
            {
                ESP_LOGW(TAG, "Failed to rename file %s to %s", fname.c_str(), fname2.c_str());
                answer += "\"error\":\"Failed to rename file " + fname + " to " + fname2 + "\"";
            }
            else
            {
                answer += "\"fold\":\"" + fname + "\",\"fnew\":\"" + fname2 + "\"";
            }
            answer += '}';
        }
        else if (cmd->getString(t2, "wr", fname))
        {
            answer = "\"spiffs\":{";
            std::string str = "/spiffs/" + fname;
            FILE *f = std::fopen(str.c_str(), "a");
            if (f == nullptr)
            {
                ESP_LOGW(TAG, "Failed to open file %s", fname.c_str());
                answer += "\"error\":\"Failed to open file " + fname + "\"";
            }
            else
            {
                int offset = 0;
                cmd->getInt(t2, "offset", offset);
                if (offset != std::ftell(f))
                {
                    ESP_LOGW(TAG, "Wrong offset of file %s(%d)", fname.c_str(), offset);
                    answer += "\"error\":\"Wrong offset of file " + fname + "\"";
                }
                else if (cmd->getString(t2, "data", str))
                {
                    int size = str.size() / 2;
                    uint8_t *data = new uint8_t[size];
                    try
                    {
                        for (size_t i = 0; i < size; i++)
                        {
                            fname2 = str.substr(i * 2, 2);
                            data[i] = std::stoi(fname2, 0, 16);
                        }
                        if (std::fwrite(data, 1, size, f) != size)
                        {
                            ESP_LOGW(TAG, "Failed to write to file %s(%d)", fname.c_str(), size);
                            answer += "\"error\":\"Failed to write to file " + fname + "\"";
                        }
                        else
                        {
                            answer += "\"fw\":\"" + fname + "\",";
                            answer += "\"offset\":" + std::to_string(offset) + ",\"size\":" + std::to_string(size);
                        }
                    }
                    catch (std::invalid_argument const &ex)
                    {
                        ESP_LOGW(TAG, "Failed to write to file %s(convert data)", fname.c_str());
                        answer += "\"error\":\"Failed to write to file  " + fname + "(convert data)\"";
                    }

                    delete[] data;
                }
                std::fclose(f);
            }
            answer += '}';
        }
    }
    return answer;
}
