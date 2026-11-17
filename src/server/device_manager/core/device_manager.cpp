// device_manager.cpp
#include "device_manager.h"
#include "../network/nmap_scanner.h"
#include "../network/ping_scanner.h"
#include "../network/scanner.h"
#include "../ssh_client.h"
#include "../utils/mac_utils.h"
#include "../utils/network_utils.h"
#include "../../../performance/performance_monitor.h"  // 添加性能监控头文件
#include <algorithm>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
#include <random>
#include <shared_mutex>
#include <sstream>
#include <unordered_map>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <algorithm>
#include <atomic>

// @formatter:off
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include <icmpapi.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
// @formatter:on

// UUID Generator
std::string generateUUID()
{
  static std::random_device rd;
  static std::mt19937 gen(rd());
  static std::uniform_int_distribution<> dis(0, 255);

  std::stringstream ss;
  ss << std::hex;
  for (int i = 0; i < 16; i++)
  {
    if (i == 4 || i == 6 || i == 8 || i == 10)
    {
      ss << "-";
    }
    ss << std::setw(2) << std::setfill('0') << dis(gen);
  }
  return ss.str();
}

class DeviceManager::Impl
{
public:
  explicit Impl(DeviceManager *parent) : parent_(parent), running_(true)
  {
    loadHistoryFromFile();
  }

  ~Impl()
  {

    std::unique_lock<std::shared_mutex> lock(devices_mutex_);
    for (auto &pair : connections_)
    {
      if (pair.second && pair.second->isConnected())
      {
        pair.second->disconnect();
      }
    }
    connections_.clear();
    devices_.clear();
  }

