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
        if (mParts != nullptr)
        {
            delete[] mParts;
            mParts = nullptr;
        }
    }
}

std::string CBufferSystem::command(CJsonParser *cmd)
{
    std::string answer = "";
    int t2;
    int x;
    if (cmd->getObject(1, "buf", t2))
    {
        std::string fname;
        answer = "\"buf\":{";

        if (cmd->getInt(t2, "create", x))
        {
            if (init(x))
            {
                if (cmd->getInt(t2, "part", x))
                    mPart = x;
                else
                    mPart = BUF_PART_SIZE;
                mLastPart = mSize / mPart;
                if(mSize%mPart == 0)mLastPart--;
                mParts = new uint8_t[mLastPart + 1];
                std::memset(mParts, 0, mLastPart + 1);
                answer += "\"ok\":\"Buf was created " + std::to_string(mSize) + "(" + std::to_string(mPart) + ")" + "\"";
            }
            else
            {
                answer += "\"error\":\"Buf wasn't created " + std::to_string(x) + "\"";
            }
        }
        else if (cmd->getField(t2, "check"))
        {
            if (mParts == nullptr)
            {
                answer += "\"error\":\"Buf wasn't created\"";
            }
            else
            {
                answer += "\"empty\":[";
                bool f = true;
                for (int i = 0; i <= mLastPart; i++)
                {
                    if (mParts[i] == 0)
                    {
                        if (f)
                            f = false;
                        else
                            answer += ",";
                        answer += std::to_string(i);
                    }
                }
                answer += "]";
                answer += ",\"size\":" + std::to_string(mSize) + ",\"part\":" + std::to_string(mPart);
            }
        }
        else if (cmd->getString(t2, "wr", fname))
        {
            if (mBuffer == nullptr)
            {
                answer += "\"error\":\"Buf wasn't created\"";
            }
            else
            {
                std::string str = "/spiffs/" + fname;
                FILE *f = std::fopen(str.c_str(), "a");
                if (f == nullptr)
                {
                    ESP_LOGE(TAG, "Failed to open file %s", fname.c_str());
                    answer += "\"error\":\"Failed to open file " + fname + "\"";
                }
                else
                {
                    if (std::fwrite(mBuffer, 1, mSize, f) != mSize)
                    {
                        ESP_LOGE(TAG, "Failed to write to file %s(%ld)", fname.c_str(), mSize);
                        answer += "\"error\":\"Failed to write to file " + fname + "\"";
                    }
                    else
                    {
                        if (cmd->getField(t2, "free"))
                            free();
                        answer += "\"ok\":\"file " + fname + " was saved\"";
                    }
                    std::fclose(f);
                }
            }
        }
        else if (cmd->getString(t2, "rd", fname))
        {
            std::string str = "/spiffs/" + fname;
            FILE *f = std::fopen(str.c_str(), "r");
            if (f == nullptr)
            {
                ESP_LOGW(TAG, "Failed to open file %s", fname.c_str());
                answer += "\"error\":\"Failed to open file " + fname + "\"";
            }
            else
            {
                answer += "\"fr\":\"" + fname + "\",";
                std::fseek(f, 0, SEEK_END);
                int32_t sz = std::ftell(f);
                if (init(sz))
                {
                    if (cmd->getInt(t2, "part", x))
                        mPart = x;
                    else
                        mPart = BUF_PART_SIZE;
                    mLastPart = mSize / mPart;
                    if(mSize%mPart == 0)mLastPart--;
                    std::fseek(f, 0, SEEK_SET);
                    size_t sz = std::fread(mBuffer, 1, mSize, f);
                    if (sz == mSize)
                    {
                        mParts = new uint8_t[mLastPart + 1];
                        std::memset(mParts, 1, mLastPart + 1);
                        answer += "\"ok\":\"buffer was loaded from " + fname + "\"";
                        answer += ",\"size\":" + std::to_string(mSize) + ",\"part\":" + std::to_string(mPart);
                    }
                    else
                    {
                        free();
                        answer += "\"error\":\"Failed to read file " + fname + "\"";
                    }
                }
                else
                {
                    answer += "\"error\":\"Buf wasn't created " + std::to_string(x) + "\"";
                }
                std::fclose(f);
            }
        }
        else if (cmd->getField(t2, "free"))
        {
            if (mBuffer == nullptr)
            {
                answer += "\"error\":\"Buf wasn't created\"";
            }
            else
            {
                free();
                answer += "\"ok\":\"buffer was deleted\"";
            }
        }
        answer += '}';
    }
    return answer;
}

void CBufferSystem::addData(uint8_t *data, uint32_t size)
{
    if ((mBuffer != nullptr) && (mParts != nullptr))
    {
        uint16_t part = data[0] + data[1] * 256;
        if (part < mLastPart)
        {
            if (size == (mPart + 2))
            {
                std::memcpy(&mBuffer[part * mPart], &data[2], mPart);
                if (mParts[part] != 0)
                    ESP_LOGW(TAG, "rewrite part %d", part);
                else
                    mParts[part] = 1;
            }
            else
                ESP_LOGE(TAG, "size %ld != %d for %d", (size - 2), mPart, part);
        }
        else if (part == mLastPart)
        {
            uint32_t sz = mSize - mLastPart * mPart;
            if (size == (sz + 2))
            {
                std::memcpy(&mBuffer[part * mPart], &data[2], sz);
                if (mParts[part] != 0)
                    ESP_LOGW(TAG, "rewrite part %d", part);
                else
                    mParts[part] = 1;
            }
            else
                ESP_LOGE(TAG, "size %ld != %ld for %d", (size - 2), sz, part);
        }
        else
            ESP_LOGE(TAG, "part %d > %d", part, mPart);
    }
    else
        ESP_LOGE(TAG, "mBuffer == null");
}

uint8_t *CBufferSystem::getData(uint32_t &size, uint16_t &index)
{
    uint8_t *res = nullptr;
    if (mParts != nullptr)
    {
        for (int i = 0; i <= mLastPart; i++)
        {
            if (mParts[i] == 1)
            {
                mParts[i] = 0;
                if (i < mLastPart)
                {
                    size = mPart;
                }
                else
                {
                    size = mSize - i * mPart;
                }
                res = &mBuffer[i * mPart];
                index = i;
                break;
            }
        }
    }
    return res;
}