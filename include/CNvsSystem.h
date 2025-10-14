/*!
	\file
	\brief Class for working with NVS.
	\authors Bliznets R.A. (r.bliznets@gmail.com)
	\version 1.1.0.0
	\date 02.05.2024
*/

#pragma once

#include "sdkconfig.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

/// Static class for working with Non-Volatile Storage (NVS) memory.
/*!
  Provides static methods for initialization, working with variables
  and performing service operations with ESP32 NVS memory.
  Supports working with various data types: integers, floating-point numbers, strings.
*/
class CNvsSystem
{
public:
	/// Initialize NVS memory.
	/*!
	  Performs initialization of main NVS partition and additional "nvs2" partition.
	  Automatically performs memory cleanup if errors are detected.
	  \return true if initialization is successful, false in case of error.
	*/
	static bool init();

	/// Deinitialize NVS memory.
	/*!
	  Releases NVS memory resources and closes file system.
	  Called before finishing work with NVS.
	*/
	static void free();

	/// Process JSON commands for NVS operations.
	/*!
	  Processes NVS memory commands from JSON object.
	  Supported commands:
	  - "clear": clear entire NVS memory
	  - "reset": restart device
	  - Work with variables of various types (u8, i8, i16, i32, u32, float, double)

	  \param[in] cmd JSON object with NVS commands (key "nvs" at root).
	  \param[out] answer JSON object with command execution results.
	*/
	static void command(json &cmd, json &answer);
};