  std::vector<DeviceInfo>
  scanNetwork(const std::string &mode,
              std::function<void(const std::string &, int, int, size_t)> 
                  progress_callback)
  {
    std::vector<std::string> subnets;

    if (mode == "vmware")
    {
      subnets = getVMwareSubnets();
    }
    else if (mode == "huawei")
    {
      // 华为设备扫描优化：优先扫描常见的华为设备网段
      // 避免扫描过大的网段（如/8、/12），这些网段包含太多IP地址
      std::vector<std::string> commonHuaweiSubnets = {
        "192.168.140.0/24", // 常见的华为升腾开发板网段
        "192.168.137.0/24", // 手册推荐的华为升腾开发板网段
        "192.168.0.0/24",
        "192.168.1.0/24",
        "192.168.10.0/24",
        "192.168.100.0/24"
      };
      
      // 获取本地网络网段，但过滤掉过大的网段
      std::vector<std::string> localSubnets = NetworkUtils::getLocalNetworkSubnets();
      std::vector<std::string> filteredLocalSubnets;
      
      // 过滤掉过大的网段（如/8、/12），只保留/24或更小区间的网段
      for (const auto& subnet : localSubnets) {
        size_t slashPos = subnet.find('/');
        if (slashPos != std::string::npos) {
          std::string prefixStr = subnet.substr(slashPos + 1);
          try {
            int prefix = std::stoi(prefixStr);
            // 只保留子网掩码长度大于等于24的网段
            if (prefix >= 24) {
              filteredLocalSubnets.push_back(subnet);
            }
          } catch (...) {
            // 忽略无效的网段
          }
        }
      }
      
      // 合并网段，确保本地网段优先
      subnets.insert(subnets.end(), filteredLocalSubnets.begin(), filteredLocalSubnets.end());
      subnets.insert(subnets.end(), commonHuaweiSubnets.begin(), commonHuaweiSubnets.end());
      
      // 去重处理
      std::sort(subnets.begin(), subnets.end());
      subnets.erase(std::unique(subnets.begin(), subnets.end()), subnets.end());
    }
    else if (mode == "quick")
    {
      subnets = {"192.168.140.0/24", "192.168.137.0/24"};
    }
    else if (mode == "common")
    {
      subnets = {"192.168.140.0/24", "192.168.137.0/24", "192.168.0.0/24", "192.168.1.0/24",
                 "192.168.10.0/24", "192.168.100.0/24"};
    }
    else if (mode == "full")
    {
      subnets = {"192.168.0.0/16", "10.0.0.0/8", "172.16.0.0/12"};
    }
    else
    {
      subnets = getVMwareSubnets();
    }

    std::cout << "[DeviceManager::Impl] Starting device scan with mode: " << mode 
              << ", subnets count: " << subnets.size() << std::endl;

    std::vector<DeviceInfo> all_devices;
    size_t total = subnets.size();

    // 并行扫描实现
    if (total > 1 && mode != "full") { // 对于full模式，仍然使用串行扫描以避免网络负载过高
      std::vector<std::thread> threads;
      std::vector<std::vector<DeviceInfo>> thread_results(total);
      std::atomic<size_t> completed_count(0);
      std::mutex results_mutex;
      
      std::cout << "[DeviceManager::Impl] Starting parallel scan with " << total << " threads" << std::endl;

      // 启动多个线程进行并行扫描
      for (size_t i = 0; i < total && running_; ++i) {
        threads.emplace_back([this, &subnets, i, mode, &thread_results, &completed_count, &results_mutex, total, &all_devices, progress_callback]() {
          const auto &subnet = subnets[i];
          
          if (progress_callback) {
            progress_callback("starting", static_cast<int>(i + 1),
                              static_cast<int>(total), 0);
          }

          try {
            auto devices = scanWithNmap(subnet, mode);
            
            { // 线程安全地更新结果
              std::lock_guard<std::mutex> lock(results_mutex);
              thread_results[i] = devices;
              all_devices.insert(all_devices.end(), devices.begin(), devices.end());
              std::cout << "[DeviceManager::Impl] Thread " << i << " scanned subnet " << subnet 
                        << ", found " << devices.size() << " devices" << std::endl;
            }
          } catch (const std::exception& e) {
            std::cerr << "[DeviceManager::Impl] Thread " << i << " error scanning subnet " << subnet 
                      << ": " << e.what() << std::endl;
          } catch (...) {
            std::cerr << "[DeviceManager::Impl] Thread " << i << " unknown error scanning subnet " << subnet << std::endl;
          }

          // 更新完成计数并回调进度
          size_t current_completed = completed_count.fetch_add(1) + 1;
          if (progress_callback) {
            size_t current_devices = 0;
            {
              std::lock_guard<std::mutex> lock(results_mutex);
              current_devices = all_devices.size();
            }
            progress_callback("completed", static_cast<int>(current_completed),
                              static_cast<int>(total), current_devices);
          }
        });
      }

      // 等待所有线程完成
      for (auto &thread : threads) {
        if (thread.joinable()) {
          thread.join();
        }
      }
      
      std::cout << "[DeviceManager::Impl] Parallel scan completed" << std::endl;
    } else {
      // 串行扫描（用于full模式或网段数量较少的情况）
      for (size_t i = 0; i < total && running_; ++i)
      {
        const auto &subnet = subnets[i];
        if (progress_callback)
        {
          progress_callback("starting", static_cast<int>(i + 1),
                            static_cast<int>(total), all_devices.size());
        }

        try {
          auto devices = scanWithNmap(subnet, mode);
          all_devices.insert(all_devices.end(), devices.begin(), devices.end());
          std::cout << "[DeviceManager::Impl] Scanned subnet " << subnet 
                    << ", found " << devices.size() << " devices" << std::endl;
        } catch (const std::exception& e) {
          std::cerr << "[DeviceManager::Impl] Error scanning subnet " << subnet 
                    << ": " << e.what() << std::endl;
        } catch (...) {
          std::cerr << "[DeviceManager::Impl] Unknown error scanning subnet " << subnet << std::endl;
        }

        if (progress_callback)
        {
          progress_callback("completed", static_cast<int>(i + 1),
                            static_cast<int>(total), all_devices.size());
        }
      }
    }

    // 去重处理
    std::unordered_map<std::string, DeviceInfo> unique_devices;
    for (const auto& device : all_devices) {
      unique_devices[device.ip_address] = device;
    }
    
    std::vector<DeviceInfo> unique_all_devices;
    for (const auto& pair : unique_devices) {
      unique_all_devices.push_back(pair.second);
    }

    std::cout << "[DeviceManager::Impl] Total unique devices found: " << unique_all_devices.size() << std::endl;

    // Update devices map
    {
      std::unique_lock<std::shared_mutex> lock(devices_mutex_);
      for (const auto &device : unique_all_devices)
      {
        devices_[device.ip_address] = device;
      }
    }

    return unique_all_devices;
  }

