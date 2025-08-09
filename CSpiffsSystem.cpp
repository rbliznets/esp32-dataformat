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
#include <filesystem>

static const char *TAG = "spiffs"; ///< Тег для логирования

// Статический список обработчиков событий работы с SPIFFS
// Хранит указатели на функции, которые будут вызываться при начале/окончании операций записи
// Используется для уведомления внешних модулей о состоянии файловой системы
std::list<onSpiffsWork *> CSpiffsSystem::mWriteQueue;

/*!
 * @brief Вызов всех зарегистрированных обработчиков событий
 * @param lock true - начало транзакции (запись), false - окончание транзакции
 *
 * Последовательно вызывает каждый обработчик из mWriteQueue с переданным флагом
 * Гарантирует вызов всех уникальных обработчиков (без дублирования)
 *
 * \note Метод блокирует доступ к файловой системе до завершения всех вызовов
 * \warning Не должен использоваться внутри обработчиков событий
 */
void CSpiffsSystem::writeEvent(bool lock)
{
    for (auto const &event : mWriteQueue)
    {
        event(lock);
    }
}

/*!
 * @brief Регистрация нового обработчика событий
 * @param event Указатель на функцию-обработчик
 *
 * Добавляет функцию в список mWriteQueue только если она еще не существует
 * Предотвращает дублирование обработчиков
 *
 * \note Проверка на уникальность выполняется перед добавлением
 * \warning Необходимо убедиться, что обработчик корректно освобождается память
 */
void CSpiffsSystem::addWriteEvent(onSpiffsWork *event)
{
    // Проверка наличия обработчика в списке перед добавлением
    for (auto &e : mWriteQueue)
    {
        if (e == event)
        {
            return; // Обработчик уже существует - выходим
        }
    }
    mWriteQueue.push_back(event);
}

/*!
 * @brief Удаление обработчика событий
 * @param event Указатель на функцию-обработчик
 *
 * Удаляет все вхождения указанного обработчика из списка mWriteQueue
 * Использует безопасное удаление с помощью std::erase_if
 *
 * \note Метод гарантирует удаление всех дубликатов обработчика
 * \warning После удаления обработчика необходимо корректно освободить память
 */
void CSpiffsSystem::removeWriteEvent(onSpiffsWork *event)
{
    // Удаление всех вхождений обработчика из списка
    std::erase_if(mWriteQueue, [event](const auto &item)
                  { return item == event; });
}

/*!
 * @brief Инициализация файловой системы SPIFFS
 * @param check Флаг проверки целостности файловой системы (true - проверять, false - не проверять)
 * @return true - если инициализация прошла успешно, иначе false
 *
 * Выполняет следующие действия:
 * 1. Регистрация SPIFFS в системе виртуальных файловых систем (VFS)
 * 2. Проверка целостности файловой системы (при check=true)
 * 3. Получение и логирование информации о разделе
 * 4. Восстановление файловой системы при необходимости
 *
 * \note При check=true выполняется долгая операция проверки (может вызвать таймаут Watchdog)
 * \warning Форматирование файловой системы приведет к потере всех данных
 */
bool CSpiffsSystem::init(bool check)
{
    // Конфигурация параметров монтирования SPIFFS
    // - base_path: корневой каталог для доступа к SPIFFS
    // - max_files: максимальное количество одновременно открытых файлов
    // - format_if_mount_failed: автоматическое форматирование при неудачном монтировании
    esp_vfs_spiffs_conf_t conf = {
        .base_path = "/spiffs",
        .partition_label = nullptr,
        .max_files = 15,
        .format_if_mount_failed = true};

    // Регистрация SPIFFS в VFS
    // Возвращает ESP_OK при успешной регистрации
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK)
    {
        // Обработка ошибок монтирования
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

    // Дополнительная проверка завершения транзакций
    check |= endTransaction();

    // Проверка целостности файловой системы
    if (check)
    {
        // Предупреждение о длительной операции при включенном Watchdog
#ifdef CONFIG_ESP_TASK_WDT_INIT
        ESP_LOGW(TAG, "Long time operation, but WD is enabled.");
#endif
        ESP_LOGI(TAG, "SPIFFS checking...");
        // Проверка целостности раздела
        ret = esp_spiffs_check(conf.partition_label);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "SPIFFS_check() failed (%s)", esp_err_to_name(ret));
            return false;
        }
        else
        {
            ESP_LOGI(TAG, "SPIFFS_check() successful");
            // Сборка мусора для освобождения пространства
            esp_spiffs_gc(conf.partition_label, 100000);
        }
    }

    // Получение информации о разделе
    size_t total = 0, used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK)
    {
        // Предупреждение о длительной операции при включенном Watchdog
#ifdef CONFIG_ESP_TASK_WDT_INIT
        ESP_LOGW(TAG, "Long time operation, but WD is enabled.");
#endif
        // Попытка восстановления через форматирование
        ESP_LOGE(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
        ret = esp_spiffs_format(conf.partition_label);
        return (ret == ESP_OK);
    }
    else
    {
        // Логирование размера раздела
        ESP_LOGI(TAG, "Partition size: total: %d, used: %d", total, used);
    }
    return true;
}

