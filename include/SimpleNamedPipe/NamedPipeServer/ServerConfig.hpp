#pragma once
#ifndef _SIMPLE_NAMED_PIPE_SERVER_CONFIG_HPP_INCLUDED
#define _SIMPLE_NAMED_PIPE_SERVER_CONFIG_HPP_INCLUDED

/// \file ServerConfig.hpp
/// \brief Конфигурация сервера именованных каналов.

#include <string>

namespace SimpleNamedPipe {

    /// \struct WriteQueueLimits
    /// \brief Ограничения на очередь записи, включая лимиты на количество сообщений, размер сообщений и общий объём.
    struct WriteQueueLimits {
        size_t max_pending_writes_per_client = 1000;           ///< Макс. сообщений в очереди на клиента
        size_t max_message_size = 64 * 1024;                   ///< Макс. размер одного сообщения (64 KB)
        size_t max_total_queue_memory = 100 * 1024 * 1024;     ///< Макс. общий объём сообщений (100 MB)
    };

    /// \class Config
    /// \brief Конфигурация сервера именованных каналов.
    class ServerConfig {
    public:
        std::string      pipe_name;    ///< Имя именованного канала
        WriteQueueLimits write_limits; ///< Лимиты очереди записи
        size_t           buffer_size;  ///< Размер буфера для чтения и записи
        size_t           timeout;      ///< Время ожидания в миллисекундах

        /// \brief Конструктор с параметрами.
        /// \param pipe_name Имя канала.
        /// \param buffer_size Размер буфера.
        /// \param timeout Таймаут в мс.
        ServerConfig(const std::string& pipe_name = "server",
               size_t buffer_size = 65536,
               size_t timeout = 50)
            : pipe_name(pipe_name), buffer_size(buffer_size), timeout(timeout) {}
    };
}

#endif // _SIMPLE_NAMED_PIPE_SERVER_CONFIG_HPP_INCLUDED
