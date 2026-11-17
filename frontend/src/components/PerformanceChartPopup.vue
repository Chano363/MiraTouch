<template>
  <el-dialog
    v-model="visible"
    :title="`性能监控 - ${selectedDeviceIp}`"
    width="80%"
    :before-close="handleClose"
  >
    <div v-if="selectedDeviceIp" class="chart-container">
      <div class="four-sections-container">
        <!-- 第一个区域：基础性能指标 -->
        <el-card class="section-card">
          <template #header>
            <div class="section-header">
              <span class="section-title">基础性能指标</span>
            </div>
          </template>
          <div class="metric-grid">
            <div class="metric-item" v-for="metric in basicMetrics" :key="metric.key">
              <div class="metric-header">
                <span class="metric-title">{{ metric.title }}</span>
                <el-tag :type="getMetricTagType(metric.value)">
                  {{ formatMetricValue(metric) }}
                </el-tag>
              </div>
            </div>
          </div>
        </el-card>
        
        <!-- 第二个区域：系统负载 -->
        <el-card class="section-card">
          <template #header>
            <div class="section-header">
              <span class="section-title">系统负载</span>
            </div>
          </template>
          <div class="metric-grid">
            <div class="metric-item" v-for="metric in loadMetrics" :key="metric.key">
              <div class="metric-header">
                <span class="metric-title">{{ metric.title }}</span>
                <el-tag :type="getMetricTagType(metric.value)">
                  {{ formatMetricValue(metric) }}
                </el-tag>
              </div>
            </div>
          </div>
        </el-card>
        
        <!-- 第三个区域：NPU相关指标 -->
        <el-card class="section-card">
          <template #header>
            <div class="section-header">
              <span class="section-title">NPU相关指标</span>
            </div>
          </template>
          <div class="metric-grid">
            <div class="metric-item" v-for="metric in npuMetrics" :key="metric.key">
              <div class="metric-header">
                <span class="metric-title">{{ metric.title }}</span>
                <el-tag :type="getMetricTagType(metric.value)">
                  {{ formatMetricValue(metric) }}
                </el-tag>
              </div>
            </div>
          </div>
        </el-card>
        
        <!-- 第四个区域：内存指标和图表 -->
        <el-card class="section-card">
          <template #header>
            <div class="section-header">
              <span class="section-title">内存指标与图表</span>
            </div>
          </template>
          <div class="memory-content-wrapper">
            <div class="memory-metrics">
              <div class="metric-item" v-for="metric in memoryMetrics" :key="metric.key">
                <div class="metric-header">
                  <span class="metric-title">{{ metric.title }}</span>
                  <el-tag :type="getMetricTagType(metric.value)">
                    {{ formatMetricValue(metric) }}
                  </el-tag>
                </div>

              </div>
            </div>
            <!-- 性能趋势图 -->
            <div class="chart-wrapper">
              <div ref="cpuChartRef" class="chart"></div>
              <div ref="memoryChartRef" class="chart"></div>
              <div ref="npuChartRef" class="chart"></div>
            </div>
          </div>
        </el-card>
      </div>
    </div>
    <div v-else class="no-device-selected">
      <p>请选择要监控的设备</p>
    </div>
    
    <template #footer>
      <div v-if="errorMessage" class="error-message">
        {{ errorMessage }}
      </div>
      <span class="dialog-footer">
        <el-button @click="stopMonitoring" :loading="isLoading">停止监控</el-button>
        <el-button type="primary" @click="startMonitoring" :disabled="isMonitoring" :loading="isLoading">
          开始监控
        </el-button>
      </span>
    </template>
  </el-dialog>
</template>

<script setup>
import { ref, watch, onMounted, onUnmounted, computed } from 'vue'
import { useWebSocket } from '../composables/useWebSocket'
import { useDeviceStore } from '../stores/deviceStore'
import * as echarts from 'echarts'

// Props
const props = defineProps({
  modelValue: Boolean,
  deviceIp: String
})

// Emits
const emit = defineEmits(['update:modelValue', 'close'])

