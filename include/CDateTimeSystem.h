/*!
	\file
	\brief Class for synchronizing system time.
	\authors Bliznets R.A. (r.bliznets@gmail.com)
	\version 1.2.1.0
	\date 13.09.2024
*/

#pragma once

#include "sdkconfig.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include <sys/time.h>

/// Static class for synchronizing and managing system time.
/*!
  Provides functions for setting, saving and synchronizing system time.
  Supports NVS memory operations for saving time between reboots.
*/
class CDateTimeSystem
{
protected:
	static bool mSync; ///< Time synchronization status flag (true - time is synchronized)
	static bool mApproximate; 
	static bool mUpdateFlashApproximate; 
	static bool mUpdateFlashSync; 

public:
	/// Initialize system time on startup.
	/*!
	  Loads saved time from NVS memory and sets system time.
	  If data is missing, uses default time.
	*/
	static void init();

	/// @brief Set system time
	/// @param now Time in UNIX timestamp format (seconds since January 1, 1970)
	/// @param force Set even if time is already synchronized (default false)
	/// @param approximate Flag for imprecise time source, checks monotonicity (default false)
	/// @return true if time set successfully, otherwise false
	static bool setDateTime(time_t now, bool force = false, bool approximate = false);

	/// @brief Write current system time to NVS memory
	/// @return true if time successfully written, otherwise false
	static bool saveDateTime(bool forceSave = true);

	/// Process JSON time synchronization commands.
	/*!
	  Processes time synchronization commands from JSON object.
	  Supported commands:
	  - "epoch": set time by UNIX timestamp
	  - "force": force save current time
	  - "approximate": approximate synchronization

	  \param[in] cmd JSON object with time synchronization commands.
	  \param[out] answer JSON object with command execution results.
	*/
	static void command(json &cmd, json &answer);

	/// @brief Get time synchronization flag
	/// @return true if time is synchronized, false if not
	static inline bool isSync() { return mSync; }
	static inline bool isApproximate() { return (mSync || mApproximate); }

	/// @brief Output current date and time to log
	/*!
	  Formats and outputs current system time in YYYY-MM-DD HH:MM:SS format
	  along with time synchronization status.
	*/
	static void log();
};