<template>
  <div class="multi-device-manager">
    <!-- 全局配置区域 -->
    <el-card class="global-config-panel">
      <template #header>
        <div class="config-header">
          <div class="config-title">
            <el-icon><Setting /></el-icon>
            <span>全局配置</span>
          </div>
        </div>
      </template>
      <div class="global-config-content">
        <el-form :model="globalConfig" label-width="120px">
          <el-form-item label="模型项目根路径">
            <el-input 
              v-model="globalConfig.modelRootPath" 
              placeholder="/home/chano/Github/dynamic_gestures"
              clearable
              size="large"
              style="width: 400px"
            />
            <div class="config-info">所有设备操作的默认模型项目根目录</div>
          </el-form-item>
        </el-form>
      </div>
    </el-card>

    <!-- 网络扫描区域 -->
    <el-card class="scan-panel">
      <template #header>
        <div class="scan-header">
          <div class="scan-header-content">
            <div class="scan-header-info">
              <div class="scan-icon-wrapper">
                <el-icon class="scan-icon"><Search /></el-icon>
                <h3 class="scan-title">网络扫描</h3>
              </div>
              <div class="scan-description">
                <span class="scan-desc-text">扫描局域网中的可用设备</span>
              </div>
            </div>
            <div class="scan-controls">
              <el-select 
                v-model="scanMode" 
                placeholder="扫描模式" 
                size="large" 
                class="scan-mode-select"
              >
                <el-option label="VMware虚拟机" value="vmware" />
                <el-option label="华为设备" value="huawei" />
                <el-option label="快速扫描" value="quick" />
                <el-option label="常见网段" value="common" />
                <el-option label="完整扫描" value="full" />
              </el-select>
              <el-button 
                v-if="!scanning"
                type="primary" 
                :icon="Search" 
                @click="startScan"
                size="large"
                class="scan-action-btn start-scan-btn"
              >
                开始扫描
              </el-button>
              <el-button 
                v-else
                type="danger" 
                :icon="VideoPause" 
                @click="stopScan"
                size="large"
                class="scan-action-btn stop-scan-btn"
              >
                停止扫描
              </el-button>
              <!-- 调试按钮：强制刷新设备列表 -->
              <el-button 
                type="success" 
                @click="refreshDeviceList"
                size="large"
                class="scan-action-btn refresh-btn"
              >
                刷新设备
              </el-button>
            </div>
          </div>
          <div v-if="scanProgress" class="scan-progress-container">
            <div class="progress-content">
              <el-progress 
                :percentage="scanProgress.percentage" 
                :status="scanProgress.status" 
                :text-inside="true" 
                :stroke-width="28"
                :duration="5"
                class="custom-progress"
              />
              <span class="progress-text">{{ scanProgress.text }}</span>
            </div>
          </div>
        </div>
      </template>
    </el-card>

    <!-- 发现的设备列表 -->
    <el-card class="discovered-devices-panel">
      <template #header>
        <div class="panel-header">
          <div class="header-icon-title">
            <el-icon style="font-size: 1.2em; margin-right: 8px;"><Monitor /></el-icon>
            <span class="header-title">发现设备 ({{ discoveredDevices.length }})</span>
          </div>
        </div>
      </template>
      
      <div v-if="discoveredDevices.length > 0" class="discovered-devices">
        <el-table 
          :data="discoveredDevices" 
          stripe 
          style="width: 100%"
          :row-class-name="getDeviceRowClass"
        >
        <!-- 添加调试信息，显示当前discoveredDevices长度 -->
        <template #empty>
          <div style="padding: 20px; text-align: center; color: #909399;">
            <p>暂无发现设备</p>
            <p style="font-size: 12px;">当前设备列表长度: {{ discoveredDevices.length }}</p>
          </div>
        </template>
          <el-table-column prop="hostname" label="设备名称" min-width="180">
            <template #default="{ row }">
              <div class="device-name-cell">
                <el-icon><Monitor /></el-icon>
                <span>{{ row.hostname || row.ip_address }}</span>
              </div>
            </template>
          </el-table-column>
          
          <el-table-column prop="ip_address" label="IP地址" min-width="120" />
          
          <el-table-column prop="mac_address" label="MAC地址" min-width="160" />
          
          <el-table-column label="设备类型" min-width="100">
            <template #default="{ row }">
              <el-tag v-if="row.device_type_display" size="small">
                {{ row.device_type_display }}
              </el-tag>
              <span v-else>未知</span>
            </template>
          </el-table-column>
          
          <el-table-column label="状态" min-width="80">
            <template #default="{ row }">
              <el-tag :type="row.is_online ? 'success' : 'info'" size="small">
                {{ row.is_online ? '在线' : '离线' }}
              </el-tag>
            </template>
          </el-table-column>
          
          <el-table-column label="操作" min-width="120">
            <template #default="{ row }">
              <el-button 
                type="primary" 
                size="small" 
                :disabled="row.is_connected"
                @click="showConnectDeviceDialog(row.ip_address)"
                :icon="Connection"
                :loading="connectingDevice === row.ip_address"
              >
                连接
              </el-button>
            </template>
          </el-table-column>
        </el-table>
      </div>
      
      <div v-else class="no-discovered-devices">
        <el-empty description="暂无发现设备，请先开始网络扫描" :image-size="100" />
      </div>
    </el-card>

    <!-- 已连接设备列表 - 仅在有发现设备时显示 -->
    <DeviceList 
      v-if="discoveredDevices.length > 0"
      :connected-devices="connectedDevices"
      :selected-devices="selectedDevices"
      :monitoring-devices="monitoringDevices"
      @toggle-device-selection="onDeviceSelect"
      @disconnect-device="onDeviceDisconnect"
      @toggle-performance="onPerformanceToggle"
      @open-parameter-config="onOpenParameterConfig"
      @check-model-status="onCheckModelStatus"
      @open-performance-chart="onOpenPerformanceChart"
      @open-performance-popup="onOpenPerformancePopup"
      @open-config-manager="onOpenConfigManager"
    />

    <!-- 历史设备面板 - 总是显示 -->
    <HistoryDevicesPanel
      :history-devices="historyDevices"
      :show-history="true"
      :show-detailed-history="showDetailedHistory"
      @toggle-history-view="toggleHistoryView"
      @clear-history="clearHistory"
      @connect-to-device="showConnectDeviceDialog"
    />

    <!-- 设备连接对话框 -->
    <el-dialog v-model="showConnectDialog" title="设备连接" width="500px" :before-close="handleConnectCancel">
      <el-form label-position="right" label-width="100px">
        <el-form-item label="IP地址">
          <el-input v-model="connectDeviceIp" disabled />
        </el-form-item>
        <el-form-item label="用户名">
          <el-input v-model="connectCredentials.username" placeholder="请输入用户名" />
        </el-form-item>
        <el-form-item label="密码">
          <el-input 
            v-model="connectCredentials.password" 
            :type="connectCredentials.showPassword ? 'text' : 'password'" 
            placeholder="请输入密码" 
            show-password
          />
        </el-form-item>
        <el-form-item label="SSH端口">
          <el-input-number 
            v-model="connectCredentials.port" 
            :min="1" 
            :max="65535" 
            style="width: 100%" 
          />
        </el-form-item>
        <el-form-item label="连接超时">
          <el-input-number 
            v-model="connectCredentials.timeout" 
            :min="1" 
            :max="300" 
            style="width: 100%" 
          />
        </el-form-item>
        <!-- 连接状态显示 -->
        <el-form-item v-if="connectionStatus" label="连接状态">
          <el-alert
            :title="connectionStatus"
            :type="connectionStatus.includes('成功') ? 'success' : connectionStatus.includes('失败') || connectionStatus.includes('错误') || connectionStatus.includes('超时') ? 'error' : 'info'"
            :closable="false"
            show-icon
          />
        </el-form-item>
      </el-form>
      <template #footer>
        <span class="dialog-footer">
          <el-button @click="handleConnectCancel">取消</el-button>
          <el-button type="primary" @click="handleConnectConfirm" :loading="connectingDevice !== null" :disabled="connectingDevice !== null">连接</el-button>
        </span>
      </template>
    </el-dialog>

    <!-- 性能图表模态框 -->
    <PerformanceChartModal
      v-model:show="showPerformanceChart"
      :devices="connectedDevices"
      :selected-device-ip="selectedChartDeviceIp"
      @close="onClosePerformanceChart"
    />
    
    <!-- 性能监测管理器 - 处理性能监测复杂逻辑 -->
    <PerformanceMonitorManager ref="performanceMonitorManagerRef" />
    
    <!-- 性能图表弹窗 -->
    <PerformanceChartPopup
      v-model="showPerformancePopup"
      :device-ip="selectedPerformanceDeviceIp"
      @close="onClosePerformancePopup"
    />
    
    <!-- 配置管理弹窗 -->
    <ConfigManagerPopup
      v-model:show="showConfigManager"
      :device-ip="selectedConfigDeviceIp"
      :is-connected="isDeviceConnected(selectedConfigDeviceIp)"
      @close="onCloseConfigManager"
      @save="onConfigSave"
    />
    
    <!-- 参数配置模态框 -->
    <ParameterConfigModal
      v-model:show="showParameterConfig"
      :device-ip="selectedParameterDeviceIp"
      :is-monitoring="false"
      :model-path="''"
      :inference-engine="'tensorrt'"
      :batch-size="1"
      :confidence-threshold="0.5"
      @close="onCloseParameterConfig"
      @send-config="onSendParameterConfig"
    />
  </div>
