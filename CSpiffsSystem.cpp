/*!
    \file
    \brief Класс для работы с SPIFFS.
    \authors Близнец Р.А. (r.bliznets@gmail.com)
    \version 1.3.1.0
    \date 12.12.2023
    \details Реализация методов для работы с файловой системой SPIFFS на ESP32.
             Включает функции инициализации, управления файлами и транзакциями.
*/

#include "CSpiffsSystem.h"
#include "esp_spiffs.h"
#include "esp_log.h"
#include <cstdio>
#include <dirent.h>
#include <string.h>
#include <stdexcept>
#include <map>
#include <sys\stat.h>
#include "esp_task_wdt.h"

static const char *TAG = "spiffs"; ///< Тег для логирования

std::list<onSpiffsWork *> CSpiffsSystem::mWriteQueue;

void CSpiffsSystem::writeEvent(bool lock)
{
    for (auto const &event : mWriteQueue)
    {
        event(lock);
    }
}

void CSpiffsSystem::addWriteEvent(onSpiffsWork *event)
{
    mWriteQueue.push_back(event);
}

void CSpiffsSystem::removeWriteEvent(onSpiffsWork *event)
{
    std::erase_if(mWriteQueue, [event](const auto &item)
                  { return item == event; });
}

/*!
    \brief Инициализация файловой системы SPIFFS
    \param check Если true, выполняется проверка целостности файловой системы
    \return true при успешной инициализации, false при ошибке
    \details Монтирует раздел SPIFFS, при необходимости форматирует.
             Проверяет состояние раздела и собирает мусор при необходимости.
*/
bool CSpiffsSystem::init(bool check)
{
    // Конфигурация параметров монтирования
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = nullptr,
        .max_files = 15,
        .format_if_mount_failed = true};

    // Попытка регистрации SPIFFS в VFS
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        // Обработка различных ошибок монтирования
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
        return false;
    }

    check |= endTransaction();

    // Проверка целостности файловой системы при необходимости
    if (check)
    {
#ifdef CONFIG_ESP_TASK_WDT_INIT
        ESP_LOGW(TAG, "Long time operation, but WD is enabled.");
#endif
        ESP_LOGI(TAG, "SPIFFS checking...");
        ret = esp_spiffs_check(conf.partition_label);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return false;
        }
        else
        {
            ESP_LOGI(TAG, "SPIFFS_check() successful");
            esp_spiffs_gc(conf.partition_label, 100000); // Сборка мусора
        }
    }

    // Получение информации о разделе
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        // Попытка восстановления через форматирование
#ifdef CONFIG_ESP_TASK_WDT_INIT
        ESP_LOGW(TAG, "Long time operation, but WD is enabled.");
#endif
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        ret = esp_spiffs_format(conf.partition_label);
        return (ret == ESP_OK);
    }
    else
    {
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return true;
}

/*!
    \brief Освобождение ресурсов SPIFFS
    \details Удаляет мьютекс и отмонтирует файловую систему
*/
void CSpiffsSystem::free()
{
    esp_vfs_spiffs_unregister(nullptr);
}

