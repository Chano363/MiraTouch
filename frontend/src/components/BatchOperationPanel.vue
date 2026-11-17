<template>
  <div class="batch-operation-panel">
    <div class="panel-header">
      <h4>⚡ 批量操作</h4>
      <div class="operation-controls">
        <button class="btn btn-primary modern-button" @click="$emit('select-all-devices')">
          全选
        </button>
        <button class="btn btn-secondary modern-button" @click="$emit('clear-selection')">
          清除选择
        </button>
        <button class="btn btn-info modern-button" @click="$emit('toggle-performance-monitoring')">
          {{ isMonitoringAll ? '停止全部监控' : '开启全部监控' }}
        </button>
      </div>
    </div>

    <div class="batch-controls">
      <div class="command-input-group">
        <input 
          v-model="batchCommand" 
          placeholder="输入要在选中设备上执行的命令" 
          class="modern-input"
          @keyup.enter="$emit('execute-command')"
        />
        <button 
          class="btn btn-success modern-button" 
          @click="$emit('execute-command')"
          :disabled="!batchCommand || selectedDevices.length === 0"
        >
          执行命令
        </button>
      </div>

      <div class="execution-results" v-if="executionResults.length > 0">
        <h5>执行结果:</h5>
        <div 
          v-for="result in executionResults" 
          :key="result.id"
          :class="['result-item', result.success ? 'success' : 'error']"
        >
          <span class="ip">{{ result.ip }}</span>
          <span class="status">{{ result.message }}</span>
        </div>
      </div>
    </div>
  </div>
</template>

<script>
import { ref, computed } from 'vue'

export default {
  name: 'BatchOperationPanel',
  props: {
    selectedDevices: {
      type: Array,
      required: true
    },
    connectedDevices: {
      type: Array,
      required: true
    },
    monitoringDevices: {
      type: Array,
      required: true
    },
    executionResults: {
      type: Array,
      default: () => []
    }
  },
  emits: [
    'update:batchCommand',
    'select-all-devices',
    'clear-selection',
    'toggle-performance-monitoring',
    'execute-command'
  ],
  setup(props) {
    const batchCommand = ref('')
    
    // 同步内部状态到父组件
    batchCommand.value = ''
    
    const isMonitoringAll = computed(() => {
      return props.connectedDevices.length > 0 && 
             props.connectedDevices.every(device => 
               props.monitoringDevices.includes(device.ip_address)
             )
    })
    
    return {
      batchCommand,
      isMonitoringAll
    }
  }
}
</script>

<style scoped>
.batch-operation-panel {
  background: #ffffff;
  border-radius: 8px;
  padding: 1.25rem;
  margin-bottom: 1.5rem;
  border: 1px solid #e0e6ed;
  color: #2c3e50;
}

.panel-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 1rem;
  padding-bottom: 0.75rem;
  border-bottom: 1px solid #e0e6ed;
}

.operation-controls {
  display: flex;
  gap: 0.5rem;
}

.batch-controls {
  display: flex;
  flex-direction: column;
  gap: 1rem;
}

.command-input-group {
  display: flex;
  gap: 0.75rem;
  align-items: center;
}

.modern-input {
  flex: 1;
  padding: 0.8rem 1rem;
  border: 1px solid #ddd;
  border-radius: 4px;
  font-size: 0.9rem;
}

.modern-input:focus {
  outline: none;
  border-color: #3498db;
  box-shadow: 0 0 0 3px rgba(52, 152, 219, 0.2);
}

.modern-button {
  padding: 0.6rem 1.2rem !important;
  border-radius: 8px !important;
  font-weight: 500 !important;
  transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1) !important;
  cursor: pointer !important;
}

.execution-results {
  background: #f8f9fa;
  border-radius: 4px;
  padding: 1rem;
  border: 1px solid #e0e6ed;
}

.result-item {
  display: flex;
  justify-content: space-between;
  padding: 0.5rem 0;
  border-bottom: 1px solid #e0e6ed;
}

.result-item:last-child {
  border-bottom: none;
}

.result-item.success .ip {
  color: #2ecc71;
}

.result-item.error .ip {
  color: #e74c3c;
}

.result-item.success .status {
  color: #2ecc71;
}

.result-item.error .status {
  color: #e74c3c;
}

@media (max-width: 768px) {
  .panel-header {
    flex-direction: column;
    align-items: flex-start;
    gap: 1rem;
  }
  
  .operation-controls {
    width: 100%;
    justify-content: flex-end;
  }
  
  .command-input-group {
    flex-direction: column;
    align-items: stretch;
  }
  
  .modern-input {
    width: 100%;
  }
}
</style>