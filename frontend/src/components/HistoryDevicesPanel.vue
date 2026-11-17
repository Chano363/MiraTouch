<template>
  <el-card v-if="historyDevices.length > 0" class="history-devices-card">
    <template #header>
      <div class="card-header">
        <div class="header-info">
          <span class="header-title">🕒 历史设备 ({{ historyDevices.length }})</span>
        </div>
        <div class="header-actions">
          <el-button size="small" @click="$emit('toggle-history-view')">
            {{ showDetailedHistory ? '简略' : '详细' }}
          </el-button>
          <el-button type="danger" size="small" @click="$emit('clear-history')">
            清空历史
          </el-button>
        </div>
      </div>
    </template>

    <div v-show="showHistory" class="history-content">
      <el-table 
        :data="historyDevices" 
        stripe 
        style="width: 100%"
        :row-class-name="getRowClass"
        fit="true"
      >
        <el-table-column prop="hostname" :label="showDetailedHistory ? '设备名称' : '设备'" :min-width="showDetailedHistory ? 180 : 220" resizable>
          <template #default="{ row }">
            <span>{{ row.hostname || row.ip || '未知设备' }}</span>
            <el-tag 
              v-if="row.type_display" 
              size="small" 
              :type="getDeviceTypeTagType(row.type)"
              style="margin-left: 8px;"
            >
              {{ row.type_display }}
            </el-tag>
          </template>
        </el-table-column>

        <el-table-column prop="ip" :label="showDetailedHistory ? 'IP地址' : 'IP'" :min-width="showDetailedHistory ? 140 : 180" resizable />

        <el-table-column v-if="showDetailedHistory" prop="mac" label="MAC地址" min-width="160" resizable />

        <el-table-column v-if="showDetailedHistory" prop="lastSeen" label="最后发现" min-width="160" resizable>
          <template #default="{ row }">
            {{ formatDate(row.lastSeen) }}
          </template>
        </el-table-column>

        <el-table-column label="操作" :min-width="showDetailedHistory ? 100 : 100" resizable>
          <template #default="{ row }">
            <el-button 
              type="primary" 
              size="small"
              @click="$emit('connect-to-device', row.ip)"
            >
              连接
            </el-button>
          </template>
        </el-table-column>
      </el-table>
    </div>
  </el-card>
</template>

<script setup>
import { defineProps, defineEmits } from 'vue'

const props = defineProps({
  historyDevices: {
    type: Array,
    required: true
  },
  showHistory: {
    type: Boolean,
    required: true
  },
  showDetailedHistory: {
    type: Boolean,
    required: true
  }
})

const emit = defineEmits([
  'toggle-history',
  'toggle-history-view',
  'clear-history',
  'connect-to-device'
])

const formatDate = (dateString) => {
  if (!dateString) return '从未';
  const date = new Date(dateString);
  return date.toLocaleString();
}

const getDeviceTypeTagType = (type) => {
  switch (type) {
    case 'vmware_virtual_machine':
      return 'primary';
    case 'huawei_atlas':
      return 'danger';
    case 'other':
      return 'info';
    default:
      return 'info';
  }
}

const getRowClass = ({ rowIndex }) => {
  if (rowIndex % 2 === 0) {
    return 'even-row';
  } else {
    return 'odd-row';
  }
}
</script>

<style scoped>
.history-devices-card {
  margin-top: 24px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-info {
  display: flex;
  align-items: center;
  gap: 12px;
}

.header-title {
  font-size: 1.1rem;
  font-weight: 600;
  color: #303133;
}

.header-actions {
  display: flex;
  gap: 8px;
}

.history-content {
  padding: 16px 0;
}

.even-row {
  background-color: #fafafa;
}

.odd-row {
  background-color: #ffffff;
}
</style>