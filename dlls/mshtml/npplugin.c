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

#include "config.h"

#include <stdarg.h>

#define COBJMACROS

#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"
#include "shlobj.h"

#include "mshtml_private.h"
#include "pluginhost.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(mshtml);

/* Parts of npapi.h */

#define NPERR_BASE                         0
#define NPERR_NO_ERROR                    (NPERR_BASE + 0)
#define NPERR_GENERIC_ERROR               (NPERR_BASE + 1)
#define NPERR_INVALID_INSTANCE_ERROR      (NPERR_BASE + 2)
#define NPERR_INVALID_FUNCTABLE_ERROR     (NPERR_BASE + 3)
#define NPERR_MODULE_LOAD_FAILED_ERROR    (NPERR_BASE + 4)
#define NPERR_OUT_OF_MEMORY_ERROR         (NPERR_BASE + 5)
#define NPERR_INVALID_PLUGIN_ERROR        (NPERR_BASE + 6)
#define NPERR_INVALID_PLUGIN_DIR_ERROR    (NPERR_BASE + 7)
#define NPERR_INCOMPATIBLE_VERSION_ERROR  (NPERR_BASE + 8)
#define NPERR_INVALID_PARAM               (NPERR_BASE + 9)
#define NPERR_INVALID_URL                 (NPERR_BASE + 10)
#define NPERR_FILE_NOT_FOUND              (NPERR_BASE + 11)
#define NPERR_NO_DATA                     (NPERR_BASE + 12)
#define NPERR_STREAM_NOT_SEEKABLE         (NPERR_BASE + 13)

/* Parts of npfunctions.h */

typedef NPError (CDECL *NPP_NewProcPtr)(NPMIMEType,NPP,UINT16,INT16,char**,char**,NPSavedData*);
typedef NPError (CDECL *NPP_DestroyProcPtr)(NPP,NPSavedData**);
typedef NPError (CDECL *NPP_SetWindowProcPtr)(NPP,NPWindow*);
typedef NPError (CDECL *NPP_NewStreamProcPtr)(NPP,NPMIMEType,NPStream*,NPBool,UINT16*);
typedef NPError (CDECL *NPP_DestroyStreamProcPtr)(NPP,NPStream*,NPReason);
typedef INT32 (CDECL *NPP_WriteReadyProcPtr)(NPP,NPStream*);
typedef INT32 (CDECL *NPP_WriteProcPtr)(NPP,NPStream*,INT32,INT32,void*);
typedef void (CDECL *NPP_StreamAsFileProcPtr)(NPP,NPStream*,const char*);
typedef void (CDECL *NPP_PrintProcPtr)(NPP,NPPrint*);
typedef INT16 (CDECL *NPP_HandleEventProcPtr)(NPP,void*);
typedef void (CDECL *NPP_URLNotifyProcPtr)(NPP,const char*,NPReason,void*);
typedef NPError (CDECL *NPP_GetValueProcPtr)(NPP,NPPVariable,void*);
typedef NPError (CDECL *NPP_SetValueProcPtr)(NPP,NPNVariable,void*);
typedef NPBool (CDECL *NPP_GotFocusPtr)(NPP,NPFocusDirection);
typedef void (CDECL *NPP_LostFocusPtr)(NPP);

typedef struct _NPPluginFuncs {
    UINT16 size;
    UINT16 version;
    NPP_NewProcPtr newp;
    NPP_DestroyProcPtr destroy;
    NPP_SetWindowProcPtr setwindow;
    NPP_NewStreamProcPtr newstream;
    NPP_DestroyStreamProcPtr destroystream;
    NPP_StreamAsFileProcPtr asfile;
    NPP_WriteReadyProcPtr writeready;
    NPP_WriteProcPtr write;
    NPP_PrintProcPtr print;
    NPP_HandleEventProcPtr event;
    NPP_URLNotifyProcPtr urlnotify;
    void *javaClass;
    NPP_GetValueProcPtr getvalue;
    NPP_SetValueProcPtr setvalue;
    NPP_GotFocusPtr gotfocus;
    NPP_LostFocusPtr lostfocus;
} NPPluginFuncs;

static nsIDOMElement *get_dom_element(NPP instance)
{
    nsISupports *instance_unk = (nsISupports*)instance->ndata;
    nsIPluginInstance *plugin_instance;
    nsIPluginInstanceOwner *owner;
    nsIPluginTagInfo *tag_info;
    nsIDOMElement *elem;
    nsresult nsres;

    nsres = nsISupports_QueryInterface(instance_unk, &IID_nsIPluginInstance, (void**)&plugin_instance);
    if(NS_FAILED(nsres))
        return NULL;

    nsres = nsIPluginInstance_GetOwner(plugin_instance, &owner);
    nsIPluginInstance_Release(instance_unk);
    if(NS_FAILED(nsres) || !owner)
        return NULL;

    nsres = nsISupports_QueryInterface(owner, &IID_nsIPluginTagInfo, (void**)&tag_info);
    nsISupports_Release(owner);
    if(NS_FAILED(nsres))
        return NULL;

    nsres = nsIPluginTagInfo_GetDOMElement(tag_info, &elem);
    nsIPluginTagInfo_Release(tag_info);
    if(NS_FAILED(nsres))
        return NULL;

    return elem;
}