// 使用设备存储
const deviceStore = useDeviceStore()

// Refs
const visible = ref(false)
const selectedDeviceIp = ref('')
const cpuChartRef = ref()
const memoryChartRef = ref()
const npuChartRef = ref()
const cpuChart = ref(null)
const memoryChart = ref(null)
const npuChart = ref(null)

const isMonitoring = ref(false)
const isLoading = ref(false)
const errorMessage = ref('')

// 初始化为0，等待真实数据更新
const allMetrics = ref([
  { key: 'cpu', title: 'CPU使用率', value: 0 },
  { key: 'memory', title: '内存使用率', value: 0 },
  { key: 'npu', title: 'NPU使用率', value: 0 },
  { key: 'disk', title: '磁盘使用率', value: 0 },
  { key: 'load_avg_1m', title: '系统负载(1分钟)', value: 0 },
  { key: 'load_avg_5m', title: '系统负载(5分钟)', value: 0 },
  { key: 'load_avg_15m', title: '系统负载(15分钟)', value: 0 },
  { key: 'npu_memory', title: 'NPU内存使用率', value: 0 },
  { key: 'npu_power', title: 'NPU功耗', value: 0, unit: 'W' },
  { key: 'npu_temp', title: 'NPU温度', value: 0, unit: '°C' },
  { key: 'sys_temp', title: '系统温度', value: 0, unit: '°C' },
  { key: 'memory_available', title: '可用内存', value: 0, unit: 'MB' },
  { key: 'memory_total', title: '总内存', value: 0, unit: 'MB' }
])

// 按类别分组的指标
const basicMetrics = computed(() => [
  allMetrics.value.find(m => m.key === 'cpu') || { key: 'cpu', title: 'CPU使用率', value: 0 },
  allMetrics.value.find(m => m.key === 'memory') || { key: 'memory', title: '内存使用率', value: 0 },
  allMetrics.value.find(m => m.key === 'disk') || { key: 'disk', title: '磁盘使用率', value: 0 },
  allMetrics.value.find(m => m.key === 'sys_temp') || { key: 'sys_temp', title: '系统温度', value: 0, unit: '°C' }
])

const loadMetrics = computed(() => [
  allMetrics.value.find(m => m.key === 'load_avg_1m') || { key: 'load_avg_1m', title: '系统负载(1分钟)', value: 0 },
  allMetrics.value.find(m => m.key === 'load_avg_5m') || { key: 'load_avg_5m', title: '系统负载(5分钟)', value: 0 },
  allMetrics.value.find(m => m.key === 'load_avg_15m') || { key: 'load_avg_15m', title: '系统负载(15分钟)', value: 0 }
])

const npuMetrics = computed(() => [
  allMetrics.value.find(m => m.key === 'npu') || { key: 'npu', title: 'NPU使用率', value: 0 },
  allMetrics.value.find(m => m.key === 'npu_memory') || { key: 'npu_memory', title: 'NPU内存使用率', value: 0 },
  allMetrics.value.find(m => m.key === 'npu_power') || { key: 'npu_power', title: 'NPU功耗', value: 0, unit: 'W' },
  allMetrics.value.find(m => m.key === 'npu_temp') || { key: 'npu_temp', title: 'NPU温度', value: 0, unit: '°C' }
])

const memoryMetrics = computed(() => [
  allMetrics.value.find(m => m.key === 'memory_available') || { key: 'memory_available', title: '可用内存', value: 0, unit: 'MB' },
  allMetrics.value.find(m => m.key === 'memory_total') || { key: 'memory_total', title: '总内存', value: 0, unit: 'MB' }
])

// 用于更新指标值的辅助函数
const updateMetricValue = (key, value) => {
  const metricIndex = allMetrics.value.findIndex(m => m.key === key);
  if (metricIndex !== -1) {
    // 使用 Vue 的响应式更新方式
    allMetrics.value[metricIndex] = { ...allMetrics.value[metricIndex], value };
  }
}

