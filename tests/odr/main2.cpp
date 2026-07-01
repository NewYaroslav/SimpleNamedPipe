#include <SimpleNamedPipe/NamedPipeServer.hpp>
#include <SimpleNamedPipe/NamedPipeClient.hpp>

void run_server() {
    SimpleNamedPipe::NamedPipeServer server;
    SimpleNamedPipe::NamedPipeClient client;
    server.stop();
    client.close();
}