</template>

<script setup>
import { ref, computed, onMounted } from 'vue'
import { Search, Monitor, Connection, RefreshRight, VideoPause } from '@element-plus/icons-vue'
import { ElMessage } from 'element-plus'
import DeviceList from './DeviceList.vue'
import HistoryDevicesPanel from './HistoryDevicesPanel.vue'
import PerformanceChartModal from './PerformanceChartModal.vue'
import PerformanceChartPopup from './PerformanceChartPopup.vue'
import PerformanceMonitorManager from './PerformanceMonitorManager.vue'
import ConfigManagerPopup from './ConfigManagerPopup.vue'
import { useDeviceStore } from '../stores/deviceStore'
import { useWebSocket } from '../composables/useWebSocket'
import { createLogger } from '../utils/logger'

// 创建日志实例
const logger = createLogger('MultiDeviceManager')

// 使用Pinia Store
const deviceStore = useDeviceStore()

// 响应式数据
const selectedDevices = computed({
  get: () => deviceStore.selectedDevices,
  set: (value) => deviceStore.setSelectedDevices(value)
})
const monitoringDevices = computed(() => deviceStore.monitoringDevices)

// 扫描相关
const scanMode = ref('vmware')
const scanning = ref(false)
const discoveredDevices = ref([])
const scanProgress = ref(null)
let scanController = null  // 用于控制扫描请求

// WebSocket实例
const wsInstance = useWebSocket()
const { registerMessageHandler, unregisterMessageHandler, connectWebSocket } = wsInstance

// 注册WebSocket消息处理器
const scanProgressHandler = (data) => {
  logger.debug('scanProgressHandler被调用，收到数据:', data);
  if (data && data.type === 'scan_progress') {
    logger.debug('处理scan_progress消息:', data);
    // 更新扫描进度
    scanProgress.value = {
      percentage: Math.round((data.current / data.total) * 100),
      status: data.status === 'completed' ? 'success' : 'warning',
      text: `扫描进度: ${data.current}/${data.total}, 发现设备: ${data.device_count}`
    }
    
    // 检查是否扫描已完成，如果是，立即获取最新的设备列表
    if (data.status === 'completed') {
      logger.info('扫描完成，正在获取最新设备列表...');
      // 扫描完成后，立即从API获取完整设备列表
      fetch('/api/devices/list', {
        method: 'GET',
        headers: { 'Content-Type': 'application/json' }
      })
        .then(response => {
          logger.debug('API响应状态:', response.status);
          return response.json();
        })
        .then(apiData => {
          logger.debug('从API获取的数据:', apiData);
          if (apiData.success && apiData.devices) {
            discoveredDevices.value = [...apiData.devices];
            logger.info('通过WebSocket完成消息，从API获取到设备列表', { count: apiData.devices.length });
            
            // 同时更新store中的发现设备列表
            deviceStore.setDiscoveredDevices([...apiData.devices]);
          } else {
            logger.warn('API返回的数据不符合预期:', apiData);
          }
        })
        .catch(error => {
          logger.error('通过WebSocket获取设备列表时出错:', error);
        });
    } else {
      logger.debug('收到非完成状态消息:', data.status);
    }
  } else {
    logger.debug('收到非scan_progress消息或数据格式不正确');
  }
}

