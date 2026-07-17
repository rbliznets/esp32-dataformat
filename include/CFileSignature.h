/*!
    \file
    \brief Проверка ECDSA P-256 / SHA-256 подписи файлов на устройстве.
    \authors Bliznets R.A. (r.bliznets@gmail.com)
    \version 1.0.0.0
    \date 17.07.2026
*/
#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <string>

/// @brief Проверка файлов, подписанных инструментом tools/sign_file.py (ECDSA P-256 + SHA-256).
/*!
  Формат подписанного файла (совпадает с tools/sign_file.py):
    offset 0   (4 байта)            magic "SFV1"
    offset 4   (4 байта)            payload_len, uint32 little-endian
    offset 8   (32 байта)           подпись: r (big-endian)
    offset 40  (32 байта)           подпись: s (big-endian)
    offset 72  (payload_len байт)   полезная нагрузка (исходный файл как есть)

  Публичный ключ передаётся вызывающей стороной в несжатом формате SEC1
  (0x04 || X(32) || Y(32), итого 65 байт) — обычно как константа, сгенерированная
  `tools/sign_file.py genkey` (файл public_key.h), зашитая в прошивку.
*/
class CFileSignature
{
public:
    static constexpr size_t PUBKEY_SIZE = 65;                     ///< 0x04 || X(32) || Y(32)
    static constexpr size_t SIGNATURE_SIZE = 64;                  ///< r(32) || s(32)
    static constexpr size_t HEADER_SIZE = 4 + 4 + SIGNATURE_SIZE; ///< magic + payload_len + signature

    /// @brief Проверить подписанный буфер (заголовок + полезная нагрузка), уже загруженный в память.
    /// @param pubKey65 публичный ключ, 65 байт (0x04||X||Y)
    /// @param signedData буфер целиком (magic+payload_len+signature+payload)
    /// @param signedSize размер буфера в байтах
    /// @param[out] payload указатель на начало полезной нагрузки внутри signedData (без копирования); nullptr при ошибке
    /// @param[out] payloadSize размер полезной нагрузки в байтах; 0 при ошибке
    /// @return true, если подпись корректна
    static bool verifyBuffer(const uint8_t *pubKey65, const uint8_t *signedData, size_t signedSize,
                              const uint8_t **payload, size_t *payloadSize);

    /// @brief Прочитать подписанный файл (LittleFS/SPIFFS, обычный путь VFS) и проверить подпись.
    /// @param pubKey65 публичный ключ, 65 байт (0x04||X||Y)
    /// @param path путь к файлу (например "/spiffs/c/config.signed")
    /// @param[out] payload проверенная полезная нагрузка (заполняется только при успехе)
    /// @return true, если файл прочитан и подпись корректна
    static bool verifyFile(const uint8_t *pubKey65, const char *path, std::vector<uint8_t> &payload);

    /// @brief Прочитать подписанный файл и проверить подпись, вернув полезную нагрузку как текст.
    /// @param pubKey65 публичный ключ, 65 байт (0x04||X||Y)
    /// @param path путь к файлу (например "/spiffs/lic.signed")
    /// @param[out] payload проверенная полезная нагрузка (заполняется только при успехе)
    /// @return true, если файл прочитан и подпись корректна
    static bool verifyFile(const uint8_t *pubKey65, const char *path, std::string &payload);

private:
    /// @brief Проверить сырую ECDSA-подпись (r||s, 64 байта) над данными по SHA-256.
    static bool verifyRaw(const uint8_t *pubKey65, const uint8_t *data, size_t dataLen, const uint8_t *signature64);
};
