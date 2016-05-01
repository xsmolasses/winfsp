/**
 * @file dll/library.c
 *
 * @copyright 2015-2016 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the
 * GNU Affero General Public License version 3 as published by the
 * Free Software Foundation.
 *
 * Licensees holding a valid commercial license may use this file in
 * accordance with the commercial license agreement provided with the
 * software.
 */

#include <dll/library.h>

HINSTANCE DllInstance;
HANDLE ProcessHeap;

BOOL WINAPI DllMain(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    switch (Reason)
    {
    case DLL_PROCESS_ATTACH:
        DllInstance = Instance;
        ProcessHeap = GetProcessHeap();
        if (0 == ProcessHeap)
            return FALSE;
        FspFileSystemInitialize();
        break;
    case DLL_PROCESS_DETACH:
        FspFileSystemFinalize();
        break;
    }

    return TRUE;
}

/* see comments in library.h */
#if defined(WINFSP_DLL_NODEFAULTLIB)
BOOL WINAPI _DllMainCRTStartup(HINSTANCE Instance, DWORD Reason, PVOID Reserved)
{
    return DllMain(Instance, Reason, Reserved);
}
#endif

HRESULT WINAPI DllRegisterServer(VOID)
{
    NTSTATUS Result;

    Result = FspNpRegister();

    FspDebugLog("FspNpRegister = %ld\n", Result);

    return NT_SUCCESS(Result) ? S_OK : 0x80040201/*SELFREG_E_CLASS*/;
}

HRESULT WINAPI DllUnregisterServer(VOID)
{
    NTSTATUS Result;

    Result = FspNpUnregister();

    FspDebugLog("FspNpUnregister = %ld\n", Result);

    return NT_SUCCESS(Result) ? S_OK : 0x80040201/*SELFREG_E_CLASS*/;
}
