/*!
    \file
    \brief Проверка ECDSA P-256 / SHA-256 подписи файлов на устройстве.
    \authors Bliznets R.A. (r.bliznets@gmail.com)
    \version 1.0.0.0
    \date 17.07.2026
*/
#include "CFileSignature.h"

#include <cstdio>
#include <cstring>

#include "psa/crypto.h"
#include "esp_log.h"

static const char *TAG = "filesig";

// Проверка сырой ECDSA P-256 подписи (r||s) над SHA-256 от data через PSA Crypto API.
// Схема повторяет верификацию secure boot v2 в этой версии ESP-IDF
// (components/bootloader_support/src/secure_boot_v2/secure_boot_ecdsa_signature.c).
bool CFileSignature::verifyRaw(const uint8_t *pubKey65, const uint8_t *data, size_t dataLen, const uint8_t *signature64)
{
    psa_key_attributes_t key_attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_handle = 0;

    psa_set_key_usage_flags(&key_attributes, PSA_KEY_USAGE_VERIFY_HASH);
    psa_set_key_algorithm(&key_attributes, PSA_ALG_ECDSA(PSA_ALG_SHA_256));
    psa_set_key_type(&key_attributes, PSA_KEY_TYPE_ECC_PUBLIC_KEY(PSA_ECC_FAMILY_SECP_R1));
    psa_set_key_bits(&key_attributes, 256);

    psa_status_t status = psa_import_key(&key_attributes, pubKey65, PUBKEY_SIZE, &key_handle);
    if (status != PSA_SUCCESS)
    {
        ESP_LOGE(TAG, "psa_import_key failed: %d", (int)status);
        psa_reset_key_attributes(&key_attributes);
        return false;
    }

    bool ok = false;
    uint8_t hash[32];
    size_t hashLen = 0;
    status = psa_hash_compute(PSA_ALG_SHA_256, data, dataLen, hash, sizeof(hash), &hashLen);
    if (status == PSA_SUCCESS)
    {
        status = psa_verify_hash(key_handle, PSA_ALG_ECDSA(PSA_ALG_SHA_256), hash, hashLen, signature64, SIGNATURE_SIZE);
        ok = (status == PSA_SUCCESS);
        if (!ok)
        {
            ESP_LOGW(TAG, "signature verification failed: %d", (int)status);
        }
    }
    else
    {
        ESP_LOGE(TAG, "psa_hash_compute failed: %d", (int)status);
    }

    psa_destroy_key(key_handle);
    psa_reset_key_attributes(&key_attributes);
    return ok;
}

bool CFileSignature::verifyBuffer(const uint8_t *pubKey65, const uint8_t *signedData, size_t signedSize,
                                   const uint8_t **payload, size_t *payloadSize)
{
    if (payload != nullptr)
        *payload = nullptr;
    if (payloadSize != nullptr)
        *payloadSize = 0;

    if ((pubKey65 == nullptr) || (signedData == nullptr) || (signedSize < HEADER_SIZE))
        return false;

    if (std::memcmp(signedData, "SFV1", 4) != 0)
    {
        ESP_LOGE(TAG, "bad magic");
        return false;
    }

    uint32_t len;
    std::memcpy(&len, signedData + 4, sizeof(len)); // little-endian - совпадает с порядком байт Xtensa/ESP32-S3

    // Вычитание вместо сложения (HEADER_SIZE + len) - иначе len, пришедший из файла,
    // может переполнить size_t и ложно пройти проверку.
    if ((size_t)len != signedSize - HEADER_SIZE)
    {
        ESP_LOGE(TAG, "length mismatch: header=%u, actual payload=%u", (unsigned)len, (unsigned)(signedSize - HEADER_SIZE));
        return false;
    }

    const uint8_t *signature = signedData + 8;
    const uint8_t *data = signedData + HEADER_SIZE;

    if (!verifyRaw(pubKey65, data, len, signature))
        return false;

    if (payload != nullptr)
        *payload = data;
    if (payloadSize != nullptr)
        *payloadSize = len;
    return true;
}

bool CFileSignature::verifyFile(const uint8_t *pubKey65, const char *path, std::vector<uint8_t> &payload)
{
    FILE *f = std::fopen(path, "rb");
    if (f == nullptr)
    {
        ESP_LOGE(TAG, "failed to open %s", path);
        return false;
    }

    std::fseek(f, 0, SEEK_END);
    long size = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (size < (long)HEADER_SIZE)
    {
        std::fclose(f);
        ESP_LOGE(TAG, "%s too small for signature header", path);
        return false;
    }

    std::vector<uint8_t> buf((size_t)size);
    size_t read = std::fread(buf.data(), 1, buf.size(), f);
    std::fclose(f);
    if (read != buf.size())
    {
        ESP_LOGE(TAG, "failed to read %s", path);
        return false;
    }

    const uint8_t *data;
    size_t dataSize;
    if (!verifyBuffer(pubKey65, buf.data(), buf.size(), &data, &dataSize))
        return false;

    payload.assign(data, data + dataSize);
    return true;
}
