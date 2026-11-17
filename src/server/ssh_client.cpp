#include "ssh_client.h"
#include <atomic>
#include <cstring>
#include <iostream>
#include <libssh2.h>
#include <libssh2_sftp.h>
#include <thread>
#include <mutex>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#endif

class SSHClient::Impl
{
private:
    LIBSSH2_SESSION *session_;
    LIBSSH2_CHANNEL *channel_;
    int socket_;
    bool connected_;
    std::string last_error_;
    OutputCallback async_output_callback_;
    std::thread output_thread_;
    std::atomic<bool> reading_output_;
    std::mutex callback_mutex_;

    // 添加输出缓冲区
    std::string output_buffer_;
    std::mutex buffer_mutex_;
    std::condition_variable output_available_;

public:
    Impl()
        : session_(nullptr), channel_(nullptr), socket_(-1), connected_(false),
          reading_output_(false) {}

    ~Impl() { disconnect(); }
    bool connect(const std::string &host, int port, const std::string &username,
                 const std::string &password)
    { // 初始化WinSock（仅Windows）
#ifdef _WIN32
        WSADATA wsaData;
        int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (res != 0)
        {
            last_error_ = "WSAStartup failed";
            return false;
        }
#endif

        // 创建socket
        socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ < 0)
        {
            last_error_ = "Failed to create socket";
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }

        // 解析主机名
        struct sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        sin.sin_addr.s_addr = inet_addr(host.c_str());
        if (sin.sin_addr.s_addr == INADDR_NONE)
        {
            struct hostent *he = gethostbyname(host.c_str());
            if (!he)
            {
                last_error_ = "Failed to resolve hostname";
                closeSocket();
                return false;
            }
            memcpy(&sin.sin_addr, he->h_addr, he->h_length);
        }

        // 连接
        if (::connect(socket_, (struct sockaddr *)(&sin), sizeof(struct sockaddr_in)) !=
            0)
        {
            last_error_ = "Failed to connect to server";
            closeSocket();
            return false;
        }

        // 初始化libssh2
        int rc = libssh2_init(0);
        if (rc != 0)
        {
            last_error_ = "libssh2 initialization failed";
            closeSocket();
            return false;
        }

        // 创建SSH会话
        session_ = libssh2_session_init();
        if (!session_)
        {
            last_error_ = "Failed to create SSH session";
            libssh2_exit();
            closeSocket();
            return false;
        }

        // 设置会话超时
        libssh2_session_set_timeout(session_, 30000); // 默认30秒超时
        libssh2_session_set_blocking(session_, 0);

        // 握手
        while ((rc = libssh2_session_handshake(session_, socket_)) ==
               LIBSSH2_ERROR_EAGAIN)
            ;
        if (rc)
        {
            last_error_ = "SSH handshake failed";
            libssh2_session_free(session_);
            session_ = nullptr;
            libssh2_exit();
            closeSocket();
            return false;
        }

        // 认证
        while ((rc = libssh2_userauth_password(session_, username.c_str(),
                                               password.c_str())) ==
               LIBSSH2_ERROR_EAGAIN)
            ;
        if (rc)
        {
            last_error_ = "Authentication failed";
            libssh2_session_disconnect(session_, "Normal Shutdown");
            libssh2_session_free(session_);
            session_ = nullptr;
            libssh2_exit();
            closeSocket();
            return false;
        }

        connected_ = true;
        return true;
    }

    bool connectWithConfig(const std::string &host, int port, const std::string &username,
                          const std::string &password, int timeout_seconds)
    {
        // 初始化WinSock（仅Windows）
#ifdef _WIN32
        WSADATA wsaData;
        int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (res != 0)
        {
            last_error_ = "WSAStartup failed";
            return false;
        }
#endif

        // 创建socket
        socket_ = socket(AF_INET, SOCK_STREAM, 0);
        if (socket_ < 0)
        {
            last_error_ = "Failed to create socket";
#ifdef _WIN32
            WSACleanup();
#endif
            return false;
        }

        // 设置socket超时
#ifdef _WIN32
        DWORD timeout_ms = timeout_seconds * 1000;
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
        setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, (char*)&timeout_ms, sizeof(timeout_ms));
