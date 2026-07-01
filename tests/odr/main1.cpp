#include <SimpleNamedPipe/NamedPipeServer.hpp>
#include <SimpleNamedPipe/NamedPipeClient.hpp>

int main() {
    SimpleNamedPipe::NamedPipeServer server;
    SimpleNamedPipe::NamedPipeClient client;
    (void)server.is_running();
    (void)client.connected();
    return 0;
}
