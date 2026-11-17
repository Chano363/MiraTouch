<template>
  <el-card class="device-list-card">
    <template #header>
      <div class="card-header">
        <div class="header-info">
          <span class="header-title">📋 已连接设备 ({{ connectedDevices.length }})</span>
          <div class="device-stats">
            <el-statistic title="在线设备" :value="onlineCount" />
            <el-statistic title="已连接" :value="connectedCount" />
          </div>
        </div>
      </div>
    </template>

    <div v-if="connectedDevices.length === 0" class="empty-state">
      <el-empty description="暂无已连接设备" :image-size="100" />
      <p>请先使用设备扫描功能发现设备并手动连接</p>
    </div>

    <el-table 
      v-if="connectedDevices.length > 0"
      :data="connectedDevices" 
      stripe 
      style="width: 100%"
      row-key="ip_address"
      :row-class-name="tableRowClassName"
      @selection-change="handleSelectionChange"
    >
      <el-table-column type="selection" />
      
      <el-table-column prop="hostname" label="设备名称" width="200">
        <template #default="{ row }">
          <div class="device-name-cell">
            <el-icon><Monitor /></el-icon>
            <span>{{ row.hostname || row.ip_address }}</span>
          </div>
        </template>
      </el-table-column>

      <el-table-column prop="ip_address" label="IP地址" width="150" />

      <el-table-column prop="mac_address" label="MAC地址" width="180" />

      <el-table-column label="设备类型" width="120">
        <template #default="{ row }">
          <el-tag v-if="row.device_type_display" size="small">
            {{ row.device_type_display }}
          </el-tag>
          <span v-else>未知</span>
        </template>
      </el-table-column>

      <el-table-column label="状态" width="100">
        <template #default="{ row }">
          <el-tag :type="getStatusTagType(row)" size="small">
            {{ getStatusText(row) }}
          </el-tag>
        </template>
      </el-table-column>

      <el-table-column label="性能监控" width="200">
        <template #default="{ row }">
          <div class="performance-indicators">
            <div class="perf-item">
              <span class="perf-label">CPU:</span>
              <el-progress 
                :percentage="row.performance?.cpu_usage || 0" 
                :color="getPerformanceColor(row.performance?.cpu_usage || 0)"
                :stroke-width="8"
                status="success"
              />
            </div>
            <div class="perf-item">
              <span class="perf-label">内存:</span>
              <el-progress 
                :percentage="row.performance?.memory_usage || 0" 
                :color="getPerformanceColor(row.performance?.memory_usage || 0)"
                :stroke-width="8"
                status="success"
              />
            </div>
          </div>
        </template>
      </el-table-column>

      <el-table-column label="操作" width="320">
        <template #default="{ row }">
          <div class="action-buttons">
            <el-button 
              type="danger" 
              size="small"
              :disabled="!row.is_connected"
              @click="$emit('disconnect-device', row.ip_address)"
              :icon="CircleCloseFilled"
            >
              断开
          </el-button>
          <el-button 
            :type="monitoringDevices.includes(row.ip_address) ? 'info' : 'primary'" 
            size="small"
            :disabled="!row.is_connected"
            @click="$emit('open-performance-popup', row.ip_address)"
            :icon="Monitor"
          >
            查看监控
          </el-button>
          <el-button 
            type="warning" 
            size="small"
            :disabled="!row.is_connected"
            @click="$emit('check-model-status', row.ip_address)"
            :icon="Document"
          >
            模型状态
          </el-button>
          <el-button 
            type="primary" 
            size="small"
            :disabled="!row.is_connected"
            @click="$emit('open-config-manager', row.ip_address)"
            :icon="Setting"
          >
            配置管理
          </el-button>
          </div>
        </template>
      </el-table-column>
    </el-table>
  </el-card>
</template>

<script setup>
import { computed } from 'vue'
import { Monitor, Setting, Document, CircleCloseFilled, Histogram } from '@element-plus/icons-vue'
import { useDeviceStore } from '../stores/deviceStore'

const deviceStore = useDeviceStore()

const props = defineProps({
  connectedDevices: {
    type: Array,
    required: true
  },
  selectedDevices: {
    type: Array,
    required: true
  },
  monitoringDevices: {
    type: Array,
    required: true
  }
})

const emit = defineEmits([
  'toggle-device-selection',
  'disconnect-device',
  'toggle-performance',
  'check-model-status',
  'open-performance-chart',
  'open-performance-popup',
  'open-config-manager'
])

const onlineCount = computed(() => {
  return props.connectedDevices.filter(device => device.is_online).length
})

const connectedCount = computed(() => {
  return props.connectedDevices.filter(device => device.is_connected).length
})

const tableRowClassName = ({ row }) => {
  if (row.is_connected) return 'connected-row'
  if (row.is_online) return 'online-row'
  return 'offline-row'
}

const getStatusTagType = (device) => {
  if (device.is_connected && device.is_online) return 'success'
  if (device.is_online) return 'warning'
  return 'info'
}

const getStatusText = (device) => {
  if (device.is_connected && device.is_online) return '已连接'
  if (device.is_online) return '在线'
  return '离线'
}

const getPerformanceColor = (value) => {
  if (value < 50) return '#67C23A' // 绿色
  if (value < 80) return '#E6A23C' // 黄色
  return '#F56C6C' // 红色
}

const handleSelectionChange = (selection) => {
  const selectedIPs = selection.map(item => item.ip_address)
  deviceStore.setSelectedDevices(selectedIPs)
  emit('toggle-device-selection', selectedIPs)
}
</script>

<style scoped>
.device-list-card {
  margin-bottom: 24px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-info {
  display: flex;
  align-items: center;
  gap: 24px;
}

.header-title {
  font-size: 1.2rem;
  font-weight: 600;
  color: #303133;
}

.device-stats {
  display: flex;
  gap: 24px;
}

.empty-state {
  text-align: center;
  padding: 40px 0;
}

.empty-state p {
  color: #909399;
  margin-top: 16px;
}

.device-name-cell {
  display: flex;
  align-items: center;
  gap: 8px;
}

.performance-indicators {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.perf-item {
  display: flex;
  align-items: center;
  gap: 8px;
}

.perf-label {
  font-size: 0.8rem;
  color: #909399;
  white-space: nowrap;
}

.action-buttons {
  display: flex;
  gap: 8px;
  flex-wrap: wrap;
  justify-content: center;
}

.connected-row {
  background-color: #f0f9ff !important;
}

.online-row {
  background-color: #fff7e6 !important;
}
</style>