// 在组件挂载时注册处理器
logger.info('准备注册WebSocket消息处理器...');
if (registerMessageHandler) {
  registerMessageHandler('scan_progress', scanProgressHandler)
  logger.info('已注册scan_progress消息处理器');
} else {
  logger.error('registerMessageHandler未定义');
}

// 设备列表更新定时器
let deviceUpdateTimer = null;

// 统一的设备列表更新函数
const updateDeviceList = (devices) => {
  if (devices && Array.isArray(devices)) {
    // 更新发现的设备列表
    discoveredDevices.value = [...devices];
    logger.info('统一更新设备列表', { count: devices.length });
    
    // 同时将这些设备添加到历史记录
    devices.forEach(device => {
      try {
        deviceStore.addHistoryDevice(device);
      } catch (error) {
        logger.error('添加设备到历史记录时出错:', error);
      }
    });
    
    // 更新store中的发现设备列表
    deviceStore.setDiscoveredDevices([...devices]);
    
    // 强制刷新设备列表以确保UI更新
    deviceStore.loadFromLocalStorage();
    
    // 确保discoveredDevices响应式更新
    discoveredDevices.value = [...devices];
    
    // 确保已连接设备列表也更新
    const connectedDevices = devices.filter(device => device.is_connected);
    deviceStore.setConnectedDevices([...connectedDevices]);
    logger.debug('已连接设备数量:', connectedDevices.length);
  }
};

// 防抖更新设备列表
const debouncedUpdateDeviceList = (devices) => {
  // 清除之前的定时器
  if (deviceUpdateTimer) {
    clearTimeout(deviceUpdateTimer);
    deviceUpdateTimer = null;
  }
  
  // 添加防抖处理，避免短时间内多次更新
  deviceUpdateTimer = setTimeout(() => {
    updateDeviceList(devices);
    deviceUpdateTimer = null;
  }, 100); // 100ms防抖延迟
};

// 设备列表更新处理器 - 监听设备列表变化
const deviceListUpdateHandler = (data) => {
  logger.debug('收到设备列表更新消息:', data);
  if (data && data.type === 'device_list_update') {
    // 如果收到设备列表更新消息，更新本地设备列表
    if (data.devices && Array.isArray(data.devices)) {
      logger.info('处理设备列表更新消息', { deviceCount: data.devices.length });
      debouncedUpdateDeviceList(data.devices);
    }
  }
  // 检查是否是扫描完成消息
  else if (data && data.type === 'scan_progress' && data.status === 'completed') {
    // 扫描完成，可能包含设备列表
    if (data.devices && Array.isArray(data.devices)) {
      logger.info('处理扫描完成消息', { deviceCount: data.devices.length });
      debouncedUpdateDeviceList(data.devices);
    }
  }
  // 检查是否是设备连接/断开消息
  else if (data && (data.type === 'device_connect' || data.type === 'device_disconnect')) {
    logger.info('收到设备连接状态变化消息', { type: data.type, ip: data.ip_address });
    // 重新获取设备列表以确保状态同步
    fetch('/api/devices/list', {
      method: 'GET',
      headers: { 'Content-Type': 'application/json' }
    })
      .then(response => response.json())
      .then(apiData => {
        if (apiData.success && apiData.devices) {
          logger.debug('通过设备连接状态变化消息获取设备列表成功', { deviceCount: apiData.devices.length });
          debouncedUpdateDeviceList(apiData.devices);
        }
      })
      .catch(error => {
        logger.error('通过设备连接状态变化消息获取设备列表时出错:', error);
      });
  }
};

// 设备类型映射表
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

// 获取设备类型显示名称
const getDeviceTypeDisplay = (deviceType) => {
  if (!deviceType) return '未知';
  return deviceTypeMap[deviceType.toUpperCase()] || deviceTypeMap[deviceType] || '未知';
};

