#pragma once

#include <string>
#include <vector>
#include <memory>
#include <set>
#include <thread>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <chrono>
#include <unordered_map>
#include <nlohmann/json.hpp>  // 使用nlohmann_json库

using json = nlohmann::json;

class DeviceManager;  // Forward declaration

struct PerformanceMetrics {
    std::string device_ip;
    std::string timestamp;
    double cpu_usage = 0.0;
    double memory_usage = 0.0;
    double memory_total = 0.0;
    double memory_available = 0.0;
    double disk_usage = 0.0;
    double load_average_1m = 0.0;
    double load_average_5m = 0.0;
    double load_average_15m = 0.0;
    double npu_usage = 0.0;
    double npu_memory_usage = 0.0;
    double npu_power = 0.0;
    double npu_temperature = 0.0;
    double system_temperature = 0.0;
    bool connected = false;

    json toJson() const {
        json j;
        j["device_ip"] = device_ip;
        j["timestamp"] = timestamp;
        j["cpu_usage"] = cpu_usage;
        j["memory_usage"] = memory_usage;
        j["memory_total"] = memory_total;
        j["memory_available"] = memory_available;
        j["disk_usage"] = disk_usage;
        j["load_average_1m"] = load_average_1m;
        j["load_average_5m"] = load_average_5m;
        j["load_average_15m"] = load_average_15m;
        j["npu_usage"] = npu_usage;
        j["npu_memory_usage"] = npu_memory_usage;
        j["npu_power"] = npu_power;
        j["npu_temperature"] = npu_temperature;
        j["system_temperature"] = system_temperature;
        j["connected"] = connected;
        return j;
    }
};

class SSHPerformanceCollector {
private:
    // 不再存储认证信息，由DeviceManager统一管理

public:
    // 构造函数不再接收认证参数
    SSHPerformanceCollector();
    
    bool initialize();
    void cleanup();
    
    // collectMetrics方法通过DeviceManager执行命令
    PerformanceMetrics collectMetrics(const std::string& device_ip, DeviceManager& device_manager);

private:
    std::string getCurrentTimestamp();
    void parseMetricsFromResult(const std::string& result, PerformanceMetrics& metrics);
};

class PerformanceMonitor {
private:
    std::unique_ptr<SSHPerformanceCollector> collector_;
    std::thread monitor_thread_;
    std::atomic<bool> running_{false};
    std::mutex devices_mutex_;
    std::mutex stop_mutex_;
    std::condition_variable stop_cv_;
    std::set<std::string> devices_;
    int interval_ms_;
    std::function<void(const std::string&)> output_callback_;
    
    DeviceManager* device_manager_; // 引用DeviceManager实例
    void collectAndBroadcast();
    
    // 缓存最近的性能数据
    std::unordered_map<std::string, PerformanceMetrics> cached_metrics_;
    std::mutex cache_mutex_;

public:
    // 构造函数接收DeviceManager引用，不再接收认证参数
    explicit PerformanceMonitor(DeviceManager& device_manager);
    
    ~PerformanceMonitor();

    bool start();
    void stop();
    void setCollectionInterval(int milliseconds);

    void addDevice(const std::string& ip);
    void removeDevice(const std::string& ip);
    void startMonitoringForDevice(const std::string& ip);
    void stopMonitoringForDevice(const std::string& ip);
    PerformanceMetrics collectMetricsForDevice(const std::string& ip);
    PerformanceMetrics getCachedMetricsForDevice(const std::string& ip);
    void setOutputCallback(std::function<void(const std::string&)> callback);
};