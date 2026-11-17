// multi_device_manager.cpp
#include "multi_device_manager.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <nlohmann/json.hpp>

extern "C" {
#include <sqlite3.h>
}
#include "../../../performance/performance_monitor.h"

// @formatter:off
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif
// @formatter:on

MultiDeviceManager::MultiDeviceManager() : db_handle_(nullptr) {
    // 初始化DeviceManager实例
    device_manager_ = new DeviceManager();
    
    initializeDatabase();
    // 从数据库加载已保存的设备
    auto saved_devices = loadDevicesFromDB();
    for (const auto& device : saved_devices) {
        devices_[device.ip_address] = device;
    }
}

MultiDeviceManager::MultiDeviceManager(DeviceManager* device_manager) : db_handle_(nullptr) {
    // 使用传入的DeviceManager实例
    device_manager_ = device_manager;
    
    initializeDatabase();
    // 从数据库加载已保存的设备
    auto saved_devices = loadDevicesFromDB();
    for (const auto& device : saved_devices) {
        devices_[device.ip_address] = device;
    }
}

MultiDeviceManager::~MultiDeviceManager() {
    // 清理WSA (Windows)
#ifdef _WIN32
    WSACleanup();
#endif
    
    // 关闭数据库连接
    if (db_handle_) {
        sqlite3_close(static_cast<sqlite3*>(db_handle_));
    }
    
    // 删除DeviceManager实例
    if (device_manager_) {
        delete device_manager_;
        device_manager_ = nullptr;
    }
}

void MultiDeviceManager::initializeDatabase() {
    int rc = sqlite3_open("multi_devices.db", reinterpret_cast<sqlite3**>(&db_handle_));
    if (rc) {
        std::cerr << "Can't open database: " << sqlite3_errmsg(static_cast<sqlite3*>(db_handle_)) << std::endl;
        return;
    }

    // 创建设备表
    const char* sql = R"(
        CREATE TABLE IF NOT EXISTS devices (
            ip_address TEXT PRIMARY KEY,
            hostname TEXT,
            mac_address TEXT,
            is_online INTEGER,
            is_connected INTEGER,
            last_seen TEXT,
            username TEXT,
            password TEXT
        );
    )";

    char* errMsg = 0;
    rc = sqlite3_exec(static_cast<sqlite3*>(db_handle_), sql, 0, 0, &errMsg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << errMsg << std::endl;
        sqlite3_free(errMsg);
    }
}

void MultiDeviceManager::saveDeviceToDB(const DeviceInfo& device) {
    const char* sql = R"(
        INSERT OR REPLACE INTO devices (ip_address, hostname, mac_address, is_online, is_connected, last_seen, username, password)
        VALUES (?, ?, ?, ?, ?, ?, ?, ?);
    )";

    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(db_handle_), sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, device.ip_address.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 2, device.hostname.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 3, device.mac_address.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_int(stmt, 4, device.is_online ? 1 : 0);
        sqlite3_bind_int(stmt, 5, device.is_connected ? 1 : 0);
        sqlite3_bind_text(stmt, 6, device.last_seen.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 7, device.username.c_str(), -1, SQLITE_STATIC);
        sqlite3_bind_text(stmt, 8, device.password.c_str(), -1, SQLITE_STATIC);

        rc = sqlite3_step(stmt);
        if (rc != SQLITE_DONE) {
            std::cerr << "Insert failed: " << sqlite3_errmsg(static_cast<sqlite3*>(db_handle_)) << std::endl;
        }
    } else {
        std::cerr << "Prepare failed: " << sqlite3_errmsg(static_cast<sqlite3*>(db_handle_)) << std::endl;
    }
    sqlite3_finalize(stmt);
}

