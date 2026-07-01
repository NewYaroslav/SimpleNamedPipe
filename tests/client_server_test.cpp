#include <SimpleNamedPipe/NamedPipeClient.hpp>
#include <SimpleNamedPipe/NamedPipeServer.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {

const std::chrono::milliseconds kDefaultWait(5000);
const std::chrono::milliseconds kHeavyWait(15000);

std::string make_test_pipe_name(const std::string& suffix) {
    static std::atomic<int> counter{0};
    const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    std::ostringstream out;
    out << "SimpleNamedPipeClientServerTest_"
        << stamp << "_" << counter.fetch_add(1) << "_" << suffix;
    return out.str();
}

template<class Predicate>
bool wait_until(
        std::condition_variable& cv,
        std::mutex& mutex,
        Predicate predicate,
        std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock, timeout, predicate);
}

template<class Predicate>
bool poll_until(Predicate predicate, std::chrono::milliseconds timeout) {
    const auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < timeout) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return predicate();
}

std::string with_error(const std::string& message, const std::error_code& error) {
    return message + error.message();
}

struct TestRunner {
    int failures = 0;

    bool expect(bool condition, const std::string& message) {
        if (!condition) {
            ++failures;
            std::cerr << "FAILED: " << message << "\n";
        }
        return condition;
    }

    void run(const std::string& name, const std::function<void(TestRunner&)>& test) {
        const int before = failures;
        std::cerr << "[ RUN      ] " << name << "\n";
        try {
            test(*this);
        } catch (const std::exception& ex) {
            ++failures;
            std::cerr << "FAILED: " << name << " threw: " << ex.what() << "\n";
        } catch (...) {
            ++failures;
            std::cerr << "FAILED: " << name << " threw an unknown exception\n";
        }

        if (failures == before) {
            std::cerr << "[       OK ] " << name << "\n";
        } else {
            std::cerr << "[  FAILED  ] " << name << "\n";
        }
    }
};

class ServerHarness {
public:
    explicit ServerHarness(
            const std::string& suffix,
            size_t buffer_size = 4096,
            size_t timeout_ms = 25)
        : pipe_name(make_test_pipe_name(suffix)),
          config(pipe_name, buffer_size, timeout_ms),
          server(config) {
        server.on_start = [this](const SimpleNamedPipe::ServerConfig&) {
            std::lock_guard<std::mutex> lock(mutex);
            started = true;
            cv.notify_all();
        };

        server.on_stop = [this](const SimpleNamedPipe::ServerConfig&) {
            std::lock_guard<std::mutex> lock(mutex);
            stopped = true;
            cv.notify_all();
        };

        server.on_connected = [this](int client_id) {
            std::lock_guard<std::mutex> lock(mutex);
            ++connected_events;
            connected_ids.insert(client_id);
            cv.notify_all();
        };

        server.on_disconnected = [this](int client_id, const std::error_code&) {
            std::lock_guard<std::mutex> lock(mutex);
            ++disconnected_events;
            connected_ids.erase(client_id);
            cv.notify_all();
        };

        server.on_message = [this](int client_id, const std::string& message) {
            {
                std::lock_guard<std::mutex> lock(mutex);
                ++message_events;
                messages.push_back(message);
                cv.notify_all();
            }
            server.send_to(client_id, "Echo: " + message);
        };

        server.on_error = [this](const std::error_code& error) {
            std::lock_guard<std::mutex> lock(mutex);
            ++error_events;
            last_error = error;
            cv.notify_all();
        };
    }

    ~ServerHarness() {
        server.stop();
    }

    void start() {
        server.start();
    }

    void stop() {
        server.stop();
    }

    bool wait_started(std::chrono::milliseconds timeout = kDefaultWait) {
        return wait_until(cv, mutex, [this] { return started; }, timeout);
    }

    bool wait_connected_at_least(int value, std::chrono::milliseconds timeout = kDefaultWait) {
        return wait_until(cv, mutex, [this, value] { return connected_events >= value; }, timeout);
    }

