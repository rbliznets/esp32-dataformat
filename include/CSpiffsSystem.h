/*!
	\file
	\brief Класс для работы с SPIFFS.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 1.1.0.0
	\date 12.12.2023
*/

#pragma once

#include "sdkconfig.h"
#include "CJsonParser.h"

/// Статические методы для работы с файловой системой.
class CSpiffsSystem
{
public:
	/// Инициализация файловой системы.
	/*!
	  \param[in] check флаг проверки на ошибки.
	*/
	static void init(bool check = false);
	/// Закрытие файловой системы.
	static void free();
	/// Проверка на незавершенные транзакции и их очистка.
	static bool endTransaction();

	/// Обработка команды.
	/*!
	  \param[in] cmd json объектом spiffs в корне.
	  \return json строка с ответом (без обрамления в начале и конце {}), либо "".
	*/
	static std::string command(CJsonParser *cmd);
};
