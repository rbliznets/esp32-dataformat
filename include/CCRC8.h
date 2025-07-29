/*!
	\file
	\brief Алгоритм CRC-8.
	\authors Близнец Р.А.
	\version 1.0.0.0
	\date 10.03.2017
*/

#if !defined CCRC8_H
#define CCRC8_H

#include <stdint.h>

/// Класс CRC
/*!
 Вычисление и проверка CRC через статические методы.
 */
class CCRC8
{
protected:
	static const uint8_t CRCTable[256]; ///< Таблица для CRC-8 (X8+X2+X1+1).
public:
	/// Проверить CRC-8.
	/*!
	  Полином X8+X2+X1+1
	  \param[in] data данные.
	  \param[in] size размер данных.
	  \return true если CRC верно.
	*/
	static bool Check(uint8_t *data, uint16_t size);
	/// Посчитать CRC.
	/*!
	  Полином X8+X2+X1+1
	  \param[in] data данные.
	  \param[in] size размер данных.
	  \param[out] crc CRC.
	*/
	static void Create(uint8_t *data, uint16_t size, uint8_t *crc);
};
#endif // CCRC_H