/*!
    \brief Завершение транзакции с файловой системой
    \return true если требуется повторная проверка файловой системы
    \details Обрабатывает специальные файлы транзакций ($ - временные, ! - для переименования).
             Восстанавливает целостность после прерванных операций. Алгоритм:
             1. При отсутствии маркера $ удаляет временные файлы и завершает отложенные переименования
             2. При наличии $ и ! выполняет полную очистку следов транзакций
             3. При наличии $ без ! инициирует восстановление прерванной операции
*/
bool CSpiffsSystem::endTransaction()
{
    struct dirent *entry;
    DIR *dp;
    bool res = false;

    // Открываем корневую директорию SPIFFS
    dp = opendir("/spiffs");
    if (dp == nullptr)
    {
        ESP_LOGE(TAG, "Failed to open dir /spiffs");
        res = true; // Требуется проверка ФС
    }
    else
    {
        std::string str = "/spiffs/";

        // Проверка существования маркера активной транзакции
        FILE *f = std::fopen("/spiffs/$", "r");
        if (f == nullptr)
        {
            // Режим восстановления: обрабатываем оставшиеся артефакты
            while ((entry = readdir(dp)))
            {
                std::string fname = entry->d_name;

                // Удаление временных файлов (оканчиваются на $)
                if (fname[fname.length() - 1] == '$')
                {
                    res = true; // Файловая система модифицировалась
                    std::remove((str + fname).c_str());
                }
                // Завершение отложенного переименования (оканчиваются на !)
                else if (fname[fname.length() - 1] == '!')
                {
                    res = true;
                    // Удаление оригинального файла перед переименованием
                    std::remove((str + fname.substr(0, fname.length() - 1)).c_str());
                    // Переименование файла с ! в оригинальное имя
                    std::rename((str + fname).c_str(),
                                (str + fname.substr(0, fname.length() - 1)).c_str());
                }
            }
            closedir(dp);
        }
        else
        {
            // Активная транзакция обнаружена (существует маркер $)
            std::fclose(f);
            f = std::fopen("/spiffs/!", "r");

            if (f != nullptr)
            {
                // Аварийное завершение: удаляем все следы транзакции
                std::fclose(f);
                while ((entry = readdir(dp)))
                {
                    std::string fname = entry->d_name;
                    // Удаление всех временных и промежуточных файлов
                    if ((fname.length() > 1) &&
                        ((fname[fname.length() - 1] == '$') ||
                         (fname[fname.length() - 1] == '!')))
                    {
                        std::remove((str + fname).c_str());
                    }
                }
                closedir(dp);
                // Удаление системных маркеров транзакции
                std::remove("/spiffs/!");
                std::remove("/spiffs/$");
                res = true;
            }
            else
            {
                // Начало процедуры восстановления транзакции
                f = std::fopen("/spiffs/!", "w");
                std::fclose(f);

                // Этап 1: преобразование $-файлов в !-файлы
                while ((entry = readdir(dp)))
                {
                    std::string fname = entry->d_name;
                    if ((fname.length() > 1) &&
                        (fname[fname.length() - 1] == '$'))
                    {
                        // Заменяем $ на ! в имени файла
                        std::string fname2 = fname;
                        fname2[fname2.length() - 1] = '!';

                        // Атомарное переименование с обработкой ошибок
                        if (std::rename(fname.c_str(), fname2.c_str()) != 0)
                        {
                            ESP_LOGE(TAG, "Failed to rename file %s to %s",
                                     fname.c_str(), fname2.c_str());
                            res = true;
                            break;
                        }
                    }
                }
                closedir(dp);

                // Рекурсивный вызов для обработки новых !-файлов
                if (res)
                {
                    endTransaction(); // Повторная попытка
                }
                else
                {
                    // Финализация: удаление маркеров транзакции
                    std::remove("/spiffs/!");
                    std::remove("/spiffs/$");
                    res = endTransaction(); // Окончательная проверка
                }
            }
        }
    }
    return res; // true если были изменения, требующие проверки
}

