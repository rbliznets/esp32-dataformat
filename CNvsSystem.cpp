/*!
	\file
	\brief Class for working with NVS.
	\authors Bliznets R.A. (r.bliznets@gmail.com)
	\version 1.2.0.0
	\date 02.05.2024
*/

#include "CNvsSystem.h"
#include "esp_log.h"
#include <cstdio>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"

// Depends on json, assuming it's declared globally or in header
using json = nlohmann::json; // Example, if using nlohmann/json

static const char *TAG = "nvs";

// Static class members
bool CNvsSystem::nvs2 = false;
bool CNvsSystem::nvs2_lock = true;

// Need to include headers for templates
#include <type_traits>
#include <cstring>

// === Special functions for strings and blob ===

/**
 * @brief Writes a string to the specified NVS namespace
 * @param ns Namespace ("nvs", "nvs2")
 * @param key Key (variable name)
 * @param value String value
 * @return true on success, false otherwise
 */
bool CNvsSystem::writeStringToNamespace(const char *ns, const std::string &key, const std::string &value)
{
	nvs_handle_t nvs_handle;
	esp_err_t err = nvs_open(ns, NVS_READWRITE, &nvs_handle);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to open NVS namespace %s: %s", ns, esp_err_to_name(err));
		return false;
	}

	err = nvs_set_str(nvs_handle, key.c_str(), value.c_str());
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to set string '%s' in %s: %s", key.c_str(), ns, esp_err_to_name(err));
		nvs_close(nvs_handle);
		return false;
	}

	err = nvs_commit(nvs_handle);
	nvs_close(nvs_handle);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to commit changes in %s: %s", ns, esp_err_to_name(err));
		return false;
	}
	return true;
}

/**
 * @brief Writes binary data to the specified NVS namespace
 * @param ns Namespace ("nvs", "nvs2")
 * @param key Key (variable name)
 * @param data Pointer to binary data
 * @param length Size of data
 * @return true on success, false otherwise
 */
bool CNvsSystem::writeBlobToNamespace(const char *ns, const std::string &key, const uint8_t *data, size_t length)
{
	nvs_handle_t nvs_handle;
	esp_err_t err = nvs_open(ns, NVS_READWRITE, &nvs_handle);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to open NVS namespace %s: %s", ns, esp_err_to_name(err));
		return false;
	}

	err = nvs_set_blob(nvs_handle, key.c_str(), data, length);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to set blob '%s' in %s: %s", key.c_str(), ns, esp_err_to_name(err));
		nvs_close(nvs_handle);
		return false;
	}

	err = nvs_commit(nvs_handle);
	nvs_close(nvs_handle);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to commit changes in %s: %s", ns, esp_err_to_name(err));
		return false;
	}
	return true;
}

/**
 * @brief Saves a string to NVS with backup storage support
 * @param name Variable name
 * @param value String value
 * @param mode Save mode (MAIN, BACKUP, BOTH)
 * @return Save result (NVS_MAIN, NVS_BACKUP, NVS_BOTH, NVS_NONE)
 */
uint16_t CNvsSystem::saveString(std::string &name, const std::string &value, uint16_t mode)
{
	if ((mode & NVS_MAIN) != 0)
	{
		if (!writeStringToNamespace("nvs", name, value))
		{
			return NVS_NONE;
		}
	}

	// Check: is backup allowed, is nvs2 available, and is it not locked
	if (((mode & NVS_BACKUP) == 0) || !nvs2 || nvs2_lock)
	{
		return mode;
	}

	if (!writeStringToNamespace("nvs2", name, value))
	{
		return mode & NVS_MAIN; // Main saved, backup failed
	}

	return mode;
}

/**
 * @brief Saves binary data to NVS with backup storage support
 * @param name Variable name
 * @param data Pointer to data
 * @param length Size of data
 * @param mode Save mode
 * @return Save result
 */
