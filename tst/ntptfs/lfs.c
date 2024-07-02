/**
 * @file lfs.c
 *
 * @copyright 2015-2024 Bill Zissimopoulos
 */
/*
 * This file is part of WinFsp.
 *
 * You can redistribute it and/or modify it under the terms of the GNU
 * General Public License version 3 as published by the Free Software
 * Foundation.
 *
 * Licensees holding a valid commercial license may use this software
 * in accordance with the commercial license agreement provided in
 * conjunction with the software.  The terms and conditions of any such
 * commercial license agreement shall govern, supersede, and render
 * ineffective any application of the GPLv3 license to this software,
 * notwithstanding of any reference thereto in the software or
 * associated repository.
 */

#include "ptfs.h"

static inline HANDLE LfsThreadEvent(VOID)
{
    static __declspec(thread) HANDLE Event;

    if (0 == Event)
        Event = CreateEventW(0, TRUE, FALSE, 0);
    return Event;
}

NTSTATUS LfsCreateFile(
    PHANDLE PHandle,
    ACCESS_MASK DesiredAccess,
    HANDLE RootHandle,
    PWSTR FileName,
    PSECURITY_DESCRIPTOR SecurityDescriptor,
    PLARGE_INTEGER AllocationSize,
    ULONG FileAttributes,
    ULONG CreateDisposition,
    ULONG CreateOptions,
    PVOID EaBuffer,
    ULONG EaLength)
{
    info(L"ENTER:         LfsCreateFile() DesiredAccess:%08lX FileAttributes:%08lX CreateDisposition:%08lX CreateOptions:%08lX EaLength:%lu", FILE_READ_ATTRIBUTES | DesiredAccess, FileAttributes, CreateDisposition, CreateOptions, EaLength); // xsmolasses

    UNICODE_STRING Ufnm;
    OBJECT_ATTRIBUTES Obja;
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Result;
 
    RtlInitUnicodeString(&Ufnm, FileName + 1);
    InitializeObjectAttributes(&Obja, &Ufnm, 0, RootHandle, SecurityDescriptor);

    info(L"Obja.Attributes:%08lX", Obja.Attributes); // xsmolasses

    Result = NtCreateFile(
        PHandle,
        FILE_READ_ATTRIBUTES | DesiredAccess,
        &Obja,
        &Iosb,
        AllocationSize,
        FileAttributes,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        CreateDisposition,
        CreateOptions,
        EaBuffer,
        EaLength);
#if 0
    if (STATUS_DELETE_PENDING == Result && IsDebuggerPresent())
        DebugBreak();
#endif
    info(L"LEAVE:%08lX=LfsCreateFile() DesiredAccess:%08lX FileAttributes:%08lX CreateDisposition:%08lX CreateOptions:%08lX EaLength:%lu", Result, FILE_READ_ATTRIBUTES | DesiredAccess, FileAttributes, CreateDisposition, CreateOptions, EaLength); // xsmolasses
    return Result;
}

NTSTATUS LfsOpenFile(
    PHANDLE PHandle,
    ACCESS_MASK DesiredAccess,
    HANDLE RootHandle,
    PWSTR FileName,
    ULONG OpenOptions)
{
    info(L"ENTER:         LfsOpenFile()"); // xsmolasses
    
    UNICODE_STRING Ufnm;
    OBJECT_ATTRIBUTES Obja;
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Result;

    RtlInitUnicodeString(&Ufnm, FileName + 1);
    InitializeObjectAttributes(&Obja, &Ufnm, 0, RootHandle, 0); // xsmolasses earmarked

    if(Obja.RootDirectory)
    {
        WCHAR szFilePath[MAX_PATH];
        
        if(GetFinalPathNameByHandleW(Obja.RootDirectory, szFilePath, MAX_PATH, FILE_NAME_NORMALIZED|VOLUME_NAME_DOS)) //|VOLUME_NAME_NT))
            info(L"Obja.RootDirectory FILE_NAME_NORMALIZED: %ws", szFilePath);
        
        if(GetFinalPathNameByHandleW(Obja.RootDirectory, szFilePath, MAX_PATH, FILE_NAME_OPENED|VOLUME_NAME_DOS)) //|VOLUME_NAME_NT))
            info(L"Obja.RootDirectory FILE_NAME_OPENED: %ws", szFilePath);
    }

    info(L"Obja.Attributes:%08lX", Obja.Attributes); // xsmolasses
    //Obja.Attributes|=OBJ_OPENLINK; // DIR: "The parameter is incorrect."
    //info(L"Obja.Attributes:%08lX", Obja.Attributes); // xsmolasses

    Result = NtOpenFile(
        PHandle,
        FILE_READ_ATTRIBUTES | DesiredAccess,
        &Obja,
        &Iosb,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        OpenOptions|FILE_OPEN_REPARSE_POINT);
#if 0
    if (STATUS_DELETE_PENDING == Result && IsDebuggerPresent())
        DebugBreak();
#endif

    if(PHandle && *PHandle)
    {
        WCHAR szFilePath[MAX_PATH];
        
        if(GetFinalPathNameByHandleW(*PHandle, szFilePath, MAX_PATH, FILE_NAME_NORMALIZED|VOLUME_NAME_DOS)) //|VOLUME_NAME_NT))
            info(L"PHandle FILE_NAME_NORMALIZED: %ws", szFilePath);
        
        if(GetFinalPathNameByHandleW(*PHandle, szFilePath, MAX_PATH, FILE_NAME_OPENED|VOLUME_NAME_DOS)) //|VOLUME_NAME_NT))
            info(L"PHandle FILE_NAME_OPENED: %ws", szFilePath);
    }
    
    info(L"LEAVE:%08lX=LfsOpenFile() DesiredAccess:%08lX OpenOptions:%08lX FileName:%ws", Result, DesiredAccess, OpenOptions, FileName); // xsmolasses
    return Result;
}

