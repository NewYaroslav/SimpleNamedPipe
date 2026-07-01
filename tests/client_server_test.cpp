#include <SimpleNamedPipe/NamedPipeClient.hpp>
#include <SimpleNamedPipe/NamedPipeServer.hpp>

#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <string>

namespace {

bool wait_for(
        std::condition_variable& cv,
        std::mutex& mutex,
        bool& flag,
        std::chrono::milliseconds timeout) {
    std::unique_lock<std::mutex> lock(mutex);
    return cv.wait_for(lock, timeout, [&flag] { return flag; });
}

std::string make_test_pipe_name() {
    const auto stamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    return "SimpleNamedPipeClientServerTest_" + std::to_string(stamp);
}

} // namespace

int main() {
    const std::string pipe_name = make_test_pipe_name();

    SimpleNamedPipe::ServerConfig server_config(pipe_name, 4096, 50);
    SimpleNamedPipe::NamedPipeServer server(server_config);

    std::mutex mutex;
    std::condition_variable cv;
    bool server_started = false;
    bool client_connected = false;
    bool server_received_message = false;
    std::error_code server_error;

    server.on_start = [&](const SimpleNamedPipe::ServerConfig&) {
        std::lock_guard<std::mutex> lock(mutex);
        server_started = true;
        cv.notify_all();
    };

    server.on_connected = [&](int) {
        std::lock_guard<std::mutex> lock(mutex);
        client_connected = true;
        cv.notify_all();
    };

    server.on_message = [&](int client_id, const std::string& message) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            server_received_message = true;
        }
        server.send_to(client_id, "Echo: " + message);
        cv.notify_all();
    };

    server.on_error = [&](const std::error_code& error) {
        std::lock_guard<std::mutex> lock(mutex);
        server_error = error;
        cv.notify_all();
    };

    server.start();

    if (!wait_for(cv, mutex, server_started, std::chrono::milliseconds(3000))) {
        std::cerr << "server did not start\n";
        server.stop();
        return EXIT_FAILURE;
    }

    SimpleNamedPipe::ClientConfig client_config(pipe_name, 4096, 3000);
    SimpleNamedPipe::NamedPipeClient client(client_config);

    std::error_code client_error;
    if (!client.connect(&client_error)) {
        std::cerr << "client connect failed: " << client_error.message() << "\n";
        server.stop();
        return EXIT_FAILURE;
    }

    if (!wait_for(cv, mutex, client_connected, std::chrono::milliseconds(3000))) {
        std::cerr << "server did not report client connection\n";
        client.close();
        server.stop();
        return EXIT_FAILURE;
    }

    if (!client.write("ping", &client_error)) {
        std::cerr << "client write failed: " << client_error.message() << "\n";
        client.close();
        server.stop();
        return EXIT_FAILURE;
    }

    if (!wait_for(cv, mutex, server_received_message, std::chrono::milliseconds(3000))) {
        std::cerr << "server did not receive client message\n";
        client.close();
        server.stop();
        return EXIT_FAILURE;
    }

    std::string response;
    if (!client.read(response, 3000, &client_error)) {
        std::cerr << "client read failed: " << client_error.message() << "\n";
        client.close();
        server.stop();
        return EXIT_FAILURE;
    }

    if (response != "Echo: ping") {
        std::cerr << "unexpected response: " << response << "\n";
        client.close();
        server.stop();
        return EXIT_FAILURE;
    }

    client.close();
    server.stop();

    if (server_error) {
        std::cerr << "server error: " << server_error.message() << "\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
