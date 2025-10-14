/*!
	\file
	\brief CRC-8 algorithm.
	\authors Bliznets R.A. (r.bliznets@gmail.com)
	\version 1.0.0.0
	\date 10.03.2017
*/

#pragma once

#include <stdint.h>

/// CRC class
/*!
 Calculation and verification of CRC-8 through static methods.
 Uses polynomial X^8 + X^2 + X^1 + 1 (0x07).
 */
class CCRC8
{
protected:
	static const uint8_t CRCTable[256]; ///< Precomputed CRC-8 values table for faster calculations

public:
	/// Check CRC-8 of data.
	/*!
	  Verifies data integrity by calculating CRC-8 and comparing with expected result.
	  Uses polynomial X^8 + X^2 + X^1 + 1 (0x07).
	  Initial CRC value: 0xFF.

	  \param[in] data Pointer to data array for verification.
	  \param[in] size Data size in bytes.
	  \return true if CRC calculated correctly (checksum is valid), false if data is corrupted.
	*/
	static bool Check(uint8_t *data, uint16_t size);

	/// Calculate CRC-8 for data.
	/*!
	  Calculates CRC-8 checksum for data array.
	  Uses polynomial X^8 + X^2 + X^1 + 1 (0x07).
	  Initial CRC value: 0xFF.

	  \param[in] data Pointer to data array for CRC calculation.
	  \param[in] size Data size in bytes.
	  \param[out] crc Pointer to variable where CRC calculation result will be written.
	*/
	static void Create(uint8_t *data, uint16_t size, uint8_t *crc);
};