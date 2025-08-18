/*!
    \file
    \brief Класс для работы с обновлением firmware.
    \authors Близнец Р.А. (r.bliznets@gmail.com)
    \version 1.0.0.0
    \date 27.04.2025
*/

#include "COTASystem.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ota"; ///< Тег для логирования

std::list<onOTAWork *> COTASystem::mWriteQueue; ///< Очередь callback-функций для уведомления о процессе OTA

/**
 * @brief Вызов всех callback-функций из очереди
 * @param lock Флаг блокировки (true - начало OTA, false - завершение OTA)
 * 
 * Функция уведомляет все зарегистрированные callback-функции о начале или завершении процесса OTA.
 */
void COTASystem::writeEvent(bool lock)
{
    for (auto const &event : mWriteQueue)
    {
        event(lock);
    }
}

/**
 * @brief Добавление callback-функции в очередь уведомлений
 * @param event Указатель на callback-функцию
 * 
 * Регистрирует функцию, которая будет вызвана при начале и завершении OTA обновления.
 */
void COTASystem::addWriteEvent(onOTAWork *event)
{
    mWriteQueue.push_back(event);
}

/**
 * @brief Удаление callback-функции из очереди уведомлений
 * @param event Указатель на callback-функцию для удаления
 * 
 * Удаляет указанную функцию из очереди уведомлений OTA.
 */
void COTASystem::removeWriteEvent(onOTAWork *event)
{
    std::erase_if(mWriteQueue, [event](const auto &item)
                  { return item == event; });
}

esp_ota_handle_t COTASystem::update_handle = 0; ///< Дескриптор текущего OTA обновления
int COTASystem::offset = 0;                     ///< Текущее смещение в процессе записи

/**
 * @brief Инициализация OTA системы
 * @return true если требуется подтверждение firmware, false в противном случае
 * 
 * Проверяет состояние текущего раздела firmware и определяет, требуется ли подтверждение
 * нового образа после перезагрузки.
 */
bool COTASystem::init()
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "Running partition: %s", running->label); ///< Вывод информации о текущем разделе
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        return (ota_state == ESP_OTA_IMG_PENDING_VERIFY);
    }
    return false;
}

/**
 * @brief Подтверждение или откат firmware
 * @param ok true для подтверждения текущего firmware, false для отката
 * 
 * Подтверждает успешную работу нового firmware или инициирует откат к предыдущей версии.
 */
void COTASystem::confirmFirmware(bool ok)
{
    if (ok)
    {
        ESP_LOGI(TAG, "Firmware confirmed");
        esp_ota_mark_app_valid_cancel_rollback(); // Подтверждаем текущий firmware
    }
    else
    {
        ESP_LOGE(TAG, "Diagnostics failed! Start rollback to the previous version ...");
        esp_ota_mark_app_invalid_rollback_and_reboot(); // Откатываемся к предыдущей версии
    }
}

/**
 * @brief Прерывание OTA обновления
 * 
 * Аварийно завершает текущий процесс OTA обновления и сбрасывает состояние.
 */
void COTASystem::abort()
{
    if (update_handle != 0)
    {
        esp_ota_abort(update_handle);
        update_handle = 0;
        offset = 0;
    }
}

/**
 * @brief Обработка команд OTA обновления через JSON
 * @param cmd JSON-команда с параметрами OTA
 * @param answer JSON-ответ с результатами операции
 * 
 * Обрабатывает пошаговое OTA обновление через JSON команды.
 * Поддерживает режимы: "begin" (начало), "end" (завершение) и передачу данных.
 */
