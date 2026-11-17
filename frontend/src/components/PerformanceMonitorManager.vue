<template>
  <!-- 性能监测管理组件，处理所有性能监测相关的复杂逻辑 -->
  <div style="display: none;">
    <!-- 这个组件在后台运行，不显示任何UI -->
  </div>
</template>

<script setup>
import { onMounted, onUnmounted } from 'vue';
import { useWebSocket } from '../composables/useWebSocket';

// WebSocket实例
const wsInstance = useWebSocket(`${window.location.protocol}//${window.location.hostname}:8082`);
const { registerMessageHandler, unregisterMessageHandler } = wsInstance;

// 性能数据处理器
const performanceDataHandler = (data) => {
  // 检查消息类型，只处理真正的性能数据
  if (data.type === 'config_status') {
    // 这是配置状态消息，不是性能数据
    console.log('[PerformanceMonitorManager] 接收到配置状态消息:', data);
    return;
  } else if (data.type === 'scan_progress') {
    // 这是设备扫描进度消息，不是性能数据
    return;
  } else if (data.type !== 'performance_data' && !data.device_ip) {
    // 不是性能数据消息，跳过处理
    return;
  }
  
  // 处理性能数据
  console.log('[PerformanceMonitorManager] 接收到性能数据:', data);
  
  // 这里可以处理从WebSocket接收到的性能数据
  // 并更新全局状态或触发其他操作
};

// 注册性能数据处理器
if (registerMessageHandler) {
  registerMessageHandler('performance_data', performanceDataHandler);
}

// 性能监控控制函数
const controlPerformanceMonitoring = async (ip, action) => {
  try {
    // 添加请求状态反馈
    console.log(`[PerformanceMonitorManager] 正在${action === 'start' ? '启动' : '停止'}性能监控:`, ip);
    
    // 设置超时控制，避免长时间无响应
    const controller = new AbortController();
    const timeoutId = setTimeout(() => controller.abort(), 5000); // 5秒超时
    
    const response = await fetch('/api/devices/performance_control', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({
        ip_address: ip,
        action: action // 'start' 或 'stop'
      }),
      signal: controller.signal
    });
    
    clearTimeout(timeoutId);
    
    if (!response.ok) {
      throw new Error(`HTTP error! status: ${response.status}`);
    }
    
    const result = await response.json();
    
    if (!result.success) {
      console.error(`性能监控${action === 'start' ? '启动' : '停止'}失败:`, result.message);
      return { success: false, message: result.message };
    } else {
      console.log(`性能监控${action === 'start' ? '启动' : '停止'}成功:`, ip);
      return { success: true, message: `性能监控${action === 'start' ? '启动' : '停止'}成功` };
    }
  } catch (error) {
    if (error.name === 'AbortError') {
      console.error('性能监控控制命令超时:', error);
      return { success: false, message: '操作超时，请重试' };
    }
    console.error('发送性能监控控制命令失败:', error);
    return { success: false, message: '操作失败，请检查网络连接' };
  }
};

// 启动性能监控
const startMonitoring = async (ip) => {
  return await controlPerformanceMonitoring(ip, 'start');
};

// 停止性能监控
const stopMonitoring = async (ip) => {
  return await controlPerformanceMonitoring(ip, 'stop');
};

// 组件卸载时注销处理器
onUnmounted(() => {
  if (unregisterMessageHandler) {
    unregisterMessageHandler('performance_data');
  }
});

// 提供方法供其他组件调用
defineExpose({
  startMonitoring,
  stopMonitoring
});

onMounted(() => {
  console.log('[PerformanceMonitorManager] 性能监测管理器已启动');
});
</script>

<style scoped>
/* 不需要样式 */
</style>