  std::vector<DeviceInfo> scanWithNmap(const std::string &target,
                                       const std::string &mode)
  {
    std::cout << "[DeviceManager::Impl] Starting scan for target: " << target << " with mode: " << mode << std::endl;
    
    // 首先尝试使用NMAP扫描器
    auto nmap_scanner =
        ScannerFactory::createScanner(ScannerFactory::ScannerType::NMAP);
    
    std::vector<DeviceInfo> result;
    
    // 尝试NMAP扫描
    if (nmap_scanner) {
      std::cout << "[DeviceManager::Impl] Using NMAP scanner for target: " << target << std::endl;
      try {
        result = nmap_scanner->scan(target, mode);
        std::cout << "[DeviceManager::Impl] NMAP scan completed for target: " << target 
                  << ", found " << result.size() << " devices" << std::endl;
        
        // 如果NMAP扫描成功且找到设备，直接返回结果
        if (!result.empty()) {
          std::cout << "[DeviceManager::Impl] NMAP scan successful, returning results" << std::endl;
          return result;
        }
      } catch (const std::exception& e) {
        std::cerr << "[DeviceManager::Impl] Exception during NMAP scan for target " << target 
                  << ": " << e.what() << std::endl;
      } catch (...) {
        std::cerr << "[DeviceManager::Impl] Unknown exception during NMAP scan for target " 
                  << target << std::endl;
      }
    } else {
      std::cout << "[DeviceManager::Impl] NMAP scanner not available, falling back to PING scanner" << std::endl;
    }
    
    // 如果NMAP扫描失败或未找到设备，尝试使用PING扫描器
    std::cout << "[DeviceManager::Impl] Falling back to PING scanner for target: " << target << std::endl;
    auto ping_scanner =
        ScannerFactory::createScanner(ScannerFactory::ScannerType::PING);
    
    if (ping_scanner) {
      std::cout << "[DeviceManager::Impl] Using PING scanner for target: " << target << std::endl;
      try {
        result = ping_scanner->scan(target, mode);
        std::cout << "[DeviceManager::Impl] PING scan completed for target: " << target 
                  << ", found " << result.size() << " devices" << std::endl;
      } catch (const std::exception& e) {
        std::cerr << "[DeviceManager::Impl] Exception during PING scan for target " << target 
                  << ": " << e.what() << std::endl;
      } catch (...) {
        std::cerr << "[DeviceManager::Impl] Unknown exception during PING scan for target " 
                  << target << std::endl;
      }
    } else {
      std::cerr << "[DeviceManager::Impl] Failed to create any scanner for target: " << target << std::endl;
    }
    
    // 去重处理，确保返回的设备列表中没有重复的设备
    std::unordered_map<std::string, DeviceInfo> unique_devices;
    for (const auto& device : result) {
      unique_devices[device.ip_address] = device;
    }
    
    std::vector<DeviceInfo> unique_result;
    for (const auto& pair : unique_devices) {
      unique_result.push_back(pair.second);
    }
    
    std::cout << "[DeviceManager::Impl] Scan completed for target: " << target 
              << ", unique devices found: " << unique_result.size() << std::endl;
    
    return unique_result;
  }

