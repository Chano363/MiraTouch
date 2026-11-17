#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <locale>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>


#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#endif

#include "../performance/performance_monitor.h" // 添加性能监控头文件
#include "device_manager/core/device_manager.h"
#include "device_manager/core/multi_device_manager.h" // 添加多设备管理头文件
#include "device_manager/core/device_types.h"
#include "http_server.h"
#include "server.h"
#include "websocket_handler.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <string>

std::string deviceTypeToString(DeviceType type) {
    switch (type) {
        case DeviceType::UNKNOWN:
            return "UNKNOWN";
        case DeviceType::VMWARE_VIRTUAL_MACHINE:
            return "VMWARE_VIRTUAL_MACHINE";
        case DeviceType::HUAWEI_ATLAS:
            return "HUAWEI_ATLAS";
        case DeviceType::OTHER:
            return "OTHER";
        default:
            return "UNKNOWN";
    }
}

bool endsWith(const std::string &str, const std::string &suffix) {
  if (suffix.length() > str.length()) {
    return false;
  }
  return str.substr(str.length() - suffix.length()) == suffix;
}
std::string extractRequestBody(const std::string &request) {
  size_t body_pos = request.find("\r\n\r\n");
  if (body_pos != std::string::npos) {
    return request.substr(body_pos + 4);
  }

  body_pos = request.find("\n\n");
  if (body_pos != std::string::npos) {
    return request.substr(body_pos + 2);
  }

  return "";
}

class RemoteDebuggerServer::Impl {
public:
  Impl();
  ~Impl();

  bool initialize(int port);
  void run();
  void stop();

private:
  void startWebServer();
  void startWebSocketServer();
  std::string buildCommandFromConfig(const nlohmann::json &config);
  std::string escapeSingleQuotes(const std::string &input);

  std::unique_ptr<DeviceManager> device_manager_;
  std::unique_ptr<MultiDeviceManager>
      multi_device_manager_; // 添加多设备管理成员
  std::unique_ptr<WebSocketHandler> websocket_handler_;
  std::unique_ptr<SimpleHTTPServer> web_server_;
  std::unique_ptr<PerformanceMonitor> performance_monitor_; // 添加性能监控成员
  std::thread server_thread_;
  std::atomic<bool> running_{false};
  int port_;
};

RemoteDebuggerServer::Impl::Impl()
    : device_manager_(std::make_unique<DeviceManager>()),
      multi_device_manager_(
          std::make_unique<MultiDeviceManager>(device_manager_.get())),
      websocket_handler_(std::make_unique<WebSocketHandler>()),
      web_server_(nullptr), performance_monitor_(nullptr), running_(false),
      port_(8082) {
  std::locale::global(std::locale(""));
}

RemoteDebuggerServer::Impl::~Impl() { stop(); }