// WebSocket
// 修改WebSocket连接地址以使用与useWebSocket相同的端口
const wsInstance = useWebSocket(`ws://${window.location.hostname}:8083`)
const { registerMessageHandler, unregisterMessageHandler } = wsInstance

// 初始化图表
const initCharts = () => {
  if (cpuChartRef.value) {
    cpuChart.value = echarts.init(cpuChartRef.value)
    cpuChart.value.setOption({
      title: { text: 'CPU使用率趋势' },
      xAxis: { type: 'time' },
      yAxis: { type: 'value', max: 100 },
      series: [{ 
        type: 'line', 
        smooth: true,
        data: []
      }]
    })
  }

  if (memoryChartRef.value) {
    memoryChart.value = echarts.init(memoryChartRef.value)
    memoryChart.value.setOption({
      title: { text: '内存使用率趋势' },
      xAxis: { type: 'time' },
      yAxis: { type: 'value', max: 100 },
      series: [{ 
        type: 'line', 
        smooth: true,
        data: []
      }]
    })
  }

  if (npuChartRef.value) {
    npuChart.value = echarts.init(npuChartRef.value)
    npuChart.value.setOption({
      title: { text: 'NPU使用率趋势' },
      xAxis: { type: 'time' },
      yAxis: { type: 'value', max: 100 },
      series: [{ 
        type: 'line', 
        smooth: true,
        data: []
      }]
    })
  }
}