  bool connectToDevice(const std::string &ip, const std::string &user,
                       const std::string &pass)
  {
    std::cout << "[DeviceManager::Impl] connectToDevice called for IP: " << ip
              << " with user: " << user << std::endl;

    std::unique_lock<std::shared_mutex> lock(devices_mutex_);

    // Check if already connected
    auto it = connections_.find(ip);
    if (it != connections_.end() && it->second && it->second->isConnected())
    {
      std::cout << "[DeviceManager::Impl] Device already connected: " << ip << std::endl;
      return true;
    }

    // Create new SSH connection
    std::cout << "[DeviceManager::Impl] Creating new SSH connection" << std::endl;
    auto ssh = std::make_shared<SSHClient>();
    if (!ssh->connect(ip, 22, user, pass))
    {
      std::cout << "[DeviceManager::Impl] SSH connection failed for IP: " << ip << std::endl;
      return false;
    }

    // Update connection map
    connections_[ip] = ssh;

    // Ensure device exists in devices_ map
    if (devices_.find(ip) == devices_.end())
    {
      // Create new device entry
      DeviceInfo device;
      device.ip_address = ip;
      device.hostname = resolveHostname(ip);
      device.is_online = true;
      device.is_connected = true;
      device.last_seen = getCurrentTime();
      device.username = user;
      device.password = pass;
      devices_[ip] = device;
      std::cout << "[DeviceManager::Impl] Added new device to devices_ map: " << ip
                << " with user: " << user << std::endl;
    }
    else
    {
      // Update existing device entry
      devices_[ip].is_connected = true;
      devices_[ip].username = user;
      devices_[ip].password = pass;
      devices_[ip].last_seen = getCurrentTime();
      std::cout << "[DeviceManager::Impl] Updated device info for IP: " << ip
                << " with user: " << user << std::endl;
    }

    // After successful connection, notify performance monitor if available
    if (performance_monitor_) {
      performance_monitor_->addDevice(ip);
    }

    std::cout << "[DeviceManager::Impl] Device connected successfully: " << ip << std::endl;
    return true;
  }

  bool connectToDeviceWithConfig(const std::string &ip, const std::string &user,
                                const std::string &pass, int port, int timeout)
  {
    std::cout << "[DeviceManager::Impl] connectToDeviceWithConfig called for IP: " << ip
              << " with user: " << user << ", port: " << port << ", timeout: " << timeout << std::endl;

    std::unique_lock<std::shared_mutex> lock(devices_mutex_);

    // Check if already connected
    auto it = connections_.find(ip);
    if (it != connections_.end() && it->second && it->second->isConnected())
    {
      std::cout << "[DeviceManager::Impl] Device already connected: " << ip << std::endl;
      return true;
    }

    // Create new SSH connection with custom port and timeout
    std::cout << "[DeviceManager::Impl] Creating new SSH connection with custom config" << std::endl;
    auto ssh = std::make_shared<SSHClient>();
    if (!ssh->connectWithConfig(ip, port, user, pass, timeout))
    {
      std::cout << "[DeviceManager::Impl] SSH connection failed for IP: " << ip << std::endl;
      return false;
    }

    // Update connection map
    connections_[ip] = ssh;

    // Ensure device exists in devices_ map
    if (devices_.find(ip) == devices_.end())
    {
      // Create new device entry
      DeviceInfo device;
      device.ip_address = ip;
      device.hostname = resolveHostname(ip);
      device.is_online = true;
      device.is_connected = true;
      device.last_seen = getCurrentTime();
      device.username = user;
      device.password = pass;
      devices_[ip] = device;
      std::cout << "[DeviceManager::Impl] Added new device to devices_ map: " << ip
                << " with user: " << user << std::endl;
    }
    else
    {
      // Update existing device entry
      devices_[ip].is_connected = true;
      devices_[ip].username = user;
      devices_[ip].password = pass;
      devices_[ip].last_seen = getCurrentTime();
      std::cout << "[DeviceManager::Impl] Updated device info for IP: " << ip
                << " with user: " << user << std::endl;
    }

    // After successful connection, notify performance monitor if available
    if (performance_monitor_) {
      performance_monitor_->addDevice(ip);
    }

    std::cout << "[DeviceManager::Impl] Device connected successfully: " << ip << std::endl;
    return true;
  }