bool RemoteDebuggerServer::Impl::initialize(int port) {
  port_ = port;

  // 初始化性能监控系统
  performance_monitor_ = std::make_unique<PerformanceMonitor>(*device_manager_);

  // 设置WebSocket回调以广播性能数据
  performance_monitor_->setOutputCallback([this](const std::string &data) {
    // 不再输出性能数据准备广播日志
    // std::cout << "[Server] Performance data ready to broadcast: " << data << std::endl;
    if (websocket_handler_) {
      // 为性能数据添加类型标识，以便前端能正确识别
      nlohmann::json json_data = nlohmann::json::parse(data);
      json_data["type"] = "performance_data";
      std::string message_with_type = json_data.dump();
      websocket_handler_->broadcast(message_with_type);
      // 禁用性能数据广播成功日志
      // 不再输出 [Server] Performance data broadcasted successfully 日志
      // std::cout << "[Server] Performance data broadcasted successfully" << std::endl;
    } else {
      std::cout << "[Server] WebSocket handler is null, cannot broadcast "
                   "performance data"
                << std::endl;
    }
  });

  // 启动性能监控
  performance_monitor_->start();

  // 设置DeviceManager的性能监控引用
  device_manager_->setPerformanceMonitor(performance_monitor_.get());

  // 设置MultiDeviceManager的性能监控引用
  // 将DeviceManager和PerformanceMonitor传递给MultiDeviceManager
  // 这样MultiDeviceManager可以使用DeviceManager执行命令，使用PerformanceMonitor获取性能数据
  // 但目前MultiDeviceManager内部实现可能还需要进一步完善以充分利用这些引用

  startWebServer();

  startWebSocketServer();
  websocket_handler_->setMessageCallback([this](const std::string &message) {
    try {
      // 禁用WebSocket消息接收日志
      // 不再输出 [WebSocket] Received message 日志
      // std::cout << "[WebSocket] Received message: " << message << std::endl;
      auto json_msg = nlohmann::json::parse(message);
      std::string type = json_msg.value("type", "");

      if (type == "heartbeat") {
        nlohmann::json heartbeat_response;
        heartbeat_response["type"] = "heartbeat";
        websocket_handler_->broadcast(heartbeat_response.dump());
      }
      // 添加处理设备连接/断开消息的逻辑
      else if (type == "device_connect") {
        std::string ip_address = json_msg.value("ip_address", "");
        bool success = json_msg.value("success", false);
        // 不再自动添加到性能监控，只有当用户明确点击开始监控按钮时才添加
        if (!ip_address.empty() && performance_monitor_ && !success) {
          // 如果连接失败，确保从性能监控中移除设备
          performance_monitor_->removeDevice(ip_address);
          std::cout << "[Server] Removed device " << ip_address << " from performance monitoring via WebSocket (connection failed)" << std::endl;
        }
      } else if (type == "device_disconnect") {
        std::string ip_address = json_msg.value("ip_address", "");
        if (!ip_address.empty() && performance_monitor_) {
          // 在断开连接时，同时停止该设备的性能监控
          performance_monitor_->stopMonitoringForDevice(ip_address);
          performance_monitor_->removeDevice(ip_address);
          std::cout << "[Server] Removed device " << ip_address << " from performance monitoring via WebSocket (disconnected)" << std::endl;
        }
      } else if (type == "performance_control") {
        std::string ip_address = json_msg.value("ip_address", "");
        std::string action = json_msg.value("action", "");
        
        std::cout << "[Server] Received WebSocket performance_control message - IP: " << ip_address << ", Action: " << action << std::endl;
        
        if (!ip_address.empty() && performance_monitor_) {
          if (action == "start") {
            performance_monitor_->startMonitoringForDevice(ip_address);
            std::cout << "[Server] Started performance monitoring for device: " << ip_address << std::endl;
          } else if (action == "stop") {
            performance_monitor_->stopMonitoringForDevice(ip_address);
            std::cout << "[Server] Stopped performance monitoring for device: " << ip_address << std::endl;
          }
        } else {
          std::cout << "[Server] Performance control failed - IP empty or performance_monitor null" << std::endl;
        }
      }
    } catch (const std::exception &e) {
      std::cerr << "Error processing WebSocket message: " << e.what()
                << std::endl;
    }
  });

  return true;
}
void RemoteDebuggerServer::Impl::startWebServer() {
  std::cout << "[Server] Starting web server on port " << port_ << std::endl;
  web_server_ = std::make_unique<SimpleHTTPServer>(port_);

  std::cout << "[Server] Registering /api/devices/scan route" << std::endl;
  web_server_->addRoute(
      "/api/devices/scan", [this](const std::string &request) -> std::string {
        std::cout << "[Server] /api/devices/scan endpoint called" << std::endl;
        std::istringstream request_stream(request);
        std::string method, path, version;
        request_stream >> method >> path >> version;

        if (method == "OPTIONS") {
          std::cout << "[Server] Handling OPTIONS request for /api/devices/scan"
                    << std::endl;
          nlohmann::json response;
          response["success"] = true;
          return response.dump();
        }

        std::string body = extractRequestBody(request);
        std::string mode = "quick";

        std::string ssh_username = "root"; // 默认用户名
        if (!body.empty()) {
          try {
            auto json_request = nlohmann::json::parse(body);
            std::string scan_mode = json_request.value("scan_mode", "");
            if (!scan_mode.empty()) {
              mode = scan_mode;
            } else {
              mode = json_request.value("mode", "quick");
            }
            // 读取ssh_username参数（虽然扫描不需要，但为了兼容性保留）
            ssh_username = json_request.value("ssh_username", "root");
          } catch (...) {
            // 解析失败时使用默认值，继续执行
          }
        }

        std::vector<DeviceInfo> devices;
        std::string error_message;
        bool scan_success = true;

        try {
          devices = device_manager_->scanNetwork(
              mode, [this](const std::string &status, int current, int total,
                           size_t device_count) {
                nlohmann::json progress_msg;
                progress_msg["type"] = "scan_progress";
                progress_msg["status"] = status;
                progress_msg["current"] = current;
                progress_msg["total"] = total;
                progress_msg["device_count"] = device_count;
                if (this->websocket_handler_) {
                  this->websocket_handler_->broadcast(progress_msg.dump());
                }
              });
        } catch (const std::exception &e) {
          std::cerr << "[Server] Error during device scan: " << e.what()
                    << std::endl;
          error_message = std::string("Device scan failed: ") + e.what();
          scan_success = false;
        } catch (...) {
          std::cerr << "[Server] Unknown error during device scan" << std::endl;
          error_message = "Device scan failed with unknown error";
          scan_success = false;
        }

        nlohmann::json response;
        response["success"] = scan_success;
        nlohmann::json devices_json = nlohmann::json::array();

        if (scan_success) {
          for (const auto &device : devices) {
            nlohmann::json device_json;
            device_json["ip_address"] = device.ip_address;
            device_json["hostname"] = device.hostname;
            device_json["mac_address"] = device.mac_address;
            device_json["device_type"] = deviceTypeToString(device.device_type);
            device_json["is_online"] = device.is_online;
            device_json["is_connected"] = device.is_connected;
            device_json["last_seen"] = device.last_seen;
            devices_json.push_back(device_json);
          }
        }

        response["devices"] = devices_json;
        response["found_devices"] = scan_success ? devices.size() : 0;

        if (!scan_success) {
          response["message"] = error_message;
        }

        return response.dump();
      });

  web_server_->addRoute(
      "/api/devices/connect", [this](const std::string &request) {
        std::istringstream request_stream(request);
        std::string method, path, version;
        request_stream >> method >> path >> version;

        if (method == "OPTIONS") {
          nlohmann::json response;
          response["success"] = true;
          return response.dump();
        }

        std::string body = extractRequestBody(request);
        auto json_request = nlohmann::json::parse(body);

        std::string ip_address = json_request["ip_address"];
        std::string username = json_request["username"];
        std::string password = json_request.value("password", "");
        int port = json_request.value("port", 22);       // 默认端口22
        int timeout = json_request.value("timeout", 30); // 默认超时30秒

        std::cout << "[Server] Attempting to connect to device: " << ip_address
                  << " with username: " << username << ", port: " << port
                  << ", timeout: " << timeout << std::endl;

        bool success = device_manager_->connectToDeviceWithConfig(
            ip_address, username, password, port, timeout);

        // 连接成功后，将设备添加到性能监控
        if (success && performance_monitor_) {
          performance_monitor_->addDevice(ip_address);
          std::cout << "[Server] Added device " << ip_address << " to performance monitoring" << std::endl;
          
          // 广播设备连接状态更新
          nlohmann::json connect_msg;
          connect_msg["type"] = "device_connect";
          connect_msg["ip_address"] = ip_address;
          connect_msg["success"] = true;
          if (websocket_handler_) {
            websocket_handler_->broadcast(connect_msg.dump());
          }
        } else if (!success) {
          std::cout << "[Server] Device connection failed, removing from performance monitoring if present" << std::endl;
          // 如果连接失败，从性能监控中移除设备（如果存在的话）
          if (performance_monitor_) {
            performance_monitor_->removeDevice(ip_address);
          }
        }

        std::cout << "[Server] Device connection result for " << ip_address
                  << ": " << (success ? "SUCCESS" : "FAILED") << std::endl;

        nlohmann::json response;
        response["success"] = success;
        response["message"] =
            success ? "Connected successfully" : "Failed to connect";

        return response.dump();
      });

  // 添加获取设备列表的API端点
  web_server_->addRoute(
      "/api/devices/list", [this](const std::string &request) {
        std::istringstream request_stream(request);
        std::string method, path, version;
        request_stream >> method >> path >> version;

        if (method == "OPTIONS") {
          nlohmann::json response;
          response["success"] = true;
          return response.dump();
        }

        std::vector<DeviceInfo> devices;
        try {
          // 获取所有设备列表
          devices = device_manager_->getDevices();
        } catch (const std::exception &e) {
          std::cerr << "[Server] Error getting device list: " << e.what()
                    << std::endl;
          nlohmann::json response;
          response["success"] = false;
          response["message"] = std::string("Failed to get device list: ") + e.what();
          return response.dump();
        } catch (...) {
          std::cerr << "[Server] Unknown error getting device list" << std::endl;
          nlohmann::json response;
          response["success"] = false;
          response["message"] = "Failed to get device list with unknown error";
          return response.dump();
        }

        nlohmann::json response;
        response["success"] = true;
        nlohmann::json devices_json = nlohmann::json::array();

        for (const auto &device : devices) {
          nlohmann::json device_json;
          device_json["ip_address"] = device.ip_address;
          device_json["hostname"] = device.hostname;
          device_json["mac_address"] = device.mac_address;
          device_json["is_online"] = device.is_online;
          device_json["is_connected"] = device.is_connected;
          device_json["last_seen"] = device.last_seen;
          devices_json.push_back(device_json);
        }

        response["devices"] = devices_json;
        response["total_devices"] = devices.size();

        return response.dump();
      });

  web_server_->addRoute("/api/devices/disconnect",
                        [this](const std::string &request) {
                          std::istringstream request_stream(request);
                          std::string method, path, version;
                          request_stream >> method >> path >> version;

                          if (method == "OPTIONS") {
                            nlohmann::json response;
                            response["success"] = true;
                            return response.dump();
                          }

                          std::string body = extractRequestBody(request);
                          auto json_request = nlohmann::json::parse(body);

                          std::string ip_address = json_request["ip_address"];

                          // 断开连接前，从性能监控中移除设备
                          if (performance_monitor_) {
                            performance_monitor_->removeDevice(ip_address);
                            std::cout << "[Server] Removed device " << ip_address << " from performance monitoring" << std::endl;
                          }
                          
                          device_manager_->disconnectDevice(ip_address);
                          
                          // 广播设备断开连接状态
                          nlohmann::json disconnect_msg;
                          disconnect_msg["type"] = "device_disconnect";
                          disconnect_msg["ip_address"] = ip_address;
                          disconnect_msg["success"] = true;
                          if (websocket_handler_) {
                            websocket_handler_->broadcast(disconnect_msg.dump());
                          }
                          
                          nlohmann::json response;
                          response["success"] = true;
                          response["message"] = "Disconnected successfully";
                          
                          return response.dump();
                        });

  // 添加性能数据API端点
  web_server_->addRoute(
      "/api/performance/data", [this](const std::string &request) {
        std::istringstream request_stream(request);
        std::string method, path, version;
        request_stream >> method >> path >> version;

        if (method == "OPTIONS") {
          nlohmann::json response;
          response["success"] = true;
          return response.dump();
        }

        std::string body = extractRequestBody(request);
        auto json_request = nlohmann::json::parse(body);

        std::string ip_address = json_request.value("ip_address", "");

        nlohmann::json response;
        if (performance_monitor_ && !ip_address.empty()) {
          auto metrics =
              performance_monitor_->getCachedMetricsForDevice(ip_address);
          response["success"] = true;
          response["data"] = metrics.toJson();
        } else {
          response["success"] = false;
          response["error"] = "IP address required";
        }

        return response.dump();
      });

  // 添加设备级别性能监控控制API
  web_server_->addRoute(
      "/api/devices/performance_control", [this](const std::string &request) {
        std::istringstream request_stream(request);
        std::string method, path, version;
        request_stream >> method >> path >> version;

        if (method == "OPTIONS") {
          nlohmann::json response;
          response["success"] = true;
          return response.dump();
        }

        std::string body = extractRequestBody(request);
        auto json_request = nlohmann::json::parse(body);

        std::string ip_address = json_request.value("ip_address", "");
        std::string action = json_request.value("action", "");

        std::cout << "[Server] Received HTTP performance_control request - IP: " << ip_address << ", Action: " << action << std::endl;
        
        nlohmann::json response;
        if (performance_monitor_ && !ip_address.empty()) {
          if (action == "start") {
            performance_monitor_->startMonitoringForDevice(ip_address);
            std::cout << "[Server] Started performance monitoring for device via HTTP: " << ip_address << std::endl;
            response["success"] = true;
            response["message"] = "Performance monitoring started for device: " + ip_address;
          } else if (action == "stop") {
            performance_monitor_->stopMonitoringForDevice(ip_address);
            std::cout << "[Server] Stopped performance monitoring for device via HTTP: " << ip_address << std::endl;
            response["success"] = true;
            response["message"] = "Performance monitoring stopped for device: " + ip_address;
          } else {
            std::cout << "[Server] Invalid action received via HTTP: " << action << std::endl;
            response["success"] = false;
            response["error"] = "Invalid action";
          }
        } else {
          std::cout << "[Server] Performance control failed via HTTP - IP empty or performance_monitor null" << std::endl;
          response["success"] = false;
          response["error"] = "Performance monitor not initialized or IP address missing";
        }

        return response.dump();
      });

  // 添加全局性能监控控制API
  web_server_->addRoute(
      "/api/performance/control", [this](const std::string &request) {
        std::istringstream request_stream(request);
        std::string method, path, version;
        request_stream >> method >> path >> version;

        if (method == "OPTIONS") {
          nlohmann::json response;
          response["success"] = true;
          return response.dump();
        }

        std::string body = extractRequestBody(request);
        auto json_request = nlohmann::json::parse(body);

        std::string action = json_request.value("action", "");

        nlohmann::json response;
        if (performance_monitor_) {
          if (action == "start") {
            performance_monitor_->start();
            response["success"] = true;
            response["message"] = "Performance monitoring started";
          } else if (action == "stop") {
            performance_monitor_->stop();
            response["success"] = true;
            response["message"] = "Performance monitoring stopped";
          } else if (action == "interval") {
            int interval = json_request.value("interval", 5000);
            performance_monitor_->setCollectionInterval(interval);
            response["success"] = true;
            response["message"] = "Performance collection interval updated";
          } else {
            response["success"] = false;
            response["error"] = "Invalid action";
          }
        } else {
          response["success"] = false;
          response["error"] = "Performance monitor not initialized";
        }

        return response.dump();
      });

  web_server_->addRoute("/api/config/send", [this](const std::string &request) {
    std::cout << "[sendConfig] Processing config send request" << std::endl;

    std::istringstream request_stream(request);
    std::string method, path, version;
    request_stream >> method >> path >> version;

    if (method == "OPTIONS") {
      nlohmann::json response;
      response["success"] = true;
      std::cout << "[sendConfig] OPTIONS request processed" << std::endl;
      return response.dump();
    }

    std::string body = extractRequestBody(request);
    std::cout << "[sendConfig] Request body: " << body << std::endl;

    auto json_request = nlohmann::json::parse(body);

    std::string ip_address = json_request["ip_address"];
    auto config = json_request["config"];
    std::string username = json_request.value("username", "chano");
    std::string root_dir =
        json_request.value("root_dir", json_request.value("model_path", "/home/" + username + "/Github/dynamic_gestures"));

    std::cout << "[sendConfig] Target IP: " << ip_address
              << ", Root dir: " << root_dir << std::endl;

    if (config.is_null()) {
      nlohmann::json error_response;
      error_response["success"] = false;
      error_response["error"] = "Configuration data is missing";
      error_response["message"] = "Configuration data is missing";
      std::cout << "[sendConfig] Error: Configuration data is missing"
                << std::endl;
      return error_response.dump();
    }

    std::string json_str = config.dump();
    std::string escaped_json = escapeSingleQuotes(json_str);
    std::string command =
        "export LANG=en_US.UTF-8 && export LC_ALL=en_US.UTF-8 && cd " +
        root_dir +
        " && if [ ! -d configs ]; then mkdir -p configs; fi && echo " +
        escaped_json +
        " > configs/gesture_config.json.tmp && mv "
        "configs/gesture_config.json.tmp "
        "configs/gesture_config.json && echo 'Runtime parameters "
        "updated at $(date)'";

    std::cout << "[sendConfig] Executing command on IP " << ip_address << ": "
              << command << std::endl;

    // 检查设备连接状态
    bool is_connected = device_manager_->isDeviceConnected(ip_address);
    std::cout << "[sendConfig] Device " << ip_address << " connection status: "
              << (is_connected ? "CONNECTED" : "NOT CONNECTED") << std::endl;

    bool success = device_manager_->executeOnDevice(
        ip_address, command, [this, ip_address](const std::string &output) {
          std::cout << "[sendConfig] Received complete output for IP "
                    << ip_address << ": [" << output << "]" << std::endl;

          nlohmann::json output_msg;
          output_msg["type"] = "config_status";
          output_msg["ip_address"] = ip_address;

          // 智能解析输出
          std::string trimmed_output = output;

          // 检查是否包含JSON配置
          size_t json_start = trimmed_output.find('{');
          size_t json_end = trimmed_output.rfind('}');

          if (json_start != std::string::npos &&
              json_end != std::string::npos && json_end > json_start) {
            // 提取JSON部分
            std::string json_str =
                trimmed_output.substr(json_start, json_end - json_start + 1);
            try {
              auto config_json = nlohmann::json::parse(json_str);
              output_msg["has_config"] = true;
              output_msg["config"] = config_json;
              output_msg["status"] = "Configuration file loaded successfully";
            } catch (const std::exception &e) {
              // 如果JSON解析失败，使用原始输出
              output_msg["has_config"] = false;
              output_msg["status"] = trimmed_output;
            }
          } else if (trimmed_output.find("Configuration file does not exist") !=
                     std::string::npos) {
            // 明确的文件不存在信息
            output_msg["has_config"] = false;
            output_msg["status"] = "Configuration file does not exist";
          } else {
            // 其他情况，使用原始输出
            output_msg["has_config"] =
                !trimmed_output.empty() &&
                trimmed_output != "Configuration file does not exist";
            output_msg["status"] = trimmed_output;
          }

          // 保留原始输出用于调试
          output_msg["raw_output"] = output;

          std::cout << "[sendConfig] Sending parsed status: "
                    << output_msg["status"] << std::endl;

          if (this->websocket_handler_) {
            this->websocket_handler_->broadcast(output_msg.dump());
          }
        });

    std::cout << "[sendConfig] executeOnDevice for IP " << ip_address
              << " returned: " << (success ? "true" : "false") << std::endl;
    std::cout << "[sendConfig] Device connection status after execution: "
              << (device_manager_->isDeviceConnected(ip_address)
                      ? "STILL CONNECTED"
                      : "DISCONNECTED")
              << std::endl;

    nlohmann::json response;
    if (success) {
      response["success"] = true;
      response["message"] = "Runtime parameters sent successfully";
      response["error"] = "";
    } else {
      response["success"] = false;
      response["error"] = "Failed to execute command on device: " + ip_address +
                          " - Command: " + command;
      response["message"] = "Failed to send parameters to device";
    }

    return response.dump();
  });

  web_server_->addRoute(
      "/api/config/receive", [this](const std::string &request) {
        std::istringstream request_stream(request);
        std::string method, path, version;
        request_stream >> method >> path >> version;

        if (method == "OPTIONS") {
          nlohmann::json response;
          response["success"] = true;
          return response.dump();
        }

        std::string body = extractRequestBody(request);
        auto json_request = nlohmann::json::parse(body);

        std::string ip_address = json_request["ip_address"];
        std::string username = json_request.value("username", "chano");
        std::string root_dir = json_request.value("root_dir", "/home/" + username + "/Github/dynamic_gestures");
        root_dir = json_request.value("model_path", root_dir);

        std::string command =
        "export LANG=en_US.UTF-8 && export LC_ALL=en_US.UTF-8 && cd " +
        root_dir +
        " && if [ ! -d configs ]; then mkdir -p configs; fi && if [ -f "
        "configs/gesture_config.json ]; then cat "
        "configs/gesture_config.json; else echo 'Configuration "
        "file does not exist'; fi";

        std::cout << "[Config Status] Executing command for IP " << ip_address
                  << ": " << command << std::endl;

        nlohmann::json response;
        response["success"] = false;
        response["has_config"] = false;

        bool success = device_manager_->executeOnDevice(
            ip_address, command, [this, ip_address, &response](const std::string &output) {
              std::cout << "[Config Status] Received output for IP "
                        << ip_address << ": [" << output << "]" << std::endl;

              // 智能解析输出
              std::string trimmed_output = output;

              // 检查是否包含JSON配置
              size_t json_start = trimmed_output.find('{');
              size_t json_end = trimmed_output.rfind('}');

              if (json_start != std::string::npos &&
                  json_end != std::string::npos && json_end > json_start) {
                // 提取JSON部分
                std::string json_str =
                    trimmed_output.substr(json_start, json_end - json_start + 1);
                try {
                  auto config_json = nlohmann::json::parse(json_str);
                  response["has_config"] = true;
                  response["config"] = config_json;
                  response["success"] = true;
                  std::cout << "[Config Status] Configuration file loaded successfully" << std::endl;
                } catch (const std::exception &e) {
                  // 如果JSON解析失败，使用原始输出
                  response["has_config"] = false;
                  response["error"] = "Failed to parse configuration file: " + std::string(e.what());
                  std::cout << "[Config Status] Failed to parse configuration file: " << e.what() << std::endl;
                }
              } else if (trimmed_output.find("Configuration file does not exist") !=
                         std::string::npos) {
                // 明确的文件不存在信息
                response["has_config"] = false;
                response["error"] = "Configuration file does not exist";
                std::cout << "[Config Status] Configuration file does not exist" << std::endl;
              } else {
                // 其他情况，使用原始输出
                response["has_config"] = false;
                response["error"] = "Unknown error: " + trimmed_output;
                std::cout << "[Config Status] Unknown error: " << trimmed_output << std::endl;
              }

              // 广播配置状态
              nlohmann::json output_msg;
              output_msg["type"] = "config_status";
              output_msg["ip_address"] = ip_address;
              output_msg["status"] = output;
              if (this->websocket_handler_) {
                this->websocket_handler_->broadcast(output_msg.dump());
              }
            });

        std::cout << "[Config Status] ExecuteOnDevice returned: "
                  << (success ? "true" : "false") << std::endl;

        if (success) {
          response["success"] = true;
          response["message"] = "Configuration status retrieved successfully";
          response["error"] = "";
        } else {
          response["success"] = false;
          response["error"] = "Failed to execute command on device: " + ip_address + " - Command: " + command;
          response["message"] = "Failed to retrieve configuration status";
        }

        return response.dump();
      });
  web_server_->addRoute("/api/model/status", [this](
                                                 const std::string &request) {
    std::istringstream request_stream(request);
    std::string method, path, version;
    request_stream >> method >> path >> version;

    if (method == "OPTIONS") {
      nlohmann::json response;
      response["success"] = true;
      return response.dump();
    }

    std::string body = extractRequestBody(request);
    auto json_request = nlohmann::json::parse(body);

    std::string ip_address = json_request["ip_address"];
    std::string username = json_request.value("username", "chano");
    std::string root_dir =
        json_request.value("root_dir", json_request.value("model_path", "/home/" + username + "/Github/dynamic_gestures"));

    // 简化命令：只检测run_demo.py进程
    std::string command =
        "cd " + root_dir +
        " && (pgrep -f 'run_demo\\.py' || echo 'No run_demo.py process')";

    std::cout << "[Model Status] Executing command for IP " << ip_address
              << ": " << command << std::endl;

    bool success = device_manager_->executeOnDevice(
        ip_address, command, [this, ip_address](const std::string &output) {
          std::cout << "[Model Status] Received raw output for IP "
                    << ip_address << ": [" << output << "]" << std::endl;
          nlohmann::json output_msg;
          output_msg["type"] = "model_status";
          output_msg["ip_address"] = ip_address;

          // 分析输出以判断模型是否运行
          // pgrep返回PID（数字）如果进程存在，返回"No run_demo.py process"如果进程不存在
          bool has_run_demo =
              output.find("No run_demo.py process") == std::string::npos;

          if (has_run_demo) {
            output_msg["status"] = "running";
            output_msg["message"] = "Model is running";
          } else {
            output_msg["status"] = "stopped";
            output_msg["message"] = "Model is not running";
          }

          std::cout << "[Model Status] Sending status: " << output_msg["status"]
                    << std::endl;

          if (this->websocket_handler_) {
            this->websocket_handler_->broadcast(output_msg.dump());
          }
        });

    std::cout << "[Model Status] ExecuteOnDevice returned: "
              << (success ? "true" : "false") << std::endl;

    nlohmann::json response;
    response["success"] = success;
    response["status"] = success ? "checking" : "error";
    response["message"] = success ? "Model status check initiated" : "Failed to check model status";

    return response.dump();
  });

  

  // 添加获取默认SSH配置的API端点
  web_server_->addRoute("/api/config/default_ssh_config",
                        [this](const std::string &request) {
                          std::istringstream request_stream(request);
                          std::string method, path, version;
                          request_stream >> method >> path >> version;

                          if (method == "OPTIONS") {
                            nlohmann::json response;
                            response["success"] = true;
                            return response.dump();
                          }

                          // 返回默认的SSH配置值
                          nlohmann::json ssh_config;
                          ssh_config["username"] = "root"; // 默认用户名
                          ssh_config["default_port"] = 22; // 默认端口
                          ssh_config["timeout"] = 30;      // 默认超时时间

                          nlohmann::json response;
                          response["success"] = true;
                          response["ssh_config"] = ssh_config;

                          return response.dump();
                        });

  // 添加单个设备模型状态检查API端点
  



  if (!web_server_->start()) {
    std::cerr << "Failed to start web server" << std::endl;
  }
}

