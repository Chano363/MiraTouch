#pragma once

#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <mutex>
#include <functional>
#include "device_info.h"
#include "device_manager.h"

class MultiDeviceManager {
public:
    MultiDeviceManager();
    MultiDeviceManager(DeviceManager* device_manager);
    ~MultiDeviceManager();

    // 设备连接管理
    bool connectToDevice(const std::string& ip_address, const std::string& username, const std::string& password);
    bool disconnectDevice(const std::string& ip_address);
    bool isDeviceConnected(const std::string& ip_address);

    // 设备状态管理
    std::vector<DeviceInfo> getConnectedDevices();
    DeviceInfo getDeviceInfo(const std::string& ip_address);

    // 批量操作
    bool executeCommandOnAllDevices(const std::string& command);
    bool executeCommandOnMultipleDevices(const std::vector<std::string>& ip_addresses, const std::string& command);

    // 设备配置管理
    bool sendConfigToDevice(const std::string& ip_address, const std::string& config);
    bool sendConfigToMultipleDevices(const std::vector<std::string>& ip_addresses, const std::string& config);

    // 性能数据聚合
    std::string getAggregatedPerformanceData();

private:
    std::unordered_map<std::string, DeviceInfo> devices_;
    std::mutex devices_mutex_;
    
    // 引用DeviceManager实例
    DeviceManager* device_manager_;
    
    // SQLite数据库相关
    void* db_handle_;  // 使用void*以避免在这里包含sqlite3.h
    void initializeDatabase();
    void saveDeviceToDB(const DeviceInfo& device);
    std::vector<DeviceInfo> loadDevicesFromDB();
    std::string getCurrentTime();
    std::string resolveHostname(const std::string& ip);
};

