/*!
	\file
	\brief Класс для работы с SPIFFS.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 1.3.0.0
	\date 12.12.2023
*/

#pragma once

#include "sdkconfig.h"
#include "CJsonParser.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/// Статические методы для работы с файловой системой.
class CSpiffsSystem
{
protected:
	static SemaphoreHandle_t mMutex; ///< Семафор захвата файловой системы
public:
	/// @brief Инициализация файловой системы.
	/// @param check флаг проверки на ошибки.
	/// @return true - если успех.
	static bool init(bool check = false);
	/// Закрытие файловой системы.
	static void free();
	/// Проверка на незавершенные транзакции и их очистка.
	static bool endTransaction();

	/// @brief Захват файловой системы задачей
	/// @param delay Время таймаута
	/// @return true - если захват успешен, false - если не удалось захватить.
	static bool lock(TickType_t delay = portMAX_DELAY)
	{
		return (xSemaphoreTake(mMutex, delay) == pdTRUE);
	};

	/// @brief Освобождения захвата файловой системы задачей
	static void unlock()
	{
		xSemaphoreGive(mMutex);
	};

	/// Обработка команды.
	/*!
	  \param[in] cmd json объектом spiffs в корне.
	  \return json строка с ответом (без обрамления в начале и конце {}), либо "".
	*/
	static std::string command(CJsonParser *cmd);
};
