#pragma once
#ifndef _SIMLPE_UTILS_HPP_INCLUDED
#define _SIMLPE_UTILS_HPP_INCLUDED

/// \file Utils.hpp
/// \brief

#include "Config.hpp"
#include <windows.h>

namespace SimpleNamedPipe {

    HANDLE create_named_pipe(HANDLE completion_port, const Config& config) {
        std::string pipe_name = "\\\\.\\pipe\\" + config.pipe_name;

        HANDLE pipe = CreateNamedPipeA(
            pipe_name.c_str(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            static_cast<DWORD>(config.buffer_size),
            static_cast<DWORD>(config.buffer_size),
            static_cast<DWORD>(config.timeout),
            NULL);

        // Критическая ошибка: не удалось создать именованный канал
        if (pipe == INVALID_HANDLE_VALUE) {
            throw std::runtime_error("Failed to create named pipe. Error: " + std::to_string(GetLastError()));
        }

        // Привязываем дескриптор канала к Completion Port
        if (!CreateIoCompletionPort(pipe, completion_port, reinterpret_cast<ULONG_PTR>(pipe), 0)) {
            CloseHandle(pipe);
            throw std::runtime_error("Failed to associate pipe with Completion Port. Error: " + std::to_string(GetLastError()));
        }
        return pipe;
    }

}; // namespace SimpleNamedPipe

#endif // _SIMLPE_UTILS_HPP_INCLUDED
