/*!
	\file
	\brief Class for working with NVS.
	\authors Bliznets R.A. (r.bliznets@gmail.com)
	\version 1.1.0.0
	\date 02.05.2024
*/

#include "CNvsSystem.h"
#include "esp_log.h"
#include <cstdio>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"

static const char *TAG = "nvs";

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
 * - Work with variables of various types (u8, i8, i16, i32, u32, float, double)
 */
void CNvsSystem::command(json &cmd, json &answer)
{
	if (cmd.contains("nvs"))
	{
		nvs_handle_t nvs_handle;
		error_t err;

		// Clear NVS memory command
		if (cmd["nvs"].contains("clear"))
		{
			err = nvs_flash_deinit();
			err |= nvs_flash_erase();
			err |= nvs_flash_init();
			answer["nvs"] = err;
		}
		// Device restart command
		else if (cmd["nvs"].contains("reset"))
		{
			esp_restart();
		}
		// Work with NVS variables
		else if (nvs_open("nvs", NVS_READWRITE, &nvs_handle) == ESP_OK)
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

				// Work with uint8_t variable type
				if (tp == "u8")
				{
					uint8_t u8;
					// Write value
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number_unsigned())
					{
						u8 = cmd["nvs"]["value"].template get<uint8_t>();
						if ((err = nvs_set_u8(nvs_handle, name.c_str(), u8)) == ESP_OK)
						{
							answer["nvs"]["value"] = u8;
							err = nvs_commit(nvs_handle); // Confirm write
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
					// Read value
					else
					{
						if ((err = nvs_get_u8(nvs_handle, name.c_str(), &u8)) == ESP_OK)
						{
							answer["nvs"]["value"] = u8;
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
				}
				// Work with int8_t variable type
				else if (tp == "i8")
				{
					int8_t i8;
					// Write value
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number_integer())
					{
						i8 = cmd["nvs"]["value"].template get<int8_t>();
						if ((err = nvs_set_i8(nvs_handle, name.c_str(), i8)) == ESP_OK)
						{
							answer["nvs"]["value"] = i8;
							err = nvs_commit(nvs_handle);
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
					// Read value
					else
					{
						if ((err = nvs_get_i8(nvs_handle, name.c_str(), &i8)) == ESP_OK)
						{
							answer["nvs"]["value"] = i8;
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
				}
				// Work with int16_t variable type
				else if (tp == "i16")
				{
					int16_t i16;
					// Write value
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number_integer())
					{
						i16 = cmd["nvs"]["value"].template get<int16_t>();
						if ((err = nvs_set_i16(nvs_handle, name.c_str(), i16)) == ESP_OK)
						{
							answer["nvs"]["value"] = i16;
							err = nvs_commit(nvs_handle);
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
					// Read value
					else
					{
						if ((err = nvs_get_i16(nvs_handle, name.c_str(), &i16)) == ESP_OK)
						{
							answer["nvs"]["value"] = i16;
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
				}
				// Work with int32_t variable type
				else if (tp == "i32")
				{
					int32_t i32;
					// Write value
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number_integer())
					{
						i32 = cmd["nvs"]["value"].template get<int32_t>();
						if ((err = nvs_set_i32(nvs_handle, name.c_str(), i32)) == ESP_OK)
						{
							answer["nvs"]["value"] = i32;
							err = nvs_commit(nvs_handle);
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
					// Read value
					else
					{
						if ((err = nvs_get_i32(nvs_handle, name.c_str(), &i32)) == ESP_OK)
						{
							answer["nvs"]["value"] = i32;
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
				}
				// Work with uint32_t variable type
				else if (tp == "u32")
				{
					uint32_t u32;
					// Write value
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number_unsigned())
					{
						u32 = cmd["nvs"]["value"].template get<uint32_t>();
						if ((err = nvs_set_u32(nvs_handle, name.c_str(), u32)) == ESP_OK)
						{
							answer["nvs"]["value"] = u32;
							err = nvs_commit(nvs_handle);
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
					// Read value
					else
					{
						if ((err = nvs_get_u32(nvs_handle, name.c_str(), &u32)) == ESP_OK)
						{
							answer["nvs"]["value"] = u32;
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
				}
				// Work with float variable type
				else if (tp == "float")
				{
					float d;
					uint32_t u32;
					// Write value
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number())
					{
						d = cmd["nvs"]["value"].template get<float>();
						std::memcpy(&u32, &d, sizeof(d)); // Convert float to uint32_t for storage
						if ((err = nvs_set_u32(nvs_handle, name.c_str(), u32)) == ESP_OK)
						{
							answer["nvs"]["value"] = d;
							err = nvs_commit(nvs_handle);
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
					// Read value
					else
					{
						if ((err = nvs_get_u32(nvs_handle, name.c_str(), &u32)) == ESP_OK)
						{
							std::memcpy(&d, &u32, sizeof(d)); // Convert uint32_t back to float
							answer["nvs"]["value"] = d;
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
				}
				// Work with double variable type
				else if (tp == "double")
				{
					double d;
					uint64_t d64;
					// Write value
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number())
					{
						d = cmd["nvs"]["value"].template get<double>();
						std::memcpy(&d64, &d, sizeof(d)); // Convert double to uint64_t for storage
						if ((err = nvs_set_u64(nvs_handle, name.c_str(), d64)) == ESP_OK)
						{
							answer["nvs"]["value"] = d;
							err = nvs_commit(nvs_handle);
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
					// Read value
					else
					{
						if ((err = nvs_get_u64(nvs_handle, name.c_str(), &d64)) == ESP_OK)
						{
							std::memcpy(&d, &d64, sizeof(d)); // Convert uint64_t back to double
							answer["nvs"]["value"] = d;
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
				}
				// Work with uint16_t variable type (default)
				else
				{
					uint16_t u16;
					// Write value
					if (cmd["nvs"].contains("value") && cmd["nvs"]["value"].is_number_unsigned())
					{
						u16 = cmd["nvs"]["value"].template get<uint16_t>();
						if ((err = nvs_set_u16(nvs_handle, name.c_str(), u16)) == ESP_OK)
						{
							answer["nvs"]["value"] = u16;
							err = nvs_commit(nvs_handle);
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
					// Read value
					else
					{
						if ((err = nvs_get_u16(nvs_handle, name.c_str(), &u16)) == ESP_OK)
						{
							answer["nvs"]["value"] = u16;
						}
						else
						{
							answer["nvs"]["error"] = err;
						}
					}
				}

				// Check for errors
				if (err != ESP_OK)
				{
					answer["nvs"]["error"] = err;
				}
			}
			// Close NVS handle
			nvs_close(nvs_handle);
		}
	}
}