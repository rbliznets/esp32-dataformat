/*!
    \file
    \brief Класс для конвертации JSON <-> CBOR.
    \authors Близнец Р.А. (r.bliznets@gmail.com)
    \version 0.0.0.1
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
    static void data2bin(json& item);
	static void bin2data(json &item);
#endif

public:
    static std::vector<uint8_t> Json2Cbor(json& src);
    static json Cbor2Json(std::vector<uint8_t>& src);
};