NTSTATUS LfsGetFileInfo(
    HANDLE Handle,
    ULONG RootPrefixLength,
    ULONG FsAttributeMask, // xsmolasses
    FSP_FSCTL_FILE_INFO *FileInfo)
{
    info(L"ENTER:         LfsGetFileInfo()"); // xsmolasses

    FSP_FSCTL_OPEN_FILE_INFO *OpenFileInfo = -1 != RootPrefixLength ?
        FspFileSystemGetOpenFileInfo(FileInfo) : 0;
    IO_STATUS_BLOCK Iosb;
    union
    {
        FILE_ALL_INFORMATION V;
        UINT8 B[FIELD_OFFSET(FILE_ALL_INFORMATION, NameInformation.FileName) + FSP_FSCTL_TRANSACT_PATH_SIZEMAX];
    } FileAllInfo;
    FILE_ATTRIBUTE_TAG_INFORMATION FileAttrInfo;
    NTSTATUS Result;

    Result = NtQueryInformationFile(
        Handle,
        &Iosb,
        &FileAllInfo,
        sizeof FileAllInfo,
        18/*FileAllInformation*/);
    if (STATUS_BUFFER_OVERFLOW == Result)
        OpenFileInfo = 0;
    else if (!NT_SUCCESS(Result))
        goto exit;
    if ((FsAttributeMask & PtfsReparsePoints) && // xsmolasses
        0 != (FILE_ATTRIBUTE_REPARSE_POINT & FileAllInfo.V.BasicInformation.FileAttributes))
    {
        Result = NtQueryInformationFile(
            Handle,
            &Iosb,
            &FileAttrInfo,
            sizeof FileAttrInfo,
            35/*FileAttributeTagInformation*/);
        if (!NT_SUCCESS(Result))
            goto exit;
    }

    Result = STATUS_SUCCESS;

    FileInfo->FileAttributes = (FsAttributeMask & PtfsReparsePoints) ? // xsmolasses
        FileAllInfo.V.BasicInformation.FileAttributes : FileAllInfo.V.BasicInformation.FileAttributes & ~FILE_ATTRIBUTE_REPARSE_POINT; // xsmolasses
    FileInfo->ReparseTag = (FsAttributeMask & PtfsReparsePoints) && // xsmolasses
        0 != (FILE_ATTRIBUTE_REPARSE_POINT & FileAllInfo.V.BasicInformation.FileAttributes) ?
        FileAttrInfo.ReparseTag : 0;
    FileInfo->AllocationSize = FileAllInfo.V.StandardInformation.AllocationSize.QuadPart;
    FileInfo->FileSize = FileAllInfo.V.StandardInformation.EndOfFile.QuadPart;
    FileInfo->CreationTime = FileAllInfo.V.BasicInformation.CreationTime.QuadPart;
    FileInfo->LastAccessTime = FileAllInfo.V.BasicInformation.LastAccessTime.QuadPart;
    FileInfo->LastWriteTime = FileAllInfo.V.BasicInformation.LastWriteTime.QuadPart;
    FileInfo->ChangeTime = FileAllInfo.V.BasicInformation.ChangeTime.QuadPart;
    FileInfo->IndexNumber = FileAllInfo.V.InternalInformation.IndexNumber.QuadPart;
    FileInfo->HardLinks = 0;
    //FileInfo->EaSize = LfsGetEaSize(FileAllInfo.V.EaInformation.EaSize);
    FileInfo->EaSize = 0; // xsmolasses

    if (0 != OpenFileInfo &&
        OpenFileInfo->NormalizedNameSize > sizeof(WCHAR) + FileAllInfo.V.NameInformation.FileNameLength &&
        RootPrefixLength <= FileAllInfo.V.NameInformation.FileNameLength)
    {
        PWSTR P = (PVOID)((PUINT8)FileAllInfo.V.NameInformation.FileName + RootPrefixLength);
        ULONG L = FileAllInfo.V.NameInformation.FileNameLength - RootPrefixLength;

        if (L'\\' == *P)
        {
            memcpy(OpenFileInfo->NormalizedName, P, L);
            OpenFileInfo->NormalizedNameSize = (UINT16)L;
        }
        else
        {
            *OpenFileInfo->NormalizedName = L'\\';
            memcpy(OpenFileInfo->NormalizedName + 1, P, L);
            OpenFileInfo->NormalizedNameSize = (UINT16)(L + sizeof(WCHAR));
        }
    }

exit:
    info(L"LEAVE:%08lX=LfsGetFileInfo()", Result); // xsmolasses
    return Result;
}

