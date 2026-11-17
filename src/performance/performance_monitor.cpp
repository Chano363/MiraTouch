#include "performance_monitor.h"
#include "../../server/device_manager/core/device_manager.h"
#include <sstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <thread>
#include <date/date.h>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <set>
#include <mutex>
#include <memory>
#include <functional>
#include <unordered_map>

using json = nlohmann::json;

// 重构性能监控模块实现

// SSH性能收集器实现
SSHPerformanceCollector::SSHPerformanceCollector() {}

bool SSHPerformanceCollector::initialize() {
    return true;
}

void SSHPerformanceCollector::cleanup() {
    // 清理资源
}

std::string SSHPerformanceCollector::getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    return date::format("%Y-%m-%d %H:%M:%S", now);
}

void SSHPerformanceCollector::parseMetricsFromResult(const std::string& result, PerformanceMetrics& metrics) {
    std::istringstream stream(result);
    std::string line;
    
    while (std::getline(stream, line)) {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string key = line.substr(0, colon_pos);
            std::string value_str = line.substr(colon_pos + 1);

            try {
                // 移除首尾空白字符
                value_str.erase(0, value_str.find_first_not_of(" \t\n\r"));
                value_str.erase(value_str.find_last_not_of(" \t\n\r") + 1);
                
                double value = std::stod(value_str);
                
                if (key == "CPU_USAGE") {
                    metrics.cpu_usage = value;
                } else if (key == "MEMORY_USAGE") {
                    metrics.memory_usage = value;
                } else if (key == "MEMORY_TOTAL") {
                    metrics.memory_total = value;
                } else if (key == "MEMORY_AVAILABLE") {
                    metrics.memory_available = value;
                } else if (key == "DISK_USAGE") {
                    metrics.disk_usage = value;
                } else if (key == "LOAD_AVG_1M") {
                    metrics.load_average_1m = value;
                } else if (key == "LOAD_AVG_5M") {
                    metrics.load_average_5m = value;
                } else if (key == "LOAD_AVG_15M") {
                    metrics.load_average_15m = value;
                } else if (key == "NPU_USAGE") {
                    metrics.npu_usage = value;
                } else if (key == "NPU_MEMORY_USAGE") {
                    metrics.npu_memory_usage = value;
                } else if (key == "NPU_POWER") {
                    metrics.npu_power = value;
                } else if (key == "NPU_TEMPERATURE") {
                    metrics.npu_temperature = value;
                } else if (key == "SYSTEM_TEMPERATURE") {
                    metrics.system_temperature = value;
                }
            } catch (const std::exception& e) {
                // 只记录错误级别的日志
                std::cerr << "[SSHPerformanceCollector] Failed to parse value for key '" << key << "': " << value_str << "" << std::endl;
            }
        }
    }
}

