<template>
  <el-dialog
    v-model="showDialog"
    title="设备性能监控"
    width="85%"
    top="3vh"
    :before-close="handleClose"
  >
    <div class="performance-chart-container">
      <!-- 控制面板 -->
      <div class="control-panel">
        <el-select 
          v-model="selectedDevice" 
          placeholder="选择设备" 
          @change="onDeviceChange"
          style="width: 300px; margin-right: 20px;"
        >
          <el-option
            v-for="device in devices"
            :key="device.ip_address"
            :label="`${device.hostname || device.ip_address} (${device.ip_address})`"
            :value="device.ip_address"
          />
        </el-select>
        
        <el-button-group>
          <el-button 
            :type="activeTab === 'realtime' ? 'primary' : 'default'"
            @click="activeTab = 'realtime'"
          >
            实时监控
          </el-button>
          <el-button 
            :type="activeTab === 'history' ? 'primary' : 'default'"
            @click="activeTab = 'history'"
          >
            历史数据
          </el-button>
        </el-button-group>
        
        <!-- 性能监控控制按钮 -->
        <el-button 
          :type="isMonitoring ? 'danger' : 'success'"
          @click="togglePerformanceMonitoring"
          :loading="monitoringLoading"
        >
          {{ isMonitoring ? '停止监控' : '开始监控' }}
        </el-button>
        
        <div class="refresh-control">
          <span>刷新间隔:</span>
          <el-slider
            v-model="updateIntervalLocal"
            :min="1000"
            :max="30000"
            :step="1000"
            :format-tooltip="formatTooltip"
            style="width: 150px; margin: 0 10px;"
          />
          <span>{{ updateIntervalLocal }}ms</span>
        </div>
      </div>

      <!-- 实时监控视图 -->
      <div v-if="activeTab === 'realtime'" class="realtime-view">
        <div class="metrics-grid">
          <el-card shadow="hover" class="metric-card">
            <div class="metric-header">
              <span class="metric-title">{{ metric.title }}</span>
              <el-tag :type="getMetricStatus(metric.key) === 'danger' ? 'danger' : getMetricStatus(metric.key) === 'warning' ? 'warning' : 'success'">
                {{ getMetricValue(metric.key) }}{{ metric.unit }}
              </el-tag>
            </div>
            <el-progress
              :percentage="getMetricValue(metric.key)"
              :color="getProgressColor(getMetricStatus(metric.key))"
              :stroke-width="20"
              class="metric-progress"
            />
          </el-card>
        </div>
      </div>

      <!-- 历史数据视图 -->
      <div v-if="activeTab === 'history'" class="history-view">
        <div class="chart-grid">
          <el-card shadow="hover" class="chart-card">
            <template #header>
              <div class="chart-header">
                <span>{{ chart.title }}</span>
              </div>
            </template>
            <div class="chart-container">
              <div class="chart-line" v-if="getChartData(chart.key).length > 0">
                <div class="chart-y-axis">
                  <div 
                    v-for="(value, index) in getChartData(chart.key)" 
                    :key="index" 
                    class="chart-point"
                    :style="{ height: value + '%' }"
                  >
                    <span class="chart-value">{{ value }}{{ chart.unit || '' }}</span>
                  </div>
                </div>
                <div class="chart-x-axis">
                  <div 
                    v-for="(time, index) in timeAxis.slice(timeAxis.length - getChartData(chart.key).length)" 
                    :key="index" 
                    class="chart-time"
                  >
                    {{ time }}
                  </div>
                </div>
              </div>
              <div v-else class="no-data">暂无数据</div>
            </div>
          </el-card>
        </div>
      </div>

      <!-- 设备状态信息 -->
      <el-card class="device-status-card" shadow="never">
        <template #header>
          <div class="device-status-header">
            <span>设备信息</span>
          </div>
        </template>
        <el-descriptions :column="2" size="small" border>
          <el-descriptions-item label="设备名称">{{ selectedDeviceData.hostname || selectedDeviceData.ip_address }}</el-descriptions-item>
          <el-descriptions-item label="IP地址">{{ selectedDeviceData.ip_address }}</el-descriptions-item>
          <el-descriptions-item label="连接状态">
            <el-tag :type="selectedDeviceData.is_connected ? 'success' : 'info'">
              {{ selectedDeviceData.is_connected ? '已连接' : '未连接' }}
            </el-tag>
          </el-descriptions-item>
          <el-descriptions-item label="在线状态">
            <el-tag :type="selectedDeviceData.is_online ? 'success' : 'info'">
              {{ selectedDeviceData.is_online ? '在线' : '离线' }}
            </el-tag>
          </el-descriptions-item>
        </el-descriptions>
      </el-card>
    </div>
  </el-dialog>
</template>

<script setup>
import { ref, computed, watch, onMounted, onUnmounted } from 'vue'
import { useDeviceStore } from '../stores/deviceStore'
import { ElCard, ElProgress, ElDescriptions, ElDescriptionsItem, ElTag } from 'element-plus'
import { useWebSocket } from '../composables/useWebSocket'

