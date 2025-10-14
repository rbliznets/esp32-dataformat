/*!
	\file
	\brief Class for working with PSRAM buffer.
	\authors Bliznets R.A. (r.bliznets@gmail.com)
	\version 1.2.0.0
	\date 21.02.2024
*/

#pragma once

#include "sdkconfig.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#define BUF_PART_SIZE (200) ///< Default buffer part size in bytes

/// @brief Class for working with PSRAM buffer with partial filling capability.
class CBufferSystem
{
protected:
	uint8_t *mBuffer = nullptr;		///< Pointer to data buffer in memory
	uint32_t mSize;					///< Total buffer size in bytes
	uint16_t mPart = BUF_PART_SIZE; ///< Size of each buffer part in bytes
	uint8_t *mParts = nullptr;		///< Array of part status flags (0 - empty, 1 - filled)
	uint16_t mLastPart;				///< Index of last part in array (0-based)
	bool mRead = false;				///< Flag indicating buffer loaded from file and ready for reading

	/**
		@fn CBufferSystem::init(uint32_t size)
		@brief Initializes buffer of specified size in PSRAM or regular memory.
		@param size Buffer size in bytes for memory allocation.
		@return Returns true if initialization was successful, false if memory allocation failed.
	*/
	bool init(uint32_t size);

	/**
	@fn CBufferSystem::free()
	@brief Frees allocated buffer memory and resets all class properties.
	*/
	void free();

public:
	/// @brief Destructor of CBufferSystem class. Automatically frees buffer memory.
	~CBufferSystem()
	{
		free();
	};

	/// Process JSON commands for buffer management.
	/*!
	  \param[in] cmd JSON object with commands at root (contains key "buf").
	  \param[out] answer JSON object with command response.
	  \param[out] cancel Operation cancellation flag (set to true for "cancel" command).

	  Supported commands:
	  - "create": create buffer of specified size
	  - "check": check buffer status and list of unfilled parts
	  - "wr": write buffer to file
	  - "ota": perform OTA update from buffer
	  - "rd": read buffer from file
	  - "free": free buffer
	  - "cancel": cancel operation with buffer free
	*/
	void command(json &cmd, json &answer, bool &cancel);

	/// Add data to buffer by parts.
	/*!
	  \param[in] data Pointer to data (first 2 bytes contain part number, rest are data).
	  \param[in] size Data size in bytes (including 2 bytes of part number).

	  Data is added to buffer according to the part number specified in the first two bytes.
	  Each part is marked as filled in the mParts array.
	*/
	void addData(uint8_t *data, uint32_t size);

	/// Get data from buffer by parts for sequential reading.
	/*!
	  \param[out] size Size of received data in bytes.
	  \param[out] index Number of part from which data was received.
	  \return Pointer to data or nullptr if all parts have already been read or buffer not loaded.

	  Function returns pointer to next unread buffer part and marks it as read.
	  After reading, all parts will be returned sequentially, after which function returns nullptr.
	*/
	uint8_t *getData(uint32_t &size, uint16_t &index);
};