NTSTATUS LfsReadFile(
    HANDLE Handle,
    PVOID Buffer,
    UINT64 Offset,
    ULONG Length,
    PULONG PBytesTransferred)
{
    info(L"ENTER:         LfsReadFile()"); // xsmolasses

    HANDLE Event;
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Result;

    Event = LfsThreadEvent();
    if (0 == Event)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    Result = NtReadFile(
        Handle,
        Event,
        0,
        0,
        &Iosb,
        Buffer,
        Length,
        (PLARGE_INTEGER)&Offset,
        0);
    if (STATUS_PENDING == Result)
    {
        WaitForSingleObject(Event, INFINITE);
        Result = Iosb.Status;
    }

    *PBytesTransferred = (ULONG)Iosb.Information;

exit:
    info(L"LEAVE:%08lX=LfsReadFile()", Result); // xsmolasses
    return Result;
}

NTSTATUS LfsWriteFile(
    HANDLE Handle,
    PVOID Buffer,
    UINT64 Offset,
    ULONG Length,
    PULONG PBytesTransferred)
{
    info(L"ENTER:         LfsWriteFile()"); // xsmolasses

    HANDLE Event;
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Result;

    Event = LfsThreadEvent();
    if (0 == Event)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    Result = NtWriteFile(
        Handle,
        Event,
        0,
        0,
        &Iosb,
        Buffer,
        Length,
        (PLARGE_INTEGER)&Offset,
        0);
    if (STATUS_PENDING == Result)
    {
        WaitForSingleObject(Event, INFINITE);
        Result = Iosb.Status;
    }

    *PBytesTransferred = (ULONG)Iosb.Information;

exit:
    info(L"LEAVE:%08lX=LfsWriteFile()", Result); // xsmolasses
    return Result;
}

NTSTATUS LfsQueryDirectoryFile(
    HANDLE Handle,
    PVOID Buffer,
    ULONG Length,
    FILE_INFORMATION_CLASS FileInformationClass,
    BOOLEAN ReturnSingleEntry,
    PWSTR FileName,
    BOOLEAN RestartScan,
    PULONG PBytesTransferred)
{
    info(L"ENTER:         LfsQueryDirectoryFile()"); // xsmolasses

    HANDLE Event;
    UNICODE_STRING Ufnm;
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Result;

    Event = LfsThreadEvent();
    if (0 == Event)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    if (0 != FileName)
        RtlInitUnicodeString(&Ufnm, FileName);

    Result = NtQueryDirectoryFile(
        Handle,
        Event,
        0,
        0,
        &Iosb,
        Buffer,
        Length,
        FileInformationClass,
        ReturnSingleEntry,
        0 != FileName ? &Ufnm : 0,
        RestartScan);
    if (STATUS_PENDING == Result)
    {
        WaitForSingleObject(Event, INFINITE);
        Result = Iosb.Status;
    }

    *PBytesTransferred = (ULONG)Iosb.Information;

exit:
    info(L"LEAVE:%08lX=LfsQueryDirectoryFile()", Result); // xsmolasses
    return Result;
}

NTSTATUS LfsFsControlFile(
    HANDLE Handle,
    ULONG FsControlCode,
    PVOID InputBuffer,
    ULONG InputBufferLength,
    PVOID OutputBuffer,
    ULONG OutputBufferLength,
    PULONG PBytesTransferred)
{
    info(L"ENTER:         LfsFsControlFile()"); // xsmolasses

    HANDLE Event;
    IO_STATUS_BLOCK Iosb;
    NTSTATUS Result;

    Event = LfsThreadEvent();
    if (0 == Event)
    {
        Result = STATUS_INSUFFICIENT_RESOURCES;
        goto exit;
    }

    Result = NtFsControlFile(
        Handle,
        Event,
        0,
        0,
        &Iosb,
        FsControlCode,
        InputBuffer,
        InputBufferLength,
        OutputBuffer,
        OutputBufferLength);
    if (STATUS_PENDING == Result)
    {
        WaitForSingleObject(Event, INFINITE);
        Result = Iosb.Status;
    }

    *PBytesTransferred = (ULONG)Iosb.Information;

exit:
    info(L"LEAVE:%08lX=LfsFsControlFile()", Result); // xsmolasses
    return Result;
}
