#include "server/http_server.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <iterator>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#endif

const size_t SimpleHTTPServer::MAX_CLIENTS = 100;

SimpleHTTPServer::SimpleHTTPServer(int port) : port_(port), running_(false), server_socket_(-1)
{
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif
}

SimpleHTTPServer::~SimpleHTTPServer()
{
    stop();
#ifdef _WIN32
    WSACleanup();
#endif
}

bool SimpleHTTPServer::start()
{
    if (running_)
        return true;

    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0)
    {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
#else
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port_);

    if (bind(server_socket_, (sockaddr *)&server_addr, sizeof(server_addr)) < 0)
    {
        std::cerr << "Failed to bind to port " << port_ << std::endl;
        return false;
    }

    if (listen(server_socket_, 10) < 0)
    {
        std::cerr << "Failed to listen on port " << port_ << std::endl;
        return false;
    }

    running_ = true;
    server_thread_ = std::thread(&SimpleHTTPServer::run, this);
    std::cout << "HTTP Server started on port " << port_ << std::endl;
    return true;
}

void SimpleHTTPServer::stop()
{
    if (!running_)
        return;

    running_ = false;
    if (server_socket_ >= 0)
    {
#ifdef _WIN32
        closesocket(server_socket_);
#else
        close(server_socket_);
#endif
        server_socket_ = -1;
    }

    if (server_thread_.joinable())
    {
        server_thread_.join();
    }
    std::cout << "HTTP Server stopped" << std::endl;
}

void SimpleHTTPServer::addRoute(const std::string &path, std::function<std::string(const std::string &)> handler)
{
    std::lock_guard<std::mutex> lock(routes_mutex_);
    routes_[path] = handler;
    std::cout << "[HTTP Server] Route registered: " << path << ", Total routes: " << routes_.size() << std::endl;
}

void SimpleHTTPServer::setStaticPath(const std::string &path)
{
    std::lock_guard<std::mutex> lock(static_path_mutex_);
    static_path_ = path;
}

void SimpleHTTPServer::run()
{
    while (running_)
    {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);

        int client_socket = accept(server_socket_, (sockaddr *)&client_addr, &client_len);
        if (client_socket < 0)
        {
            if (running_)
            {
                std::cerr << "Accept failed" << std::endl;
            }
            continue;
        }

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            if (clients_.size() >= MAX_CLIENTS)
            {
                std::cerr << "Too many clients, rejecting connection" << std::endl;
#ifdef _WIN32
                closesocket(client_socket);
#else
                close(client_socket);
#endif
                continue;
            }

            clients_.emplace_back(std::thread(&SimpleHTTPServer::handleClient, this, client_socket));
            clients_.back().detach();
        }
    }
}

// 在 SimpleHTTPServer::handleClient 方法中修改路由处理逻辑
void SimpleHTTPServer::handleClient(int client_socket)
{
    char buffer[4096];
    std::string request;
    int bytes_received;

    // 读取请求数据
    while (true)
    {
        bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
        if (bytes_received <= 0)
        {
            break;
        }
        buffer[bytes_received] = '\0';
        request += buffer;
        
        // 检查是否收到完整的请求
        if (request.find("\r\n\r\n") != std::string::npos)
        {
            // 检查是否有Content-Length头
            size_t content_length_pos = request.find("Content-Length:");
            if (content_length_pos != std::string::npos)
            {
                size_t end_of_line = request.find("\r\n", content_length_pos);
                std::string content_length_str = request.substr(content_length_pos + 16, end_of_line - (content_length_pos + 16));
                int content_length = std::stoi(content_length_str);
                
                // 计算已收到的请求体长度
                size_t body_start = request.find("\r\n\r\n") + 4;
                int body_length = request.size() - body_start;
                
                // 如果请求体还不完整，继续读取
                if (body_length < content_length)
                {
                    continue;
                }
            }
            break;
        }
    }

    if (bytes_received < 0)
    {
#ifdef _WIN32
        closesocket(client_socket);
#else
        close(client_socket);
#endif
        return;
    }

    // 检查是否有Expect: 100-continue头
    if (request.find("Expect: 100-continue") != std::string::npos)
    {
        // 发送100 Continue响应
        std::string continue_response = "HTTP/1.1 100 Continue\r\n\r\n";
        send(client_socket, continue_response.c_str(), continue_response.length(), 0);
        
        // 继续读取请求体
        std::string body;
        while (true)
        {
            bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            if (bytes_received <= 0)
            {
                break;
            }
            buffer[bytes_received] = '\0';
            body += buffer;
        }
        
        // 找到请求头和请求体的分界
        size_t body_start = request.find("\r\n\r\n");
        if (body_start != std::string::npos)
        {
            request = request.substr(0, body_start + 4) + body;
        }
    }

    std::cout << "=== Received HTTP Request ===" << std::endl;
    std::cout << request << std::endl;
    std::cout << "============================" << std::endl;

    std::istringstream request_stream(request);
    std::string method, full_path, version;
    request_stream >> method >> full_path >> version;

    // 提取路径部分，去掉查询参数
    std::string path = full_path;
    size_t query_pos = path.find('?');
    if (query_pos != std::string::npos) {
        path = path.substr(0, query_pos);
    }

    std::cout << "Full path: " << full_path << std::endl;
    std::cout << "Parsed path: " << path << std::endl;
    std::cout << "Method: " << method << ", Version: " << version << std::endl;

    std::string response;

    // 处理 CORS 预检请求
    if (method == "OPTIONS")
    {
        std::cout << "Handling CORS preflight request" << std::endl;
        response = createCORSResponse();
    }
    else
    {
        bool route_found = false;
        {
            std::lock_guard<std::mutex> lock(routes_mutex_);
            std::cout << "Registered routes count: " << routes_.size() << std::endl;

            for (const auto &route : routes_)
            {
                std::cout << "  Registered route: " << route.first << std::endl;
            }

            if (routes_.find(path) != routes_.end())
            {
                std::cout << "Route found for path: " << path << std::endl;
                try {
                    response = createJSONResponse(routes_[path](request));
                    route_found = true;
                } catch (const std::exception &e) {
                    std::cerr << "Error handling route: " << e.what() << std::endl;
                    response = createErrorResponse(500, "Internal Server Error");
                }
            }
            else
            {
                std::cout << "No route found for path: " << path << std::endl;
            }
        }

        if (!route_found)
        {
            if (!static_path_.empty() && path != "/")
            {
                std::lock_guard<std::mutex> lock(static_path_mutex_);
                std::cout << "Serving static file for path: " << path << std::endl;
                response = serveStaticFile(path);
            }
            else
            {
                std::lock_guard<std::mutex> lock(static_path_mutex_);
                std::cout << "Serving index page" << std::endl;
                response = serveIndexPage();
            }
        }
    }

    std::cout << "Sending response..." << std::endl;
    send(client_socket, response.c_str(), response.length(), 0);

#ifdef _WIN32
    closesocket(client_socket);
#else
    close(client_socket);
#endif
}

