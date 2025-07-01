/*!
	\file
	\brief Класс для синхронизации системного времени.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 1.1.0.0
	\date 13.09.2024
*/

#pragma once

#include "sdkconfig.h"
#include "CJsonParser.h"
#include <sys/time.h>

/// Статические для синхронизации системного времени.
class CDateTimeSystem
{
protected:
	static bool mSync; ///< Флаг синхронизации
public:
	/// Инициализация
	static void init();

	/// @brief Установка системного времени
	/// @param now секунды с 1 января 1970 года
	/// @param force устанавливать даже если время уже синхронизовано
	/// @param approximate флаг неточного источника времени
	/// @return true если установлено, иначе false
	static bool setDateTime(time_t now, bool force = false, bool approximate = false);

	/// @brief Записать текущее время в NVS
	/// @return true если записано, иначе false
	static bool saveDateTime();

	/// Обработка команды.
	/*!
	  \param[in] cmd json объектом nvs в корне.
	  \return json строка с ответом (без обрамления в начале и конце {}), либо "".
	*/
	static std::string command(CJsonParser *cmd);

	/// @brief Флаг синхронизации
	/// @return Флаг синхронизации
	static inline bool isSync() { return mSync; }

	/// @brief вывод текущей даты
	static void log();
};
