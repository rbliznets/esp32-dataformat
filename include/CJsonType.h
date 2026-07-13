/*!
    \file
    \brief Единый тип json для проекта; на платах с PSRAM узлы JSON (объекты/массивы)
           выделяются в PSRAM через кастомный allocator, а не во внутренней RAM.
    \authors Близнец Р.А. (r.bliznets@gmail.com)
    \version 1.0.0.0
    \date 13.07.2026
*/
#pragma once

#include "sdkconfig.h"
#include <nlohmann/json.hpp>

#if CONFIG_DATAFORMAT_JSON_INPSRAM
#include "esp_heap_caps.h"
#include <new>

/// @brief Allocator, выделяющий память под узлы nlohmann::json в PSRAM (MALLOC_CAP_SPIRAM).
template <class T>
struct SpiramAllocator
{
    using value_type = T;

    SpiramAllocator() noexcept = default;
    template <class U>
    SpiramAllocator(const SpiramAllocator<U> &) noexcept {}

    T *allocate(std::size_t n)
    {
        if (auto p = static_cast<T *>(heap_caps_malloc(n * sizeof(T), MALLOC_CAP_SPIRAM)))
            return p;
        throw std::bad_alloc();
    }
    void deallocate(T *p, std::size_t) noexcept { heap_caps_free(p); }
};
template <class T, class U>
bool operator==(const SpiramAllocator<T> &, const SpiramAllocator<U> &) { return true; }
template <class T, class U>
bool operator!=(const SpiramAllocator<T> &, const SpiramAllocator<U> &) { return false; }

// string_t в этом алиасе остаётся обычным std::string: длинные строковые значения
// (свыше SSO, ~15 байт) по-прежнему получают буфер во внутренней RAM через malloc().
using json = nlohmann::basic_json<
    std::map, std::vector, std::string, bool,
    std::int64_t, std::uint64_t, double,
    SpiramAllocator>;
#else
using json = nlohmann::json;
#endif
