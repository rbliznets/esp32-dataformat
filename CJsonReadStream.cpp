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

/**
 * @brief Конструктор класса CJsonReadStream
 * @param max_size Максимальный размер буфера для хранения неполных JSON объектов
 * @param auto_free Флаг автоматического освобождения памяти буфера
 */
CJsonReadStream::CJsonReadStream(uint16_t max_size, bool auto_free) : mSize(max_size), mFree(auto_free)
{
}

/**
 * @brief Деструктор класса CJsonReadStream
 * Освобождает память и очищает список найденных JSON строк
 */
CJsonReadStream::~CJsonReadStream()
{
    free();
    mStrings.clear();
}

/**
 * @brief Освобождение памяти буфера
 * Освобождает выделенную память для временного буфера хранения неполных JSON объектов
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
 * @brief Добавление данных в поток и поиск JSON объектов
 * @param data Указатель на массив байт данных
 * @param size Размер данных в байтах
 * @return true если найден незавершенный JSON объект, false если все объекты завершены
 * 
 * Функция анализирует поток байт, отслеживает баланс открывающих и закрывающих фигурных скобок
 * для выделения полных JSON объектов. Неполные объекты сохраняются во временный буфер.
 */
bool CJsonReadStream::add(uint8_t *data, uint16_t size)
{
    int start = -1; // Индекс начала текущего JSON объекта в данных
    int i;
    
    // Обрабатываем каждый байт данных
    for (i = 0; i < size; i++)
    {
        if (mCount == 0)
        {
            // Ищем начало нового JSON объекта
            if (data[i] == '{')
            {
                mCount++; // Увеличиваем счетчик скобок
                start = i; // Запоминаем позицию начала
            }
        }
        else
        {
            // Внутри JSON объекта - отслеживаем баланс скобок
            if (data[i] == '{')
            {
                mCount++; // Вложенные объекты
            }
            else if (data[i] == '}')
            {
                mCount--; // Закрывающая скобка
                
                // Если объект завершен (баланс скобок = 0)
                if (mCount == 0)
                {
                    // Выделяем буфер при необходимости
                    if (mBuf == nullptr)
                    {
#ifdef CONFIG_DATAFORMAT_BUFFERS_INPSRAM
                        mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_SPIRAM);
#else
                        mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_DEFAULT);
#endif
                        mBufIndex = 0;
                    }
                    
                    // Обрабатываем завершенный JSON объект
                    if (start != -1)
                    {
                        // Объект полностью содержится в текущих данных
                        if ((i - start + 1) < mSize)
                        {
                            std::memcpy(mBuf, &data[start], i - start + 1);
                            mBuf[i - start + 1] = 0; // Добавляем терминатор строки
                            std::string s((char *)mBuf);
                            mStrings.push_back(s); // Добавляем в список найденных объектов
                        }
                        else
                        {
                            ESP_LOGW(TAG, "datasize %d > bufsize %d", (i - start + 1), mSize);
                        }
                        start = -1;
                    }
                    else
                    {
                        // Объект был начат в предыдущих данных, завершается в текущих
                        if ((i + mBufIndex) < mSize)
                        {
                            std::memcpy(&mBuf[mBufIndex], data, i + 1);
                            mBufIndex += i + 1;
                            mBuf[mBufIndex] = 0; // Добавляем терминатор строки
                            std::string s((char *)mBuf);
                            mStrings.push_back(s); // Добавляем в список найденных объектов
                        }
                        else
                        {
                            ESP_LOGW(TAG, "datasize %d > bufsize %d", (i + mBufIndex), mSize);
                        }
                        mBufIndex = 0; // Сбрасываем индекс буфера
                    }
                }
            }
        }
    }

    // Обработка незавершенных JSON объектов
    if (mCount != 0)
    {
        if (start != -1)
        {
            // Начало объекта найдено в текущих данных
            if (mBuf == nullptr)
            {
#ifdef CONFIG_DATAFORMAT_BUFFERS_INPSRAM
                mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_SPIRAM);
#else
                mBuf = (uint8_t *)heap_caps_malloc(mSize + 1, MALLOC_CAP_DEFAULT);
#endif
                mBufIndex = 0;
            }
            
            // Сохраняем неполные данные во временный буфер
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
            // Продолжение ранее начатого объекта
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
        // Все объекты завершены - освобождаем буфер при необходимости
        if (mFree && (mBuf != nullptr))
        {
            heap_caps_free(mBuf);
            mBuf = nullptr;
        }
    }
    
    return (mCount != 0); // Возвращаем true если есть незавершенные объекты
}

/**
 * @brief Получение следующей найденной JSON строки
 * @param str Ссылка на строку, куда будет записан результат
 * @return true если строка получена, false если список пуст
 * 
 * Функция извлекает первую найденную JSON строку из очереди и удаляет её из списка.
 */
bool CJsonReadStream::get(std::string &str)
{
    if (mStrings.size() != 0)
    {
        str = mStrings.front(); // Получаем первую строку
        mStrings.pop_front();   // Удаляем её из списка
        return true;
    }
    else
        return false;
}