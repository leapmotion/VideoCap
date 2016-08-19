#include "stdafx.h"
#include "FormatCorrector.h"
#include "../baseclasses/fourcc.h"

// {82CDE25B-4955-459B-A29E-2D366667C5E1}
static const GUID IID_GrayscaleFormatCorrector = { 0x82cde25b, 0x4955, 0x459b,{ 0xa2, 0x9e, 0x2d, 0x36, 0x66, 0x67, 0xc5, 0xe1 } };

FormatCorrector::FormatCorrector():
	CTransformFilter("Leap Motion grayscale conversion filter", nullptr, IID_GrayscaleFormatCorrector)
{
}

FormatCorrector::~FormatCorrector()
{
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
	width = pbmi->biWidth;
	height = pbmi->biHeight;
	channels = pbmi->biBitCount / 8;

	ppropInputRequest->cbBuffer = DIBSIZE(*pbmi);
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
	return S_OK;
}

HRESULT FormatCorrector::Transform(IMediaSample *pIn, IMediaSample *pOut) {
	{
		std::lock_guard<std::mutex> lk{ m_lock };
		m_lastInput = pIn;
		m_lastOutput = pOut;
	}

	if(!grayscale_conversion) {
		// Trivial copy from the input to the output buffers
		BYTE* pOutBuf;
		BYTE* pInBuf;

		pIn->GetPointer(&pInBuf);
		pOut->GetPointer(&pOutBuf);

		long size = pOut->GetSize();
		memcpy(pOutBuf, pInBuf, size);
		pOut->SetActualDataLength(size);
		pOut->SetSyncPoint(true);

		// No work to be done, we can short-circuit here
		return S_OK;
	}

	;
	return S_OK;
}