uint16_t CNvsSystem::saveBlob(std::string &name, const uint8_t *data, size_t length, uint16_t mode)
{
	if ((mode & NVS_MAIN) != 0)
	{
		if (!writeBlobToNamespace("nvs", name, data, length))
		{
			return NVS_NONE;
		}
	}

	if (((mode & NVS_BACKUP) == 0) || !nvs2 || nvs2_lock)
	{
		return mode;
	}

	if (!writeBlobToNamespace("nvs2", name, data, length))
	{
		return mode & NVS_MAIN;
	}

	return mode;
}

/**
 * @brief Restores a string from NVS with backup storage support
 * @param name Variable name
 * @param value Variable to receive the value
 * @param copy If true, value from backup will be copied to main
 * @return Restore result
 */
uint16_t CNvsSystem::restoreString(std::string &name, std::string &value, bool copy)
{
	nvs_handle_t nvs_handle;
	esp_err_t err;

	// Reading from main storage
	err = nvs_open("nvs", NVS_READONLY, &nvs_handle);
	if (err == ESP_OK)
	{
		size_t len = 0;
		err = nvs_get_str(nvs_handle, name.c_str(), nullptr, &len);
		if (err == ESP_OK)
		{
			value.resize(len - 1); // -1 for null-terminator
			err = nvs_get_str(nvs_handle, name.c_str(), value.data(), &len);
		}
		nvs_close(nvs_handle);
		if (err == ESP_OK)
		{
			return NVS_MAIN;
		}
	}

	// Reading from backup storage
	if (nvs2)
	{
		err = nvs_open("nvs2", NVS_READONLY, &nvs_handle);
		if (err == ESP_OK)
		{
			size_t len = 0;
			err = nvs_get_str(nvs_handle, name.c_str(), nullptr, &len);
			if (err == ESP_OK)
			{
				value.resize(len - 1);
				err = nvs_get_str(nvs_handle, name.c_str(), value.data(), &len);
			}
			nvs_close(nvs_handle);
			if (err == ESP_OK)
			{
				if (copy)
					saveString(name, value, NVS_MAIN);
				return NVS_BACKUP;
			}
		}
	}

	return NVS_NONE;
}

/**
 * @brief Restores binary data from NVS with backup storage support
 * @param name Variable name
 * @param data Vector to receive binary data
 * @param copy If true, value from backup will be copied to main
 * @return Restore result
 */
uint16_t CNvsSystem::restoreBlob(std::string &name, std::vector<uint8_t> &data, bool copy)
{
	nvs_handle_t nvs_handle;
	esp_err_t err;

	// Reading from main storage
	err = nvs_open("nvs", NVS_READONLY, &nvs_handle);
	if (err == ESP_OK)
	{
		size_t len = 0;
		err = nvs_get_blob(nvs_handle, name.c_str(), nullptr, &len);
		if (err == ESP_OK)
		{
			data.resize(len);
			err = nvs_get_blob(nvs_handle, name.c_str(), data.data(), &len);
		}
		nvs_close(nvs_handle);
		if (err == ESP_OK)
		{
			return NVS_MAIN;
		}
	}

	// Reading from backup storage
	if (nvs2)
	{
		err = nvs_open("nvs2", NVS_READONLY, &nvs_handle);
		if (err == ESP_OK)
		{
			size_t len = 0;
			err = nvs_get_blob(nvs_handle, name.c_str(), nullptr, &len);
			if (err == ESP_OK)
			{
				data.resize(len);
				err = nvs_get_blob(nvs_handle, name.c_str(), data.data(), &len);
			}
			nvs_close(nvs_handle);
			if (err == ESP_OK)
			{
				if (copy)
					saveBlob(name, data.data(), data.size(), NVS_MAIN);
				return NVS_BACKUP;
			}
		}
	}

	return NVS_NONE;
}

// === Public methods for strings and blob ===
uint16_t CNvsSystem::save(std::string &name, const std::string &value, uint16_t mode) { return saveString(name, value, mode); }
uint16_t CNvsSystem::save(std::string &name, const std::vector<uint8_t> &data, uint16_t mode) { return saveBlob(name, data.data(), data.size(), mode); }

uint16_t CNvsSystem::restore(std::string &name, std::string &value, bool copy) { return restoreString(name, value, copy); }
uint16_t CNvsSystem::restore(std::string &name, std::vector<uint8_t> &data, bool copy) { return restoreBlob(name, data, copy); }