PerformanceMetrics SSHPerformanceCollector::collectMetrics(const std::string& device_ip, DeviceManager& device_manager) {
    PerformanceMetrics metrics;
    metrics.device_ip = device_ip;
    metrics.timestamp = getCurrentTimestamp();

    // 首先检查设备连接状态，避免向未连接的设备发送命令
    if (!device_manager.isDeviceConnected(device_ip)) {
        metrics.connected = false;
        return metrics;
    }

    try {
        // 构建SSH命令来收集性能数据
        std::string ssh_command = R"RAW(
#!/bin/bash
# 设置错误处理和调试模式
set -e

# 定义安全的数值转换函数
safe_numeric_convert() {
    local value="$1"
    if [[ -z "$value" ]] || ! [[ "$value" =~ ^[0-9]+([.][0-9]+)?$ ]]; then
        echo "0"
    else
        # 使用printf格式化确保数值格式正确
        printf "%.2f" "$value" 2>/dev/null || echo "0"
    fi
}

# 收集CPU使用率 - 使用更简单可靠的方法
CPU_USAGE=0
if [ -f "/proc/stat" ]; then
    # 读取第一次CPU统计
    cpu_prev=$(head -n 1 /proc/stat 2>/dev/null)
    if [ -n "$cpu_prev" ]; then
        sleep 0.2
        # 读取第二次CPU统计
        cpu_curr=$(head -n 1 /proc/stat 2>/dev/null)
        if [ -n "$cpu_curr" ]; then
            # 解析CPU时间
            prev_idle=$(echo $cpu_prev | awk '{print $5}' 2>/dev/null)
            prev_total=$(echo $cpu_prev | awk '{sum=0; for(i=2;i<=NF;i++) sum+=$i; print sum}' 2>/dev/null)
            
            curr_idle=$(echo $cpu_curr | awk '{print $5}' 2>/dev/null)
            curr_total=$(echo $cpu_curr | awk '{sum=0; for(i=2;i<=NF;i++) sum+=$i; print sum}' 2>/dev/null)
            
            if [ -n "$prev_idle" ] && [ -n "$prev_total" ] && [ -n "$curr_idle" ] && [ -n "$curr_total" ]; then
                idle_diff=$(echo "$curr_idle - $prev_idle" | bc -l 2>/dev/null)
                total_diff=$(echo "$curr_total - $prev_total" | bc -l 2>/dev/null)
                
                if [ -n "$idle_diff" ] && [ -n "$total_diff" ] && [ "$(echo "$total_diff > 0" | bc -l 2>/dev/null)" = "1" ]; then
                    cpu_usage_calc=$(echo "scale=2; (1 - $idle_diff/$total_diff) * 100" | bc -l 2>/dev/null)
                    CPU_USAGE=$(safe_numeric_convert "$cpu_usage_calc")
                fi
            fi
        fi
    fi
fi

# 收集内存使用情况
MEMORY_USAGE=0
MEMORY_TOTAL=0
MEMORY_AVAILABLE=0

if [ -f "/proc/meminfo" ]; then
    # 获取总内存
    total_kb=$(awk '/^MemTotal:/ {print $2}' /proc/meminfo 2>/dev/null)
    if [ -n "$total_kb" ] && [ "$total_kb" -gt 0 ]; then
        MEMORY_TOTAL=$(safe_numeric_convert $(echo "$total_kb/1024" | bc -l 2>/dev/null))
        
        # 尝试获取可用内存
        available_kb=$(awk '/^MemAvailable:/ {print $2}' /proc/meminfo 2>/dev/null)
        if [ -z "$available_kb" ] || [ "$available_kb" -lt 0 ]; then
            # 备选方案：使用MemFree + Buffers + Cached
            free_kb=$(awk '/^MemFree:/ {print $2}' /proc/meminfo 2>/dev/null)
            buffers_kb=$(awk '/^Buffers:/ {print $2}' /proc/meminfo 2>/dev/null)
            cached_kb=$(awk '/^Cached:/ {print $2}' /proc/meminfo 2>/dev/null)
            
            if [ -n "$free_kb" ] && [ -n "$buffers_kb" ] && [ -n "$cached_kb" ]; then
                available_kb=$(echo "$free_kb + $buffers_kb + $cached_kb" | bc -l 2>/dev/null)
            else
                available_kb=0
            fi
        fi
        
        if [ -n "$available_kb" ] && [ "$available_kb" -gt 0 ]; then
            MEMORY_AVAILABLE=$(safe_numeric_convert $(echo "$available_kb/1024" | bc -l 2>/dev/null))
            used_kb=$(echo "$total_kb - $available_kb" | bc -l 2>/dev/null)
            memory_usage_calc=$(echo "scale=2; $used_kb/$total_kb*100" | bc -l 2>/dev/null)
            MEMORY_USAGE=$(safe_numeric_convert "$memory_usage_calc")
        fi
    fi
fi

# 收集磁盘使用情况
DISK_USAGE=0
df_result=$(df / 2>/dev/null | tail -n +2 | head -1)
if [ -n "$df_result" ]; then
    # 提取磁盘使用百分比
    disk_usage_raw=$(echo "$df_result" | awk '{print $5}' 2>/dev/null | sed 's/%//' 2>/dev/null)
    DISK_USAGE=$(safe_numeric_convert "$disk_usage_raw")
fi

# 收集负载平均值
LOAD_AVG_1M=0.00
LOAD_AVG_5M=0.00
LOAD_AVG_15M=0.00

if [ -f "/proc/loadavg" ]; then
    loadavg_line=$(cat /proc/loadavg 2>/dev/null)
    if [ -n "$loadavg_line" ]; then
        LOAD_AVG_1M=$(safe_numeric_convert $(echo "scale=2; $(echo "$loadavg_line" | awk '{print $1}' 2>/dev/null)" | bc -l 2>/dev/null))
        LOAD_AVG_5M=$(safe_numeric_convert $(echo "scale=2; $(echo "$loadavg_line" | awk '{print $2}' 2>/dev/null)" | bc -l 2>/dev/null))
        LOAD_AVG_15M=$(safe_numeric_convert $(echo "scale=2; $(echo "$loadavg_line" | awk '{print $3}' 2>/dev/null)" | bc -l 2>/dev/null))
    fi
fi

# 收集NPU信息（适用于华为昇腾）
NPU_USAGE=0
NPU_MEMORY_USAGE=0
NPU_POWER=0
NPU_TEMPERATURE=0

if command -v npu-smi >/dev/null 2>&1; then
    # 添加超时保护和错误处理
    npu_info=$(timeout 5s npu-smi info 2>/dev/null)
    if [ -n "$npu_info" ]; then
        # 查找NPU使用率
        npu_usage_match=$(echo "$npu_info" | grep -E 'ACL|ASCEND|NPU' | grep -o '[0-9]\+%' | head -1 | sed 's/%//' 2>/dev/null)
        if [ -n "$npu_usage_match" ]; then
            NPU_USAGE=$(safe_numeric_convert "$npu_usage_match")
        fi
        
        # 查找NPU内存使用率
        npu_mem_match=$(echo "$npu_info" | grep -i 'memory' | grep -o '[0-9]\+%' | head -1 | sed 's/%//' 2>/dev/null)
        if [ -n "$npu_mem_match" ]; then
            NPU_MEMORY_USAGE=$(safe_numeric_convert "$npu_mem_match")
        fi
        
        # 查找NPU功耗
        npu_power_match=$(echo "$npu_info" | grep -i 'power' | grep -o '[0-9]\+\.[0-9]\+W\|[0-9]\+W' | head -1 | sed 's/W//' 2>/dev/null)
        if [ -n "$npu_power_match" ]; then
            NPU_POWER=$(safe_numeric_convert "$npu_power_match")
        fi
        
        # 查找NPU温度
        npu_temp_match=$(echo "$npu_info" | grep -i 'temperature\|temp' | grep -o '[0-9]\+\.[0-9]\+C\|[0-9]\+C' | head -1 | sed 's/C//' 2>/dev/null)
        if [ -n "$npu_temp_match" ]; then
            NPU_TEMPERATURE=$(safe_numeric_convert "$npu_temp_match")
        fi
    fi
fi

# 收集系统温度
SYSTEM_TEMPERATURE=0
if [ -d "/sys/class/thermal" ]; then
    # 遍历所有热区
    for thermal_zone in /sys/class/thermal/thermal_zone*/; do
        if [ -f "$thermal_zone/temp" ]; then
            temp_raw=$(cat "$thermal_zone/temp" 2>/dev/null)
            if [ -n "$temp_raw" ] && [[ "$temp_raw" =~ ^[0-9]+$ ]] && [ "$temp_raw" -gt 0 ]; then
                # 根据数值范围判断温度单位
                if [ "$temp_raw" -lt 1000 ]; then
                    # 可能已经是摄氏度
                    SYSTEM_TEMPERATURE=$(safe_numeric_convert "$temp_raw")
                    break
                elif [ "$temp_raw" -lt 100000 ]; then
                    # 可能是十分之一摄氏度
                    SYSTEM_TEMPERATURE=$(safe_numeric_convert $(echo "$temp_raw/10" | bc -l 2>/dev/null))
                    break
                else
                    # 可能是毫摄氏度
                    SYSTEM_TEMPERATURE=$(safe_numeric_convert $(echo "$temp_raw/1000" | bc -l 2>/dev/null))
                    break
                fi
            fi
        fi
    done
fi

# 确保所有值都在合理范围内
CPU_USAGE=$(echo "$CPU_USAGE" | awk '{print ($1 > 100) ? 100 : ($1 < 0) ? 0 : $1}' 2>/dev/null || echo 0)
MEMORY_USAGE=$(echo "$MEMORY_USAGE" | awk '{print ($1 > 100) ? 100 : ($1 < 0) ? 0 : $1}' 2>/dev/null || echo 0)
DISK_USAGE=$(echo "$DISK_USAGE" | awk '{print ($1 > 100) ? 100 : ($1 < 0) ? 0 : $1}' 2>/dev/null || echo 0)
NPU_USAGE=$(echo "$NPU_USAGE" | awk '{print ($1 > 100) ? 100 : ($1 < 0) ? 0 : $1}' 2>/dev/null || echo 0)
NPU_MEMORY_USAGE=$(echo "$NPU_MEMORY_USAGE" | awk '{print ($1 > 100) ? 100 : ($1 < 0) ? 0 : $1}' 2>/dev/null || echo 0)

# 输出结果，确保每行都有值
echo "CPU_USAGE:$CPU_USAGE"
echo "MEMORY_USAGE:$MEMORY_USAGE"
echo "MEMORY_TOTAL:$MEMORY_TOTAL"
echo "MEMORY_AVAILABLE:$MEMORY_AVAILABLE"
echo "DISK_USAGE:$DISK_USAGE"
echo "LOAD_AVG_1M:$LOAD_AVG_1M"
echo "LOAD_AVG_5M:$LOAD_AVG_5M"
echo "LOAD_AVG_15M:$LOAD_AVG_15M"
echo "NPU_USAGE:$NPU_USAGE"
echo "NPU_MEMORY_USAGE:$NPU_MEMORY_USAGE"
echo "NPU_POWER:$NPU_POWER"
echo "NPU_TEMPERATURE:$NPU_TEMPERATURE"
echo "SYSTEM_TEMPERATURE:$SYSTEM_TEMPERATURE"
exit 0
)RAW";
        
        // 收集SSH命令执行结果
        std::string result;
        
        bool success = device_manager.executeOnDevice(device_ip, ssh_command, 
            [&result](const std::string& output) {
                result += output;
                // 不再将输出直接打印到终端，而是仅保存到result变量
            });
            
        if (!success) {
            // 命令执行失败不代表设备连接断开，保持connected:true
            // 只更新性能数据为0，表示无法获取性能数据
            metrics.connected = true;
            // 性能数据设置为0，但保持连接状态为true
        } else {
            parseMetricsFromResult(result, metrics);
            metrics.connected = true;
        }
    } catch (...) {
        // 异常发生不代表设备连接断开，保持connected:true
        metrics.connected = true;
        // 性能数据设置为0，但保持连接状态为true
    }

    return metrics;
}