void COTASystem::command(json &cmd, json &answer)
{
    if (cmd.contains("ota") && cmd["ota"].is_object())
    {
        answer["ota"] = json::object();
        esp_err_t err;
        std::string str;
        bool end = false;

        // Проверяем режим работы
        if (cmd["ota"].contains("mode") && cmd["ota"]["mode"].is_string())
        {
            str = cmd["ota"]["mode"].template get<std::string>();
            if (str == "begin")
            {
                abort(); // Начинаем новое обновление - прерываем предыдущее
            }
            else if (str == "end")
            {
                end = true; // Флаг завершения обновления
            }
        }
        
        // Обрабатываем данные для записи
        if (cmd["ota"].contains("data") && cmd["ota"]["data"].is_string())
        {
            // Преобразуем HEX-строку в бинарные данные
            std::string hexString = cmd["ota"]["data"].template get<std::string>();
            std::vector<uint8_t> data(hexString.length() / 2);
            for (size_t i = 0; i < hexString.length(); i += 2)
            {
                std::string byteStr = hexString.substr(i, 2);
                try
                {
                    // Конвертируем двухсимвольную HEX-строку в байт
                    uint8_t byteValue = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
                    data[i >> 1] = byteValue;
                }
                catch (const std::invalid_argument &e)
                {
                    answer["ota"]["error"] = "Invalid hex character in string: " + byteStr;
                    return;
                }
                catch (const std::out_of_range &e)
                {
                    answer["ota"]["error"] = "Hex value out of range for uint8_t: " + byteStr;
                    return;
                }
            }
            
            writeEvent(true); // Уведомляем о начале записи
            
            const esp_partition_t *update_partition;
            // Инициализируем OTA обновление если это первый блок данных
            if (update_handle == 0)
            {
                update_partition = esp_ota_get_next_update_partition(nullptr);
                if (update_partition == nullptr)
                {
                    writeEvent(false);
                    ESP_LOGE(TAG, "update partition failed");
                    answer["ota"]["error"] = "update partition failed";
                    return;
                }
                err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                    esp_ota_abort(update_handle);
                    update_handle = 0;
                    writeEvent(false);
                    answer["ota"]["error"] = "esp_ota_begin failed";
                    return;
                }
            }
            
            // Записываем данные
            err = esp_ota_write(update_handle, (const void *)data.data(), data.size());
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
                abort();
                writeEvent(false);
                answer["ota"]["error"] = "esp_ota_write failed";
                return;
            }
            offset += data.size(); // Обновляем смещение
            
            // Если это последний блок данных - завершаем обновление
            if (end)
            {
                err = esp_ota_end(update_handle);
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
                    abort();
                    writeEvent(false);
                    answer["ota"]["error"] = "esp_ota_end failed";
                    return;
                }

                update_partition = esp_ota_get_next_update_partition(nullptr);
                if (update_partition == nullptr)
                {
                    writeEvent(false);
                    ESP_LOGE(TAG, "update partition failed");
                    answer["ota"]["error"] = "update partition failed";
                    offset = 0;
                    update_handle = 0;
                    return;
                }
                
                // Устанавливаем новый раздел как загрузочный
                err = esp_ota_set_boot_partition(update_partition);
                if (err != ESP_OK)
                {
                    writeEvent(false);
                    ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
                    answer["ota"]["error"] = "esp_ota_set_boot_partition failed";
                    offset = 0;
                    update_handle = 0;
                    return;
                }
                
                writeEvent(false); // Уведомляем о завершении записи
                answer["ota"]["offset"] = offset;
                answer["ota"]["mode"] = "end";
                offset = 0;
                update_handle = 0;
            }
            else
            {
                writeEvent(false); // Уведомляем о завершении текущей порции записи
                answer["ota"]["offset"] = offset;
            }
        }
        else
        {
            answer["ota"]["error"] = "wrong format";
        }
    }
}

/**
 * @brief Выполнение OTA обновления из буфера данных
 * @param data Указатель на буфер с firmware данными
 * @param size Размер данных в байтах
 * @return JSON-строка с результатом операции
 * 
 * Выполняет полное OTA обновление из переданного буфера данных.
 * Используется для обновления из предварительно загруженного буфера.
 */
std::string COTASystem::update(uint8_t *data, uint32_t size)
{
    std::string answer;
    writeEvent(true); // Уведомляем о начале OTA
    abort();          // Прерываем предыдущее обновление если есть
    
    // Получаем следующий доступный раздел для обновления
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(nullptr);
    if (update_partition == nullptr)
    {
        writeEvent(false);
        ESP_LOGE(TAG, "update partition failed");
        answer = "\"error\":\"update partition failed\"";
        return answer;
    }
    
    // Начинаем процесс OTA обновления
    esp_err_t err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
    if (err != ESP_OK)
    {
        esp_ota_abort(update_handle);
        writeEvent(false);
        ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
        update_handle = 0;
        answer = "\"error\":\"esp_ota_begin failed\"";
        return answer;
    }
    
    // Записываем все данные
    err = esp_ota_write(update_handle, data, size);
    if (err != ESP_OK)
    {
        abort();
        writeEvent(false);
        ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
        answer += "\"error\":\"esp_ota_write failed\"";
        return answer;
    }
    
    // Завершаем процесс записи
    err = esp_ota_end(update_handle);
    if (err != ESP_OK)
    {
        abort();
        writeEvent(false);
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        answer += "\"error\":\"esp_ota_end failed\"}";
        return answer;
    }
    
    // Устанавливаем новый раздел как загрузочный
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        writeEvent(false);
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        answer += "\"error\":\"esp_ota_set_boot_partition failed\"";
        update_handle = 0;
        return answer;
    }
    
    writeEvent(false);     // Уведомляем о завершении OTA
    update_handle = 0;     // Сбрасываем дескриптор
    answer = "\"ok\":\"firmware was saved\"";
    return answer;
}