<template>
  <div class="connection-status">
    <el-card class="status-card">
      <template #header>
        <div class="card-header">
          <div class="status-info">
            <h3 class="status-title">服务器状态</h3>
            <p class="status-desc">后端服务运行在: {{ apiUrl }}</p>
          </div>
          <div class="status-tags">
            <el-tag :type="serverConnected ? 'success' : 'danger'" size="large">
              {{ serverConnected ? '已连接' : '未连接' }}
            </el-tag>
            <el-tag :type="wsConnected ? 'success' : 'danger'" size="large" style="margin-left: 8px;">
              WebSocket {{ wsConnected ? '已连接' : '未连接' }}
            </el-tag>
          </div>
        </div>
      </template>
    </el-card>
  </div>
</template>

<script>
import { defineComponent } from 'vue'
import { ElCard, ElTag } from 'element-plus'

export default defineComponent({
  name: 'ConnectionStatus',
  components: {
    ElCard,
    ElTag
  },
  props: {
    serverConnected: {
      type: Boolean,
      required: true
    },
    wsConnected: {
      type: Boolean,
      required: true
    },
    apiUrl: {
      type: String,
      required: true
    }
  }
})
</script>

<style scoped>
.connection-status {
  display: inline-block;
  vertical-align: middle;
}

.status-card {
  margin-bottom: 24px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.status-info {
  flex: 1;
}

.status-title {
  margin: 0;
  font-size: 1.2rem;
  font-weight: 600;
  color: #303133;
}

.status-desc {
  margin: 4px 0 0 0;
  font-size: 0.9rem;
  color: #909399;
}

.status-tags {
  display: flex;
  align-items: center;
}
</style>