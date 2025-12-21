/*!
	\file
	\brief Class for synchronizing system time.
	\authors Bliznets R.A. (r.bliznets@gmail.com)
	\version 1.2.0.0
	\date 13.09.2024
*/

#include "CDateTimeSystem.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "CNvsSystem.h"
#include "esp_system.h"
#include <ctime>

bool CDateTimeSystem::mSync = false; ///< Time synchronization flag (true - time is synchronized)

/**
 * @brief Initialize system time on startup
 *
 * Function loads saved time from NVS memory and sets system time.
 * If loading fails, sets default time (1726208190).
 */
void CDateTimeSystem::init()
{
	// If time is already synchronized, re-initialization is not required
	if (mSync)
		return;

	std::time_t now = 1766188805; // Default time (backup value)

	CNvsSystem::restore("timestamp", now);

	// Set system time
	timeval t = {.tv_sec = now, .tv_usec = 0};
	settimeofday(&t, nullptr);

	ESP_LOGI("start data", "%s", std::ctime(&now));
}

/**
 * @brief Set system time
 * @param now Time in UNIX timestamp format
 * @param force Force setting flag (ignores synchronization flag)
 * @param approximate Approximate synchronization flag (checks time monotonicity)
 * @return true if setting is successful, false if operation is blocked
 *
 * Function sets system time with various operation modes:
 * - Normal mode: sets time and saves it
 * - Force mode: ignores synchronization flag
 * - Approximate mode: checks that new time is not less than current
 */
bool CDateTimeSystem::setDateTime(time_t now, bool force, bool approximate)
{
	// If not force setting and time already synchronized - block
	if (!force && mSync)
		return false;

	timeval t = {.tv_sec = now, .tv_usec = 0};

	if (approximate)
	{
		// Approximate synchronization - check time monotonicity
		timeval tv_start;
		gettimeofday(&tv_start, nullptr);
		if (tv_start.tv_sec <= now)
		{
			settimeofday(&t, nullptr);
			// If force setting - save time
			if (force)
			{
				saveDateTime();
			}
		}
	}
	else
	{
		// Precise synchronization - set time and save
		settimeofday(&t, nullptr);
		saveDateTime();
		mSync = true; // Mark time as synchronized
	}
	return true;
}

/**
 * @brief Save current system time to NVS
 * @return true if saving is successful, false in case of error
 *
 * Function saves current system time to NVS flash memory
 * for recovery on device reboot.
 */
bool CDateTimeSystem::saveDateTime()
{
	timeval tv_start;
	gettimeofday(&tv_start, nullptr);

	return (CNvsSystem::save("timestamp", tv_start.tv_sec, NVS_BOTH) != NVS_NONE);
}

/**
 * @brief Process time synchronization commands
 * @param cmd JSON command with synchronization parameters
 * @param answer JSON response with operation results
 *
 * Supported commands:
 * - "epoch": set time by UNIX timestamp
 * - "force": force save current time
 * - "approximate": approximate synchronization
 */
void CDateTimeSystem::command(json &cmd, json &answer)
{
	if (cmd.contains("sync"))
	{
		bool force = false;
		// Check force synchronization flag
		if (cmd["sync"].contains("force") && cmd["sync"]["force"].is_boolean())
		{
			force = cmd["sync"]["force"].template get<bool>();
		}

		bool approximate = false;
		// Check approximate synchronization flag
		if (cmd["sync"].contains("approximate") && cmd["sync"]["approximate"].is_boolean())
		{
			approximate = cmd["sync"]["approximate"].template get<bool>();
		}

		time_t epoch;
		// Set time by timestamp
		if (cmd["sync"].contains("epoch") && cmd["sync"]["epoch"].is_number_unsigned())
		{
			epoch = cmd["sync"]["epoch"].template get<time_t>();
			force = setDateTime(epoch, force, approximate);
		}
		// Force save current time
		else if (cmd["sync"].contains("force"))
		{
			if (force)
			{
				force = saveDateTime();
			}
		}
		else
		{
			answer["sync"]["error"] = "wrong param";
			return;
		}

		// Form response
		answer["sync"]["result"] = force;
		timeval tv_start;
		gettimeofday(&tv_start, nullptr);
		answer["sync"]["epoch"] = tv_start.tv_sec;
		if (!mSync)
			answer["sync"]["sync"] = false;
	}
}

/**
 * @brief Log current system time
 *
 * Function outputs current system time in readable format
 * and time synchronization status to log.
 */
void CDateTimeSystem::log()
{
	timeval tv_start;
	gettimeofday(&tv_start, nullptr);
	time_t nowtime = tv_start.tv_sec;
	tm *t = localtime(&nowtime);
	char tmbuf[64];

	// Format time to string YYYY-MM-DD HH:MM:SS
	strftime(tmbuf, sizeof(tmbuf), "%Y-%m-%d %H:%M:%S", t);

	// Output time information and synchronization status to log
	ESP_LOGI("DateTimeSystem", "Time: %s (Sync: %s)", tmbuf, mSync ? "true" : "false");
}