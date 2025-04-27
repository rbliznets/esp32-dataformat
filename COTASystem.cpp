/*!
    \file
    \brief Класс для работы с обновлением firmware.
    \authors Близнец Р.А. (r.bliznets@gmail.com)
    \version 0.0.0.1
    \date 27.04.2025
*/

#include "COTASystem.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ota"; ///< Тег для логирования

esp_ota_handle_t COTASystem::update_handle = 0;
int COTASystem::offset = 0;

void COTASystem::abort()
{
    if (update_handle != 0)
    {
        esp_ota_abort(update_handle);
        update_handle = 0;
        offset = 0;
    }
}

std::string COTASystem::command(CJsonParser *cmd)
{
    std::string answer = "";
    int t2;
    if (cmd->getObject(1, "ota", t2))
    {
        esp_err_t err;
        answer = "\"ota\":{";
        std::string str;
        bool end = false;
        if (cmd->getString(t2, "mode", str))
        {
            if (str == "begin")
            {
                abort();
            }
            else if (str == "end")
            {
                end = true;
            }
        }

        std::vector<uint8_t> *data;
        if (cmd->getBytes(t2, "data", data))
        {
            const esp_partition_t *update_partition;
            if (update_handle == 0)
            {
                update_partition = esp_ota_get_next_update_partition(nullptr);
                if (update_partition == nullptr)
                {
                    ESP_LOGE(TAG, "update partition failed");
                    answer += "\"error\":\"update partition failed\"}";
                    return answer;
                }
                err = esp_ota_begin(update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "esp_ota_begin failed (%s)", esp_err_to_name(err));
                    esp_ota_abort(update_handle);
                    update_handle = 0;
                    answer += "\"error\":\"esp_ota_begin failed\"}";
                    return answer;
                }
            }
            err = esp_ota_write(update_handle, (const void *)data->data(), data->size());
            if (err != ESP_OK)
            {
                delete data;
                ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
                abort();
                answer += "\"error\":\"esp_ota_write failed\"}";
                return answer;
            }
            offset += data->size();
            delete data;
            if (end)
            {
                err = esp_ota_end(update_handle);
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
                    abort();
                    answer += "\"error\":\"esp_ota_end failed\"}";
                    return answer;
                }

                update_partition = esp_ota_get_next_update_partition(nullptr);
                if (update_partition == nullptr)
                {
                    ESP_LOGE(TAG, "update partition failed");
                    answer += "\"error\":\"update partition failed\"}";
                    offset = 0;
                    update_handle = 0;
                    return answer;
                }
                err = esp_ota_set_boot_partition(update_partition);
                if (err != ESP_OK)
                {
                    ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
                    answer += "\"error\":\"esp_ota_set_boot_partition failed\"}";
                    offset = 0;
                    update_handle = 0;
                    return answer;
                }
                answer += "\"offset\":" + std::to_string(offset) + ",\"mode\":\"end\"}";
                offset = 0;
                update_handle = 0;
            }
            else
            {
                answer += "\"offset\":" + std::to_string(offset) + "}";
            }
        }
        else
        {
            answer += "\"error\":\"wrong format\"}";
        }
    }
    return answer;
}
