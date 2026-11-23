#pragma once

template <typename... Args>
inline void host_esp_log(const char *, const char *, Args...)
{
}

#define ESP_LOGW(...) host_esp_log(__VA_ARGS__)
