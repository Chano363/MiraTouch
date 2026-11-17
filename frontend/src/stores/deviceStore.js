import { defineStore } from 'pinia'

export const useDeviceStore = defineStore('device', {
  state: () => ({
    connectedDevices: [],
    discoveredDevices: [],
    selectedDevices: [],
    monitoringDevices: [],
    historyDevices: [],
    sshConfig: {
      username: '',
      port: 22,
      timeout: 30
    },
    scanMode: 'vmware',
    scanning: false,
    scanProgress: null
  }),

  getters: {
    onlineCount: (state) => {
      return state.connectedDevices.filter(device => device.is_online).length
    },
    connectedCount: (state) => {
      return state.connectedDevices.filter(device => device.is_connected).length
    },
    allMonitoring: (state) => {
      return state.connectedDevices.length > 0 && 
             state.connectedDevices.every(device => state.monitoringDevices.includes(device.ip_address))
    }
  },

  actions: {
    setConnectedDevices(devices) {
      this.connectedDevices = devices
    },

    setDiscoveredDevices(devices) {
      this.discoveredDevices = devices
    },

    setSelectedDevices(devices) {
      this.selectedDevices = devices
    },

    toggleDeviceSelection(ip) {
      const index = this.selectedDevices.indexOf(ip)
      if (index > -1) {
        this.selectedDevices.splice(index, 1)
      } else {
        this.selectedDevices.push(ip)
      }
    },

    toggleDeviceMonitoring(ip) {
      const index = this.monitoringDevices.indexOf(ip)
      if (index > -1) {
        this.monitoringDevices.splice(index, 1)
      } else {
        this.monitoringDevices.push(ip)
      }
    },

    addMonitoringDevice(ip) {
      if (!this.monitoringDevices.includes(ip)) {
        this.monitoringDevices.push(ip)
      }
    },

    removeMonitoringDevice(ip) {
      const index = this.monitoringDevices.indexOf(ip)
      if (index > -1) {
        this.monitoringDevices.splice(index, 1)
      }
    },

    startDeviceMonitoring(ip) {
      if (!this.monitoringDevices.includes(ip)) {
        this.monitoringDevices.push(ip)
      }
    },

    stopDeviceMonitoring(ip) {
      const index = this.monitoringDevices.indexOf(ip)
      if (index > -1) {
        this.monitoringDevices.splice(index, 1)
      }
    },

    setSSHConfig(config) {
      this.sshConfig = { ...this.sshConfig, ...config }
    },

    setScanMode(mode) {
      this.scanMode = mode
    },

    setScanning(status) {
      this.scanning = status
    },

    setScanProgress(progress) {
      this.scanProgress = progress
    },

    updateDevicePerformance(ip, performanceData) {
      const deviceIndex = this.connectedDevices.findIndex(d => d.ip_address === ip)
      if (deviceIndex !== -1) {
        this.connectedDevices[deviceIndex].performance = {
          ...this.connectedDevices[deviceIndex].performance,
          ...performanceData
        }
      }
    },

    addHistoryDevice(device) {
      // 检查是否已存在该设备
      const existingIndex = this.historyDevices.findIndex(h => h.ip === device.ip)
      if (existingIndex !== -1) {
        // 如果存在，则更新最后发现时间
        this.historyDevices[existingIndex].lastSeen = new Date().toISOString()
      } else {
        // 如果不存在，则添加新设备
        this.historyDevices.unshift({
          ...device,
          lastSeen: new Date().toISOString()
        })
        
        // 限制历史设备数量
        if (this.historyDevices.length > 50) {
          this.historyDevices = this.historyDevices.slice(0, 50)
        }
      }
    },

    clearHistory() {
      this.historyDevices = []
    },

    updateDeviceConnectionStatus(ip, isConnected) {
      // 更新connectedDevices中的设备状态
      const connectedDeviceIndex = this.connectedDevices.findIndex(device => device.ip_address === ip);
      if (connectedDeviceIndex !== -1) {
        this.connectedDevices[connectedDeviceIndex].is_connected = isConnected;
        
        // 如果设备断开连接，将其从selectedDevices中移除
        if (!isConnected) {
          const selectedIndex = this.selectedDevices.indexOf(ip);
          if (selectedIndex !== -1) {
            this.selectedDevices.splice(selectedIndex, 1);
          }
        }
      }
      
      // 更新discoveredDevices中的设备状态
      const discoveredDeviceIndex = this.discoveredDevices.findIndex(device => device.ip_address === ip);
      if (discoveredDeviceIndex !== -1) {
        this.discoveredDevices[discoveredDeviceIndex].is_connected = isConnected;
      }
      
      // 如果设备连接成功，将其添加到connectedDevices（如果尚不存在）
      if (isConnected && connectedDeviceIndex === -1) {
        // 查找设备信息
        const deviceInfo = this.discoveredDevices.find(device => device.ip_address === ip);
        if (deviceInfo) {
          this.connectedDevices.push({...deviceInfo, is_connected: true});
        }
      }
      
      // 如果设备断开连接，从connectedDevices中移除
      if (!isConnected && connectedDeviceIndex !== -1) {
        this.connectedDevices.splice(connectedDeviceIndex, 1);
      }
    },

    // 从本地存储加载数据
    loadFromLocalStorage() {
      try {
        const savedSSHConfig = localStorage.getItem('sshConfig')
        if (savedSSHConfig) {
          this.sshConfig = { ...this.sshConfig, ...JSON.parse(savedSSHConfig) }
        }

        const savedHistory = localStorage.getItem('historyDevices')
        if (savedHistory) {
          this.historyDevices = JSON.parse(savedHistory)
        }
      } catch (error) {
        console.error('从本地存储加载数据失败:', error)
      }
    },

    // 保存到本地存储
    saveToLocalStorage() {
      try {
        localStorage.setItem('sshConfig', JSON.stringify(this.sshConfig))
        localStorage.setItem('historyDevices', JSON.stringify(this.historyDevices))
      } catch (error) {
        console.error('保存到本地存储失败:', error)
      }
    }
  }
})