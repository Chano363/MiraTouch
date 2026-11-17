#pragma once
#include <functional>
#include <memory>
#include <string>
#include <chrono>

class SSHClient
{
public:
  using OutputCallback = std::function<void(const std::string &output)>;

  SSHClient();
  ~SSHClient();

  bool connect(const std::string &host, int port, const std::string &username,
               const std::string &password = "");
  bool connectWithConfig(const std::string &host, int port, const std::string &username,
                        const std::string &password = "", int timeout = 30);
  void disconnect();
  bool isConnected() const;

  bool isActuallyConnected();

  bool hasActiveChannel() const;
  void setAsyncOutputCallback(OutputCallback callback);
  bool startShellSession();

  void sendInput(const std::string &input);
  std::string getLastError() const;

  // 添加新的公共方法
  std::string waitForOutput(const std::string &command, std::chrono::seconds timeout = std::chrono::seconds(5));
  void clearOutputBuffer();
  bool executeCommand(const std::string &command, std::function<void(const std::string &output)> callback);

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};