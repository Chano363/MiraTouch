#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include <string>
#include <functional>
#include <map>
#include <thread>
#include <mutex>
#include <vector>

class SimpleHTTPServer
{
public:
    SimpleHTTPServer(int port);
    ~SimpleHTTPServer();

    bool start();
    void stop();
    void addRoute(const std::string &path, std::function<std::string(const std::string &)> handler);
    void setStaticPath(const std::string &path);

private:
    void run();
    void handleClient(int client_socket);
    std::string createJSONResponse(const std::string &content);
    std::string serveStaticFile(const std::string &path);
    std::string serveIndexPage();
    std::string createHTMLResponse(const std::string &content);
    std::string createErrorResponse(int code, const std::string &message);
    std::string getContentType(const std::string &file_path);
    std::string createCORSResponse();

    int port_;
    bool running_;
    int server_socket_;
    std::thread server_thread_;
    std::map<std::string, std::function<std::string(const std::string &)>> routes_;
    std::string static_path_;
    std::mutex routes_mutex_;
    std::mutex static_path_mutex_;
    std::mutex clients_mutex_;
    std::vector<std::thread> clients_;
    static const size_t MAX_CLIENTS;
};

#endif