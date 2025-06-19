#pragma once
#ifndef _SIMLPE_NAMED_PIPE_SERVER_ICONNECTION_HPP_INCLUDED
#define _SIMLPE_NAMED_PIPE_SERVER_ICONNECTION_HPP_INCLUDED

#include <functional>

namespace SimpleNamedPipe {

    class IConnection {
    public:
        using DoneCallback = std::function<void(const std::error_code&)>;

        virtual ~IConnection() = default;

        /// \brief
        /// \param client_id
        /// \param message
        /// \param on_done
        virtual void send_to(int client_id, const std::string& message, DoneCallback on_done) = 0;

        /// \brief
        /// \param client_id
        /// \param on_done
        virtual void close(int client_id, DoneCallback on_done) = 0;

        /// \brief Checks whether a client is currently connected.
        /// \param client_id ID of the client.
        /// \return true if connected; false otherwise.
        virtual bool is_connected(int client_id) const = 0;
    };

};

#endif // _SIMLPE_NAMED_PIPE_SERVER_ICONNECTION_HPP_INCLUDED
