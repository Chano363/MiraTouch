#include <iostream>
#include <vector>
#include "src/server/device_manager/utils/mac_utils.h"

int main() {
    std::cout << "=== 设备类型识别功能测试 ===\n";
    
    // 测试华为升腾设备的MAC地址
    std::vector<std::string> huaweiMacs = {
        "00:9A:CD:12:34:56",
        "00:E0:FC:12:34:56",
        "A4:4C:C8:12:34:56",
        "FC:48:EF:12:34:56",
        "00:18:82:12:34:56",
        "00:1A:4A:12:34:56"
    };
    
    // 测试VMware虚拟机的MAC地址
    std::vector<std::string> vmwareMacs = {
        "00:50:56:12:34:56",
        "00:0C:29:12:34:56",
        "00:1C:14:12:34:56",
        "00:05:69:12:34:56"
    };
    
    // 测试其他设备的MAC地址
    std::vector<std::string> otherMacs = {
        "00:11:22:12:34:56",
        "00:22:33:12:34:56",
        "00:33:44:12:34:56"
    };
    
    std::cout << "\n1. 测试华为升腾设备识别:\n";
    for (const auto& mac : huaweiMacs) {
        DeviceType type = MacUtils::identifyDeviceType(mac);
        std::cout << "MAC: " << mac << " -> 类型: ";
        switch (type) {
            case DeviceType::HUAWEI_ATLAS:
                std::cout << "HUAWEI_ATLAS (正确)";
                break;
            case DeviceType::VMWARE_VIRTUAL_MACHINE:
                std::cout << "VMWARE_VIRTUAL_MACHINE (错误)";
                break;
            case DeviceType::OTHER:
                std::cout << "OTHER (错误)";
                break;
            case DeviceType::UNKNOWN:
                std::cout << "UNKNOWN (错误)";
                break;
            default:
                std::cout << "未知类型 (错误)";
                break;
        }
        std::cout << std::endl;
    }
    
    std::cout << "\n2. 测试VMware虚拟机识别:\n";
    for (const auto& mac : vmwareMacs) {
        DeviceType type = MacUtils::identifyDeviceType(mac);
        std::cout << "MAC: " << mac << " -> 类型: ";
        switch (type) {
            case DeviceType::HUAWEI_ATLAS:
                std::cout << "HUAWEI_ATLAS (错误)";
                break;
            case DeviceType::VMWARE_VIRTUAL_MACHINE:
                std::cout << "VMWARE_VIRTUAL_MACHINE (正确)";
                break;
            case DeviceType::OTHER:
                std::cout << "OTHER (错误)";
                break;
            case DeviceType::UNKNOWN:
                std::cout << "UNKNOWN (错误)";
                break;
            default:
                std::cout << "未知类型 (错误)";
                break;
        }
        std::cout << std::endl;
    }
    
    std::cout << "\n3. 测试其他设备识别:\n";
    for (const auto& mac : otherMacs) {
        DeviceType type = MacUtils::identifyDeviceType(mac);
        std::cout << "MAC: " << mac << " -> 类型: ";
        switch (type) {
            case DeviceType::HUAWEI_ATLAS:
                std::cout << "HUAWEI_ATLAS (错误)";
                break;
            case DeviceType::VMWARE_VIRTUAL_MACHINE:
                std::cout << "VMWARE_VIRTUAL_MACHINE (错误)";
                break;
            case DeviceType::OTHER:
                std::cout << "OTHER (正确)";
                break;
            case DeviceType::UNKNOWN:
                std::cout << "UNKNOWN (错误)";
                break;
            default:
                std::cout << "未知类型 (错误)";
                break;
        }
        std::cout << std::endl;
    }
    
    std::cout << "\n4. 测试无效MAC地址:\n";
    std::vector<std::string> invalidMacs = {
        "",
        "12:34:56",
        "12:34:56:78",
        "12:34:56:78:9A"
    };
    
    for (const auto& mac : invalidMacs) {
        DeviceType type = MacUtils::identifyDeviceType(mac);
        std::cout << "MAC: \"" << mac << "\" -> 类型: ";
        switch (type) {
            case DeviceType::UNKNOWN:
                std::cout << "UNKNOWN (正确)";
                break;
            default:
                std::cout << "非UNKNOWN (错误)";
                break;
        }
        std::cout << std::endl;
    }
    
    std::cout << "\n=== 测试完成 ===\n";
    return 0;
}
