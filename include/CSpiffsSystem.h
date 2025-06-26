/*!
    \file
    \brief Класс для работы с SPIFFS.
    \authors Близнец Р.А. (r.bliznets@gmail.com)
    \version 1.4.0.0
    \date 12.12.2023
*/

#pragma once

#include "sdkconfig.h"
#include "CJsonParser.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h" 
#include <list>

// Тип функции обратного вызова для событий работы с SPIFFS
// lock: true - начало транзакции (запись), false - окончание транзакции
typedef void onSpiffsWork(bool lock);

/// Статические методы для работы с файловой системой SPIFFS
class CSpiffsSystem
{
protected:
    // Очередь обработчиков событий работы с SPIFFS
    // Используется для уведомления внешних модулей о начале/окончании операций записи
    static std::list<onSpiffsWork*> mWriteQueue;

    // Внутренний метод для генерации событий
    // lock: true - начало транзакции, false - окончание транзакции
    static void writeEvent(bool lock);

public:
    /*!
     * @brief Инициализация файловой системы SPIFFS
     * @param check Флаг проверки целостности файловой системы (по умолчанию false)
     * @return true - если инициализация прошла успешно, иначе false
     * 
     * При check = true выполняется проверка файловой системы на наличие ошибок
     */
    static bool init(bool check = false);

    /*!
     * @brief Освобождение ресурсов файловой системы
     * 
     * Выполняет деинициализацию SPIFFS и очищает внутренние структуры
     */
    static void free();

    /*!
     * @brief Проверка и завершение незавершенных транзакций
     * @return true - если все транзакции завершены успешно, иначе false
     * 
     * Используется для гарантии завершения операций ввода-вывода перед выходом из программы
     */
    static bool endTransaction();

    /*!
     * @brief Запись буфера данных в файл
     * @param fileName Имя файла для записи
     * @param data Указатель на данные для записи
     * @param size Размер данных в байтах
     * @return true - если запись прошла успешно, иначе false
     * 
     * Выполняет атомарную операцию записи с блокировкой файловой системы
     */
    static bool writeBuffer(const char *fileName, uint8_t *data, uint32_t size);

    /*!
     * @brief Обработка команд JSON для SPIFFS
     * @param cmd JSON-объект с командой в корне файловой системы
     * @return JSON-строка с ответом (без обрамления {}) или пустая строка при ошибке
     */
    static std::string command(CJsonParser *cmd);

    /*!
     * @brief Добавление обработчика события работы с SPIFFS
     * @param event Указатель на функцию-обработчик
     * 
     * Функция будет вызываться при начале/окончании операций записи
     */
    static void addWriteEvent(onSpiffsWork* event);

    /*!
     * @brief Удаление обработчика события работы с SPIFFS
     * @param event Указатель на функцию-обработчик
     * 
     * Удаляет ранее добавленный обработчик из очереди
     */
    static void removeWriteEvent(onSpiffsWork* event);
};