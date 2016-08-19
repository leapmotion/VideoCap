#pragma once
#include "../baseclasses/transfrm.h"
#include <mutex>

extern const GUID IID_GrayscaleFormatCorrector;

class FormatCorrector:
	public CTransformFilter
{
public:
	FormatCorrector(void);
	~FormatCorrector(void);

	// Holds true if a grayscale conversion is necesary.  Otherwise this filter does no work.
	bool grayscale_conversion = false;

private:
	// Holds a reference to the last input media sample
	mutable std::mutex m_lock;
	CComPtr<IMediaSample> m_lastInput;
	CComPtr<IMediaSample> m_lastOutput;

	// Last configured width and height
	ULONG width = 0;
	ULONG height = 0;
	ULONG channels = 0;

public:
	ULONG GetConnectedWidth(void) const { return width; }
	ULONG GetConnectedHeight(void) const { return height; }
	ULONG GetInputChannels(void) const { return channels; }

	CComPtr<IMediaSample> GetLastSample(void) const {
		std::lock_guard<std::mutex> lk(m_lock);
		return m_lastInput;
	}

	HRESULT CheckInputType(const CMediaType *mtIn);
	HRESULT CheckTransform(const CMediaType *mtIn, const CMediaType *mtOut);
	HRESULT DecideBufferSize(IMemAllocator *pAlloc, ALLOCATOR_PROPERTIES *ppropInputRequest);
	HRESULT GetMediaType(int  iPosition, CMediaType *pMediaType);
	HRESULT Transform(IMediaSample *pIn, IMediaSample *pOut);
};

