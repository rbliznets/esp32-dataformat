/*!
	\file
	\brief Класс для работы с NVS.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 0.1.0.0
	\date 02.05.2024
*/

#include "CNvsSystem.h"
#include "esp_log.h"
#include <cstdio>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"

static const char *TAG = "nvs";

void CNvsSystem::init()
{
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
	{
		ESP_LOGW(TAG, "nvs_flash_erase (%d)", err);
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
	}
	ESP_ERROR_CHECK(err);
}

void CNvsSystem::free()
{
	nvs_flash_deinit();
}

std::string CNvsSystem::command(CJsonParser *cmd)
{
	std::string answer = "";
	int t;
	if (cmd->getObject(1, "nvs", t))
	{
		nvs_handle_t nvs_handle;
		if (cmd->getField(t, "clear"))
		{
			nvs_flash_deinit();
			ESP_ERROR_CHECK(nvs_flash_erase());
			ESP_ERROR_CHECK(nvs_flash_init());
			answer = "\"nvs\":null";
		}
		else if (cmd->getField(t, "reset"))
		{
			esp_restart();
		}
		else if (nvs_open("nvs", NVS_READWRITE, &nvs_handle) == ESP_OK)
		{
			std::string name;
			if (cmd->getString(t, "name", name))
			{
				answer = "\"nvs\":{";
				answer += "\"name\":\"" + name + "\"";
				int value;
				std::string tp = "u16";
				cmd->getString(t, "type", tp);
				if (tp == "u8")
				{
					uint8_t u8;
					if (cmd->getInt(t, "value", value))
					{
						if (nvs_set_u8(nvs_handle, name.c_str(), (uint8_t)value) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(value);
						}
						ESP_ERROR_CHECK(nvs_commit(nvs_handle));
					}
					else
					{
						if (nvs_get_u8(nvs_handle, name.c_str(), &u8) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(u8);
						}
					}
				}
				else if (tp == "i8")
				{
					int8_t i8;
					if (cmd->getInt(t, "value", value))
					{
						if (nvs_set_i8(nvs_handle, name.c_str(), (int8_t)value) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(value);
						}
						ESP_ERROR_CHECK(nvs_commit(nvs_handle));
					}
					else
					{
						if (nvs_get_i8(nvs_handle, name.c_str(), &i8) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(i8);
						}
					}
				}
				else if (tp == "i16")
				{
					int16_t i16;
					if (cmd->getInt(t, "value", value))
					{
						if (nvs_set_i16(nvs_handle, name.c_str(), (int16_t)value) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(value);
						}
						ESP_ERROR_CHECK(nvs_commit(nvs_handle));
					}
					else
					{
						if (nvs_get_i16(nvs_handle, name.c_str(), &i16) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(i16);
						}
					}
				}
				else if (tp == "i32")
				{
					int32_t i32;
					if (cmd->getInt(t, "value", value))
					{
						if (nvs_set_i32(nvs_handle, name.c_str(), (int32_t)value) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(value);
						}
						ESP_ERROR_CHECK(nvs_commit(nvs_handle));
					}
					else
					{
						if (nvs_get_i32(nvs_handle, name.c_str(), &i32) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(i32);
						}
					}
				}
				else if (tp == "u32")
				{
					uint32_t u32;
					if (cmd->getInt(t, "value", value))
					{
						if (nvs_set_u32(nvs_handle, name.c_str(), (uint32_t)value) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(value);
						}
						ESP_ERROR_CHECK(nvs_commit(nvs_handle));
					}
					else
					{
						if (nvs_get_u32(nvs_handle, name.c_str(), &u32) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(u32);
						}
					}
				}
				else if (tp == "float")
				{
					float d;
					uint32_t u32;
					if (cmd->getFloat(t, "value", d))
					{
						std::memcpy(&u32, &d, sizeof(d));
						if (nvs_set_u32(nvs_handle, name.c_str(), u32) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(d);
						}
						ESP_ERROR_CHECK(nvs_commit(nvs_handle));
					}
					else
					{
						if (nvs_get_u32(nvs_handle, name.c_str(), &u32) == ESP_OK)
						{
							std::memcpy(&d, &u32, sizeof(d));
							answer += ",\"value\":" + std::to_string(d);
						}
					}
				}
				else if (tp == "double")
				{
					double d;
					uint64_t d64;
					if (cmd->getDouble(t, "value", d))
					{
						std::memcpy(&d64, &d, sizeof(d));
						if (nvs_set_u64(nvs_handle, name.c_str(), d64) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(d);
						}
						ESP_ERROR_CHECK(nvs_commit(nvs_handle));
					}
					else
					{
						if (nvs_get_u64(nvs_handle, name.c_str(), &d64) == ESP_OK)
						{
							std::memcpy(&d, &d64, sizeof(d));
							answer += ",\"value\":" + std::to_string(d);
						}
					}
				}
				else
				{
					uint16_t u16;
					if (cmd->getInt(t, "value", value))
					{
						if (nvs_set_u16(nvs_handle, name.c_str(), (uint16_t)value) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(value);
						}
						ESP_ERROR_CHECK(nvs_commit(nvs_handle));
					}
					else
					{
						if (nvs_get_u16(nvs_handle, name.c_str(), &u16) == ESP_OK)
						{
							answer += ",\"value\":" + std::to_string(u16);
						}
					}
				}
				answer += "}";
			}
			nvs_close(nvs_handle);
		}
	}
	return answer;
}
