#pragma once
#ifndef _SIMPLE_NAMED_PIPE_SERVER_ICONNECTION_HPP_INCLUDED
#define _SIMPLE_NAMED_PIPE_SERVER_ICONNECTION_HPP_INCLUDED

#include <functional>

namespace SimpleNamedPipe {

    class IConnection {
    public:
        using DoneCallback = std::function<void(const std::error_code&)>;

        virtual ~IConnection() = default;

        /// \brief Send a message to a specific client.
        /// \param client_id Identifier of the client.
        /// \param message   UTF-8 message to send.
        /// \param on_done   Optional completion callback.
        virtual void send_to(int client_id, const std::string& message, DoneCallback on_done) = 0;

        /// \brief Close connection with the given client.
        /// \param client_id Identifier of the client.
        /// \param on_done   Optional completion callback.
        virtual void close(int client_id, DoneCallback on_done) = 0;

        /// \brief Checks whether a client is currently connected.
        /// \param client_id ID of the client.
        /// \return true if connected; false otherwise.
        virtual bool is_connected(int client_id) const = 0;
    };

};

#endif // _SIMPLE_NAMED_PIPE_SERVER_ICONNECTION_HPP_INCLUDED