// === Template methods for numeric types ===

/**
 * @brief Writes a numeric value to the specified NVS namespace
 * @tparam T Value type (uint8_t, float, double, etc.)
 * @param ns Namespace
 * @param key Key
 * @param value Value
 * @return true on success
 */
template <typename T>
bool CNvsSystem::writeValueToNamespace(const char *ns, const std::string &key, T value)
{
	nvs_handle_t nvs_handle;
	esp_err_t err = nvs_open(ns, NVS_READWRITE, &nvs_handle);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to open NVS namespace %s: %s", ns, esp_err_to_name(err));
		return false;
	}

	if constexpr (std::is_same_v<T, uint8_t>)
	{
		err = nvs_set_u8(nvs_handle, key.c_str(), value);
	}
	else if constexpr (std::is_same_v<T, int8_t>)
	{
		err = nvs_set_i8(nvs_handle, key.c_str(), value);
	}
	else if constexpr (std::is_same_v<T, uint16_t>)
	{
		err = nvs_set_u16(nvs_handle, key.c_str(), value);
	}
	else if constexpr (std::is_same_v<T, int16_t>)
	{
		err = nvs_set_i16(nvs_handle, key.c_str(), value);
	}
	else if constexpr (std::is_same_v<T, uint32_t>)
	{
		err = nvs_set_u32(nvs_handle, key.c_str(), value);
	}
	else if constexpr (std::is_same_v<T, int32_t>)
	{
		err = nvs_set_i32(nvs_handle, key.c_str(), value);
	}
	else if constexpr (std::is_same_v<T, float>)
	{
		uint32_t raw = 0;
		std::memcpy(&raw, &value, sizeof(float));
		err = nvs_set_u32(nvs_handle, key.c_str(), raw);
	}
	else if constexpr (std::is_same_v<T, double>)
	{
		uint64_t raw = 0;
		std::memcpy(&raw, &value, sizeof(double));
		err = nvs_set_u64(nvs_handle, key.c_str(), raw);
	}
	else
	{
		ESP_LOGE(TAG, "Unsupported type for NVS storage");
		nvs_close(nvs_handle);
		return false;
	}

	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to set value for key '%s' in %s: %s", key.c_str(), ns, esp_err_to_name(err));
		nvs_close(nvs_handle);
		return false;
	}

	err = nvs_commit(nvs_handle);
	nvs_close(nvs_handle);
	if (err != ESP_OK)
	{
		ESP_LOGE(TAG, "Failed to commit changes in %s: %s", ns, esp_err_to_name(err));
		return false;
	}
	return true;
}

/**
 * @brief Saves a numeric value to NVS with backup storage support
 * @tparam T Value type
 * @param name Variable name
 * @param value Value
 * @param mode Save mode
 * @return Save result
 */
template <typename T>
uint16_t CNvsSystem::saveValue(std::string &name, T value, uint16_t mode)
{
	if ((mode & NVS_MAIN) != 0)
	{
		if (!writeValueToNamespace("nvs", name, value))
		{
			return NVS_NONE;
		}
	}

	if (((mode & NVS_BACKUP) == 0) || !nvs2 || nvs2_lock)
	{
		return mode;
	}

	if (!writeValueToNamespace("nvs2", name, value))
	{
		return mode & NVS_MAIN;
	}

	return mode;
}

/**
 * @brief Restores a numeric value from NVS with backup storage support
 * @tparam T Value type
 * @param name Variable name
 * @param value Variable to receive the value
 * @param copy If true, value from backup will be copied to main
 * @return Restore result
 */