static HTMLWindow *get_elem_window(nsIDOMElement *elem)
{
    nsIDOMWindow *nswindow;
    nsIDOMDocument *nsdoc;
    HTMLWindow *window;
    nsresult nsres;

    nsres = nsIDOMElement_GetOwnerDocument(elem, &nsdoc);
    if(NS_FAILED(nsres))
        return NULL;

    nswindow = get_nsdoc_window(nsdoc);
    nsIDOMDocument_Release(nsdoc);
    if(!nswindow)
        return NULL;

    window = nswindow_to_window(nswindow);
    nsIDOMWindow_Release(nswindow);

    return window;
}

static BOOL parse_classid(const PRUnichar *classid, CLSID *clsid)
{
    const WCHAR *ptr;
    unsigned len;
    HRESULT hres;

    static const PRUnichar clsidW[] = {'c','l','s','i','d',':'};

    if(strncmpiW(classid, clsidW, sizeof(clsidW)/sizeof(WCHAR)))
        return FALSE;

    ptr = classid + sizeof(clsidW)/sizeof(WCHAR);
    len = strlenW(ptr);

    if(len == 38) {
        hres = CLSIDFromString(ptr, clsid);
    }else if(len == 36) {
        WCHAR buf[39];

        buf[0] = '{';
        memcpy(buf+1, ptr, len*sizeof(WCHAR));
        buf[37] = '}';
        buf[38] = 0;
        hres = CLSIDFromString(buf, clsid);
    }else {
        return FALSE;
    }

    return SUCCEEDED(hres);
}

static BOOL get_elem_clsid(nsIDOMElement *elem, CLSID *clsid)
{
    nsAString attr_str, val_str;
    nsresult nsres;
    BOOL ret = FALSE;

    static const PRUnichar classidW[] = {'c','l','a','s','s','i','d',0};

    nsAString_InitDepend(&attr_str, classidW);
    nsAString_Init(&val_str, NULL);
    nsres = nsIDOMElement_GetAttribute(elem, &attr_str, &val_str);
    nsAString_Finish(&attr_str);
    if(NS_SUCCEEDED(nsres)) {
        const PRUnichar *val;

        nsAString_GetData(&val_str, &val);
        if(*val)
            ret = parse_classid(val, clsid);
    }else {
        ERR("GetAttribute failed: %08x\n", nsres);
    }

    nsAString_Finish(&attr_str);
    return ret;
}

static IUnknown *create_activex_object(HTMLWindow *window, nsIDOMElement *nselem, CLSID *clsid)
{
    IClassFactoryEx *cfex;
    IClassFactory *cf;
    IUnknown *obj;
    DWORD policy;
    HRESULT hres;

    if(!get_elem_clsid(nselem, clsid)) {
        WARN("Could not determine element CLSID\n");
        return NULL;
    }

    TRACE("clsid %s\n", debugstr_guid(clsid));

    policy = 0;
    hres = IInternetHostSecurityManager_ProcessUrlAction(&window->doc->IInternetHostSecurityManager_iface,
            URLACTION_ACTIVEX_RUN, (BYTE*)&policy, sizeof(policy), (BYTE*)clsid, sizeof(GUID), 0, 0);
    if(FAILED(hres) || policy != URLPOLICY_ALLOW) {
        WARN("ProcessUrlAction returned %08x %x\n", hres, policy);
        return NULL;
    }

    hres = CoGetClassObject(clsid, CLSCTX_INPROC_SERVER|CLSCTX_LOCAL_SERVER, NULL, &IID_IClassFactory, (void**)&cf);
    if(FAILED(hres))
        return NULL;

    hres = IClassFactory_QueryInterface(cf, &IID_IClassFactoryEx, (void**)&cfex);
    if(SUCCEEDED(hres)) {
        FIXME("Use IClassFactoryEx\n");
        IClassFactoryEx_Release(cfex);
    }

    hres = IClassFactory_CreateInstance(cf, NULL, &IID_IUnknown, (void**)&obj);
    if(FAILED(hres))
        return NULL;

    return obj;
}

