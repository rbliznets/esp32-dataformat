/*!
    \file
    \brief Class for working with firmware update.
    \authors Bliznets R.A. (r.bliznets@gmail.com)
    \version 1.0.0.0
    \date 27.04.2025
*/

#include "COTASystem.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "ota"; ///< Tag for logging

std::list<onOTAWork *> COTASystem::mWriteQueue; ///< Queue of callback functions for OTA process notifications

/**
 * @brief Call all callback functions from queue
 * @param lock Lock flag (true - OTA start, false - OTA end)
 *
 * Function notifies all registered callback functions about OTA process start or end.
 */
void COTASystem::writeEvent(bool lock)
{
    for (auto const &event : mWriteQueue)
    {
        event(lock);
    }
}

/**
 * @brief Add callback function to notification queue
 * @param event Pointer to callback function
 *
 * Registers function that will be called at OTA update start and end.
 */
void COTASystem::addWriteEvent(onOTAWork *event)
{
    mWriteQueue.push_back(event);
}

/**
 * @brief Remove callback function from notification queue
 * @param event Pointer to callback function to remove
 *
 * Removes specified function from OTA notification queue.
 */
void COTASystem::removeWriteEvent(onOTAWork *event)
{
    std::erase_if(mWriteQueue, [event](const auto &item)
                  { return item == event; });
}

esp_ota_handle_t COTASystem::update_handle = 0; ///< Handle of current OTA update
int COTASystem::offset = 0;                     ///< Current offset during write process

/**
 * @brief Initialize OTA system
 * @return true if firmware confirmation is required, false otherwise
 *
 * Checks current firmware partition state and determines if new image
 * confirmation is required after reboot.
 */
bool COTASystem::init()
{
    const esp_partition_t *running = esp_ota_get_running_partition();
    ESP_LOGI(TAG, "Running partition: %s", running->label); ///< Output current partition info
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK)
    {
        return (ota_state == ESP_OTA_IMG_PENDING_VERIFY);
    }
    return false;
}

/**
 * @brief Confirm or rollback firmware
 * @param ok true to confirm current firmware, false to rollback
 *
 * Confirms successful operation of new firmware or initiates rollback to previous version.
 */
void COTASystem::confirmFirmware(bool ok)
{
    if (ok)
    {
        ESP_LOGI(TAG, "Firmware confirmed");
        esp_ota_mark_app_valid_cancel_rollback(); // Confirm current firmware
    }
    else
    {
        ESP_LOGE(TAG, "Diagnostics failed! Start rollback to the previous version ...");
        esp_ota_mark_app_invalid_rollback_and_reboot(); // Rollback to previous version
    }
}

/**
 * @brief Abort OTA update
 *
 * Emergency terminates current OTA update process and resets state.
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
 * @brief Process OTA update commands via JSON
 * @param cmd JSON command with OTA parameters
 * @param answer JSON response with operation results
 *
 * Processes step-by-step OTA update via JSON commands.
 * Supports modes: "begin" (start), "end" (finish) and data transfer.
 */
void COTASystem::command(json &cmd, json &answer)
{
    if (cmd.contains("ota") && cmd["ota"].is_object())
    {
        answer["ota"] = json::object();
        esp_err_t err;
        std::string str;
        bool end = false;

        // Check operation mode
        if (cmd["ota"].contains("mode") && cmd["ota"]["mode"].is_string())
        {
            str = cmd["ota"]["mode"].template get<std::string>();
            if (str == "begin")
            {
                abort(); // Start new update - abort previous
            }
            else if (str == "end")
            {
                end = true; // End update flag
            }
        }

        // Process data for writing
        if (cmd["ota"].contains("data") && cmd["ota"]["data"].is_string())
        {
            // Convert HEX string to binary data
            std::string hexString = cmd["ota"]["data"].template get<std::string>();
            std::vector<uint8_t> data(hexString.length() / 2);
            for (size_t i = 0; i < hexString.length(); i += 2)
            {
                std::string byteStr = hexString.substr(i, 2);
                try
                {
                    // Convert two-character HEX string to byte
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

            writeEvent(true); // Notify about write start

            const esp_partition_t *update_partition;
            // Initialize OTA update if this is first data block
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

            // Write data
            err = esp_ota_write(update_handle, (const void *)data.data(), data.size());
            if (err != ESP_OK)
            {
                ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
                abort();
                writeEvent(false);
                answer["ota"]["error"] = "esp_ota_write failed";
                return;
            }
            offset += data.size(); // Update offset

            // If this is last data block - finish update
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

                // Set new partition as boot partition
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

                writeEvent(false); // Notify about write end
                answer["ota"]["offset"] = offset;
                answer["ota"]["mode"] = "end";
                offset = 0;
                update_handle = 0;
            }
            else
            {
                writeEvent(false); // Notify about current write portion end
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
 * @brief Perform OTA update from data buffer
 * @param data Pointer to firmware data buffer
 * @param size Data size in bytes
 * @return JSON string with operation result
 *
 * Performs complete OTA update from passed data buffer.
 * Used for update from pre-loaded buffer.
 */
std::string COTASystem::update(uint8_t *data, uint32_t size)
{
    std::string answer;
    writeEvent(true); // Notify about OTA start
    abort();          // Abort previous update if exists

    // Get next available partition for update
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(nullptr);
    if (update_partition == nullptr)
    {
        writeEvent(false);
        ESP_LOGE(TAG, "update partition failed");
        answer = "\"error\":\"update partition failed\"";
        return answer;
    }

    // Start OTA update process
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

    // Write all data
    err = esp_ota_write(update_handle, data, size);
    if (err != ESP_OK)
    {
        abort();
        writeEvent(false);
        ESP_LOGE(TAG, "esp_ota_write failed (%s)", esp_err_to_name(err));
        answer += "\"error\":\"esp_ota_write failed\"";
        return answer;
    }

    // Finish write process
    err = esp_ota_end(update_handle);
    if (err != ESP_OK)
    {
        abort();
        writeEvent(false);
        ESP_LOGE(TAG, "esp_ota_end failed (%s)", esp_err_to_name(err));
        answer += "\"error\":\"esp_ota_end failed\"}";
        return answer;
    }

    // Set new partition as boot partition
    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK)
    {
        writeEvent(false);
        ESP_LOGE(TAG, "esp_ota_set_boot_partition failed (%s)!", esp_err_to_name(err));
        answer += "\"error\":\"esp_ota_set_boot_partition failed\"";
        update_handle = 0;
        return answer;
    }

    writeEvent(false); // Notify about OTA end
    update_handle = 0; // Reset handle
    answer = "\"ok\":\"firmware was saved\"";
    return answer;
}