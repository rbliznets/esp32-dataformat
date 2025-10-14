/*!
    \file
    \brief Class for converting JSON <-> CBOR.
    \authors Bliznets R.A. (r.bliznets@gmail.com)
    \version 1.0.0.0
    \date 29.08.2025
*/

#pragma once
#include "sdkconfig.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

class CJsonConvertor
{
protected:
#ifdef CONFIG_CBOR_BINARY_FIELD
    /**
     * @brief Convert hex string fields to binary data in JSON object
     * @param item JSON object to process
     *
     * Converts string values with the configured binary field name to binary format.
     */
    static void data2bin(json &item);

    /**
     * @brief Convert binary data fields to hex string in JSON object
     * @param item JSON object to process
     *
     * Converts binary data back to hex string representation for JSON serialization.
     */
    static void bin2data(json &item);
#endif

public:
    /**
     * @brief Convert JSON object to CBOR binary format
     * @param src Source JSON object to convert
     * @return Vector containing CBOR binary data
     */
    static std::vector<uint8_t> Json2Cbor(json &src);

    /**
     * @brief Convert CBOR binary data to JSON object
     * @param src Source CBOR binary data to convert
     * @return JSON object
     */
    static json Cbor2Json(std::vector<uint8_t> &src);
};