static NPError CDECL NPP_New(NPMIMEType pluginType, NPP instance, UINT16 mode, INT16 argc, char **argn,
        char **argv, NPSavedData *saved)
{
    nsIDOMElement *nselem;
    HTMLWindow *window;
    IUnknown *obj;
    CLSID clsid;
    NPError err = NPERR_NO_ERROR;

    TRACE("(%s %p %x %d %p %p %p)\n", debugstr_a(pluginType), instance, mode, argc, argn, argv, saved);

    nselem = get_dom_element(instance);
    if(!nselem) {
        ERR("Could not get DOM element\n");
        return NPERR_GENERIC_ERROR;
    }

    window = get_elem_window(nselem);
    if(!window) {
        ERR("Could not get element's window object\n");
        nsIDOMElement_Release(nselem);
        return NPERR_GENERIC_ERROR;
    }

    obj = create_activex_object(window, nselem, &clsid);
    if(obj) {
        PluginHost *host;
        HRESULT hres;

        hres = create_plugin_host(window->doc, nselem, obj, &clsid, &host);
        nsIDOMElement_Release(nselem);
        IUnknown_Release(obj);
        if(SUCCEEDED(hres))
            instance->pdata = host;
        else
            err = NPERR_GENERIC_ERROR;
    }else {
        err = NPERR_GENERIC_ERROR;
    }

    nsIDOMElement_Release(nselem);
    return err;
}

static NPError CDECL NPP_Destroy(NPP instance, NPSavedData **save)
{
    PluginHost *host = instance->pdata;

    TRACE("(%p %p)\n", instance, save);

    if(!host)
        return NPERR_GENERIC_ERROR;

    detach_plugin_host(host);
    IOleClientSite_Release(&host->IOleClientSite_iface);
    instance->pdata = NULL;
    return NPERR_NO_ERROR;
}

static NPError CDECL NPP_SetWindow(NPP instance, NPWindow *window)
{
    PluginHost *host = instance->pdata;
    RECT pos_rect = {0, 0, window->width, window->height};

    TRACE("(%p %p)\n", instance, window);

    if(!host)
        return NPERR_GENERIC_ERROR;

    update_plugin_window(host, window->window, &pos_rect);
    return NPERR_NO_ERROR;
}

static NPError CDECL NPP_NewStream(NPP instance, NPMIMEType type, NPStream *stream, NPBool seekable, UINT16 *stype)
{
    TRACE("\n");
    return NPERR_GENERIC_ERROR;
}

static NPError CDECL NPP_DestroyStream(NPP instance, NPStream *stream, NPReason reason)
{
    TRACE("\n");
    return NPERR_GENERIC_ERROR;
}

static INT32 CDECL NPP_WriteReady(NPP instance, NPStream *stream)
{
    TRACE("\n");
    return NPERR_GENERIC_ERROR;
}

static INT32 CDECL NPP_Write(NPP instance, NPStream *stream, INT32 offset, INT32 len, void *buffer)
{
    TRACE("\n");
    return NPERR_GENERIC_ERROR;
}

static void CDECL NPP_StreamAsFile(NPP instance, NPStream *stream, const char *fname)
{
    TRACE("\n");
}

static void CDECL NPP_Print(NPP instance, NPPrint *platformPrint)
{
    FIXME("\n");
}

static INT16 CDECL NPP_HandleEvent(NPP instance, void *event)
{
    TRACE("\n");
    return NPERR_GENERIC_ERROR;
}

static void CDECL NPP_URLNotify(NPP instance, const char *url, NPReason reason, void *notifyData)
{
    TRACE("\n");
}

static NPError CDECL NPP_GetValue(NPP instance, NPPVariable variable, void *ret_value)
{
    TRACE("\n");
    return NPERR_GENERIC_ERROR;
}

static NPError CDECL NPP_SetValue(NPP instance, NPNVariable variable, void *value)
{
    TRACE("\n");
    return NPERR_GENERIC_ERROR;
}

static NPBool CDECL NPP_GotFocus(NPP instance, NPFocusDirection direction)
{
    FIXME("\n");
    return NPERR_GENERIC_ERROR;
}

static void CDECL NPP_LostFocus(NPP instance)
{
    FIXME("\n");
}

/***********************************************************************
 *          NP_GetEntryPoints (mshtml.@)
 */
NPError WINAPI NP_GetEntryPoints(NPPluginFuncs* funcs)
{
    TRACE("(%p)\n", funcs);

    funcs->version = (NP_VERSION_MAJOR << 8) | NP_VERSION_MINOR;
    funcs->newp = NPP_New;
    funcs->destroy = NPP_Destroy;
    funcs->setwindow = NPP_SetWindow;
    funcs->newstream = NPP_NewStream;
    funcs->destroystream = NPP_DestroyStream;
    funcs->asfile = NPP_StreamAsFile;
    funcs->writeready = NPP_WriteReady;
    funcs->write = NPP_Write;
    funcs->print = NPP_Print;
    funcs->event = NPP_HandleEvent;
    funcs->urlnotify = NPP_URLNotify;
    funcs->javaClass = NULL;
    funcs->getvalue = NPP_GetValue;
    funcs->setvalue = NPP_SetValue;
    funcs->gotfocus = NPP_GotFocus;
    funcs->lostfocus = NPP_LostFocus;

    return NPERR_NO_ERROR;
}
