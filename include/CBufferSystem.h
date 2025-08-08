/*!
	\file
	\brief Класс для работы с буфером PSRAM.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 1.2.0.0
	\date 21.02.2024
*/

#pragma once

#include "sdkconfig.h"
#include "CJsonParser.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

#define BUF_PART_SIZE (200) ///< Default size of each part in bytes. (default 200)

/// @brief Класс для работы с буфером PSRAM.
class CBufferSystem
{
protected:
	uint8_t *mBuffer = nullptr;		///< Pointer to the buffer.
	uint32_t mSize;					///< Size of the buffer in bytes.
	uint16_t mPart = BUF_PART_SIZE; ///< Size of each part in bytes. (default BUF_PART_SIZE)
	uint8_t *mParts = nullptr;		///< Pointer to an array of part sizes.
	uint16_t mLastPart;				///< Index of the last part in the array.
	bool mRead = false;				///< Flag indicating if the buffer is being read.

	/**
		@fn CBufferSystem::init(uint32_t size)
		@brief Initializes the buffer with a specified size.
		@param size The size of the buffer to initialize.
		@return Returns true if initialization was successful, false otherwise.
	*/
	bool init(uint32_t size);

	/**
	@fn CBufferSystem::free()
	@brief Frees the allocated buffer and resets its properties.
	*/
	void free();

public:
	/// @brief Denstructor for CBufferSystem.
	~CBufferSystem()
	{
		free();
	};

	/// Обработка команды.
	/*!
	  \param[in] cmd json объектом spiffs в корне.
	  \return json строка с ответом (без обрамления в начале и конце {}), либо "".
	*/
	std::string command(CJsonParser *cmd, bool &cancel);

	/// Обработка команды.
	/*!
	  \param[in] cmd json объектом nvs в корне.
	  \param[out] answer json с ответом.
	*/
	void command(json& cmd, json& answer);

	/// Добавление данных в буфер.
	/*!
	  \param[in] data указатель на данные.
	  \param[in] size размер данных.
	*/
	void addData(uint8_t *data, uint32_t size);

	/// Получение данных из буфера.
	/*!
	  \param[out] size размер полученных данных.
	  \return указатель на данные или nullptr.
	*/
	uint8_t *getData(uint32_t &size, uint16_t &index);
};