template <typename T>
uint16_t CNvsSystem::restoreValue(std::string &name, T &value, bool copy)
{
	nvs_handle_t nvs_handle;
	esp_err_t err;

	err = nvs_open("nvs", NVS_READONLY, &nvs_handle);
	if (err == ESP_OK)
	{
		if constexpr (std::is_same_v<T, uint8_t>)
		{
			err = nvs_get_u8(nvs_handle, name.c_str(), reinterpret_cast<uint8_t *>(&value));
		}
		else if constexpr (std::is_same_v<T, int8_t>)
		{
			err = nvs_get_i8(nvs_handle, name.c_str(), reinterpret_cast<int8_t *>(&value));
		}
		else if constexpr (std::is_same_v<T, uint16_t>)
		{
			err = nvs_get_u16(nvs_handle, name.c_str(), reinterpret_cast<uint16_t *>(&value));
		}
		else if constexpr (std::is_same_v<T, int16_t>)
		{
			err = nvs_get_i16(nvs_handle, name.c_str(), reinterpret_cast<int16_t *>(&value));
		}
		else if constexpr (std::is_same_v<T, uint32_t>)
		{
			err = nvs_get_u32(nvs_handle, name.c_str(), reinterpret_cast<uint32_t *>(&value));
		}
		else if constexpr (std::is_same_v<T, int32_t>)
		{
			err = nvs_get_i32(nvs_handle, name.c_str(), reinterpret_cast<int32_t *>(&value));
		}
		else if constexpr (std::is_same_v<T, float>)
		{
			uint32_t raw = 0;
			err = nvs_get_u32(nvs_handle, name.c_str(), &raw);
			if (err == ESP_OK)
			{
				std::memcpy(&value, &raw, sizeof(float));
			}
		}
		else if constexpr (std::is_same_v<T, double>)
		{
			uint64_t raw = 0;
			err = nvs_get_u64(nvs_handle, name.c_str(), &raw);
			if (err == ESP_OK)
			{
				std::memcpy(&value, &raw, sizeof(double));
			}
		}
		nvs_close(nvs_handle);
		if (err == ESP_OK)
		{
			return NVS_MAIN;
		}
	}

	if (nvs2)
	{
		err = nvs_open("nvs2", NVS_READONLY, &nvs_handle);
		if (err == ESP_OK)
		{
			if constexpr (std::is_same_v<T, uint8_t>)
			{
				err = nvs_get_u8(nvs_handle, name.c_str(), reinterpret_cast<uint8_t *>(&value));
			}
			else if constexpr (std::is_same_v<T, int8_t>)
			{
				err = nvs_get_i8(nvs_handle, name.c_str(), reinterpret_cast<int8_t *>(&value));
			}
			else if constexpr (std::is_same_v<T, uint16_t>)
			{
				err = nvs_get_u16(nvs_handle, name.c_str(), reinterpret_cast<uint16_t *>(&value));
			}
			else if constexpr (std::is_same_v<T, int16_t>)
			{
				err = nvs_get_i16(nvs_handle, name.c_str(), reinterpret_cast<int16_t *>(&value));
			}
			else if constexpr (std::is_same_v<T, uint32_t>)
			{
				err = nvs_get_u32(nvs_handle, name.c_str(), reinterpret_cast<uint32_t *>(&value));
			}
			else if constexpr (std::is_same_v<T, int32_t>)
			{
				err = nvs_get_i32(nvs_handle, name.c_str(), reinterpret_cast<int32_t *>(&value));
			}
			else if constexpr (std::is_same_v<T, float>)
			{
				uint32_t raw = 0;
				err = nvs_get_u32(nvs_handle, name.c_str(), &raw);
				if (err == ESP_OK)
				{
					std::memcpy(&value, &raw, sizeof(float));
				}
			}
			else if constexpr (std::is_same_v<T, double>)
			{
				uint64_t raw = 0;
				err = nvs_get_u64(nvs_handle, name.c_str(), &raw);
				if (err == ESP_OK)
				{
					std::memcpy(&value, &raw, sizeof(double));
				}
			}
			nvs_close(nvs_handle);
			if (err == ESP_OK)
			{
				if (copy)
					saveValue(name, value, NVS_MAIN);
				return NVS_BACKUP;
			}
		}
	}

	return NVS_NONE;
}