// 设备发现处理器 - 处理单个设备发现消息
  const deviceDiscoveryHandler = (data) => {
    logger.debug('收到设备发现消息:', data);
    
    // 处理包含设备列表的消息
    if (data && data.devices && Array.isArray(data.devices)) {
      logger.info('处理包含设备列表的消息，设备数量:', data.devices.length);
      
      // 批量处理设备列表
      data.devices.forEach(deviceData => {
        if (deviceData && deviceData.ip_address) {
          const newDevice = {
            ip_address: deviceData.ip_address,
            hostname: deviceData.hostname || deviceData.ip_address,
            mac_address: deviceData.mac_address || '',
            is_online: deviceData.is_online !== false,
            is_connected: deviceData.is_connected || false,
            last_seen: deviceData.last_seen || new Date().toISOString(),
            device_type_display: getDeviceTypeDisplay(deviceData.device_type_display || deviceData.device_type)
          };
          
          // 检查设备是否已存在于列表中
          const existingIndex = discoveredDevices.value.findIndex(
            d => d.ip_address === newDevice.ip_address
          );
          
          if (existingIndex === -1) {
            // 如果设备不存在，添加到列表
            discoveredDevices.value.push(newDevice);
            logger.info('添加新发现的设备', { ip: newDevice.ip_address, hostname: newDevice.hostname, type: newDevice.device_type_display });
            
            // 将发现的设备添加到历史记录
            deviceStore.addHistoryDevice(newDevice);
          } else {
            // 如果设备已存在，更新设备信息
            discoveredDevices.value[existingIndex] = { ...discoveredDevices.value[existingIndex], ...newDevice };
            logger.info('更新已存在的设备', { ip: newDevice.ip_address, hostname: newDevice.hostname, type: newDevice.device_type_display });
          }
        }
      });
      
      // 同步更新store中的发现设备列表
      deviceStore.setDiscoveredDevices([...discoveredDevices.value]);
      return;
    }
    
    // 处理单个设备发现消息
    if (data && (data.type === 'device_found' || data.type === 'scan_result' || data.status === 'device_found' || data.device)) {
      // 处理新发现的设备
      if (data.device || data.ip_address) {
        const deviceData = data.device || data;
        const newDevice = {
          ip_address: deviceData.ip_address,
          hostname: deviceData.hostname || deviceData.ip_address,
          mac_address: deviceData.mac_address || '',
          is_online: true,
          is_connected: false,
          last_seen: deviceData.last_seen || new Date().toISOString(),
          device_type_display: getDeviceTypeDisplay(deviceData.device_type_display || deviceData.device_type)
        };
        
        // 检查设备是否已存在于列表中
        const existingIndex = discoveredDevices.value.findIndex(
          d => d.ip_address === newDevice.ip_address
        );
        
        if (existingIndex === -1) {
          // 如果设备不存在，添加到列表
          discoveredDevices.value.push(newDevice);
          logger.info('添加新发现的设备', { ip: newDevice.ip_address, hostname: newDevice.hostname, type: newDevice.device_type_display });
          
          // 将发现的设备添加到历史记录
          deviceStore.addHistoryDevice(newDevice);
        } else {
          // 如果设备已存在，更新设备信息
          discoveredDevices.value[existingIndex] = { ...discoveredDevices.value[existingIndex], ...newDevice };
          logger.info('更新已存在的设备', { ip: newDevice.ip_address, hostname: newDevice.hostname, type: newDevice.device_type_display });
        }
        
        // 同步更新store中的发现设备列表
        deviceStore.setDiscoveredDevices([...discoveredDevices.value]);
      }
    }
  };

// 注册设备列表更新处理器
if (registerMessageHandler) {
  registerMessageHandler('device_list_update', deviceListUpdateHandler);
  registerMessageHandler('device_discovery', deviceDiscoveryHandler);
}

// 历史设备相关
const showHistoryDevices = ref(true)
const showDetailedHistory = ref(false)
const historyDevices = computed(() => deviceStore.historyDevices)

// 性能监测管理器
const performanceMonitorManagerRef = ref(null)

// 性能图表弹窗相关
const showPerformancePopup = ref(false)
const selectedPerformanceDeviceIp = ref('')

// 性能图表相关
const showPerformanceChart = ref(false)
const selectedChartDeviceIp = ref('')

// 配置管理弹窗相关
const showConfigManager = ref(false)
const selectedConfigDeviceIp = ref('')

// 全局配置
const globalConfig = ref({
  modelRootPath: '/home/chano/Github/dynamic_gestures'
})



// 设备连接对话框相关
const showConnectDialog = ref(false)
const connectDeviceIp = ref('')
const connecting = ref(false)
const connectCredentials = ref({
  username: '',
  password: '',
  port: 22,
  timeout: 30
})

// 方法
const onDeviceSelect = (ip) => {
  deviceStore.toggleDeviceSelection(ip)
}

// 扫描设备
let isScanning = false;
let scanRequestTimer = null;
const startScan = async () => {
  if (isScanning) {
    console.log('扫描已在进行中，忽略重复请求');
    return; // 防止重复启动扫描
  }
  
  // 清除之前的扫描请求定时器
  if (scanRequestTimer) {
    clearTimeout(scanRequestTimer);
    scanRequestTimer = null;
  }
  
  // 添加防抖处理，避免短时间内多次触发扫描
  scanRequestTimer = setTimeout(async () => {
    const scanStartTime = Date.now();
    console.log('设备扫描开始时间:', new Date(scanStartTime).toLocaleTimeString());
    console.log('扫描模式:', scanMode.value);
    
    isScanning = true;
    scanning.value = true;
    scanProgress.value = {
      percentage: 0,
      status: 'warning',
      text: '准备开始扫描...'
    };
    scanController = new AbortController(); // 创建新的控制器
    
    // 显示扫描开始信息
    ElMessage({
      message: '开始扫描设备...',
      type: 'info',
      duration: 2000
    });
    
    // 记录日志
    logger.info('设备扫描开始', { mode: scanMode.value });
    
    // 清空之前的扫描进度和设备列表
    discoveredDevices.value = [];
    deviceStore.setDiscoveredDevices([]);
    
    try {
      console.log('开始发送扫描请求到后端...');
      const response = await fetch('/api/devices/scan', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ mode: scanMode.value }),
        signal: scanController.signal  // 添加信号用于取消请求
      });
      
      console.log('扫描请求响应状态:', response.status);
      
      if (!response.ok) {
        throw new Error(`HTTP错误: ${response.status} ${response.statusText}`);
      }
      
      const result = await response.json();
      console.log('扫描请求响应数据:', result);
      
      if (result.success) {
        let processedDevices = [];
        
        // 扫描完成，获取最新的设备列表
        if (result.devices && Array.isArray(result.devices)) {
          // 处理设备列表，添加设备类型显示名称
          processedDevices = result.devices.map(device => ({
            ...device,
            device_type_display: getDeviceTypeDisplay(device.device_type)
          }));
          
          // 更新发现的设备列表
          discoveredDevices.value = [...processedDevices];
          console.log(`设备扫描完成，共发现 ${processedDevices.length} 个设备`);
          console.log('发现的设备:', processedDevices);
          
          // 显示扫描完成信息
          ElMessage({
            message: `扫描完成，共发现 ${processedDevices.length} 个设备`,
            type: 'success',
            duration: 3000
          });
          
          // 记录日志
          logger.info(`设备扫描完成，共发现 ${processedDevices.length} 个设备`);
          
          // 同时将这些设备添加到历史记录
          processedDevices.forEach(device => {
            try {
              deviceStore.addHistoryDevice(device);
            } catch (historyError) {
              console.error('添加设备到历史记录时出错:', historyError);
            }
          });
        } else {
          console.log(`设备扫描完成，共发现 0 个设备`);
          // 即使没有发现设备，也要清空列表
          discoveredDevices.value = [];
          
          // 显示扫描完成信息
          ElMessage({
            message: '扫描完成，未发现设备',
            type: 'info',
            duration: 3000
          });
          
          // 记录日志
          logger.info('设备扫描完成，未发现设备');
        }
        
        // 更新store中的发现设备列表
        deviceStore.setDiscoveredDevices([...processedDevices]);
        
        // 强制刷新设备列表以确保UI更新
        deviceStore.loadFromLocalStorage();
        
        // 确保discoveredDevices响应式更新
        discoveredDevices.value = processedDevices;
        console.log('扫描完成，已更新discoveredDevices，当前数量:', processedDevices.length);
        
        // 确保已连接设备列表也更新
        const connectedDevices = processedDevices.filter(device => device.is_connected);
        deviceStore.setConnectedDevices([...connectedDevices]);
        console.log('已连接设备数量:', connectedDevices.length);
      } else {
        const errorMessage = result.message || '扫描失败，未知错误';
        console.error('扫描失败:', errorMessage);
        // 显示错误信息
        ElMessage.error(`扫描失败: ${errorMessage}`);
        
        // 记录日志
        logger.error(`设备扫描失败: ${errorMessage}`);
      }
    } catch (error) {
      if (error.name === 'AbortError') {
        console.log('扫描被用户取消');
        ElMessage.info('扫描已取消');
        
        // 记录日志
        logger.info('设备扫描被用户取消');
      } else {
        console.error('扫描过程中发生错误:', error);
        ElMessage.error(`扫描过程中发生错误: ${error.message}`);
        
        // 记录日志
        logger.error('设备扫描过程中发生错误:', error);
      }
    } finally {
      const scanEndTime = Date.now();
      console.log('设备扫描结束时间:', new Date(scanEndTime).toLocaleTimeString());
      console.log('扫描总耗时:', scanEndTime - scanStartTime, 'ms');
      
      scanning.value = false;
      scanProgress.value = null;
      scanController = null;
      isScanning = false;
      scanRequestTimer = null;
      // 确保不会自动重复扫描
      console.log('扫描结束，已重置扫描状态');
    }
  }, 300); // 300ms防抖延迟
}

