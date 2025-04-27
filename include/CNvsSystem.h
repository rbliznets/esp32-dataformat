/*!
	\file
	\brief Класс для работы с NVS.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 1.0.0.0
	\date 02.05.2024
*/

#pragma once

#include "sdkconfig.h"
#include "CJsonParser.h"

/// Статические методы для работы с nvs.
class CNvsSystem
{
public:
	/// Инициализация
	static bool init();
	/// Закрытие файловой системы.
	static void free();

	/// Обработка команды.
	/*!
	  \param[in] cmd json объектом nvs в корне.
	  \return json строка с ответом (без обрамления в начале и конце {}), либо "".
	*/
	static std::string command(CJsonParser *cmd);
};
