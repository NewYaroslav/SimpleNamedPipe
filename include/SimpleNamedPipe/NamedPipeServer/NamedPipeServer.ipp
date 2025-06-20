#ifdef SIMPLE_NAMED_PIPE_STATIC_LIB
#include "../NamedPipeServer.hpp"
#endif

#include <codecvt>
#include <locale>
#include <algorithm>

namespace SimpleNamedPipe {

    NamedPipeServer::NamedPipeServer() {
        for (size_t i = 0; i < MAX_CLIENTS; ++i) {
            m_pipes[i] = INVALID_HANDLE_VALUE;
        }
    };

    NamedPipeServer::NamedPipeServer(const ServerConfig& config) {
        set_config(config);
    };

    NamedPipeServer::~NamedPipeServer() {
        stop();
    }

    void NamedPipeServer::set_event_handler(std::shared_ptr<ServerEventHandler> handler) {
        m_event_handler = handler;
    }

    std::shared_ptr<ServerEventHandler> NamedPipeServer::get_event_handler() const {
        return m_event_handler;
    }

    void NamedPipeServer::set_config(const ServerConfig& config) {
        std::unique_lock<std::mutex> lock(m_config_mutex);
        m_config = config;
        m_is_config_updated = true;
        lock.unlock();

        m_config_cv.notify_one();

        if (m_is_running.load(std::memory_order_acquire)) {
            HANDLE completion_port = m_completion_port.load(std::memory_order_acquire);
            if (completion_port) {
                PostQueuedCompletionStatus(completion_port, 0, CMD_TYPE_STOP, nullptr);
            }
        }
    }

    const ServerConfig NamedPipeServer::get_config() const {
        std::lock_guard<std::mutex> lock(m_config_mutex);
        return m_config;
    }

