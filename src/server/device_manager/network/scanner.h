// src/server/device_manager/network/scanner.h
#pragma once

#include <vector>
#include <string>
#include <memory>
#include <functional>
#include "../../device_manager/core/device_info.h"

class NetworkScannerInterface {
public:
    virtual ~NetworkScannerInterface() = default;
    
    virtual std::vector<DeviceInfo> scan(const std::string& target, const std::string& mode) = 0;
};

class ScannerFactory {
public:
    enum class ScannerType {
        NMAP,
        PING
    };
    
    static std::unique_ptr<NetworkScannerInterface> createScanner(ScannerType type);
};