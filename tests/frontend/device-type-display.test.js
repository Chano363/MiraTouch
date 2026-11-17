const assert = require('assert');

describe('Device Type Display Tests', function() {
  // 模拟设备类型映射表和getDeviceTypeDisplay函数
  const deviceTypeMap = {
    'VMWARE_VIRTUAL_MACHINE': 'VMware虚拟机',
    'HUAWEI_ATLAS': '华为升腾设备',
    'OTHER': '其他设备',
    'UNKNOWN': '未知设备',
    'vmware': 'VMware虚拟机',
    'huawei': '华为升腾设备',
    'quick': '快速扫描设备',
    'common': '常见网段设备',
    'full': '完整扫描设备'
  };

  // 模拟getDeviceTypeDisplay函数
  const getDeviceTypeDisplay = (deviceType) => {
    if (!deviceType) return '未知';
    return deviceTypeMap[deviceType.toUpperCase()] || deviceTypeMap[deviceType] || '未知';
  };

  describe('Device Type Mapping', function() {
    it('should map VMWARE_VIRTUAL_MACHINE to VMware虚拟机', function() {
      const result = getDeviceTypeDisplay('VMWARE_VIRTUAL_MACHINE');
      assert.strictEqual(result, 'VMware虚拟机');
    });

    it('should map HUAWEI_ATLAS to 华为升腾设备', function() {
      const result = getDeviceTypeDisplay('HUAWEI_ATLAS');
      assert.strictEqual(result, '华为升腾设备');
    });

    it('should map OTHER to 其他设备', function() {
      const result = getDeviceTypeDisplay('OTHER');
      assert.strictEqual(result, '其他设备');
    });

    it('should map UNKNOWN to 未知设备', function() {
      const result = getDeviceTypeDisplay('UNKNOWN');
      assert.strictEqual(result, '未知设备');
    });

    it('should map lowercase device types correctly', function() {
      const result1 = getDeviceTypeDisplay('vmware');
      const result2 = getDeviceTypeDisplay('huawei');
      assert.strictEqual(result1, 'VMware虚拟机');
      assert.strictEqual(result2, '华为升腾设备');
    });

    it('should map scan modes correctly', function() {
      const result1 = getDeviceTypeDisplay('quick');
      const result2 = getDeviceTypeDisplay('common');
      const result3 = getDeviceTypeDisplay('full');
      assert.strictEqual(result1, '快速扫描设备');
      assert.strictEqual(result2, '常见网段设备');
      assert.strictEqual(result3, '完整扫描设备');
    });

    it('should handle unknown device types', function() {
      const result = getDeviceTypeDisplay('NON_EXISTENT_TYPE');
      assert.strictEqual(result, '未知');
    });

    it('should handle empty device type', function() {
      const result = getDeviceTypeDisplay('');
      assert.strictEqual(result, '未知');
    });

    it('should handle null device type', function() {
      const result = getDeviceTypeDisplay(null);
      assert.strictEqual(result, '未知');
    });

    it('should handle undefined device type', function() {
      const result = getDeviceTypeDisplay(undefined);
      assert.strictEqual(result, '未知');
    });
  });

  describe('Device Type Display Integration', function() {
    it('should generate correct device objects with display names', function() {
      // 模拟设备数据
      const deviceData = {
        ip_address: '192.168.1.100',
        hostname: 'test-device',
        mac_address: '00:11:22:33:44:55',
        is_online: true,
        is_connected: false,
        device_type: 'VMWARE_VIRTUAL_MACHINE'
      };

      // 模拟设备发现处理
      const processedDevice = {
        ...deviceData,
        device_type_display: getDeviceTypeDisplay(deviceData.device_type)
      };

      assert.strictEqual(processedDevice.device_type_display, 'VMware虚拟机');
      assert.strictEqual(processedDevice.ip_address, deviceData.ip_address);
    });

    it('should handle devices without device_type', function() {
      // 模拟没有device_type的设备数据
      const deviceData = {
        ip_address: '192.168.1.100',
        hostname: 'test-device',
        mac_address: '00:11:22:33:44:55',
        is_online: true,
        is_connected: false
        // 没有device_type字段
      };

      // 模拟设备发现处理
      const processedDevice = {
        ...deviceData,
        device_type_display: getDeviceTypeDisplay(deviceData.device_type)
      };

      assert.strictEqual(processedDevice.device_type_display, '未知');
    });
  });
});
