/*!
	\file
	\brief Класс для разбора json строк.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 1.2.0.0
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
#include <stdexcept>

static const char *TAG = "CJsonParser";

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
	return parse((uint8_t *)json, std::strlen(json));
}

int CJsonParser::parse(uint8_t *data, size_t size)
{
	jsmn_init(&mParser);
	mJson.clear();

	mRootSize = jsmn_parse(&mParser, (const char *)data, size, mRootTokens, mRootTokensSize);
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
			mRootTokensSize = jsmn_parse(&mParser, (const char *)data, size, nullptr, 0) + 1;
			mRootTokens = new jsmntok_t[mRootTokensSize];
			jsmn_init(&mParser);
			mRootSize = jsmn_parse(&mParser, (const char *)data, size, mRootTokens, mRootTokensSize);
		}
		else
		{
			ESP_LOGE(TAG, "JSON string error");
			return -1;
		}
	}
	if ((mRootSize > 1) && (mRootTokens[0].type == JSMN_OBJECT))
	{
		mJson = std::string((const char *)data, size);
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
					int index;
					while ((index = value.rfind("\\\"")) >= 0)
					{
						value.erase(index, 1);
					}
					return true;
				}
			}
		}
	}
	return false;
}

void CJsonParser::updateString(std::string &value)
{
	int index = 0;
	while ((index = value.find("\"", index)) >= 0)
	{
		value.insert(index, "\\");
		index += 2;
	}
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

bool CJsonParser::getUlong(int beg, const char *name, unsigned long &value)
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
					value = std::stoul(str, nullptr, 0);
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

bool CJsonParser::getDouble(int beg, const char *name, double &value)
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

bool CJsonParser::getArrayInt(int beg, const char *name, std::vector<int> *&data)
{
	data = nullptr;
	if (mJson.empty())
		return false;

	int sz = std::strlen(name);
	for (int i = beg; (i < (mRootSize - 1)) && (mRootTokens[beg].parent <= mRootTokens[i].parent); i++)
	{
		if (mRootTokens[beg].parent == mRootTokens[i].parent)
		{
			if ((sz == (mRootTokens[i].end - mRootTokens[i].start)) && (std::memcmp(name, &mJson[mRootTokens[i].start], sz) == 0))
			{
				if ((mRootTokens[i + 1].type == JSMN_ARRAY) && (mRootTokens[i + 1].parent == i))
				{
					if (mRootTokens[i + 1].size > 0)
					{
						data = new std::vector<int>(mRootTokens[i + 1].size);
						data->clear();
						for (int j = 0; j < data->capacity(); j++)
						{
							std::string str = mJson.substr(mRootTokens[j + i + 2].start, mRootTokens[j + i + 2].end - mRootTokens[j + i + 2].start);
							data->push_back(std::atoi((const char *)str.c_str()));
						}
					}
					return true;
				}
			}
		}
	}
	return false;
}

bool CJsonParser::getBytes(int beg, const char *name, std::vector<uint8_t> *&data)
{
	data = nullptr;
	if (mJson.empty())
		return false;

	std::string str;
	if (getString(beg, name, str))
	{
		std::string tmp;
		data = new std::vector<uint8_t>(str.size() / 2);
		data->clear();
		try
		{
			for (size_t i = 0; i < data->capacity(); i++)
			{
				tmp = str.substr(i * 2, 2);
				data->push_back(std::stoi(tmp, 0, 16));
			}
			return true;
		}
		catch (std::invalid_argument const &ex)
		{
			delete[] data;
			data = nullptr;
			ESP_LOGW(TAG, "getBytes failed %s", tmp.c_str());
			return false;
		}
	}
	else
		return false;
}

bool CJsonParser::getArrayBytes(int beg, const char *name, std::vector<std::vector<uint8_t> *> *&data)
{
	data = nullptr;
	if (mJson.empty())
		return false;

	int sz = std::strlen(name);
	for (int i = beg; (i < (mRootSize - 1)) && (mRootTokens[beg].parent <= mRootTokens[i].parent); i++)
	{
		if (mRootTokens[beg].parent == mRootTokens[i].parent)
		{
			if ((sz == (mRootTokens[i].end - mRootTokens[i].start)) && (std::memcmp(name, &mJson[mRootTokens[i].start], sz) == 0))
			{
				if ((mRootTokens[i + 1].type == JSMN_ARRAY) && (mRootTokens[i + 1].parent == i))
				{
					if (mRootTokens[i + 1].size > 0)
					{
						std::string tmp;
						data = new std::vector<std::vector<uint8_t> *>(mRootTokens[i + 1].size);
						data->clear();
						for (int j = 0; j < data->capacity(); j++)
						{
							std::string str = mJson.substr(mRootTokens[j + i + 2].start, mRootTokens[j + i + 2].end - mRootTokens[j + i + 2].start);
							data->push_back(new std::vector<uint8_t>(str.size() / 2));
							data->back()->clear();
							try
							{
								for (size_t i = 0; i < data->back()->capacity(); i++)
								{
									tmp = str.substr(i * 2, 2);
									data->back()->push_back(std::stoi(tmp, 0, 16));
								}
							}
							catch (std::invalid_argument const &ex)
							{
								for (auto &x : *data)
									delete x;
								delete data;
								data = nullptr;
								ESP_LOGW(TAG, "getArrayBytes failed %s", tmp.c_str());
								return false;
							}
						}
						return true;
					}
				}
			}
		}
	}
	return false;
}

bool CJsonParser::getArrayObject(int beg, const char *name, std::vector<int> *&data)
{
	data = nullptr;
	if (mJson.empty())
		return false;

	int sz = std::strlen(name);
	for (int i = beg; (i < (mRootSize - 1)) && (mRootTokens[beg].parent <= mRootTokens[i].parent); i++)
	{
		if (mRootTokens[beg].parent == mRootTokens[i].parent)
		{
			if ((sz == (mRootTokens[i].end - mRootTokens[i].start)) && (std::memcmp(name, &mJson[mRootTokens[i].start], sz) == 0))
			{
				if ((mRootTokens[i + 1].type == JSMN_ARRAY) && (mRootTokens[i + 1].parent == i))
				{
					int parent = i + 1;
					// ESP_LOGI(TAG, "sz=%d,%d", mRootTokens[parent].size, parent);
					if (mRootTokens[parent].size > 0)
					{
						data = new std::vector<int>(mRootTokens[parent].size);
						data->clear();
						i = parent + 1;
						sz = 0;
						while ((i < (mRootSize - 1)) && (sz < mRootTokens[parent].size))
						{
							// ESP_LOGW(TAG, "%d,%d,%d,%d", i, mRootTokens[i].type, mRootTokens[i].parent, mRootTokens[i].size);
							if ((mRootTokens[i].type == JSMN_OBJECT) && (mRootTokens[i].parent == parent) && (mRootTokens[i].size > 0))
							{
								// ESP_LOGI(TAG, "obj=%d", i);
								data->push_back(i + 1);
								sz++;
								i += 2;
							}
							else
								i++;
						}
						if (sz == mRootTokens[parent].size)
						{
							return true;
						}
						else
						{
							// ESP_LOGE(TAG, "size=%d,%d", data->size(),data->capacity());
							delete data;
							data = nullptr;
							return false;
						}
					}
					return false;
				}
			}
		}
	}
	return false;
}
