#pragma once
#ifndef _SIMLPE_NAMED_PIPE_SERVER_HPP_INCLUDED
#define _SIMLPE_NAMED_PIPE_SERVER_HPP_INCLUDED

/// \file NamedPipeServer.hpp
/// \brief Server implementation for managing named pipes

#include "Config.hpp"
#include "Connection.hpp"
#include <windows.h>
#include <memory>
#include <list>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <iostream>

namespace SimpleNamedPipe {

    /// \class NamedPipeServer
    /// \brief Manages named pipe server connections and communication.
    class NamedPipeServer {
    public:
        using connection_t = std::shared_ptr<Connection>;

        std::function<void(connection_t)>                         on_open;          ///< Called when a client connects.
        std::function<void(connection_t, const std::string&)>     on_message;       ///< Called when a message is received.
        std::function<void(connection_t)>                         on_close;         ///< Called when a client disconnects.
        std::function<void(connection_t, const std::error_code&)> on_error;         ///< Called when an error occurs.
        std::function<void(const Config&)>                        on_server_start;  ///< Called when the server starts.
        std::function<void()>                                     on_server_close;  ///< Called when the server stops.
        std::function<void(const std::error_code&)>               on_server_error;  ///< Called when a server error occurs.

        /// \brief Default constructor.
        NamedPipeServer()
                : m_completion_port(nullptr),
                  m_named_pipe(nullptr), m_stop_server(false)  {
        };

        /// \brief Constructs the server with the specified configuration.
        /// \param config Configuration for the named pipe server.
        NamedPipeServer(Config& config)
                : m_config(config), m_completion_port(nullptr),
                  m_named_pipe(nullptr), m_stop_server(false) {
        }

         /// \brief Destructor.
        ~NamedPipeServer() {
            stop();
        }

        /// \brief Sets the server configuration.
        /// \param config
        void set_config(Config& config) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_config = config;
            //m_update_config = true;
        }

        /// \brief Sends a message to all connected clients.
        /// \param message
        void send_all(const std::string& message) {
            std::lock_guard<std::mutex> lock(m_connections_mutex);
            for (auto& connection : m_connections) {
                //connection->send(message);
                connection.second->send(message);
            }
        }

        size_t count_connections() const {
            std::lock_guard<std::mutex> lock(m_connections_mutex);
            return m_connections.size();
        }

