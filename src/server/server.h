#pragma once

#include <string>
#include <memory>
#include <thread>
#include <atomic>


class DeviceManager;
class WebSocketHandler;

class SimpleHTTPServer;

extern const int SERVER_PORT;

class RemoteDebuggerServer
{
public:
    RemoteDebuggerServer();  
    ~RemoteDebuggerServer(); 

    bool initialize(int port = SERVER_PORT);
    void run();
    void stop();

private:
    class Impl;
    void startWebServer();
    void startWebSocketServer();

    std::unique_ptr<DeviceManager> device_manager_;
    std::unique_ptr<WebSocketHandler> websocket_handler_;
    std::thread server_thread_;
    std::atomic<bool> running_{false};
    int port_;
    std::unique_ptr<Impl> impl_;
};