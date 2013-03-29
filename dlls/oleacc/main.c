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
#include "shlwapi.h"

#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(oleacc);

static HINSTANCE oleacc_handle = 0;
static HANDLE shared_variable_handle = NULL;
static HANDLE shared_variable_mutex_handle = NULL;
typedef struct SharedVariable_ {
    LRESULT marshal_sequence;
} SharedVariable;

static IStream* CreateObjectStreamOnFile( LRESULT marshal_sequence, DWORD grfMode, BOOL fCreate )
{
    IStream* stream = NULL;
    WCHAR dir_name[MAX_PATH + 1] = {0};
    WCHAR file_name[1024 / sizeof(WCHAR)] = {0}; /* 1024 bytes. see http://msdn.microsoft.com/en-us/library/windows/desktop/ms647550%28v=vs.85%29.aspx */
    const WCHAR file_name_format[] = {
        L'\\',
        L'L', L'r', L'e', L's', L'u', L'l', L't',
        L'F', L'r', L'o', L'm',
        L'O', L'b', L'j', L'e', L'c', L't',
        L'-', L'%', L'x'
    };
    WCHAR* full_name = NULL;
    HANDLE heap_handle;
    HRESULT result_handle;
    LARGE_INTEGER seek_offset = {{0}};

    heap_handle = GetProcessHeap();
    if (heap_handle == NULL) {
        DWORD e = GetLastError();
        FIXME("GetProcessHeap e=%d\n", e);
        goto clean_up;
    }
    wsprintfW(file_name, file_name_format, marshal_sequence);
    if (GetTempPathW(sizeof(dir_name) / sizeof(dir_name[0]), dir_name) == 0) {
        DWORD e = GetLastError();
        FIXME("GetTempPathW e=%d\n", e);
        goto clean_up;
    }
    full_name = HeapAlloc(heap_handle, 0, sizeof(WCHAR) * (lstrlenW(dir_name) + lstrlenW(file_name) + (/* null term */ 1)));
    if (full_name == NULL) {
        DWORD e = GetLastError();
        FIXME("HeapAlloc e=%d\n", e);
        goto clean_up;
    }
    lstrcpyW(full_name, dir_name);
    {
        int l = lstrlenW(full_name) - 1;
        if (l > 0 && full_name[l] == L'\\') {
            full_name[l] = 0;
        }
    }
    lstrcatW(full_name, file_name);

    result_handle = SHCreateStreamOnFileEx(
        full_name, grfMode,
        FILE_ATTRIBUTE_NORMAL, fCreate, NULL, &stream);
    if (result_handle != S_OK) {
        DWORD e = GetLastError();
        FIXME("SHCreateStreamOnFileEx e=%d hr=%d\n", e, result_handle);
        goto clean_up;
    }
    result_handle = stream->lpVtbl->Seek(stream, seek_offset, STREAM_SEEK_SET, NULL);
    if (result_handle != S_OK) {
        DWORD e = GetLastError();
        FIXME("IStream::Seek e=%d hr=%d\n", e, result_handle);
        goto clean_up;
    }

clean_up:
    if (full_name != NULL) {
        HeapFree(heap_handle, 0, full_name);
    }
    return stream;
}

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
    IStream* stream = NULL;
    HRESULT result_handle;
    
    FIXME("args: %ld %s %ld %p pid=%d\n", result, debugstr_guid(riid), wParam, ppObject, GetCurrentProcessId() );

    stream = CreateObjectStreamOnFile( result, STGM_READ | STGM_FAILIFTHERE | STGM_DELETEONRELEASE, FALSE);
    if (stream == NULL) {
        goto clean_up;
    }

    result_handle = CoUnmarshalInterface(stream, riid, ppObject);
    if (result_handle != S_OK) {
        e = GetLastError();
        FIXME("CoUnmarshalInterface e=%d hr=%d\n", e, result_handle);
        goto clean_up;
    }

    return_value = S_OK;

clean_up:
    if (stream) {
        stream->lpVtbl->Release(stream);
    }
    FIXME("rv=%d\n", return_value);
    return return_value;
}

LRESULT WINAPI LresultFromObject( REFIID riid, WPARAM wParam, LPUNKNOWN pAcc )
{
    DWORD e = NO_ERROR;
    DWORD wait_result = WAIT_FAILED;
    LRESULT return_value = E_OUTOFMEMORY;
    SharedVariable* shared_variable = NULL;
    IStream* stream = NULL;
    HRESULT result_handle;

    FIXME("ARGS %s %ld %p pid=%d\n", debugstr_guid(riid), wParam, pAcc, GetCurrentProcessId() );

    if (!shared_variable_handle) {
        FIXME("invalid shared_variable_handle\n");
        return_value = E_OUTOFMEMORY;
        goto clean_up;
    }

    wait_result = WaitForSingleObject(shared_variable_mutex_handle, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        e = GetLastError();
        FIXME("WaitForSingleObject wait_result=%d e=%d\n", wait_result, e);
        return_value = E_OUTOFMEMORY;
        goto clean_up;
    }

    shared_variable = (SharedVariable*)MapViewOfFile(shared_variable_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (shared_variable == NULL) {
        e = GetLastError();
        FIXME("MapViewOfFile failure e=%d\n", e);
        return_value = E_OUTOFMEMORY;
        goto clean_up;
    }

    shared_variable->marshal_sequence++;

    stream = CreateObjectStreamOnFile( shared_variable->marshal_sequence, STGM_WRITE | STGM_CREATE, TRUE);
    if (stream == NULL) {
        goto clean_up;
    }

    result_handle = CoMarshalInterface(
        stream, riid, pAcc,
        MSHCTX_LOCAL, NULL, MSHLFLAGS_NORMAL);
    if (result_handle != S_OK) {
        DWORD e = GetLastError();
        FIXME("CoMarshalInterface e e=%d hr=%d\n", e, result_handle);
        goto clean_up;
    }

    return_value = shared_variable->marshal_sequence;

clean_up:
    if (stream) {
        result_handle = stream->lpVtbl->Commit(stream, STGC_DEFAULT);
        if (result_handle != S_OK) {
            DWORD e = GetLastError();
            FIXME("IStream::Commit e=%d hr=%d\n", e, result_handle);
        }
        stream->lpVtbl->Release(stream);
    }
    if (shared_variable) {
        UnmapViewOfFile(shared_variable);
    }
    if (wait_result == WAIT_OBJECT_0) {
        ReleaseMutex(shared_variable_mutex_handle);
    }
    FIXME("rv=%ld\n", return_value);
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
            oleacc_handle = hinstDLL;
            DisableThreadLibraryCalls(hinstDLL);
            shared_variable_mutex_handle = CreateMutexA(
                NULL,
                FALSE,
                "Kumei's MONIKER_INDEX_MUTEX_NAME");
            if (!shared_variable_mutex_handle) {
                DWORD e = GetLastError();
                FIXME("SELENIUM CreateMutex failure e=%d\n", e);
                break;
            }
            shared_variable_handle = CreateFileMappingA(
                INVALID_HANDLE_VALUE,
                NULL,
                PAGE_READWRITE,
                0, 
                1024,
                "Kumei's Object");
            if (!shared_variable_handle) {
                DWORD e = GetLastError();
                FIXME("SELENIUM CreateFileMapping failure e=%d\n", e);
                break;
            }
            break;
        }
        case DLL_PROCESS_DETACH:
            if (shared_variable_handle) {
                CloseHandle(shared_variable_handle);
            }
            if (shared_variable_mutex_handle) {
                CloseHandle(shared_variable_mutex_handle);
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

