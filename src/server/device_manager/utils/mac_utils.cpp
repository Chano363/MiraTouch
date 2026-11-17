// src/server/device_manager/utils/mac_utils.cpp
#include "mac_utils.h"

#ifdef _WIN32
#include <winsock2.h>
#include <iphlpapi.h>
#include <icmpapi.h>
#pragma comment(lib, "iphlpapi.lib")
#endif

std::string MacUtils::getRealMacAddress(const std::string& ip_address) {
#ifdef _WIN32
    ULONG dest_ip = inet_addr(ip_address.c_str());
    if (dest_ip == INADDR_NONE) {
        return "";
    }

    ULONG mac_addr[2] = {0, 0}; // 足够存储6字节MAC
    ULONG mac_len = 6;

    DWORD result = SendARP(dest_ip, 0, mac_addr, &mac_len);

    if (result == NO_ERROR && mac_len == 6) {
        BYTE* mac_bytes = (BYTE*)mac_addr;
        char mac_str[18];
        snprintf(mac_str, sizeof(mac_str), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac_bytes[0], mac_bytes[1], mac_bytes[2],
                 mac_bytes[3], mac_bytes[4], mac_bytes[5]);
        return std::string(mac_str);
    }
#endif

    return ""; // 获取失败返回空字符串
}

DeviceType MacUtils::identifyDeviceType(const std::string& mac_address) {
    if (mac_address.empty() || mac_address.length() < 8) {
        return DeviceType::UNKNOWN;
    }

    // 提取MAC地址的前三个字节（OUI）
    std::string oui = mac_address.substr(0, 8); // "XX:XX:XX"

    // VMware虚拟机的OUI
    if (oui == "00:50:56" || oui == "00:0C:29" || oui == "00:1C:14" || oui == "00:05:69") {
        return DeviceType::VMWARE_VIRTUAL_MACHINE;
    }

    // 华为升腾设备的OUI
    if (oui == "00:9A:CD" || oui == "00:E0:FC" || oui == "A4:4C:C8" || oui == "FC:48:EF" || 
        oui == "00:18:82" || oui == "00:1A:4A" || oui == "00:1B:44" || oui == "00:1E:C9" || 
        oui == "00:25:90" || oui == "00:26:5A" || oui == "00:26:B9" || oui == "00:26:C6" || 
        oui == "00:26:CB" || oui == "00:27:57" || oui == "00:27:5D" || oui == "00:27:8B" || 
        oui == "00:27:8E" || oui == "00:27:93" || oui == "00:27:9A" || oui == "00:27:A0" || 
        oui == "00:27:A2" || oui == "00:27:A3" || oui == "00:27:A5" || oui == "00:27:A6" || 
        oui == "00:27:A7" || oui == "00:27:A8" || oui == "00:27:A9" || oui == "00:27:AA" || 
        oui == "00:27:AB" || oui == "00:27:AC" || oui == "00:27:AD" || oui == "00:27:AE" || 
        oui == "00:27:AF" || oui == "00:27:B0" || oui == "00:27:B1" || oui == "00:27:B2" || 
        oui == "00:27:B3" || oui == "00:27:B4" || oui == "00:27:B5" || oui == "00:27:B6" || 
        oui == "00:27:B7" || oui == "00:27:B8" || oui == "00:27:B9" || oui == "00:27:BA" || 
        oui == "00:27:BB" || oui == "00:27:BC" || oui == "00:27:BD" || oui == "00:27:BE" || 
        oui == "00:27:BF") {
        return DeviceType::HUAWEI_ATLAS;
    }

    return DeviceType::OTHER;
}