// 更新图表数据
const updateChartData = (data) => {
  const now = new Date()
  
  // 更新指标值
  if (data.cpu_usage !== undefined) {
    updateMetricValue('cpu', Math.round(data.cpu_usage));
  }
  if (data.memory_usage !== undefined) {
    updateMetricValue('memory', Math.round(data.memory_usage));
  }
  if (data.npu_usage !== undefined) {
    updateMetricValue('npu', Math.round(data.npu_usage));
  }
  if (data.disk_usage !== undefined) {
    updateMetricValue('disk', Math.round(data.disk_usage));
  }
  if (data.load_average_1m !== undefined) {
    updateMetricValue('load_avg_1m', parseFloat(data.load_average_1m.toFixed(2)));
  }
  if (data.load_average_5m !== undefined) {
    updateMetricValue('load_avg_5m', parseFloat(data.load_average_5m.toFixed(2)));
  }
  if (data.load_average_15m !== undefined) {
    updateMetricValue('load_avg_15m', parseFloat(data.load_average_15m.toFixed(2)));
  }
  if (data.npu_memory_usage !== undefined) {
    updateMetricValue('npu_memory', Math.round(data.npu_memory_usage));
  }
  if (data.npu_power !== undefined) {
    updateMetricValue('npu_power', parseFloat(data.npu_power.toFixed(2)));
  }
  if (data.npu_temperature !== undefined) {
    updateMetricValue('npu_temp', Math.round(data.npu_temperature));
  }
  if (data.system_temperature !== undefined) {
    updateMetricValue('sys_temp', Math.round(data.system_temperature));
  }
  if (data.memory_available !== undefined) {
    updateMetricValue('memory_available', Math.round(data.memory_available));
  }
  if (data.memory_total !== undefined) {
    updateMetricValue('memory_total', Math.round(data.memory_total));
  }

  // 确保图表已初始化后再更新图表数据
  if (!cpuChart.value && data.cpu_usage !== undefined) {
    console.log('CPU图表未初始化，跳过图表更新');
  } else if (cpuChart.value && data.cpu_usage !== undefined) {
    try {
      // 尝试使用appendData方法
      if (cpuChart.value.appendData) {
        cpuChart.value.appendData({
          seriesIndex: 0,
          data: [now.getTime(), data.cpu_usage]
        });
      } else {
        // 如果appendData不可用，使用setOption方法
        let option = cpuChart.value.getOption();
        if (!option || !option.series || option.series.length === 0) {
          cpuChart.value.setOption({
            title: { text: 'CPU使用率趋势' },
            xAxis: { type: 'time' },
            yAxis: { type: 'value', max: 100 },
            series: [{ 
              type: 'line', 
              smooth: true,
              data: [[now.getTime(), data.cpu_usage]]
            }]
          });
        } else {
          option.series[0].data.push([now.getTime(), data.cpu_usage]);
          // 限制数据点数量，避免过多数据影响性能
          if (option.series[0].data.length > 50) {
            option.series[0].data.shift();
          }
          cpuChart.value.setOption(option, { notMerge: false });
        }
      }
      // 强制刷新图表
      cpuChart.value.resize();
    } catch (e) {
      console.error('CPU图表更新失败:', e);
      // 如果出现错误，重新初始化图表
      cpuChart.value.setOption({
        title: { text: 'CPU使用率趋势' },
        xAxis: { type: 'time' },
        yAxis: { type: 'value', max: 100 },
        series: [{ 
          type: 'line', 
          smooth: true,
          data: [[now.getTime(), data.cpu_usage]]
        }]
      });
    }
  }
  
  if (!memoryChart.value && data.memory_usage !== undefined) {
    console.log('内存图表未初始化，跳过图表更新');
  } else if (memoryChart.value && data.memory_usage !== undefined) {
    try {
      if (memoryChart.value.appendData) {
        memoryChart.value.appendData({
          seriesIndex: 0,
          data: [now.getTime(), data.memory_usage]
        });
      } else {
        let option = memoryChart.value.getOption();
        if (!option || !option.series || option.series.length === 0) {
          memoryChart.value.setOption({
            title: { text: '内存使用率趋势' },
            xAxis: { type: 'time' },
            yAxis: { type: 'value', max: 100 },
            series: [{ 
              type: 'line', 
              smooth: true,
              data: [[now.getTime(), data.memory_usage]]
            }]
          });
        } else {
          option.series[0].data.push([now.getTime(), data.memory_usage]);
          if (option.series[0].data.length > 50) {
            option.series[0].data.shift();
          }
          memoryChart.value.setOption(option, { notMerge: false });
        }
      }
      // 强制刷新图表
      memoryChart.value.resize();
    } catch (e) {
      console.error('内存图表更新失败:', e);
      memoryChart.value.setOption({
        title: { text: '内存使用率趋势' },
        xAxis: { type: 'time' },
        yAxis: { type: 'value', max: 100 },
        series: [{ 
          type: 'line', 
          smooth: true,
          data: [[now.getTime(), data.memory_usage]]
        }]
      });
    }
  }
  
  if (!npuChart.value && data.npu_usage !== undefined) {
    console.log('NPU图表未初始化，跳过图表更新');
  } else if (npuChart.value && data.npu_usage !== undefined) {
    try {
      if (npuChart.value.appendData) {
        npuChart.value.appendData({
          seriesIndex: 0,
          data: [now.getTime(), data.npu_usage]
        });
      } else {
        let option = npuChart.value.getOption();
        if (!option || !option.series || option.series.length === 0) {
          npuChart.value.setOption({
            title: { text: 'NPU使用率趋势' },
            xAxis: { type: 'time' },
            yAxis: { type: 'value', max: 100 },
            series: [{ 
              type: 'line', 
              smooth: true,
              data: [[now.getTime(), data.npu_usage]]
            }]
          });
        } else {
          option.series[0].data.push([now.getTime(), data.npu_usage]);
          if (option.series[0].data.length > 50) {
            option.series[0].data.shift();
          }
          npuChart.value.setOption(option, { notMerge: false });
        }
      }
      // 强制刷新图表
      npuChart.value.resize();
    } catch (e) {
      console.error('NPU图表更新失败:', e);
      npuChart.value.setOption({
        title: { text: 'NPU使用率趋势' },
        xAxis: { type: 'time' },
        yAxis: { type: 'value', max: 100 },
        series: [{ 
          type: 'line', 
          smooth: true,
          data: [[now.getTime(), data.npu_usage]]
        }]
      });
    }
  }
}

