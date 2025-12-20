/*!
	\file
	\brief Class for working with NVS.
	\authors Bliznets R.A. (r.bliznets@gmail.com)
	\version 1.2.0.0
	\date 02.05.2024
*/

#pragma once

#include "sdkconfig.h"
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#define NVS_NONE (0)
#define NVS_MAIN (1)
#define NVS_BACKUP (2)
#define NVS_BOTH (3)

/**
 * @brief Static class for working with Non-Volatile Storage (NVS) memory.
 *
 * Provides static methods for initialization, working with variables
 * and performing service operations with ESP32 NVS memory.
 * Supports working with various data types: integers, floating-point numbers, strings.
 */
class CNvsSystem
{
private:
	static bool nvs2;	   // nvs2 exist
	static bool nvs2_lock; // nvs2 writing lock

	template <typename T>
	static bool writeValueToNamespace(const char *ns, const std::string &key, T value);

	template <typename T>
	static uint16_t saveValue(std::string &name, T value, uint16_t mode);

	template <typename T>
	static uint16_t restoreValue(std::string &name, T &value, bool copy);

	/**
	 * @brief Writes a string to the specified NVS namespace.
	 *
	 * @param ns Namespace ("nvs", "nvs2")
	 * @param key Key (variable name)
	 * @param value String value to store
	 * @return true on success, false otherwise
	 */
	static bool writeStringToNamespace(const char *ns, const std::string &key, const std::string &value);

	/**
	 * @brief Writes binary data to the specified NVS namespace.
	 *
	 * @param ns Namespace ("nvs", "nvs2")
	 * @param key Key (variable name)
	 * @param data Pointer to binary data
	 * @param length Size of data
	 * @return true on success, false otherwise
	 */
	static bool writeBlobToNamespace(const char *ns, const std::string &key, const uint8_t *data, size_t length);

	/**
	 * @brief Saves a string to NVS with backup storage support.
	 *
	 * @param name Variable name
	 * @param value String value
	 * @param mode Save mode (MAIN, BACKUP, BOTH)
	 * @return Result of save operation (NVS_MAIN, NVS_BACKUP, NVS_BOTH, NVS_NONE)
	 */
	static uint16_t saveString(std::string &name, const std::string &value, uint16_t mode);

	/**
	 * @brief Saves binary data to NVS with backup storage support.
	 *
	 * @param name Variable name
	 * @param data Pointer to binary data
	 * @param length Size of data
	 * @param mode Save mode
	 * @return Result of save operation
	 */
	static uint16_t saveBlob(std::string &name, const uint8_t *data, size_t length, uint16_t mode);

	/**
	 * @brief Restores a string from NVS with backup storage support.
	 *
	 * @param name Variable name
	 * @param value Variable to receive the value
	 * @param copy If true, value from backup will be copied to main
	 * @return Result of restore operation
	 */
	static uint16_t restoreString(std::string &name, std::string &value, bool copy);

	/**
	 * @brief Restores binary data from NVS with backup storage support.
	 *
	 * @param name Variable name
	 * @param data Vector to receive binary data
	 * @param copy If true, value from backup will be copied to main
	 * @return Result of restore operation
	 */
	static uint16_t restoreBlob(std::string &name, std::vector<uint8_t> &data, bool copy);

public:
	/**
	 * @brief Initialize NVS memory.
	 *
	 * Performs initialization of main NVS partition and additional "nvs2" partition.
	 * Automatically performs memory cleanup if errors are detected.
	 *
	 * @return true if initialization is successful, false in case of error.
	 */
	static bool init();

	/**
	 * @brief Deinitialize NVS memory.
	 *
	 * Releases NVS memory resources and closes file system.
	 * Called before finishing work with NVS.
	 */
	static void free();

	/**
	 * @brief Process JSON commands for NVS operations.
	 *
	 * Processes NVS memory commands from JSON object.
	 * Supported commands:
	 * - "clear": clear entire NVS memory
	 * - "reset": restart device
	 * - Work with variables of various types (u8, i8, i16, i32, u32, float, double)
	 *
	 * @param[in] cmd JSON object with NVS commands (key "nvs" at root).
	 * @param[out] answer JSON object with command execution results.
	 */
	static void command(json &cmd, json &answer);

	// Save methods
	/**
	 * @brief Save uint8_t value to NVS.
	 * @param name Variable name
	 * @param value Value to save
	 * @param mode Save mode (default: NVS_MAIN)
	 * @return Result of save operation
	 */
	static uint16_t save(std::string &name, uint8_t value, uint16_t mode = NVS_MAIN);

	/**
	 * @brief Save int8_t value to NVS.
	 * @param name Variable name
	 * @param value Value to save
	 * @param mode Save mode (default: NVS_MAIN)
	 * @return Result of save operation
	 */
	static uint16_t save(std::string &name, int8_t value, uint16_t mode = NVS_MAIN);

	/**
	 * @brief Save uint16_t value to NVS.
	 * @param name Variable name
	 * @param value Value to save
	 * @param mode Save mode (default: NVS_MAIN)
	 * @return Result of save operation
	 */
	static uint16_t save(std::string &name, uint16_t value, uint16_t mode = NVS_MAIN);

