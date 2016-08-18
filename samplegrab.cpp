

#include "stdafx.h"
#include "samplegrab.h"


IBaseFilter *pGrabberFilter = NULL;
ISampleGrabber *pGrabber = NULL;

long pBufferSize = 0;
unsigned char* pBuffer = 0;

Gdiplus::Bitmap *pCapturedBitmap = 0;
unsigned int gWidth = 0;
unsigned int gHeight = 0;
unsigned int gChannels = 0;

CRITICAL_SECTION mediaSampleLock;
CComPtr<IMediaSample> lastMediaSample;

int sgSetBitmapData(Gdiplus::Bitmap* pBitmap, const unsigned char* pData);
void sgFlipUpDown(unsigned char* pData);
void sgFreeMediaType(AM_MEDIA_TYPE& mt);


void sgInitialize()
{
	InitializeCriticalSection(&mediaSampleLock);
}

void sgCleanup()
{
	DeleteCriticalSection(&mediaSampleLock);
}

IBaseFilter* sgGetSampleGrabber()
{
        return pGrabberFilter;
}

void sgCloseSampleGrabber()
{
        if (pBuffer != 0) {
                delete[] pBuffer;
                pBuffer = 0;
                pBufferSize = 0;
        }

	if (pCapturedBitmap != 0) {
		::delete pCapturedBitmap;
		pCapturedBitmap = 0;
	}

	SAFE_RELEASE(pGrabberFilter);
	SAFE_RELEASE(pGrabber);
	lastMediaSample = NULL;

	gWidth = 0;
	gHeight = 0;
	gChannels = 0;
}

HRESULT sgAddSampleGrabber(IGraphBuilder *pGraph)
{
        // Create the Sample Grabber.
        HRESULT hr = CoCreateInstance(CLSID_SampleGrabber, NULL, CLSCTX_INPROC_SERVER,
                                      IID_IBaseFilter, (void**) & pGrabberFilter);
        if (FAILED(hr)) {
                return hr;
        }
        hr = pGraph->AddFilter(pGrabberFilter, L"Sample Grabber");
        if (FAILED(hr)) {
                return hr;
        }

        pGrabberFilter->QueryInterface(IID_ISampleGrabber, (void**)&pGrabber);
        return hr;
}

class FormatFixer :
	public ISampleGrabberCB
{
public:
	FormatFixer(void) :
		count(0)
	{}

	ULONG STDMETHODCALLTYPE AddRef(void) {
		return count++;
	}

	ULONG STDMETHODCALLTYPE Release(void) {
		if (!--count) {
			delete this;
			return 0;
		}
		return count;
	}

	ULONG count;

	STDMETHODIMP SampleCB(double SampleTime, IMediaSample *pSample) {
		BYTE* pBuffer;
		HRESULT hr;

		hr = pSample->GetPointer(&pBuffer);
		if (FAILED(hr))
			return hr;

		for (unsigned int y = 0; y < gHeight; y++) {
			for(unsigned int x = 0; x < gWidth; x++) {
				int sum =
					(pBuffer[x * 3 + 0] << 16) |
					(pBuffer[x * 3 + 1] << 8) |
					(pBuffer[x * 3 + 2] << 0);

				unsigned char normalized = (unsigned char)(sum * 0x100 / 0x1000000);

				pBuffer[x * 3 + 0] = normalized;
				pBuffer[x * 3 + 1] = normalized;
				pBuffer[x * 3 + 2] = normalized;
			}
			pBuffer += gWidth * gChannels;
		}

		// Ensure that destruction of the media sample happens outside of the lock
		CComPtr<IMediaSample> priorSample;
		{
			EnterCriticalSection(&mediaSampleLock);
			priorSample = lastMediaSample;
			lastMediaSample = pSample;
			LeaveCriticalSection(&mediaSampleLock);
		}

		return S_OK;
	}
	STDMETHODIMP BufferCB(double SampleTime, BYTE *pBuffer, long BufferLen) {
		return E_NOTIMPL;
	}

	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, void** ppvObject)
	{
		if (riid == IID_ISampleGrabber)
			*ppvObject = (ISampleGrabber*)this;
		else if (riid == IID_IUnknown)
			*ppvObject = (IUnknown*)this;
		else
			return E_NOINTERFACE;
		return S_OK;
	}
};

