/*!
	\file
	\brief Класс для работы с обновлением firmware.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 0.0.0.1
	\date 27.04.2025
*/

#pragma once

#include "sdkconfig.h"
#include "CJsonParser.h"

#include "freertos/FreeRTOS.h"
#include "esp_ota_ops.h"

/// Статические методы для работы с обновлением firmware.
class COTASystem
{
protected:
    static esp_ota_handle_t update_handle;
    static int offset;
public:

    static void abort();

	/// Обработка команды.
	/*!
	  \param[in] cmd json объектом spiffs в корне.
	  \return json строка с ответом (без обрамления в начале и конце {}), либо "".
	*/
	static std::string command(CJsonParser *cmd);
};
