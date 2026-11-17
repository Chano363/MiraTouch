// src/server/device_manager/utils/mac_utils.h
#pragma once

#include <string>
#include "../core/device_types.h"

class MacUtils {
public:
    // 获取真实的MAC地址（使用SendARP API）
    static std::string getRealMacAddress(const std::string& ip_address);

    // 根据MAC地址识别设备类型
    static DeviceType identifyDeviceType(const std::string& mac_address);
};