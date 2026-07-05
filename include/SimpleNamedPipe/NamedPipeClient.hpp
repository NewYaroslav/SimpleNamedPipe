#pragma once
#ifndef SIMPLE_NAMED_PIPE_HEADER_SIMPLENAMEDPIPE_NAMEDPIPECLIENT_HPP_INCLUDED
#define SIMPLE_NAMED_PIPE_HEADER_SIMPLENAMEDPIPE_NAMEDPIPECLIENT_HPP_INCLUDED

/// \file NamedPipeClient.hpp
/// \brief Synchronous named pipe client for tests and lightweight integrations.

#include "NamedPipeClient/ClientConfig.hpp"
#include "NamedPipeServer/errors.hpp"

#include <windows.h>
#include <atomic>
#include <chrono>
#include <functional>
#include <mutex>
#include <string>
#include <system_error>
#include <vector>

namespace SimpleNamedPipe {

    /// \class NamedPipeClient
    /// \brief Synchronous client for message-mode Windows named pipes.
    class NamedPipeClient final {
    public:
        /// \brief Construct client with default configuration.
        NamedPipeClient();

        /// \brief Construct client with explicit configuration.
        /// \param config Configuration to use.
        explicit NamedPipeClient(const ClientConfig& config);

        /// \brief Destructor closes the pipe handle if it is open.
        ~NamedPipeClient();

        NamedPipeClient(const NamedPipeClient&) = delete;
        NamedPipeClient& operator=(const NamedPipeClient&) = delete;

        /// \brief Applies a new client configuration.
        /// \param config Configuration to use for subsequent connect calls.
        void set_config(const ClientConfig& config);

        /// \brief Returns current client configuration.
        /// \return Copy of the current configuration.
        ClientConfig get_config() const;

        /// \brief Opens the pipe configured in ClientConfig.
        /// \param error Optional output error code.
        /// \return true on success.
        bool connect(std::error_code* error = nullptr);

        /// \brief Opens the named pipe and stores that name in the configuration.
        /// \param pipe_name Pipe name or full \\.\pipe\ path.
        /// \param error Optional output error code.
        /// \return true on success.
        bool open(const std::string& pipe_name, std::error_code* error = nullptr);

        /// \brief Closes the pipe handle.
        void close();

        /// \brief Returns true while a pipe handle is open.
        bool is_connected() const;

        /// \brief Alias for is_connected().
        bool connected() const;

        /// \brief Writes a complete message to the pipe.
        /// \param message UTF-8 message.
        /// \param error Optional output error code.
        /// \return true when the message was fully written.
        bool write(const std::string& message, std::error_code* error = nullptr);

        /// \brief Reads one complete message, blocking until data is available.
        /// \param message Output message.
        /// \param error Optional output error code.
        /// \return true when a message was read.
        bool read(std::string& message, std::error_code* error = nullptr);

        /// \brief Reads one message if bytes are already available.
        /// \param message Output message.
        /// \param error Optional output error code.
        /// \return true when a message was read, false when no message is available or on error.
        bool try_read(std::string& message, std::error_code* error = nullptr);

        /// \brief Waits for one message up to timeout_ms and reads it.
        /// \param message Output message.
        /// \param timeout_ms Timeout in milliseconds.
        /// \param error Optional output error code.
        /// \return true when a message was read.
        bool read(std::string& message, size_t timeout_ms, std::error_code* error = nullptr);

        /// \brief Returns number of bytes currently available in the pipe.
        /// \param error Optional output error code.
        /// \return Available byte count, or 0 when disconnected or on error.
        size_t available(std::error_code* error = nullptr);

        /// \brief Flushes pipe write buffers.
        /// \param error Optional output error code.
        /// \return true on success.
        bool flush(std::error_code* error = nullptr);

        /// \brief Returns native Windows pipe handle.
        HANDLE native_handle() const;

        std::function<void()> on_connected;                       ///< Called after a successful connect.
        std::function<void()> on_disconnected;                    ///< Called after close of an active connection.
        std::function<void(const std::string&)> on_message;       ///< Called by try_read/read timeout overloads.
        std::function<void(const std::error_code&)> on_error;     ///< Called when an operation fails.

    private:
        ClientConfig m_config;
        mutable std::mutex m_mutex;
        HANDLE m_pipe = INVALID_HANDLE_VALUE;
        std::atomic<bool> m_is_connected{false};

        static std::wstring make_pipe_path(const std::string& pipe_name);
        static DWORD to_dword_timeout(size_t timeout_ms);

        bool connect_no_lock(std::error_code* error);
        bool read_no_lock(std::string& message, std::error_code* error, bool* disconnected);
        bool close_no_lock();
        bool validate_config_no_lock(std::error_code* error) const;
        void set_error(std::error_code* out, const std::error_code& error) const;
        void clear_error(std::error_code* out) const;
    };

} // namespace SimpleNamedPipe

/// \note Implementation is included only in header-only mode.
///       When building as a static library, do NOT include the .ipp here.
#ifndef SIMPLE_NAMED_PIPE_STATIC_LIB
#include "NamedPipeClient/NamedPipeClient.ipp"
#endif

#endif // SIMPLE_NAMED_PIPE_HEADER_SIMPLENAMEDPIPE_NAMEDPIPECLIENT_HPP_INCLUDED