    bool wait_disconnected_at_least(int value, std::chrono::milliseconds timeout = kDefaultWait) {
        return wait_until(cv, mutex, [this, value] { return disconnected_events >= value; }, timeout);
    }

    bool wait_messages_at_least(int value, std::chrono::milliseconds timeout = kDefaultWait) {
        return wait_until(cv, mutex, [this, value] { return message_events >= value; }, timeout);
    }

    int connected_count() const {
        std::lock_guard<std::mutex> lock(mutex);
        return connected_events;
    }

    int disconnected_count() const {
        std::lock_guard<std::mutex> lock(mutex);
        return disconnected_events;
    }

    int error_count() const {
        std::lock_guard<std::mutex> lock(mutex);
        return error_events;
    }

    std::error_code error() const {
        std::lock_guard<std::mutex> lock(mutex);
        return last_error;
    }

    std::vector<int> current_client_ids() const {
        std::lock_guard<std::mutex> lock(mutex);
        return std::vector<int>(connected_ids.begin(), connected_ids.end());
    }

    std::string pipe_name;
    SimpleNamedPipe::ServerConfig config;
    SimpleNamedPipe::NamedPipeServer server;

private:
    mutable std::mutex mutex;
    std::condition_variable cv;
    bool started = false;
    bool stopped = false;
    int connected_events = 0;
    int disconnected_events = 0;
    int message_events = 0;
    int error_events = 0;
    std::error_code last_error;
    std::set<int> connected_ids;
    std::vector<std::string> messages;
};

std::unique_ptr<SimpleNamedPipe::NamedPipeClient> make_client(
        const std::string& pipe_name,
        size_t buffer_size = 4096,
        size_t timeout_ms = 3000) {
    return std::unique_ptr<SimpleNamedPipe::NamedPipeClient>(
        new SimpleNamedPipe::NamedPipeClient(
            SimpleNamedPipe::ClientConfig(pipe_name, buffer_size, timeout_ms)));
}

bool read_echo(
        SimpleNamedPipe::NamedPipeClient& client,
        const std::string& payload,
        std::error_code& ec) {
    if (!client.write(payload, &ec)) {
        return false;
    }

    std::string response;
    if (!client.read(response, 3000, &ec)) {
        return false;
    }

    return response == "Echo: " + payload;
}

void basic_io_edges(TestRunner& tr) {
    ServerHarness harness("basic", 4096, 25);
    harness.start();
    if (!tr.expect(harness.wait_started(), "server did not start")) return;

    auto client = make_client(harness.pipe_name, 16, 3000);
    std::error_code ec;
    if (!tr.expect(client->connect(&ec), with_error("client connect failed: ", ec))) return;
    tr.expect(harness.wait_connected_at_least(1), "server did not observe client connect");

    std::string response;
    ec.clear();
    tr.expect(!client->try_read(response, &ec), "try_read should report no immediate message");
    tr.expect(!ec, "try_read without data should not set an error");

    tr.expect(!client->read(response, 25, &ec), "timed read should fail with timeout");
    tr.expect(ec.value() == WAIT_TIMEOUT, "timed read should return WAIT_TIMEOUT");

    const std::string long_payload(300, 'x');
    tr.expect(read_echo(*client, long_payload, ec), with_error("long message echo failed: ", ec));
    tr.expect(harness.wait_messages_at_least(1), "server did not receive long message");

    client->close();
    tr.expect(harness.wait_disconnected_at_least(1), "server did not observe client close");
    harness.stop();
    tr.expect(harness.error_count() == 0, with_error("server reported error: ", harness.error()));
}

