#include "server/websocket_handler.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <sstream>
#include <random>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include <map>
#include <mutex>

typedef websocketpp::server<websocketpp::config::asio> server;

struct ClientInfo
{
    websocketpp::connection_hdl hdl;
    std::string id;
    std::chrono::steady_clock::time_point connect_time;

    ClientInfo(websocketpp::connection_hdl hdl, const std::string &id)
        : hdl(hdl), id(id), connect_time(std::chrono::steady_clock::now()) {}
};
class WebSocketHandler::Impl
{
private:
    server endpoint_;
    std::thread server_thread_;
    WebSocketHandler *parent_;
    std::map<websocketpp::connection_hdl, std::shared_ptr<ClientInfo>, std::owner_less<websocketpp::connection_hdl>> connections_;
    std::map<std::string, websocketpp::connection_hdl> client_ids_;
    mutable std::mutex connections_mutex_;
    
    // 日志级别控制
    enum LogLevel {
        LOG_DEBUG,
        LOG_INFO,
        LOG_WARN,
        LOG_ERROR,
        LOG_OFF
    };
    
    LogLevel currentLogLevel = LOG_ERROR;
    
    // 日志计数，用于批量输出
    int logCounter = 0;
    const int LOG_BATCH_THRESHOLD = 5;
    
    // 日志节流
    std::map<std::string, std::chrono::steady_clock::time_point> logThrottleCache; 

public:
    Impl(WebSocketHandler *parent) : parent_(parent) {}
    
    // 日志工具方法
    bool shouldLog(LogLevel level) {
        return level >= currentLogLevel;
    }
    
    // 日志节流方法
    bool shouldThrottleLog(const std::string &key, int intervalMs = 1000) {
        auto now = std::chrono::steady_clock::now();
        auto it = logThrottleCache.find(key);
        
        if (it == logThrottleCache.end()) {
            logThrottleCache[key] = now;
            return false;
        }
        
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
        if (elapsed >= intervalMs) {
            logThrottleCache[key] = now;
            return false;
        }
        
        return true;
    }
    
    // 批量日志计数
    void incrementLogCounter() {
        logCounter++;
        if (logCounter >= LOG_BATCH_THRESHOLD) {
            // 禁用批量消息处理日志
            // 不再输出 [WebSocketHandler] Processed X messages 日志
            logCounter = 0;
        }
    }
    
