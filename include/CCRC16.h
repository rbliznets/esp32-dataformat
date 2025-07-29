/*!
	\file
	\brief Алгоритм CRC-16.
	\authors Близнец Р.А.
	\version 1.0.0.0
	\date 10.03.2017
*/

#if !defined CCRC16_H
#define CCRC16_H

#include <stdint.h>

/// Класс CRC16
/*!
 Вычисление и проверка CRC через статические методы.
 */
class CCRC16
{
protected:
	static const uint16_t CRCTable[256]; ///< Таблица для CRC-16 (X16+X15+X2+1).
public:
	/// Проверить CRC-16.
	/*!
	  \param[in] data данные.
	  \param[in] size размер данных.
	  \param[in] ex алгоритм CRC волновой сети.
	  \return true если CRC верно.
	*/
	static bool Check(uint8_t *data, uint16_t size);
	/// Посчитать CRC-16.
	/*!
	  \param[in] data данные.
	  \param[in] size размер данных.
	  \param[out] crc CRC.
	*/
	static void Create(uint8_t *data, uint16_t size, uint16_t *crc);

	/// Начальная иницилизация.
	/*!
	  \return Начальное значение.
	*/
	inline static uint16_t Init() { return 0xffff; };
	/// Расчет CRC-16.
	/*!
	  \param[in] data данные.
	  \param[in] size размер данных.
	  \param[in,out] crc CRC.
	*/
	static void Add(uint8_t *data, uint16_t size, uint16_t *crc);
};
#endif // CCRC16_H
