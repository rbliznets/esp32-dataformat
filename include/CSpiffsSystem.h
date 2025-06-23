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


typedef void onSpiffsWork(bool lock);

/// Статические методы для работы с файловой системой.
class CSpiffsSystem
{
protected:
	static std::list<onSpiffsWork*> mWriteQueue;

	static void writeEvent(bool lock);
public:
	/// @brief Инициализация файловой системы.
	/// @param check флаг проверки на ошибки.
	/// @return true - если успех.
	static bool init(bool check = false);
	/// Закрытие файловой системы.
	static void free();
	/// Проверка на незавершенные транзакции и их очистка.
	static bool endTransaction();

    static bool writeBuffer(const char *fileName, uint8_t *data, uint32_t size);

    /// Обработка команды.
	/*!
	  \param[in] cmd json объектом spiffs в корне.
	  \return json строка с ответом (без обрамления в начале и конце {}), либо "".
	*/
	static std::string command(CJsonParser *cmd);

	static void addWriteEvent(onSpiffsWork* event);
	static void removeWriteEvent(onSpiffsWork* event);
};
