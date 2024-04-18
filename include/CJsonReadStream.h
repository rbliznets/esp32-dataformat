/*!
    \file
    \brief Класс для обнаружения json строки из потока байтов.
    \authors Близнец Р.А. (r.bliznets@gmail.com)
    \version 0.0.0.1
    \date 18.04.2022
*/

#pragma once
#include "sdkconfig.h"

#include <string>
#include <cstring>
#include <list>

/// Класс для обнаружения json строки из потока байтов.
class CJsonReadStream
{
protected:
    uint8_t *mBuf = nullptr;
    uint16_t mBufIndex;
    uint16_t mSize;
    bool mFree;

    uint16_t mCount = 0;
    std::list<std::string> mStrings;

public:
    CJsonReadStream(uint16_t max_size, bool auto_free = true);
    ~CJsonReadStream();

    void free();
    uint16_t add(uint8_t *data, uint16_t size);
    bool get(std::string &str);
};