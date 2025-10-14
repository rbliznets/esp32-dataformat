/*!
    \file
    \brief Class for detecting JSON string from byte stream.
    \authors Bliznets R.A. (r.bliznets@gmail.com)
    \version 1.0.0.0
    \date 18.04.2022
*/

#include "CJsonReadStream.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

static const char *TAG = "json";

/**
 * @brief Constructor for CJsonReadStream class
 * @param max_size Maximum buffer size for storing incomplete JSON objects
 * @param auto_free Automatic buffer memory deallocation flag
 */
CJsonReadStream::CJsonReadStream(uint16_t max_size, bool auto_free) : mSize(max_size), mFree(auto_free)
{
}

/**
 * @brief Destructor for CJsonReadStream class
 * Frees memory and clears the list of found JSON strings
 */
CJsonReadStream::~CJsonReadStream()
{
    free();
    mStrings.clear();
}

/**
 * @brief Free buffer memory
 * Frees allocated memory for temporary buffer storing incomplete JSON objects
 */
void CJsonReadStream::free()
{
    if (mBuf != nullptr)
    {
        heap_caps_free(mBuf);
        mBuf = nullptr;
    }
    mCount = 0;
}

/**
 * @brief Add data to stream and search for JSON objects
 * @param data Pointer to byte array data
 * @param size Data size in bytes
 * @return true if incomplete JSON object found, false if all objects are complete
 *
 * Function analyzes byte stream, tracks balance of opening and closing curly braces
 * to extract complete JSON objects. Incomplete objects are saved to temporary buffer.
 */
bool CJsonReadStream::add(uint8_t *data, uint16_t size)
{
    int start = -1; // Index of current JSON object start in data
    int i;

    // Process each byte of data
    for (i = 0; i < size; i++)
    {
        if (mCount == 0)
        {
            // Looking for start of new JSON object
            if (data[i] == '{')
            {
                mCount++;  // Increment bracket counter
                start = i; // Remember start position
            }
        }
        else
        {
            // Inside JSON object - track bracket balance
            if (data[i] == '{')
            {
                mCount++; // Nested objects
            }
            else if (data[i] == '}')
            {
                mCount--; // Closing bracket

                // If object is complete (bracket balance = 0)
                if (mCount == 0)
                {
                    // Allocate buffer if needed
                    if (mBuf == nullptr)
                    {
#ifdef CONFIG_DATAFORMAT_BUFFERS_INPSRAM
                        mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_SPIRAM);
#else
                        mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_DEFAULT);
#endif
                        mBufIndex = 0;
                    }

                    // Process completed JSON object
                    if (start != -1)
                    {
                        // Object completely contained in current data
                        if ((i - start + 1) < mSize)
                        {
                            std::memcpy(mBuf, &data[start], i - start + 1);
                            mBuf[i - start + 1] = 0; // Add string terminator
                            std::string s((char *)mBuf);
                            mStrings.push_back(s); // Add to list of found objects
                        }
                        else
                        {
                            ESP_LOGW(TAG, "datasize %d > bufsize %d", (i - start + 1), mSize);
                        }
                        start = -1;
                    }
                    else
                    {
                        // Object started in previous data, ends in current
                        if ((i + mBufIndex) < mSize)
                        {
                            std::memcpy(&mBuf[mBufIndex], data, i + 1);
                            mBufIndex += i + 1;
                            mBuf[mBufIndex] = 0; // Add string terminator
                            std::string s((char *)mBuf);
                            mStrings.push_back(s); // Add to list of found objects
                        }
                        else
                        {
                            ESP_LOGW(TAG, "datasize %d > bufsize %d", (i + mBufIndex), mSize);
                        }
                        mBufIndex = 0; // Reset buffer index
                    }
                }
            }
        }
    }

    // Process incomplete JSON objects
    if (mCount != 0)
    {
        if (start != -1)
        {
            // Object start found in current data
            if (mBuf == nullptr)
            {
#ifdef CONFIG_DATAFORMAT_BUFFERS_INPSRAM
                mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_SPIRAM);
#else
                mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_DEFAULT);
#endif
                mBufIndex = 0;
            }

            // Save incomplete data to temporary buffer
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
            // Continuation of previously started object
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
        // All objects completed - free buffer if needed
        if (mFree && (mBuf != nullptr))
        {
            heap_caps_free(mBuf);
            mBuf = nullptr;
        }
    }

    return (mCount != 0); // Return true if there are incomplete objects
}

/**
 * @brief Get next found JSON string
 * @param str Reference to string where result will be written
 * @return true if string obtained, false if queue is empty
 *
 * Function extracts first found JSON string from queue and removes it from list.
 */
bool CJsonReadStream::get(std::string &str)
{
    if (mStrings.size() != 0)
    {
        str = mStrings.front(); // Get first string
        mStrings.pop_front();   // Remove it from list
        return true;
    }
    else
        return false;
}