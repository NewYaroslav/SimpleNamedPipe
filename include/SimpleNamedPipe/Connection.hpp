#pragma once
#ifndef _SIMPLE_NAMED_PIPE_CONNECTION_HPP_INCLUDED
#define _SIMPLE_NAMED_PIPE_CONNECTION_HPP_INCLUDED

/// \file Connection.hpp
/// \brief Defines the Connection class for managing individual named pipe connections.

#include <windows.h>
#include <functional>
#include <vector>
#include <string>
#include <system_error>
#include <memory>
#include <queue>
#include <mutex>
#include <atomic>

#include <iostream>

namespace SimpleNamedPipe {

    class NamedPipeServer; // Forward declaration

    /// \class Connection
    /// \brief Represents a single connection to a named pipe.
    class Connection : public std::enable_shared_from_this<Connection> {
    public:
        using connection_t = std::shared_ptr<Connection>;
        using on_message_t = std::function<void(connection_t, const std::string&)>;
        using on_error_t   = std::function<void(connection_t, const std::error_code&)>;
        using on_close_t   = std::function<void(connection_t)>;
        using on_send_t    = std::function<void(connection_t, const std::error_code&)>;


        /// \brief Constructor for the Connection class.
        /// \param pipe The HANDLE to the named pipe.
        /// \param buffer_size The size of the buffer for reading messages.
        /// \param on_message Callback for receiving messages.
        /// \param on_close Callback for handling connection closure.
        /// \param on_error Callback for handling errors.
        Connection(
            HANDLE pipe,
            size_t buffer_size,
            on_message_t on_message,
            on_close_t on_close,
            on_error_t on_error)
            : m_pipe(pipe),
              m_buffer(buffer_size),
              m_on_message(std::move(on_message)),
              m_on_close(std::move(on_close)),
              m_on_error(std::move(on_error)),
              m_connection_closed(false),
              m_pending_close(false),
              m_write_in_progress(false) {
            memset(&m_overlapped, 0, sizeof(OVERLAPPED));
            memset(&m_write_overlapped, 0, sizeof(OVERLAPPED));
        }

        /// \brief Destructor for the Connection class.
        ~Connection() {}

        const HANDLE get_pipe() {
            return m_pipe;
        }

        /// \brief Queues a message for sending.
        /// \param message The message to send.
        /// \param on_send Callback for send completion or error.
        void send(const std::string& message, on_send_t on_send = nullptr) {
            if (m_connection_closed) return;
            std::lock_guard<std::mutex> lock(m_send_mutex);
            m_pending_send_buffer.push({message, std::move(on_send)});
        }

        /// \brief Closes the connection and releases associated resources.
        void close() {
            m_pending_close = true;
        }

    protected:

        /// \brief Starts reading data from the pipe.
        void start_read() {
            if (m_connection_closed) return;
            DWORD bytes_read = 0;
            memset(&m_overlapped, 0, sizeof(OVERLAPPED));
            BOOL success = ReadFile(m_pipe, m_buffer.data(), static_cast<DWORD>(m_buffer.size()), &bytes_read, &m_overlapped);
            DWORD error = GetLastError();
            if (!success && error != ERROR_IO_PENDING && error != ERROR_IO_INCOMPLETE) {
                report_error(std::error_code(error, std::system_category()));
            }
        }

        /// \brief Completes the read operation and processes the received data.
        void complete_read() {
            if (m_connection_closed) return;
            DWORD bytes_read = 0;
            if (GetOverlappedResult(m_pipe, &m_overlapped, &bytes_read, FALSE)) {
                handle_read(bytes_read);
                start_read();
            } else {
                DWORD error = GetLastError();
                if (error == ERROR_BROKEN_PIPE ||
                    error == ERROR_PIPE_NOT_CONNECTED ||
                    error == ERROR_PIPE_CONNECTED) {
                    handle_close();
                } else
                if (error != ERROR_IO_INCOMPLETE) {
                    report_error(std::error_code(error, std::system_category()));
                }
            }
        }