        /// \brief Starts the server.
        /// \param run_in_thread
        void start(bool run_in_thread = true) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop_server = false;
            if (run_in_thread) {
                m_server_thread = std::thread(&NamedPipeServer::server_main_loop, this);
            } else {
                server_main_loop();
            }
        }

        /// \brief Stops the server.
        void stop() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop_server = true;
            if (m_server_thread.joinable()) {
                m_server_thread.join();
            }
        }

    private:
        std::mutex m_mutex;                        ///< Protects server configuration.
        Config m_config;                           ///< Server configuration.
        HANDLE m_completion_port;                  ///< Completion port for I/O operations.
        HANDLE m_named_pipe;                       ///< Handle for the current named pipe.
        mutable std::mutex m_connections_mutex;            ///< Protects client connections.
        std::unordered_map<HANDLE, connection_t> m_connections; ///< List of active connections.
        std::thread m_server_thread;               ///< Thread for server main loop.
        std::atomic<bool> m_stop_server;           ///< Indicates if the server is stopping.
        //std::atomic<bool> m_update_config;         ///<

        /// \brief Retrieves the current server configuration.
        Config get_config() {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_config;
        }

        /// \brief Creates a named pipe.
        /// \param config The configuration for the named pipe.
        void _create_named_pipe(const Config& config) {
            try {
                if (!m_named_pipe) {
                    std::cout << "-c0" << std::endl;
                    std::string pipe_name = "\\\\.\\pipe\\" + config.pipe_name;
                    m_named_pipe = CreateNamedPipeA(
                        pipe_name.c_str(),
                        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                        PIPE_UNLIMITED_INSTANCES,
                        static_cast<DWORD>(config.buffer_size),
                        static_cast<DWORD>(config.buffer_size),
                        static_cast<DWORD>(config.timeout),
                        NULL);

                    // Критическая ошибка: не удалось создать именованный канал
                    if (m_named_pipe == INVALID_HANDLE_VALUE) {
                        m_named_pipe = nullptr;
                        throw std::system_error(GetLastError(), std::system_category(), "Failed to create named pipe.");
                    }

                    auto connection = std::make_shared<Connection>(
                        m_named_pipe,
                        config.buffer_size,
                        on_message,
                        on_close,
                        on_error);

                    // Привязываем дескриптор канала к Completion Port
                    if (!CreateIoCompletionPort(m_named_pipe, m_completion_port, reinterpret_cast<ULONG_PTR>(connection->get_pipe()), 0)) {
                        CloseHandle(m_named_pipe);
                        m_named_pipe = nullptr;
                        throw std::system_error(GetLastError(), std::system_category(), "Failed to associate pipe with Completion Port.");
                    }

                    std::cout << "-c1" << std::endl;
                    m_connections[m_named_pipe] = connection;
                }

                std::unique_lock<std::mutex> lock(m_connections_mutex);
                auto it = m_connections.find(m_named_pipe);
                if (it == m_connections.end()) {
                    std::cout << "-c2" << std::endl;
                    lock.unlock();
                    CloseHandle(m_named_pipe);
                    m_named_pipe = nullptr;
                    throw std::system_error(GetLastError(), std::system_category(), "Failed to create named pipe.");
                }
                auto& connection = it->second;
                lock.unlock();

                // Подключаем клиента асинхронно
                OVERLAPPED overlapped = {};
                memset(&overlapped, 0, sizeof(OVERLAPPED));
                overlapped.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr); // Создаём событие сброса вручную
                if (!overlapped.hEvent) {
                    std::cout << "-c3" << std::endl;
                    lock.lock();
                    m_connections.erase(it);
                    lock.unlock();
                    CloseHandle(m_named_pipe);
                    m_named_pipe = nullptr;
                    throw std::system_error(GetLastError(), std::system_category(), "Failed to create event for overlapped structure.");
                }

                BOOL result = ConnectNamedPipe(m_named_pipe, &overlapped);
                DWORD error = GetLastError();

                // Если клиент уже подключен (ERROR_PIPE_CONNECTED), немедленно обрабатываем подключение
                if (error == ERROR_PIPE_CONNECTED) {
                    std::cout << "-c4" << std::endl;
                    // Постим завершение операции вручную
                    if (!PostQueuedCompletionStatus(m_completion_port, 0, reinterpret_cast<ULONG_PTR>(connection->get_pipe()), &overlapped)) {
                        std::cout << "-c5" << std::endl;
                        lock.lock();
                        m_connections.erase(it);
                        lock.unlock();

                        CloseHandle(overlapped.hEvent); // Очищаем событие
                        CloseHandle(m_named_pipe);
                        m_named_pipe = nullptr;
                        throw std::system_error(error, std::system_category(), "Failed to post completion status.");
                    }
                } else
                if (!result) {
                    std::cout << "-c6" << std::endl;
                    if (error == ERROR_IO_PENDING ||
                        error == ERROR_PIPE_LISTENING) {
                        CloseHandle(overlapped.hEvent); // Очищаем событие
                        std::cout << "-cc6" << std::endl;
                        return;
                    }
                    std::cout << "-c7" << std::endl;
                    lock.lock();
                    m_connections.erase(it);
                    lock.unlock();

                    CloseHandle(overlapped.hEvent); // Очищаем событие
                    CloseHandle(m_named_pipe);
                    m_named_pipe = nullptr;

                    if (error != ERROR_NO_DATA) {
                        throw std::system_error(error, std::system_category(), "Failed to connect named pipe.");
                    }
                    return;
                }

                connection->start_read();
                on_open(connection);

                CloseHandle(overlapped.hEvent); // Очищаем событие
                m_named_pipe = nullptr; // Будем ожидать новое соединение, HANDLE теперь хранится в Connection
                std::cout << "-c8" << std::endl;
            } catch(const std::system_error& sys_err) {
                on_server_error(sys_err.code());
            }
        }

        /// \brief Creates a named pipe.
        /// \param config The configuration for the named pipe.
        void create_named_pipe(const Config& config) {
            try {
                if (!m_named_pipe) {
                    std::string pipe_name = "\\\\.\\pipe\\" + config.pipe_name;
                    m_named_pipe = CreateNamedPipeA(
                        pipe_name.c_str(),
                        PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
                        PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                        PIPE_UNLIMITED_INSTANCES,
                        static_cast<DWORD>(config.buffer_size),
                        static_cast<DWORD>(config.buffer_size),
                        static_cast<DWORD>(config.timeout),
                        NULL);

                    // Критическая ошибка: не удалось создать именованный канал
                    if (m_named_pipe == INVALID_HANDLE_VALUE) {
                        m_named_pipe = nullptr;
                        throw std::system_error(GetLastError(), std::system_category(), "Failed to create named pipe.");
                    }
                }
                // Подключаем клиента асинхронно
                OVERLAPPED overlapped = {};
                memset(&overlapped, 0, sizeof(OVERLAPPED));
                BOOL result = ConnectNamedPipe(m_named_pipe, &overlapped);

                // Проверяем результат подключения
                if (!result) {
                    DWORD error = GetLastError();
                    if (error == ERROR_IO_PENDING) {
                        // Операция подключения начата, но клиент еще не подключился
                        return;
                    } else
                    if (error == ERROR_PIPE_LISTENING) {
                        // Канал всё ещё ожидает подключения
                        return;
                    } else
                    if (error == ERROR_PIPE_CONNECTED) {
                        // Клиент уже подключен
                    } else {
                        // Ошибка подключения клиента (не критическая)
                        CloseHandle(m_named_pipe);
                        m_named_pipe = nullptr;
                        return;
                    }
                } else {
                    // Клиент подключен сразу
                }

                auto connection = std::make_shared<Connection>(
                    m_named_pipe,
                    config.buffer_size,
                    on_message,
                    on_close,
                    on_error);

                // Привязываем дескриптор канала к Completion Port
                if (!CreateIoCompletionPort(m_named_pipe, m_completion_port, reinterpret_cast<ULONG_PTR>(connection->get_pipe()), 0)) {
                    CloseHandle(m_named_pipe);
                    throw std::system_error(GetLastError(), std::system_category(), "Failed to associate pipe with Completion Port.");
                }

                connection->start_read();
                on_open(connection);

                std::lock_guard<std::mutex> lock(m_connections_mutex);
                m_connections[m_named_pipe] = connection;
                m_named_pipe = nullptr; // Будем ожидать новое соединение, HANDLE теперь хранится в Connection
            } catch(const std::system_error& sys_err) {
                on_server_error(sys_err.code());
            }
        }

        /// \brief Main loop for the server.
        void server_main_loop() {
            try {
                m_completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, NULL, 0, 0);
                if (!m_completion_port) {
                    throw std::system_error(GetLastError(), std::system_category(), "Failed to create Completion Port");
                }

                auto config = get_config();
                if (on_server_start) on_server_start(config);

                while (!m_stop_server) {
                    create_named_pipe(config);

                    std::unique_lock<std::mutex> lock_connections(m_connections_mutex);
                    if (m_connections.empty()) {
                        lock_connections.unlock();
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                    for (auto it = m_connections.begin(); it != m_connections.end();) {
                        auto &connection = it->second;
                        if (!connection->is_closed()) {
                            connection->process_send_queue();
                            ++it;
                        }
                    }
                    lock_connections.unlock();

                    for (;;) {
                        DWORD bytes_transferred;
                        ULONG_PTR key;
                        LPOVERLAPPED overlapped = nullptr;

                        BOOL success = GetQueuedCompletionStatus(m_completion_port, &bytes_transferred, &key, &overlapped, 0);
                        if (!success) {
                            DWORD error = GetLastError();
                            if (error == WAIT_TIMEOUT) {
                                break; // Нет завершённых операций, выходим из цикла.
                            } else {
                                // https://stackoverflow.com/questions/64863170/whats-the-return-of-getqueuedcompletionstatus-when-the-i-o-is-cancelled-via-can
                                // Если GetQueuedCompletionStatus()возвращает FALSE И ВЫВОДИТ НЕНУЛЕВОЙ OVERLAPPED*указатель , это означает, что пакет ввода-вывода со статусом ошибки был исключен из очереди
                                // Если GetQueuedCompletionStatus()возвращает FALSE И ВЫВОДИТ NULL- OVERLAPPED*указатель , это означает GetQueuedCompletionStatus(), что произошел сбой, поэтому все выходные значения неопределенны и должны игнорироваться

                                // Обработка критической ошибки.
                                if (on_server_error) on_server_error(std::error_code(error, std::system_category()));
                                continue;
                            }
                        }

                        HANDLE pipe = reinterpret_cast<HANDLE>(key);
                        std::lock_guard<std::mutex> lock(m_connections_mutex);
                        auto it = m_connections.find(pipe);
                        if (it != m_connections.end()) {
                            if (it->second) it->second->complete_read();
                        }
                    }

                    lock_connections.lock();
                    for (auto it = m_connections.begin(); it != m_connections.end();) {
                        auto &connection = it->second;
                        if (!connection || connection->is_closed()) {
                            it = m_connections.erase(it);
                        } else {
                            ++it;
                        }
                    }
                    lock_connections.unlock();

                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }

                std::unique_lock<std::mutex> lock_connections(m_connections_mutex);
                for (auto it = m_connections.begin(); it != m_connections.end(); it++) {
                    auto &connection = it->second;
                    connection->close();
                    connection->process_send_queue();
                }
                lock_connections.unlock();

                if (m_completion_port) {
                    CloseHandle(m_completion_port);
                    m_completion_port = nullptr;
                }

                if (on_server_close) on_server_close();
            } catch (const std::system_error& sys_err) {
                if (on_server_error) on_server_error(sys_err.code());
            } catch (const std::exception& ex) {
                if (on_server_error) on_server_error(std::make_error_code(std::errc::operation_not_permitted));
            } catch (...) {
                if (on_server_error) on_server_error(std::make_error_code(std::errc::operation_not_permitted));
            }
        }

    }; // NamedPipeServer
}; // namespace SimpleNamedPipe

#endif // _SIMLPE_NAMED_PIPE_SERVER_HPP_INCLUDED