// PerformanceMonitor实现
PerformanceMonitor::PerformanceMonitor(DeviceManager& device_manager)
    : running_(false), interval_ms_(2000), device_manager_(&device_manager) {
    
    collector_ = std::make_unique<SSHPerformanceCollector>();
    collector_->initialize();
}

PerformanceMonitor::~PerformanceMonitor() {
    stop();
    if (collector_) {
        collector_->cleanup();
    }
}

bool PerformanceMonitor::start() {
    if (running_.exchange(true)) {
        return false; // 已经在运行
    }

    monitor_thread_ = std::thread([this]() {
        while (running_) {
            collectAndBroadcast();
            
            // 等待指定间隔或提前退出
            std::unique_lock<std::mutex> lock(stop_mutex_);
            stop_cv_.wait_for(lock, std::chrono::milliseconds(interval_ms_), [this] { return !running_; });
        }
    });

    return true;
}

void PerformanceMonitor::stop() {
    if (running_.exchange(false)) {
        stop_cv_.notify_all();
        if (monitor_thread_.joinable()) {
            monitor_thread_.join();
        }
    }
}

void PerformanceMonitor::setCollectionInterval(int milliseconds) {
    interval_ms_ = milliseconds;
}

void PerformanceMonitor::addDevice(const std::string& ip) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    devices_.insert(ip);
    
    // 当添加设备时，初始化其缓存数据
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        PerformanceMetrics initial_metrics;
        initial_metrics.device_ip = ip;
        initial_metrics.connected = false;
        cached_metrics_[ip] = initial_metrics;
    }
}

