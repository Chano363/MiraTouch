import { ref } from 'vue'
import axios from 'axios'

export function useDeviceManager(apiUrl, addLog) {
  const devices = ref([])
  const scanning = ref(false)
  const selectedDevice = ref(null)
  const sshUsername = ref(localStorage.getItem('sshUsername') || 'root')
  const scanProgress = ref(null)
  const historyDevices = ref(JSON.parse(localStorage.getItem('historyDevices') || '[]'))

  // 设备类型到中文的映射
  const deviceTypeMap = {
    'VMWARE_VIRTUAL_MACHINE': 'VMware虚拟机',
    'HUAWEI_ATLAS': '华为升腾设备',
    'OTHER': '其他设备',
    'UNKNOWN': '未知设备'
  }

  const checkServerConnection = async () => {
    try {
      await axios.get(`${apiUrl}/api/devices/scan`)
      return true
    } catch (error) {
      return false
    }
  }

  const scanDevices = async (mode = 'vmware') => {
    // 只在开始时设置扫描状态为true
    scanning.value = true
    scanProgress.value = null

    const modeNames = {
      'vmware': 'VMware虚拟机',
      'huawei': '华为设备',
      'quick': '快速扫描',
      'common': '常见网段',
      'full': '完整扫描'
    }

    addLog(`开始${modeNames[mode] || 'VMware虚拟机'}扫描...`, 'info')

    try {
      const response = await axios.post(`${apiUrl}/api/devices/scan`, {
        mode: mode
      })

      // 处理响应数据
      devices.value = response.data.devices || []

      // 添加设备类型显示名称
      devices.value = devices.value.map(device => ({
        ...device,
        device_type_display: deviceTypeMap[device.device_type] || '未知设备'
      }))

      addLog(`发现 ${devices.value.length} 个设备`, 'success')

      // 注意：这里不再自动将扫描结果添加到历史记录中

    } catch (error) {
      addLog('扫描失败: ' + error.message, 'error')
      throw error // 重新抛出错误以便上层处理
    } finally {
      // 确保在所有情况下都重置扫描状态
      scanning.value = false
      scanProgress.value = null
    }
  }

  const connectToDevice = async (ipAddress, username = null, password = null) => {
    // 1. 如果没有提供用户名，弹出对话框询问用户名
    if (username === null) {
      // 提供默认值：优先使用 sshUsername.value，其次使用 'chano'
      const defaultUsername = sshUsername.value || 'chano';
      const inputUsername = window.prompt(`请输入设备 ${ipAddress} 的SSH用户名:`, defaultUsername);
      if (inputUsername === null || inputUsername.trim() === '') {
        addLog('连接取消: 未输入用户名', 'info');
        return false;
      }
      username = inputUsername;
    }

    // 2. 如果没有提供密码，弹出对话框询问密码
    if (password === null) {
      const inputPassword = window.prompt(`请输入设备 ${ipAddress} 的SSH密码:`, '');
      if (inputPassword === null || inputPassword.trim() === '') {
        addLog('连接取消: 未输入密码', 'info');
        return false;
      }
      password = inputPassword;
    }

    // 3. 使用用户输入的用户名和密码
    const finalUsername = username; // 直接使用用户输入的用户名
    const finalPassword = password;

    addLog(`正在连接到设备 ${ipAddress} (用户: ${finalUsername})...`, 'info');

    try {
      const response = await axios.post(`${apiUrl}/api/devices/connect`, {
        ip_address: ipAddress,
        username: finalUsername,
        password: finalPassword
      });

      if (response.data.success) {
        addLog(`设备 ${ipAddress} 连接成功`, 'success');

        // 更新设备状态
        const device = devices.value.find(d => d.ip_address === ipAddress);
        if (device) {
          device.is_connected = true;
        }

        selectedDevice.value = ipAddress;

        // 保存用户名到localStorage
        localStorage.setItem('sshUsername', finalUsername);

        // 只有连接成功才添加到历史记录
        if (device) {
          const newHistory = [...historyDevices.value];
          const existingIndex = newHistory.findIndex(d => d.ip === device.ip_address);

          const historyDevice = {
            ip: device.ip_address,
            hostname: device.hostname,
            mac: device.mac_address,
            type: device.device_type,
            type_display: deviceTypeMap[device.device_type] || '未知设备',
            lastSeen: new Date().toISOString()
          };

          if (existingIndex === -1) {
            // 新设备
            newHistory.push(historyDevice);
          } else {
            // 更新现有设备
            newHistory[existingIndex] = historyDevice;
          }

          historyDevices.value = newHistory.slice(-20);
          localStorage.setItem('historyDevices', JSON.stringify(historyDevices.value));
        }

        return true;
      } else {
        addLog(`设备 ${ipAddress} 连接失败`, 'error');
        return false;
      }
    } catch (error) {
      addLog('连接失败: ' + error.message, 'error');
      return false;
    }
  };

  const disconnectDevice = async (ipAddress) => {
    addLog(`断开设备 ${ipAddress} 的连接`, 'info')

    try {
      await axios.post(`${apiUrl}/api/devices/disconnect`, {
        ip_address: ipAddress
      })

      const device = devices.value.find(d => d.ip_address === ipAddress)
      if (device) {
        device.is_connected = false
      }

      if (selectedDevice.value === ipAddress) {
        selectedDevice.value = null
      }

      addLog(`设备 ${ipAddress} 已断开`, 'success')
    } catch (error) {
      addLog('断开连接失败: ' + error.message, 'error')
    }
  }

  return {
    devices,
    scanning,
    selectedDevice,
    sshUsername,
    scanProgress,
    historyDevices,
    deviceTypeMap, // 导出这个映射对象
    checkServerConnection,
    scanDevices,
    connectToDevice,
    disconnectDevice
  }
}