  void disconnectDevice(const std::string &ip)
  {
    // Before disconnecting, notify performance monitor if available
    if (performance_monitor_) {
      performance_monitor_->removeDevice(ip);
    }

    std::unique_lock<std::shared_mutex> lock(devices_mutex_);

    auto it = connections_.find(ip);
    if (it != connections_.end())
    {
      if (it->second)
      {
        it->second->disconnect();
      }
      connections_.erase(it);
    }

    // Update device info
    if (devices_.find(ip) != devices_.end())
    {
      devices_[ip].is_connected = false;
      devices_[ip].is_online = false; // Also mark as offline
    }
  }

  bool executeOnDevice(const std::string &ip, const std::string &cmd,
                       std::function<void(const std::string &)> cb) {
    std::unique_lock<std::shared_mutex> lock(devices_mutex_);

    auto it = connections_.find(ip);
    if (it == connections_.end() || !it->second || !it->second->isConnected()) {
      return false;
    }
    
    // 直接尝试执行命令，不进行严格的连接验证
    bool success = it->second->executeCommand(cmd, cb);
    
    // 如果命令执行失败，更新设备状态
    if (!success) {
      // 尝试验证连接是否仍然有效
      bool isActuallyConnected = it->second->isActuallyConnected();
      if (!isActuallyConnected) {
        // 如果连接已失效，更新设备状态
        if (devices_.find(ip) != devices_.end()) {
          devices_[ip].is_connected = false;
          devices_[ip].is_online = false;
        }
        connections_.erase(it);
      }
    }

    return success;
  }
  std::vector<DeviceInfo> getDevices() const
  {
    std::shared_lock<std::shared_mutex> lock(devices_mutex_);
    std::vector<DeviceInfo> devices;
    devices.reserve(devices_.size());

    for (const auto &pair : devices_)
    {
      devices.push_back(pair.second);
    }

    return devices;
  }

  DeviceInfo getDevice(const std::string &ip) const
  {
    std::shared_lock<std::shared_mutex> lock(devices_mutex_);
    auto it = devices_.find(ip);
    if (it != devices_.end())
    {
      return it->second;
    }
    return DeviceInfo{}; // Return default constructed object
  }

  bool isDeviceConnected(const std::string &ip) const
  {
    std::shared_lock<std::shared_mutex> lock(devices_mutex_);
    auto it = connections_.find(ip);
    bool isConnected = it != connections_.end() && it->second && it->second->isConnected();
    
    // 如果SSH连接存在，进一步验证连接是否仍然有效
    if (isConnected && it->second) {
      isConnected = it->second->isActuallyConnected();
    }
    
    return isConnected;
  }
  
  void updateDeviceConnectionStatus(const std::string &ip)
  {
    std::unique_lock<std::shared_mutex> lock(devices_mutex_);
    auto it = connections_.find(ip);
    if (it != connections_.end() && it->second) {
      bool isConnected = it->second->isConnected();
      bool isActuallyConnected = false;
      
      // 如果基础连接状态为真，再进行深度连接验证
      if (isConnected) {
        isActuallyConnected = it->second->isActuallyConnected();
      } else {
        isActuallyConnected = false;
      }
      
      // 更新设备状态，但保持在线状态的稳定性
      if (devices_.find(ip) != devices_.end()) {
        devices_[ip].is_connected = isActuallyConnected;
        // 保持在线状态，除非经过多次验证确定设备离线
        // 只有当设备实际断开连接时才更新在线状态，避免短暂连接问题导致状态变化
        if (isActuallyConnected) {
          devices_[ip].is_online = true; // 连接有效，设备在线
        }
        // 注意：不将is_online设置为false，除非通过其他机制（如ping测试）确认设备离线
      }
      
      // 如果SSH连接已断开，清理连接
      if (!isActuallyConnected) {
        connections_.erase(it);
      }
    }
  }

