#pragma once

#include <Windows.h>

#include <vector>
#include <cstdint>

class Grabber
{
private:
	int GetEncoderCLSID(const WCHAR* format, CLSID* pClsid);
	std::vector<uint8_t> bitmapToPNGData(HBITMAP bmp);
public:
	std::vector<uint8_t> grabActive();
	std::vector<uint8_t> grab(int sx = -1, int sy = -1, int ex = -1, int ey = -1);
};