HRESULT sgSetSampleGrabberMediaType()
{
	AM_MEDIA_TYPE mt;
	ZeroMemory(&mt, sizeof(AM_MEDIA_TYPE));
	mt.majortype = MEDIATYPE_Video;
	mt.subtype = MEDIASUBTYPE_RGB24;
	HRESULT hr = pGrabber->SetMediaType(&mt);
	if (FAILED(hr)) {
		return hr;
	}
	hr = pGrabber->SetOneShot(FALSE);
	hr = pGrabber->SetBufferSamples(FALSE);
	return hr;
}

HRESULT sgSetSampleGrabberCallbacks()
{
	CComPtr<FormatFixer> pFixer(new FormatFixer());
	return pGrabber->SetCallback(pFixer, 0);
}

HRESULT sgGetSampleGrabberMediaType()
{
        AM_MEDIA_TYPE mt;
        HRESULT hr = pGrabber->GetConnectedMediaType(&mt);
        if (FAILED(hr)) {
                return hr;
        }

        VIDEOINFOHEADER *pVih = (VIDEOINFOHEADER *)mt.pbFormat;
        gChannels = pVih->bmiHeader.biBitCount / 8;
        gWidth = pVih->bmiHeader.biWidth;
        gHeight = pVih->bmiHeader.biHeight;

        sgFreeMediaType(mt);
        return hr;
}

Gdiplus::Bitmap *sgGetBitmap()
{
        if (pGrabber == 0 || pBuffer == 0 || gChannels != 3)
                return 0;

        if (pCapturedBitmap == 0)
                pCapturedBitmap = ::new Gdiplus::Bitmap(gWidth, gHeight, PixelFormat24bppRGB);
        else if (gWidth != pCapturedBitmap->GetWidth() || gHeight != pCapturedBitmap->GetHeight()) {
                ::delete pCapturedBitmap;
                pCapturedBitmap = ::new Gdiplus::Bitmap(gWidth, gHeight, PixelFormat24bppRGB);
        }

        if (pBufferSize != gWidth * gHeight * gChannels)
                return 0;

        if (sgSetBitmapData(pCapturedBitmap, pBuffer) == 0)
                return pCapturedBitmap;
        else
                return 0;
}

unsigned char* sgGrabData()
{
	HRESULT hr;

	if (pGrabber == 0)
		return 0;

	CComPtr<IMediaSample> currentSample;
	{
		EnterCriticalSection(&mediaSampleLock);
		currentSample = lastMediaSample;
		LeaveCriticalSection(&mediaSampleLock);
	}
	if (!currentSample)
		return 0;

	long Size = currentSample->GetSize();
	if (Size != pBufferSize) {
		pBufferSize = Size;
		if (pBuffer != 0)
			delete[] pBuffer;
		pBuffer = new unsigned char[pBufferSize];
	}

	{
		BYTE* pSourceBuffer;
		hr = currentSample->GetPointer(&pSourceBuffer);
		if (FAILED(hr))
			return 0;
		memcpy(pBuffer, pSourceBuffer, Size);
	}

	sgFlipUpDown(pBuffer);

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
		fwrite(pBuffer, Size, 1, fp);
		fclose(fp);
	}

	delete []tmpch;

	return pBuffer;
}

long sgGetBufferSize()
{
        return pBufferSize;
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
        unsigned char* scan0 = pData;
        unsigned char* scan1 = pData + ((gWidth * gHeight * gChannels) - (gWidth * gChannels));

        for (unsigned int y = 0; y < gHeight / 2; y++) {
                for (unsigned int x = 0; x < gWidth * gChannels; x++) {
                        swap(scan0[x], scan1[x]);
                }
                scan0 += gWidth * gChannels;
                scan1 -= gWidth * gChannels;
        }

}

unsigned int sgGetDataWidth()
{
        return gWidth;
}

unsigned int sgGetDataHeight()
{
        return gHeight;
}

unsigned int sgGetDataChannels()
{
        return gChannels;
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