std::vector<DeviceInfo> MultiDeviceManager::loadDevicesFromDB() {
    std::vector<DeviceInfo> devices;
    const char* sql = "SELECT ip_address, hostname, mac_address, is_online, is_connected, last_seen, username, password FROM devices;";
    
    sqlite3_stmt* stmt;
    int rc = sqlite3_prepare_v2(static_cast<sqlite3*>(db_handle_), sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        while ((rc = sqlite3_step(stmt)) == SQLITE_ROW) {
            DeviceInfo device;
            device.ip_address = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
            device.hostname = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
            device.mac_address = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2));
            device.is_online = sqlite3_column_int(stmt, 3) != 0;
            device.is_connected = sqlite3_column_int(stmt, 4) != 0;
            device.last_seen = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
            device.username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
            device.password = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 7));
            
            devices.push_back(device);
        }
    } else {
        std::cerr << "Load failed: " << sqlite3_errmsg(static_cast<sqlite3*>(db_handle_)) << std::endl;
    }
    sqlite3_finalize(stmt);
    
    return devices;
}



bool MultiDeviceManager::connectToDevice(const std::string& ip_address, const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    // 检查设备是否已存在
    auto it = devices_.find(ip_address);
    if (it != devices_.end()) {
        // 更新现有设备信息
        it->second.username = username;
        it->second.password = password;
        it->second.is_connected = true;
        it->second.last_seen = getCurrentTime();
        
        // 保存到数据库
        saveDeviceToDB(it->second);
        
        return true;
    } else {
        // 创建新设备条目
        DeviceInfo device;
        device.ip_address = ip_address;
        device.hostname = resolveHostname(ip_address);
        device.username = username;
        device.password = password;
        device.is_connected = true;
        device.last_seen = getCurrentTime();
        
        devices_[ip_address] = device;
        saveDeviceToDB(device);
        
        return true;
    }
}

bool MultiDeviceManager::disconnectDevice(const std::string& ip_address) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    auto it = devices_.find(ip_address);
    if (it != devices_.end()) {
        it->second.is_connected = false;
        saveDeviceToDB(it->second);
        return true;
    }
    
    return false;
}

bool MultiDeviceManager::isDeviceConnected(const std::string& ip_address) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    auto it = devices_.find(ip_address);
    if (it != devices_.end()) {
        return it->second.is_connected;
    }
    
    return false;
}

std::vector<DeviceInfo> MultiDeviceManager::getConnectedDevices() {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    std::vector<DeviceInfo> connected_devices;
    for (const auto& pair : devices_) {
        if (pair.second.is_connected) {
            connected_devices.push_back(pair.second);
        }
    }
    
    return connected_devices;
}

DeviceInfo MultiDeviceManager::getDeviceInfo(const std::string& ip_address) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    auto it = devices_.find(ip_address);
    if (it != devices_.end()) {
        return it->second;
    }
    
    return DeviceInfo{}; // 返回默认构造的对象
}

bool MultiDeviceManager::executeCommandOnAllDevices(const std::string& command) {
    // 实际实现：遍历所有设备并执行命令
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    bool all_success = true;
    for (const auto& pair : devices_) {
        if (pair.second.is_connected) {
            // 使用DeviceManager实例执行命令
            bool success = device_manager_->executeOnDevice(pair.first, command, [&pair, this](const std::string& output) {
                // 处理命令输出
                std::cout << "Command output from " << pair.first << ": " << output << std::endl;
            });
            
            if (!success) {
                all_success = false;
            }
        }
    }
    
    return all_success;
}

bool MultiDeviceManager::executeCommandOnMultipleDevices(const std::vector<std::string>& ip_addresses, const std::string& command) {
    // 实际实现：对指定IP地址列表执行命令
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    bool all_success = true;
    for (const auto& ip : ip_addresses) {
        auto it = devices_.find(ip);
        if (it != devices_.end() && it->second.is_connected) {
            // 使用DeviceManager实例执行命令
            bool success = device_manager_->executeOnDevice(ip, command, [](const std::string& output) {
                // 处理命令输出
                std::cout << "Command output: " << output << std::endl;
            });
            
            if (!success) {
                all_success = false;
            }
        }
    }
    
    return all_success;
}

