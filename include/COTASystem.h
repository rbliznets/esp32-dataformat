/*!
  \file
  \brief Class for working with firmware update.
  \authors Bliznets R.A. (r.bliznets@gmail.com)
  \version 1.0.0.0
  \date 27.04.2025
*/

#pragma once

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "esp_ota_ops.h"
#include <list>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

/// Callback function type for OTA update process notification
/// @param lock true - OTA update start, false - OTA update end
typedef void onOTAWork(bool lock);

/// Static class for OTA (Over-The-Air) firmware update operations.
/*!
  Provides functions for performing ESP32 firmware OTA update,
  including step-by-step update through JSON commands and update from data buffer.
  Supports callback function registration for update process notifications.
*/
class COTASystem
{
protected:
  static std::list<onOTAWork *> mWriteQueue; ///< Queue of callback functions for OTA process notification
  static void writeEvent(bool lock);         ///< Call all callback functions from queue

  static esp_ota_handle_t update_handle; ///< Handle of current OTA update process
  static int offset;                     ///< Current offset during data write process

public:
  /// Initialize OTA system.
  /*!
    Checks current firmware partition state and determines if update confirmation is needed.
    \return true if new firmware confirmation is required, false otherwise.
  */
  static bool init();

  /// Confirm or rollback firmware.
  /*!
    Confirms successful operation of new firmware or initiates rollback to previous version.
    \param ok true to confirm current firmware (default), false to rollback.
  */
  static void confirmFirmware(bool ok = true);

  /// Abort current OTA update process.
  /*!
    Emergency terminates current OTA update process and resets state.
  */
  static void abort();

  /// Process JSON OTA update commands.
  /*!
   * @brief Process JSON commands for OTA update
   * Supported commands:
   * - "mode": operation mode ("begin", "end")
   * - "data": HEX string with firmware data
   *
   * @param cmd JSON object with OTA update commands (key "ota" at root)
   * @param[out] answer JSON object with command execution results.
   */
  static void command(json &cmd, json &answer);

  /// Perform OTA update from data buffer.
  /*!
    Performs complete OTA update from passed data buffer.
    Used for update from pre-loaded buffer.

    \param data Pointer to firmware data buffer
    \param size Data size in bytes
    \return JSON string with operation result ("ok" or "error").
  */
  static std::string update(uint8_t *data, uint32_t size);

  /// Add callback function for OTA process notification.
  /*!
    Registers function that will be called at OTA update start and end.
    \param event Pointer to callback function of type onOTAWork.
  */
  static void addWriteEvent(onOTAWork *event);

  /// Remove callback function from OTA notifications.
  /*!
    Removes specified function from OTA update process notification queue.
    \param event Pointer to callback function for removal.
  */
  static void removeWriteEvent(onOTAWork *event);
};