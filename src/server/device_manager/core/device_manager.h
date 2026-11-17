#pragma once

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include "../../ssh_client.h"
#include "device_info.h"

// Forward declaration
class PerformanceMonitor;
#include <functional>
#include <memory>
#include <string>
#include <vector>


class DeviceManager {
public:
  DeviceManager(const std::string& default_ssh_username = "ubuntu");
  ~DeviceManager();

  std::vector<DeviceInfo>
  scanNetwork(const std::string &mode = "quick",
              std::function<void(const std::string &, int, int, size_t)>
                  progress_callback = nullptr);

  std::vector<DeviceInfo> scanSubnet(const std::string &subnet);

  bool connectToDevice(const std::string &ip_address,
                       const std::string &username,
                       const std::string &password);
  bool connectToDeviceWithConfig(const std::string &ip_address,
                                const std::string &username,
                                const std::string &password,
                                int port = 22,
                                int timeout = 30);
  void disconnectDevice(const std::string &ip_address);

  bool
  executeOnDevice(const std::string &ip_address, const std::string &command,
                  std::function<void(const std::string &)> output_callback);
  std::vector<DeviceInfo> getDevices() const;
  DeviceInfo getDevice(const std::string &ip_address) const;
  bool isDeviceConnected(const std::string &ip_address) const;
  void updateDeviceConnectionStatus(const std::string &ip_address);

  std::vector<DeviceInfo> getHistoryDevices() const;
  void addHistoryDevice(const DeviceInfo &device);
  void removeHistoryDevice(const std::string &ip_address);
  std::vector<DeviceInfo> scanHistoryDevices();

  std::vector<std::string> getVMwareSubnets() const;
  bool getDefaultCredentials(DeviceType device_type, std::string &username,
                             std::string &password) const;
                              
  // Methods to manage performance monitor reference
  void setPerformanceMonitor(PerformanceMonitor* monitor);
  PerformanceMonitor* getPerformanceMonitor() const;
private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};
#endif 