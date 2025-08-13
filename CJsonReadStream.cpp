/*!
    \file
    \brief Класс для обнаружения json строки из потока байтов.
    \authors Близнец Р.А. (r.bliznets@gmail.com)
    \version 1.0.0.0
    \date 18.04.2022
*/

#include "CJsonReadStream.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "json";

CJsonReadStream::CJsonReadStream(uint16_t max_size, bool auto_free) : mSize(max_size), mFree(auto_free)
{
}

CJsonReadStream::~CJsonReadStream()
{
    free();
    mStrings.clear();
}

void CJsonReadStream::free()
{
    if (mBuf != nullptr)
    {
        heap_caps_free(mBuf);
        mBuf = nullptr;
    }
    mCount = 0;
}

bool CJsonReadStream::add(uint8_t *data, uint16_t size)
{
    int start = -1;
    int i;
    for (i = 0; i < size; i++)
    {
        if (mCount == 0)
        {
            if (data[i] == '{')
            {
                mCount++;
                start = i;
            }
        }
        else
        {
            if (data[i] == '{')
            {
                mCount++;
            }
            else if (data[i] == '}')
            {
                mCount--;
                if (mCount == 0)
                {
                    if (mBuf == nullptr)
                    {
#ifdef CONFIG_DATAFORMAT_BUFFERS_INPSRAM
                        mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_SPIRAM);
#else
                        mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_DEFAULT);
#endif
                        mBufIndex = 0;
                    }
                    if (start != -1)
                    {
                        if ((i - start + 1) < mSize)
                        {
                            std::memcpy(mBuf, &data[start], i - start + 1);
                            mBuf[i - start + 1] = 0;
                            std::string s((char *)mBuf);
                            mStrings.push_back(s);
                        }
                        else
                        {
                            ESP_LOGW(TAG, "datasize %d > bufsize %d", (i - start + 1), mSize);
                        }
                        start = -1;
                    }
                    else
                    {
                        if ((i + mBufIndex) < mSize)
                        {
                            std::memcpy(&mBuf[mBufIndex], data, i + 1);
                            mBufIndex += i + 1;
                            mBuf[mBufIndex] = 0;
                            std::string s((char *)mBuf);
                            mStrings.push_back(s);
                        }
                        else
                        {
                            ESP_LOGW(TAG, "datasize %d > bufsize %d", (i + mBufIndex), mSize);
                        }
                        mBufIndex = 0;
                    }
                }
            }
        }
    }

    if (mCount != 0)
    {
        if (start != -1)
        {
            if (mBuf == nullptr)
            {
#ifdef CONFIG_DATAFORMAT_BUFFERS_INPSRAM
                mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_SPIRAM);
#else
                mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_DEFAULT);
#endif
                mBufIndex = 0;
            }
            if ((size - start) < mSize)
            {
                std::memcpy(mBuf, &data[start], (size - start));
                mBufIndex = (size - start);
            }
            else
            {
                ESP_LOGW(TAG, "datasize %d > bufsize %d", (size - start), mSize);
             }
        }
        else
        {
            if ((size + mBufIndex) < mSize)
            {
                std::memcpy(&mBuf[mBufIndex], data, size);
                mBufIndex += size;
            }
            else
            {
                ESP_LOGW(TAG, "datasize %d > bufsize %d", (size + mBufIndex), mSize);
            }
        }
    }
    else
    {
        if (mFree && (mBuf != nullptr))
        {
            heap_caps_free(mBuf);
            mBuf = nullptr;
        }
    }
    return (mCount != 0);
}

bool CJsonReadStream::get(std::string &str)
{
    if (mStrings.size() != 0)
    {
        str = mStrings.front();
        mStrings.pop_front();
        return true;
    }
    else
        return false;
}
