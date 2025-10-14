/*!
    \file
    \brief Class for working with SPIFFS.
    \authors Bliznets R.A. (r.bliznets@gmail.com)
    \version 1.4.0.0
    \date 12.12.2023
*/

#pragma once

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <list>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

/// Callback function type for SPIFFS work event notification
/// @param lock true - start transaction (write), false - end transaction
typedef void onSpiffsWork(bool lock);

/// Static class for SPIFFS file system operations.
/*!
  Provides functions for initialization, file management and transactions of SPIFFS.
  Supports callback function registration for file system state notification.
  Implements atomic input/output operations and transaction management.
*/
class CSpiffsSystem
{
protected:
    /// Queue of SPIFFS work event handlers
    /// Used to notify external modules about start/end of write operations
    static std::list<onSpiffsWork *> mWriteQueue;

    /// Internal method for generating file system work events
    /// @param lock true - start transaction (write), false - end transaction
    static void writeEvent(bool lock);

public:
    /*!
     * @brief Initialize SPIFFS file system
     * @param check File system integrity check flag (default false)
     * @return true - if initialization was successful, otherwise false
     *
     * Performs SPIFFS registration in VFS, integrity check (when check=true)
     * and gets partition information. Performs formatting if necessary.
     *
     * \note When check=true may cause Watchdog timeout due to long operation
     * \warning Formatting will result in loss of all data in file system
     */
    static bool init(bool check = false);

    /*!
     * @brief Free file system resources
     *
     * Performs SPIFFS deinitialization, unregisters from VFS
     * and frees associated resources.
     *
     * \note After calling this method, access to SPIFFS is impossible
     * \warning Must ensure all files are closed before calling
     */
    static void free();

    /*!
     * @brief Check and complete unfinished transactions
     * @return true - if all transactions completed successfully, otherwise false
     *
     * Processes special transaction files (*.tmp, *.bak) and completes
     * unfinished input/output operations to ensure data integrity.
     *
     * \note Used to guarantee completion of input/output operations before exit
     * \warning Method modifies file system
     */
    static bool endTransaction();

    /*!
     * @brief Write data buffer to file
     * @param fileName File name for writing (full path)
     * @param data Pointer to buffer with data for writing
     * @param size Data size in bytes
     * @return true - if write was successful, otherwise false
     *
     * Performs atomic write operation with file system locking.
     * File opens in append mode ("a").
     *
     * \note Automatically notifies handlers about start/end of operation
     * \warning Doesn't guarantee atomicity on power failures
     */
    static bool writeBuffer(const char *fileName, uint8_t *data, uint32_t size);

    /// Process JSON commands for SPIFFS operations.
    /*!
     * @brief Process JSON commands for file system management
     *
     * Supported commands:
     * - "ls": Get list of files in directory (with pagination)
     * - "rd": Read binary file content
     * - "rm": Delete file
     * - "trans": Transaction management ("end"/"cancel")
     * - "old"/"new": Rename file
     * - "wr": Write binary data to file
     * - "ct": Create text file
     * - "at": Append text to end of file
     * - "rt": Read text file
     *
     * @param cmd JSON object with SPIFFS commands (key "spiffs" at root)
     * @param[out] answer JSON object with command execution results
     *
     * \note All file operations are wrapped in synchronization mechanisms
     * \warning Some operations may be time-consuming
     */
    static void command(json &cmd, json &answer);

    /*!
     * @brief Add SPIFFS work event handler
     * @param event Pointer to handler function
     *
     * Registers function that will be called at start/end of
     * file system write operations. Checks handler uniqueness.
     *
     * \note Handler is called synchronously in calling thread context
     * \warning Must properly free handler memory
     */
    static void addWriteEvent(onSpiffsWork *event);

    /*!
     * @brief Remove SPIFFS work event handler
     * @param event Pointer to handler function
     *
     * Removes previously added handler from notification queue.
     * Safely removes all handler duplicates.
     *
     * \note Method guarantees removal of all handler occurrences
     * \warning After removal, handler must be properly freed
     */
    static void removeWriteEvent(onSpiffsWork *event);

    /**
     * @brief Clears content of files in specified directory (at end of transaction)
     *
     * Function opens all files in directory and marks them for erasure at transaction end.
     * Files with extensions '!' and '$' are ignored (considered service files).
     *
     * @param dirName Path to directory whose file content needs to be cleared
     * @return uint16_t Number of successfully cleared files
     */
    static uint16_t clearDir(const char *dirName);
};