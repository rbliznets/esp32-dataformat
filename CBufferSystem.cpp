/*!
    \file
    \brief Класс для работы с буфером PSRAM.
    \authors Близнец Р.А. (r.bliznets@gmail.com)
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
 * @brief Инициализация буфера заданного размера
 * @param size Размер буфера в байтах
 * @return true если инициализация успешна, false в случае ошибки
 */
bool CBufferSystem::init(uint32_t size)
{
    // Освобождаем память, если буфер уже был создан
    free();
    
#ifdef CONFIG_DATAFORMAT_BUFFERS_INPSRAM
    // Выделяем память в PSRAM (внешняя память)
    mBuffer = (uint8_t *)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
#else
    // Выделяем память в обычной куче
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
 * @brief Освобождение памяти буфера
 */
void CBufferSystem::free()
{
    if (mBuffer != nullptr)
    {
        // Освобождаем буфер
        heap_caps_free(mBuffer);
        mBuffer = nullptr;
        
        // Освобождаем массив флагов частей
        if (mParts != nullptr)
        {
            delete[] mParts;
            mParts = nullptr;
        }
    }
}

/**
 * @brief Обработка команд для работы с буфером
 * @param cmd JSON-команда
 * @param answer JSON-ответ
 * @param cancel флаг отмены операции
 */
void CBufferSystem::command(json &cmd, json &answer, bool &cancel)
{
    cancel = false;
    
    // Проверяем наличие команды для буфера
    if (cmd.contains("buf"))
    {
        answer["buf"] = json::object();
        
        // Команда создания буфера
        if (cmd["buf"].contains("create") && cmd["buf"]["create"].is_number_unsigned())
        {
            uint32_t x = cmd["buf"]["create"].template get<uint32_t>();
            
            // Инициализируем буфер заданного размера
            if (init(x))
            {
                // Устанавливаем размер части буфера
                if (cmd["buf"].contains("part") && cmd["buf"]["part"].is_number_unsigned())
                {
                    mPart = cmd["buf"]["part"].template get<uint16_t>();
                }
                else
                {
                    mPart = BUF_PART_SIZE;
                }
                
                // Вычисляем номер последней части
                mLastPart = mSize / mPart;
                if (mSize % mPart == 0)
                    mLastPart--;
                
                // Создаем массив флагов для отслеживания заполненных частей
                mParts = new uint8_t[mLastPart + 1];
                std::memset(mParts, 0, mLastPart + 1);
                
                mRead = false;
                
                std::string str = "Buf wasn created ";
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
        // Команда проверки состояния буфера
        else if (cmd["buf"].contains("check"))
        {
            if (mParts == nullptr)
            {
                answer["buf"]["error"] = "Buf wasn't created";
            }
            else
            {
                // Формируем список незаполненных частей
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
        // Команда записи буфера в файл
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
                
                // Записываем буфер в файл SPIFFS
                if (!CSpiffsSystem::writeBuffer(str.c_str(), mBuffer, mSize))
                {
                    answer["buf"]["error"] = "Failed to write to file " + fname;
                }
                else
                {
                    // Опционально освобождаем буфер после записи
                    if (cmd["buf"].contains("free"))
                        free();
                    answer["buf"]["ok"] = "file " + fname + " was saved";
                }
            }
        }
        // Команда обновления OTA из буфера
        else if (cmd["buf"].contains("ota"))
        {
            if (mBuffer == nullptr)
            {
                answer["buf"]["error"] = "Buf wasn't created";
            }
            else
            {
                // Выполняем OTA обновление из содержимого буфера
                answer["buf"] = json::parse("{" + COTASystem::update(mBuffer, mSize) + "}");
                
                // Опционально освобождаем буфер после обновления
                if (cmd["buf"].contains("free"))
                    free();
            }
        }
        // Команда чтения буфера из файла
        else if (cmd["buf"].contains("rd") && cmd["buf"]["rd"].is_string())
        {
            std::string fname = cmd["buf"]["rd"].template get<std::string>();
            std::string str = "/spiffs/" + fname;
            
            // Открываем файл для чтения
            FILE *f = std::fopen(str.c_str(), "r");
            if (f == nullptr)
            {
                ESP_LOGW(TAG, "Failed to open file %s", fname.c_str());
                answer["buf"]["error"] = "Failed to open file " + fname;
            }
            else
            {
                answer["buf"]["fr"] = fname;
                
                // Получаем размер файла
                std::fseek(f, 0, SEEK_END);
                int32_t sz = std::ftell(f);
                
                // Создаем буфер под размер файла
                if (init(sz))
                {
                    // Устанавливаем размер части
                    if (cmd["buf"].contains("part") && cmd["buf"]["part"].is_number_unsigned())
                        mPart = cmd["buf"]["part"].template get<uint16_t>();
                    else
                        mPart = BUF_PART_SIZE;
                    
                    mLastPart = mSize / mPart;
                    if (mSize % mPart == 0)
                        mLastPart--;
                    
                    // Читаем данные из файла в буфер
                    std::fseek(f, 0, SEEK_SET);
                    size_t sz = std::fread(mBuffer, 1, mSize, f);
                    if (sz == mSize)
                    {
                        // Инициализируем массив флагов - все части заполнены
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
        // Команда освобождения буфера
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
        // Команда отмены с освобождением буфера
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
 * @brief Добавление данных в буфер
 * @param data указатель на данные (первые 2 байта - номер части)
 * @param size размер данных
 */
void CBufferSystem::addData(uint8_t *data, uint32_t size)
{
    if ((mBuffer != nullptr) && (mParts != nullptr))
    {
        // Извлекаем номер части из первых двух байт данных
        uint16_t part = data[0] + data[1] * 256;
        
        // Проверяем, что номер части в допустимом диапазоне
        if (part < mLastPart)
        {
            // Проверяем размер данных для обычной части
            if (size == (mPart + 2))
            {
                // Копируем данные в соответствующую позицию буфера
                std::memcpy(&mBuffer[part * mPart], &data[2], mPart);
                
                // Проверяем, не перезаписываем ли мы уже существующую часть
                if (mParts[part] != 0)
                    ESP_LOGW(TAG, "rewrite part %d", part);
                else
                    mParts[part] = 1; // Помечаем часть как заполненную
            }
            else
                ESP_LOGE(TAG, "size %ld != %d for %d", (size - 2), mPart, part);
        }
        // Обработка последней части (может быть меньшего размера)
        else if (part == mLastPart)
        {
            uint32_t sz = mSize - mLastPart * mPart; // Размер последней части
            if (size == (sz + 2))
            {
                // Копируем данные последней части
                std::memcpy(&mBuffer[part * mPart], &data[2], sz);
                
                // Проверяем перезапись
                if (mParts[part] != 0)
                    ESP_LOGW(TAG, "rewrite part %d", part);
                else
                    mParts[part] = 1; // Помечаем как заполненную
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
 * @brief Получение данных из буфера по частям
 * @param size размер возвращаемых данных
 * @param index номер части
 * @return указатель на данные или nullptr если данных нет
 */
uint8_t *CBufferSystem::getData(uint32_t &size, uint16_t &index)
{
    uint8_t *res = nullptr;
    
    // Проверяем, что буфер загружен и массив частей существует
    if (mRead && (mParts != nullptr))
    {
        // Ищем первую заполненную часть
        for (int i = 0; i <= mLastPart; i++)
        {
            if (mParts[i] == 1)
            {
                mParts[i] = 0; // Помечаем часть как прочитанную
                
                // Определяем размер части
                if (i < mLastPart)
                {
                    size = mPart; // Обычная часть
                }
                else
                {
                    size = mSize - i * mPart; // Последняя часть
                }
                
                res = &mBuffer[i * mPart]; // Указатель на данные
                index = i; // Номер части
                break;
            }
        }
    }
    return res;
}