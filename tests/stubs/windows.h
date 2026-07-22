#pragma once
#include <cstdint>
#include <cstddef>
#include <cstdarg>

using HANDLE = void*;
using DWORD = std::uint32_t;
using ULONG_PTR = std::uintptr_t;
using BOOL = int;
using LPVOID = void*;
using WORD = std::uint16_t;
using DWORD_PTR = std::uintptr_t;
using LPDWORD = DWORD*;
using PULONG_PTR = ULONG_PTR*;

struct OVERLAPPED {
    ULONG_PTR Internal;
    ULONG_PTR InternalHigh;
    union {
        struct {
            DWORD Offset;
            DWORD OffsetHigh;
        } DUMMYSTRUCTNAME;
        LPVOID Pointer;
    } DUMMYUNIONNAME;
    HANDLE hEvent;
};

struct SECURITY_ATTRIBUTES {
    DWORD nLength;
    LPVOID lpSecurityDescriptor;
    BOOL bInheritHandle;
};

struct FILETIME {
    DWORD dwLowDateTime;
    DWORD dwHighDateTime;
};

using LPOVERLAPPED = OVERLAPPED*;

#define INVALID_HANDLE_VALUE reinterpret_cast<HANDLE>(-1)

#define PIPE_ACCESS_DUPLEX 0x00000003
#define FILE_FLAG_OVERLAPPED 0x40000000
#define PIPE_TYPE_MESSAGE 0x00000004
#define PIPE_READMODE_MESSAGE 0x00000002
#define PIPE_WAIT 0x00000000
#define PIPE_UNLIMITED_INSTANCES 255
#define PIPE_REJECT_REMOTE_CLIENTS 0x00000008
#define NMPWAIT_USE_DEFAULT_WAIT 0x00000000
#define GENERIC_READ 0x80000000
#define GENERIC_WRITE 0x40000000
#define OPEN_EXISTING 3
#define FILE_ATTRIBUTE_NORMAL 0x00000080
#define ERROR_IO_PENDING 997
#define ERROR_IO_INCOMPLETE 996
#define ERROR_FILE_NOT_FOUND 2
#define ERROR_PIPE_BUSY 231
#define ERROR_PIPE_CONNECTED 535
#define ERROR_IO_PENDING 997
#define ERROR_PIPE_LISTENING 536
#define ERROR_OPERATION_ABORTED 995
#define ERROR_MORE_DATA 234
#define ERROR_BROKEN_PIPE 109
#define ERROR_PIPE_NOT_CONNECTED 233
#define ERROR_SUCCESS 0
#define ERROR_NO_DATA 232
#define INFINITE 0xFFFFFFFF
#define WAIT_TIMEOUT 258
#define WAIT_OBJECT_0 0
#define WAIT_FAILED 0xFFFFFFFF
#define CP_UTF8 65001
#define MB_ERR_INVALID_CHARS 0x00000008

inline DWORD GetLastError() { return 0; }
inline int MultiByteToWideChar(
        unsigned int,
        DWORD,
        const char* input,
        int input_size,
        wchar_t* output,
        int output_size) {
    if (!input || input_size < 0 || output_size < 0) {
        return 0;
    }

    if (!output) {
        return input_size;
    }

    const int count = input_size < output_size ? input_size : output_size;
    for (int i = 0; i < count; ++i) {
        output[i] = static_cast<unsigned char>(input[i]);
    }
    return count;
}
inline HANDLE CreateIoCompletionPort(HANDLE, HANDLE, ULONG_PTR, DWORD) { return reinterpret_cast<HANDLE>(1); }
inline BOOL PostQueuedCompletionStatus(HANDLE, DWORD, ULONG_PTR, LPOVERLAPPED) { return 1; }
inline HANDLE CreateNamedPipeW(const wchar_t*, DWORD, DWORD, DWORD, DWORD, DWORD, DWORD, const SECURITY_ATTRIBUTES*) { return reinterpret_cast<HANDLE>(1); }
inline HANDLE CreateFileW(const wchar_t*, DWORD, DWORD, const SECURITY_ATTRIBUTES*, DWORD, DWORD, HANDLE) { return reinterpret_cast<HANDLE>(1); }
inline BOOL WaitNamedPipeW(const wchar_t*, DWORD) { return 1; }
inline BOOL ConnectNamedPipe(HANDLE, LPOVERLAPPED) { return 1; }
inline BOOL DisconnectNamedPipe(HANDLE) { return 1; }
inline BOOL CloseHandle(HANDLE) { return 1; }
inline BOOL GetQueuedCompletionStatus(HANDLE, LPDWORD, PULONG_PTR, LPOVERLAPPED*, DWORD) { return 0; }
inline BOOL GetOverlappedResult(HANDLE, LPOVERLAPPED, LPDWORD, BOOL) { return 1; }
inline BOOL CancelIoEx(HANDLE, LPOVERLAPPED) { return 1; }
inline HANDLE CreateEventW(void*, BOOL, BOOL, const wchar_t*) { return reinterpret_cast<HANDLE>(1); }
inline BOOL SetEvent(HANDLE) { return 1; }
inline BOOL ResetEvent(HANDLE) { return 1; }
inline DWORD WaitForSingleObject(HANDLE, DWORD) { return WAIT_OBJECT_0; }
inline BOOL FlushFileBuffers(HANDLE) { return 1; }
inline BOOL GetNamedPipeClientProcessId(HANDLE, ULONG_PTR*) { return 1; }
inline BOOL GetNamedPipeClientSessionId(HANDLE, ULONG_PTR*) { return 1; }
inline BOOL GetFileInformationByHandle(HANDLE, void*) { return 1; }
inline BOOL ReadFile(HANDLE, void*, DWORD, LPDWORD, LPOVERLAPPED) { return 1; }
inline BOOL WriteFile(HANDLE, const void*, DWORD, LPDWORD, LPOVERLAPPED) { return 1; }
inline BOOL PeekNamedPipe(HANDLE, void*, DWORD, LPDWORD, LPDWORD, LPDWORD) { return 1; }
inline DWORD FormatMessageW(DWORD, const void*, DWORD, DWORD, wchar_t*, DWORD, va_list*) { return 0; }
inline DWORD QueueUserWorkItem(void(*)(LPVOID), LPVOID, ULONG_PTR) { return 0; }

inline DWORD SleepEx(DWORD, BOOL) { return 0; }

struct SYSTEM_INFO {
    union {
        DWORD dwOemId;
        struct {
            WORD wProcessorArchitecture;
            WORD wReserved;
        } DUMMYSTRUCTNAME;
    } DUMMYUNIONNAME;
    DWORD dwPageSize;
    LPVOID lpMinimumApplicationAddress;
    LPVOID lpMaximumApplicationAddress;
    DWORD_PTR dwActiveProcessorMask;
    DWORD dwNumberOfProcessors;
    DWORD dwProcessorType;
    DWORD dwAllocationGranularity;
    WORD wProcessorLevel;
    WORD wProcessorRevision;
};

inline void GetSystemInfo(SYSTEM_INFO*) {}

inline BOOL GetCommTimeouts(HANDLE, void*) { return 1; }
inline BOOL SetCommTimeouts(HANDLE, const void*) { return 1; }

inline BOOL SetNamedPipeHandleState(HANDLE, DWORD*, DWORD*, DWORD*) { return 1; }

#define CALLBACK
