/*!
	\file
	\brief Класс для работы с буфером PSRAM.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 0.0.0.1
	\date 21.02.2024
*/

#pragma once

#include "sdkconfig.h"
#include "CJsonParser.h"

#define BUF_PART_SIZE (200)

class CBufferSystem
{
protected:
	uint8_t *mBuffer = nullptr;
	uint32_t mSize;
	uint16_t mPart = BUF_PART_SIZE;
	uint8_t*  mParts = nullptr;
	uint16_t mLastPart;

	bool init(uint32_t size);
	void free();

public:
	~CBufferSystem()
	{
		free();
	};

	/// Обработка команды.
	/*!
	  \param[in] cmd json объектом spiffs в корне.
	  \return json строка с ответом (без обрамления в начале и конце {}), либо "".
	*/
	std::string command(CJsonParser *cmd);
	void addData(uint8_t* data, uint32_t size);
	uint8_t* getData(uint32_t& size, uint16_t& index);
};
