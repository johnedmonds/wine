/*
 * Copyright 2010 Jacek Caban for CodeWeavers
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

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "winreg.h"
#include "ole2.h"

#include "wine/debug.h"

#include "mshtml_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

typedef struct {
    HTMLElement element;

    IHTMLObjectElement IHTMLObjectElement_iface;

    nsIDOMHTMLObjectElement *nsobject;
} HTMLObjectElement;

static inline HTMLObjectElement *impl_from_IHTMLObjectElement(IHTMLObjectElement *iface)
{
    return CONTAINING_RECORD(iface, HTMLObjectElement, IHTMLObjectElement_iface);
}

static HRESULT WINAPI HTMLObjectElement_QueryInterface(IHTMLObjectElement *iface,
        REFIID riid, void **ppv)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);

    return IHTMLDOMNode_QueryInterface(HTMLDOMNODE(&This->element.node), riid, ppv);
}

static ULONG WINAPI HTMLObjectElement_AddRef(IHTMLObjectElement *iface)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);

    return IHTMLDOMNode_AddRef(HTMLDOMNODE(&This->element.node));
}

static ULONG WINAPI HTMLObjectElement_Release(IHTMLObjectElement *iface)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);

    return IHTMLDOMNode_Release(HTMLDOMNODE(&This->element.node));
}

static HRESULT WINAPI HTMLObjectElement_GetTypeInfoCount(IHTMLObjectElement *iface, UINT *pctinfo)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    return IDispatchEx_GetTypeInfoCount(DISPATCHEX(&This->element.node.dispex), pctinfo);
}

static HRESULT WINAPI HTMLObjectElement_GetTypeInfo(IHTMLObjectElement *iface, UINT iTInfo,
                                              LCID lcid, ITypeInfo **ppTInfo)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    return IDispatchEx_GetTypeInfo(DISPATCHEX(&This->element.node.dispex), iTInfo, lcid, ppTInfo);
}

static HRESULT WINAPI HTMLObjectElement_GetIDsOfNames(IHTMLObjectElement *iface, REFIID riid,
                                                LPOLESTR *rgszNames, UINT cNames,
                                                LCID lcid, DISPID *rgDispId)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    return IDispatchEx_GetIDsOfNames(DISPATCHEX(&This->element.node.dispex), riid, rgszNames, cNames, lcid, rgDispId);
}

static HRESULT WINAPI HTMLObjectElement_Invoke(IHTMLObjectElement *iface, DISPID dispIdMember,
                            REFIID riid, LCID lcid, WORD wFlags, DISPPARAMS *pDispParams,
                            VARIANT *pVarResult, EXCEPINFO *pExcepInfo, UINT *puArgErr)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    return IDispatchEx_Invoke(DISPATCHEX(&This->element.node.dispex), dispIdMember, riid, lcid, wFlags, pDispParams,
            pVarResult, pExcepInfo, puArgErr);
}

static HRESULT WINAPI HTMLObjectElement_get_object(IHTMLObjectElement *iface, IDispatch **p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_classid(IHTMLObjectElement *iface, BSTR *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_data(IHTMLObjectElement *iface, BSTR *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_put_recordset(IHTMLObjectElement *iface, IDispatch *v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, v);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_recordset(IHTMLObjectElement *iface, IDispatch **p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_put_align(IHTMLObjectElement *iface, BSTR v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_align(IHTMLObjectElement *iface, BSTR *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_put_name(IHTMLObjectElement *iface, BSTR v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_name(IHTMLObjectElement *iface, BSTR *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_put_codeBase(IHTMLObjectElement *iface, BSTR v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_codeBase(IHTMLObjectElement *iface, BSTR *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_put_codeType(IHTMLObjectElement *iface, BSTR v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_codeType(IHTMLObjectElement *iface, BSTR *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_put_code(IHTMLObjectElement *iface, BSTR v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_code(IHTMLObjectElement *iface, BSTR *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_BaseHref(IHTMLObjectElement *iface, BSTR *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_put_type(IHTMLObjectElement *iface, BSTR v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_type(IHTMLObjectElement *iface, BSTR *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_form(IHTMLObjectElement *iface, IHTMLFormElement **p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_put_width(IHTMLObjectElement *iface, VARIANT v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_width(IHTMLObjectElement *iface, VARIANT *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_put_height(IHTMLObjectElement *iface, VARIANT v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_height(IHTMLObjectElement *iface, VARIANT *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_readyState(IHTMLObjectElement *iface, LONG *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_put_onreadystatechange(IHTMLObjectElement *iface, VARIANT v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_onreadystatechange(IHTMLObjectElement *iface, VARIANT *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_put_onerror(IHTMLObjectElement *iface, VARIANT v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_variant(&v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_onerror(IHTMLObjectElement *iface, VARIANT *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_put_altHtml(IHTMLObjectElement *iface, BSTR v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%s)\n", This, debugstr_w(v));
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_altHtml(IHTMLObjectElement *iface, BSTR *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_put_vspace(IHTMLObjectElement *iface, LONG v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%d)\n", This, v);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_vspace(IHTMLObjectElement *iface, LONG *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    PRInt32 vspace;
    nsresult nsres;

    TRACE("(%p)->(%p)\n", This, p);

    nsres = nsIDOMHTMLObjectElement_GetVspace(This->nsobject, &vspace);
    if(NS_FAILED(nsres)) {
        ERR("GetVspace failed: %08x\n", nsres);
        return E_FAIL;
    }

    *p = vspace;
    return S_OK;
}

static HRESULT WINAPI HTMLObjectElement_put_hspace(IHTMLObjectElement *iface, LONG v)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%d)\n", This, v);
    return E_NOTIMPL;
}

static HRESULT WINAPI HTMLObjectElement_get_hspace(IHTMLObjectElement *iface, LONG *p)
{
    HTMLObjectElement *This = impl_from_IHTMLObjectElement(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

static const IHTMLObjectElementVtbl HTMLObjectElementVtbl = {
    HTMLObjectElement_QueryInterface,
    HTMLObjectElement_AddRef,
    HTMLObjectElement_Release,
    HTMLObjectElement_GetTypeInfoCount,
    HTMLObjectElement_GetTypeInfo,
    HTMLObjectElement_GetIDsOfNames,
    HTMLObjectElement_Invoke,
    HTMLObjectElement_get_object,
    HTMLObjectElement_get_classid,
    HTMLObjectElement_get_data,
    HTMLObjectElement_put_recordset,
    HTMLObjectElement_get_recordset,
    HTMLObjectElement_put_align,
    HTMLObjectElement_get_align,
    HTMLObjectElement_put_name,
    HTMLObjectElement_get_name,
    HTMLObjectElement_put_codeBase,
    HTMLObjectElement_get_codeBase,
    HTMLObjectElement_put_codeType,
    HTMLObjectElement_get_codeType,
    HTMLObjectElement_put_code,
    HTMLObjectElement_get_code,
    HTMLObjectElement_get_BaseHref,
    HTMLObjectElement_put_type,
    HTMLObjectElement_get_type,
    HTMLObjectElement_get_form,
    HTMLObjectElement_put_width,
    HTMLObjectElement_get_width,
    HTMLObjectElement_put_height,
    HTMLObjectElement_get_height,
    HTMLObjectElement_get_readyState,
    HTMLObjectElement_put_onreadystatechange,
    HTMLObjectElement_get_onreadystatechange,
    HTMLObjectElement_put_onerror,
    HTMLObjectElement_get_onerror,
    HTMLObjectElement_put_altHtml,
    HTMLObjectElement_get_altHtml,
    HTMLObjectElement_put_vspace,
    HTMLObjectElement_get_vspace,
    HTMLObjectElement_put_hspace,
    HTMLObjectElement_get_hspace
};

#define HTMLOBJECT_NODE_THIS(iface) DEFINE_THIS2(HTMLObjectElement, element.node, iface)

static HRESULT HTMLObjectElement_QI(HTMLDOMNode *iface, REFIID riid, void **ppv)
{
    HTMLObjectElement *This = HTMLOBJECT_NODE_THIS(iface);

    if(IsEqualGUID(&IID_IUnknown, riid)) {
        TRACE("(%p)->(IID_IUnknown %p)\n", This, ppv);
        *ppv = &This->IHTMLObjectElement_iface;
    }else if(IsEqualGUID(&IID_IDispatch, riid)) {
        TRACE("(%p)->(IID_IDispatch %p)\n", This, ppv);
        *ppv = &This->IHTMLObjectElement_iface;
    }else if(IsEqualGUID(&IID_IHTMLObjectElement, riid)) {
        TRACE("(%p)->(IID_IHTMLObjectElement %p)\n", This, ppv);
        *ppv = &This->IHTMLObjectElement_iface;
    }else {
        return HTMLElement_QI(&This->element.node, riid, ppv);
    }

    IUnknown_AddRef((IUnknown*)*ppv);
    return S_OK;
}

static void HTMLObjectElement_destructor(HTMLDOMNode *iface)
{
    HTMLObjectElement *This = HTMLOBJECT_NODE_THIS(iface);

    if(This->nsobject)
        nsIDOMHTMLObjectElement_Release(This->nsobject);

    HTMLElement_destructor(&This->element.node);
}

static HRESULT HTMLObjectElement_get_readystate(HTMLDOMNode *iface, BSTR *p)
{
    HTMLObjectElement *This = HTMLOBJECT_NODE_THIS(iface);
    FIXME("(%p)->(%p)\n", This, p);
    return E_NOTIMPL;
}

#undef HTMLOBJECT_NODE_THIS

static const NodeImplVtbl HTMLObjectElementImplVtbl = {
    HTMLObjectElement_QI,
    HTMLObjectElement_destructor,
    HTMLElement_clone,
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    HTMLObjectElement_get_readystate
};

static const tid_t HTMLObjectElement_iface_tids[] = {
    HTMLELEMENT_TIDS,
    IHTMLObjectElement_tid,
    0
};
static dispex_static_data_t HTMLObjectElement_dispex = {
    NULL,
    DispHTMLObjectElement_tid,
    NULL,
    HTMLObjectElement_iface_tids
};

HRESULT HTMLObjectElement_Create(HTMLDocumentNode *doc, nsIDOMHTMLElement *nselem, HTMLElement **elem)
{
    HTMLObjectElement *ret;
    nsresult nsres;

    ret = heap_alloc_zero(sizeof(*ret));
    if(!ret)
        return E_OUTOFMEMORY;

    ret->IHTMLObjectElement_iface.lpVtbl = &HTMLObjectElementVtbl;
    ret->element.node.vtbl = &HTMLObjectElementImplVtbl;

    nsres = nsIDOMHTMLElement_QueryInterface(nselem, &IID_nsIDOMHTMLObjectElement, (void**)&ret->nsobject);
    if(NS_FAILED(nsres)) {
        ERR("Could not get nsIDOMHTMLObjectElement iface: %08x\n", nsres);
        heap_free(ret);
        return E_FAIL;
    }

    HTMLElement_Init(&ret->element, doc, nselem, &HTMLObjectElement_dispex);

    *elem = &ret->element;
    return S_OK;
}