std::string RemoteDebuggerServer::Impl::buildCommandFromConfig(
    const nlohmann::json &config) {
  std::string json_str = config.dump();

  std::string command = "echo '" + escapeSingleQuotes(json_str) +
                        "' > /tmp/remote_debugger_config.json";

  return command;
}

std::string
RemoteDebuggerServer::Impl::escapeSingleQuotes(const std::string &input) {
  std::string result = "'";
  size_t pos = 0;
  for (char c : input) {
    if (c == '\'') {
      result += "'\"'\"'";
    } else {
      result += c;
    }
  }
  result += "'";
  return result;
}

void RemoteDebuggerServer::Impl::startWebSocketServer() {
  websocket_handler_->start(port_ + 1);
}

void RemoteDebuggerServer::Impl::run() {
  running_ = true;

  // 启动定期检查设备连接状态的线程
  std::thread status_check_thread([this]() {
    while (running_) {
      std::this_thread::sleep_for(std::chrono::seconds(5)); // 每5秒检查一次
      
      // 获取所有已知设备
      auto devices = device_manager_->getDevices();
      for (const auto &device : devices) {
        if (device.is_connected) { // 只检查已连接的设备
          device_manager_->updateDeviceConnectionStatus(device.ip_address);
        }
      }
    }
  });
  
  status_check_thread.detach();
  
  while (running_) {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

void RemoteDebuggerServer::Impl::stop() {
  // 停止性能监控
  if (performance_monitor_) {
    performance_monitor_->stop();
  }

  running_ = false;
  std::cout << "Server stopped" << std::endl;
}

RemoteDebuggerServer::RemoteDebuggerServer()
    : impl_(std::make_unique<Impl>()) {}

RemoteDebuggerServer::~RemoteDebuggerServer() = default;

bool RemoteDebuggerServer::initialize(int port) {
  return impl_->initialize(port);
}

void RemoteDebuggerServer::run() { impl_->run(); }

void RemoteDebuggerServer::stop() { impl_->stop(); }
