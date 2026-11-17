#include <iostream>
#include <csignal>
#include <memory>
#include "server/server.h"

std::unique_ptr<RemoteDebuggerServer> g_server;
const int SERVER_PORT = 8082;

void signalHandler(int signal) {
    std::cout << "Received signal " << signal << ", shutting down..." << std::endl;
    if (g_server) {
        g_server->stop();
    }
}

int main() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    std::cout << "=== Remote Debugger Server ===" << std::endl;
    std::cout << "Starting server on port " << SERVER_PORT << "..." << std::endl;
    
    g_server = std::make_unique<RemoteDebuggerServer>();
    
    if (!g_server->initialize(SERVER_PORT)) {
        std::cerr << "Failed to initialize server!" << std::endl;
        return 1;
    }
    
    std::cout << "Server initialized successfully!" << std::endl;
    std::cout << "Access the web interface at: http://localhost:" << SERVER_PORT << std::endl;
    std::cout << "Press Ctrl+C to stop the server" << std::endl;
    
    g_server->run();
    
    std::cout << "Server stopped gracefully." << std::endl;
    return 0;
}