// 停止扫描
const stopScan = () => {
  if (scanController) {
    scanController.abort()  // 取消正在进行的请求
    scanController = null
  }
  scanning.value = false
  scanProgress.value = null
  logger.info('扫描已停止')
}

// 连接设备对话框相关
const connectingDevice = ref(null);
const connectionStatus = ref('');

// 显示连接设备对话框
const showConnectDeviceDialog = (ip) => {
  connectDeviceIp.value = ip;
  connectCredentials.value = {
    username: 'root',
    password: '',
    port: 22,
    timeout: 30
  };
  connectionStatus.value = '';
  showConnectDialog.value = true;
};

// 连接到设备
const connectToDevice = async (ip) => {
  connectingDevice.value = ip; // 显示加载状态
  connectionStatus.value = '正在连接...';
  
  try {
    // 获取默认SSH配置，带超时
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 10000); // 10秒超时
    
    const response = await fetch('/api/config/default_ssh_config', {
      signal: controller.signal
    })
    clearTimeout(timeoutId);
    
    if (!response.ok) {
      throw new Error(`获取SSH配置失败: ${response.status} ${response.statusText}`)
    }
    
    const sshConfig = await response.json()
    
    const credentials = {
      ip_address: ip,
      username: connectCredentials.value.username || sshConfig.ssh_config?.username || sshConfig.username || 'root',
      password: connectCredentials.value.password || sshConfig.ssh_config?.password || sshConfig.password || '',
      port: connectCredentials.value.port || sshConfig.ssh_config?.default_port || sshConfig.port || 22,
      timeout: connectCredentials.value.timeout || sshConfig.ssh_config?.timeout || sshConfig.timeout || 30
    }
    
    // 注意：这里的超时设置应小于后端SSH连接的超时时间，否则用户会等待较长时间才能得到反馈
    const connectController = new AbortController();
    // 设置比后端SSH连接超时稍短的超时时间，以便前端及时反馈
    const connectTimeoutId = setTimeout(() => connectController.abort(), (credentials.timeout * 1000) + 5000); // 超时时间 = SSH超时 + 5秒缓冲
    
    const connectResponse = await fetch('/api/devices/connect', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(credentials),
      signal: connectController.signal
    })
    clearTimeout(connectTimeoutId);
    
    const result = await connectResponse.json()
    
    if (result.success) {
      connectionStatus.value = '连接成功！';
      console.log(`设备 ${ip} 连接成功`)
      
      // 显示成功信息
      ElMessage({
        message: '设备连接成功！',
        type: 'success',
        duration: 3000
      })
      
      // 记录日志
      logger.info(`设备 ${ip} 连接成功`)
      
      // 更新设备连接状态
      discoveredDevices.value = discoveredDevices.value.map(device => 
        device.ip_address === ip ? { ...device, is_connected: true } : device
      )
      
      // 更新store中的设备连接状态
      deviceStore.updateDeviceConnectionStatus(ip, true);
      
      // 确保设备被添加到已连接设备列表
      const connectedDevices = deviceStore.connectedDevices;
      const existingIndex = connectedDevices.findIndex(device => device.ip_address === ip);
      
      if (existingIndex === -1) {
        // 如果设备不在已连接列表中，从发现列表获取设备信息并添加
        const discoveredDevice = discoveredDevices.value.find(device => device.ip_address === ip);
        if (discoveredDevice) {
          deviceStore.setConnectedDevices([...connectedDevices, {...discoveredDevice, is_connected: true}]);
        }
      } else {
        // 如果设备已在连接列表中，更新其状态
        const updatedConnectedDevices = [...connectedDevices];
        updatedConnectedDevices[existingIndex] = {...updatedConnectedDevices[existingIndex], is_connected: true};
        deviceStore.setConnectedDevices(updatedConnectedDevices);
      }
      
      // 刷新已连接设备列表
      setTimeout(() => {
        deviceStore.loadFromLocalStorage()
        
        // 更新选中的设备列表，确保连接的设备被正确选中
        if (!deviceStore.selectedDevices.includes(ip)) {
          deviceStore.selectedDevices.push(ip);
        }
      }, 500)
      
      // 关闭对话框
      setTimeout(() => {
        showConnectDialog.value = false;
      }, 1000); // 成功后稍等片刻再关闭对话框
    } else {
      connectionStatus.value = `连接失败: ${result.message}`;
      console.error(`设备 ${ip} 连接失败:`, result.message)
      
      // 显示错误信息
      ElMessage({
        message: `设备连接失败: ${result.message}`,
        type: 'error',
        duration: 3000
      })
      
      // 记录日志
      logger.error(`设备 ${ip} 连接失败:`, result.message)
    }
  } catch (error) {
    if (error.name === 'AbortError') {
      connectionStatus.value = '连接超时，请检查设备是否可达';
      console.error('请求超时:', error)
    } else {
      connectionStatus.value = `连接出错: ${error.message}`;
      console.error('连接设备时发生错误:', error)
    }
  } finally {
    setTimeout(() => {
      connectingDevice.value = null; // 隐藏加载状态
    }, 1500); // 延迟隐藏加载状态，让用户能看到结果
  }
};

