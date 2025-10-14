/*!
	\file
	\brief CRC-16 algorithm.
	\authors Bliznets R.A. (r.bliznets@gmail.com)
	\version 1.0.1.0
	\date 10.03.2017
*/

#pragma once

#include <stdint.h>

/// CRC16 class
/*!
 Calculation and verification of CRC-16 through static methods.
 Uses polynomial X^16 + X^15 + X^2 + 1 (0x8005) in reflected form.
 */
class CCRC16
{
protected:
	static const uint16_t CRCTable[256]; ///< Precomputed CRC-16 values table for faster calculations

public:
	/// Check CRC-16 of data.
	/*!
	  Verifies data integrity by calculating CRC-16 and comparing with expected result.
	  Uses polynomial X^16 + X^15 + X^2 + 1 (0x8005) in reflected form.
	  Initial CRC value: 0xFFFF.

	  \param[in] data Pointer to data array for CRC verification.
	  \param[in] size Data size in bytes.
	  \return true if CRC calculated correctly and data is intact, false if data is corrupted.
	*/
	static bool Check(uint8_t *data, uint16_t size);

	/// Calculate CRC-16 for data.
	/*!
	  Calculates CRC-16 checksum for data array.
	  Uses polynomial X^16 + X^15 + X^2 + 1 (0x8005) in reflected form.
	  Initial CRC value: 0xFFFF.

	  \param[in] data Pointer to data array for CRC calculation.
	  \param[in] size Data size in bytes.
	  \param[out] crc Pointer to variable where CRC-16 calculation result will be written.
	*/
	static void Create(uint8_t *data, uint16_t size, uint16_t *crc);

	/// Get initial CRC-16 value.
	/*!
	  Returns initial value for CRC-16 calculation initialization.
	  \return Initial CRC-16 value (0xFFFF).
	*/
	inline static uint16_t Init() { return 0xffff; };

	/// Add data to existing CRC-16.
	/*!
	  Allows incremental CRC-16 calculation for streaming data or large volumes.
	  Continues CRC-16 calculation from current value.

	  \param[in] data Pointer to array of additional data.
	  \param[in] size Size of additional data in bytes.
	  \param[in,out] crc Pointer to variable with current CRC-16 value, which will be updated.
	*/
	static void Add(uint8_t *data, uint16_t size, uint16_t *crc);
};