#include "SimpleNamedPipe/NamedPipeServer.hpp"
#include <iostream>
#include <chrono>

using namespace SimpleNamedPipe;

int main() {
    // Server configuration
    ServerConfig config;
    config.pipe_name = "ExamplePipe";
    config.buffer_size = 1024;
    config.timeout = 5000;

    NamedPipeServer server(config);

    // Counter for messages from each client
    std::array<int, 256> message_counters{};

    // Callbacks
    server.on_connected = [](int client_id) {
        std::cout << "client(" << client_id << ") connected." << std::endl;
    };

    server.on_disconnected = [](int client_id, const std::error_code& ec) {
        std::cout << "client(" << client_id << ") disconnected: " << ec.message() << std::endl;
    };

    server.on_message = [&server, &message_counters](int client_id, const std::string& message) {
        std::cout << "client(" << client_id << ") received: " << message << std::endl;

        // Echo the message back
        server.send_to(client_id, "Echo: " + message);
        message_counters[static_cast<size_t>(client_id)]++;

        // Disconnect after 10 messages
        if (message_counters[static_cast<size_t>(client_id)] >= 10) {
            server.close(client_id);
        }
    };

    server.on_start = [](const ServerConfig& cfg) {
        std::cout << "Server started on pipe: " << cfg.pipe_name << std::endl;
    };

    server.on_stop = [](const ServerConfig&) {
        std::cout << "Server stopped." << std::endl;
    };

    server.on_error = [](const std::error_code& error) {
        std::cerr << "Error: " << error.message() << std::endl;
    };

    std::cout << "Press Enter to stop the server..." << std::endl;
    server.start();

    std::cin.get();

    server.stop();
    return 0;
}
