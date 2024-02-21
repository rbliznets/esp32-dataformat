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
#include <list>

#define BUF_PART_SIZE (200)

/// Статические методы для работы с файловой системой.
class CBufferSystem
{
protected:
	uint8_t *mBuffer = nullptr;
	uint32_t mSize;
	uint16_t mPart = BUF_PART_SIZE;
	std::list<uint16_t>  mParts;
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
};