// WebSocket消息处理器
const performanceDataHandler = (data) => {
  // 检查消息类型，只处理真正的性能数据
  if (data.type === 'config_status') {
    // 这是配置状态消息，不是性能数据
    console.log('PerformanceChartPopup收到配置状态消息:', data);
    return;
  } else if (data.type === 'scan_progress') {
    // 这是设备扫描进度消息，不是性能数据
    return;
  }
  
  console.log('PerformanceChartPopup收到WebSocket消息:', data);
  
  // 处理性能数据消息 - 包含"type": "performance_data"标识或包含性能指标字段
  if (data.type === 'performance_data' || 
      (data.device_ip && (data.hasOwnProperty('cpu_usage') || data.hasOwnProperty('memory_usage') || 
         data.hasOwnProperty('npu_usage') || data.hasOwnProperty('disk_usage')))) {
    
    // 检查设备IP是否匹配当前选中的设备，如果没有设置选中的IP，则先缓存数据
    if (selectedDeviceIp.value && data.device_ip === selectedDeviceIp.value) {
      console.log('更新性能数据:', data);
      updateChartData(data);
      
      // 同时更新到设备store中，确保数据一致性
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
      
      // 强制触发响应式更新
      allMetrics.value = [...allMetrics.value];
    } else if (!selectedDeviceIp.value && data.device_ip) {
      // 如果当前没有设置设备IP但收到了特定设备的数据，可以考虑设置为当前设备
      console.log('首次收到设备数据，设置为当前设备:', data.device_ip);
      selectedDeviceIp.value = data.device_ip;
      
      // 立即更新数据
      console.log('更新性能数据:', data);
      updateChartData(data);
      
      // 同时更新到设备store中
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
      
      // 强制触发响应式更新
      allMetrics.value = [...allMetrics.value];
    } else {
      console.log('收到其他设备的性能数据，忽略:', data.device_ip, '当前选中:', selectedDeviceIp.value);
    }
    return;
  }
  
  // 处理批量性能数据
  if (data.type === 'performance_batch' && data.devices) {
    const targetDeviceData = data.devices.find(device => device.device_ip === selectedDeviceIp.value);
    if (targetDeviceData) {
      console.log('更新批量性能数据:', targetDeviceData);
      updateChartData(targetDeviceData);
      
      // 同时更新到设备store中
      deviceStore.updateDevicePerformance(targetDeviceData.device_ip, {
        cpu_usage: targetDeviceData.cpu_usage,
        memory_usage: targetDeviceData.memory_usage,
        disk_usage: targetDeviceData.disk_usage,
        load_average_1m: targetDeviceData.load_average_1m,
        npu_usage: targetDeviceData.npu_usage,
        system_temperature: targetDeviceData.system_temperature,
        npu_temperature: targetDeviceData.npu_temperature,
        npu_power: targetDeviceData.npu_power,
        memory_available: targetDeviceData.memory_available,
        memory_total: targetDeviceData.memory_total
      });
      
      // 强制触发响应式更新
      allMetrics.value = [...allMetrics.value];
    }
    return;
  }
}

