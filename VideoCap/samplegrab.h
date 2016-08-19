
#ifndef SAMPLEGRAB_h
#define SAMPLEGRAB_h

#define SAFE_RELEASE(x) { if (x) x->Release(); x = NULL; }

class FormatCorrector;

extern CComPtr<IBaseFilter> g_pGrabberFilter;
extern CComPtr<FormatCorrector> g_pFormatCorrector;

HRESULT sgAddSampleGrabber(IGraphBuilder *pGraph, CComPtr<FormatCorrector>& pFormatCorrector);

unsigned char* sgGrabData();            //call grab data first
Gdiplus::Bitmap* sgGetBitmap();        //fill bitmap with grabbed data
long sgGetBufferSize();

void sgCloseSampleGrabber();

#endif
