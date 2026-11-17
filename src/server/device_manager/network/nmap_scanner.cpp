// src/server/device_manager/network/nmap_scanner.cpp
#include "nmap_scanner.h"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <memory>
#include <array>
#include "../utils/network_utils.h"
#include "../utils/mac_utils.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#endif

bool NmapScanner::isNmapAvailable() {
#ifdef HAVE_NMAP
    return true;
#else
    return false;
#endif
}

std::string NmapScanner::executeNmapCommand(const std::string& target, const std::string& mode) {
#ifdef HAVE_NMAP
    std::string command;

    if (mode == "vmware") {
        command = "nmap -T4 -p 22 --open " + target + " 2>&1";
    } else if (mode == "huawei") {
        command = "nmap -T4 -p 22,23,80,443,161,162 --open " + target + " 2>&1";
    } else if (mode == "quick") {
        command = "nmap -T4 -sn --script mac-address " + target + " 2>&1";
    } else if (mode == "common") {
        command = "nmap -T4 -p 21,22,23,80,443,3389 --open " + target + " 2>&1";
    } else if (mode == "full") {
        command = "nmap -T4 -p- --open " + target + " 2>&1";
    } else {
        command = "nmap -T4 -sn --script mac-address " + target + " 2>&1";
    }

#ifdef _WIN32
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) return "";

    char buffer[128];
    std::string result = "";
    while (!feof(pipe)) {
        if (fgets(buffer, 128, pipe) != nullptr)
            result += buffer;
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    return result;
#else
    return "";
#endif
}

std::vector<DeviceInfo> NmapScanner::parseNmapOutput(const std::string& nmap_output) {
    std::vector<DeviceInfo> devices;
    std::istringstream iss(nmap_output);
    std::string line;

    std::string current_ip;
    std::string current_mac;
    std::string current_hostname;

    while (std::getline(iss, line)) {
        // 查找IP地址行
        if (line.find("Nmap scan report for") != std::string::npos) {
            // 解析IP地址和主机名
            size_t start = line.find("for ") + 4;
            size_t end = line.find(" (", start);
            if (end != std::string::npos) {
                current_hostname = line.substr(start, end - start);
                start = end + 2;
                end = line.find(")", start);
                if (end != std::string::npos) {
                    current_ip = line.substr(start, end - start);
                }
            } else {
                // 如果没有主机名，只有IP
                current_ip = line.substr(start);
                current_hostname = current_ip;
            }
        }

        // 查找MAC地址行
        if (line.find("MAC Address:") != std::string::npos) {
            size_t start = line.find("MAC Address: ") + 13;
            size_t end = line.find(" (", start);
            if (end != std::string::npos) {
                current_mac = line.substr(start, end - start);
            } else {
                current_mac = line.substr(start);
            }

            // 当找到MAC地址时，创建设备信息
            if (!current_ip.empty()) {
                DeviceInfo device;
                device.ip_address = current_ip;
                device.hostname = current_hostname;
                device.mac_address = current_mac;
                device.device_type = MacUtils::identifyDeviceType(current_mac);
                device.is_online = true;
                device.is_connected = false;
                device.last_seen = NetworkUtils::getCurrentTime();

                devices.push_back(device);

                // 重置当前设备信息
                current_ip.clear();
                current_mac.clear();
                current_hostname.clear();
            }
        }
    }

    return devices;
}

std::vector<DeviceInfo> NmapScanner::scan(const std::string& target, const std::string& mode) {
    if (!isNmapAvailable()) {
        return {}; // nmap不可用时返回空列表
    }

    std::string nmap_output = executeNmapCommand(target, mode);
    if (nmap_output.empty()) {
        return {}; // 执行失败时返回空列表
    }

    return parseNmapOutput(nmap_output);
}