    // 清理过期的节流缓存
    void cleanupThrottleCache() {
        auto now = std::chrono::steady_clock::now();
        for (auto it = logThrottleCache.begin(); it != logThrottleCache.end();) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
            if (elapsed >= 5000) { // 5秒过期
                it = logThrottleCache.erase(it);
            } else {
                ++it;
            }
        }
    }

    bool start(int port)
    {
        try
        {
            std::cout << "Starting WebSocket server on port " << port << std::endl;

            endpoint_.set_access_channels(websocketpp::log::alevel::none);
            endpoint_.clear_access_channels(websocketpp::log::alevel::all);

            endpoint_.init_asio();

            endpoint_.set_message_handler([this](websocketpp::connection_hdl hdl, server::message_ptr msg)
                                          {
                if (parent_->message_callback_) {
                    parent_->message_callback_(msg->get_payload());
                } });

            endpoint_.set_open_handler([this](websocketpp::connection_hdl hdl)
                                       {
                std::string client_id = generateClientId();
                
                {
                    std::lock_guard<std::mutex> lock(connections_mutex_);
                    auto client_info = std::make_shared<ClientInfo>(hdl, client_id);
                    connections_[hdl] = client_info;
                    client_ids_[client_id] = hdl;
                }
                
                if (parent_->connection_callback_) {
                    parent_->connection_callback_();
                }
                // 不再输出WebSocket连接打开日志
                // std::cout << "WebSocket connection opened. Client ID: " << client_id 
                //           << ", Total connections: " << getConnectionCount() << std::endl;
            });

            endpoint_.set_close_handler([this](websocketpp::connection_hdl hdl)
                                        {
                {
                    std::lock_guard<std::mutex> lock(connections_mutex_);
                    auto it = connections_.find(hdl);
                    if (it != connections_.end()) {
                        std::string client_id = it->second->id;
                        connections_.erase(hdl);
                        client_ids_.erase(client_id);
                    }
                }
                // 不再输出WebSocket连接关闭日志
                // std::cout << "WebSocket connection closed. Total connections: " << getConnectionCount() << std::endl;
            });

            endpoint_.listen(port);
            endpoint_.start_accept();

            server_thread_ = std::thread([this]()
                                         {
                std::cout << "WebSocket server thread started" << std::endl;
                endpoint_.run(); });

            std::cout << "WebSocket server successfully started on port " << port << std::endl;
            return true;
        }
        catch (const std::exception &e)
        {
            std::cerr << "WebSocket server error: " << e.what() << std::endl;
            return false;
        }
        catch (...)
        {
            std::cerr << "WebSocket server unknown error" << std::endl;
            return false;
        }
    }

    void stop()
    {
        try
        {
            if (endpoint_.is_listening())
            {
                endpoint_.stop_listening();
            }

            std::lock_guard<std::mutex> lock(connections_mutex_);
            for (auto &pair : connections_)
            {
                websocketpp::lib::error_code ec;
                endpoint_.close(pair.second->hdl, websocketpp::close::status::going_away, "Server shutdown", ec);
                if (ec)
                {
                    std::cerr << "Error closing connection: " << ec.message() << std::endl;
                }
            }

            if (server_thread_.joinable())
            {
                server_thread_.join();
            }

            std::cout << "WebSocket server stopped" << std::endl;
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error stopping WebSocket server: " << e.what() << std::endl;
        }
    }

    void broadcastPerformanceData(const std::string &data)
    {
        broadcast(data);
    }

    void broadcast(const std::string &message)
    {
        try
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            
            // 检测消息类型，特别是心跳消息
            bool is_heartbeat = false;
            if (message.find("\"type\":\"heartbeat\"") != std::string::npos) {
                is_heartbeat = true;
            }
            
            // 只在非心跳消息时输出详细日志，并应用日志节流
            if (!is_heartbeat) {
                // 不再输出广播消息日志
                // if (!shouldThrottleLog("broadcast_message", 2000)) {
                //     if (shouldLog(LOG_INFO)) {
                //         std::cout << "[WebSocketHandler] Broadcasting message to " << connections_.size() << " clients" << std::endl;
                //     }
                // }
            } else {
                // 不再输出心跳消息日志
                // if (!shouldThrottleLog("broadcast_heartbeat", 5000)) {
                //     if (shouldLog(LOG_DEBUG)) {
                //         std::cout << "[WebSocketHandler] Broadcasting heartbeat message to " << connections_.size() << " clients" << std::endl;
                //     }
                // }
            }
            
            int success_count = 0;
            int error_count = 0;
            
            for (auto &pair : connections_)
            {
                try
                {
                    endpoint_.send(pair.second->hdl, message, websocketpp::frame::opcode::text);
                    success_count++;
                }
                catch (const std::exception &e)
                {
                    if (shouldLog(LOG_ERROR)) {
                        std::cerr << "Error sending message to client: " << e.what() << std::endl;
                    }
                    error_count++;
                }
            }
            
            // 批量日志计数
            incrementLogCounter();
            
            // 不再输出广播结果日志
            // if (!is_heartbeat) {
            //     if (!shouldThrottleLog("broadcast_result", 3000)) {
            //         if (shouldLog(LOG_INFO)) {
            //             std::cout << "[WebSocketHandler] Message sent successfully to " << success_count << " clients" << std::endl;
            //         }
            //     }
            // }
            
            if (error_count > 0) {
                if (shouldLog(LOG_WARN)) {
                    std::cerr << "[WebSocketHandler] Failed to send message to " << error_count << " clients" << std::endl;
                }
            }
            
            // 定期清理节流缓存
            cleanupThrottleCache();
        }
        catch (const std::exception &e)
        {
            if (shouldLog(LOG_ERROR)) {
                std::cerr << "Error broadcasting message: " << e.what() << std::endl;
            }
        }
    }

    void sendToClient(const std::string &client_id, const std::string &message)
    {
        try
        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            auto it = client_ids_.find(client_id);
            if (it != client_ids_.end())
            {
                try
                {
                    endpoint_.send(it->second, message, websocketpp::frame::opcode::text);
                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error sending message to client " << client_id << ": " << e.what() << std::endl;
                }
            }
            else
            {
                std::cerr << "Client with ID " << client_id << " not found" << std::endl;
            }
        }
        catch (const std::exception &e)
        {
            std::cerr << "Error sending message to client " << client_id << ": " << e.what() << std::endl;
        }
    }

    int getConnectionCount() const
    {
        std::lock_guard<std::mutex> lock(connections_mutex_);
        return static_cast<int>(connections_.size());
    }

private:
    std::string generateClientId()
    {
        auto now = std::chrono::high_resolution_clock::now();
        auto duration = now.time_since_epoch();
        auto nanoseconds = std::chrono::duration_cast<std::chrono::nanoseconds>(duration).count();

        static std::random_device rd;
        static std::mt19937 gen(rd());
        static std::uniform_int_distribution<> dis(100000, 999999);

        std::stringstream ss;
        ss << "client_" << std::hex << nanoseconds << "_" << dis(gen);
        return ss.str();
    }
};

WebSocketHandler::WebSocketHandler() : impl_(std::make_unique<Impl>(this)), running_(false) {}

WebSocketHandler::~WebSocketHandler()
{
    stop();
}

bool WebSocketHandler::start(int port)
{
    if (running_)
    {
        return true;
    }

    bool result = impl_->start(port);
    if (result)
    {
        running_ = true;
    }
    return result;
}

void WebSocketHandler::stop()
{
    if (!running_)
    {
        return;
    }

    impl_->stop();
    running_ = false;
}

void WebSocketHandler::broadcast(const std::string &message)
{
    impl_->broadcast(message);
}

void WebSocketHandler::sendToClient(const std::string &client_id, const std::string &message)
{
    impl_->sendToClient(client_id, message);
}

int WebSocketHandler::getConnectionCount() const
{
    return impl_->getConnectionCount();
}

void WebSocketHandler::setMessageCallback(MessageCallback callback)
{
    message_callback_ = callback;
}

void WebSocketHandler::setConnectionCallback(ConnectionCallback callback)
{
    connection_callback_ = callback;
}