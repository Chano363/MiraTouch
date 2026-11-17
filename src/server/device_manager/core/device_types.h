#pragma once

// 设备类型枚举，用于识别扫描到的设备类别
enum class DeviceType {
    UNKNOWN,               // 未知设备
    VMWARE_VIRTUAL_MACHINE, // VMware 虚拟机
    HUAWEI_ATLAS,          // 华为 Atlas 设备
    OTHER                  // 其他已知设备
};