  bool pingDevice(const std::string &ip)
  {
#ifdef _WIN32
    HANDLE hIcmpFile = IcmpCreateFile();
    if (hIcmpFile == INVALID_HANDLE_VALUE)
    {
      return false;
    }

    ULONG dest_ip = inet_addr(ip.c_str());
    if (dest_ip == INADDR_NONE)
    {
      IcmpCloseHandle(hIcmpFile);
      return false;
    }

    char sendData[32] = "ping";
    DWORD replySize = sizeof(ICMP_ECHO_REPLY) + sizeof(sendData) + 8;
    std::unique_ptr<char[]> replyBuffer(new char[replySize]);

    DWORD status = IcmpSendEcho(hIcmpFile, dest_ip, sendData, sizeof(sendData),
                                NULL, replyBuffer.get(), replySize, 1000);

    bool result = (status != 0);

    IcmpCloseHandle(hIcmpFile);
    return result;
#else
    return false;
#endif
  }

  std::string getRealMacAddress(const std::string &ip)
  {
#ifdef _WIN32
    ULONG dest_ip = inet_addr(ip.c_str());
    if (dest_ip == INADDR_NONE)
      return "";

    BYTE mac_addr[6];
    ULONG mac_len = 6;
    if (SendARP(dest_ip, 0, mac_addr, &mac_len) == NO_ERROR && mac_len == 6)
    {
      char mac_str[18];
      snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
               mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4],
               mac_addr[5]);
      return mac_str;
    }
