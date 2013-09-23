#include "Grabber.h"

#include <Windows.h>
#include <gdiplus.h>

//http://msdn.microsoft.com/en-us/library/windows/desktop/ms533843%28v=vs.85%29.aspx
int Grabber::GetEncoderCLSID(const WCHAR* format, CLSID* pClsid)
{
	UINT  num = 0;          // number of image encoders
	UINT  size = 0;         // size of the image encoder array in bytes

	Gdiplus::ImageCodecInfo* pImageCodecInfo = NULL;

	Gdiplus::GetImageEncodersSize(&num, &size);
	if(size == 0)
		return -1;  // Failure

	pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
	if(pImageCodecInfo == NULL)
		return -1;  // Failure

	Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

	for(UINT j = 0; j < num; ++j)
	{
		if( wcscmp(pImageCodecInfo[j].MimeType, format) == 0 )
		{
			*pClsid = pImageCodecInfo[j].Clsid;
			free(pImageCodecInfo);
			return j;  // Success
		}    
	}

	free(pImageCodecInfo);
	return -1;  // Failure
}

std::vector<uint8_t> Grabber::bitmapToPNGData(HBITMAP bmp)
{
	CLSID pngClsid;
	GetEncoderCLSID(L"image/png", &pngClsid);
	Gdiplus::Bitmap bitmap(bmp, NULL);

	IStream *memStream;
	CreateStreamOnHGlobal(NULL, TRUE, &memStream);
	bitmap.Save(memStream, &pngClsid);

	LARGE_INTEGER zero;
	zero.QuadPart = 0;
	STATSTG stat;
	memStream->Stat(&stat, 0);

	std::vector<uint8_t> result(static_cast<size_t>(stat.cbSize.QuadPart));
	memStream->Seek(zero, STREAM_SEEK_SET, NULL);
	memStream->Read(&result[0], static_cast<size_t>(stat.cbSize.QuadPart), NULL);
	memStream->Release();

	return result;
}

std::vector<uint8_t> Grabber::grabActive()
{
	HDC screenDC = GetDC(NULL);
	HDC memDC = CreateCompatibleDC(screenDC);
	HWND foregroundWnd = GetForegroundWindow();
	RECT foregroundRect;
	GetWindowRect(foregroundWnd, &foregroundRect);
	HBITMAP bmp = CreateCompatibleBitmap(screenDC, foregroundRect.right - foregroundRect.left, foregroundRect.bottom - foregroundRect.top);
	SelectObject(memDC, bmp);

	if(!PrintWindow(foregroundWnd, memDC, 0))
		BitBlt(memDC, 0, 0, foregroundRect.right - foregroundRect.left, foregroundRect.bottom - foregroundRect.top, screenDC, foregroundRect.left, foregroundRect.top, SRCCOPY);

	std::vector<uint8_t> result = bitmapToPNGData(bmp);

	DeleteObject(bmp);
	DeleteDC(memDC);

	return result;
}

std::vector<uint8_t> Grabber::grab(int sx, int sy, int ex, int ey)
{
	//Capture
	int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
	int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
	int left = GetSystemMetrics(SM_XVIRTUALSCREEN);
	int top = GetSystemMetrics(SM_YVIRTUALSCREEN);

	if(sx == -1)
		sx = left;
	if(sy == -1)
		sy = top;
	if(ex == -1)
		ex = left + width;
	if(ey == -1)
		ey = top + height;

	HDC screenDC = GetDC(NULL);
	HDC memDC = CreateCompatibleDC(screenDC);
	HBITMAP bmp = CreateCompatibleBitmap(screenDC, ex - sx, ey - sy);
	SelectObject(memDC, bmp);
	BitBlt(memDC, 0, 0, ex - sx, ey - sy, screenDC, sx, sy, SRCCOPY);

	std::vector<uint8_t> result = bitmapToPNGData(bmp);

	DeleteObject(bmp);
	DeleteDC(memDC);

	return result;
}