/*!
 * @brief Освобождение ресурсов SPIFFS
 *
 * Выполняет следующие действия:
 * 1. Отменяет регистрацию SPIFFS в системе VFS
 * 2. Освобождает связанные ресурсы
 *
 * \note После вызова этого метода доступ к файловой системе невозможен
 * \warning Необходимо убедиться, что все файлы закрыты перед вызовом
 */
void CSpiffsSystem::free()
{
    // Отмена регистрации SPIFFS
    // nullptr указывает на отмену регистрации всех разделов
    esp_vfs_spiffs_unregister(nullptr);
}

/*!
 * @brief Завершение транзакции с файловой системой
 * @return true - если требуется повторная проверка файловой системы, иначе false
 *
 * Обрабатывает специальные файлы транзакций:
 * - Файлы с суффиксом "$" - содержат временные данные для переименования
 * - Файлы с суффиксом "!" - содержат данные для удаления
 *
 * Алгоритм работы:
 * 1. Открывает корневую директорию SPIFFS
 * 2. Проверяет наличие маркера активной транзакции "$"
 * 3. При отсутствии маркера:
 *    - Удаляет оставшиеся временные файлы ($ и !)
 *    - Выполняет восстановление целостности
 * 4. При наличии маркера:
 *    - Выполняет завершение транзакции (переименование и удаление)
 *    - Удаляет маркер транзакции
 *
 * \note Рекурсивный вызов обрабатывает оставшиеся артефакты
 * \warning Метод модифицирует файловую систему - не использовать в критических секциях
 */
bool CSpiffsSystem::endTransaction()
{
    struct dirent *entry;
    DIR *dp;
    bool res = false;

    // Открытие корневой директории SPIFFS
    dp = opendir("/spiffs");
    if (dp == nullptr)
    {
        ESP_LOGE(TAG, "Failed to open dir /spiffs");
        res = true; // Требуется проверка ФС из-за ошибки открытия
    }
    else
    {
        std::string str = "/spiffs/";

        // Проверка наличия маркера активной транзакции ($)
        FILE *f = std::fopen("/spiffs/$", "r");
        if (f == nullptr)
        {
            // Режим восстановления: обработка оставшихся артефактов
            while ((entry = readdir(dp)))
            {
                std::string fname = entry->d_name;

                // Удаление временных файлов с суффиксом $
                if (fname[fname.length() - 1] == '$')
                {
                    res = true; // ФС была изменена
                    std::remove((str + fname).c_str());
                }
                // Удаление временных файлов с суффиксом !
                else if (fname[fname.length() - 1] == '!')
                {
                    res = true; // ФС была изменена
                    std::remove((str + fname).c_str());
                }
            }
            closedir(dp);
        }
        else
        {
            // Активная транзакция обнаружена - завершение операций
            std::fclose(f);

            // Обработка всех файлов в директории
            while ((entry = readdir(dp)))
            {
                std::string fname = entry->d_name;

                // Обработка файлов с суффиксом $ (переименование)
                if ((fname.length() > 1) && (fname[fname.length() - 1] == '$'))
                {
                    std::string fname2 = fname.substr(0, fname.length() - 1);

                    // Удаление оригинального файла перед переименованием
                    std::remove((str + fname2).c_str());

                    // Переименование временного файла
                    if (std::rename((str + fname).c_str(), (str + fname2).c_str()) != 0)
                    {
                        ESP_LOGE(TAG, "Failed to rename file %s to %s",
                                 fname.c_str(), fname2.c_str());
                    }
                }
                // Обработка файлов с суффиксом ! (удаление)
                else if (fname[fname.length() - 1] == '!')
                {
                    std::string fname2 = fname.substr(0, fname.length() - 1);

                    // Удаление оригинального файла
                    std::remove((str + fname2).c_str());

                    // Удаление временного файла
                    if (std::remove((str + fname).c_str()) != 0)
                    {
                        ESP_LOGE(TAG, "Failed to remove file %s",
                                 fname.c_str());
                    }
                }
            }
            closedir(dp);

            // Рекурсивный вызов для обработки оставшихся артефактов
            if (res)
            {
                endTransaction(); // Повторная попытка завершения
            }
            else
            {
                // Удаление маркера транзакции
                std::remove("/spiffs/$");
                res = true;
            }
        }
    }
    return res; // true - если были изменения, требующие проверки
}