// 开始监控
const startMonitoring = async () => {
  try {
    isLoading.value = true
    errorMessage.value = ''
    
    // 添加请求状态反馈
    console.log(`[PerformanceChartPopup] 正在启动性能监控: ${selectedDeviceIp.value}`)
    
    // 设置超时控制，避免长时间无响应
    const controller = new AbortController()
    const timeoutId = setTimeout(() => controller.abort(), 5000) // 5秒超时
    
    const response = await fetch('/api/devices/performance_control', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        ip_address: selectedDeviceIp.value,
        action: 'start'
      }),
      signal: controller.signal
    })
    
    clearTimeout(timeoutId)
    
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`)
    }
    
    const result = await response.json()
    if (result.success) {
      isMonitoring.value = true
      console.log(`性能监控已开始: ${selectedDeviceIp.value}`)
    } else {
      errorMessage.value = `启动失败: ${result.message}`
      console.error(`启动性能监控失败: ${result.message}`)
    }
  } catch (error) {
    if (error.name === 'AbortError') {
      errorMessage.value = '操作超时，请重试'
      console.error('启动性能监控超时:', error)
    } else {
      errorMessage.value = '启动失败，请检查网络连接'
      console.error('启动性能监控失败:', error)
    }
  } finally {
    isLoading.value = false
  }
}

// 停止监控
const stopMonitoring = async () => {
  try {
    isLoading.value = true
    errorMessage.value = ''
    
    // 添加请求状态反馈
    console.log(`[PerformanceChartPopup] 正在停止性能监控: ${selectedDeviceIp.value}`)
    
    // 设置超时控制，避免长时间无响应
    const controller = new AbortController()
    const timeoutId = setTimeout(() => controller.abort(), 5000) // 5秒超时
    
    const response = await fetch('/api/devices/performance_control', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        ip_address: selectedDeviceIp.value,
        action: 'stop'
      }),
      signal: controller.signal
    })
    
    clearTimeout(timeoutId)
    
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`)
    }
    
    const result = await response.json()
    if (result.success) {
      isMonitoring.value = false
      console.log(`性能监控已停止: ${selectedDeviceIp.value}`)
    } else {
      errorMessage.value = `停止失败: ${result.message}`
      console.error(`停止性能监控失败: ${result.message}`)
    }
  } catch (error) {
    if (error.name === 'AbortError') {
      errorMessage.value = '操作超时，请重试'
      console.error('停止性能监控超时:', error)
    } else {
      errorMessage.value = '停止失败，请检查网络连接'
      console.error('停止性能监控失败:', error)
    }
  } finally {
    isLoading.value = false
  }
}

// 判断是否是百分比类型的指标
const isPercentageMetric = (key) => {
  const percentageMetrics = ['cpu', 'memory', 'npu', 'disk', 'npu_memory'];
  return percentageMetrics.includes(key);
};

// 格式化指标值显示
const formatMetricValue = (metric) => {
  if (isPercentageMetric(metric.key)) {
    return `${Math.round(metric.value)}%`;
  } else if (metric.unit) {
    // 对于有单位的指标，显示数值和单位
    let displayValue = metric.value;
    
    // 特殊处理内存值，如果数值较大则转换为GB
    if (['memory_available', 'memory_total'].includes(metric.key)) {
      if (typeof metric.value === 'number' && metric.value > 1024) {
        displayValue = (metric.value / 1024).toFixed(2);
        return `${displayValue} GB`;
      } else {
        return `${metric.value} MB`;
      }
    }
    
    return `${displayValue} ${metric.unit || ''}`;
  } else {
    return metric.value;
  }
};

// 获取指标颜色
const getMetricColor = (value) => {
  if (value < 50) return '#67C23A'  // 绿色
  if (value < 80) return '#E6A23C'  // 黄色
  return '#F56C6C'  // 红色
}

// 获取指标标签类型
const getMetricTagType = (value) => {
  if (value < 50) return 'success'
  if (value < 80) return 'warning'
  return 'danger'
}

// 关闭对话框
const handleClose = () => {
  if (isMonitoring.value) {
    stopMonitoring()
  }
  visible.value = false
  emit('update:modelValue', false)
  emit('close')
}

// 监听props变化
watch(() => props.modelValue, (newValue) => {
  visible.value = newValue
  if (newValue && props.deviceIp) {
    selectedDeviceIp.value = props.deviceIp
  }
})

watch(() => props.deviceIp, (newIp) => {
  if (visible.value) {
    selectedDeviceIp.value = newIp
  }
})