bool MultiDeviceManager::sendConfigToDevice(const std::string& ip_address, const std::string& config) {
    // 实际实现：将配置发送到指定设备
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    auto it = devices_.find(ip_address);
    if (it != devices_.end() && it->second.is_connected) {
        // 发送配置的命令，比如将配置写入远程设备
        std::string cmd = "echo '" + config + "' > /tmp/device_config.json";
        
        // 使用DeviceManager实例执行命令
        bool success = device_manager_->executeOnDevice(ip_address, cmd, [](const std::string& output) {
            // 处理命令输出
            std::cout << "Config send output: " << output << std::endl;
        });
        
        return success;
    }
    
    return false;
}

bool MultiDeviceManager::sendConfigToMultipleDevices(const std::vector<std::string>& ip_addresses, const std::string& config) {
    // 实际实现：将配置发送到多个设备
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    bool all_success = true;
    for (const auto& ip : ip_addresses) {
        if (!sendConfigToDevice(ip, config)) {
            all_success = false;
        }
    }
    
    return all_success;
}

std::string MultiDeviceManager::getAggregatedPerformanceData() {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    
    // 创建JSON对象存储聚合的性能数据
    nlohmann::json result;
    
    for (const auto& pair : devices_) {
        if (pair.second.is_connected) {
            // 尝试从DeviceManager获取性能数据
            // 通过执行性能监控命令获取数据
            std::string perf_cmd = "top -b -n 1 | head -n 5";  // 示例命令，实际可能需要更具体的性能指标
            std::string perf_data = "";
            
            bool success = device_manager_->executeOnDevice(pair.first, perf_cmd, [&perf_data, &pair, this](const std::string& output) {
                perf_data += output;
            });
            
            nlohmann::json device_data;
            device_data["ip"] = pair.first;
            device_data["connected"] = pair.second.is_connected;
            device_data["online"] = pair.second.is_online;
            device_data["hostname"] = pair.second.hostname;
            device_data["performance_data"] = perf_data.empty() ? "N/A" : perf_data;  // 添加性能数据，如果为空则标记为N/A
            
            if (device_manager_) {
                PerformanceMonitor* perf_monitor = device_manager_->getPerformanceMonitor();
                if (perf_monitor) {
                    PerformanceMetrics metrics = perf_monitor->getCachedMetricsForDevice(pair.first);
                    device_data["cpu_usage"] = metrics.cpu_usage;
                    device_data["memory_usage"] = metrics.memory_usage;
                    device_data["npu_usage"] = metrics.npu_usage;
                    device_data["npu_temperature"] = metrics.npu_temperature;
                    device_data["system_temperature"] = metrics.system_temperature;
                }
            }
            
            result["devices"][pair.first] = device_data;
        }
    }
    
    // 添加汇总信息
    result["summary"]["total_devices"] = devices_.size();
    result["summary"]["connected_devices"] = result["devices"].size();
    result["timestamp"] = getCurrentTime();
    
    return result.dump();
}

std::string MultiDeviceManager::getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string MultiDeviceManager::resolveHostname(const std::string& ip) {
    // 使用平台特定的代码解析主机名
    #ifdef _WIN32
        WSADATA wsaData;
        if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
            return ip;
        }

        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;

        INT addrLen = sizeof(addr);
        if (WSAStringToAddressA(const_cast<LPSTR>(ip.c_str()), AF_INET, NULL,
                                (struct sockaddr *)&addr, &addrLen) != 0) {
            WSACleanup();
            return ip;
        }

        char host[NI_MAXHOST] = {0};
        int result = getnameinfo((sockaddr *)&addr, sizeof(addr), host,
                                 sizeof(host), nullptr, 0, 0);

        WSACleanup();

        if (result == 0 && host[0] != '\0') {
            return std::string(host);
        }
    #else
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        inet_pton(AF_INET, ip.c_str(), &addr.sin_addr);

        char host[NI_MAXHOST] = {0};
        int result = getnameinfo((sockaddr *)&addr, sizeof(addr), host,
                                 sizeof(host), nullptr, 0, 0);

        if (result == 0 && host[0] != '\0') {
            return std::string(host);
        }
    #endif
    return ip;
}