/*!
 * @brief Запись буфера данных в файл
 * @param fileName Имя файла для записи
 * @param data Указатель на данные для записи
 * @param size Размер данных в байтах
 * @return true - если запись прошла успешно, иначе false
 *
 * Выполняет атомарную операцию записи с блокировкой файловой системы:
 * 1. Уведомляет обработчики событий о начале транзакции
 * 2. Открывает файл в режиме дозаписи
 * 3. Выполняет запись данных
 * 4. Уведомляет обработчики событий об окончании транзакции
 *
 * \note Файл открывается в режиме "a" для дозаписи
 * \warning Не гарантирует атомарность при сбоях питания
 */
bool CSpiffsSystem::writeBuffer(const char *fileName, uint8_t *data, uint32_t size)
{
    // Уведомление обработчиков о начале операции
    writeEvent(true);

    // Открытие файла для дозаписи
    FILE *f = std::fopen(fileName, "a");
    if (f == nullptr)
    {
        // Уведомление обработчиков об окончании операции
        writeEvent(false);
        ESP_LOGE(TAG, "Failed to open file %s", fileName);
        return false;
    }
    else
    {
        // Запись данных в файл
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
            // Уведомление обработчиков об окончании операции
            writeEvent(false);
            return true;
        }
    }
}

/*!
 * @brief Обработка команд JSON для SPIFFS
 * @param cmd JSON-объект с командой в корне файловой системы
 * @param[out] answer json с ответом.
 *
 * Обрабатывает следующие команды:
 * - ls: Получение списка файлов в директории
 * - rd: Чтение содержимого файла
 * - rm: Удаление файла
 * - trans: Управление транзакциями (end/cancel)
 * - old/new: Переименование файла
 * - wr: Дозапись данных в файл
 * - ct: Создание файла с текстом
 * - at: Добавление текста в конец файла
 * - rt: Чтение текстового файла
 *
 * \note Все операции с файлами обёрнуты в writeEvent() для синхронизации
 */
