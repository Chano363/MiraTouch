<template>
  <div id="app">
    <el-container class="main-container">
      <el-header class="app-header">
        <div class="header-content">
          <div class="logo-section">
            <h1 class="app-title">MiraTouch</h1>
            <p class="app-subtitle">华为升腾开发板远程调试系统</p>
          </div>
          <div class="connection-status-simple">
            <div class="status-text">{{ apiUrl }}</div>
            <div class="status-tags">
              <el-tag :type="serverConnected ? 'success' : 'danger'" size="small">
                后端 {{ serverConnected ? '已连接' : '未连接' }}
              </el-tag>
              <el-tag :type="wsConnected ? 'success' : 'danger'" size="small" style="margin-left: 8px;">
                WS {{ wsConnected ? '已连接' : '未连接' }}
              </el-tag>
            </div>
          </div>
        </div>
      </el-header>

      <el-main class="app-main">
        <div class="main-content-wrapper">
          <!-- 统一的设备管理界面，整合了设备扫描和多设备管理功能 -->
          <div class="vertical-layout">
            <!-- 多设备管理界面（整合了设备扫描、设备列表、参数配置等功能） -->
            <el-card class="section-card">
              <template #header>
                <div class="card-header">
                  <span class="card-title">设备管理</span>
                </div>
              </template>
              <MultiDeviceManager :websocket="websocket" />
            </el-card>
            
            
          </div>
        </div>
      </el-main>
    </el-container>
  </div>
</template>

<script>
import { ref, onMounted, onUnmounted, provide } from 'vue'
import { ElContainer, ElHeader, ElMain, ElCard, ElTag } from 'element-plus'
import ConnectionStatus from './components/ConnectionStatus.vue'
import MultiDeviceManager from './components/MultiDeviceManager.vue'
import { useWebSocket } from './composables/useWebSocket'
import { useDeviceManager } from './composables/useDeviceManager'

export default {
  name: 'App',
  components: {
    ConnectionStatus,
    MultiDeviceManager,
    ElContainer,
    ElHeader,
    ElMain,
    ElCard,
    ElTag
  },
  setup() {
    const apiUrl = ref(`${window.location.protocol}//${window.location.hostname}:8082`)
    const serverConnected = ref(false)

    // 使用 composables
    const {
      wsConnected,
      websocket,
      logs,
      addLog,
      connectWebSocket,
      closeWebSocket,
      setScanProgressCallback,
      registerMessageHandler,
      unregisterMessageHandler,
      sendMessage
    } = useWebSocket(apiUrl.value)
    
    // 提供WebSocket实例给子组件
    provide('websocket', {
      registerMessageHandler,
      unregisterMessageHandler,
      sendMessage,
      wsConnected,
      websocket
    })

    const {
      devices,
      selectedDevice,
      checkServerConnection
    } = useDeviceManager(apiUrl.value, addLog)

    const checkServerConnectionWrapper = async () => {
      serverConnected.value = await checkServerConnection()
    }

    onMounted(() => {
      console.log('App.vue mounted, 开始初始化...')
      checkServerConnectionWrapper()
      connectWebSocket()

      // 暴露 WebSocket 实例到全局，便于调试
      setTimeout(() => {
        if (websocket.value) {
          window.__debugWebSocket = websocket.value;
          console.log('WebSocket 实例已暴露到 window.__debugWebSocket');
        }
      }, 500)
    })

    onUnmounted(() => {
      closeWebSocket()
    })


    
    // 传递原始的websocket ref和相关方法给子组件
    const websocketRef = websocket

    return {
      apiUrl,
      serverConnected,
      wsConnected,
      websocket: websocketRef,
      devices,
      selectedDevice
    }
  }
}
</script>

<style scoped>
#app {
  min-height: 100vh;
  position: relative;
  overflow-x: hidden;
  font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, Oxygen, Ubuntu, Cantarell, 'Open Sans', 'Helvetica Neue', sans-serif;
  background: #f8fafc;
}

.main-container {
  min-height: 100vh;
}

.app-header {
  background: linear-gradient(135deg, #2c3e50 0%, #34495e 100%) !important;
  padding: 0 24px !important;
  color: white;
  position: sticky;
  top: 0;
  z-index: 100;
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
  min-height: 100px;
}

.header-content {
  width: 100%;
  display: grid;
  grid-template-columns: 1fr auto;
  align-items: center;
  gap: 16px;
  padding: 16px 0;
  min-height: 100px;
}

.logo-section {
  display: flex;
  flex-direction: column;
  align-items: flex-start;
  text-align: left;
  gap: 0.25rem;
  justify-self: start;
  min-width: 300px;
}

.app-title {
  margin: 0;
  font-size: 2rem;
  font-weight: 700;
  letter-spacing: -0.025em;
  color: white;
  text-shadow: 0 2px 4px rgba(0, 0, 0, 0.3);
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.app-subtitle {
  margin: 0;
  font-size: 0.9rem;
  opacity: 0.9;
  font-weight: 500;
  color: white;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
}

.connection-status-simple {
  display: flex;
  flex-direction: column;
  align-items: flex-end;
  gap: 8px;
  justify-self: end;
  min-width: 250px;
}

.status-text {
  color: rgba(255, 255, 255, 0.85);
  font-size: 0.85rem;
  white-space: nowrap;
  overflow: hidden;
  text-overflow: ellipsis;
  max-width: 200px;
  text-align: right;
}

.status-tags {
  display: flex;
  gap: 8px;
  flex-wrap: nowrap;
}

.connection-status-wrapper {
  flex-shrink: 0;
  display: flex;
  align-items: center;
}

.app-main {
  padding: 24px !important;
  background: #f8fafc;
}

.main-content-wrapper {
  max-width: 1400px;
  margin: 0 auto;
  width: 100%;
}

.vertical-layout {
  display: flex;
  flex-direction: column;
  gap: 24px;
}

.section-card {
  margin-bottom: 24px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.card-title {
  font-size: 1.2rem;
  font-weight: 600;
  color: #303133;
}

/* 响应式设计 */
@media (max-width: 768px) {
  .app-header {
    padding: 0 12px !important;
  }
  
  .logo-section {
    align-items: center;
    text-align: center;
  }
  
  .app-title {
    font-size: 1.5rem;
  }
  
  .header-content {
    flex-direction: column;
    gap: 12px;
  }
}

/* 滚动条样式 */
::-webkit-scrollbar {
  width: 8px;
}

::-webkit-scrollbar-track {
  background: #f1f5f9;
  border-radius: 4px;
}

::-webkit-scrollbar-thumb {
  background: #cbd5e1;
  border-radius: 4px;
  transition: background 0.2s ease;
}

::-webkit-scrollbar-thumb:hover {
  background: #94a3b8;
}
</style>