	/**
	 * @brief Save int16_t value to NVS.
	 * @param name Variable name
	 * @param value Value to save
	 * @param mode Save mode (default: NVS_MAIN)
	 * @return Result of save operation
	 */
	static uint16_t save(std::string &name, int16_t value, uint16_t mode = NVS_MAIN);

	/**
	 * @brief Save uint32_t value to NVS.
	 * @param name Variable name
	 * @param value Value to save
	 * @param mode Save mode (default: NVS_MAIN)
	 * @return Result of save operation
	 */
	static uint16_t save(std::string &name, uint32_t value, uint16_t mode = NVS_MAIN);

	/**
	 * @brief Save int32_t value to NVS.
	 * @param name Variable name
	 * @param value Value to save
	 * @param mode Save mode (default: NVS_MAIN)
	 * @return Result of save operation
	 */
	static uint16_t save(std::string &name, int32_t value, uint16_t mode = NVS_MAIN);

	/**
	 * @brief Save float value to NVS.
	 * @param name Variable name
	 * @param value Value to save
	 * @param mode Save mode (default: NVS_MAIN)
	 * @return Result of save operation
	 */
	static uint16_t save(std::string &name, float value, uint16_t mode = NVS_MAIN);

	/**
	 * @brief Save double value to NVS.
	 * @param name Variable name
	 * @param value Value to save
	 * @param mode Save mode (default: NVS_MAIN)
	 * @return Result of save operation
	 */
	static uint16_t save(std::string &name, double value, uint16_t mode = NVS_MAIN);

	/**
	 * @brief Save std::string value to NVS.
	 * @param name Variable name
	 * @param value String value to save
	 * @param mode Save mode (default: NVS_MAIN)
	 * @return Result of save operation
	 */
	static uint16_t save(std::string &name, const std::string &value, uint16_t mode = NVS_MAIN);

	/**
	 * @brief Save std::vector<uint8_t> value to NVS.
	 * @param name Variable name
	 * @param data Binary data to save
	 * @param mode Save mode (default: NVS_MAIN)
	 * @return Result of save operation
	 */
	static uint16_t save(std::string &name, const std::vector<uint8_t> &data, uint16_t mode = NVS_MAIN);

	// Restore methods
	/**
	 * @brief Restore uint8_t value from NVS.
	 * @param name Variable name
	 * @param value Variable to receive the value
	 * @param copy If true, value from backup will be copied to main (default: true)
	 * @return Result of restore operation
	 */
	static uint16_t restore(std::string &name, uint8_t &value, bool copy = true);

	/**
	 * @brief Restore int8_t value from NVS.
	 * @param name Variable name
	 * @param value Variable to receive the value
	 * @param copy If true, value from backup will be copied to main (default: true)
	 * @return Result of restore operation
	 */
	static uint16_t restore(std::string &name, int8_t &value, bool copy = true);

	/**
	 * @brief Restore uint16_t value from NVS.
	 * @param name Variable name
	 * @param value Variable to receive the value
	 * @param copy If true, value from backup will be copied to main (default: true)
	 * @return Result of restore operation
	 */
	static uint16_t restore(std::string &name, uint16_t &value, bool copy = true);

	/**
	 * @brief Restore int16_t value from NVS.
	 * @param name Variable name
	 * @param value Variable to receive the value
	 * @param copy If true, value from backup will be copied to main (default: true)
	 * @return Result of restore operation
	 */
	static uint16_t restore(std::string &name, int16_t &value, bool copy = true);

	/**
	 * @brief Restore uint32_t value from NVS.
	 * @param name Variable name
	 * @param value Variable to receive the value
	 * @param copy If true, value from backup will be copied to main (default: true)
	 * @return Result of restore operation
	 */
	static uint16_t restore(std::string &name, uint32_t &value, bool copy = true);

	/**
	 * @brief Restore int32_t value from NVS.
	 * @param name Variable name
	 * @param value Variable to receive the value
	 * @param copy If true, value from backup will be copied to main (default: true)
	 * @return Result of restore operation
	 */
	static uint16_t restore(std::string &name, int32_t &value, bool copy = true);

	/**
	 * @brief Restore float value from NVS.
	 * @param name Variable name
	 * @param value Variable to receive the value
	 * @param copy If true, value from backup will be copied to main (default: true)
	 * @return Result of restore operation
	 */
	static uint16_t restore(std::string &name, float &value, bool copy = true);

	/**
	 * @brief Restore double value from NVS.
	 * @param name Variable name
	 * @param value Variable to receive the value
	 * @param copy If true, value from backup will be copied to main (default: true)
	 * @return Result of restore operation
	 */
	static uint16_t restore(std::string &name, double &value, bool copy = true);

	/**
	 * @brief Restore std::string value from NVS.
	 * @param name Variable name
	 * @param value Variable to receive the value
	 * @param copy If true, value from backup will be copied to main (default: true)
	 * @return Result of restore operation
	 */
	static uint16_t restore(std::string &name, std::string &value, bool copy = true);

	/**
	 * @brief Restore std::vector<uint8_t> value from NVS.
	 * @param name Variable name
	 * @param data Vector to receive binary data
	 * @param copy If true, value from backup will be copied to main (default: true)
	 * @return Result of restore operation
	 */
	static uint16_t restore(std::string &name, std::vector<uint8_t> &data, bool copy = true);
};