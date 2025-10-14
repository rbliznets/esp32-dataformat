/*!
	\file
	\brief Class for converting JSON <-> CBOR.
	\authors Bliznets R.A. (r.bliznets@gmail.com)
	\version 1.0.0.0
	\date 29.08.2025
*/

#include "CJsonConvertor.h"
#include "esp_log.h"

static const char *TAG = "conv";

#ifdef CONFIG_CBOR_BINARY_FIELD
/**
 * @brief Convert string data to binary format in JSON object
 * @param item JSON object to process
 *
 * This function recursively searches for fields with the configured binary field name
 * and converts their string values from hex representation to binary format.
 */
void CJsonConvertor::data2bin(json &item)
{
	// Iterate through all key-value pairs in the JSON object
	for (auto &[key, val] : item.items())
	{
		// Check if this is the configured binary field and it's a string
		if ((key == CONFIG_CBOR_BINARY_FIELD_NAME) && (val.is_string()))
		{
			// Get the hex string value
			std::string hexString = val.template get<std::string>();
			// Prepare vector for binary data (half the length of hex string)
			std::vector<uint8_t> data(hexString.length() / 2);
			bool convert = true;
			// Convert HEX string to binary data
			for (size_t i = 0; i < hexString.length(); i += 2)
			{
				// Extract two characters representing one hex byte
				std::string byteStr = hexString.substr(i, 2);
				try
				{
					// Convert the two-character hex string to an integer with base 16
					uint8_t byteValue = static_cast<uint8_t>(std::stoi(byteStr, nullptr, 16));
					data[i >> 1] = byteValue; // Store the converted byte
				}
				catch (const std::invalid_argument &e)
				{
					// Conversion failed due to invalid hex format
					convert = false;
					break;
				}
				catch (const std::out_of_range &e)
				{
					// Conversion failed due to out of range value
					convert = false;
					break;
				}
			}
			// If conversion was successful, replace string with binary data
			if (convert)
			{
				val = json::binary(data);
			}
		}
		// If value is object or array, recursively process its contents
		else if ((val.is_object() || val.is_array()))
		{
			data2bin(val);
		}
	}
}

/**
 * @brief Convert binary data to string format in JSON object
 * @param item JSON object to process
 *
 * This function recursively searches for binary data fields and converts them
 * back to hex string representation for JSON serialization.
 */
void CJsonConvertor::bin2data(json &item)
{
	// Iterate through all key-value pairs in the JSON object
	for (auto &[key, val] : item.items())
	{
		// Check if this value is binary data
		if (val.is_binary())
		{
			// Get reference to the binary data
			auto &data = val.get_binary();
			std::string str = "";
			// Convert binary data to HEX string
			char tmp[4];
			for (size_t i = 0; i < data.size(); i++)
			{
				// Format each byte as two-digit hex
				std::sprintf(tmp, "%02x", data[i]);
				str += tmp;
			}
			// Replace binary data with hex string
			val = str;
		}
		// If value is object or array, recursively process its contents
		else if ((val.is_object() || val.is_array()))
		{
			bin2data(val);
		}
	}
}
#endif

/**
 * @brief Convert JSON object to CBOR binary format
 * @param src Source JSON object to convert
 * @return Vector containing CBOR binary data
 *
 * Converts JSON to CBOR format, with optional binary field processing if enabled.
 */
std::vector<uint8_t> CJsonConvertor::Json2Cbor(json &src)
{
	std::vector<std::uint8_t> res;
#ifdef CONFIG_CBOR_BINARY_FIELD
	// Convert hex strings to binary data before CBOR conversion
	data2bin(src);
#endif
	try
	{
		// Perform the actual JSON to CBOR conversion
		res = json::to_cbor(src);
	}
	catch (const std::exception &e)
	{
		// Log conversion errors
		ESP_LOGE(TAG, "%s", e.what());
	}
	return res;
}

/**
 * @brief Convert CBOR binary data to JSON object
 * @param src Source CBOR binary data to convert
 * @return JSON object
 *
 * Converts CBOR binary data to JSON format, with optional binary field processing if enabled.
 */
json CJsonConvertor::Cbor2Json(std::vector<uint8_t> &src)
{
	json res;
	try
	{
		// Perform the actual CBOR to JSON conversion
		res = json::from_cbor(src);
#ifdef CONFIG_CBOR_BINARY_FIELD
		// Convert binary data back to hex strings after JSON conversion
		bin2data(res);
#endif
	}
	catch (const std::exception &e)
	{
		// Log conversion errors
		ESP_LOGE(TAG, "%s", e.what());
	}
	return res;
}