/*!
    \file
    \brief Класс для обнаружения json строки из потока байтов.
    \authors Близнец Р.А. (r.bliznets@gmail.com)
    \version 1.0.0.0
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
    uint8_t *mBuf = nullptr; ///< Буфер для json строки.
    uint16_t mBufIndex;      ///< Текущий размер данных в буфере.
    uint16_t mSize;          ///< Размер буфера.
    bool mFree;              ///< Флаг динамического удаления буфера.

    uint16_t mCount = 0;             ///< Количество непарных символов {.
    std::list<std::string> mStrings; ///< Список найденных json строк.

public:
    /// Конструктор.
    /*!
      \param[in] max_size Размер буфера.
      \param[in] auto_free Флаг динамического удаления буфера.
    */
    CJsonReadStream(uint16_t max_size, bool auto_free = true);
    /// Деструктор.
    ~CJsonReadStream();

    /// Очистить буфер.
    void free();
    /// Добавить данные для обработки.
    /*!
      \param[in] data данные.
      \param[in] size размер данных.
      \return true если ожидаются еще данные.
    */
    bool add(uint8_t *data, uint16_t size);
    /// Получить строку json.
    /*!
      \param[out] str строка json.
      \return true если есть строка.
    */
    bool get(std::string &str);
};