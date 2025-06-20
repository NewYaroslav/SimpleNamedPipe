#pragma once
#ifndef _SIMPLE_NAMED_PIPE_SERVER_HPP_INCLUDED
#define _SIMPLE_NAMED_PIPE_SERVER_HPP_INCLUDED

/// \file NamedPipeServer.hpp
/// \brief Server implementation for managing named pipes

#include "NamedPipeServer/ServerConfig.hpp"
#include "NamedPipeServer/errors.hpp"
#include "NamedPipeServer/IConnection.hpp"
#include "NamedPipeServer/Connection.hpp"
#include "NamedPipeServer/ServerEvent.hpp"
#include "NamedPipeServer/ServerEventHandler.hpp"

#include <windows.h>
#include <array>
#include <vector>
#include <queue>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <functional>

namespace SimpleNamedPipe {

    /// \class NamedPipeServer
    /// \brief Asynchronous named pipe server implementation.
    class NamedPipeServer final : public IConnection {
    public:

        /// \brief Constructor.
        NamedPipeServer();

        /// \brief Constructor with config.
        explicit NamedPipeServer(const ServerConfig& config);

        /// \brief Destructor.
        virtual ~NamedPipeServer();

        // Callbacks
        std::function<void(const ServerEvent&)>          on_event;
        std::function<void(int)>                         on_connected;
        std::function<void(int, const std::error_code&)> on_disconnected;
        std::function<void(int, const std::string&)>     on_message;
        std::function<void(const ServerConfig&)>         on_start;
        std::function<void(const ServerConfig&)>         on_stop;
        std::function<void(const std::error_code&)>      on_error;

        // API

        /// \brief Sets a custom event handler instance.
        /// \param handler Shared pointer to the handler.
        void set_event_handler(std::shared_ptr<ServerEventHandler> handler);

        /// \brief Returns the currently assigned event handler.
        /// \return Shared pointer to the handler or nullptr.
        std::shared_ptr<ServerEventHandler> get_event_handler() const;

        /// \brief Applies a new server configuration.
        /// \param config Configuration to use.
        void set_config(const ServerConfig& config);

        /// \brief Retrieves the current server configuration.
        /// \return Copy of the configuration object.
        const ServerConfig get_config() const;

        /// \brief Starts the server.
        /// \param run_async Whether to run the server in a background thread.
        void start(bool run_async = true);

        /// \brief Stops the server and waits for the thread to finish.
        void stop();

        /// \brief Checks whether the server is currently running.
        bool is_running() const;

        /// \brief Sends a message to a connected client.
        /// \param client_id ID of the client.
        /// \param message Message to send.
        /// \param on_done Optional callback invoked when send completes.
        void send_to(int client_id, const std::string& message, DoneCallback on_done = nullptr) override;

        /// \brief Closes the connection with a client.
        /// \param client_id ID of the client.
        /// \param on_done Optional callback invoked when the client is closed.
        void close(int client_id, DoneCallback on_done = nullptr) override;

        /// \brief Checks whether a client is currently connected.
        /// \param client_id ID of the client.
        /// \return true if connected; false otherwise.
        bool is_connected(int client_id) const override;

    private:
        // --- Internal types ---
        enum CommandType : ULONG_PTR {
            CMD_TYPE_SEND  = 0x10000000,
            CMD_TYPE_CLOSE = 0x20000000,
            CMD_TYPE_STOP  = 0x40000000,
        };

        static constexpr ULONG_PTR CMD_INDEX_MASK = 0x000000FF;
        static constexpr size_t MAX_CLIENTS = 256;

        struct WriteCommand {
            size_t client_index;
            size_t offset;
            std::string message;
            DoneCallback on_done;
        };

        // --- Config ---
        ServerConfig            m_config;
        std::condition_variable m_config_cv;
        mutable std::mutex      m_config_mutex;
        bool                    m_is_config_updated = false;

        // --- OS Handles ---
        //HANDLE m_completion_port = nullptr;
        std::atomic<HANDLE>                 m_completion_port{nullptr};
        std::array<HANDLE, MAX_CLIENTS>     m_pipes{};
        std::array<OVERLAPPED, MAX_CLIENTS> m_read_overlapped{};
        std::array<OVERLAPPED, MAX_CLIENTS> m_write_overlapped{};

        // --- Buffers ---
        std::array<std::vector<char>, MAX_CLIENTS> m_read_buffers;
        std::array<std::vector<char>, MAX_CLIENTS> m_write_buffers;
        std::array<std::string, MAX_CLIENTS>       m_message_buffers;
        std::array<std::shared_ptr<Connection>, MAX_CLIENTS> m_connections;

        // --- Write pipeline ---
        std::array<std::queue<WriteCommand>, MAX_CLIENTS> m_pending_writes;
        std::array<std::queue<WriteCommand>, MAX_CLIENTS> m_active_writes;
        std::array<std::queue<DoneCallback>, MAX_CLIENTS> m_pending_closes;
        WriteQueueLimits                                  m_write_limits;

        // --- State flags ---
        std::array<bool, MAX_CLIENTS>              m_is_writing{};
        std::array<std::atomic<bool>, MAX_CLIENTS> m_is_connected{};

        // --- Threading ---
        std::atomic<bool>  m_is_running{false};
        std::atomic<bool>  m_is_stop_server{false};
        std::thread        m_server_thread;
        mutable std::mutex m_mutex;
        std::mutex         m_write_mutex;

        // --- Event handler ---
        std::shared_ptr<ServerEventHandler> m_event_handler;

        // --- Internal logic ---
        size_t check_client_id(int client_id) const;
        bool check_write_limits(int client_id, const std::string& message, std::error_code& ec);
        void init(const ServerConfig& config);
        void main_loop();
        void run_server_loop(const ServerConfig& config);
        void create_pipe(size_t index, HANDLE completion_port, const ServerConfig& config);
        bool reconnect_client(size_t index, HANDLE completion_port, OVERLAPPED* ov);

        void process_write_commands();
        void process_write_commands(size_t index);
        void post_next_write(size_t index);
        void handle_write_completion(size_t index, size_t bytes_transferred);
        void handle_close(size_t index, HANDLE completion_port);
        void cleanup_pending_operations(const std::error_code& reason);

        void notify_connected(size_t index);
        void notify_disconnected(size_t index, const std::error_code& ec);
        void notify_message(size_t index);
        void notify_start(const ServerConfig& config);
        void notify_stop(const ServerConfig& config);
        void notify_error(const std::error_code& ec);
    };

}; // namespace SimpleNamedPipe

#include "NamedPipeServer/NamedPipeServer.ipp"

#endif // _SIMPLE_NAMED_PIPE_SERVER_HPP_INCLUDED