// 处理连接取消
const handleConnectCancel = () => {
  showConnectDialog.value = false;
  connectionStatus.value = '';
};

// 处理连接确认
const handleConnectConfirm = async () => {
  if (connectDeviceIp.value) {
    await connectToDevice(connectDeviceIp.value);
  }
};

// 显示连接对话框
const showConnectDialogForDevice = (ip) => {
  connectDeviceIp.value = ip
  showConnectDialog.value = true
}

// 获取设备行样式
const getDeviceRowClass = ({ rowIndex }) => {
  return rowIndex % 2 === 0 ? 'even-row' : 'odd-row'
}

// 历史设备相关方法
const toggleHistoryView = () => {
  showDetailedHistory.value = !showDetailedHistory.value
}

const clearHistory = () => {
  deviceStore.clearHistory()
}

const onDeviceDisconnect = async (ip) => {
  try {
    // 显示加载状态
    const loadingInstance = ElMessage({
      message: '正在断开设备...',
      type: 'info',
      duration: 0,
      showClose: true
    })
    
    const response = await fetch('/api/devices/disconnect', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ ip_address: ip })
    })
    
    const result = await response.json()
    
    // 关闭加载状态
    loadingInstance.close()
    
    if (result.success) {
      // 更新设备连接状态
      deviceStore.updateDeviceConnectionStatus(ip, false);
      
      // 确保设备从已连接设备列表中移除
      const connectedDevices = deviceStore.connectedDevices;
      const updatedConnectedDevices = connectedDevices.filter(device => device.ip_address !== ip);
      deviceStore.setConnectedDevices(updatedConnectedDevices);
      
      deviceStore.toggleDeviceSelection(ip)
      deviceStore.removeMonitoringDevice(ip)
      console.log('设备断开成功')
      
      // 显示成功信息
      ElMessage({
        message: '设备已成功断开',
        type: 'success',
        duration: 3000
      })
      
      // 记录日志
      logger.info(`设备 ${ip} 断开成功`)
      
      // 刷新设备列表
      deviceStore.loadFromLocalStorage()
    } else {
      // 显示错误信息
      ElMessage({
        message: `断开设备失败: ${result.message || '未知错误'}`,
        type: 'error',
        duration: 3000
      })
      
      console.error(`断开设备 ${ip} 失败:`, result.message)
      logger.error(`断开设备 ${ip} 失败:`, result.message)
    }
  } catch (error) {
    // 显示错误信息
    ElMessage({
      message: `断开设备时发生错误: ${error.message}`,
      type: 'error',
      duration: 3000
    })
    
    console.error('断开设备时发生错误:', error)
    logger.error('断开设备时发生错误:', error)
  }
}

const onPerformanceToggle = (ip) => {
  // 直接打开性能监控弹窗
  selectedPerformanceDeviceIp.value = ip
  showPerformancePopup.value = true
  
  // 切换监控状态
  const isMonitoring = monitoringDevices.value.includes(ip);
  if (isMonitoring) {
    // 如果正在监控，停止监控
    deviceStore.toggleDeviceMonitoring(ip)
    if (performanceMonitorManagerRef.value) {
      performanceMonitorManagerRef.value.stopMonitoring(ip);
    }
  } else {
    // 如果未监控，启动监控
    deviceStore.toggleDeviceMonitoring(ip)
    if (performanceMonitorManagerRef.value) {
      performanceMonitorManagerRef.value.startMonitoring(ip);
    }
  }
}

// 设备数据
const connectedDevices = computed(() => {
  return deviceStore.connectedDevices
})

// 检查设备是否连接
const isDeviceConnected = (ip) => {
  return connectedDevices.value.some(device => device.ip_address === ip && device.is_connected)
}



// 模型状态处理器
const modelStatusHandler = (data) => {
  logger.debug('modelStatusHandler被调用，收到数据:', data);
  if (data && data.type === 'model_status') {
    logger.debug('处理model_status消息:', data);
    const { ip_address, status, message } = data;
    
    if (status === 'running') {
      ElMessage({
        message: `设备 ${ip_address} 模型正在运行`,
        type: 'success',
        duration: 3000
      });
    } else if (status === 'stopped') {
      ElMessage({
        message: `设备 ${ip_address} 模型已停止`,
        type: 'info',
        duration: 3000
      });
    } else if (status === 'error') {
      ElMessage({
        message: `设备 ${ip_address} 模型状态错误: ${message}`,
        type: 'error',
        duration: 3000
      });
    }
  }
}

// 注册模型状态处理器
if (registerMessageHandler) {
  registerMessageHandler('model_status', modelStatusHandler);
  logger.info('已注册model_status消息处理器');
}