        /// \brief Processes the send queue and sends messages.
        void process_send_queue() {
            if (m_connection_closed) return;

            if (m_pending_close) {
                handle_close();
                return;
            }

            for (;;) {
                if (is_write_complete()) {
                    if (!m_send_buffer.empty()) {
                        auto callback = m_send_buffer.front().second;
                        m_send_buffer.pop();
                        if (callback) callback(shared_from_this(), {});
                    }
                } else {
                    break;
                }

                std::unique_lock<std::mutex> lock(m_send_mutex);
                if (m_pending_send_buffer.empty()) break;
                auto buffer = m_pending_send_buffer.front();
                auto& message = buffer.first;
                auto& callback = buffer.second;
                lock.unlock();

                m_write_in_progress = true;
                DWORD bytes_written = 0;
                memset(&m_write_overlapped, 0, sizeof(OVERLAPPED));
                BOOL success = WriteFile(m_pipe, message.data(), static_cast<DWORD>(message.size()), &bytes_written, &m_write_overlapped);

                if (!success) {
                    DWORD error = GetLastError();
                    if (error == ERROR_IO_PENDING) {
                        m_send_buffer.push(buffer);
                        lock.lock();
                        m_pending_send_buffer.pop();
                        lock.unlock();
                        continue;
                    } else
                    if (error == ERROR_BROKEN_PIPE ||
                        error == ERROR_PIPE_NOT_CONNECTED ||
                        error == ERROR_PIPE_CONNECTED ||
                        error == ERROR_NO_DATA) {
                        handle_close();
                        m_write_in_progress = false;
                        break;
                    } else
                    if (error == ERROR_IO_INCOMPLETE) {
                        break;
                    }
                    if (callback) callback(shared_from_this(), std::error_code(error, std::system_category()));
                    m_write_in_progress = false;
                    lock.lock();
                    m_pending_send_buffer.pop();
                    lock.unlock();
                    break;
                } else {
                    if (callback) callback(shared_from_this(), {});
                    m_write_in_progress = false;
                    lock.lock();
                    m_pending_send_buffer.pop();
                    lock.unlock();
                }
            }
        }

        /// \brief Checks if the connection is closed.
        /// \return True if the connection is closed, false otherwise.
        bool is_closed() const {
            return m_connection_closed;
        }

        friend class NamedPipeServer;

    private:

        HANDLE            m_pipe;       ///< Handle to the named pipe.
        OVERLAPPED        m_overlapped; ///< Overlapped structure for asynchronous I/O.
        OVERLAPPED        m_write_overlapped;///< OVERLAPPED для записи
        std::vector<char> m_buffer;     ///< Buffer for reading data.
        on_message_t      m_on_message; ///< Callback for message reception.
        on_close_t        m_on_close;   ///< Callback for connection closure.
        on_error_t        m_on_error;   ///< Callback for error handling.
        std::mutex        m_send_mutex; ///< Mutex for synchronizing access to the send queue.
        using send_data_t = std::pair<std::string, on_send_t>;
        std::queue<send_data_t> m_pending_send_buffer; ///< Queue for messages to send.
        std::queue<send_data_t> m_send_buffer; ///< Queue for messages to send.
        std::atomic<bool> m_connection_closed; ///< Indicates if the connection is closed.
        std::atomic<bool> m_pending_close;    ///< Indicates if the connection is pending closure.
        std::atomic<bool> m_write_in_progress;

        bool is_write_complete() {
            if (!m_write_in_progress) return true;

            DWORD bytes_written = 0;
            BOOL result = GetOverlappedResult(m_pipe, &m_write_overlapped, &bytes_written, FALSE);

            if (result) {
                m_write_in_progress = false;
                return true;
            }

            DWORD error = GetLastError();
            if (error == ERROR_IO_INCOMPLETE) {
                return false;
            }

            m_write_in_progress = false;
            return true;
        }

        /// \brief Closes the pipe and invokes the close callback.
        void close_pipe() {
            if (m_connection_closed) return;
            if (m_pipe != INVALID_HANDLE_VALUE) {
                DisconnectNamedPipe(m_pipe);
                CancelIoEx(m_pipe, &m_overlapped);
                CancelIoEx(m_pipe, &m_write_overlapped);
                //CloseHandle(m_pipe);
               // m_pipe = INVALID_HANDLE_VALUE;
            }
            m_connection_closed = true;
        }

        /// \brief Cancels all pending sends in the queue
        void cancel_send_queue() {
            while (!m_send_buffer.empty()) {
                auto& callback = m_send_buffer.front().second;
                if (callback) callback(shared_from_this(), std::error_code(ERROR_OPERATION_ABORTED, std::system_category()));
                m_send_buffer.pop();
            }
            std::unique_lock<std::mutex> lock(m_send_mutex);
            while (!m_pending_send_buffer.empty()) {
                lock.unlock();
                auto& callback = m_pending_send_buffer.front().second;
                if (callback) callback(shared_from_this(), std::error_code(ERROR_OPERATION_ABORTED, std::system_category()));
                lock.lock();
                m_pending_send_buffer.pop();
            }
        }

        void handle_close() {
            if (m_connection_closed) return;
            if (m_on_close) {
                m_on_close(shared_from_this());
            }
            cancel_send_queue();
            close_pipe();
        }

        /// \brief Handles the read operation and invokes the message callback.
        /// \param bytes_read The number of bytes read from the pipe.
        void handle_read(DWORD bytes_read) {
            if (bytes_read > 0) {
                std::string message(m_buffer.begin(), m_buffer.begin() + bytes_read);
                if (m_on_message) {
                    m_on_message(shared_from_this(), message);
                }
            }
        }

        /// \brief Reports an error via the error callback.
        void report_error(const std::error_code& error) {
            if (m_on_error) {
                m_on_error(shared_from_this(), error);
            }
        }

    }; // Connection

} // namespace SimpleNamedPipe

#endif // _SIMPLE_NAMED_PIPE_CONNECTION_HPP_INCLUDED