std::string CSpiffsSystem::command(CJsonParser *cmd)
{
    std::string answer = "";
    char tmp[32];

    int t2;
    if (cmd->getObject(1, "spiffs", t2))
    {
        std::string fname;
        std::string fname2;
        if (cmd->getString(t2, "ls", fname))
        {
            answer = "\"spiffs\":{";
            answer += "\"root\":\"" + fname + "\"";
            fname2 = "/spiffs";
            int offset = 0;
            int count = -1;
            cmd->getInt(t2, "offset", offset);
            cmd->getInt(t2, "count", count);
            if (fname != "")
                fname2 += "/" + fname;
            struct dirent *entry;
            DIR *dp;
            std::map<std::string, uint32_t> dirs;
            dp = opendir(fname2.c_str());
            if (dp == nullptr)
            {
                ESP_LOGE(TAG, "Failed to open dir %s", fname2.c_str());
                answer += ",\"error\":\"Failed to open dir " + fname2 + "\"";
            }
            else
            {
                fname2 += "/";
                answer += ",\"files\":[";
                bool point = false;
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
                                if (point)
                                    answer += ',';
                                else
                                    point = true;
#if (CONFIG_SPIFFS_USE_MTIME == 1)
                                strftime(tmp, 32, "%Y.%m.%d %H:%M:%S", localtime(&buf.st_mtime));
                                answer = answer + "{\"name\":\"" + entry->d_name + "\",\"size\":" + std::to_string(sz) + ",\"modify\":\"" + tmp + "\"}";
#else
                                answer = answer + "{\"name\":\"" + entry->d_name + "\",\"size\":" + std::to_string(sz) + "}";
#endif
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
                for (const auto &[key, value] : dirs)
                {
                    offset--;
                    if (offset < 0)
                    {
                        if (count != 0)
                        {
                            count--;
                            if (point)
                                answer += ',';
                            else
                                point = true;
                            answer = answer + "{\"name\":\"" + key + "\",\"count\":" + std::to_string(value) + "}";
                        }
                        else
                            break;
                    }
                }
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
                if (offset != 0)
                {
                    answer += "\"offset\":" + std::to_string(offset) + ",";
                }
                answer += "\"data\":\"";
                int size = CONFIG_DATAFORMAT_DEFAULT_DATA_SIZE / 2;
                cmd->getInt(t2, "size", size);
                uint8_t *data = new uint8_t[size];
                std::fseek(f, offset, SEEK_SET);
                size = std::fread(data, 1, size, f);
                std::fclose(f);
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
            writeEvent(true);
            std::remove(str.c_str());
            writeEvent(false);
            answer += "\"fd\":\"" + fname + "\"}";
        }
        else if (cmd->getString(t2, "trans", fname))
        {
            answer = "\"spiffs\":{";
            if (fname == "end")
            {
                writeEvent(true);
                FILE *f = std::fopen("/spiffs/$", "w");
                std::fclose(f);
                endTransaction();
                writeEvent(false);
                answer += "\"trans\":\"end\"";
            }
            else if (fname == "cancel")
            {
                writeEvent(true);
                endTransaction();
                writeEvent(false);
                answer += "\"trans\":\"cancel\"";
            }
            else
            {
                answer += "\"error\":\"Wrong transaction command: " + fname + "\"";
            }
            answer += '}';
        }
        else if ((cmd->getString(t2, "old", fname)) && (cmd->getString(t2, "new", fname2)))
        {
            answer = "\"spiffs\":{";
            std::string str = "/spiffs/" + fname;
            std::string str2 = "/spiffs/" + fname2;
            writeEvent(true);
            std::remove(str2.c_str());
            if (std::rename(str.c_str(), str2.c_str()) != 0)
            {
                writeEvent(false);
                ESP_LOGW(TAG, "Failed to rename file %s to %s", fname.c_str(), fname2.c_str());
                answer += "\"error\":\"Failed to rename file " + fname + " to " + fname2 + "\"";
            }
            else
            {
                writeEvent(false);
                answer += "\"fold\":\"" + fname + "\",\"fnew\":\"" + fname2 + "\"";
            }
            answer += '}';
        }
        else if (cmd->getString(t2, "wr", fname))
        {
            answer = "\"spiffs\":{";
            std::string str = "/spiffs/" + fname;
            writeEvent(true);
            FILE *f = std::fopen(str.c_str(), "a");
            if (f == nullptr)
            {
                writeEvent(false);
                ESP_LOGW(TAG, "Failed to open file %s", fname.c_str());
                answer += "\"error\":\"Failed to open file " + fname + "\"";
            }
            else
            {
                int offset = 0;
                long fsize = std::ftell(f);
                std::vector<uint8_t> *data;
                cmd->getInt(t2, "offset", offset);
                if (offset < fsize)
                {
                    if (std::fseek(f, offset, SEEK_SET) == 0)
                    {
                        fsize = offset;
                        answer += "\"rewrite\":true,";
                    }
                }

                if (offset != fsize)
                {
                    ESP_LOGW(TAG, "Wrong offset of file %s(%d)", fname.c_str(), offset);
                    answer += "\"error\":\"Wrong offset of file " + fname + "(" + std::to_string(fsize) + ")\"";
                }
                else if (cmd->getBytes(t2, "data", data))
                {
                    if (std::fwrite(data->data(), 1, data->size(), f) != data->size())
                    {
                        ESP_LOGW(TAG, "Failed to write to file %s(%d)", fname.c_str(), data->size());
                        answer += "\"error\":\"Failed to write to file " + fname + "\"";
                    }
                    else
                    {
                        answer += "\"fw\":\"" + fname + "\",";
                        if (offset != 0)
                        {
                            answer += "\"offset\":" + std::to_string(offset) + ",";
                        }
                        answer += "\"size\":" + std::to_string(data->size());
                    }
                    delete data;
                }
                else
                {
                    answer += "\"error\":\"No data to write for " + fname + "\"";
                }
                std::fclose(f);
                writeEvent(false);
            }
            answer += '}';
        }
        else if (cmd->getString(t2, "ct", fname) && (cmd->getString(t2, "text", fname2)))
        {
            answer = "\"spiffs\":{";
            std::string str = "/spiffs/" + fname;
            writeEvent(true);
            FILE *f = std::fopen(str.c_str(), "w");
            if (f == nullptr)
            {
                writeEvent(false);
                ESP_LOGW(TAG, "Failed to open file %s", fname.c_str());
                answer += "\"error\":\"Failed to open file " + fname + "\"";
            }
            else
            {
                if (std::fwrite(fname2.c_str(), 1, fname2.length(), f) != fname2.length())
                {
                    ESP_LOGW(TAG, "Failed to write to file %s(%d)", fname.c_str(), fname2.length());
                    answer += "\"error\":\"Failed to write to file " + fname + "\"";
                }
                else
                {
                    answer += "\"tc\":\"" + fname + "\",";
                    answer += "\"size\":" + std::to_string(std::ftell(f));
                }
                std::fclose(f);
                writeEvent(false);
            }
            answer += '}';
        }
        else if (cmd->getString(t2, "at", fname) && (cmd->getString(t2, "text", fname2)))
        {
            answer = "\"spiffs\":{";
            std::string str = "/spiffs/" + fname;
            writeEvent(true);
            FILE *f = std::fopen(str.c_str(), "a");
            if (f == nullptr)
            {
                writeEvent(false);
                ESP_LOGW(TAG, "Failed to open file %s", fname.c_str());
                answer += "\"error\":\"Failed to open file " + fname + "\"";
            }
            else
            {
                if (std::fwrite(fname2.c_str(), 1, fname2.length(), f) != fname2.length())
                {
                    ESP_LOGW(TAG, "Failed to write to file %s(%d)", fname.c_str(), fname2.length());
                    answer += "\"error\":\"Failed to write to file " + fname + "\"";
                }
                else
                {
                    answer += "\"ta\":\"" + fname + "\",";
                    answer += "\"size\":" + std::to_string(std::ftell(f));
                }
                std::fclose(f);
                writeEvent(false);
            }
            answer += '}';
        }
        else if (cmd->getString(t2, "rt", fname))
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
                answer += "\"tr\":\"" + fname + "\",";
                int offset = 0;
                cmd->getInt(t2, "offset", offset);
                if (offset != 0)
                {
                    answer += "\"offset\":" + std::to_string(offset) + ",";
                }
                answer += "\"text\":\"";
                int size = CONFIG_DATAFORMAT_DEFAULT_DATA_SIZE;
                cmd->getInt(t2, "size", size);
                uint8_t *data = new uint8_t[size + 1];
                std::fseek(f, offset, SEEK_SET);
                size = std::fread(data, 1, size, f);
                std::fclose(f);
                data[size] = 0;
                std::string str((const char *)data);
                CJsonParser::updateString(str);
                answer += str + "\"";
                delete[] data;
            }
            answer += '}';
        }
    }
    return answer;
}
