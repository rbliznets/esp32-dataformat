/*!
	\file
	\brief Класс для синхронизации системного времени.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 1.1.0.0
	\date 13.09.2024
*/

#include "CDateTimeSystem.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"

bool CDateTimeSystem::mSync = false; ///< Флаг синхронизации времени (true - время синхронизировано)

/**
 * @brief Инициализация системного времени при запуске
 * 
 * Функция загружает сохраненное время из NVS памяти и устанавливает системное время.
 * Если загрузка не удалась, устанавливается время по умолчанию (1726208190).
 */
void CDateTimeSystem::init()
{
	// Если время уже синхронизировано, повторная инициализация не требуется
	if (mSync)
		return;
		
	nvs_handle_t nvs_handle;
	time_t now = 1726208190; // Время по умолчанию (резервное значение)
	
	// Открываем NVS для чтения сохраненного времени
	esp_err_t err = nvs_open("nvs", NVS_READONLY, &nvs_handle);
	if (err == ESP_OK)
	{
		// Читаем сохраненное время из NVS
		nvs_get_i64(nvs_handle, "timestamp", &now);
		nvs_close(nvs_handle);
	}
	else
	{
		ESP_LOGE("DateTimeSystem", "Failed to open NVS %d", err);
	}
	
	// Устанавливаем системное время
	timeval t = {.tv_sec = now, .tv_usec = 0};
	settimeofday(&t, nullptr);
}

/**
 * @brief Установка системного времени
 * @param now Время в формате UNIX timestamp
 * @param force Флаг принудительной установки (игнорирует флаг синхронизации)
 * @param approximate Флаг приблизительной синхронизации (проверяет монотонность времени)
 * @return true если установка успешна, false если операция заблокирована
 * 
 * Функция устанавливает системное время с различными режимами работы:
 * - Обычный режим: устанавливает время и сохраняет его
 * - Принудительный режим: игнорирует флаг синхронизации
 * - Приблизительный режим: проверяет, что новое время не меньше текущего
 */
bool CDateTimeSystem::setDateTime(time_t now, bool force, bool approximate)
{
	// Если не принудительная установка и время уже синхронизировано - блокируем
	if (!force && mSync)
		return false;
		
	timeval t = {.tv_sec = now, .tv_usec = 0};
	
	if (approximate)
	{
		// Приблизительная синхронизация - проверяем монотонность времени
		timeval tv_start;
		gettimeofday(&tv_start, nullptr);
		if (tv_start.tv_sec <= now)
		{
			settimeofday(&t, nullptr);
			// Если принудительная установка - сохраняем время
			if (force)
			{
				saveDateTime();
			}
		}
	}
	else
	{
		// Точная синхронизация - устанавливаем время и сохраняем
		settimeofday(&t, nullptr);
		saveDateTime();
		mSync = true; // Помечаем время как синхронизированное
	}
	return true;
}

/**
 * @brief Сохранение текущего системного времени в NVS
 * @return true если сохранение успешно, false в случае ошибки
 * 
 * Функция сохраняет текущее системное время во флеш-память NVS
 * для восстановления при перезагрузке устройства.
 */
bool CDateTimeSystem::saveDateTime()
{
	nvs_handle_t nvs_handle;
	
	// Открываем NVS для записи
	if (nvs_open("nvs", NVS_READWRITE, &nvs_handle) == ESP_OK)
	{
		timeval tv_start;
		gettimeofday(&tv_start, nullptr);
		
		// Сохраняем текущее время в NVS
		if (nvs_set_i64(nvs_handle, "timestamp", tv_start.tv_sec) == ESP_OK)
		{
			nvs_commit(nvs_handle); // Подтверждаем запись
			nvs_close(nvs_handle);
			return true;
		}
		nvs_close(nvs_handle);
	}
	return false;
}

/**
 * @brief Обработка команд синхронизации времени
 * @param cmd JSON-команда с параметрами синхронизации
 * @param answer JSON-ответ с результатами операции
 * 
 * Поддерживаемые команды:
 * - "epoch": установка времени по UNIX timestamp
 * - "force": принудительное сохранение текущего времени
 * - "approximate": приблизительная синхронизация
 */
void CDateTimeSystem::command(json &cmd, json &answer)
{
	if (cmd.contains("sync"))
	{
		bool force = false;
		// Проверяем флаг принудительной синхронизации
		if (cmd["sync"].contains("force") && cmd["sync"]["force"].is_boolean())
		{
			force = cmd["sync"]["force"].template get<bool>();
		}
		
		bool approximate = false;
		// Проверяем флаг приблизительной синхронизации
		if (cmd["sync"].contains("approximate") && cmd["sync"]["approximate"].is_boolean())
		{
			approximate = cmd["sync"]["approximate"].template get<bool>();
		}
		
		time_t epoch;
		// Установка времени по timestamp
		if (cmd["sync"].contains("epoch") && cmd["sync"]["epoch"].is_number_unsigned())
		{
			epoch = cmd["sync"]["epoch"].template get<time_t>();
			force = setDateTime(epoch, force, approximate);
		}
		// Принудительное сохранение текущего времени
		else if (cmd["sync"].contains("force"))
		{
			if (force)
			{
				force = saveDateTime();
			}
		}
		else
		{
			answer["sync"]["error"] = "wrong param";
			return;
		}

		// Формируем ответ
		answer["sync"]["result"] = force;
		timeval tv_start;
		gettimeofday(&tv_start, nullptr);
		answer["sync"]["epoch"] = tv_start.tv_sec;
		if (!mSync)
			answer["sync"]["sync"] = false;
	}
}

/**
 * @brief Логирование текущего системного времени
 * 
 * Функция выводит в лог текущее системное время в читаемом формате
 * и состояние синхронизации времени.
 */
void CDateTimeSystem::log()
{
	timeval tv_start;
	gettimeofday(&tv_start, nullptr);
	time_t nowtime = tv_start.tv_sec;
	tm *t = localtime(&nowtime);
	char tmbuf[64];
	
	// Форматируем время в строку YYYY-MM-DD HH:MM:SS
	strftime(tmbuf, sizeof(tmbuf), "%Y-%m-%d %H:%M:%S", t);
	
	// Выводим в лог информацию о времени и состоянии синхронизации
	ESP_LOGI("DateTimeSystem", "Time: %s (Sync: %s)", tmbuf, mSync ? "true" : "false");
}