#else
        struct timeval timeout;
        timeout.tv_sec = timeout_seconds;
        timeout.tv_usec = 0;
        setsockopt(socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        setsockopt(socket_, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif

        // 解析主机名
        struct sockaddr_in sin;
        sin.sin_family = AF_INET;
        sin.sin_port = htons(port);
        sin.sin_addr.s_addr = inet_addr(host.c_str());
        if (sin.sin_addr.s_addr == INADDR_NONE)
        {
            struct hostent *he = gethostbyname(host.c_str());
            if (!he)
            {
                last_error_ = "Failed to resolve hostname";
                closeSocket();
                return false;
            }
            memcpy(&sin.sin_addr, he->h_addr, he->h_length);
        }

        // 连接
        if (::connect(socket_, (struct sockaddr *)(&sin), sizeof(struct sockaddr_in)) !=
            0)
        {
            last_error_ = "Failed to connect to server";
            closeSocket();
            return false;
        }

        // 初始化libssh2
        int rc = libssh2_init(0);
        if (rc != 0)
        {
            last_error_ = "libssh2 initialization failed";
            closeSocket();
            return false;
        }

        // 创建SSH会话
        session_ = libssh2_session_init();
        if (!session_)
        {
            last_error_ = "Failed to create SSH session";
            libssh2_exit();
            closeSocket();
            return false;
        }

        // 设置会话超时
        libssh2_session_set_timeout(session_, timeout_seconds * 1000); // 超时毫秒
        libssh2_session_set_blocking(session_, 0);

        // 握手
        while ((rc = libssh2_session_handshake(session_, socket_)) ==
               LIBSSH2_ERROR_EAGAIN)
            ;
        if (rc)
        {
            last_error_ = "SSH handshake failed";
            libssh2_session_free(session_);
            session_ = nullptr;
            libssh2_exit();
            closeSocket();
            return false;
        }

        // 认证
        while ((rc = libssh2_userauth_password(session_, username.c_str(),
                                               password.c_str())) ==
               LIBSSH2_ERROR_EAGAIN)
            ;
        if (rc)
        {
            last_error_ = "Authentication failed";
            libssh2_session_disconnect(session_, "Normal Shutdown");
            libssh2_session_free(session_);
            session_ = nullptr;
            libssh2_exit();
            closeSocket();
            return false;
        }

        connected_ = true;
        return true;
    }

    void disconnect()
    {
        reading_output_ = false;

        // 等待输出读取线程结束
        if (output_thread_.joinable())
        {
            output_thread_.join();
        }

        if (channel_)
        {
            libssh2_channel_close(channel_);
            libssh2_channel_free(channel_);
            channel_ = nullptr;
        }

        if (session_)
        {
            libssh2_session_disconnect(session_, "Normal Shutdown");
            libssh2_session_free(session_);
            session_ = nullptr;
        }

        if (socket_ >= 0)
        {
#ifdef _WIN32
            closesocket(socket_);
#else
            close(socket_);
#endif
            socket_ = -1;
        }

        connected_ = false;
    }

    bool isConnected() const { return connected_; }

    bool startInteractiveSession(OutputCallback callback)
    {
        if (!connected_ || !session_)
        {
            return false;
        }

        // 创建通道
        while ((channel_ = libssh2_channel_open_session(session_)) ==
               (LIBSSH2_CHANNEL *)LIBSSH2_ERROR_EAGAIN)
            ;

        if (!channel_)
        {
            last_error_ = "Failed to open SSH channel";
            return false;
        }

        // 请求PTY
        int rc;
        while ((rc = libssh2_channel_request_pty(channel_, "vanilla")) ==
               LIBSSH2_ERROR_EAGAIN)
            ;

        if (rc)
        {
            last_error_ = "Failed to request PTY";
            libssh2_channel_free(channel_);
            channel_ = nullptr;
            return false;
        }

        // 启动shell
        while ((rc = libssh2_channel_shell(channel_)) == LIBSSH2_ERROR_EAGAIN)
            ;

        if (rc)
        {
            last_error_ = "Failed to start shell";
            libssh2_channel_free(channel_);
            channel_ = nullptr;
            return false;
        }

        // 设置回调
        async_output_callback_ = callback;

        // 启动输出读取线程
        // Removed debug output to prevent console spamming
        // std::cout << "[SSHClient::Impl] Starting output reader thread" << std::endl;

        reading_output_ = true;
        output_thread_ = std::thread([this]()
                                     {
            char buffer[1024];
            int bytes_read;

            while (reading_output_ && connected_ && channel_) {
                bytes_read = libssh2_channel_read(channel_, buffer, sizeof(buffer) - 1);
                if (bytes_read > 0) {
                    buffer[bytes_read] = '\0';
                    std::string output(buffer, bytes_read);
                    
                    // 更新输出缓冲区
                    {
                        std::lock_guard<std::mutex> lock(buffer_mutex_);
                        output_buffer_ += output;
                    }
                    output_available_.notify_all();
                    
                    // 线程安全地访问回调函数
                    OutputCallback callback_copy;
                    {
                        std::lock_guard<std::mutex> lock(callback_mutex_);
                        callback_copy = async_output_callback_;
                    }
                    if (callback_copy) {
                        // std::cout << "[SSHClient::Impl] Calling async output callback with: " << output << std::endl;
                        callback_copy(output);
                    } else {
                        // std::cout << "[SSHClient::Impl] No async output callback set" << std::endl;
                    }
                } else if (bytes_read == LIBSSH2_ERROR_EAGAIN) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } else if (bytes_read < 0) {
                    // 检查是否是EAGAIN错误
                    int error_code = libssh2_session_last_errno(session_);
                    if (error_code != LIBSSH2_ERROR_EAGAIN) {
                        // std::cerr << "[SSHClient::Impl] Error reading from channel: " << bytes_read 
                        //           << " (error code: " << error_code << ")" << std::endl;
                        break;
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                } else {
                    // bytes_read == 0, 没有数据可读
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            }
            // std::cout << "[SSHClient::Impl] Output reader thread exiting" << std::endl;
        });

        return true;
    }

    void sendInput(const std::string &input)
    {
        if (channel_ && connected_)
        {
            std::string cmd_with_newline = input;
            if (cmd_with_newline.back() != '\n')
            {
                cmd_with_newline += "\n";
            }

            int bytes_written = 0;
            int total_bytes = static_cast<int>(cmd_with_newline.length());
            const char *data = cmd_with_newline.c_str();

            while (bytes_written < total_bytes)
            {
                int result = libssh2_channel_write(channel_, data + bytes_written, total_bytes - bytes_written);
                if (result > 0)
                {
                    bytes_written += result;
                }
                else if (result == LIBSSH2_ERROR_EAGAIN)
                {
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
                else
                {
                    // 错误处理
                    std::cerr << "[SSHClient::Impl] Error writing to channel: " << result << std::endl;
                    break;
                }
            }
        }
    }

    std::string getLastError() const { return last_error_; }

    // 添加获取命令输出的方法
    std::string waitForOutput(const std::string &command, std::chrono::seconds timeout)
    {
        std::unique_lock<std::mutex> lock(buffer_mutex_);

        // 记录开始时的缓冲区大小
        size_t initial_size = output_buffer_.size();

        // 等待输出可用或超时
        output_available_.wait_for(lock, timeout, [this, initial_size]
                                   { return output_buffer_.size() > initial_size; });

        // 返回新添加的输出
        if (output_buffer_.size() > initial_size)
        {
            return output_buffer_.substr(initial_size);
        }

        return "";
    }

    // 清空输出缓冲区
    void clearOutputBuffer()
    {
        std::lock_guard<std::mutex> lock(buffer_mutex_);
        output_buffer_.clear();
    }

    bool executeCommand(const std::string &command, OutputCallback callback)
    {
        // Removed debug output to prevent console spamming
        // std::cout << "[SSHClient::Impl] Attempting to execute command: " << command << std::endl;

        if (!connected_ || !session_)
        {
            // std::cout << "[SSHClient::Impl] Error: Not connected or session is null" << std::endl;
            // std::cout << "[SSHClient::Impl] Connected: " << connected_ << ", Session: " << (session_ ? "valid" : "null") << std::endl;
            return false;
        }

        LIBSSH2_CHANNEL *exec_channel = nullptr;
        int rc;

        // 创建执行通道 - 如果当前有活动的交互式通道，需要使用同一会话创建新通道
        // Removed debug output to prevent console spamming
        // std::cout << "[SSHClient::Impl] Opening SSH channel for command execution..." << std::endl;
        int attempts = 0;
        const int max_attempts = 10;

        while (attempts < max_attempts)
        {
            exec_channel = libssh2_channel_open_session(session_);
            if (exec_channel)
            {
                // std::cout << "[SSHClient::Impl] SSH channel opened successfully" << std::endl;
                break;
            }

            int error_code = libssh2_session_last_errno(session_);
            if (error_code == LIBSSH2_ERROR_EAGAIN)
            {
                // std::cout << "[SSHClient::Impl] Channel open would block, attempt " << attempts + 1 << "/" << max_attempts << ", waiting..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                attempts++;
            }
            else
            {
                char *error_msg;
                int error_len;
                int session_error = libssh2_session_last_error(session_, &error_msg, &error_len, 0);
                // std::cout << "[SSHClient::Impl] Failed to open SSH channel, error: " << session_error
                //           << ", message: " << std::string(error_msg, error_len) << std::endl;
                last_error_ = "Failed to open SSH channel for command execution, error: " + std::to_string(session_error);
                return false;
            }
        }

        if (!exec_channel)
        {
            std::cout << "[SSHClient::Impl] Failed to open SSH channel after " << max_attempts << " attempts" << std::endl;
            last_error_ = "Failed to open SSH channel for command execution after multiple attempts";
            return false;
        }

        // 执行命令
        // Removed debug output to prevent console spamming
        // std::cout << "[SSHClient::Impl] Executing command: " << command << std::endl;
        attempts = 0;
        while (attempts < max_attempts)
        {
            rc = libssh2_channel_exec(exec_channel, command.c_str());
            if (rc == 0)
            {
                // std::cout << "[SSHClient::Impl] Command executed successfully, starting to read output..." << std::endl;
                break;
            }

            if (rc == LIBSSH2_ERROR_EAGAIN)
            {
                // std::cout << "[SSHClient::Impl] Command execution would block, attempt " << attempts + 1 << "/" << max_attempts << ", waiting..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                attempts++;
            }
            else
            {
                // std::cout << "[SSHClient::Impl] Failed to execute command, error code: " << rc << std::endl;
                char *error_msg;
                int error_len;
                int session_error = libssh2_session_last_error(session_, &error_msg, &error_len, 0);
                // std::cout << "[SSHClient::Impl] Session error: " << session_error
                //           << ", message: " << std::string(error_msg, error_len) << std::endl;

                last_error_ = "Failed to execute command, error code: " + std::to_string(rc);
                libssh2_channel_free(exec_channel);
                return false;
            }
        }

        if (rc != 0)
        {
            std::cout << "[SSHClient::Impl] Failed to execute command after " << max_attempts << " attempts" << std::endl;
            libssh2_channel_free(exec_channel);
            return false;
        }

        // 读取完整输出（等待命令执行完成）
        char buffer[4096];
        int bytes_read;
        std::string output;
        auto timeout = std::chrono::seconds(30); // 设置30秒超时
        auto start_time = std::chrono::steady_clock::now();
        bool command_finished = false;

        // 读取输出直到命令执行完成或超时
        while (!command_finished && std::chrono::steady_clock::now() - start_time < timeout)
        {
            bytes_read = libssh2_channel_read(exec_channel, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0)
            {
                buffer[bytes_read] = '\0';
                output += std::string(buffer, bytes_read);
                
                // 每次收到数据后重置开始时间，避免因长时间输出导致的超时
                start_time = std::chrono::steady_clock::now();
            }
            else if (bytes_read == LIBSSH2_ERROR_EAGAIN)
            {
                // std::cout << "[SSHClient::Impl] Read would block, waiting..." << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                
                // 检查命令是否完成
                if (libssh2_channel_eof(exec_channel))
                {
                    // std::cout << "[SSHClient::Impl] Command execution finished (EOF reached after EAGAIN)" << std::endl;
                    command_finished = true;
                }
            }
            else if (bytes_read < 0)
            {
                int error_code = libssh2_session_last_errno(session_);
                // std::cout << "[SSHClient::Impl] Error reading from channel: " << bytes_read
                //           << " (error code: " << error_code << ")" << std::endl;
                if (error_code != LIBSSH2_ERROR_EAGAIN)
                {
                    break; // 错误情况，退出循环
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
                
                // 检查命令是否完成
                if (libssh2_channel_eof(exec_channel))
                {
                    // std::cout << "[SSHClient::Impl] Command execution finished (EOF reached after error)" << std::endl;
                    command_finished = true;
                }
            }
            else
            {
                // bytes_read == 0, 没有立即可用的数据
                // 检查命令是否执行完成
                if (libssh2_channel_eof(exec_channel))
                {
                    // std::cout << "[SSHClient::Impl] Command execution finished (EOF reached)" << std::endl;
                    command_finished = true;
                }
                else
                {
                    // 继续等待更多数据
                    std::this_thread::sleep_for(std::chrono::milliseconds(50));
                }
            }
        }
        
        // 检查是否超时
        if (!command_finished && std::chrono::steady_clock::now() - start_time >= timeout)
        {
            // std::cout << "[SSHClient::Impl] Command execution timed out after " << timeout.count() << " seconds" << std::endl;
        }

        // 确保读取所有剩余输出
        // std::cout << "[SSHClient::Impl] Reading any remaining output..." << std::endl;
        while (true)
        {
            bytes_read = libssh2_channel_read(exec_channel, buffer, sizeof(buffer) - 1);
            if (bytes_read > 0)
            {
                buffer[bytes_read] = '\0';
                output += std::string(buffer, bytes_read);
                // std::cout << "[SSHClient::Impl] Read additional " << bytes_read << " bytes: " << std::string(buffer, bytes_read) << std::endl;
            }
            else
            {
                // std::cout << "[SSHClient::Impl] No more output to read" << std::endl;
                break;
            }
        }

        // 获取命令退出状态
        int exit_status = libssh2_channel_get_exit_status(exec_channel);
        // std::cout << "[SSHClient::Impl] Command exit status: " << exit_status << std::endl;

        // 关闭通道
        int close_rc = libssh2_channel_close(exec_channel);
        if (close_rc < 0)
        {
            // std::cout << "[SSHClient::Impl] Warning: Failed to close channel, error code: " << close_rc << std::endl;
        }
        libssh2_channel_free(exec_channel);
        // std::cout << "[SSHClient::Impl] Channel closed, total output length: " << output.length() << std::endl;

        // 调用回调（只在完整输出后调用一次）
        if (callback)
        {
            // Removed debug output to prevent console spamming
            // std::cout << "[SSHClient::Impl] Calling callback with output: [" << output << "]" << std::endl;
            callback(output);
        }

        return true;
    }

    void setAsyncOutputCallback(OutputCallback callback)
    {
        std::lock_guard<std::mutex> lock(callback_mutex_);
        async_output_callback_ = callback;
    }

    bool startShellSession()
    {
        if (!connected_ || !session_)
        {
            return false;
        }

        // 如果已经有活动通道，直接返回
        if (channel_)
        {
            return true;
        }

        int rc;

        // 创建通道
        // Removed debug output to prevent console spamming
        // std::cout << "[SSHClient::Impl] Creating new SSH channel" << std::endl;
        do
        {
            channel_ = libssh2_channel_open_session(session_);
            if (!channel_)
            {
                int error_code = libssh2_session_last_errno(session_);
                if (error_code == LIBSSH2_ERROR_EAGAIN)
                {
                    // std::cout << "[SSHClient::Impl] Would block, waiting..." << std::endl;
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
                else
                {
                    char *error_msg;
                    int error_len;
                    error_code = libssh2_session_last_error(session_, &error_msg, &error_len, 0);
                    last_error_ = "Failed to open SSH channel: " + std::string(error_msg, error_len);
                    // std::cerr << "[SSHClient::Impl] " << last_error_ << " (code: " << error_code << ")" << std::endl;
                    return false;
                }
            }
        } while (!channel_);

        // 请求PTY
        std::cout << "[SSHClient::Impl] Requesting PTY" << std::endl;
        int pty_rc = libssh2_channel_request_pty(channel_, "xterm");
        while (pty_rc == LIBSSH2_ERROR_EAGAIN)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            pty_rc = libssh2_channel_request_pty(channel_, "xterm");
        }

        if (pty_rc)
        {
            last_error_ = "Failed to request PTY";
            std::cerr << "[SSHClient::Impl] " << last_error_ << std::endl;
            libssh2_channel_free(channel_);
            channel_ = nullptr;
            return false;
        }

        // 启动shell
        std::cout << "[SSHClient::Impl] Starting shell" << std::endl;
        int shell_rc = libssh2_channel_shell(channel_);
        while (shell_rc == LIBSSH2_ERROR_EAGAIN)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            shell_rc = libssh2_channel_shell(channel_);
        }

        if (shell_rc)
        {
            last_error_ = "Failed to start shell";
            std::cerr << "[SSHClient::Impl] " << last_error_ << std::endl;
            libssh2_channel_free(channel_);
            channel_ = nullptr;
            return false;
        }

        std::cout << "[SSHClient::Impl] Shell session started successfully" << std::endl;

        // Removed debug output to prevent console spamming
        // std::cout << "[SSHClient::Impl] Starting output reader thread in startShellSession" << std::endl;
        if (!reading_output_)
        {
            reading_output_ = true;
            output_thread_ = std::thread([this]()
                                         {
                char buffer[1024];
                int bytes_read;

                // std::cout << "[SSHClient::Impl] Output reader thread started" << std::endl;
                while (reading_output_ && connected_ && channel_) {
                    bytes_read = libssh2_channel_read(channel_, buffer, sizeof(buffer) - 1);
                    if (bytes_read > 0) {
                        buffer[bytes_read] = '\0';
                        std::string output(buffer, bytes_read);
                        
                        // std::cout << "[SSHClient::Impl] Read " << bytes_read << " bytes from channel: " << output << std::endl;
                        
                        {
                            std::lock_guard<std::mutex> lock(buffer_mutex_);
                            output_buffer_ += output;
                        }
                        output_available_.notify_all();
                        
                        // 线程安全地访问回调函数
                        OutputCallback callback_copy;
                        {
                            std::lock_guard<std::mutex> lock(callback_mutex_);
                            callback_copy = async_output_callback_;
                        }
                        if (callback_copy) {
                            // std::cout << "[SSHClient::Impl] Calling async output callback with: " << output << std::endl;
                            callback_copy(output);
                        } else {
                            // std::cout << "[SSHClient::Impl] No async output callback set" << std::endl;
                        }
                    } else if (bytes_read == LIBSSH2_ERROR_EAGAIN) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    } else if (bytes_read < 0) {
                        // 检查是否是EAGAIN错误
                        int error_code = libssh2_session_last_errno(session_);
                        if (error_code != LIBSSH2_ERROR_EAGAIN) {
                            // std::cerr << "[SSHClient::Impl] Error reading from channel: " << bytes_read 
                            //           << " (error code: " << error_code << ")" << std::endl;
                            break;
                        }
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    } else {
                        // bytes_read == 0, 没有数据可读
                        std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                }
                // std::cout << "[SSHClient::Impl] Output reader thread exiting" << std::endl;
            });
        }

        return true;
    }

    bool hasActiveChannel() const
    {
        return connected_ && channel_ != nullptr;
    }

private:
    void closeSocket()
    {
#ifdef _WIN32
        if (socket_ >= 0)
        {
            closesocket(socket_);
            socket_ = -1;
        }
        WSACleanup();
#else
        if (socket_ >= 0)
        {
            close(socket_);
            socket_ = -1;
        }
#endif
    }
};

SSHClient::SSHClient() : impl_(std::make_unique<Impl>()) {}

SSHClient::~SSHClient()
{
    if (impl_)
    {
        impl_->disconnect();
    }
}

bool SSHClient::connect(const std::string &host, int port,
                        const std::string &username,
                        const std::string &password)
{
    return impl_->connect(host, port, username, password);
}

bool SSHClient::connectWithConfig(const std::string &host, int port,
                                  const std::string &username,
                                  const std::string &password,
                                  int timeout)
{
    return impl_->connectWithConfig(host, port, username, password, timeout);
}

void SSHClient::disconnect() { impl_->disconnect(); }

bool SSHClient::isConnected() const { return impl_->isConnected(); }

bool SSHClient::isActuallyConnected() {
    if (!isConnected()) {
        return false;
    }
    
    // 尝试执行一个简单的命令来验证连接是否仍然有效
    std::string test_cmd = "echo ping";
    bool result = false;
    bool executed = impl_->executeCommand(test_cmd, [&result](const std::string &output) {
        if (output.find("ping") != std::string::npos) {
            result = true;
        }
    });
    
    return executed && result;
}

bool SSHClient::hasActiveChannel() const { return impl_->hasActiveChannel(); }

void SSHClient::sendInput(const std::string &input)
{
    impl_->sendInput(input);
}

void SSHClient::setAsyncOutputCallback(OutputCallback callback)
{
    impl_->setAsyncOutputCallback(callback);
}

bool SSHClient::startShellSession() { return impl_->startShellSession(); }

std::string SSHClient::getLastError() const { return impl_->getLastError(); }

std::string SSHClient::waitForOutput(const std::string &command, std::chrono::seconds timeout)
{
    return impl_->waitForOutput(command, timeout);
}

void SSHClient::clearOutputBuffer()
{
    impl_->clearOutputBuffer();
}
bool SSHClient::executeCommand(const std::string &command, std::function<void(const std::string &output)> callback)
{
    return impl_->executeCommand(command, callback);
}