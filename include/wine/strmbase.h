/*
 * Header file for Wine's strmbase implementation
 *
 * Copyright 2003 Robert Shearman
 * Copyright 2010 Aric Stewart, CodeWeavers
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

HRESULT WINAPI CopyMediaType(AM_MEDIA_TYPE * pDest, const AM_MEDIA_TYPE *pSrc);
void WINAPI FreeMediaType(AM_MEDIA_TYPE * pMediaType);
AM_MEDIA_TYPE * WINAPI CreateMediaType(AM_MEDIA_TYPE const * pSrc);
void WINAPI DeleteMediaType(AM_MEDIA_TYPE * pMediaType);

typedef HRESULT (WINAPI *BasePin_GetMediaType)(IPin* iface, int iPosition, AM_MEDIA_TYPE *amt);
typedef LONG (WINAPI *BasePin_GetMediaTypeVersion)(IPin* iface);
typedef HRESULT (WINAPI *BasePin_AttemptConnection)(IPin *iface, IPin *pReceivePin, const AM_MEDIA_TYPE *pmt);
typedef HRESULT (WINAPI *BasePin_CheckMediaType)(IPin *iface, const AM_MEDIA_TYPE *pmt);

typedef IPin* (WINAPI *BaseFilter_GetPin)(IBaseFilter* iface, int iPosition);
typedef LONG (WINAPI *BaseFilter_GetPinCount)(IBaseFilter* iface);
typedef LONG (WINAPI *BaseFilter_GetPinVersion)(IBaseFilter* iface);

typedef HRESULT (WINAPI *BaseInputPin_Receive)(IPin *This, IMediaSample *pSample);

HRESULT WINAPI EnumMediaTypes_Construct(IPin *iface, BasePin_GetMediaType enumFunc, BasePin_GetMediaTypeVersion versionFunc, IEnumMediaTypes ** ppEnum);

HRESULT WINAPI EnumPins_Construct(IBaseFilter *base,  BaseFilter_GetPin receive_pin, BaseFilter_GetPinCount receive_pincount, BaseFilter_GetPinVersion receive_version, IEnumPins ** ppEnum);

/* Pin functions */

typedef struct BasePin
{
	const struct IPinVtbl * lpVtbl;
	LONG refCount;
	LPCRITICAL_SECTION pCritSec;
	PIN_INFO pinInfo;
	IPin * pConnectedTo;
	AM_MEDIA_TYPE mtCurrent;
} BasePin;

typedef struct BaseOutputPin
{
	/* inheritance C style! */
	BasePin pin;

	IMemInputPin * pMemInputPin;
	BasePin_AttemptConnection pAttemptConnection;
	BOOL custom_allocator;
	IMemAllocator *alloc;
	BOOL readonly;
	ALLOCATOR_PROPERTIES allocProps;
} BaseOutputPin;

typedef struct BaseInputPin
{
	/* inheritance C style! */
	BasePin pin;

	const IMemInputPinVtbl * lpVtblMemInput;
	IMemAllocator * pAllocator;
	BaseInputPin_Receive fnReceive;
	BasePin_CheckMediaType fnCheckMediaType;
	REFERENCE_TIME tStart;
	REFERENCE_TIME tStop;
	double dRate;
	BOOL flushing, end_of_stream;
	IMemAllocator *preferred_allocator;
} BaseInputPin;

/* Base Pin */
HRESULT WINAPI BasePinImpl_GetMediaType(IPin *iface, int iPosition, AM_MEDIA_TYPE *pmt);
LONG WINAPI BasePinImpl_GetMediaTypeVersion(IPin *iface);
ULONG WINAPI BasePinImpl_AddRef(IPin * iface);
HRESULT WINAPI BasePinImpl_Disconnect(IPin * iface);
HRESULT WINAPI BasePinImpl_ConnectedTo(IPin * iface, IPin ** ppPin);
HRESULT WINAPI BasePinImpl_ConnectionMediaType(IPin * iface, AM_MEDIA_TYPE * pmt);
HRESULT WINAPI BasePinImpl_QueryPinInfo(IPin * iface, PIN_INFO * pInfo);
HRESULT WINAPI BasePinImpl_QueryDirection(IPin * iface, PIN_DIRECTION * pPinDir);
HRESULT WINAPI BasePinImpl_QueryId(IPin * iface, LPWSTR * Id);
HRESULT WINAPI BasePinImpl_QueryAccept(IPin * iface, const AM_MEDIA_TYPE * pmt);
HRESULT WINAPI BasePinImpl_EnumMediaTypes(IPin * iface, IEnumMediaTypes ** ppEnum);
HRESULT WINAPI BasePinImpl_QueryInternalConnections(IPin * iface, IPin ** apPin, ULONG * cPin);

