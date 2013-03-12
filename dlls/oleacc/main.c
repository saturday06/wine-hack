/*
 * Implementation of the OLEACC dll
 *
 * Copyright 2003 Mike McCormack for CodeWeavers
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
#include "windef.h"
#include "winbase.h"
#include "winuser.h"
#include "ole2.h"
#include "oleacc.h"

#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(oleacc);

static HINSTANCE oleacc_handle = 0;
static HANDLE objects_handle = NULL;
static HANDLE mutex_handle = NULL;
typedef struct Objects_ {
    int index;
    void* objects[100];
} Objects;

HRESULT WINAPI CreateStdAccessibleObject( HWND hwnd, LONG idObject,
                             REFIID riidInterface, void** ppvObject )
{
    FIXME("%p %d %s %p\n", hwnd, idObject,
          debugstr_guid( riidInterface ), ppvObject );
    return E_NOTIMPL;
}

HRESULT WINAPI ObjectFromLresult( LRESULT result, REFIID riid, WPARAM wParam, void **ppObject )
{
    HRESULT return_value = E_NOTIMPL;
    DWORD e = NO_ERROR;
    Objects* objects = NULL;
    DWORD wait_result = WAIT_FAILED;
    HRESULT query_interface_result;
    LPUNKNOWN pAcc = (LPUNKNOWN)objects->objects[result];
    
    FIXME("SELENIUM %ld %s %ld %p pid=%d\n", result, debugstr_guid(riid), wParam, ppObject, GetCurrentProcessId() );
    if (!objects_handle) {
        FIXME("SELENIUM invalid objects_handle\n");
        goto clean_up;
    }

    wait_result = WaitForSingleObject(mutex_handle, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        e = GetLastError();
        FIXME("SELENIUM wait_result=%d e=%d\n", wait_result, e);
        goto clean_up;
    }

    objects = (Objects*)MapViewOfFile(objects_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (objects == NULL) {
        e = GetLastError();
        FIXME("SELENIUM MapViewOfFile failure e=%d\n", e);
        goto clean_up;
    }

    if (0 > result || result >= objects->index || result >= (sizeof(objects->objects) / sizeof(objects->objects[0]))) {
        FIXME("SELENIUM read failure %p[%ld - %d - %d]\n",
              objects->objects, result, objects->index,
              (sizeof(objects->objects) / sizeof(objects->objects[0])));
        return_value = E_INVALIDARG;
        goto clean_up;
    }

    FIXME("SELENIUM read pAcc=%p\n", pAcc);
    FIXME("SELENIUM read pAcc->lpVtbl=%p\n", pAcc->lpVtbl);
    FIXME("SELENIUM read pAcc->lpVtbl->QueryInterface=%p\n", pAcc->lpVtbl->QueryInterface);
    query_interface_result = pAcc->lpVtbl->QueryInterface(pAcc, riid, ppObject);
    if (FAILED(query_interface_result)) {
        return_value = E_NOTIMPL;
        goto clean_up;
    }

    FIXME("SELENIUM read %p[%ld] -> %p / %p\n",
          objects->objects, result, objects->objects[result], *ppObject);
    return_value = S_OK;

clean_up:
    if (objects) {
        UnmapViewOfFile(objects);
    }
    if (wait_result == WAIT_OBJECT_0) {
        ReleaseMutex(mutex_handle);
    }
    FIXME("SELENIUM rv=%d\n", return_value);
    return return_value;
}

LRESULT WINAPI LresultFromObject( REFIID riid, WPARAM wParam, LPUNKNOWN pAcc )
{
    DWORD e = NO_ERROR;
    DWORD wait_result = WAIT_FAILED;
    LRESULT return_value = 0;
    Objects* objects = NULL;

    FIXME("SELENIUM %s %ld %p pid=%d\n", debugstr_guid(riid), wParam, pAcc, GetCurrentProcessId() );
    FIXME("SELENIUM write pAcc=%p\n", pAcc);
    FIXME("SELENIUM write pAcc->lpVtbl=%p\n", pAcc->lpVtbl);
    FIXME("SELENIUM write pAcc->lpVtbl->QueryInterface=%p\n", pAcc->lpVtbl->QueryInterface);
    if (!objects_handle) {
        FIXME("SELENIUM invalid objects_handle\n");
        return_value = E_NOTIMPL;
        goto clean_up;
    }

    wait_result = WaitForSingleObject(mutex_handle, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        e = GetLastError();
        FIXME("SELENIUM wait_result=%d e=%d\n", wait_result, e);
        goto clean_up;
    }

    objects = (Objects*)MapViewOfFile(objects_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (objects == NULL) {
        e = GetLastError();
        FIXME("SELENIUM MapViewOfFile failure e=%d\n", e);
        return_value = E_NOTIMPL;
        goto clean_up;
    }

    if (objects->index >= (sizeof(objects->objects) / sizeof(objects->objects[0]))) {
        return_value = E_OUTOFMEMORY;
        goto clean_up;
    }

    objects->objects[objects->index] = pAcc;
    return_value = objects->index;
    FIXME("SELENIUM write %p[%d] <- %p\n", objects->objects, objects->index, pAcc);
    objects->index++;
    pAcc->lpVtbl->AddRef(pAcc);

clean_up:
    if (objects) {
        UnmapViewOfFile(objects);
    }
    if (wait_result == WAIT_OBJECT_0) {
        ReleaseMutex(mutex_handle);
    }
    FIXME("SELENIUM rv=%ld\n", return_value);
    return return_value;
}

HRESULT WINAPI AccessibleObjectFromPoint( POINT ptScreen, IAccessible** ppacc, VARIANT* pvarChild )
{
    FIXME("{%d,%d} %p %p: stub\n", ptScreen.x, ptScreen.y, ppacc, pvarChild );
    return E_NOTIMPL;
}

HRESULT WINAPI AccessibleObjectFromWindow( HWND hwnd, DWORD dwObjectID,
                             REFIID riid, void** ppvObject )
{
    FIXME("%p %d %s %p\n", hwnd, dwObjectID,
          debugstr_guid( riid ), ppvObject );
    return E_NOTIMPL;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason,
                    LPVOID lpvReserved)
{
    TRACE("%p, %d, %p\n", hinstDLL, fdwReason, lpvReserved);

    switch (fdwReason)
    {
        case DLL_PROCESS_ATTACH: {
            Objects initializer = {0};
            Objects* objects = NULL;
            oleacc_handle = hinstDLL;
            DisableThreadLibraryCalls(hinstDLL);
            mutex_handle = CreateMutexA(
                NULL,
                FALSE,
                "Kumei's MONIKER_INDEX_MUTEX_NAME");
            if (!mutex_handle) {
                DWORD e = GetLastError();
                FIXME("SELENIUM CreateMutex failure e=%d\n", e);
                break;
            }
            objects_handle = CreateFileMappingA(
                INVALID_HANDLE_VALUE,
                NULL,
                PAGE_READWRITE,
                0, 
                1024,
                "Kumei's Object");
            if (!objects_handle) {
                DWORD e = GetLastError();
                FIXME("SELENIUM CreateFileMapping failure e=%d\n", e);
                break;
            }
            objects = (Objects*)MapViewOfFile(objects_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
            if (objects == NULL) {
                DWORD e = GetLastError();
                FIXME("SELENIUM MapViewOfFile failure e=%d\n", e);
                break;
            }
            initializer.index = 1; /* Positive value required. */
            *objects = initializer;
            UnmapViewOfFile(objects);
            break;
        }
        case DLL_PROCESS_DETACH:
            if (objects_handle) {
                CloseHandle(objects_handle);
            }
            if (mutex_handle) {
                CloseHandle(mutex_handle);
            }
            break;
    }
    return TRUE;
}

