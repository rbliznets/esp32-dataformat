/*!
	\file
	\brief Класс для разбора json строк.
	\authors Близнец Р.А. (r.bliznets@gmail.com)
	\version 1.0.0.0
	\date 28.10.2021

	Декоратор для https://github.com/zserge/jsmn
*/

#if !defined CJSONPARSER_H
#define CJSONPARSER_H

#define JSMN_HEADER
#include "jsmn.h"

#include <string>
#include <cstring>

/// Класс для разбора json строки.
/*!
  Ограничение для массивов. Только для чисел
*/
class CJsonParser
{
protected:
	jsmn_parser mParser;	///< Данные парсера.
	jsmntok_t *mRootTokens; ///< Массив токенов.
	int mRootTokensSize;	///< Размер массива токенов.
	int mRootSize;			///< Количество токенов в массиве.

	std::string mJson; ///< Парсируемая строка.

public:
	/// Конструктор класса.
	CJsonParser();
	/// Деструктор класса.
	~CJsonParser();

	/// Парсинг.
	/*!
	  \param[in] json Парсируемая строка.
	  \return 1 (индекс первого токена) в случае успеха, иначе ошибка
	*/
	int parse(const char *json);
	/// Строка Json.
	/*!
	  \return Строка Json
	*/
	inline const char *getJson() { return mJson.c_str(); };

	/// Получить поле null.
	/*!
	  \param[in] beg индекс первого токена объекта.
	  \param[in] name название поля.
	  \return true в случае успеха
	*/
	bool getField(int beg, const char *name);
	/// Получить строковое поле.
	/*!
	  \param[in] beg индекс первого токена объекта.
	  \param[in] name название поля.
	  \param[out] value значение поля.
	  \return true в случае успеха
	*/
	bool getString(int beg, const char *name, std::string &value);
	/// Получить поле int.
	/*!
	  \param[in] beg индекс первого токена объекта.
	  \param[in] name название поля.
	  \param[out] value значение поля.
	  \return true в случае успеха
	*/
	bool getInt(int beg, const char *name, int &value);
    /// Получить поле unsigned int.
    /*!
      \param[in] beg индекс первого токена объекта.
      \param[in] name название поля.
      \param[out] value значение поля.
      \return true в случае успеха
    */
    bool getUlong(int beg, const char *name, unsigned long &value);
	/// Получить поле float.
	/*!
	  \param[in] beg индекс первого токена объекта.
	  \param[in] name название поля.
	  \param[out] value значение поля.
	  \return true в случае успеха
	*/
	bool getFloat(int beg, const char *name, float &value);
	/// Получить логическое поле.
	/*!
	  \param[in] beg индекс первого токена объекта.
	  \param[in] name название поля.
	  \param[out] value значение поля.
	  \return true в случае успеха
	*/
	bool getBool(int beg, const char *name, bool &value);
	/// Получить поле объекта.
	/*!
	  \param[in] beg индекс первого токена объекта.
	  \param[in] name название поля.
	  \param[out] value значение поля.
	  \return true в случае успеха
	*/
	bool getObject(int beg, const char *name, int &value);

	/// Получить массив int.
	/*!
	  \param[in] beg индекс первого токена объекта.
	  \param[in] name название поля.
	  \param[out] data данные.
	  \param[out] size размер данных.
	  \return true в случае успеха

	  После использования уничтожить данные delete[] data.
	*/
	bool getArrayInt(int beg, const char *name, int *&data, int &size);
};

#endif // CJSONPARSER_H
