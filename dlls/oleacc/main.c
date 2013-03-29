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

static IStream* CreateObjectStreamOnFile( LRESULT marshal_sequence, REFIID riid, LPUNKNOWN pAcc, DWORD grfMode, BOOL fCreate )
{
    IStream* stream = NULL;
    WCHAR dir_name[MAX_PATH + 1] = {0};
    WCHAR file_name[1024 / sizeof(WCHAR)] = {0}; // 1024 bytes. see http://msdn.microsoft.com/en-us/library/windows/desktop/ms647550%28v=vs.85%29.aspx
    const WCHAR file_name_format[] = {
        L'a', L'c', L'c', L'-',
        L'%', L'x'
    };
    WCHAR* full_name = NULL;
    HANDLE heap_handle;
    HRESULT result_handle;
    LARGE_INTEGER move = {0};

    heap_handle = GetProcessHeap();
    if (heap_handle == NULL) {
        DWORD e = GetLastError();
        FIXME("HeapAllocObjectTempPath a e=%d\n", e);
        goto clean_up;
    }
    wsprintfW(file_name, file_name_format, marshal_sequence);
    if (GetTempPathW(sizeof(dir_name) / sizeof(dir_name[0]), dir_name) == 0) {
        DWORD e = GetLastError();
        FIXME("HeapAllocObjectTempPath b e=%d\n", e);
        goto clean_up;
    }
    full_name = HeapAlloc(heap_handle, 0, sizeof(WCHAR) * (lstrlenW(dir_name) + lstrlenW(file_name) + (/* null term */ 1)));
    if (full_name == NULL) {
        DWORD e = GetLastError();
        FIXME("HeapAllocObjectTempPath c e=%d\n", e);
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
    FIXME("HeapAllocObjectTempPath success p=%p\n", full_name);

    result_handle = SHCreateStreamOnFileEx(
        full_name, grfMode,
        FILE_ATTRIBUTE_NORMAL, fCreate, NULL, &stream);
    if (result_handle != S_OK) {
        DWORD e = GetLastError();
        FIXME("SHCreateStreamOnFileEx d e=%d hr=%d\n", e, result_handle);
        goto clean_up;
    }
    result_handle = stream->lpVtbl->Seek(stream, move, STREAM_SEEK_SET, NULL);
    if (result_handle != S_OK) {
        DWORD e = GetLastError();
        FIXME("Y%d\n", 10);
        FIXME("IStream::Seek e=%d hr=%d\n", e, result_handle);
        goto clean_up;
    }

    FIXME("X SUCCEED\n");
clean_up:
    FIXME("X%d\n", 2);
    if (full_name != NULL) {
        FIXME("X%d\n", 3);
        HeapFree(heap_handle, 0, full_name);
    }
    FIXME("X%d\n", 4);
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
    SharedVariable* shared_variable = NULL;
    DWORD wait_result = WAIT_FAILED;
    LPUNKNOWN pAcc = NULL; // (LPUNKNOWN)shared_variable->objects[result];
    IStream* stream = NULL;
    HRESULT result_handle;
    
    FIXME("SELENIUM %ld %s %ld %p pid=%d\n", result, debugstr_guid(riid), wParam, ppObject, GetCurrentProcessId() );
    if (!shared_variable_handle) {
        FIXME("SELENIUM invalid shared_variable_handle\n");
        goto clean_up;
    }

    wait_result = WaitForSingleObject(shared_variable_mutex_handle, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        e = GetLastError();
        FIXME("SELENIUM wait_result=%d e=%d\n", wait_result, e);
        goto clean_up;
    }

    stream = CreateObjectStreamOnFile( result, riid, pAcc, STGM_READ | STGM_FAILIFTHERE, FALSE);
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
    if (shared_variable) {
        UnmapViewOfFile(shared_variable);
    }
    if (wait_result == WAIT_OBJECT_0) {
        ReleaseMutex(shared_variable_mutex_handle);
    }
    FIXME("SELENIUM rv=%d\n", return_value);
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
    INT marshal_sequence;

    FIXME("SELENIUM %s %ld %p pid=%d\n", debugstr_guid(riid), wParam, pAcc, GetCurrentProcessId() );

    if (!shared_variable_handle) {
        FIXME("SELENIUM invalid shared_variable_handle\n");
        return_value = E_OUTOFMEMORY;
        goto clean_up;
    }

    wait_result = WaitForSingleObject(shared_variable_mutex_handle, INFINITE);
    if (wait_result != WAIT_OBJECT_0) {
        e = GetLastError();
        FIXME("SELENIUM wait_result=%d e=%d\n", wait_result, e);
        return_value = E_OUTOFMEMORY;
        goto clean_up;
    }

    shared_variable = (SharedVariable*)MapViewOfFile(shared_variable_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
    if (shared_variable == NULL) {
        e = GetLastError();
        FIXME("SELENIUM MapViewOfFile failure e=%d\n", e);
        return_value = E_OUTOFMEMORY;
        goto clean_up;
    }

    marshal_sequence = shared_variable->marshal_sequence;

    stream = CreateObjectStreamOnFile( marshal_sequence, riid, pAcc, STGM_WRITE | STGM_CREATE, TRUE);
    FIXME("Y%d\n", 1);
    if (stream == NULL) {
        FIXME("Y%d\n", 2);
        goto clean_up;
    }

    FIXME("Y%d\n", 3);
    result_handle = CoMarshalInterface(
        stream, riid, pAcc,
        MSHCTX_LOCAL, NULL, MSHLFLAGS_NORMAL);
    FIXME("Y%d\n", 4);
    if (result_handle != S_OK) {
        DWORD e = GetLastError();
        FIXME("Y%d\n", 5);
        FIXME("CoMarshalInterface e e=%d hr=%d\n", e, result_handle);
        goto clean_up;
    }

//    result_handle = stream->lpVtbl->Write(stream, "foo", 3, NULL);
//    FIXME("Y%d\n", 20);
//    if (result_handle != S_OK) {
//        DWORD e = GetLastError();
//        FIXME("Y%d\n", 21);
//        FIXME("IStream::Commit e=%d hr=%d\n", e, result_handle);
//    }

    FIXME("Y%d\n", 6);
    return_value = marshal_sequence; // redudant?

clean_up:
    FIXME("Y%d\n", 7);
    if (stream) {
        FIXME("Y%d\n", 8);
        result_handle = stream->lpVtbl->Commit(stream, STGC_DEFAULT);
        FIXME("Y%d\n", 9);
        if (result_handle != S_OK) {
            DWORD e = GetLastError();
            FIXME("Y%d\n", 10);
            FIXME("IStream::Commit e=%d hr=%d\n", e, result_handle);
        }
        FIXME("Y%d\n", 11);
        stream->lpVtbl->Release(stream);
        FIXME("Y%d\n", 12);
    }
    FIXME("Y%d\n", 13);
    if (shared_variable) {
        FIXME("Y%d\n", 14);
        UnmapViewOfFile(shared_variable);
    }
    FIXME("Y%d\n", 15);
    if (wait_result == WAIT_OBJECT_0) {
        FIXME("Y%d\n", 16);
        ReleaseMutex(shared_variable_mutex_handle);
    }
    FIXME("Y%d\n", 17);
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

            // No initialization.
            // shared_variable = (SharedVariable*)MapViewOfFile(shared_variable_handle, FILE_MAP_ALL_ACCESS, 0, 0, 0);
            // if (shared_variable == NULL) {
            //     DWORD e = GetLastError();
            //     FIXME("SELENIUM MapViewOfFile failure e=%d\n", e);
            //     break;
            // }
            // *shared_variable = initializer;
            // UnmapViewOfFile(shared_variable);
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