HRESULT WINAPI DllRegisterServer(void)
{
    FIXME("\n");
    return S_OK;
}

HRESULT WINAPI DllUnregisterServer(void)
{
    FIXME("\n");
    return S_OK;
}

void WINAPI GetOleaccVersionInfo(DWORD* pVersion, DWORD* pBuild)
{
    *pVersion = MAKELONG(0,7); /* Windows 7 version of oleacc: 7.0.0.0 */
    *pBuild = MAKELONG(0,0);
}

UINT WINAPI GetRoleTextW(DWORD role, LPWSTR lpRole, UINT rolemax)
{
    INT ret;
    WCHAR *resptr;

    TRACE("%u %p %u\n", role, lpRole, rolemax);

    /* return role text length */
    if(!lpRole)
        return LoadStringW(oleacc_handle, role, (LPWSTR)&resptr, 0);

    ret = LoadStringW(oleacc_handle, role, lpRole, rolemax);
    if(!(ret > 0)){
        if(rolemax > 0) lpRole[0] = '\0';
        return 0;
    }

    return ret;
}

UINT WINAPI GetRoleTextA(DWORD role, LPSTR lpRole, UINT rolemax)
{
    UINT length;
    WCHAR *roletextW;

    TRACE("%u %p %u\n", role, lpRole, rolemax);

    length = GetRoleTextW(role, NULL, 0);
    if((length == 0) || (lpRole && !rolemax))
        return 0;

    roletextW = HeapAlloc(GetProcessHeap(), 0, (length + 1)*sizeof(WCHAR));
    if(!roletextW)
        return 0;

    GetRoleTextW(role, roletextW, length + 1);

    length = WideCharToMultiByte( CP_ACP, 0, roletextW, -1, NULL, 0, NULL, NULL );

    if(!lpRole){
        HeapFree(GetProcessHeap(), 0, roletextW);
        return length - 1;
    }

    WideCharToMultiByte( CP_ACP, 0, roletextW, -1, lpRole, rolemax, NULL, NULL );

    if(rolemax < length){
        lpRole[rolemax-1] = '\0';
        length = rolemax;
    }

    HeapFree(GetProcessHeap(), 0, roletextW);

    return length - 1;
}