void repeated_connect_and_open_contract(TestRunner& tr) {
    ServerHarness harness("repeat", 1024, 25);
    harness.start();
    if (!tr.expect(harness.wait_started(), "server did not start")) return;

    auto client = make_client(harness.pipe_name, 1024, 3000);
    std::atomic<int> connected_callbacks{0};
    client->on_connected = [&connected_callbacks] {
        connected_callbacks.fetch_add(1);
    };

    std::error_code ec;
    tr.expect(client->connect(&ec), with_error("first connect failed: ", ec));
    tr.expect(client->connect(&ec), with_error("second connect should be idempotent: ", ec));
    tr.expect(connected_callbacks.load() == 1, "second connect should not emit on_connected");
    tr.expect(harness.wait_connected_at_least(1), "server did not observe first connect");
    tr.expect(harness.connected_count() == 1, "second connect should not create a second server connection");

    ec.clear();
    tr.expect(!client->open(harness.pipe_name + "_other", &ec), "open should reject re-open while connected");
    tr.expect(ec == std::make_error_code(std::errc::already_connected),
              "open while connected should return already_connected");
    tr.expect(client->is_connected(), "client should remain connected after rejected open");

    client->close();
    tr.expect(harness.wait_disconnected_at_least(1), "server did not observe client close");
    harness.stop();
    tr.expect(harness.error_count() == 0, with_error("server reported error: ", harness.error()));
}

void server_disconnect_notifies_client(TestRunner& tr) {
    ServerHarness harness("server_disconnect", 1024, 25);
    harness.start();
    if (!tr.expect(harness.wait_started(), "server did not start")) return;

    auto client = make_client(harness.pipe_name, 1024, 3000);
    std::atomic<int> disconnected_callbacks{0};
    client->on_disconnected = [&disconnected_callbacks] {
        disconnected_callbacks.fetch_add(1);
    };

    std::error_code ec;
    tr.expect(client->connect(&ec), with_error("client connect failed: ", ec));
    tr.expect(harness.wait_connected_at_least(1), "server did not observe client connect");

    const std::vector<int> ids = harness.current_client_ids();
    if (!tr.expect(!ids.empty(), "server has no connected client id")) return;

    std::mutex done_mutex;
    std::condition_variable done_cv;
    bool close_done = false;
    harness.server.close(ids.front(), [&](const std::error_code&) {
        std::lock_guard<std::mutex> lock(done_mutex);
        close_done = true;
        done_cv.notify_all();
    });

    tr.expect(wait_until(done_cv, done_mutex, [&close_done] { return close_done; }, kDefaultWait),
              "server close callback did not fire");
    tr.expect(harness.wait_disconnected_at_least(1), "server did not emit disconnect");

    tr.expect(poll_until([&] {
        std::error_code local_ec;
        client->available(&local_ec);
        return !client->is_connected() && disconnected_callbacks.load() == 1;
    }, kDefaultWait), "client did not synchronize disconnected state");

    tr.expect(disconnected_callbacks.load() == 1, "on_disconnected should fire exactly once");
    harness.stop();
    tr.expect(harness.error_count() == 0, with_error("server reported error: ", harness.error()));
}

void churn_clients(TestRunner& tr) {
    ServerHarness harness("churn", 2048, 25);
    harness.start();
    if (!tr.expect(harness.wait_started(), "server did not start")) return;

    for (int cycle = 0; cycle < 10; ++cycle) {
        const int before_connected = harness.connected_count();
        const int before_disconnected = harness.disconnected_count();
        std::vector<std::unique_ptr<SimpleNamedPipe::NamedPipeClient> > clients;

        for (int i = 0; i < 12; ++i) {
            clients.push_back(make_client(harness.pipe_name, 512, 3000));
            std::error_code ec;
            if (!tr.expect(clients.back()->connect(&ec), with_error("churn connect failed: ", ec))) {
                return;
            }
        }

        tr.expect(harness.wait_connected_at_least(before_connected + 12), "server missed churn connects");

        for (int i = 0; i < 4; ++i) {
            std::error_code ec;
            std::ostringstream payload;
            payload << "cycle-" << cycle << "-client-" << i;
            tr.expect(read_echo(*clients[static_cast<size_t>(i)], payload.str(), ec),
                      with_error("churn echo failed: ", ec));
        }

        std::vector<int> ids = harness.current_client_ids();
        const size_t server_close_count = (std::min)(ids.size(), static_cast<size_t>(4));
        for (size_t i = 0; i < server_close_count; ++i) {
            harness.server.close(ids[i]);
        }

        for (size_t i = server_close_count; i < clients.size(); ++i) {
            if (i % 2 == 0) {
                std::error_code ec;
                clients[i]->write("closing-soon", &ec);
            }
            clients[i]->close();
        }

        tr.expect(harness.wait_disconnected_at_least(before_disconnected + 12, kHeavyWait),
                  "server missed churn disconnects");
    }

    harness.stop();
    tr.expect(harness.error_count() == 0, with_error("server reported error during churn: ", harness.error()));
}