/* Base Output Pin */
HRESULT WINAPI BaseOutputPinImpl_QueryInterface(IPin * iface, REFIID riid, LPVOID * ppv);
ULONG WINAPI BaseOutputPinImpl_Release(IPin * iface);
HRESULT WINAPI BaseOutputPinImpl_Connect(IPin * iface, IPin * pReceivePin, const AM_MEDIA_TYPE * pmt);
HRESULT WINAPI BaseOutputPinImpl_ReceiveConnection(IPin * iface, IPin * pReceivePin, const AM_MEDIA_TYPE * pmt);
HRESULT WINAPI BaseOutputPinImpl_Disconnect(IPin * iface);
HRESULT WINAPI BaseOutputPinImpl_EndOfStream(IPin * iface);
HRESULT WINAPI BaseOutputPinImpl_BeginFlush(IPin * iface);
HRESULT WINAPI BaseOutputPinImpl_EndFlush(IPin * iface);
HRESULT WINAPI BaseOutputPinImpl_NewSegment(IPin * iface, REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

HRESULT WINAPI BaseOutputPinImpl_GetDeliveryBuffer(BaseOutputPin * This, IMediaSample ** ppSample, REFERENCE_TIME * tStart, REFERENCE_TIME * tStop, DWORD dwFlags);
HRESULT WINAPI BaseOutputPinImpl_Deliver(BaseOutputPin * This, IMediaSample * pSample);
HRESULT WINAPI BaseOutputPinImpl_BreakConnect(BaseOutputPin * This);
HRESULT WINAPI BaseOutputPinImpl_Active(BaseOutputPin * This);
HRESULT WINAPI BaseOutputPinImpl_Inactive(BaseOutputPin * This);
HRESULT WINAPI BaseOutputPin_Construct(const IPinVtbl *OutputPin_Vtbl, LONG outputpin_size, const PIN_INFO * pPinInfo, ALLOCATOR_PROPERTIES *props, BasePin_AttemptConnection pConnectProc, LPCRITICAL_SECTION pCritSec, IPin ** ppPin);

/* Base Input Pin */
HRESULT WINAPI BaseInputPinImpl_QueryInterface(IPin * iface, REFIID riid, LPVOID * ppv);
ULONG   WINAPI BaseInputPinImpl_Release(IPin * iface);
HRESULT WINAPI BaseInputPinImpl_Connect(IPin * iface, IPin * pConnector, const AM_MEDIA_TYPE * pmt);
HRESULT WINAPI BaseInputPinImpl_ReceiveConnection(IPin * iface, IPin * pReceivePin, const AM_MEDIA_TYPE * pmt);
HRESULT WINAPI BaseInputPinImpl_QueryAccept(IPin * iface, const AM_MEDIA_TYPE * pmt);
HRESULT WINAPI BaseInputPinImpl_EndOfStream(IPin * iface);
HRESULT WINAPI BaseInputPinImpl_BeginFlush(IPin * iface);
HRESULT WINAPI BaseInputPinImpl_EndFlush(IPin * iface);
HRESULT WINAPI BaseInputPinImpl_NewSegment(IPin * iface, REFERENCE_TIME tStart, REFERENCE_TIME tStop, double dRate);

HRESULT BaseInputPin_Construct(const IPinVtbl *InputPin_Vtbl, const PIN_INFO * pPinInfo, BasePin_CheckMediaType pQueryAccept, BaseInputPin_Receive pSampleProc,  LPCRITICAL_SECTION pCritSec, IMemAllocator *, IPin ** ppPin);