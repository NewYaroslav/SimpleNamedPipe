#pragma once
#ifndef _SIMPLE_NAMED_PIPE_SERVER_CONFIG_HPP_INCLUDED
#define _SIMPLE_NAMED_PIPE_SERVER_CONFIG_HPP_INCLUDED

/// \file ServerConfig.hpp
/// \brief Configuration for the named pipe server.

#include <string>

namespace SimpleNamedPipe {

    /// \struct WriteQueueLimits
    /// \brief Limits for the write queue, including message count and size restrictions.
    struct WriteQueueLimits {
        size_t max_pending_writes_per_client = 1000;           ///< Max messages queued per client
        size_t max_message_size = 64 * 1024;                   ///< Max single message size (64 KB)
        size_t max_total_queue_memory = 100 * 1024 * 1024;     ///< Max total queued size (100 MB)
    };

    /// \class ServerConfig
    /// \brief Named pipe server configuration.
    class ServerConfig {
    public:
        std::string      pipe_name;    ///< Named pipe name
        WriteQueueLimits write_limits; ///< Limits for the write queue
        size_t           buffer_size;  ///< Size of I/O buffers
        size_t           timeout;      ///< Timeout in milliseconds

        /// \brief Construct with optional parameters.
        /// \param pipe_name Name of the pipe.
        /// \param buffer_size Buffer size in bytes.
        /// \param timeout Wait timeout in ms.
        ServerConfig(const std::string& pipe_name = "server",
               size_t buffer_size = 65536,
               size_t timeout = 50)
            : pipe_name(pipe_name), buffer_size(buffer_size), timeout(timeout) {}
    };
}

#endif // _SIMPLE_NAMED_PIPE_SERVER_CONFIG_HPP_INCLUDED
