/* DirectMusicLoader Main
 *
 * Copyright (C) 2003-2004 Rok Mandeljc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "dmloader_private.h"
#include "rpcproxy.h"

WINE_DEFAULT_DEBUG_CHANNEL(dmloader);

static HINSTANCE instance;
LONG module_ref = 0;

typedef struct {
    IClassFactory IClassFactory_iface;
    HRESULT WINAPI (*fnCreateInstance)(REFIID riid, void **ppv, IUnknown *pUnkOuter);
} IClassFactoryImpl;

/******************************************************************
 *      IClassFactory implementation
 */
static inline IClassFactoryImpl *impl_from_IClassFactory(IClassFactory *iface)
{
    return CONTAINING_RECORD(iface, IClassFactoryImpl, IClassFactory_iface);
}

static HRESULT WINAPI ClassFactory_QueryInterface(IClassFactory *iface, REFIID riid, void **ppv)
{
    if (ppv == NULL)
        return E_POINTER;

    if (IsEqualGUID(&IID_IUnknown, riid))
        TRACE("(%p)->(IID_IUnknown %p)\n", iface, ppv);
    else if (IsEqualGUID(&IID_IClassFactory, riid))
        TRACE("(%p)->(IID_IClassFactory %p)\n", iface, ppv);
    else {
        FIXME("(%p)->(%s %p)\n", iface, debugstr_guid(riid), ppv);
        *ppv = NULL;
        return E_NOINTERFACE;
}

    *ppv = iface;
    IClassFactory_AddRef(iface);
    return S_OK;
}

static ULONG WINAPI ClassFactory_AddRef(IClassFactory *iface)
{
    lock_module();

    return 2; /* non-heap based object */
}

static ULONG WINAPI ClassFactory_Release(IClassFactory *iface)
{
    unlock_module();

    return 1; /* non-heap based object */
}

static HRESULT WINAPI ClassFactory_CreateInstance(IClassFactory *iface, IUnknown *pUnkOuter,
    REFIID riid, void **ppv)
{
    IClassFactoryImpl *This = impl_from_IClassFactory(iface);

    TRACE ("(%p, %s, %p)\n", pUnkOuter, debugstr_dmguid(riid), ppv);

    return This->fnCreateInstance(riid, ppv, pUnkOuter);
}

static HRESULT WINAPI ClassFactory_LockServer(IClassFactory *iface, BOOL dolock)
{
    TRACE("(%d)\n", dolock);

    if (dolock)
        lock_module();
    else
        unlock_module();

    return S_OK;
}

static const IClassFactoryVtbl classfactory_vtbl = {
    ClassFactory_QueryInterface,
    ClassFactory_AddRef,
    ClassFactory_Release,
    ClassFactory_CreateInstance,
    ClassFactory_LockServer
};

static IClassFactoryImpl dm_loader_CF = {{&classfactory_vtbl}, DMUSIC_CreateDirectMusicLoaderImpl};
static IClassFactoryImpl dm_container_CF = {{&classfactory_vtbl},
                                            DMUSIC_CreateDirectMusicContainerImpl};

/******************************************************************
 *		DllMain
 */
BOOL WINAPI DllMain (HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved) {
	if (fdwReason == DLL_PROCESS_ATTACH) {
                instance = hinstDLL;
		DisableThreadLibraryCalls(hinstDLL);
		/* FIXME: Initialisation */
	} else if (fdwReason == DLL_PROCESS_DETACH) {
		/* FIXME: Cleanup */
	}
	return TRUE;
}


/******************************************************************
 *		DllCanUnloadNow (DMLOADER.@)
 */
HRESULT WINAPI DllCanUnloadNow (void)
{
    TRACE("() ref=%d\n", module_ref);

    return module_ref ? S_FALSE : S_OK;
}

/******************************************************************
 *		DllGetClassObject (DMLOADER.@)
 */
HRESULT WINAPI DllGetClassObject (REFCLSID rclsid, REFIID riid, LPVOID *ppv)
{
    TRACE("(%s, %s, %p)\n", debugstr_dmguid(rclsid), debugstr_dmguid(riid), ppv);
    if (IsEqualCLSID (rclsid, &CLSID_DirectMusicLoader) && IsEqualIID (riid, &IID_IClassFactory)) {
        IClassFactory_AddRef(&dm_loader_CF.IClassFactory_iface);
        *ppv = &dm_loader_CF.IClassFactory_iface;
        return S_OK;
    } else if (IsEqualCLSID (rclsid, &CLSID_DirectMusicContainer) && IsEqualIID (riid, &IID_IClassFactory)) {
        IClassFactory_AddRef(&dm_container_CF.IClassFactory_iface);
        *ppv = &dm_container_CF.IClassFactory_iface;
        return S_OK;
    }

    WARN(": no class found\n");
    return CLASS_E_CLASSNOTAVAILABLE;
}

/***********************************************************************
 *		DllRegisterServer (DMLOADER.@)
 */
HRESULT WINAPI DllRegisterServer(void)
{
    return __wine_register_resources( instance );
}

/***********************************************************************
 *		DllUnregisterServer (DMLOADER.@)
 */
HRESULT WINAPI DllUnregisterServer(void)
{
    return __wine_unregister_resources( instance );
}
