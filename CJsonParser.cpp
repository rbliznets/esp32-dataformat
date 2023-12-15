/*!
	\file
	\brief Класс для разбора json строк.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 1.0.0.0
	\date 28.10.2021

	Декоратор для https://github.com/zserge/jsmn
*/

#define JSMN_PARENT_LINKS
#include "jsmn.h"

#include "CJsonParser.h"
#include <cstring>
#include <cstdlib>
#include "sdkconfig.h"
#include "esp_log.h"

static const char* TAG="CJsonParser";

CJsonParser::CJsonParser() : mRootTokensSize(CONFIG_JSON_MIN_TOKEN_SIZE)
{
	mRootTokens = new jsmntok_t[mRootTokensSize];
}

CJsonParser::~CJsonParser()
{
	delete[] mRootTokens;
}

int CJsonParser::parse(const char *json)
{
	jsmn_init(&mParser);
	mJson.clear();

	mRootSize = jsmn_parse(&mParser, (const char *)json, std::strlen(json), mRootTokens, mRootTokensSize);
	if (mRootSize < 0)
	{
		if (mRootSize == JSMN_ERROR_INVAL)
		{
			ESP_LOGE(TAG, "bad token, JSON string is corrupted");
			return -1;
		}
		else if (mRootSize == JSMN_ERROR_PART)
		{
			ESP_LOGE(TAG, "JSON string is too short");
			return -1;
		}
		else if (mRootSize == JSMN_ERROR_NOMEM)
		{
			delete[] mRootTokens;
			mRootTokensSize = jsmn_parse(&mParser, (const char *)json, std::strlen(json), nullptr, 0) + 1;
			mRootTokens = new jsmntok_t[mRootTokensSize];
			jsmn_init(&mParser);
			mRootSize = jsmn_parse(&mParser, (const char *)json, std::strlen(json), mRootTokens, mRootTokensSize);
		}
		else
		{
			ESP_LOGE(TAG, "JSON string error");
			return -1;
		}
	}
	if ((mRootSize > 1) && (mRootTokens[0].type == JSMN_OBJECT))
	{
		mJson = json;
		return 1;
	}
	else
	{
		ESP_LOGE(TAG, "root error");
		return 0;
	}
}

bool CJsonParser::getString(int beg, const char *name, std::string &value)
{
	if (mJson.empty())
		return false;

	int sz = std::strlen(name);
	for (int i = beg; (i < (mRootSize - 1)) && (mRootTokens[beg].parent <= mRootTokens[i].parent); i++)
	{
		if (mRootTokens[beg].parent == mRootTokens[i].parent)
		{
			if ((sz == (mRootTokens[i].end - mRootTokens[i].start)) && (std::memcmp(name, &mJson[mRootTokens[i].start], sz) == 0))
			{
				if ((mRootTokens[i + 1].type == JSMN_STRING) && (mRootTokens[i + 1].parent == i))
				{
					value = mJson.substr(mRootTokens[i + 1].start, (mRootTokens[i + 1].end - mRootTokens[i + 1].start));
					return true;
				}
			}
		}
	}
	return false;
}

