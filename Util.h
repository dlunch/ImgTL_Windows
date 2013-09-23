#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <sstream>

inline void AppendVector(std::vector<uint8_t> &vector, std::string &string)
{
	vector.insert(vector.end(), string.begin(), string.end());
}

template<typename T>
inline std::string ToString(T data)
{
	std::stringstream str;
	str << data;
	return str.str();
}

inline double StrToDouble(const std::string &str)
{
	double value = 0;
	int divisor = 1;
	size_t i = 0;
	bool isFraction = false;
	if(str[0] == '-')
		i ++;
	for(; i < str.size(); i ++)
	{
		if(str[i] <= '9' && str[i] >= '0')
		{
			if(isFraction)
				divisor *= 10;
			else
				value *= 10;
			value += (double)(str[i] - '0') / divisor;
		}
		else if(str[i] == '.')
		{
			isFraction = true;
			continue;
		}
		else
			break;
	}
	if(str[0] == '-')
		return -value;

	return value;
}

inline int hexStrToInt(const std::string &hexString)
{
	int value = 0;
	for(size_t i = 0; i < hexString.size(); i ++)
	{
		value *= 16;
		if(hexString[i] <= '9' && hexString[i] >= '0')
			value += hexString[i] - '0';
		else if(hexString[i] <= 'f' && hexString[i] >= 'a')
			value += hexString[i] - 'a' + 10;
		else
			break;
	}

	return value;
}

inline std::string wstringToString(const std::wstring &input)
{
	std::string result;
	if(sizeof(wchar_t) == 2)
	{
		//utf16
		for(size_t i = 0; i < input.length();)
		{
			if(input[i] < 0xD800 || input[i] > 0xDFFF)
			{ //no surrogate pair
				if(input[i] < 0x80) //1 byte
				{
					result.push_back((char)input[i]);
				}
				else if(input[i] < 0x800)
				{
					result.push_back((char)(0xC0 | ((input[i] & 0x7C0) >> 6)));
					result.push_back((char)(0x80 | (input[i] & 0x3F)));
				}
				else
				{
					result.push_back((char)(0xE0 | ((input[i] & 0xF000) >> 12)));
					result.push_back((char)(0x80 | ((input[i] & 0xFC0) >> 6)));
					result.push_back((char)(0x80 | (input[i] & 0x3F)));
				}
			}
			else
			{
				size_t temp;
				temp = (input[i] & 0x3FF << 10) | (input[i + 1] & 0x3FF);
				temp += 0x10000;
				if(temp < 0x80) //1 byte
				{
					result.push_back((char)temp);
				}
				else if(temp < 0x800)
				{
					result.push_back((char)(0xC0 | ((temp & 0x7C0) >> 6)));
					result.push_back((char)(0x80 | (temp & 0x3F)));
				}
				else if(temp < 0x10000)
				{
					result.push_back((char)(0xE0 | ((temp & 0xF000) >> 12)));
					result.push_back((char)(0x80 | ((temp & 0xFC0) >> 6)));
					result.push_back((char)(0x80 | (temp & 0x3F)));
				}
				else
				{
					result.push_back((char)(0xF0 | ((temp & 0x1C0000) >> 18)));
					result.push_back((char)(0x80 | ((temp & 0x3F000) >> 12)));
					result.push_back((char)(0x80 | ((temp & 0xFC0) >> 6)));
					result.push_back((char)(0x80 | (temp & 0x3F)));
				}
				i ++;
			}
			i ++;
		}
	}
	return result;
}