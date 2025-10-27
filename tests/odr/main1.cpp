#include <SimpleNamedPipe/NamedPipeServer.hpp>

int main() {
    SimpleNamedPipe::NamedPipeServer server;
    (void)server.is_running();
    return 0;
}