bool CJsonParser::getField(int beg, const char *name)
{
	if (mJson.empty())
		return false;

	int sz = std::strlen(name);
	for (int i = beg; (i < (mRootSize - 1)) && (mRootTokens[beg].parent <= mRootTokens[i].parent); i++)
	{
		if (mRootTokens[beg].parent == mRootTokens[i].parent)
		{
			if ((sz == (mRootTokens[i].end - mRootTokens[i].start)) && (std::memcmp(name, &mJson[mRootTokens[i].start], sz) == 0))
			{
				if ((mRootTokens[i + 1].type == JSMN_PRIMITIVE) && (mRootTokens[i + 1].parent == i))
				{
					if (mJson[mRootTokens[i + 1].start] == 'n')
					{
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool CJsonParser::getObject(int beg, const char *name, int &value)
{
	if (mJson.empty())
		return false;

	int sz = std::strlen(name);
	for (int i = beg; (i < (mRootSize - 1)) && (mRootTokens[beg].parent <= mRootTokens[i].parent); i++)
	{
		if (mRootTokens[beg].parent == mRootTokens[i].parent)
		{
			if ((sz == (mRootTokens[i].end - mRootTokens[i].start)) && (std::memcmp(name, &mJson[mRootTokens[i].start], sz) == 0))
			{
				if ((mRootTokens[i + 1].type == JSMN_OBJECT) && (mRootTokens[i + 1].parent == i) && (mRootTokens[i + 1].size > 0))
				{
					value = i + 2;
					return true;
				}
			}
		}
	}
	return false;
}

bool CJsonParser::getInt(int beg, const char *name, int &value)
{
	if (mJson.empty())
		return false;

	int sz = std::strlen(name);
	for (int i = beg; (i < (mRootSize - 1)) && (mRootTokens[beg].parent <= mRootTokens[i].parent); i++)
	{
		if (mRootTokens[beg].parent == mRootTokens[i].parent)
		{
			if ((sz == (mRootTokens[i].end - mRootTokens[i].start)) && (std::memcmp(name, &mJson[mRootTokens[i].start], sz) == 0))
			{
				if ((mRootTokens[i + 1].type == JSMN_PRIMITIVE) && (mRootTokens[i + 1].parent == i))
				{
					std::string str = mJson.substr(mRootTokens[i + 1].start, mRootTokens[i + 1].end - mRootTokens[i + 1].start);
					value = std::atoi((const char *)str.c_str());
					return true;
				}
			}
		}
	}
	return false;
}

bool CJsonParser::getFloat(int beg, const char *name, float &value)
{
	if (mJson.empty())
		return false;

	int sz = std::strlen(name);
	for (int i = beg; (i < (mRootSize - 1)) && (mRootTokens[beg].parent <= mRootTokens[i].parent); i++)
	{
		if (mRootTokens[beg].parent == mRootTokens[i].parent)
		{
			if ((sz == (mRootTokens[i].end - mRootTokens[i].start)) && (std::memcmp(name, &mJson[mRootTokens[i].start], sz) == 0))
			{
				if ((mRootTokens[i + 1].type == JSMN_PRIMITIVE) && (mRootTokens[i + 1].parent == i))
				{
					std::string str = mJson.substr(mRootTokens[i + 1].start, mRootTokens[i + 1].end - mRootTokens[i + 1].start);
					value = std::atof((const char *)str.c_str());
					return true;
				}
			}
		}
	}
	return false;
}

bool CJsonParser::getBool(int beg, const char *name, bool &value)
{
	if (mJson.empty())
		return false;

	int sz = std::strlen(name);
	for (int i = beg; (i < (mRootSize - 1)) && (mRootTokens[beg].parent <= mRootTokens[i].parent); i++)
	{
		if (mRootTokens[beg].parent == mRootTokens[i].parent)
		{
			if ((sz == (mRootTokens[i].end - mRootTokens[i].start)) && (std::memcmp(name, &mJson[mRootTokens[i].start], sz) == 0))
			{
				if ((mRootTokens[i + 1].type == JSMN_PRIMITIVE) && (mRootTokens[i + 1].parent == i))
				{
					if (mJson[mRootTokens[i + 1].start] == 'f')
					{
						value = false;
						return true;
					}
					else if (mJson[mRootTokens[i + 1].start] == 't')
					{
						value = true;
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool CJsonParser::getArrayInt(int beg, const char *name, int *&data, int &size)
{
	if (mJson.empty())
		return false;

	int sz = std::strlen(name);
	for (int i = beg; (i < (mRootSize - 1)) && (mRootTokens[beg].parent <= mRootTokens[i].parent); i++)
	{
		if (mRootTokens[beg].parent == mRootTokens[i].parent)
		{
			if ((sz == (mRootTokens[i].end - mRootTokens[i].start)) && (std::memcmp(name, &mJson[mRootTokens[i].start], sz) == 0))
			{
				if ((mRootTokens[i + 1].type == JSMN_ARRAY) && (mRootTokens[i + 1].parent == i) && (mRootTokens[i + 1].size > 0))
				{
					size = mRootTokens[i + 1].size;
					data = new int[size];
					for (int j = 0; j < size; j++)
					{
						std::string str = mJson.substr(mRootTokens[j + i + 2].start, mRootTokens[j + i + 2].end - mRootTokens[j + i + 2].start);
						data[j] = std::atoi((const char *)str.c_str());
					}
					return true;
				}
			}
		}
	}
	return false;
}
