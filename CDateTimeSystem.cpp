/*!
	\file
	\brief Класс для синхронизации системного времени.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 1.0.0.0
	\date 13.09.2024
*/

#include "CDateTimeSystem.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_system.h"

bool CDateTimeSystem::mSync = false;

void CDateTimeSystem::init()
{
	nvs_handle_t nvs_handle;
	time_t now = 1726208190;
	if (nvs_open("nvs", NVS_READONLY, &nvs_handle) == ESP_OK)
	{
		nvs_get_i64(nvs_handle, "timestamp", &now);
		nvs_close(nvs_handle);
	}
	timeval t = {.tv_sec = now};
	settimeofday(&t, nullptr);
	mSync = false;
}

bool CDateTimeSystem::setDateTime(time_t now, bool force)
{
	if (!force && mSync)
		return false;
	timeval t = {.tv_sec = now};
	settimeofday(&t, nullptr);
	mSync = true;

	saveDateTime();
	return true;
}

bool CDateTimeSystem::saveDateTime()
{
	nvs_handle_t nvs_handle;
	if (nvs_open("nvs", NVS_READWRITE, &nvs_handle) == ESP_OK)
	{
		timeval tv_start;
		gettimeofday(&tv_start, nullptr);
		if (nvs_set_i64(nvs_handle, "timestamp", tv_start.tv_sec) == ESP_OK)
		{
			nvs_commit(nvs_handle);
			return true;
		}
		nvs_close(nvs_handle);
	}
	return false;
}

std::string CDateTimeSystem::command(CJsonParser *cmd)
{
	std::string answer = "";
	int t;
	if (cmd->getObject(1, "sync", t))
	{
		int x;
		bool force = false;
		if (cmd->getInt(t, "epoch", x))
		{
			cmd->getBool(t, "force", force);
			force = setDateTime((time_t)x, force);
		}
		else if (cmd->getBool(t, "force", force))
		{
			if (force)
			{
				force = saveDateTime();
			}
		}
		else
		{
			answer = "\"sync\":{\"error\":\"wrong param\"}";
			return answer;
		}

		if (force)
			answer = "\"sync\":{\"result\":true,\"epoch\":";
		else
			answer = "\"sync\":{\"result\":false,\"epoch\":";
		timeval tv_start;
		gettimeofday(&tv_start, nullptr);
		answer += std::to_string(tv_start.tv_sec);
		if (!mSync)
			answer += ",\"sync\":false";
		answer += "}";
	}
	return answer;
}

void CDateTimeSystem::log()
{
	timeval tv_start;
	gettimeofday(&tv_start, nullptr);
	time_t nowtime = tv_start.tv_sec;
	tm *t = localtime(&nowtime);
	char tmbuf[64];
	strftime(tmbuf, sizeof(tmbuf), "%Y-%m-%d %H:%M:%S", t);
	ESP_LOGI("DateTimeSystem", "time %s (sync:%d)", tmbuf, mSync);
}