// Props
const props = defineProps({
  show: {
    type: Boolean,
    required: true
  },
  devices: {
    type: Array,
    required: true
  },
  updateInterval: {
    type: Number,
    default: 5000
  },
  selectedDeviceIp: {
    type: String,
    default: ''
  }
})

// Emits
const emit = defineEmits(['update:show', 'update:updateInterval', 'close'])

// 响应式数据
const showDialog = ref(props.show)
const selectedDevice = ref('')
const activeTab = ref('realtime')
const updateIntervalLocal = ref(props.updateInterval)
const monitoringLoading = ref(false)

// 性能监控状态
const isMonitoring = computed(() => {
  const deviceStore = useDeviceStore()
  return selectedDevice.value && deviceStore.monitoringDevices.includes(selectedDevice.value)
})

// 性能历史数据
const performanceHistory = ref({})

// 实时指标配置
const realtimeMetrics = [
  { key: 'cpu_usage', title: 'CPU 使用率', unit: '%', type: 'gauge' },
  { key: 'memory_usage', title: '内存使用率', unit: '%', type: 'gauge' },
  { key: 'npu_usage', title: 'NPU 使用率', unit: '%', type: 'gauge' },
  { key: 'disk_usage', title: '磁盘使用率', unit: '%', type: 'gauge' },
  { key: 'system_temperature', title: '系统温度', unit: '°C', type: 'gauge' },
  { key: 'npu_temperature', title: 'NPU 温度', unit: '°C', type: 'gauge' },
  { key: 'load_average_1m', title: '负载 1分钟', unit: '', type: 'gauge' },
  { key: 'npu_power', title: 'NPU 功耗', unit: 'W', type: 'gauge' },
  { key: 'memory_available', title: '可用内存', unit: 'MB', type: 'gauge' }
]

// 历史图表配置
const chartConfigs = [
  { key: 'cpu_usage', title: 'CPU 使用率趋势', type: 'line' },
  { key: 'memory_usage', title: '内存使用率趋势', type: 'line' },
  { key: 'npu_usage', title: 'NPU 使用率趋势', type: 'line' },
  { key: 'system_temperature', title: '系统温度趋势', type: 'line' },
  { key: 'npu_temperature', title: 'NPU 温度趋势', type: 'line' },
  { key: 'load_average_1m', title: '系统负载趋势', type: 'line' }
]

// 时间轴数据
const timeAxis = ref([])

// 监听show prop的变化
watch(() => props.show, (newVal) => {
  showDialog.value = newVal
})

// 监听showDialog的变化并通知父组件
watch(showDialog, (newVal) => {
  if (newVal !== props.show) {
    emit('update:show', newVal)
  }
})

// 监听本地更新间隔变化并通知父组件
watch(updateIntervalLocal, (newVal) => {
  emit('update:updateInterval', newVal)
})

// 监听selectedDeviceIp的变化并更新选中的设备
watch(() => props.selectedDeviceIp, (newVal) => {
  if (newVal && props.devices.some(device => device.ip_address === newVal)) {
    selectedDevice.value = newVal
  } else if (props.devices.length > 0 && !selectedDevice.value) {
    selectedDevice.value = props.devices[0].ip_address
  }
})

// WebSocket消息处理器
const wsInstance = useWebSocket(`${window.location.protocol}//${window.location.hostname}:8082`)
const { registerMessageHandler, unregisterMessageHandler } = wsInstance

// 获取进度条颜色
const getProgressColor = (status) => {
  if (status === 'danger') return '#F56C6C' // 红色
  if (status === 'warning') return '#E6A23C' // 黄色
  return '#67C23A' // 绿色
}

const performanceDataHandler = (data) => {
  // 检查消息类型，只处理真正的性能数据
  if (data.type === 'config_status') {
    // 这是配置状态消息，不是性能数据
    console.log('[PerformanceChartModal] 接收到配置状态消息:', data);
    return;
  } else if (data.type === 'scan_progress') {
    // 这是设备扫描进度消息，不是性能数据
    return;
  } else if (data.type !== 'performance_data' && data.type !== 'performance_batch' && !data.device_ip) {
    // 不是性能数据消息，跳过处理
    return;
  }
  
  console.log('[PerformanceChartModal] 接收到性能数据:', data);
  
  // 检查是否是批量数据
  if (data.type === 'performance_batch' && data.devices) {
    // 处理批量性能数据
    data.devices.forEach(deviceData => {
      if (deviceData.device_ip) {
        // 更新当前设备的性能历史数据
        if (selectedDevice.value === deviceData.device_ip) {
          updateDevicePerformanceHistory(deviceData.device_ip, deviceData);
        }
        
        // 更新store中的设备性能数据
        const deviceStore = useDeviceStore();
        deviceStore.updateDevicePerformance(deviceData.device_ip, {
          cpu_usage: deviceData.cpu_usage,
          memory_usage: deviceData.memory_usage,
          disk_usage: deviceData.disk_usage,
          load_average_1m: deviceData.load_average_1m,
          npu_usage: deviceData.npu_usage,
          system_temperature: deviceData.system_temperature,
          npu_temperature: deviceData.npu_temperature,
          npu_power: deviceData.npu_power,
          memory_available: deviceData.memory_available,
          memory_total: deviceData.memory_total
        });
      }
    });
  } else if (data.device_ip) {
    // 处理单个设备的性能数据
    // 更新当前设备的性能历史数据
    if (selectedDevice.value === data.device_ip) {
      updateDevicePerformanceHistory(data.device_ip, data);
    }
    
    // 更新store中的设备性能数据
    const deviceStore = useDeviceStore();
    deviceStore.updateDevicePerformance(data.device_ip, {
      cpu_usage: data.cpu_usage,
      memory_usage: data.memory_usage,
      disk_usage: data.disk_usage,
      load_average_1m: data.load_average_1m,
      npu_usage: data.npu_usage,
      system_temperature: data.system_temperature,
      npu_temperature: data.npu_temperature,
      npu_power: data.npu_power,
      memory_available: data.memory_available,
      memory_total: data.memory_total
    });
  }
}

