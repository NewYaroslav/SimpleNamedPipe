#pragma once
#ifndef _SIMPLE_NAMED_PIPE_SERVER_ERRORS_HPP_INCLUDED
#define _SIMPLE_NAMED_PIPE_SERVER_ERRORS_HPP_INCLUDED

/// \file errors.hpp
/// \brief Error codes and category for NamedPipeServer

#include <system_error>

namespace SimpleNamedPipe {

    /// \brief Error codes for NamedPipeServer operations.
    enum class NamedPipeErrc {
        ClientIndexOutOfRange,           ///< Client index exceeds MAX_CLIENTS
        InvalidPipeHandle,               ///< Pipe handle is invalid
        IoCompletionPortCreateFailed,    ///< Failed to create IO completion port
        NamedPipeCreateFailed,           ///< Failed to create named pipe
        NotConnected,                    ///< Operation attempted on a disconnected client
        ServerStopped,                   ///< Operation aborted because the server is stopping or stopped
        MessageTooLarge,                 ///< The message exceeds the allowed maximum size
        QueueFull,                       ///< The per-client write queue is full
        UnknownSystemError               ///< Fallback for unexpected system errors
    };

    /// \brief Error category for NamedPipeErrc.
    class NamedPipeErrCategory : public std::error_category {
    public:
        const char* name() const noexcept override {
            return "NamedPipe";
        }

        std::string message(int ev) const override {
            switch (static_cast<NamedPipeErrc>(ev)) {
            case NamedPipeErrc::ClientIndexOutOfRange:
                return "Client index out of range";
            case NamedPipeErrc::InvalidPipeHandle:
                return "Invalid pipe handle";
            case NamedPipeErrc::IoCompletionPortCreateFailed:
                return "Failed to create IO Completion Port";
            case NamedPipeErrc::NamedPipeCreateFailed:
                return "Failed to create named pipe";
            case NamedPipeErrc::NotConnected:
                return "Client is not connected";
            case NamedPipeErrc::ServerStopped:
                return "Server has been stopped";
            case NamedPipeErrc::MessageTooLarge:
                return "Message size exceeds the maximum allowed";
            case NamedPipeErrc::QueueFull:
                return "Per-client write queue is full";
            case NamedPipeErrc::UnknownSystemError:
                return "Unknown system error";
            default:
                return "Unknown error";
            }
        }
    };

    /// \brief Returns a reference to the NamedPipe error category singleton.
    inline const std::error_category& named_pipe_category() {
        static NamedPipeErrCategory instance;
        return instance;
    }

    /// \brief Constructs a std::error_code from a NamedPipeErrc value.
    inline std::error_code make_error_code(NamedPipeErrc e) {
        return { static_cast<int>(e), named_pipe_category() };
    }

} // namespace SimpleNamedPipe

// Tell the STL this is a valid error_code enum
namespace std {
    template <>
    struct is_error_code_enum<SimpleNamedPipe::NamedPipeErrc> : true_type {};
}

#endif // _SIMPLE_NAMED_PIPE_SERVER_ERRORS_HPP_INCLUDED
