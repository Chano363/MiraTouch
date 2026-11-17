#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <thread>
#include <atomic>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>

class WebSocketHandler {
public:
    using MessageCallback = std::function<void(const std::string& message)>;
    using ConnectionCallback = std::function<void()>;
    
    WebSocketHandler();
    ~WebSocketHandler();
    
    bool start(int port);
    void stop();
    
    void broadcast(const std::string& message);
    void broadcastPerformanceData(const std::string& data);
    void sendToClient(const std::string& client_id, const std::string& message);
    
    void setMessageCallback(MessageCallback callback);
    void setConnectionCallback(ConnectionCallback callback);
    
    int getConnectionCount() const;
    
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
    std::thread server_thread_;
    std::atomic<bool> running_{false};
    
    MessageCallback message_callback_;
    ConnectionCallback connection_callback_;
};
