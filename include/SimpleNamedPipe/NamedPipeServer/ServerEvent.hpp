#pragma once
#ifndef _SIMPLE_NAMED_PIPE_SERVER_EVENT_HPP_INCLUDED
#define _SIMPLE_NAMED_PIPE_SERVER_EVENT_HPP_INCLUDED

#include <system_error>
#include <string>
#include <memory>

namespace SimpleNamedPipe {

    class Connection;

    /// \brief Type of server-side event.
    enum class ServerEventType {
        ServerStarted,
        ServerStopped,
        ClientConnected,
        ClientDisconnected,
        MessageReceived,
        ErrorOccurred
    };

    /// \brief Lightweight event descriptor for callbacks.
    class ServerEvent {
    public:
        ServerEventType type;                        ///< Event type
        int client_id = -1;                          ///< Index of the client (always available)
        std::shared_ptr<Connection> connection;      ///< Optional connection wrapper
        std::string message;                         ///< Message buffer (for Message events)
        std::error_code error;                       ///< Error info (for Error and Close events)

        // --- Constructors ---
        ServerEvent(ServerEventType type)
            : type(type) {}

        ServerEvent(ServerEventType type, int client_id)
            : type(type), client_id(client_id) {}

        ServerEvent(ServerEventType type, int client_id, std::shared_ptr<Connection> conn)
            : type(type), client_id(client_id), connection(std::move(conn)) {}

        ServerEvent(ServerEventType type, int client_id, std::shared_ptr<Connection> conn, std::string&& msg)
            : type(type), client_id(client_id), connection(std::move(conn)), message(std::move(msg)) {}

        ServerEvent(ServerEventType type, int client_id, std::shared_ptr<Connection> conn, const std::error_code& ec)
            : type(type), client_id(client_id), connection(std::move(conn)), error(ec) {}

        ServerEvent(ServerEventType type, const std::error_code& ec)
            : type(type), error(ec) {}

        // --- Static factory methods ---
        static inline ServerEvent server_started() {
            return ServerEvent(ServerEventType::ServerStarted);
        }

        static inline ServerEvent server_stopped() {
            return ServerEvent(ServerEventType::ServerStopped);
        }

        static inline ServerEvent client_connected(int id, std::shared_ptr<Connection> conn) {
            return ServerEvent(ServerEventType::ClientConnected, id, std::move(conn));
        }

        static inline ServerEvent client_disconnected(int id, std::shared_ptr<Connection> conn, const std::error_code& ec) {
            return ServerEvent(ServerEventType::ClientDisconnected, id, std::move(conn), ec);
        }

        static inline ServerEvent message_received(int id, std::shared_ptr<Connection> conn, std::string&& msg) {
            return ServerEvent(ServerEventType::MessageReceived, id, std::move(conn), std::move(msg));
        }

        static inline ServerEvent error_occurred(const std::error_code& ec) {
            return ServerEvent(ServerEventType::ErrorOccurred, ec);
        }
    };

} // namespace SimpleNamedPipe

#endif // _SIMPLE_NAMED_PIPE_SERVER_EVENT_HPP_INCLUDED
