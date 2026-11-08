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
 * @param resync Restart on "{number}" found inside incomplete JSON object
 */
CJsonReadStream::CJsonReadStream(uint16_t max_size, bool auto_free, bool resync) : mSize(max_size), mFree(auto_free), mResync(resync)
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
    mMark = 0;
    mBufIndex = 0;
    mOverflow = false;
}

/**
 * @brief Append part of the current JSON object to the buffer
 * @param data Pointer to data
 * @param size Data size in bytes
 *
 * If the object does not fit into the buffer, it is marked as overflowed: the rest of it
 * is skipped and the whole object is dropped when its closing brace is found.
 * Partial copy would glue the head and the tail of the object into a valid-looking JSON string.
 */
void CJsonReadStream::append(const uint8_t *data, int size)
{
    if (mOverflow)
        return;

    if (mBuf == nullptr)
    {
#ifdef CONFIG_DATAFORMAT_BUFFERS_INPSRAM
        mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_SPIRAM);
#else
        mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_DEFAULT);
#endif
        mBufIndex = 0;
        if (mBuf == nullptr)
        {
            ESP_LOGW(TAG, "no memory for %d bytes, json dropped", mSize + 1);
            mOverflow = true;
            return;
        }
    }

    if ((mBufIndex + size) > mSize)
    {
        ESP_LOGW(TAG, "datasize %d > bufsize %d, json dropped", (mBufIndex + size), mSize);
        mOverflow = true;
        return;
    }

    std::memcpy(&mBuf[mBufIndex], data, size);
    mBufIndex += size;
}

/**
 * @brief Add data to stream and search for JSON objects
 * @param data Pointer to byte array data
 * @param size Data size in bytes
 * @return true if incomplete JSON object found, false if all objects are complete
 *
 * Function analyzes byte stream, tracks balance of opening and closing curly braces
 * to extract complete JSON objects. Incomplete objects are saved to temporary buffer.
 * An object longer than mSize is dropped entirely.
 */
bool CJsonReadStream::add(uint8_t *data, uint16_t size)
{
    int start = -1; // Index of current JSON object start in data (-1 - started in previous data)
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
                mBufIndex = 0;
                mOverflow = false;
            }
        }
        else
        {
            // Inside JSON object - track bracket balance
            if (data[i] == '{')
            {
                mCount++;                // Nested objects
                mMark = mResync ? 1 : 0; // Possible start of "{number}"
            }
            else if ((mMark != 0) && (mMark < (sizeof(mMarkBuf) - 1)) && (data[i] >= '0') && (data[i] <= '9'))
            {
                mMarkBuf[mMark++] = data[i];
            }
            else if ((mMark > 1) && (data[i] == '}'))
            {
                // "{number}" is never a part of JSON object: the sender has started a new frame.
                // The incomplete object is dropped, the number is returned as a complete string.
                mMarkBuf[0] = '{';
                mMarkBuf[mMark] = '}';
                mStrings.push_back(std::string(mMarkBuf, mMark + 1));
                mMark = 0;
                mCount = 0;
                mBufIndex = 0;
                mOverflow = false;
                start = -1;
            }
            else if (data[i] == '}')
            {
                mMark = 0;
                mCount--; // Closing bracket

                // If object is complete (bracket balance = 0)
                if (mCount == 0)
                {
                    // Object either completely contained in current data or started in previous data
                    if (start != -1)
                        append(&data[start], i - start + 1);
                    else
                        append(data, i + 1);

                    if (!mOverflow)
                    {
                        mBuf[mBufIndex] = 0;                          // Add string terminator
                        mStrings.push_back(std::string((char *)mBuf)); // Add to list of found objects
                    }
                    mBufIndex = 0;
                    mOverflow = false;
                    start = -1;
                }
            }
            else
            {
                mMark = 0;
            }
        }
    }

    // Save incomplete JSON object to temporary buffer
    if (mCount != 0)
    {
        if (start != -1)
            append(&data[start], size - start); // Object start found in current data
        else
            append(data, size); // Continuation of previously started object
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