// === Public numeric methods ===
uint16_t CNvsSystem::save(std::string &name, uint8_t value, uint16_t mode) { return saveValue(name, value, mode); }
uint16_t CNvsSystem::save(std::string &name, int8_t value, uint16_t mode) { return saveValue(name, value, mode); }
uint16_t CNvsSystem::save(std::string &name, uint16_t value, uint16_t mode) { return saveValue(name, value, mode); }
uint16_t CNvsSystem::save(std::string &name, int16_t value, uint16_t mode) { return saveValue(name, value, mode); }
uint16_t CNvsSystem::save(std::string &name, uint32_t value, uint16_t mode) { return saveValue(name, value, mode); }
uint16_t CNvsSystem::save(std::string &name, int32_t value, uint16_t mode) { return saveValue(name, value, mode); }
uint16_t CNvsSystem::save(std::string &name, float value, uint16_t mode) { return saveValue(name, value, mode); }
uint16_t CNvsSystem::save(std::string &name, double value, uint16_t mode) { return saveValue(name, value, mode); }

uint16_t CNvsSystem::restore(std::string &name, uint8_t &value, bool copy) { return restoreValue(name, value, copy); }
uint16_t CNvsSystem::restore(std::string &name, int8_t &value, bool copy) { return restoreValue(name, value, copy); }
uint16_t CNvsSystem::restore(std::string &name, uint16_t &value, bool copy) { return restoreValue(name, value, copy); }
uint16_t CNvsSystem::restore(std::string &name, int16_t &value, bool copy) { return restoreValue(name, value, copy); }
uint16_t CNvsSystem::restore(std::string &name, uint32_t &value, bool copy) { return restoreValue(name, value, copy); }
uint16_t CNvsSystem::restore(std::string &name, int32_t &value, bool copy) { return restoreValue(name, value, copy); }
uint16_t CNvsSystem::restore(std::string &name, float &value, bool copy) { return restoreValue(name, value, copy); }
uint16_t CNvsSystem::restore(std::string &name, double &value, bool copy) { return restoreValue(name, value, copy); }

/**
 * @brief Initialize NVS (Non-Volatile Storage) memory
 * @return true if initialization is successful, false in case of error
 *
 * Function initializes NVS memory for main partition and additional partition "nvs2".
 * Performs memory cleanup if initialization errors are detected.
 */
bool CNvsSystem::init()
{
	esp_err_t err = nvs_flash_init();

	// Check if main NVS partition needs cleanup
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_LOGW(TAG, "nvs_flash_erase (%d)", err);
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}

	if (err == ESP_OK)
	{
		// Initialize additional NVS partition "nvs2"
		err = nvs_flash_init_partition("nvs2");
		if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
		{
			ESP_LOGW(TAG, "nvs_flash_erase (%d)", err);
			ESP_ERROR_CHECK(nvs_flash_erase_partition("nvs2"));
			err = nvs_flash_init_partition("nvs2");
			nvs2 = false;
		}
		else
		{
			nvs2 = true;
			uint8_t lock = 0;
			std::string str = "lock";
			uint16_t res = restore(str, lock, false); // Don't copy, just read
			nvs2_lock = ((res != NVS_NONE) && (lock > 0));
		}
	}
	else
	{
		ESP_LOGE(TAG, "nvs failed (%d)", err);
	}

	return (err == ESP_OK);
}

/**
 * @brief Deinitialize NVS memory
 *
 * Function releases NVS memory resources before shutdown.
 */
void CNvsSystem::free()
{
	nvs_flash_deinit();
}

/**
 * @brief Process NVS memory commands
 * @param cmd JSON command with NVS parameters
 * @param answer JSON response with command execution results
 *
 * Supported commands:
 * - "clear": clear NVS memory
 * - "reset": restart device
 * - Work with variables of various types (u8, i8, i16, i32, u32, float, double, string)
 */
