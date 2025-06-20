#include "SimpleNamedPipe/NamedPipeServer.hpp"
#include <iostream>

using namespace SimpleNamedPipe;

int main() {
    ServerConfig config;
    config.pipe_name = "ExamplePipe";
    config.buffer_size = 1024;
    config.timeout = 5000;

    NamedPipeServer server(config);
    std::array<int, 256> message_counters{};

    server.on_event = [&server, &message_counters](const ServerEvent& ev) {
        switch (ev.type) {
        case ServerEventType::ServerStarted:
            std::cout << "Server started on pipe: " << server.get_config().pipe_name << std::endl;
            break;
        case ServerEventType::ServerStopped:
            std::cout << "Server stopped." << std::endl;
            break;
        case ServerEventType::ClientConnected:
            std::cout << "client(" << ev.client_id << ") connected." << std::endl;
            break;
        case ServerEventType::ClientDisconnected:
            std::cout << "client(" << ev.client_id << ") disconnected: " << ev.error.message() << std::endl;
            break;
        case ServerEventType::MessageReceived:
            std::cout << "client(" << ev.client_id << ") received: " << ev.message << std::endl;
            if (ev.connection) {
                ev.connection->send("Echo: " + ev.message);
                message_counters[static_cast<size_t>(ev.client_id)]++;
                if (message_counters[static_cast<size_t>(ev.client_id)] >= 10) {
                    ev.connection->close();
                }
            }
            break;
        case ServerEventType::ErrorOccurred:
            std::cerr << "Error: " << ev.error.message() << std::endl;
            break;
        }
    };

    std::cout << "Press Enter to stop the server..." << std::endl;
    server.start();
    std::cin.get();
    server.stop();
    return 0;
}