void peak_255_clients_and_slot_reuse(TestRunner& tr) {
    ServerHarness harness("peak", 512, 25);
    harness.start();
    if (!tr.expect(harness.wait_started(), "server did not start")) return;

    std::vector<std::unique_ptr<SimpleNamedPipe::NamedPipeClient> > clients;
    clients.reserve(255);

    for (int i = 0; i < 255; ++i) {
        clients.push_back(make_client(harness.pipe_name, 128, 3000));
        std::error_code ec;
        if (!tr.expect(clients.back()->connect(&ec), with_error("peak connect failed: ", ec))) {
            return;
        }
    }

    tr.expect(harness.wait_connected_at_least(255, kHeavyWait), "server did not accept 255 clients");

    auto extra = make_client(harness.pipe_name, 128, 150);
    std::error_code ec;
    tr.expect(!extra->connect(&ec), "256th client should not connect while all instances are busy");

    const int before_disconnected = harness.disconnected_count();
    clients.back()->close();
    clients.pop_back();
    tr.expect(harness.wait_disconnected_at_least(before_disconnected + 1, kHeavyWait),
              "server did not release a closed peak client slot");

    extra = make_client(harness.pipe_name, 128, 3000);
    tr.expect(extra->connect(&ec), with_error("extra client should connect after a slot is released: ", ec));
    tr.expect(harness.wait_connected_at_least(256, kHeavyWait), "server did not observe slot reuse connection");

    extra->close();
    for (size_t i = 0; i < clients.size(); ++i) {
        clients[i]->close();
    }

    harness.stop();
    tr.expect(harness.error_count() == 0, with_error("server reported error during peak test: ", harness.error()));
}

void invalid_utf8_pipe_name_fails(TestRunner& tr) {
    const std::string invalid_name("\xC3\x28", 2);
    SimpleNamedPipe::NamedPipeClient client(
        SimpleNamedPipe::ClientConfig(invalid_name, 512, 10));

    std::error_code ec;
    tr.expect(!client.connect(&ec), "invalid UTF-8 pipe name should fail");
    tr.expect(static_cast<bool>(ec), "invalid UTF-8 pipe name should set an error");
    tr.expect(!client.is_connected(), "invalid UTF-8 connect should not leave client connected");
}

} // namespace

int main() {
    TestRunner runner;

    runner.run("basic_io_edges", basic_io_edges);
    runner.run("repeated_connect_and_open_contract", repeated_connect_and_open_contract);
    runner.run("server_disconnect_notifies_client", server_disconnect_notifies_client);
    runner.run("churn_clients", churn_clients);
    runner.run("peak_255_clients_and_slot_reuse", peak_255_clients_and_slot_reuse);
    runner.run("invalid_utf8_pipe_name_fails", invalid_utf8_pipe_name_fails);

    if (runner.failures != 0) {
        std::cerr << runner.failures << " failure(s)\n";
        return EXIT_FAILURE;
    }

    std::cerr << "All client/server named pipe tests passed\n";
    return EXIT_SUCCESS;
}