void CNvsSystem::command(json &cmd, json &answer)
{
	if (cmd.contains("nvs"))
	{
		// Clear NVS memory command
		if (cmd["nvs"].contains("clear"))
		{
			esp_err_t err = nvs_flash_deinit();
			err |= nvs_flash_erase();
			err |= nvs_flash_init();
			answer["nvs"] = (err == ESP_OK ? 0 : 1);
		}
		// Device restart command
		else if (cmd["nvs"].contains("reset"))
		{
			esp_restart();
		}
		// Work with NVS variables
		else
		{
			// Check for variable name
			if (cmd["nvs"].contains("name") && cmd["nvs"]["name"].is_string())
			{
				std::string name = cmd["nvs"]["name"].template get<std::string>();
				answer["nvs"]["name"] = name;

				// Determine variable type (default u16)
				std::string tp = "u16";
				if (cmd["nvs"].contains("type") && cmd["nvs"]["type"].is_string())
				{
					tp = cmd["nvs"]["type"].template get<std::string>();
				}

				uint16_t mode = NVS_MAIN;
				if (cmd["nvs"].contains("mode") && cmd["nvs"]["mode"].is_number_unsigned())
				{
					mode = cmd["nvs"]["mode"].template get<uint16_t>();
					if (mode < NVS_MAIN)
						mode = NVS_MAIN;
					else if (mode > NVS_BOTH)
						mode = NVS_BOTH;
				}

				// === Type handling ===
				if (tp == "u8")
				{
					uint8_t val;
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number_unsigned())
					{
						val = cmd["nvs"]["value"].template get<uint8_t>();
						if (save(name, val, mode) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "save";
						}
					}
					else
					{
						if (restore(name, val, true) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "restore";
						}
					}
				}
				else if (tp == "i8")
				{
					int8_t val;
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number_integer())
					{
						val = cmd["nvs"]["value"].template get<int8_t>();
						if (save(name, val, mode) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "save";
						}
					}
					else
					{
						if (restore(name, val, true) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "restore";
						}
					}
				}
				else if (tp == "i16")
				{
					int16_t val;
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number_integer())
					{
						val = cmd["nvs"]["value"].template get<int16_t>();
						if (save(name, val, mode) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "save";
						}
					}
					else
					{
						if (restore(name, val, true) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "restore";
						}
					}
				}
				else if (tp == "i32")
				{
					int32_t val;
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number_integer())
					{
						val = cmd["nvs"]["value"].template get<int32_t>();
						if (save(name, val, mode) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "save";
						}
					}
					else
					{
						if (restore(name, val, true) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "restore";
						}
					}
				}
				else if (tp == "u32")
				{
					uint32_t val;
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number_unsigned())
					{
						val = cmd["nvs"]["value"].template get<uint32_t>();
						if (save(name, val, mode) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "save";
						}
					}
					else
					{
						if (restore(name, val, true) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "restore";
						}
					}
				}
				else if (tp == "float")
				{
					float val;
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number())
					{
						val = cmd["nvs"]["value"].template get<float>();
						if (save(name, val, mode) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "save";
						}
					}
					else
					{
						if (restore(name, val, true) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "restore";
						}
					}
				}
				else if (tp == "double")
				{
					double val;
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number())
					{
						val = cmd["nvs"]["value"].template get<double>();
						if (save(name, val, mode) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "save";
						}
					}
					else
					{
						if (restore(name, val, true) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "restore";
						}
					}
				}
				else if (tp == "string")
				{
					std::string val;
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_string())
					{
						val = cmd["nvs"]["value"].template get<std::string>();
						if (save(name, val, mode) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "save";
						}
					}
					else
					{
						if (restore(name, val, true) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "restore";
						}
					}
				}
				// Work with uint16_t variable type (default)
				else
				{
					uint16_t val;
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number_unsigned())
					{
						val = cmd["nvs"]["value"].template get<uint16_t>();
						if (save(name, val, mode) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "save";
						}
					}
					else
					{
						if (restore(name, val, true) != NVS_NONE)
						{
							answer["nvs"]["value"] = val;
						}
						else
						{
							answer["nvs"]["error"] = "restore";
						}
					}
				}
			}

			// Handle "lock" command
			if (cmd["nvs"].contains("lock"))
			{
				uint8_t lock = 1;
				std::string str = "lock";
				uint16_t res = save(str, lock, NVS_BACKUP);
				answer["nvs"]["lock"] = res;
				nvs2_lock = true;
			}
		}
	}
}