// 检查模型状态
const onCheckModelStatus = async (ip) => {
  // 检查模型状态
  console.log('检查模型状态:', ip)
  
  try {
    // 显示加载状态
    const loadingInstance = ElMessage({
      message: '正在检查模型状态...',
      type: 'info',
      duration: 0,
      showClose: true
    })
    
    // 使用全局配置中的模型路径
    const modelRootPath = globalConfig.value.modelRootPath || '/home/chano/Github/dynamic_gestures'
    
    // 创建控制器用于超时控制
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 30000); // 30秒超时
    
    // 向后端发送检查请求 - 使用已实现的接口
    const response = await fetch('/api/model/status', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        ip_address: ip,
        model_path: modelRootPath,
        root_dir: modelRootPath
      }),
      signal: controller.signal
    })
    
    clearTimeout(timeoutId);
    
    if (!response.ok) {
      throw new Error(`HTTP错误: ${response.status}`)
    }
    
    const result = await response.json()
    
    // 关闭加载状态
    loadingInstance.close()
    
    if (result.success) {
      // 显示成功信息
      ElMessage({
        message: '模型状态检查成功，正在获取状态信息...',
        type: 'success',
        duration: 3000
      })
      
      // 记录日志
      logger.info(`设备 ${ip} 模型状态检查成功`)
    } else {
      // 显示错误信息
      ElMessage({
        message: `模型状态检查失败: ${result.message || '未知错误'}`,
        type: 'error',
        duration: 3000
      })
      
      // 记录日志
      logger.error(`设备 ${ip} 模型状态检查失败: ${result.message || '未知错误'}`)
    }
  } catch (error) {
    // 显示错误信息
    if (error.name === 'AbortError') {
      ElMessage({
        message: `检查模型状态超时，请稍后重试`,
        type: 'error',
        duration: 3000
      });
      logger.error(`设备 ${ip} 检查模型状态超时`);
    } else {
      ElMessage({
        message: `检查模型状态时发生错误: ${error.message}`,
        type: 'error',
        duration: 3000
      });
      logger.error(`设备 ${ip} 检查模型状态时发生错误:`, error);
    }
  }
}

// 打开配置管理弹窗
const onOpenConfigManager = (ip) => {
  selectedConfigDeviceIp.value = ip
  showConfigManager.value = true
}

// 关闭配置管理弹窗
const onCloseConfigManager = () => {
  showConfigManager.value = false
  selectedConfigDeviceIp.value = ''
}

// 处理配置保存
const onConfigSave = (config) => {
  console.log('配置保存:', config)
  // 可以在这里添加配置保存后的处理逻辑
}



// 强制刷新设备列表
const refreshDeviceList = () => {
  console.log('开始手动刷新设备列表，当前discoveredDevices长度:', discoveredDevices.value.length);
  
  // 显示刷新状态提示
  const refreshStartTime = Date.now();
  console.log('刷新开始时间:', new Date(refreshStartTime).toLocaleTimeString());
  
  // 清空当前设备列表，显示加载状态
  discoveredDevices.value = [];
  
  // 立即更新store，确保UI响应
  deviceStore.setDiscoveredDevices([]);
  
  // 从API获取最新的设备列表
  fetch('/api/devices/list', {
    method: 'GET',
    headers: { 'Content-Type': 'application/json' }
  })
    .then(response => {
      console.log('API响应状态:', response.status);
      if (!response.ok) {
        throw new Error(`API请求失败: ${response.status}`);
      }
      return response.json();
    })
    .then(data => {
      console.log('API返回数据:', data);
      if (data.success && data.devices) {
        // 确保设备列表是数组
        const devices = Array.isArray(data.devices) ? data.devices : [];
        console.log('从API获取到设备列表，数量:', devices.length);
        console.log('设备列表内容:', devices);
        
        // 处理设备类型显示
        const processedDevices = devices.map(device => ({
          ...device,
          device_type_display: getDeviceTypeDisplay(device.device_type_display || device.device_type)
        }));
        
        // 更新发现的设备列表
        discoveredDevices.value = [...processedDevices];
        
        // 同时更新store中的发现设备列表
        deviceStore.setDiscoveredDevices([...processedDevices]);
        
        // 同时更新已连接设备列表
        const connectedDevices = processedDevices.filter(device => device.is_connected);
        deviceStore.setConnectedDevices([...connectedDevices]);
        
        // 将设备添加到历史记录
        processedDevices.forEach(device => {
          deviceStore.addHistoryDevice(device);
        });
        
        // 确保响应式更新
        discoveredDevices.value = [...processedDevices];
        
        console.log('设备列表刷新完成，耗时:', Date.now() - refreshStartTime, 'ms');
      } else {
        console.error('API返回数据格式不正确:', data);
        // 即使API返回失败，也要确保UI状态正确
        discoveredDevices.value = [];
        deviceStore.setDiscoveredDevices([]);
      }
    })
    .catch(error => {
      console.error('从API获取设备列表时出错:', error);
      // 错误处理：确保UI状态正确
      discoveredDevices.value = [];
      deviceStore.setDiscoveredDevices([]);
      // 显示错误信息
      ElMessage.error(`获取设备列表失败: ${error.message}`);
    })
    .finally(() => {
      console.log('刷新设备列表操作完成');
    });
}

// 打开性能图表
const onOpenPerformanceChart = (ip) => {
  selectedPerformanceDeviceIp.value = ip
  showPerformancePopup.value = true
}

// 打开性能监控弹窗
const onOpenPerformancePopup = (ip) => {
  selectedPerformanceDeviceIp.value = ip
  showPerformancePopup.value = true
  // 注意：这里只打开弹窗，不自动启动监控
  // 监控的启动和停止由用户在弹窗中明确操作
}

// 关闭性能弹窗
const onClosePerformancePopup = () => {
  showPerformancePopup.value = false
  selectedPerformanceDeviceIp.value = ''
}

// 关闭性能图表
const onClosePerformanceChart = () => {
  showPerformanceChart.value = false
  selectedChartDeviceIp.value = ''
}

