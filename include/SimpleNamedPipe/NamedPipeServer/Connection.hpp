#pragma once
#ifndef _SIMPLE_NAMED_PIPE_CONNECTION_HPP_INCLUDED
#define _SIMPLE_NAMED_PIPE_CONNECTION_HPP_INCLUDED

/// \file Connection.hpp
/// \brief Defines the Connection class for managing individual named pipe connections.

#include <mutex>
#include <atomic>

namespace SimpleNamedPipe {

    /// \class Connection
    /// \brief Lightweight wrapper for client-side operations.
    class Connection {
    public:
        using DoneCallback = std::function<void(const std::error_code&)>;

        /// \brief Construct connection helper for a client.
        /// \param client_id Identifier of the client.
        /// \param impl Pointer to the server implementation.
        Connection(int client_id, IConnection* impl)
            : m_client_id(client_id), m_impl(impl), m_alive(true) {}

        /// \brief Send message through this connection.
        /// \param message UTF-8 encoded string to send.
        /// \param on_done Optional completion callback.
        void send(const std::string& message, DoneCallback on_done = nullptr) const {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (is_alive()) {
                if (m_impl) m_impl->send_to(m_client_id, message, std::move(on_done));
            } else if (on_done) {
                lock.unlock();
                on_done(make_error_code(NamedPipeErrc::NotConnected));
            }
        }

        /// \brief Close this connection.
        /// \param on_done Optional completion callback.
        void close(DoneCallback on_done = nullptr) const {
            std::unique_lock<std::mutex> lock(m_mutex);
            if (is_alive()) {
                if (m_impl) m_impl->close(m_client_id, std::move(on_done));
            } else if (on_done) {
                lock.unlock();
                on_done(make_error_code(NamedPipeErrc::NotConnected));
            }
        }

        /// \brief Check whether the underlying client is connected.
        bool is_connected() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!is_alive()) return false;
            return m_impl && m_impl->is_connected(m_client_id);
        }

        /// \brief Retrieve the associated client identifier.
        int client_id() const {
            return m_client_id;
        }

        /// \brief Mark this connection as no longer valid.
        void invalidate() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_alive.store(false, std::memory_order_release);
        }

        /// \brief Determine if this wrapper is still active.
        bool is_alive() const {
            return m_alive.load(std::memory_order_acquire);
        }

    private:
        int m_client_id;
        IConnection* m_impl;
        mutable std::mutex m_mutex;
        std::atomic<bool> m_alive;
    };

} // namespace SimpleNamedPipe

#endif // _SIMPLE_NAMED_PIPE_CONNECTION_HPP_INCLUDED