// 监听设备store中的性能数据变化，以确保UI总是显示最新数据
watch(() => deviceStore.connectedDevices, () => {
  if (selectedDeviceIp.value) {
    const device = deviceStore.connectedDevices.find(d => d.ip_address === selectedDeviceIp.value);
    if (device && device.performance) {
      // 更新UI中的指标值
      const perf = device.performance;
      if (perf.cpu_usage !== undefined) updateMetricValue('cpu', Math.round(perf.cpu_usage));
      if (perf.memory_usage !== undefined) updateMetricValue('memory', Math.round(perf.memory_usage));
      if (perf.npu_usage !== undefined) updateMetricValue('npu', Math.round(perf.npu_usage));
      if (perf.disk_usage !== undefined) updateMetricValue('disk', Math.round(perf.disk_usage));
      if (perf.load_average_1m !== undefined) updateMetricValue('load_avg_1m', parseFloat(perf.load_average_1m.toFixed(2)));
      if (perf.system_temperature !== undefined) updateMetricValue('sys_temp', Math.round(perf.system_temperature));
      if (perf.npu_temperature !== undefined) updateMetricValue('npu_temp', Math.round(perf.npu_temperature));
      if (perf.npu_power !== undefined) updateMetricValue('npu_power', parseFloat(perf.npu_power.toFixed(2)));
      if (perf.memory_available !== undefined) updateMetricValue('memory_available', Math.round(perf.memory_available));
      if (perf.memory_total !== undefined) updateMetricValue('memory_total', Math.round(perf.memory_total));
      
      // 强制触发响应式更新
      allMetrics.value = [...allMetrics.value];
    }
  }
}, { deep: true })

// 组件挂载
onMounted(() => {
  initCharts()
  
  // 注册WebSocket消息处理器 - 使用唯一的处理器名称
  if (registerMessageHandler) {
    registerMessageHandler('performance_chart_popup', performanceDataHandler)
  }
  
  // 确保指标数据正确初始化
  allMetrics.value.forEach(metric => {
    if (metric.value === undefined || metric.value === null) {
      metric.value = 0;
    }
  });
})

// 组件卸载
onUnmounted(() => {
  // 销毁图表
  if (cpuChart.value) cpuChart.value.dispose()
  if (memoryChart.value) memoryChart.value.dispose()
  if (npuChart.value) npuChart.value.dispose()
  
  // 注销WebSocket消息处理器
  if (unregisterMessageHandler) {
    unregisterMessageHandler('performance_chart_popup')
  }
  
  // 停止监控
  if (isMonitoring.value) {
    stopMonitoring()
  }
})
</script>

<style scoped>
.chart-container {
  min-height: 500px;
}

.four-sections-container {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
  height: 100%;
}

.section-card {
  min-height: 150px;
  overflow-y: auto;
  border: none;
  border-radius: 6px;
  box-shadow: 0 1px 3px rgba(0,0,0,0.1);
}

.section-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.section-title {
  font-weight: bold;
  font-size: 16px;
  color: #303133;
}

.metric-grid {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.memory-content-wrapper {
  display: flex;
  flex-direction: column;
  flex: 1; /* 让其填充剩余空间 */
}

.memory-metrics {
  display: flex;
  flex-direction: column;
  gap: 8px;
  flex: 0 0 auto; /* 不伸缩，根据内容决定高度 */
  margin-bottom: 12px;
}

.metric-item {
  padding: 8px;
  border: none;
  border-radius: 4px;
  background-color: transparent;
}

.metric-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 6px;
}

.metric-title {
  font-weight: bold;
  font-size: 14px;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.non-percentage-value {
  display: flex;
  justify-content: center;
  align-items: center;
  height: 20px;
  font-size: 14px;
  color: #606266;
  margin-top: 6px;
}

.load-average-display {
  display: flex;
  justify-content: center;
  align-items: center;
  height: 20px;
  font-size: 14px;
  color: #606266;
  margin-top: 6px;
}

.chart-wrapper {
  display: flex;
  flex-direction: column;
  gap: 8px;
  margin-top: 12px;
  min-height: 0;
}

.chart {
  width: 100%;
  border: none;
  border-radius: 4px;
  background-color: transparent;
  min-height: 0;
  height: 100px;
}

.chart-title {
  font-size: 14px;
  font-weight: bold;
  margin-bottom: 8px;
  color: #303133;
}

.no-device-selected {
  text-align: center;
  padding: 40px 0;
  color: #909399;
}

.error-message {
  color: #F56C6C;
  margin-bottom: 10px;
  font-size: 14px;
  text-align: center;
}
</style>