#endif
    return "";
  }

  DeviceType identifyDeviceType(const std::string &mac)
  {
    if (mac.length() < 8)
      return DeviceType::UNKNOWN;
    std::string oui = mac.substr(0, 8);

    if (oui == "00:50:56" || oui == "00:0C:29" || oui == "00:1C:14" ||
        oui == "00:05:69")
    {
      return DeviceType::VMWARE_VIRTUAL_MACHINE;
    }
    if (oui == "00:9A:CD" || oui == "00:E0:FC" || oui == "A4:4C:C8" ||
        oui == "FC:48:EF")
    {
      return DeviceType::HUAWEI_ATLAS;
    }
    return DeviceType::OTHER;
  }

  std::vector<std::string> getVMwareSubnets() const
  {
    return {"192.168.140.0/24", "192.168.1.0/24", "192.168.0.0/24",
            "192.168.10.0/24", "192.168.100.0/24"};
  }

  bool getDefaultCredentials(DeviceType type, std::string &user,
                             std::string &pass) const
  {
    switch (type)
    {
    case DeviceType::VMWARE_VIRTUAL_MACHINE:
      user = "ubuntu";
      pass = "password";
      return true;
    case DeviceType::HUAWEI_ATLAS:
      user = "root";
      pass = "mind@123";
      return true;
    default:
      return false;
    }
  }

  std::string resolveHostname(const std::string &ip)
  {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
      return ip;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;

    INT addrLen = sizeof(addr);
    if (WSAStringToAddressA(const_cast<LPSTR>(ip.c_str()), AF_INET, NULL,
                            (struct sockaddr *)&addr, &addrLen) != 0)
    {
      WSACleanup();
      return ip;
    }

    char host[NI_MAXHOST] = {0};
    int result = getnameinfo((sockaddr *)&addr, sizeof(addr), host,
                             sizeof(host), nullptr, 0, 0);

    WSACleanup();

    if (result == 0 && host[0] != '\0')
    {
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

    if (result == 0 && host[0] != '\0')
    {
      return std::string(host);
    }
#endif
    return ip;
  }

  std::string getCurrentTime()
  {
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream oss;
    oss << std::put_time(std::localtime(&t), "%Y-%m-%d %H:%M:%S");
    return oss.str();
  }

  std::vector<DeviceInfo> getHistoryDevices() const
  {
    std::shared_lock<std::shared_mutex> lock(history_mutex_);
    return history_devices_;
  }

  void addHistoryDevice(const DeviceInfo &dev)
  {
    std::unique_lock<std::shared_mutex> lock(history_mutex_);

    // Remove existing entry if present
    history_devices_.erase(
        std::remove_if(history_devices_.begin(), history_devices_.end(),
                       [&dev](const DeviceInfo &d)
                       {
                         return d.ip_address == dev.ip_address;
                       }),
        history_devices_.end());

    // Add to front of list
    history_devices_.insert(history_devices_.begin(), dev);

    // Limit history to 100 entries
    if (history_devices_.size() > 100)
    {
      history_devices_.resize(100);
    }

    saveHistoryToFile();
  }

  void removeHistoryDevice(const std::string &ip)
  {
    std::unique_lock<std::shared_mutex> lock(history_mutex_);
    history_devices_.erase(std::remove_if(history_devices_.begin(),
                                          history_devices_.end(),
                                          [&ip](const DeviceInfo &d)
                                          {
                                            return d.ip_address == ip;
                                          }),
                           history_devices_.end());
    saveHistoryToFile();
  }

  std::vector<DeviceInfo> scanHistoryDevices()
  {
    std::shared_lock<std::shared_mutex> lock(history_mutex_);
    std::vector<DeviceInfo> online_devices;

    for (auto &device : history_devices_)
    {
      if (pingDevice(device.ip_address))
      {
        device.is_online = true;
        device.last_seen = getCurrentTime();
        online_devices.push_back(device);
      }
    }

    return online_devices;
  }

  void setPerformanceMonitorInternal(PerformanceMonitor* monitor) {
    performance_monitor_ = monitor;
  }
  
  PerformanceMonitor* getPerformanceMonitorInternal() const {
    return performance_monitor_;
  }

private:
  void saveHistoryToFile() const
  {
    try
    {
      nlohmann::json j;
      for (const auto &device : history_devices_)
      {
        nlohmann::json device_json;
        device_json["ip_address"] = device.ip_address;
        device_json["hostname"] = device.hostname;
        device_json["mac_address"] = device.mac_address;
        device_json["device_type"] = static_cast<int>(device.device_type);
        device_json["is_online"] = device.is_online;
        device_json["is_connected"] = device.is_connected;
        device_json["last_seen"] = device.last_seen;
        j.push_back(device_json);
      }

      std::ofstream file("device_history.json");
      if (file.is_open())
      {
        file << j.dump(4);
        file.close();
      }
    }
    catch (const std::exception &e)
    {
      std::cerr << "Failed to save history to file: " << e.what() << std::endl;
    }
  }

  void loadHistoryFromFile()
  {
    try
    {
      std::ifstream file("device_history.json");
      if (file.is_open())
      {
        nlohmann::json j;
        file >> j;
        file.close();

        history_devices_.clear();
        for (const auto &device_json : j)
        {
          DeviceInfo device;
          device.ip_address = device_json.value("ip_address", "");
          device.hostname = device_json.value("hostname", "");
          device.mac_address = device_json.value("mac_address", "");
          device.device_type =
              static_cast<DeviceType>(device_json.value("device_type", 0));
          device.is_online = device_json.value("is_online", false);
          device.is_connected = device_json.value("is_connected", false);
          device.last_seen = device_json.value("last_seen", "");
          history_devices_.push_back(device);
        }
      }
    }
    catch (const std::exception &e)
    {
      std::cerr << "Failed to load history from file: " << e.what()
                << std::endl;
      history_devices_.clear();
    }
  }
  
  DeviceManager *parent_;
  mutable std::shared_mutex devices_mutex_;
  std::unordered_map<std::string, DeviceInfo> devices_;
  std::unordered_map<std::string, std::shared_ptr<class SSHClient>>
      connections_;
  std::atomic<bool> running_;
  // History management
  mutable std::shared_mutex history_mutex_;
  std::vector<DeviceInfo> history_devices_;
  
  // Performance monitoring
  PerformanceMonitor* performance_monitor_ = nullptr;
};