void CSpiffsSystem::command(json &cmd, json &answer)
{
    if (cmd.contains("spiffs") && cmd["spiffs"].is_object())
    {
        answer["spiffs"] = json::object();
        if (cmd["spiffs"].contains("ls") && cmd["spiffs"]["ls"].is_string())
        {
            std::string fname = cmd["spiffs"]["ls"].template get<std::string>();
            std::string fname2 = "/spiffs";
            int offset = 0;
            int count = -1;
            if (cmd["spiffs"].contains("offset") && cmd["spiffs"]["offset"].is_number_unsigned())
            {
                offset = cmd["spiffs"]["offset"].template get<int>();
            }
            if (cmd["spiffs"].contains("count") && cmd["spiffs"]["count"].is_number_unsigned())
            {
                count = cmd["spiffs"]["count"].template get<int>();
            }

            // Формирование полного пути
            if (fname != "")
                fname2 += "/" + fname;

            struct dirent *entry;
            DIR *dp;
            std::map<std::string, uint32_t> dirs;
            writeEvent(true); // Блокировка файловой системы

            // Открытие директории
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

                // Сканирование содержимого директории
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
                writeEvent(false); // Разблокировка файловой системы

                // Добавление информации о подкаталогах
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

                // Чтение данных
                std::fseek(f, offset, SEEK_SET);
                size = std::fread(data, 1, size, f);
                std::fclose(f);

                // Преобразование в HEX
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
        else if (cmd["spiffs"].contains("trans") && cmd["spiffs"]["trans"].is_string())
        {
            std::string fname = cmd["spiffs"]["trans"].template get<std::string>();

            if (fname == "end")
            {
                writeEvent(true);
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
        else if (cmd["spiffs"].contains("wr") && cmd["spiffs"]["wr"].is_string())
        {
            std::string fname = cmd["spiffs"]["wr"].template get<std::string>();
            std::string str = "/spiffs/" + fname;
            std::string hexString;
            if (cmd["spiffs"].contains("data") && cmd["spiffs"]["data"].is_string())
            {
                hexString = cmd["spiffs"]["data"].template get<std::string>();
                std::vector<uint8_t> data(hexString.length() / 2);
                for (size_t i = 0; i < hexString.length(); i += 2)
                {
                    std::string byteStr = hexString.substr(i, 2);
                    try
                    {
                        // Convert the two-character hex string to an integer with base 16
                        uint8_t byteValue = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
                        data.push_back(byteValue);
                    }
                    catch (const std::invalid_argument &e)
                    {
                        answer["spiffs"]["error"] = "Invalid hex character in string: " + byteStr;
                        return;
                    }
                    catch (const std::out_of_range &e)
                    {
                        answer["spiffs"]["error"] = "IHex value out of range for uint8_t: " + byteStr;
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
                    if (cmd["spiffs"].contains("offset") && cmd["spiffs"]["offset"].is_number_unsigned())
                    {
                        offset = cmd["spiffs"]["offset"].template get<int>();
                    }

                    if (offset < fsize)
                    {
                        std::fclose(f);
                        truncate(str.c_str(), offset);
                        f = std::fopen(str.c_str(), "a");
                        fsize = offset;
                        answer["spiffs"]["rewrite"] = true;
                    }

                    if (offset != fsize)
                    {
                        ESP_LOGW(TAG, "Wrong offset of file %s(%d)", fname.c_str(), offset);
                        answer["spiffs"]["error"] = "Wrong offset of file " + fname + "(" + std::to_string(fsize) + ")";
                    }
                    else
                    {
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
    }
}

/*!
 * @brief Обработка команд JSON для SPIFFS
 * @param cmd JSON-объект с командой в корне файловой системы
 * @return JSON-строка с ответом (без обрамления {}) или пустая строка при ошибке
 *
 * Обрабатывает следующие команды:
 * - ls: Получение списка файлов в директории
 * - rd: Чтение содержимого файла
 * - rm: Удаление файла
 * - trans: Управление транзакциями (end/cancel)
 * - old/new: Переименование файла
 * - wr: Дозапись данных в файл
 * - ct: Создание файла с текстом
 * - at: Добавление текста в конец файла
 * - rt: Чтение текстового файла
 *
 * \note Все операции с файлами обёрнуты в writeEvent() для синхронизации
 * \warning Не обрабатывает рекурсивные операции с подкаталогами
 */
std::string CSpiffsSystem::command(CJsonParser *cmd)
{
    std::string answer = "";
    char tmp[32];
    int t2;

    // Проверка наличия основного ключа "spiffs"
    if (cmd->getObject(1, "spiffs", t2))
    {
        std::string fname;
        std::string fname2;

        /*!
         * @brief Команда ls - Получение списка файлов в директории
         *
         * Параметры:
         * - root: Корневая директория (по умолчанию "/")
         * - offset: Смещение для пагинации
         * - count: Количество элементов на странице
         *
         * Формат ответа:
         * {
         *   "spiffs": {
         *     "root": "...",
         *     "files": [...],
         *     ...
         *   }
         * }
         */
        if (cmd->getString(t2, "ls", fname))
        {
            answer = "\"spiffs\":{";
            answer += "\"root\":\"" + fname + "\"";
            fname2 = "/spiffs";
            int offset = 0;
            int count = -1;
            cmd->getInt(t2, "offset", offset);
            cmd->getInt(t2, "count", count);

            // Формирование полного пути
            if (fname != "")
                fname2 += "/" + fname;

            struct dirent *entry;
            DIR *dp;
            std::map<std::string, uint32_t> dirs;
            writeEvent(true); // Блокировка файловой системы

            // Открытие директории
            dp = opendir(fname2.c_str());
            if (dp == nullptr)
            {
                writeEvent(false);
                ESP_LOGE(TAG, "Failed to open dir %s", fname2.c_str());
                answer += ",\"error\":\"Failed to open dir " + fname2 + "\"";
            }
            else
            {
                fname2 += "/";
                answer += ",\"files\":[";
                bool point = false;

                // Сканирование содержимого директории
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
                writeEvent(false); // Разблокировка файловой системы

                // Добавление информации о подкаталогах
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

        /*!
         * @brief Команда rd - Чтение содержимого файла
         *
         * Параметры:
         * - file: Имя файла
         * - offset: Смещение для чтения
         * - size: Количество байт для чтения
         *
         * Формат ответа:
         * {
         *   "spiffs": {
         *     "fr": "файл",
         *     "offset": ...,
         *     "data": "..."
         *   }
         * }
         */
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

                // Чтение данных
                std::fseek(f, offset, SEEK_SET);
                size = std::fread(data, 1, size, f);
                std::fclose(f);

                // Преобразование в HEX
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

        /*!
         * @brief Команда rm - Удаление файла
         *
         * Параметры:
         * - file: Имя файла
         *
         * Формат ответа:
         * {
         *   "spiffs": {
         *     "fd": "файл",
         *     ...
         *   }
         * }
         */
        else if (cmd->getString(t2, "rm", fname))
        {
            answer = "\"spiffs\":{";
            std::string str = "/spiffs/" + fname;
            writeEvent(true);

            if (std::filesystem::exists(str.c_str()))
            {
                std::remove(str.c_str());
                writeEvent(false);
                answer += "\"fd\":\"" + fname + "\"}";
            }
            else
            {
                writeEvent(false);
                answer += "\"waring\":\"File do not exist\",\"fd\":\"" + fname + "\"}";
            }
        }

        /*!
         * @brief Команда trans - Управление транзакциями
         *
         * Параметры:
         * - end: Завершение транзакции
         * - cancel: Отмена транзакции
         *
         * \note Использует маркер "$" для управления транзакциями
         */
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
                std::remove("/spiffs/$");
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

        /*!
         * @brief Команда old/new - Переименование файла
         *
         * Параметры:
         * - old: Старое имя файла
         * - new: Новое имя файла
         *
         * \warning Не обрабатывает переименование в существующий файл
         */
        else if ((cmd->getString(t2, "old", fname)) && (cmd->getString(t2, "new", fname2)))
        {
            answer = "\"spiffs\":{";
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
                        answer += "\"error\":\"Failed to rename file " + fname + " to " + fname2 + "\"";
                    }
                    else
                    {
                        writeEvent(false);
                        answer += "\"fold\":\"" + fname + "\",\"fnew\":\"" + fname2 + "\"";
                    }
                }
                else
                {
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
                }
            }
            else if (std::filesystem::exists(str2.c_str()))
            {
                writeEvent(false);
                answer += "\"waring\":\"Old file do not exist\",\"fnew\":\"" + fname2 + "\"";
            }
            else
            {
                writeEvent(false);
                answer += "\"error\":\"Failed to rename file " + fname + " to " + fname2 + "\"";
            }
            answer += '}';
        }

        /*!
         * @brief Команда wr - Дозапись данных в файл
         *
         * Параметры:
         * - file: Имя файла
         * - offset: Смещение для записи
         * - data: Данные для записи
         *
         * \warning Не обеспечивает атомарность при сбоях питания
         */
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
                    std::fclose(f);
                    truncate(str.c_str(), offset);
                    f = std::fopen(str.c_str(), "a");
                    fsize = offset;
                    answer += "\"rewrite\":true,";
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

        /*!
         * @brief Команда ct - Создание файла с текстом
         *
         * Параметры:
         * - file: Имя файла
         * - text: Текст для записи
         *
         * \warning Записывает только текст без преобразования
         */
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

        /*!
         * @brief Команда at - Добавление текста в конец файла
         *
         * Параметры:
         * - file: Имя файла
         * - text: Текст для добавления
         *
         * \note Использует режим "a" для дозаписи
         */
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

        /*!
         * @brief Команда rt - Чтение текстового файла
         *
         * Параметры:
         * - file: Имя файла
         * - offset: Смещение для чтения
         * - size: Количество байт для чтения
         *
         * \note Возвращает текст как строку
         */
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

                // Чтение текста
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