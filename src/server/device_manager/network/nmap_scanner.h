// src/server/device_manager/network/nmap_scanner.h
#pragma once

#include "scanner.h"
#include <string>
#include <vector>

class NmapScanner : public NetworkScannerInterface {
public:
    std::vector<DeviceInfo> scan(const std::string& target, const std::string& mode) override;

private:
    bool isNmapAvailable();
    std::string executeNmapCommand(const std::string& target, const std::string& mode);
    std::vector<DeviceInfo> parseNmapOutput(const std::string& nmap_output);
};