// 添加创建 CORS 响应的方法
std::string SimpleHTTPServer::createCORSResponse()
{
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    response << "Access-Control-Max-Age: 86400\r\n";
    response << "Content-Length: 0\r\n";
    response << "\r\n";
    return response.str();
}

std::string SimpleHTTPServer::createJSONResponse(const std::string &content)
{
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: application/json\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    response << "\r\n";
    response << content;
    return response.str();
}

std::string SimpleHTTPServer::serveStaticFile(const std::string &path)
{
    std::string normalized_path = path;

    if (!normalized_path.empty() && normalized_path[0] == '/')
    {
        normalized_path = normalized_path.substr(1);
    }

    if (normalized_path.empty() || normalized_path.find("..") != std::string::npos ||
        normalized_path[0] == '/' || normalized_path.find("\\") != std::string::npos)
    {
        return createErrorResponse(403, "Forbidden");
    }

    std::string file_path = static_path_ + "/" + normalized_path;

    std::ifstream file(file_path, std::ios::binary);
    if (!file)
    {
        return createErrorResponse(404, "File not found");
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    std::string content_type = getContentType(file_path);

    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: " << content_type << "\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    response << "\r\n";
    response << content;
    return response.str();
}

std::string SimpleHTTPServer::serveIndexPage()
{
    std::string index_path = static_path_ + "/index.html";
    std::ifstream file(index_path, std::ios::binary);
    if (!file)
    {
        return createHTMLResponse("<html><body><h1>Remote Debugger Server</h1><p>Frontend files not found. Please build the frontend first.</p></body></html>");
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return createHTMLResponse(content);
}

std::string SimpleHTTPServer::createHTMLResponse(const std::string &content)
{
    std::stringstream response;
    response << "HTTP/1.1 200 OK\r\n";
    response << "Content-Type: text/html; charset=utf-8\r\n";
    response << "Content-Length: " << content.length() << "\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    response << "\r\n";
    response << content;
    return response.str();
}

std::string SimpleHTTPServer::createErrorResponse(int code, const std::string &message)
{
    std::stringstream response;
    response << "HTTP/1.1 " << code << " " << message << "\r\n";
    response << "Content-Type: text/plain\r\n";
    response << "Access-Control-Allow-Origin: *\r\n";
    response << "Access-Control-Allow-Methods: GET, POST, PUT, DELETE, OPTIONS\r\n";
    response << "Access-Control-Allow-Headers: Content-Type, Authorization\r\n";
    response << "\r\n";
    response << message;
    return response.str();
}

std::string SimpleHTTPServer::getContentType(const std::string &file_path)
{
    if (file_path.length() >= 5 && file_path.substr(file_path.length() - 5) == ".html")
        return "text/html";
    if (file_path.length() >= 4 && file_path.substr(file_path.length() - 4) == ".css")
        return "text/css";
    if (file_path.length() >= 3 && file_path.substr(file_path.length() - 3) == ".js")
        return "application/javascript";
    if (file_path.length() >= 5 && file_path.substr(file_path.length() - 5) == ".json")
        return "application/json";
    return "text/plain";
}
