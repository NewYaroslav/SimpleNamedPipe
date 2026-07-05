#pragma once
#ifndef SIMPLE_NAMED_PIPE_HEADER_SIMPLENAMEDPIPE_NAMEDPIPECLIENT_CLIENTCONFIG_HPP_INCLUDED
#define SIMPLE_NAMED_PIPE_HEADER_SIMPLENAMEDPIPE_NAMEDPIPECLIENT_CLIENTCONFIG_HPP_INCLUDED

/// \file ClientConfig.hpp
/// \brief Configuration for the named pipe client.

#include <cstddef>
#include <string>

namespace SimpleNamedPipe {

    /// \class ClientConfig
    /// \brief Named pipe client configuration.
    class ClientConfig {
    public:
        std::string pipe_name;   ///< Named pipe name or full \\.\pipe\ path.
        size_t      buffer_size; ///< Size of I/O buffers.
        size_t      timeout;     ///< Connect/read polling timeout in milliseconds.

        /// \brief Construct with optional parameters.
        /// \param pipe_name Name of the pipe or full pipe path.
        /// \param buffer_size Buffer size in bytes.
        /// \param timeout Wait timeout in milliseconds.
        ClientConfig(
                const std::string& pipe_name = "server",
                size_t buffer_size = 65536,
                size_t timeout = 5000)
            : pipe_name(pipe_name),
              buffer_size(buffer_size),
              timeout(timeout) {}
    };

} // namespace SimpleNamedPipe

#endif // SIMPLE_NAMED_PIPE_HEADER_SIMPLENAMEDPIPE_NAMEDPIPECLIENT_CLIENTCONFIG_HPP_INCLUDED
