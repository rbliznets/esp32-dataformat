/*!
	\file
	\brief Класс для работы с обновлением firmware.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 1.0.0.0
	\date 27.04.2025
*/

#pragma once

#include "sdkconfig.h"

#include "freertos/FreeRTOS.h"
#include "esp_ota_ops.h"
#include <list>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

/// Тип функции обратного вызова для уведомления о процессе OTA обновления
/// @param lock true - начало OTA обновления, false - завершение OTA обновления
typedef void onOTAWork(bool lock);

/// Статический класс для работы с OTA (Over-The-Air) обновлением firmware.
/*!
  Предоставляет функции для выполнения OTA обновления firmware ESP32,
  включая пошаговое обновление через JSON команды и обновление из буфера данных.
  Поддерживает регистрацию callback-функций для уведомления о процессе обновления.
*/
class COTASystem
{
protected:
	static std::list<onOTAWork*> mWriteQueue; ///< Очередь callback-функций для уведомления о процессе OTA
	static void writeEvent(bool lock);        ///< Вызов всех callback-функций из очереди

	static esp_ota_handle_t update_handle;    ///< Дескриптор текущего процесса OTA обновления
    static int offset;                        ///< Текущее смещение в процессе записи данных

public:
    /// Инициализация OTA системы.
    /*!
      Проверяет состояние текущего раздела firmware и определяет необходимость подтверждения обновления.
      \return true если требуется подтверждение нового firmware, false в противном случае.
    */
    static bool init();
    
    /// Подтверждение или откат firmware.
    /*!
      Подтверждает успешную работу нового firmware или инициирует откат к предыдущей версии.
      \param ok true для подтверждения текущего firmware (по умолчанию), false для отката.
    */
    static void confirmFirmware(bool ok = true);

    /// Прерывание текущего процесса OTA обновления.
    /*!
      Аварийно завершает текущий процесс OTA обновления и сбрасывает состояние.
    */
    static void abort();

	/// Обработка JSON-команд OTA обновления.
	/*!
     * @brief Обработка команд JSON для OTA обновления
     * Поддерживаемые команды:
     * - "mode": режим работы ("begin", "end")
     * - "data": HEX-строка с данными firmware
     * 
     * @param cmd JSON-объект с командами OTA обновления (ключ "ota" в корне)
	 * @param[out] answer JSON-объект с результатами выполнения команд.
	*/
	static void command(json& cmd, json& answer);

    /// Выполнение OTA обновления из буфера данных.
    /*!
      Выполняет полное OTA обновление из переданного буфера данных.
      Используется для обновления из предварительно загруженного буфера.
      
      \param data Указатель на буфер с данными firmware
      \param size Размер данных в байтах
      \return JSON-строка с результатом операции ("ok" или "error").
    */
    static std::string update(uint8_t *data, uint32_t size);

	/// Добавление callback-функции для уведомления о процессе OTA.
	/*!
	  Регистрирует функцию, которая будет вызвана при начале и завершении OTA обновления.
	  \param event Указатель на callback-функцию типа onOTAWork.
	*/
	static void addWriteEvent(onOTAWork* event);
	
	/// Удаление callback-функции из уведомлений OTA.
	/*!
	  Удаляет указанную функцию из очереди уведомлений о процессе OTA обновления.
	  \param event Указатель на callback-функцию для удаления.
	*/
	static void removeWriteEvent(onOTAWork* event);
};