    void NamedPipeServer::start(bool run_async) {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_server_thread.joinable()) {
            m_is_stop_server = true;

            HANDLE completion_port = m_completion_port.load(std::memory_order_acquire);
            if (m_is_running.load(std::memory_order_acquire) && completion_port) {
                PostQueuedCompletionStatus(completion_port, 0, CMD_TYPE_STOP, nullptr);
            }
            m_server_thread.join();
        }
        m_is_stop_server = false;
        if (run_async) {
            m_server_thread = std::thread(&NamedPipeServer::main_loop, this);
        } else {
            main_loop();
        }
    }

    void NamedPipeServer::stop() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_is_stop_server) return;
        m_is_stop_server = true;

        HANDLE completion_port = m_completion_port.load(std::memory_order_acquire);
        if (m_is_running.load(std::memory_order_acquire) && completion_port) {
            PostQueuedCompletionStatus(completion_port, 0, CMD_TYPE_STOP, nullptr);
        }

        if (m_server_thread.joinable()) {
            m_server_thread.join();
        }
    }

    bool NamedPipeServer::is_running() const {
        return m_is_running.load(std::memory_order_acquire);
    }

    void NamedPipeServer::send_to(int client_id, const std::string& message, DoneCallback on_done) {
        HANDLE completion_port = m_completion_port.load(std::memory_order_acquire);

        if (!m_is_running.load(std::memory_order_acquire) || !completion_port) {
            if (on_done) on_done(make_error_code(NamedPipeErrc::ServerStopped));
            return;
        }

        std::error_code ec;
        size_t index = check_client_id(client_id);

        std::unique_lock<std::mutex> lock(m_write_mutex);
        if (!check_write_limits(client_id, message, ec)) {
            if (on_done) on_done(ec);
            return;
        }
        m_pending_writes[index].push({index, 0, message, std::move(on_done)});
        lock.unlock();

        PostQueuedCompletionStatus(completion_port, 0, CMD_TYPE_SEND | (index & CMD_INDEX_MASK), nullptr);
    }

    void NamedPipeServer::close(int client_id, DoneCallback on_done) {
        HANDLE completion_port = m_completion_port.load(std::memory_order_acquire);

        if (!m_is_running.load(std::memory_order_acquire) || !completion_port) {
            if (on_done) on_done(make_error_code(NamedPipeErrc::ServerStopped));
            return;
        }

        size_t index = check_client_id(client_id);

        std::unique_lock<std::mutex> lock(m_write_mutex);
        m_pending_closes[index].push(std::move(on_done));
        lock.unlock();

        PostQueuedCompletionStatus(completion_port, 0, CMD_TYPE_CLOSE | (index & CMD_INDEX_MASK), nullptr);
    }

    bool NamedPipeServer::is_connected(int client_id) const {
        size_t index = check_client_id(client_id);
        return m_is_connected[index].load(std::memory_order_acquire);
    }

    size_t NamedPipeServer::check_client_id(int client_id) const {
        if (client_id < 0 || static_cast<size_t>(client_id) >= MAX_CLIENTS) {
            throw std::out_of_range("client_id is out of range");
        }
        return static_cast<size_t>(client_id);
    }

    bool NamedPipeServer::check_write_limits(int client_id, const std::string& message, std::error_code& ec) {
        if (message.size() > m_write_limits.max_message_size) {
            ec = make_error_code(NamedPipeErrc::MessageTooLarge);
            return false;
        }

        size_t index = check_client_id(client_id);

        if (m_pending_writes[index].size() >= m_write_limits.max_pending_writes_per_client) {
            ec = make_error_code(NamedPipeErrc::QueueFull);
            return false;
        }

        ec.clear();
        return true;
    }

    void NamedPipeServer::init(const ServerConfig& config) {
        m_write_limits = config.write_limits;

        HANDLE completion_port = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
        if (!completion_port) {
            throw std::system_error(GetLastError(), std::system_category(), "Failed to create IO completion port");
        }
        m_completion_port.store(completion_port, std::memory_order_release);

        for (size_t i = 0; i < MAX_CLIENTS; ++i) {
            m_read_buffers[i].resize(config.buffer_size);
            m_write_buffers[i].resize(config.buffer_size);
            m_read_overlapped[i] = {};
            m_write_overlapped[i] = {};
            m_is_writing[i] = false;
            m_is_connected[i] = false;
            create_pipe(i, completion_port, config);
        }
    }

    void NamedPipeServer::create_pipe(size_t index, HANDLE completion_port, const ServerConfig& config) {
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> conv;
        std::wstring pipe_name_w = L"\\\\.\\pipe\\" + conv.from_bytes(config.pipe_name);

        m_pipes[index] = CreateNamedPipeW(
            pipe_name_w.c_str(),
            PIPE_ACCESS_DUPLEX |
            FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            static_cast<DWORD>(config.buffer_size),
            static_cast<DWORD>(config.buffer_size),
            static_cast<DWORD>(config.timeout),
            nullptr
        );

        if (m_pipes[index] == INVALID_HANDLE_VALUE) {
            throw std::system_error(GetLastError(), std::system_category(), "Failed to create named pipe.");
        }

        if (!CreateIoCompletionPort(m_pipes[index], completion_port, static_cast<ULONG_PTR>(index), 0)) {
            throw std::system_error(GetLastError(), std::system_category(), "Failed to associate pipe with Completion Port.");
        }

        if (!reconnect_client(index, completion_port, &m_read_overlapped[index])) {
            throw std::system_error(GetLastError(), std::system_category(), "Failed to post completion status.");
        }
    }

    void NamedPipeServer::main_loop() {
        while (!m_is_stop_server) {
            std::unique_lock<std::mutex> lock(m_config_mutex);
            m_config_cv.wait(lock, [this] {
                return m_is_config_updated || m_is_stop_server;
            });

            if (m_is_stop_server) return;

            m_is_config_updated = false;
            auto config = m_config;
            lock.unlock();

            try {
                init(config);
                run_server_loop(config);
            } catch (const std::system_error& ex) {
                notify_error(ex.code());
                notify_stop(config);
            } catch (const std::exception&) {
                notify_error(make_error_code(NamedPipeErrc::UnhandledException));
                notify_stop(config);
            } catch (...) {
                notify_error(make_error_code(NamedPipeErrc::UnknownSystemError));
                notify_stop(config);
            }

            cleanup_pending_operations(make_error_code(NamedPipeErrc::ServerStopped));

            for (size_t i = 0; i < MAX_CLIENTS; ++i) {
                if (m_pipes[i] != nullptr &&
                    m_pipes[i] != INVALID_HANDLE_VALUE) {
                    CancelIoEx(m_pipes[i], nullptr);
                    DisconnectNamedPipe(m_pipes[i]);
                    CloseHandle(m_pipes[i]);
                    m_pipes[i] = INVALID_HANDLE_VALUE;
                }
            }

            HANDLE completion_port = m_completion_port.exchange(nullptr, std::memory_order_acq_rel);
            if (completion_port) {
                CloseHandle(completion_port);
            }
        }
    }

    void NamedPipeServer::run_server_loop(const ServerConfig& config) {
        notify_start(config);
        HANDLE completion_port = m_completion_port.load(std::memory_order_acquire);
        while (!m_is_stop_server) {
            DWORD bytes_transferred;
            ULONG_PTR key;
            OVERLAPPED* ov;

            BOOL ok = GetQueuedCompletionStatus(completion_port, &bytes_transferred, &key, &ov, INFINITE);
            size_t index = key & CMD_INDEX_MASK;

            // Special handling of send commands
            if ((key & CMD_TYPE_SEND) == CMD_TYPE_SEND) {
                process_write_commands(index);
                continue;
            }

            if ((key & CMD_TYPE_CLOSE) == CMD_TYPE_CLOSE) {
                handle_close(index, completion_port);
                continue;
            }

            // Server stop signal
            if (((key & CMD_TYPE_STOP) == CMD_TYPE_STOP) ||
                (key == 0 && ov == nullptr && bytes_transferred == 0)) {
                break;
            }

            DWORD err = GetLastError();
            if (!ok) {
                if (ov == nullptr) {
                    // Severe queue error, ov is null so we cannot continue
                    notify_error(std::error_code(static_cast<int>(err), std::system_category()));
                    continue;
                }

                // ov != nullptr means the operation completed with an error
                if (err == ERROR_BROKEN_PIPE) {
                    notify_disconnected(index, std::error_code(err, std::system_category()));
                    DisconnectNamedPipe(m_pipes[index]);
                    reconnect_client(index, completion_port, &m_read_overlapped[index]);
                    continue;
                }

                notify_error(std::error_code(static_cast<int>(err), std::system_category()));
                continue;
            }

            if (index >= MAX_CLIENTS) {
                notify_error(make_error_code(NamedPipeErrc::ClientIndexOutOfRange));
                continue;
            }

            // Connection completed (ConnectNamedPipe)
            if (ov == &m_read_overlapped[index] &&
                bytes_transferred == 0 &&
                !m_is_connected[index]) {
                notify_connected(index);
            } else
            // Handle write completion
            if (ov == &m_write_overlapped[index]) {
                handle_write_completion(index, bytes_transferred);
            } else
            // Handle read completion
            if (ov == &m_read_overlapped[index] && bytes_transferred > 0) {
                err = GetLastError();
                m_message_buffers[index].append(m_read_buffers[index].data(), bytes_transferred);
                if (err != ERROR_MORE_DATA) {
                    notify_message(index);
                }
            }

            // Skip reading if still not connected
            if (!m_is_connected[index].load(std::memory_order_acquire)) continue;

            DWORD dummy = 0;
            OVERLAPPED* new_ov = &m_read_overlapped[index];
            BOOL result = ReadFile(m_pipes[index], m_read_buffers[index].data(), static_cast<DWORD>(m_read_buffers[index].size()), &dummy, new_ov);

            err = GetLastError();
            if (!result && err != ERROR_IO_PENDING) {
                if (err == ERROR_BROKEN_PIPE ||
                    err == ERROR_NO_DATA) {
                    DisconnectNamedPipe(m_pipes[index]);
                    reconnect_client(index, completion_port, new_ov);
                    continue;
                } else {
                    notify_error(std::error_code(static_cast<int>(err), std::system_category()));
                    continue;
                }
            }
        }

        notify_stop(config);
    }

    // Process all accumulated write commands
    void NamedPipeServer::process_write_commands(size_t index) {
        std::unique_lock<std::mutex> lock(m_write_mutex);
        while (!m_pending_writes[index].empty()) {
            m_active_writes[index].push(std::move(m_pending_writes[index].front()));
            m_pending_writes[index].pop();
        }
        lock.unlock();

        if (!m_is_writing[index]) {
            m_is_writing[index] = true;
            post_next_write(index);
        }
    }

    void NamedPipeServer::handle_write_completion(size_t index, size_t bytes_transferred) {
        if (!m_active_writes[index].empty()) {
            auto& cmd = m_active_writes[index].front();
            cmd.offset += bytes_transferred;
            if (cmd.offset >= cmd.message.size()) {
                if (cmd.on_done) cmd.on_done(std::error_code{});
                m_active_writes[index].pop();
            }
        }
        post_next_write(index);
    }

    void NamedPipeServer::post_next_write(size_t index) {
        if (m_active_writes[index].empty()) {
            m_is_writing[index] = false;
            return;
        }

        auto& cmd = m_active_writes[index].front();

        if (!m_is_connected[index].load(std::memory_order_acquire)) {
            if (cmd.on_done) cmd.on_done(make_error_code(NamedPipeErrc::NotConnected));
            m_active_writes[index].pop();
            return;
        }

        if (cmd.client_index >= MAX_CLIENTS) {
            if (cmd.on_done) cmd.on_done(make_error_code(NamedPipeErrc::ClientIndexOutOfRange));
            m_active_writes[index].pop();
            return;
        }

        if (m_pipes[cmd.client_index] == INVALID_HANDLE_VALUE) {
            if (cmd.on_done) cmd.on_done(make_error_code(NamedPipeErrc::InvalidPipeHandle));
            m_active_writes[index].pop();
            return;
        }

        size_t buffer_size = m_write_buffers[index].size();
        size_t msg_offset = cmd.offset;
        size_t remaining = (msg_offset < cmd.message.size())
            ? (cmd.message.size() - msg_offset)
            : 0;
        size_t bytes_to_copy = (std::min)(buffer_size, remaining);
        auto& buffer = m_write_buffers[index];
        buffer.assign(cmd.message.begin() + msg_offset, cmd.message.begin() + msg_offset + bytes_to_copy);
        cmd.offset += bytes_to_copy;

        OVERLAPPED* ov = &m_write_overlapped[index];
        memset(ov, 0, sizeof(OVERLAPPED));

        DWORD bytes_written = 0;
        BOOL success = WriteFile(m_pipes[index], buffer.data(), static_cast<DWORD>(bytes_to_copy), &bytes_written, ov);
        DWORD err = GetLastError();
        if (!success && err != ERROR_IO_PENDING) {
            if (cmd.on_done) cmd.on_done(std::error_code(static_cast<int>(err), std::system_category()));
            m_active_writes[index].pop();
            m_is_writing[index] = false;
        }
    }

    bool NamedPipeServer::reconnect_client(size_t index, HANDLE completion_port, OVERLAPPED* ov) {
        memset(ov, 0, sizeof(OVERLAPPED));

        BOOL connected = ConnectNamedPipe(m_pipes[index], ov);
        if (connected) {
            PostQueuedCompletionStatus(completion_port, 0, static_cast<ULONG_PTR>(index), ov);
            return true;
        }

        DWORD err = GetLastError();
        if (err == ERROR_PIPE_CONNECTED) {
            PostQueuedCompletionStatus(completion_port, 0, static_cast<ULONG_PTR>(index), ov);
            return true;
        } else
        if (err != ERROR_IO_PENDING) {
            notify_error(std::error_code(static_cast<int>(err), std::system_category()));
            return false;
        }
        return true;
    }

    void NamedPipeServer::handle_close(size_t index, HANDLE completion_port) {
        std::unique_lock<std::mutex> lock(m_write_mutex);
        if (m_pending_closes[index].empty()) return;
        auto on_done = std::move(m_pending_closes[index].front());
        m_pending_closes[index].pop();
        lock.unlock();

        if (m_pipes[index] &&
            m_pipes[index] != INVALID_HANDLE_VALUE) {
            notify_disconnected(index, std::error_code{});
            CancelIoEx(m_pipes[index], nullptr);
            DisconnectNamedPipe(m_pipes[index]);
            if (reconnect_client(index, completion_port, &m_read_overlapped[index])) {
                if (on_done) on_done(std::error_code{});
            } else {
                if (on_done) on_done(std::error_code(GetLastError(), std::system_category()));
            }
            return;
        }
        if (on_done) on_done(make_error_code(NamedPipeErrc::InvalidPipeHandle));
    }

    void NamedPipeServer::cleanup_pending_operations(const std::error_code& reason) {
        std::array<std::queue<WriteCommand>, MAX_CLIENTS> pending_writes;
        std::array<std::queue<DoneCallback>, MAX_CLIENTS> pending_closes;

        std::unique_lock<std::mutex> lock(m_write_mutex);
        std::swap(pending_writes, m_pending_writes);
        std::swap(pending_closes, m_pending_closes);
        lock.unlock();

        for (size_t i = 0; i < MAX_CLIENTS; ++i) {
            if (m_is_connected[i].load(std::memory_order_acquire)) {
                if (m_connections[i]) {
                    m_connections[i]->invalidate();
                }
                notify_disconnected(i, reason);
            }

            while (!m_active_writes[i].empty()) {
                auto& cmd = m_active_writes[i].front();
                if (cmd.on_done) cmd.on_done(reason);
                m_active_writes[i].pop();
            }

            while (!pending_writes[i].empty()) {
                auto& cmd = pending_writes[i].front();
                if (cmd.on_done) cmd.on_done(reason);
                pending_writes[i].pop();
            }

            while (!pending_closes[i].empty()) {
                auto& cb = pending_closes[i].front();
                if (cb) cb(reason);
                pending_closes[i].pop();
            }
        }
    }

    void NamedPipeServer::notify_connected(size_t index) {
        if (m_is_connected[index].load(std::memory_order_acquire)) return;
        m_is_connected[index].store(true, std::memory_order_release);
        m_connections[index] = std::make_shared<Connection>(index, this);
        if (m_event_handler) m_event_handler->on_connected(static_cast<int>(index));
        if (on_connected) on_connected(static_cast<int>(index));
        if (on_event) on_event(ServerEvent::client_connected(static_cast<int>(index), m_connections[index]));
    }

    void NamedPipeServer::notify_disconnected(size_t index, const std::error_code& ec) {
        if (!m_is_connected[index].load(std::memory_order_acquire)) return;
        m_is_connected[index].store(false, std::memory_order_release);
        if (m_connections[index]) m_connections[index]->invalidate();
        if (m_event_handler) m_event_handler->on_disconnected(static_cast<int>(index), ec);
        if (on_disconnected) on_disconnected(static_cast<int>(index), ec);
        if (on_event) on_event(ServerEvent::client_disconnected(static_cast<int>(index), m_connections[index], ec));
    }

    void NamedPipeServer::notify_message(size_t index) {
        if (m_event_handler) m_event_handler->on_message(static_cast<int>(index), m_message_buffers[index]);
        if (on_message) on_message(static_cast<int>(index), m_message_buffers[index]);
        if (on_event) on_event(ServerEvent::message_received(static_cast<int>(index), m_connections[index], std::move(m_message_buffers[index])));
        m_message_buffers[index].clear();
    }

    void NamedPipeServer::notify_start(const ServerConfig& config) {
        if (m_is_running.load(std::memory_order_acquire)) return;
        m_is_running.store(true, std::memory_order_release);
        if (m_event_handler) m_event_handler->on_start(config);
        if (on_start) on_start(config);
        if (on_event) on_event(ServerEvent::server_started());
    }

    void NamedPipeServer::notify_stop(const ServerConfig& config) {
        if (!m_is_running.load(std::memory_order_acquire)) return;
        m_is_running.store(false, std::memory_order_release);
        if (m_event_handler) m_event_handler->on_stop(config);
        if (on_stop) on_stop(config);
        if (on_event) on_event(ServerEvent::server_stopped());
    }

    void NamedPipeServer::notify_error(const std::error_code& ec) {
        if (m_event_handler) m_event_handler->on_error(ec);
        if (on_error) on_error(ec);
        if (on_event) on_event(ServerEvent::error_occurred(ec));
    }
};
