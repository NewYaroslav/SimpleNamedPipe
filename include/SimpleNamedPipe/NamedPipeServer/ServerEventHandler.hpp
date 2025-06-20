#pragma once
#ifndef _SIMPLE_NAMED_PIPE_SERVER_EVENT_HANDLER_HPP_INCLUDED
#define _SIMPLE_NAMED_PIPE_SERVER_EVENT_HANDLER_HPP_INCLUDED

/// \file ServerEventHandler.hpp
/// \brief Base interface for handling server events.

namespace SimpleNamedPipe {

    /// \struct ServerEventHandler
    /// \brief Helper interface for reacting to server events.
    class ServerEventHandler {
    public:
        virtual ~ServerEventHandler() = default;

        virtual void on_connected(int client_id) {}
        virtual void on_disconnected(int client_id, const std::error_code& ec) {}
        virtual void on_message(int client_id, const std::string& message) {}
        virtual void on_start(const ServerConfig& config) {}
        virtual void on_stop(const ServerConfig& config) {}
        virtual void on_error(const std::error_code& ec) {}
    };

}; // namespace SimpleNamedPipe

#endif // _SIMPLE_NAMED_PIPE_SERVER_EVENT_HANDLER_HPP_INCLUDED
