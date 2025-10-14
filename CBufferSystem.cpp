/*!
    \file
    \brief Class for working with PSRAM buffer.
    \authors Bliznets R.A. (r.bliznets@gmail.com)
    \version 1.2.0.0
    \date 21.02.2024
*/

#include "CBufferSystem.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "COTASystem.h"
#include "CSpiffsSystem.h"

static const char *TAG = "buf";

/**
 * @brief Initialize buffer with specified size
 * @param size Buffer size in bytes
 * @return true if initialization successful, false on error
 */
bool CBufferSystem::init(uint32_t size)
{
    // Free memory if buffer was already created
    free();

#ifdef CONFIG_DATAFORMAT_BUFFERS_INPSRAM
    // Allocate memory in PSRAM (external memory)
    mBuffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
#else
    // Allocate memory in regular heap
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

/**
 * @brief Free buffer memory
 */
void CBufferSystem::free()
{
    if (mBuffer != nullptr)
    {
        // Free buffer
        heap_caps_free(mBuffer);
        mBuffer = nullptr;

        // Free parts flags array
        if (mParts != nullptr)
        {
            delete[] mParts;
            mParts = nullptr;
        }
    }
}

/**
 * @brief Process commands for buffer operations
 * @param cmd JSON command
 * @param answer JSON response
 * @param cancel operation cancellation flag
 */
void CBufferSystem::command(json &cmd, json &answer, bool &cancel)
{
    cancel = false;

    // Check if buffer command exists
    if (cmd.contains("buf"))
    {
        answer["buf"] = json::object();

        // Buffer creation command
        if (cmd["buf"].contains("create") && cmd["buf"]["create"].is_number_unsigned())
        {
            uint32_t x = cmd["buf"]["create"].template get<uint32_t>();

            // Initialize buffer with specified size
            if (init(x))
            {
                // Set buffer part size
                if (cmd["buf"].contains("part") && cmd["buf"]["part"].is_number_unsigned())
                {
                    mPart = cmd["buf"]["part"].template get<uint16_t>();
                }
                else
                {
                    mPart = BUF_PART_SIZE;
                }

                // Calculate last part number
                mLastPart = mSize / mPart;
                if (mSize % mPart == 0)
                    mLastPart--;

                // Create array of flags to track filled parts
                mParts = new uint8_t[mLastPart + 1];
                std::memset(mParts, 0, mLastPart + 1);

                mRead = false;

                std::string str = "Buf was created ";
                str += std::to_string(mSize) + "(" + std::to_string(mPart) + ")";
                answer["buf"]["ok"] = str;
            }
            else
            {
                std::string str = "Buf wasn't created ";
                str += std::to_string(x);
                answer["buf"]["error"] = str;
            }
        }
        // Buffer status check command
        else if (cmd["buf"].contains("check"))
        {
            if (mParts == nullptr)
            {
                answer["buf"]["error"] = "Buf wasn't created";
            }
            else
            {
                // Form list of unfilled parts
                answer["buf"]["empty"] = json::array();
                for (int i = 0; i <= mLastPart; i++)
                {
                    if (mParts[i] == 0)
                    {
                        answer["buf"]["empty"] += std::to_string(i);
                    }
                }
                answer["buf"]["size"] = mSize;
                answer["buf"]["part"] = mPart;
            }
        }
        // Write buffer to file command
        else if (cmd["buf"].contains("wr") && cmd["buf"]["wr"].is_string())
        {
            if (mBuffer == nullptr)
            {
                answer["buf"]["error"] = "Buf wasn't created";
            }
            else
            {
                std::string fname = cmd["buf"]["wr"].template get<std::string>();
                std::string str = "/spiffs/" + fname;

                // Write buffer to SPIFFS file
                if (!CSpiffsSystem::writeBuffer(str.c_str(), mBuffer, mSize))
                {
                    answer["buf"]["error"] = "Failed to write to file " + fname;
                }
                else
                {
                    // Optionally free buffer after writing
                    if (cmd["buf"].contains("free"))
                        free();
                    answer["buf"]["ok"] = "file " + fname + " was saved";
                }
            }
        }
        // OTA update from buffer command
        else if (cmd["buf"].contains("ota"))
        {
            if (mBuffer == nullptr)
            {
                answer["buf"]["error"] = "Buf wasn't created";
            }
            else
            {
                // Perform OTA update from buffer content
                answer["buf"] = json::parse("{" + COTASystem::update(mBuffer, mSize) + "}");

                // Optionally free buffer after update
                if (cmd["buf"].contains("free"))
                    free();
            }
        }
        // Read buffer from file command
        else if (cmd["buf"].contains("rd") && cmd["buf"]["rd"].is_string())
        {
            std::string fname = cmd["buf"]["rd"].template get<std::string>();
            std::string str = "/spiffs/" + fname;

            // Open file for reading
            FILE *f = std::fopen(str.c_str(), "r");
            if (f == nullptr)
            {
                ESP_LOGW(TAG, "Failed to open file %s", fname.c_str());
                answer["buf"]["error"] = "Failed to open file " + fname;
            }
            else
            {
                answer["buf"]["fr"] = fname;

                // Get file size
                std::fseek(f, 0, SEEK_END);
                int32_t sz = std::ftell(f);

                // Create buffer for file size
                if (init(sz))
                {
                    // Set part size
                    if (cmd["buf"].contains("part") && cmd["buf"]["part"].is_number_unsigned())
                        mPart = cmd["buf"]["part"].template get<uint16_t>();
                    else
                        mPart = BUF_PART_SIZE;

                    mLastPart = mSize / mPart;
                    if (mSize % mPart == 0)
                        mLastPart--;

                    // Read data from file to buffer
                    std::fseek(f, 0, SEEK_SET);
                    size_t sz = std::fread(mBuffer, 1, mSize, f);
                    if (sz == mSize)
                    {
                        // Initialize flags array - all parts filled
                        mParts = new uint8_t[mLastPart + 1];
                        std::memset(mParts, 1, mLastPart + 1);
                        answer["buf"]["ok"] = "buffer was loaded from " + fname;
                        answer["buf"]["size"] = mSize;
                        answer["buf"]["part"] = mPart;
                        mRead = true;
                    }
                    else
                    {
                        free();
                        answer["buf"]["error"] = "Failed to read file " + fname;
                    }
                }
                else
                {
                    answer["buf"]["error"] = "Buf wasn't created " + std::to_string(sz);
                }
                std::fclose(f);
            }
        }
        // Free buffer command
        else if (cmd["buf"].contains("free"))
        {
            if (mBuffer == nullptr)
            {
                answer["buf"]["error"] = "Buf wasn't created";
            }
            else
            {
                free();
                answer["buf"]["ok"] = "buffer was deleted";
            }
        }
        // Cancel with buffer free command
        else if (cmd["buf"].contains("cancel"))
        {
            if (mBuffer == nullptr)
            {
                answer["buf"]["error"] = "Buf wasn't created";
            }
            else
            {
                free();
                answer["buf"]["ok"] = "buffer was deleted";
                cancel = true;
            }
        }
    }
}

/**
 * @brief Add data to buffer
 * @param data pointer to data (first 2 bytes are part number)
 * @param size data size
 */
void CBufferSystem::addData(uint8_t *data, uint32_t size)
{
    if ((mBuffer != nullptr) && (mParts != nullptr))
    {
        // Extract part number from first two data bytes
        uint16_t part = data[0] + data[1] * 256;

        // Check if part number is in valid range
        if (part < mLastPart)
        {
            // Check data size for regular part
            if (size == (mPart + 2))
            {
                // Copy data to appropriate buffer position
                std::memcpy(&mBuffer[part * mPart], &data[2], mPart);

                // Check if we're overwriting existing part
                if (mParts[part] != 0)
                    ESP_LOGW(TAG, "rewrite part %d", part);
                else
                    mParts[part] = 1; // Mark part as filled
            }
            else
                ESP_LOGE(TAG, "size %ld != %d for %d", (size - 2), mPart, part);
        }
        // Handle last part (may be smaller size)
        else if (part == mLastPart)
        {
            uint32_t sz = mSize - mLastPart * mPart; // Size of last part
            if (size == (sz + 2))
            {
                // Copy last part data
                std::memcpy(&mBuffer[part * mPart], &data[2], sz);

                // Check overwrite
                if (mParts[part] != 0)
                    ESP_LOGW(TAG, "rewrite part %d", part);
                else
                    mParts[part] = 1; // Mark as filled
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

/**
 * @brief Get data from buffer by parts
 * @param size returned data size
 * @param index part number
 * @return pointer to data or nullptr if no data available
 */
uint8_t *CBufferSystem::getData(uint32_t &size, uint16_t &index)
{
    uint8_t *res = nullptr;

    // Check if buffer is loaded and parts array exists
    if (mRead && (mParts != nullptr))
    {
        // Find first filled part
        for (int i = 0; i <= mLastPart; i++)
        {
            if (mParts[i] == 1)
            {
                mParts[i] = 0; // Mark part as read

                // Determine part size
                if (i < mLastPart)
                {
                    size = mPart; // Regular part
                }
                else
                {
                    size = mSize - i * mPart; // Last part
                }

                res = &mBuffer[i * mPart]; // Pointer to data
                index = i;                 // Part number
                break;
            }
        }
    }
    return res;
}