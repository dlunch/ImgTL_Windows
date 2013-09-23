#pragma once

#include "Util.h"
#include <map>
#include <vector>
#include <memory>

class JSONParseException : public std::exception
{
public:
	JSONParseException() : std::exception("Exception while parsing JSON string") {}
};

class JSONValueTypeMismatchException : public std::exception
{
public:
	JSONValueTypeMismatchException() : std::exception("Exception while retrieving JSON value") {}
};

class JSONValue;
typedef std::map<std::string, JSONValue> JSONObject;

class JSONValue
{
private:
	std::shared_ptr<void> m_value;
	enum JSONValueType
	{
		TYPE_INVALID = 0,
		TYPE_NUMBER,
		TYPE_STRING,
		TYPE_OBJECT,
		TYPE_NULL,
		TYPE_BOOLEAN,
		TYPE_ARRAY,
	};
	JSONValueType m_type;
public:
	JSONValue() : m_type(TYPE_INVALID) {}
	~JSONValue() {}
	void setBooleanValue(const bool value)
	{
		m_type = TYPE_BOOLEAN;
		m_value = std::shared_ptr<int>(new int(value));
	}
	void setNull()
	{
		m_type = TYPE_NULL;
	}
	void setNumberValue(const double value)
	{
		m_type = TYPE_NUMBER;
		m_value = std::shared_ptr<double>(new double(value));
	}
	void setStringValue(const std::string &value)
	{
		m_type = TYPE_STRING;
		m_value = std::shared_ptr<std::string>(new std::string(value));
	}

	void setObjectValue(const JSONObject &value)
	{
		m_type = TYPE_OBJECT;
		m_value = std::shared_ptr<JSONObject>(new JSONObject(value));
	}

	void setArrayValue(const std::vector<JSONValue> &value)
	{
		m_type = TYPE_ARRAY;
		m_value = std::shared_ptr<std::vector<JSONValue> >(new std::vector<JSONValue>(value));
	}

	const double operator =(const double value)
	{
		setNumberValue(value);
	}

	const std::string &operator =(const std::string &value)
	{
		setStringValue(value);
	}

	const JSONObject &operator =(const JSONObject &value)
	{
		setObjectValue(value);
	}

	const std::vector<JSONValue> &operator =(const std::vector<JSONValue> &value)
	{
		setArrayValue(value);
	}

	operator int()
	{
		if(m_type == TYPE_NULL)
			return 0;
		if(m_type != TYPE_NUMBER && m_type != TYPE_BOOLEAN)
			throw JSONValueTypeMismatchException();
		if(m_type == TYPE_NUMBER)
			return (int)*((double *)m_value.get());
		else
			return *((int *)m_value.get());
	}

	JSONValue &operator[](const std::string &key)
	{
		if(m_type == TYPE_INVALID)
			setObjectValue(JSONObject());
		if(m_type != TYPE_OBJECT)
			throw JSONValueTypeMismatchException();
		return (*(JSONObject *)m_value.get())[key];
	}

	JSONValue &operator[](const char *key)
	{
		return operator [](std::string(key));
	}

	JSONValue &operator[](int index)
	{
		if(m_type == TYPE_INVALID)
			setArrayValue(std::vector<JSONValue>());
		if(m_type != TYPE_ARRAY)
			throw JSONValueTypeMismatchException();
		return (*(std::vector<JSONValue> *)m_value.get())[index]; 
	}

	operator std::string()
	{
		if(m_type != TYPE_STRING)
			throw JSONValueTypeMismatchException();
		return *((std::string *)m_value.get());
	}

	int getSize()
	{
		if(m_type == TYPE_ARRAY)
			return ((std::vector<JSONValue> *)m_value.get())->size();
		throw JSONValueTypeMismatchException();
	}

	bool hasKey(const std::string &key)
	{
		if(m_type == TYPE_OBJECT)
			return ((JSONObject *)m_value.get())->find(key) != ((JSONObject *)m_value.get())->end();
		throw JSONValueTypeMismatchException();
	}
};

class JSONParser 
{
private:
	static int getNextDelimiterPos(int pos, const std::string &string)
	{
		std::string::const_iterator it = string.begin() + pos;
		int i = 0;

		for(; it != string.end(); it ++, i ++)
		{
			if(*it == ' ' || *it == '\t' || *it == '\r' || *it == '\n' || *it == ',' || *it == ']' || *it == '}')
				break;
		}
		return pos + i;
	}
	static int getNextTokenPos(int pos, const std::string &string)
	{
		std::string::const_iterator it = string.begin() + pos;
		int i = 0;

		for(; it != string.end(); it ++, i ++)
		{
			if(*it == ' ' || *it == '\t' || *it == '\r' || *it == '\n')
				continue;
			break;
		}
		return pos + i;
	}
	static JSONValue getJSONValue(int &pos, const std::string &string)
	{
		int data = string.at(pos);
		JSONValue ret;
		if(data == '[')
		{
			pos ++;
			//begin of array
			std::vector<JSONValue> values;
			while(true)
			{
				pos = getNextTokenPos(pos, string);
				if(string.at(pos) == ']')
					break;
				if(string.at(pos) == ',')
				{
					pos ++;
					continue;
				}
				values.push_back(getJSONValue(pos, string));
			}
			pos ++;
			ret.setArrayValue(values);
		}
		else if(data == '{')
		{
			pos ++;
			//begin of object
			JSONObject object;
			while(true)
			{
				pos = getNextTokenPos(pos, string);
				if(string.at(pos) == '}')
					break;
				if(string.at(pos) == ',')
				{
					pos ++;
					continue;
				}

				std::string key = getJSONString(pos, string);

				pos = getNextTokenPos(pos, string);
				data = string.at(pos);
				if(data != ':')
					throw JSONParseException();
				pos ++;

				pos = getNextTokenPos(pos, string);
				object[key] = getJSONValue(pos, string);
			}
			pos ++;
			ret.setObjectValue(object);
		}
		else if(data == '\"')
		{
			ret.setStringValue(getJSONString(pos, string));
		}
		else 
		{
			//integer or keyword
			int pos1 = getNextDelimiterPos(pos, string);
			std::string temp = string.substr(pos, pos1 - pos);
			if(temp == "true")
				ret.setBooleanValue(true);
			else if(temp == "false")
				ret.setBooleanValue(false);
			else if(temp == "null")
				ret.setNull();
			else
				ret.setNumberValue(StrToDouble(temp));
			pos = pos1;
		}

		return ret;
	}
	static std::string getJSONString(int &pos, const std::string &string)
	{
		int data;
		data = string.at(pos);
		if(data != '\"')
			throw JSONParseException();
		pos ++;

		std::string ret;
		while(true)
		{
			if(string.at(pos) == '\"')
				break;
			if(string.at(pos) == '\\' && string.at(pos + 1) == '\"')
			{
				ret.push_back('\"');
				pos += 2;
				continue;
			}
			if(string.at(pos) == '\\' && string.at(pos + 1) == 'u')
			{
				//unicode
				wchar_t data = hexStrToInt(string.substr(pos + 2, 4));
				ret.append(wstringToString(std::wstring(data, 1)));
				pos += 6;
				continue;
			}
			ret.push_back(string.at(pos));
			pos ++;
		}
		pos ++; //ending "

		return ret;
	}
public:
	static JSONValue parseJSONString(const std::string &string)
	{
		int pos = 0;
		return getJSONValue(pos, string);
	}
};
