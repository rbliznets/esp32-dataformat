/*!
    \file
    \brief Класс для работы с буфером PSRAM.
    \authors Близнец Р.А. (r.bliznets@gmail.com)
    \version 0.0.0.1
    \date 21.02.2024
*/

#include "CBufferSystem.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "buf";

bool CBufferSystem::init(uint32_t size)
{
    free();
#ifdef CONFIG_SPIRAM
    mBuffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
#else
    mBuffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_DEFAULT);
#endif
    if (mBuffer != nullptr)
    {
        mSize = size;
        mParts.clear();
        return true;
    }
    else
        return false;
}

void CBufferSystem::free()
{
    if (mBuffer != nullptr)
    {
        heap_caps_free(mBuffer);
        mBuffer = nullptr;
    }
}

std::string CBufferSystem::command(CJsonParser *cmd)
{
    std::string answer = "";
    int t2;
    int x;
    if (cmd->getObject(1, "buf", t2))
    {
        answer = "\"buf\":{";

        if (cmd->getInt(t2, "create", x))
        {
            if (init(x))
            {
                if (cmd->getInt(t2, "part", x))
                    mPart = x;
                else
                    mPart = BUF_PART_SIZE;
                mLastPart = mSize/mPart;
                answer += "\"ok\":\"Buf was created " + std::to_string(mSize) + "(" + std::to_string(mPart) + ")" + "\"";
            }
            else
            {
                answer += "\"error\":\"Buf wasn't created " + std::to_string(x) + "\"";
            }
        }
        answer += '}';
    }
    return answer;
}

void CBufferSystem::addData(uint8_t* data, uint32_t size)
{
    if(mBuffer != nullptr)
    {
        uint16_t part = data[0]+data[1]*256;
        if(part < mLastPart)
        {
            if(size == (mPart + 2))
            {
                std::memcpy(&mBuffer[part*mPart],&data[2],mPart);
                mParts.push_back(part);
            }
            else
                ESP_LOGE(TAG,"size %d != %d for %d",(size-2),mPart,part);
        }
    }
    else
        ESP_LOGE(TAG,"mBuffer == null");
}