// 初始化
onMounted(() => {
  deviceStore.loadFromLocalStorage()
  
  // 建立WebSocket连接
  console.log('组件挂载，尝试建立WebSocket连接...');
  connectWebSocket();
  
  // 组件挂载时也获取当前设备列表
  console.log('组件挂载，尝试获取设备列表...');
  fetch('/api/devices/list', {
    method: 'GET',
    headers: { 'Content-Type': 'application/json' }
  })
    .then(response => response.json())
    .then(data => {
      if (data.success && data.devices) {
        discoveredDevices.value = [...data.devices];
        console.log('组件挂载时从API获取到设备列表，数量:', data.devices.length);
        
        // 同时更新store中的发现设备列表
        deviceStore.setDiscoveredDevices([...data.devices]);
        
        // 同时更新已连接设备列表
        const connectedDevices = data.devices.filter(device => device.is_connected);
        deviceStore.setConnectedDevices([...connectedDevices]);
      }
    })
    .catch(error => {
      console.error('组件挂载时获取设备列表出错:', error);
    });
})
</script>

<style scoped>
.multi-device-manager {
  width: 100%;
  margin: 0 auto;
  padding: 20px;
}

.global-config-panel {
  margin-bottom: 20px;
  border: none !important;
  box-shadow: 0 2px 10px rgba(0,0,0,0.06);
  border-radius: 12px;
}

.global-config-panel :deep(.el-card__header) {
  border-bottom: none !important;
  padding: 16px 20px;
}

.global-config-panel :deep(.el-card__body) {
  padding: 16px 20px;
}

.config-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.config-title {
  display: flex;
  align-items: center;
  gap: 8px;
  font-size: 1.1rem;
  font-weight: 600;
  color: #303133;
}

.global-config-content {
  display: flex;
  flex-direction: column;
  gap: 16px;
}

.config-info {
  font-size: 0.8rem;
  color: #909399;
  margin-top: 4px;
}

.scan-panel {
  margin-bottom: 20px;
  border: none !important;
  box-shadow: 0 2px 10px rgba(0,0,0,0.06);
  border-radius: 12px;
}

.scan-panel :deep(.el-card__header) {
  border-bottom: none !important;
  padding: 16px 20px;
}

.scan-panel :deep(.el-card__body) {
  padding: 16px 20px;
}

.discovered-devices-panel {
  margin-bottom: 24px;
}

.scan-header {
  display: flex;
  flex-direction: column;
  gap: 12px;
  border: none;
  padding: 0;
}

.scan-header-content {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  flex-wrap: wrap;
  gap: 16px;
}

.scan-header-info {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.scan-icon-wrapper {
  display: flex;
  align-items: center;
  gap: 12px;
}

.scan-icon {
  font-size: 1.5rem;
  color: #409EFF;
}

.scan-title {
  margin: 0;
  font-size: 1.3rem;
  font-weight: 600;
  color: #303133;
}

.scan-description {
  display: flex;
  align-items: center;
}

.scan-desc-text {
  font-size: 0.9rem;
  color: #909399;
}

.panel-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-title {
  font-size: 1.2rem;
  font-weight: 600;
  color: #303133;
}

.scan-controls {
  display: flex;
  gap: 12px;
  align-items: center;
  flex-shrink: 0;
}

.scan-mode-select {
  min-width: 160px;
}

.scan-action-btn {
  transition: all 0.3s ease;
}

.start-scan-btn {
  background: linear-gradient(135deg, #409EFF, #327AFF);
  border: none;
}

.start-scan-btn:hover {
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(64, 158, 255, 0.3);
}

.stop-scan-btn {
  background: linear-gradient(135deg, #F56C6C, #E64A4A);
  border: none;
}

.stop-scan-btn:hover {
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(245, 108, 108, 0.3);
}

.scan-progress-container {
  margin-top: 12px;
}

.progress-content {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.custom-progress {
  border-radius: 14px;
}

.progress-text {
  font-size: 0.9rem;
  color: #909399;
  align-self: flex-end;
  font-style: italic;
}

.discovered-devices {
  margin-top: 16px;
  width: 100%;
  overflow-x: auto;
  border-radius: 8px;
  box-shadow: 0 1px 4px rgba(0, 0, 0, 0.05);
}

.discovered-devices :deep(.el-table) {
  width: 100%;
  border-radius: 8px;
}

.discovered-devices :deep(.el-table__header-wrapper) {
  border-radius: 8px 8px 0 0;
}

.discovered-devices :deep(.el-table__body-wrapper) {
  min-height: 200px;
}

.no-discovered-devices {
  text-align: center;
  padding: 40px 0;
}

.device-name-cell {
  display: flex;
  align-items: center;
  gap: 8px;
}

.even-row {
  background-color: #f5f7fa;
}

.odd-row {
  background-color: #ffffff;
}

/* 响应式设计 */
@media (max-width: 768px) {
  .multi-device-manager {
    padding: 10px;
  }
  
  .scan-header-content {
    flex-direction: column;
    align-items: flex-start;
    gap: 12px;
  }
  
  .scan-controls {
    width: 100%;
    justify-content: center;
  }
  
  .discovered-devices {
    margin-top: 12px;
  }
  
  .discovered-devices :deep(.el-table__body-wrapper) {
    min-height: 150px;
  }
  
  .header-title {
    font-size: 1rem;
  }
}

@media (min-width: 769px) and (max-width: 1024px) {
  .multi-device-manager {
    padding: 15px;
  }
  
  .scan-header-content {
    flex-wrap: wrap;
  }
  
  .discovered-devices {
    margin-top: 14px;
  }
}

@media (min-width: 1025px) and (max-width: 1200px) {
  .multi-device-manager {
    padding: 20px;
  }
  
  .scan-header-content {
    justify-content: space-between;
  }
}

@media (min-width: 1201px) {
  .multi-device-manager {
    padding: 24px;
  }
  
  .scan-header-content {
    justify-content: space-between;
  }
  
  .discovered-devices {
    margin-top: 20px;
  }
}
</style>