void PerformanceMonitor::startMonitoringForDevice(const std::string& ip) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    devices_.insert(ip);
    
    // 确保设备也在缓存中
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        if (cached_metrics_.find(ip) == cached_metrics_.end()) {
            PerformanceMetrics initial_metrics;
            initial_metrics.device_ip = ip;
            initial_metrics.connected = false;
            cached_metrics_[ip] = initial_metrics;
        }
    }
}

void PerformanceMonitor::stopMonitoringForDevice(const std::string& ip) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    devices_.erase(ip);
    
    // 同时从缓存中移除设备数据
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        cached_metrics_.erase(ip);
    }
    
    // 唤醒等待线程，使其能够立即响应设备列表变化
    stop_cv_.notify_all();
}

void PerformanceMonitor::removeDevice(const std::string& ip) {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    devices_.erase(ip);
    
    // 当移除设备时，也从缓存中清除其数据
    {
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        cached_metrics_.erase(ip);
    }
}

PerformanceMetrics PerformanceMonitor::collectMetricsForDevice(const std::string& ip) {
    if (device_manager_) {
        return collector_->collectMetrics(ip, *device_manager_);
    }
    PerformanceMetrics empty_metrics;
    empty_metrics.device_ip = ip;
    empty_metrics.connected = false;
    return empty_metrics;
}