// 监听props.show变化来注册/注销消息处理器
onMounted(() => {
  registerMessageHandler('performance_chart_modal', performanceDataHandler)
})

onUnmounted(() => {
  unregisterMessageHandler('performance_chart_modal')
})

// 计算属性
const selectedDeviceData = computed(() => {
  return props.devices.find(d => d.ip_address === selectedDevice.value) || {}
})

// 获取指标值
const getMetricValue = (key) => {
  const device = selectedDeviceData.value
  return device.performance?.[key] || 0
}

// 获取指标状态
const getMetricStatus = (key) => {
  const value = getMetricValue(key)
  
  if (key.includes('temperature')) {
    if (value > 80) return 'danger'
    if (value > 60) return 'warning'
    return 'normal'
  } else if (key.includes('usage')) {
    if (value > 80) return 'danger'
    if (value > 60) return 'warning'
    return 'normal'
  } else {
    return 'normal'
  }
}

// 获取图表数据
const getChartData = (key) => {
  const history = performanceHistory.value[selectedDevice.value]?.[key] || []
  return history.map(item => item.value)
}

// 格式化提示工具
const formatTooltip = (value) => {
  return `${value}ms`
}

// 方法
const updateDevicePerformanceHistory = (deviceIp, metrics) => {
  if (!performanceHistory.value[deviceIp]) {
    performanceHistory.value[deviceIp] = {}
  }
  
  // 更新时间轴
  timeAxis.value.push(new Date().toLocaleTimeString())
  
  // 限制时间轴长度
  if (timeAxis.value.length > 20) {
    timeAxis.value.shift()
  }
  
  // 更新各指标的历史数据
  Object.keys(metrics).forEach(key => {
    if (!performanceHistory.value[deviceIp][key]) {
      performanceHistory.value[deviceIp][key] = []
    }
    
    performanceHistory.value[deviceIp][key].push({
      timestamp: Date.now(),
      value: metrics[key]
    })
    
    // 限制历史数据长度
    if (performanceHistory.value[deviceIp][key].length > 20) {
      performanceHistory.value[deviceIp][key].shift()
    }
  })
}

const handleClose = () => {
  emit('close')
}

const onDeviceChange = (deviceId) => {
  selectedDevice.value = deviceId
}

// 切换性能监控状态
const togglePerformanceMonitoring = async () => {
  if (!selectedDevice.value) {
    console.warn('没有选择设备，无法切换性能监控状态')
    return
  }
  
  monitoringLoading.value = true
  
  try {
    // 更新store中的监控状态
    const deviceStore = useDeviceStore()
    if (isMonitoring.value) {
      // 停止监控
      deviceStore.stopDeviceMonitoring(selectedDevice.value)
      
      // 发送停止监控命令
      fetch('/api/devices/performance_control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          ip_address: selectedDevice.value,
          action: 'stop'
        })
      }).catch(error => {
        console.error('发送性能监控控制命令失败:', error)
      })
    } else {
      // 开始监控
      deviceStore.startDeviceMonitoring(selectedDevice.value)
      
      // 发送开始监控命令
      fetch('/api/devices/performance_control', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          ip_address: selectedDevice.value,
          action: 'start'
        })
      }).catch(error => {
        console.error('发送性能监控控制命令失败:', error)
      })
    }
  } catch (error) {
    console.error('切换性能监控状态失败:', error)
  } finally {
    monitoringLoading.value = false
  }
}
</script>

<style scoped>
.performance-chart-container {
  padding: 20px 0;
}

.control-panel {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
  flex-wrap: wrap;
  gap: 15px;
  padding: 15px;
  background-color: #f8f9fa;
  border-radius: 8px;
}

.refresh-control {
  display: flex;
  align-items: center;
  gap: 10px;
}

.realtime-view {
  margin-bottom: 20px;
}

.metrics-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(250px, 1fr));
  gap: 15px;
}

.history-view {
  margin-top: 20px;
}

.chart-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(400px, 1fr));
  gap: 20px;
}

@media (max-width: 768px) {
  .control-panel {
    flex-direction: column;
    align-items: stretch;
  }
  
  .metrics-grid {
    grid-template-columns: 1fr;
  }
  
  .chart-grid {
    grid-template-columns: 1fr;
  }
}
</style>