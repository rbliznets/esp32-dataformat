/*!
    \file
    \brief Class for detecting JSON string from byte stream.
    \authors Bliznets R.A. (r.bliznets@gmail.com)
    \version 1.0.0.0
    \date 18.04.2022
*/

#pragma once
#include "sdkconfig.h"

#include <string>
#include <cstring>
#include <list>

/// Class for detecting and extracting JSON strings from byte stream.
/*!
  Class analyzes byte stream, tracks balance of curly braces { and }
  to extract complete JSON objects. Supports processing fragmented data
  and accumulating incomplete objects in temporary buffer.
*/
class CJsonReadStream
{
protected:
  uint8_t *mBuf = nullptr; ///< Buffer for storing incomplete JSON objects between calls
  uint16_t mBufIndex;      ///< Current index (size) of data in buffer
  uint16_t mSize;          ///< Maximum buffer size for incomplete objects
  bool mFree;              ///< Automatic buffer memory deallocation flag

  uint16_t mCount = 0;             ///< Counter of unmatched opening braces { (bracket balance)
  std::list<std::string> mStrings; ///< Queue of found and complete JSON strings

  bool mResync;        ///< Restart on "{number}" found inside incomplete JSON object
  uint8_t mMark = 0;   ///< Size of possible "{number}" collected in mMarkBuf (0 - none)
  char mMarkBuf[7];    ///< Possible "{number}": brace, up to 5 digits, brace

public:
  /// Constructor for class.
  /*!
    \param[in] max_size Maximum buffer size for storing incomplete JSON objects
    \param[in] auto_free Automatic buffer memory deallocation flag (default true)
    \param[in] resync Restart on "{number}" (up to 5 digits) found inside incomplete JSON object.
      For streams where every JSON object follows its checksum sent as "{number}": such a string
      is never a part of JSON object, so the sender has started a new frame and the incomplete
      object is dropped. The number itself is returned as a complete string.
  */
  CJsonReadStream(uint16_t max_size, bool auto_free = true, bool resync = false);

  /// Destructor for class.
  /*!
    Frees buffer memory and clears list of found JSON strings.
  */
  ~CJsonReadStream();

  /// Clear buffer and free memory.
  /*!
    Forcefully frees temporary buffer memory and resets class state.
  */
  void free();

  /// Add data for processing and JSON object search.
  /*!
    Analyzes byte stream, tracks balance of curly braces and extracts complete JSON objects.
    Incomplete objects are saved to temporary buffer for subsequent processing.

    \param[in] data Pointer to byte array for analysis
    \param[in] size Data size in bytes
    \return true if there are incomplete JSON objects (awaiting more data), false if all objects are completed
  */
  bool add(uint8_t *data, uint16_t size);

  /// Get next found JSON string.
  /*!
    Extracts first found JSON string from queue and removes it from list.

    \param[out] str Reference to string where found JSON string will be written
    \return true if string successfully obtained, false if queue is empty
  */
  bool get(std::string &str);
};