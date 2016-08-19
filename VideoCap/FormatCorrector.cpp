#include "stdafx.h"
#include "FormatCorrector.h"
#include "../baseclasses/fourcc.h"
#include <algorithm>

#undef min
#undef max

// {82CDE25B-4955-459B-A29E-2D366667C5E1}
static const GUID IID_GrayscaleFormatCorrector = { 0x82cde25b, 0x4955, 0x459b,{ 0xa2, 0x9e, 0x2d, 0x36, 0x66, 0x67, 0xc5, 0xe1 } };

FormatCorrector::FormatCorrector():
	CTransformFilter("Leap Motion grayscale conversion filter", NULL, IID_GrayscaleFormatCorrector)
{
	InitializeCriticalSection(&m_lock);
}

FormatCorrector::~FormatCorrector()
{
	DeleteCriticalSection(&m_lock);
}

HRESULT FormatCorrector::CheckInputType(const CMediaType *mtIn) {
	// We only convert the expected YUYV format to grayscale, nothing else
	return S_OK;
}

HRESULT FormatCorrector::CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut) {
	return S_OK;
}

HRESULT FormatCorrector::DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *ppropInputRequest) {
	// Need two buffers so  we can cache one
	if(ppropInputRequest->cBuffers < 2)
		ppropInputRequest->cBuffers = 2;

	AM_MEDIA_TYPE mediaType = {};
	HRESULT hr = m_pInput->ConnectionMediaType(&mediaType);
	if (FAILED(hr))
		return hr;

	// For a stereo image, the output width is twice the input width
	BITMAPINFOHEADER* pbmi = HEADER(mediaType.pbFormat);
	width = pbmi->biWidth * 2;
	height = pbmi->biHeight;
	channels = pbmi->biBitCount / 8;

	ppropInputRequest->cbBuffer = width * height * 3;
	FreeMediaType(mediaType);

	ALLOCATOR_PROPERTIES actual;
	hr = pAlloc->SetProperties(ppropInputRequest, &actual);
	if (FAILED(hr))
		return hr;

	if (ppropInputRequest->cbBuffer > actual.cbBuffer)
		return E_FAIL;
	return S_OK;
}

HRESULT FormatCorrector::GetMediaType(int  iPosition, CMediaType *pMediaType) {
	if (!m_pInput->IsConnected())
		return VFW_E_NOT_CONNECTED;
	if (iPosition < 0)
		return E_INVALIDARG;
	if (iPosition != 0)
		return VFW_S_NO_MORE_ITEMS;

	HRESULT hr = m_pInput->ConnectionMediaType(pMediaType);
	if (FAILED(hr))
		return hr;

	pMediaType->subtype = MEDIASUBTYPE_RGB24;
	pMediaType->SetTemporalCompression(false);

	VIDEOINFOHEADER* pVih = reinterpret_cast<VIDEOINFOHEADER*>(pMediaType->pbFormat);
	BITMAPINFOHEADER& bmiHeader = pVih->bmiHeader;
	bmiHeader.biWidth *= 2; // stereo image, twice the original nominal width
	bmiHeader.biBitCount = 24;
	bmiHeader.biCompression = BI_RGB;
	bmiHeader.biSizeImage = width * height * 3;
	return S_OK;
}

HRESULT FormatCorrector::Transform(IMediaSample *pIn, IMediaSample *pOut) {
	// Expanding copy from the input to the output buffers
	BYTE* pOutBuf;
	BYTE* pInBuf;

	pIn->GetPointer(&pInBuf);
	pOut->GetPointer(&pOutBuf);

	long inSize = pIn->GetSize();
	long outSize = pOut->GetSize();

	if (outSize < inSize * 3)
		return E_FAIL;

	for (long i = 0; i < inSize; i++) {
		pOutBuf[i * 3 + 0] = pInBuf[i];
		pOutBuf[i * 3 + 1] = pInBuf[i];
		pOutBuf[i * 3 + 2] = pInBuf[i];
	}

	pOut->SetActualDataLength(inSize * 3);
	pOut->SetSyncPoint(true);

	{
		EnterCriticalSection(&m_lock);
		m_lastInput = pIn;
		m_lastOutput = pOut;
		LeaveCriticalSection(&m_lock);
	}

	// No work to be done, we can short-circuit here
	return S_OK;
}
