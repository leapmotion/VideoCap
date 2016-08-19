

#include "stdafx.h"
#include "samplegrab.h"
#include "FormatCorrector.h"

CComPtr<IBaseFilter> g_pGrabberFilter;
CComPtr<FormatCorrector> g_pFormatCorrector;

std::vector<unsigned char> buffer;

Gdiplus::Bitmap *pCapturedBitmap = 0;

int sgSetBitmapData(Gdiplus::Bitmap* pBitmap, const unsigned char* pData);
void sgFreeMediaType(AM_MEDIA_TYPE& mt);

void sgCloseSampleGrabber()
{
	if (pCapturedBitmap != 0) {
		::delete pCapturedBitmap;
		pCapturedBitmap = 0;
	}

	g_pGrabberFilter.Release();
	g_pFormatCorrector.Release();
}

HRESULT sgAddSampleGrabber(IGraphBuilder *pGraph, CComPtr<FormatCorrector>& pFormatCorrector)
{
	g_pFormatCorrector = new FormatCorrector;
	pFormatCorrector = g_pFormatCorrector;

        HRESULT hr = pGraph->AddFilter(pFormatCorrector, L"Format Corrector");
        if (FAILED(hr)) {
                return hr;
        }
	return hr;
}

Gdiplus::Bitmap *sgGetBitmap()
{
	ULONG width = g_pFormatCorrector->GetConnectedWidth();
	ULONG height = g_pFormatCorrector->GetConnectedHeight();
	ULONG channels = 3;

        if (g_pFormatCorrector == 0 || buffer.empty())
                return 0;

        if (pCapturedBitmap == 0)
                pCapturedBitmap = ::new Gdiplus::Bitmap(width, height, PixelFormat24bppRGB);
        else if (
		width != pCapturedBitmap->GetWidth() ||
		height != pCapturedBitmap->GetHeight()
	) {
                ::delete pCapturedBitmap;
                pCapturedBitmap = ::new Gdiplus::Bitmap(width, height, PixelFormat24bppRGB);
        }

	CComPtr<IMediaSample> lastSample = g_pFormatCorrector->GetLastOutputSample();
	ULONG actualLen = lastSample->GetActualDataLength();
        if (actualLen != width * height * channels)
                return 0;

	BYTE* pBuf;
	HRESULT hr = lastSample->GetPointer(&pBuf);
	if (FAILED(hr))
		return 0;

        if (sgSetBitmapData(pCapturedBitmap, pBuf) == 0)
                return pCapturedBitmap;
        else
                return 0;
}

unsigned char* sgGrabData()
{
	HRESULT hr;

	if (g_pFormatCorrector == 0)
		return 0;

	CComPtr<IMediaSample> currentSample = g_pFormatCorrector->GetLastInputSample();
	if (!currentSample)
		return 0;

	long Size = currentSample->GetSize();
	buffer.resize(Size);

	{
		BYTE* pSourceBuffer;
		hr = currentSample->GetPointer(&pSourceBuffer);
		if (FAILED(hr))
			return 0;
		memcpy(buffer.data(), pSourceBuffer, Size);
	}

	::CreateDirectory(L"C:\\pMonitor", NULL);
	CString StrText = _T("");
	static int strCount = 0;
	char * tmpch;
	StrText.Format(L"c:\\pMonitor\\_Sample_Data_%04d.raw", strCount++);

	int sLen = WideCharToMultiByte(CP_ACP, 0, StrText, -1, NULL, 0, NULL, NULL);
	tmpch = new char[sLen + 1];
	WideCharToMultiByte(CP_ACP, 0, StrText, -1, tmpch, sLen, NULL, NULL);

	FILE* fp = fopen(tmpch, "wb");
	if (fp != NULL) {
		fwrite(buffer.data(), Size, 1, fp);
		fclose(fp);
	}

	delete []tmpch;

	return buffer.data();
}

long sgGetBufferSize()
{
        return buffer.size();
}

int sgSetBitmapData(Gdiplus::Bitmap* pBitmap, const unsigned char* pData)
{
        Gdiplus::BitmapData bitmapData;
        bitmapData.Width = pBitmap->GetWidth();
        bitmapData.Height = pBitmap->GetHeight();
        bitmapData.Stride = 3 * bitmapData.Width;
        bitmapData.PixelFormat = pBitmap->GetPixelFormat();
        bitmapData.Scan0 = (VOID*)pData;
        bitmapData.Reserved = NULL;

        Gdiplus::Status s = pBitmap->LockBits(&Gdiplus::Rect(0, 0, pBitmap->GetWidth(), pBitmap->GetHeight()),
                                              Gdiplus::ImageLockModeWrite | Gdiplus::ImageLockModeUserInputBuf,
                                              PixelFormat24bppRGB, &bitmapData);
        if (s == Gdiplus::Ok)
                pBitmap->UnlockBits(&bitmapData);

        return s;
}

void sgFlipUpDown(unsigned char* pData)
{
	ULONG width = g_pFormatCorrector->GetConnectedWidth();
	ULONG height = g_pFormatCorrector->GetConnectedHeight();
	ULONG channels = g_pFormatCorrector->GetInputChannels();

        unsigned char* scan0 = pData;
        unsigned char* scan1 = pData + ((width * height * channels) - (width * channels));

        for (unsigned int y = 0; y < height / 2; y++) {
                for (unsigned int x = 0; x < width * channels; x++) {
                        swap(scan0[x], scan1[x]);
                }
                scan0 += width * channels;
                scan1 -= width * channels;
        }

}

void sgFreeMediaType(AM_MEDIA_TYPE& mt)
{
    if (mt.cbFormat != 0)
    {
        CoTaskMemFree((PVOID)mt.pbFormat);
        mt.cbFormat = 0;
        mt.pbFormat = NULL;
    }
    if (mt.pUnk != NULL)
    {
        // Unecessary because pUnk should not be used, but safest.
        mt.pUnk->Release();
        mt.pUnk = NULL;
    }
}

