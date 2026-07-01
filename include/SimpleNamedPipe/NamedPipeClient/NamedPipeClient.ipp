#ifdef SIMPLE_NAMED_PIPE_STATIC_LIB
#include <SimpleNamedPipe/NamedPipeClient.hpp>
#endif

#include <SimpleNamedPipe/detail/string_utils.hpp>

#include <algorithm>
#include <limits>
#include <thread>

#ifndef SIMPLE_NAMED_PIPE_INLINE
#ifdef SIMPLE_NAMED_PIPE_STATIC_LIB
#define SIMPLE_NAMED_PIPE_INLINE
#else
#define SIMPLE_NAMED_PIPE_INLINE inline
#endif
#endif

namespace SimpleNamedPipe {

    SIMPLE_NAMED_PIPE_INLINE NamedPipeClient::NamedPipeClient() = default;

    SIMPLE_NAMED_PIPE_INLINE NamedPipeClient::NamedPipeClient(const ClientConfig& config)
        : m_config(config) {}

    SIMPLE_NAMED_PIPE_INLINE NamedPipeClient::~NamedPipeClient() {
        close();
    }

    SIMPLE_NAMED_PIPE_INLINE void NamedPipeClient::set_config(const ClientConfig& config) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_config = config;
    }

    SIMPLE_NAMED_PIPE_INLINE ClientConfig NamedPipeClient::get_config() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_config;
    }

    SIMPLE_NAMED_PIPE_INLINE bool NamedPipeClient::connect(std::error_code* error) {
        std::error_code ec;
        bool result = false;
        bool already_connected = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            already_connected =
                m_is_connected.load(std::memory_order_acquire) &&
                m_pipe != INVALID_HANDLE_VALUE;
            result = connect_no_lock(&ec);
        }
        set_error(error, ec);
        if (result && !already_connected) {
            if (on_connected) on_connected();
        } else if (ec && on_error) {
            on_error(ec);
        }
        return result;
    }

    SIMPLE_NAMED_PIPE_INLINE bool NamedPipeClient::open(const std::string& pipe_name, std::error_code* error) {
        std::error_code ec;
        bool result = false;
        bool already_connected = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            already_connected =
                m_is_connected.load(std::memory_order_acquire) &&
                m_pipe != INVALID_HANDLE_VALUE;
            if (already_connected) {
                ec = std::make_error_code(std::errc::already_connected);
            } else {
                m_config.pipe_name = pipe_name;
                result = connect_no_lock(&ec);
            }
        }
        set_error(error, ec);
        if (result && !already_connected) {
            if (on_connected) on_connected();
        } else if (ec && on_error) {
            on_error(ec);
        }
        return result;
    }

    SIMPLE_NAMED_PIPE_INLINE void NamedPipeClient::close() {
        bool notify = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            notify = close_no_lock();
        }
        if (notify && on_disconnected) {
            on_disconnected();
        }
    }

    SIMPLE_NAMED_PIPE_INLINE bool NamedPipeClient::is_connected() const {
        return m_is_connected.load(std::memory_order_acquire);
    }

    SIMPLE_NAMED_PIPE_INLINE bool NamedPipeClient::connected() const {
        return is_connected();
    }

    SIMPLE_NAMED_PIPE_INLINE bool NamedPipeClient::write(const std::string& message, std::error_code* error) {
        std::error_code ec;
        bool result = false;
        bool disconnected = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_is_connected.load(std::memory_order_acquire) ||
                m_pipe == INVALID_HANDLE_VALUE) {
                ec = make_error_code(NamedPipeErrc::NotConnected);
            } else if (message.empty()) {
                ec = std::make_error_code(std::errc::invalid_argument);
            } else if (message.size() > (std::numeric_limits<DWORD>::max)()) {
                ec = std::make_error_code(std::errc::message_size);
            } else {
                DWORD bytes_written = 0;
                const BOOL ok = WriteFile(
                    m_pipe,
                    message.data(),
                    static_cast<DWORD>(message.size()),
                    &bytes_written,
                    nullptr);
                if (!ok) {
                    ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
                    disconnected = close_no_lock();
                } else if (bytes_written != message.size()) {
                    ec = std::make_error_code(std::errc::io_error);
                    disconnected = close_no_lock();
                } else {
                    result = true;
                }
            }
        }
        set_error(error, ec);
        if (disconnected && on_disconnected) {
            on_disconnected();
        }
        if (!result && ec && on_error) {
            on_error(ec);
        }
        return result;
    }

    SIMPLE_NAMED_PIPE_INLINE bool NamedPipeClient::read(std::string& message, std::error_code* error) {
        std::error_code ec;
        bool result = false;
        bool disconnected = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            result = read_no_lock(message, &ec, &disconnected);
        }
        set_error(error, ec);
        if (disconnected && on_disconnected) {
            on_disconnected();
        }
        if (result) {
            if (on_message) on_message(message);
        } else if (ec && on_error) {
            on_error(ec);
        }
        return result;
    }

    SIMPLE_NAMED_PIPE_INLINE bool NamedPipeClient::try_read(std::string& message, std::error_code* error) {
        std::error_code ec;
        bool result = false;
        bool no_message = false;
        bool disconnected = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_is_connected.load(std::memory_order_acquire) ||
                m_pipe == INVALID_HANDLE_VALUE) {
                ec = make_error_code(NamedPipeErrc::NotConnected);
            } else {
                DWORD bytes_available = 0;
                const BOOL ok = PeekNamedPipe(
                    m_pipe,
                    nullptr,
                    0,
                    nullptr,
                    &bytes_available,
                    nullptr);
                if (!ok) {
                    ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
                    disconnected = close_no_lock();
                } else if (bytes_available == 0) {
                    no_message = true;
                } else {
                    result = read_no_lock(message, &ec, &disconnected);
                }
            }
        }
        set_error(error, ec);
        if (disconnected && on_disconnected) {
            on_disconnected();
        }
        if (result) {
            if (on_message) on_message(message);
        } else if (!no_message && ec && on_error) {
            on_error(ec);
        }
        return result;
    }

    SIMPLE_NAMED_PIPE_INLINE bool NamedPipeClient::read(
            std::string& message,
            size_t timeout_ms,
            std::error_code* error) {
        const auto start = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(timeout_ms);

        for (;;) {
            std::error_code ec;
            if (try_read(message, &ec)) {
                clear_error(error);
                return true;
            }
            if (ec) {
                set_error(error, ec);
                return false;
            }

            const auto now = std::chrono::steady_clock::now();
            if (now - start >= timeout) {
                const auto timeout_error = std::error_code(WAIT_TIMEOUT, std::system_category());
                set_error(error, timeout_error);
                if (on_error) on_error(timeout_error);
                return false;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    SIMPLE_NAMED_PIPE_INLINE size_t NamedPipeClient::available(std::error_code* error) {
        std::error_code ec;
        DWORD bytes_available = 0;
        bool disconnected = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_is_connected.load(std::memory_order_acquire) ||
                m_pipe == INVALID_HANDLE_VALUE) {
                ec = make_error_code(NamedPipeErrc::NotConnected);
            } else {
                const BOOL ok = PeekNamedPipe(
                    m_pipe,
                    nullptr,
                    0,
                    nullptr,
                    &bytes_available,
                    nullptr);
                if (!ok) {
                    ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
                    disconnected = close_no_lock();
                }
            }
        }
        set_error(error, ec);
        if (disconnected && on_disconnected) {
            on_disconnected();
        }
        if (ec && on_error) {
            on_error(ec);
        }
        return static_cast<size_t>(bytes_available);
    }

    SIMPLE_NAMED_PIPE_INLINE bool NamedPipeClient::flush(std::error_code* error) {
        std::error_code ec;
        bool result = false;
        bool disconnected = false;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (!m_is_connected.load(std::memory_order_acquire) ||
                m_pipe == INVALID_HANDLE_VALUE) {
                ec = make_error_code(NamedPipeErrc::NotConnected);
            } else if (!FlushFileBuffers(m_pipe)) {
                ec = std::error_code(static_cast<int>(GetLastError()), std::system_category());
                disconnected = close_no_lock();
            } else {
                result = true;
            }
        }
        set_error(error, ec);
        if (disconnected && on_disconnected) {
            on_disconnected();
        }
        if (!result && ec && on_error) {
            on_error(ec);
        }
        return result;
    }

    SIMPLE_NAMED_PIPE_INLINE HANDLE NamedPipeClient::native_handle() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        return m_pipe;
    }

    SIMPLE_NAMED_PIPE_INLINE std::wstring NamedPipeClient::make_pipe_path(const std::string& pipe_name) {
        static const std::string prefix = "\\\\.\\pipe\\";
        const std::string full_name =
            pipe_name.compare(0, prefix.size(), prefix) == 0
                ? pipe_name
                : prefix + pipe_name;

        return detail::utf8_to_wide(full_name);
    }

    SIMPLE_NAMED_PIPE_INLINE DWORD NamedPipeClient::to_dword_timeout(size_t timeout_ms) {
        const size_t max_value = (std::numeric_limits<DWORD>::max)();
        return static_cast<DWORD>((std::min)(timeout_ms, max_value));
    }

    SIMPLE_NAMED_PIPE_INLINE bool NamedPipeClient::connect_no_lock(std::error_code* error) {
        clear_error(error);

        if (m_is_connected.load(std::memory_order_acquire) &&
            m_pipe != INVALID_HANDLE_VALUE) {
            return true;
        }

        if (!validate_config_no_lock(error)) {
            return false;
        }

        std::wstring pipe_path;
        try {
            pipe_path = make_pipe_path(m_config.pipe_name);
        } catch (const std::system_error& ex) {
            set_error(error, ex.code());
            return false;
        } catch (const std::exception&) {
            set_error(error, std::make_error_code(std::errc::invalid_argument));
            return false;
        }

        const auto start = std::chrono::steady_clock::now();
        const auto timeout = std::chrono::milliseconds(m_config.timeout);

        for (;;) {
            HANDLE pipe = CreateFileW(
                pipe_path.c_str(),
                GENERIC_READ | GENERIC_WRITE,
                0,
                nullptr,
                OPEN_EXISTING,
                0,
                nullptr);

            if (pipe != INVALID_HANDLE_VALUE) {
                DWORD mode = PIPE_READMODE_MESSAGE;
                if (!SetNamedPipeHandleState(pipe, &mode, nullptr, nullptr)) {
                    const auto ec = std::error_code(
                        static_cast<int>(GetLastError()),
                        std::system_category());
                    CloseHandle(pipe);
                    set_error(error, ec);
                    return false;
                }

                m_pipe = pipe;
                m_is_connected.store(true, std::memory_order_release);
                clear_error(error);
                return true;
            }

            const DWORD open_error = GetLastError();
            if (open_error != ERROR_PIPE_BUSY &&
                open_error != ERROR_FILE_NOT_FOUND) {
                set_error(
                    error,
                    std::error_code(static_cast<int>(open_error), std::system_category()));
                return false;
            }

            const auto now = std::chrono::steady_clock::now();
            if (now - start >= timeout) {
                set_error(error, std::error_code(WAIT_TIMEOUT, std::system_category()));
                return false;
            }

            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - start);
            const auto remaining =
                elapsed >= timeout
                    ? std::chrono::milliseconds(0)
                    : (timeout - elapsed);
            const DWORD wait_ms = to_dword_timeout(
                static_cast<size_t>((std::min)(remaining, std::chrono::milliseconds(50)).count()));

            if (open_error == ERROR_PIPE_BUSY) {
                WaitNamedPipeW(pipe_path.c_str(), wait_ms);
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms > 0 ? wait_ms : 1));
            }
        }
    }

    SIMPLE_NAMED_PIPE_INLINE bool NamedPipeClient::read_no_lock(
            std::string& message,
            std::error_code* error,
            bool* disconnected) {
        clear_error(error);
        message.clear();
        if (disconnected) {
            *disconnected = false;
        }

        if (!m_is_connected.load(std::memory_order_acquire) ||
            m_pipe == INVALID_HANDLE_VALUE) {
            set_error(error, make_error_code(NamedPipeErrc::NotConnected));
            return false;
        }

        if (!validate_config_no_lock(error)) {
            return false;
        }

        std::vector<char> buffer(m_config.buffer_size);

        for (;;) {
            DWORD bytes_read = 0;
            const BOOL ok = ReadFile(
                m_pipe,
                buffer.data(),
                static_cast<DWORD>(buffer.size()),
                &bytes_read,
                nullptr);
            const DWORD read_error = GetLastError();

            if (bytes_read > 0) {
                message.append(buffer.data(), bytes_read);
            }

            if (ok) {
                clear_error(error);
                return true;
            }

            if (read_error == ERROR_MORE_DATA) {
                continue;
            }

            const auto ec = std::error_code(
                static_cast<int>(read_error),
                std::system_category());
            set_error(error, ec);

            if (read_error == ERROR_BROKEN_PIPE ||
                read_error == ERROR_NO_DATA) {
                if (disconnected) {
                    *disconnected = close_no_lock();
                } else {
                    close_no_lock();
                }
            }
            return false;
        }
    }

    SIMPLE_NAMED_PIPE_INLINE bool NamedPipeClient::close_no_lock() {
        const bool was_connected = m_is_connected.load(std::memory_order_acquire);
        if (m_pipe != INVALID_HANDLE_VALUE) {
            CloseHandle(m_pipe);
            m_pipe = INVALID_HANDLE_VALUE;
        }
        m_is_connected.store(false, std::memory_order_release);
        return was_connected;
    }

    SIMPLE_NAMED_PIPE_INLINE bool NamedPipeClient::validate_config_no_lock(std::error_code* error) const {
        if (m_config.pipe_name.empty()) {
            set_error(error, std::make_error_code(std::errc::invalid_argument));
            return false;
        }
        if (m_config.buffer_size == 0 ||
            m_config.buffer_size > static_cast<size_t>((std::numeric_limits<DWORD>::max)())) {
            set_error(error, std::make_error_code(std::errc::invalid_argument));
            return false;
        }
        clear_error(error);
        return true;
    }

    SIMPLE_NAMED_PIPE_INLINE void NamedPipeClient::set_error(std::error_code* out, const std::error_code& error) const {
        if (out) {
            *out = error;
        }
    }

    SIMPLE_NAMED_PIPE_INLINE void NamedPipeClient::clear_error(std::error_code* out) const {
        if (out) {
            out->clear();
        }
    }

} // namespace SimpleNamedPipe