PerformanceMetrics PerformanceMonitor::getCachedMetricsForDevice(const std::string& ip) {
    std::lock_guard<std::mutex> cache_lock(cache_mutex_);
    auto it = cached_metrics_.find(ip);
    if (it != cached_metrics_.end()) {
        return it->second;
    }
    
    // 如果缓存中没有找到，则返回默认的性能指标
    PerformanceMetrics default_metrics;
    default_metrics.device_ip = ip;
    default_metrics.connected = false;
    return default_metrics;
}

void PerformanceMonitor::setOutputCallback(std::function<void(const std::string&)> callback) {
    output_callback_ = callback;
}

void PerformanceMonitor::collectAndBroadcast() {
    // 每次循环都获取最新的设备列表，确保能够立即响应设备列表变化
    std::set<std::string> current_devices;
    {
        std::lock_guard<std::mutex> lock(devices_mutex_);
        current_devices = devices_;
    }
    
    if (current_devices.empty()) {
        return;
    }
    
    // 遍历当前设备列表
    for (const auto& device_ip : current_devices) {
        // 检查是否应该停止，避免在停止过程中继续执行
        if (!running_) {
            break;
        }
                
        // 再次检查设备是否仍在监控列表中，如果已被移除则跳过后续处理
        {
            std::lock_guard<std::mutex> lock(devices_mutex_);
            if (devices_.find(device_ip) == devices_.end()) {
                continue; // 此设备已被移除，跳过
            }
        }
                
        // 传递DeviceManager引用到collectMetrics方法
        if (device_manager_) {
            // 首先检查设备连接状态
            bool isActuallyConnected = device_manager_->isDeviceConnected(device_ip);
            
            // 再次检查设备是否仍在监控列表中，确保停止操作能够立即生效
            {
                std::lock_guard<std::mutex> lock(devices_mutex_);
                if (devices_.find(device_ip) == devices_.end()) {
                    continue; // 此设备已被移除，跳过
                }
            }
            
            PerformanceMetrics metrics = collector_->collectMetrics(device_ip, *device_manager_);
            
            // 确保连接状态与设备管理器一致
            metrics.connected = isActuallyConnected;
            
            // 再次检查设备是否仍在监控列表中，确保停止操作能够立即生效
            {
                std::lock_guard<std::mutex> lock(devices_mutex_);
                if (devices_.find(device_ip) == devices_.end()) {
                    continue; // 此设备已被移除，跳过
                }
            }
            
            // 检查是否应该停止，避免在停止过程中继续执行
            if (!running_) {
                break;
            }
                    
            // 更新缓存中的数据
            {
                std::lock_guard<std::mutex> cache_lock(cache_mutex_);
                cached_metrics_[device_ip] = metrics;
            }
                    
            // 再次检查设备是否仍在监控列表中，确保停止操作能够立即生效
            {
                std::lock_guard<std::mutex> lock(devices_mutex_);
                if (devices_.find(device_ip) == devices_.end()) {
                    continue; // 此设备已被移除，跳过
                }
            }
            
            // 检查是否应该停止，避免在停止过程中继续执行
            if (!running_) {
                break;
            }
                    
            // 调用输出回调，将性能数据发送给WebSocket或其他接收方
            if (output_callback_) {
                auto j = metrics.toJson();
                std::string json_str = j.dump();
                output_callback_(json_str);
            } else {
                // 只记录错误级别的日志
                std::cerr << "[PerformanceMonitor] Error: output_callback_ is null, cannot broadcast metrics for device " << device_ip << std::endl;
            }
        } else {
            // 只记录错误级别的日志
            std::cerr << "[PerformanceMonitor] Error: device_manager_ is null for device " << device_ip << std::endl;
        }
    }
}