// Methods to set/get the performance monitor reference
void DeviceManager::setPerformanceMonitor(PerformanceMonitor* monitor) {
  if (impl_) {
    impl_->setPerformanceMonitorInternal(monitor);
  }
}

PerformanceMonitor* DeviceManager::getPerformanceMonitor() const {
  if (impl_) {
    return impl_->getPerformanceMonitorInternal();
  }
  return nullptr;
}

// === Public Methods ===

DeviceManager::DeviceManager(const std::string& default_ssh_username) : impl_(std::make_unique<Impl>(this)) {}

DeviceManager::~DeviceManager() = default;

std::vector<DeviceInfo> DeviceManager::scanNetwork(
    const std::string &mode,
    std::function<void(const std::string &, int, int, size_t)> callback)
{
  return impl_->scanNetwork(mode, callback);
}

std::vector<DeviceInfo> DeviceManager::scanSubnet(const std::string &subnet)
{
  return impl_->scanWithNmap(subnet, "quick");
}

bool DeviceManager::connectToDevice(const std::string &ip,
                                    const std::string &user,
                                    const std::string &pass)
{
  return impl_->connectToDevice(ip, user, pass);
}

bool DeviceManager::connectToDeviceWithConfig(const std::string &ip,
                                           const std::string &user,
                                           const std::string &pass,
                                           int port,
                                           int timeout)
{
  return impl_->connectToDeviceWithConfig(ip, user, pass, port, timeout);
}

void DeviceManager::disconnectDevice(const std::string &ip)
{
  impl_->disconnectDevice(ip);
}

bool DeviceManager::executeOnDevice(
    const std::string &ip, const std::string &cmd,
    std::function<void(const std::string &)> cb)
{
  return impl_->executeOnDevice(ip, cmd, cb);
}

std::vector<DeviceInfo> DeviceManager::getDevices() const
{
  return impl_->getDevices();
}

DeviceInfo DeviceManager::getDevice(const std::string &ip) const
{
  return impl_->getDevice(ip);
}

bool DeviceManager::isDeviceConnected(const std::string &ip) const
{
  return impl_->isDeviceConnected(ip);
}

void DeviceManager::updateDeviceConnectionStatus(const std::string &ip)
{
  impl_->updateDeviceConnectionStatus(ip);
}

std::vector<DeviceInfo> DeviceManager::getHistoryDevices() const
{
  return impl_->getHistoryDevices();
}

void DeviceManager::addHistoryDevice(const DeviceInfo &dev)
{
  impl_->addHistoryDevice(dev);
}

void DeviceManager::removeHistoryDevice(const std::string &ip)
{
  impl_->removeHistoryDevice(ip);
}

std::vector<DeviceInfo> DeviceManager::scanHistoryDevices()
{
  return impl_->scanHistoryDevices();
}

std::vector<std::string> DeviceManager::getVMwareSubnets() const
{
  return impl_->getVMwareSubnets();
}

bool DeviceManager::getDefaultCredentials(DeviceType type, std::string &user,
                                          std::string &pass) const
{
  return impl_->getDefaultCredentials(type, user, pass);
}
