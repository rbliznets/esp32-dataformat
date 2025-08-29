/*!
    \file
    \brief Класс для конвертации JSON <-> CBOR.
    \authors Близнец Р.А. (r.bliznets@gmail.com)
    \version 0.0.0.1
    \date 29.08.2025
*/

#include "CJsonConvertor.h"
#include "esp_log.h"

static const char *TAG = "conv";

#ifdef CONFIG_CBOR_BINARY_FIELD
void CJsonConvertor::data2bin(json &item)
{
	for (auto &[key, val] : item.items())
	{
		if ((key == CONFIG_CBOR_BINARY_FIELD_NAME) && (val.is_string()))
		{
			std::string hexString = val.template get<std::string>();
			std::vector<uint8_t> data(hexString.length() / 2);
			bool convert = true;
			// Преобразование HEX-строки в бинарные данные
			for (size_t i = 0; i < hexString.length(); i += 2)
			{
				std::string byteStr = hexString.substr(i, 2);
				try
				{
					// Convert the two-character hex string to an integer with base 16
					uint8_t byteValue = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
					data[i >> 1] = byteValue;
				}
				catch (const std::invalid_argument &e)
				{
					convert = false;
					break;
				}
				catch (const std::out_of_range &e)
				{
					convert = false;
					break;
				}
			}
			if (convert)
			{
				val = json::binary(data);
			}
		}
		else if ((val.is_object() || val.is_array()))
		{
			data2bin(val);
		}
	}
}

void CJsonConvertor::bin2data(json &item)
{
	for (auto &[key, val] : item.items())
	{
		if (val.is_binary())
		{
			auto& data = val.get_binary();
			std::string str = "";
			// Преобразование бинарных данных в HEX-строку
			char tmp[4];
			for (size_t i = 0; i < data.size(); i++)
			{
				std::sprintf(tmp, "%02x", data[i]);
				str += tmp;
			}
			val = str;
		}
		else if ((val.is_object() || val.is_array()))
		{
			bin2data(val);
		}
	}
}
#endif

std::vector<uint8_t> CJsonConvertor::Json2Cbor(json& src)
{
    std::vector<std::uint8_t> res;
#ifdef CONFIG_CBOR_BINARY_FIELD
    data2bin(src);
#endif
    try
	{
        res = json::to_cbor(src);
    }
	catch (const std::exception &e)
	{
		ESP_LOGE(TAG, "%s", e.what());
	}
    return res;
}

json CJsonConvertor::Cbor2Json(std::vector<uint8_t>& src)
{
    json res;
    try
	{
        res = json::from_cbor(src);
#ifdef CONFIG_CBOR_BINARY_FIELD
        bin2data(res);
#endif
    }
	catch (const std::exception &e)
	{
		ESP_LOGE(TAG, "%s", e.what());
	}
    return res;
}
