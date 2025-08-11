/*!
	\file
	\brief Класс для работы с обновлением firmware.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 0.1.0.0
	\date 27.04.2025
*/

#pragma once

#include "sdkconfig.h"
#include "CJsonParser.h"

#include "freertos/FreeRTOS.h"
#include "esp_ota_ops.h"
#include <list>

#include <nlohmann/json.hpp>
using json = nlohmann::json;

typedef void onOTAWork(bool lock);

/// Статические методы для работы с обновлением firmware.
class COTASystem
{
protected:
	static std::list<onOTAWork*> mWriteQueue;
	static void writeEvent(bool lock);

	static esp_ota_handle_t update_handle;
    static int offset;
public:

    static bool init();
    static void confirmFirmware(bool ok = true);

    static void abort();

	/// Обработка команды.
	/*!
	  \param[in] cmd json объектом spiffs в корне.
	  \return json строка с ответом (без обрамления в начале и конце {}), либо "".
	*/
	static std::string command(CJsonParser *cmd);

	/// Обработка команды.
	/*!
     * @brief Обработка команд JSON для SPIFFS
     * @param cmd JSON-объект с командой в корне файловой системы
	 * @param[out] answer json с ответом.
	*/
	static void command(json& cmd, json& answer);

    static std::string update(uint8_t *data, uint32_t size);

	static void addWriteEvent(onOTAWork* event);
	static void removeWriteEvent(onOTAWork* event);
};
