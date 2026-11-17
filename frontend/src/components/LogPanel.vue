<template>
  <el-card class="log-panel-card">
    <template #header>
      <div class="card-header">
        <span class="header-title">📝 日志面板</span>
        <div class="header-actions">
          <el-dropdown>
            <el-button type="primary" size="small">
              导出日志
              <el-icon class="el-icon--right"><ArrowDown /></el-icon>
            </el-button>
            <template #dropdown>
              <el-dropdown-menu>
                <el-dropdown-item @click="exportLogs('json')">JSON格式</el-dropdown-item>
                <el-dropdown-item @click="exportLogs('text')">文本格式</el-dropdown-item>
                <el-dropdown-item @click="exportLogs('filtered')">仅导出过滤结果</el-dropdown-item>
              </el-dropdown-menu>
            </template>
          </el-dropdown>
          <el-button type="warning" size="small" @click="refreshLogs" style="margin-left: 8px;">刷新日志</el-button>
          <el-button type="danger" size="small" @click="clearLogs" style="margin-left: 8px;">清空日志</el-button>
        </div>
      </div>
    </template>
    
    <!-- 日志统计概览 -->
    <div class="log-stats" v-if="logs.length > 0">
      <div class="stat-item">
        <span class="stat-label">日志总数:</span>
        <span class="stat-value">{{ logs.length }}</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">错误数:</span>
        <span class="stat-value error">{{ errorCount }}</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">警告数:</span>
        <span class="stat-value warn">{{ warnCount }}</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">信息数:</span>
        <span class="stat-value info">{{ infoCount }}</span>
      </div>
      <div class="stat-item">
        <span class="stat-label">调试数:</span>
        <span class="stat-value debug">{{ debugCount }}</span>
      </div>
    </div>
    
    <!-- 过滤和搜索工具栏 -->
    <div class="log-toolbar">
      <div class="filter-section">
        <el-select 
          v-model="selectedLevel" 
          placeholder="按级别过滤" 
          size="small"
          class="level-filter"
          clearable
        >
          <el-option label="全部" value="" />
          <el-option label="DEBUG" value="DEBUG" />
          <el-option label="INFO" value="INFO" />
          <el-option label="WARN" value="WARN" />
          <el-option label="ERROR" value="ERROR" />
        </el-select>
        
        <el-select 
          v-model="selectedModule" 
          placeholder="按模块过滤" 
          size="small"
          class="module-filter"
          clearable
        >
          <el-option label="全部" value="" />
          <el-option 
            v-for="module in uniqueModules" 
            :key="module" 
            :label="module" 
            :value="module" 
          />
        </el-select>
        
        <el-select 
          v-model="selectedCategory" 
          placeholder="按分类过滤" 
          size="small"
          class="category-filter"
          clearable
        >
          <el-option label="全部" value="" />
          <el-option label="心跳" value="heartbeat" />
          <el-option label="性能" value="performance" />
          <el-option label="扫描" value="scan" />
          <el-option label="设备" value="device" />
          <el-option label="配置" value="config" />
          <el-option label="API" value="api" />
          <el-option label="WebSocket" value="websocket" />
          <el-option label="调试" value="debug" />
          <el-option label="系统" value="system" />
          <el-option label="安全" value="security" />
        </el-select>
      </div>
      
      <div class="search-section">
        <el-input
          v-model="searchKeyword"
          placeholder="搜索日志..."
          size="small"
          class="log-search"
          clearable
          @keyup.enter="performSearch"
        >
          <template #prefix>
            <el-icon><Search /></el-icon>
          </template>
          <template #append>
            <el-button size="small" @click="clearSearch">
              <el-icon><Close /></el-icon>
            </el-button>
          </template>
        </el-input>
      </div>
    </div>
    
    <div class="log-content">
      <el-empty v-if="filteredLogs.length === 0" description="暂无日志" />
      
      <div v-else class="log-window">
        <div class="log-list">
          <div 
            v-for="(item, index) in paginatedLogs" 
            :key="index"
            class="log-entry" 
            :class="getLogClass(item)"
          >
            <span class="log-timestamp">{{ item.timestamp }}</span>
            <span class="log-level-badge" :class="item.level.toLowerCase()">{{ item.level }}</span>
            <span class="log-module" v-if="item.module">{{ item.module }}</span>
            <span class="log-category" :class="item.category" v-if="item.category">{{ getCategoryLabel(item.category) }}</span>
            <span class="log-message">{{ item.message }}</span>
            <div class="log-actions" v-if="item.data">
              <el-button 
                size="small" 
                @click="toggleLogDetails(index)"
                text
              >
                {{ expandedLogs.includes(index) ? '收起详情' : '查看详情' }}
              </el-button>
            </div>
            <div v-if="expandedLogs.includes(index) && item.data" class="log-details">
              <pre class="log-data">{{ JSON.stringify(item.data, null, 2) }}</pre>
            </div>
          </div>
        </div>
        
        <!-- 分页控件 -->
        <div class="log-pagination" v-if="totalPages > 1">
          <el-pagination
            v-model:current-page="currentPage"
            v-model:page-size="pageSize"
            :page-sizes="[20, 50, 100, 200]"
            layout="total, sizes, prev, pager, next, jumper"
            :total="filteredLogs.length"
            @size-change="handleSizeChange"
            @current-change="handleCurrentChange"
            small
          />
        </div>
      </div>
    </div>
  </el-card>
</template>

<script setup>
import { ref, computed, watch } from 'vue'
import { ElCard, ElButton, ElEmpty, ElSelect, ElOption, ElInput, ElIcon, ElDropdown, ElDropdownMenu, ElDropdownItem, ElPagination } from 'element-plus'
import { Close, ArrowDown, Search } from '@element-plus/icons-vue'

const props = defineProps({
  logs: {
    type: Array,
    required: true
  }
})

const emit = defineEmits(['clear-logs', 'refresh-logs'])

// 响应式数据
const selectedLevel = ref('')
const selectedModule = ref('')
const selectedCategory = ref('')
const searchKeyword = ref('')
const expandedLogs = ref([])
const currentPage = ref(1)
const pageSize = ref(50)

// 计算属性
const filteredLogs = computed(() => {
  return props.logs.filter(log => {
    // 按级别过滤
    if (selectedLevel.value && log.level !== selectedLevel.value) {
      return false
    }
    
    // 按模块过滤
    if (selectedModule.value && log.module !== selectedModule.value) {
      return false
    }
    
    // 按分类过滤
    if (selectedCategory.value && log.category !== selectedCategory.value) {
      return false
    }
    
    // 按关键词搜索
    if (searchKeyword.value) {
      const keyword = searchKeyword.value.toLowerCase()
      return log.message.toLowerCase().includes(keyword) || 
             (log.module && log.module.toLowerCase().includes(keyword)) ||
             (log.category && log.category.toLowerCase().includes(keyword)) ||
             (log.data && JSON.stringify(log.data).toLowerCase().includes(keyword))
    }
    
    return true
  })
})

const uniqueModules = computed(() => {
  const modules = new Set()
  props.logs.forEach(log => {
    if (log.module) {
      modules.add(log.module)
    }
  })
  return Array.from(modules).sort()
})

// 分页相关计算属性
const totalPages = computed(() => {
  return Math.ceil(filteredLogs.value.length / pageSize.value)
})

const paginatedLogs = computed(() => {
  const start = (currentPage.value - 1) * pageSize.value
  const end = start + pageSize.value
  return filteredLogs.value.slice(start, end)
})

// 日志统计计算属性
const errorCount = computed(() => {
  return props.logs.filter(log => log.level === 'ERROR').length
})

const warnCount = computed(() => {
  return props.logs.filter(log => log.level === 'WARN').length
})

const infoCount = computed(() => {
  return props.logs.filter(log => log.level === 'INFO').length
})

const debugCount = computed(() => {
  return props.logs.filter(log => log.level === 'DEBUG').length
})

// 方法
const clearLogs = () => {
  emit('clear-logs')
  expandedLogs.value = []
  currentPage.value = 1
}

const refreshLogs = () => {
  emit('refresh-logs')
  expandedLogs.value = []
  currentPage.value = 1
}

const getLogClass = (log) => {
  if (!log || !log.level) {
    return 'log-info'
  }
  return `log-${log.level.toLowerCase()}`
}

const getCategoryLabel = (category) => {
  const categoryMap = {
    heartbeat: '心跳',
    performance: '性能',
    scan: '扫描',
    device: '设备',
    config: '配置',
    api: 'API',
    websocket: 'WebSocket',
    debug: '调试',
    system: '系统',
    security: '安全'
  }
  return categoryMap[category] || category
}

const toggleLogDetails = (index) => {
  const logIndex = expandedLogs.value.indexOf(index)
  if (logIndex > -1) {
    expandedLogs.value.splice(logIndex, 1)
  } else {
    expandedLogs.value.push(index)
  }
}

const clearSearch = () => {
  searchKeyword.value = ''
  currentPage.value = 1
}

const performSearch = () => {
  currentPage.value = 1
}

const exportLogs = (format = 'json') => {
  let logData, mimeType, extension
  
  if (format === 'filtered') {
    logData = JSON.stringify(filteredLogs.value, null, 2)
    mimeType = 'application/json'
    extension = 'json'
  } else if (format === 'text') {
    logData = filteredLogs.value.map(log => {
      const parts = [
        `[${log.timestamp}]`,
        `[${log.level}]`,
        log.module ? `[${log.module}]` : '',
        log.category ? `[${getCategoryLabel(log.category)}]` : '',
        log.message
      ].filter(Boolean).join(' ')
      return parts
    }).join('\n')
    mimeType = 'text/plain'
    extension = 'txt'
  } else {
    logData = JSON.stringify(props.logs, null, 2)
    mimeType = 'application/json'
    extension = 'json'
  }
  
  const blob = new Blob([logData], { type: mimeType })
  const url = URL.createObjectURL(blob)
  const a = document.createElement('a')
  a.href = url
  a.download = `logs_${new Date().toISOString().slice(0, 19).replace(/[:T]/g, '-')}.${extension}`
  a.click()
  URL.revokeObjectURL(url)
}

// 分页相关方法
const handleSizeChange = (size) => {
  pageSize.value = size
  currentPage.value = 1
}

const handleCurrentChange = (page) => {
  currentPage.value = page
  expandedLogs.value = []
}

// 监听过滤条件变化，重置分页
watch([selectedLevel, selectedModule, selectedCategory, searchKeyword], () => {
  currentPage.value = 1
  expandedLogs.value = []
})

// 监听日志变化，保持分页状态
watch(() => props.logs.length, (newLength, oldLength) => {
  if (newLength > oldLength && currentPage.value > 1) {
    const totalPages = Math.ceil(newLength / pageSize.value)
    if (currentPage.value > totalPages) {
      currentPage.value = totalPages
    }
  }
})
</script>

<style scoped>
.log-panel-card {
  margin-top: 24px;
  box-shadow: 0 2px 12px 0 rgba(0, 0, 0, 0.1) !important;
  border-radius: 8px !important;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.header-title {
  font-size: 1.2rem;
  font-weight: 600;
  color: #303133;
}

.header-actions {
  display: flex;
  gap: 8px;
}

/* 日志统计概览 */
.log-stats {
  display: flex;
  gap: 24px;
  padding: 16px;
  background-color: #f5f7fa;
  border-radius: 6px;
  margin-bottom: 16px;
  flex-wrap: wrap;
}

.stat-item {
  display: flex;
  align-items: center;
  gap: 8px;
}

.stat-label {
  font-size: 12px;
  color: #909399;
  white-space: nowrap;
}

.stat-value {
  font-size: 14px;
  font-weight: 600;
  color: #303133;
}

.stat-value.error {
  color: #f56c6c;
}

.stat-value.warn {
  color: #e6a23c;
}

.stat-value.info {
  color: #409eff;
}

.stat-value.debug {
  color: #909399;
}

.log-toolbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 16px;
  padding: 12px;
  background-color: #f5f7fa;
  border-radius: 4px;
  flex-wrap: wrap;
  gap: 12px;
}

.filter-section {
  display: flex;
  gap: 12px;
  align-items: center;
  flex-wrap: wrap;
}

.level-filter {
  min-width: 120px;
}

.module-filter {
  min-width: 150px;
}

.search-section {
  flex: 1;
  min-width: 200px;
  max-width: 300px;
}

.log-search {
  width: 100%;
}

.log-content {
  min-height: 300px;
  max-height: 600px;
  overflow-y: auto;
}

.log-window {
  font-family: 'Courier New', monospace;
  font-size: 14px;
  line-height: 1.6;
  padding: 16px;
  background-color: #f8f9fa;
  color: #303133;
  border-radius: 6px;
  border: 1px solid #e4e7ed;
  max-height: 500px;
}

.log-list {
  width: 100%;
  overflow-y: auto;
  max-height: 400px;
}

.log-entry {
  margin-bottom: 8px;
  padding: 8px 12px;
  border-radius: 4px;
  transition: all 0.2s ease;
  border-left: 4px solid transparent;
  display: flex;
  align-items: center;
  flex-wrap: wrap;
  gap: 8px;
}

.log-entry:hover {
  background-color: rgba(0, 0, 0, 0.02);
  box-shadow: 0 1px 3px rgba(0, 0, 0, 0.1);
}

.log-timestamp {
  color: #909399;
  font-size: 12px;
  white-space: nowrap;
}

.log-level-badge {
  font-size: 10px;
  font-weight: 600;
  padding: 2px 6px;
  border-radius: 10px;
  white-space: nowrap;
}

.log-level-badge.error {
  background-color: rgba(245, 108, 108, 0.2);
  color: #f56c6c;
}

.log-level-badge.warn {
  background-color: rgba(230, 162, 60, 0.2);
  color: #e6a23c;
}

.log-level-badge.info {
  background-color: rgba(64, 158, 255, 0.2);
  color: #409eff;
}

.log-level-badge.debug {
  background-color: rgba(144, 147, 153, 0.2);
  color: #909399;
}

.log-module {
  color: #67c23a;
  font-size: 12px;
  font-weight: 600;
  background-color: rgba(103, 194, 58, 0.1);
  padding: 2px 8px;
  border-radius: 10px;
  white-space: nowrap;
}

.log-category {
  font-size: 12px;
  font-weight: 600;
  padding: 2px 8px;
  border-radius: 10px;
  white-space: nowrap;
}

.log-category.heartbeat {
  background-color: rgba(144, 147, 153, 0.1);
  color: #909399;
}

.log-category.performance {
  background-color: rgba(103, 194, 58, 0.1);
  color: #67c23a;
}

.log-category.scan {
  background-color: rgba(64, 158, 255, 0.1);
  color: #409eff;
}

.log-category.device {
  background-color: rgba(230, 162, 60, 0.1);
  color: #e6a23c;
}

.log-category.config {
  background-color: rgba(139, 195, 74, 0.1);
  color: #8bc34a;
}

.log-category.api {
  background-color: rgba(255, 152, 0, 0.1);
  color: #ff9800;
}

.log-category.websocket {
  background-color: rgba(156, 39, 176, 0.1);
  color: #9c27b0;
}

.log-category.debug {
  background-color: rgba(76, 175, 80, 0.1);
  color: #4caf50;
}

.log-category.system {
  background-color: rgba(102, 126, 234, 0.1);
  color: #667eea;
}

.log-category.security {
  background-color: rgba(237, 85, 101, 0.1);
  color: #ed5565;
}

.log-message {
  flex: 1;
  min-width: 200px;
  word-break: break-word;
}

.log-actions {
  margin-top: 4px;
  margin-left: 0;
  width: 100%;
  padding-left: 200px;
}

.log-details {
  margin-top: 8px;
  margin-left: 0;
  padding: 12px;
  background-color: #f0f2f5;
  border-radius: 4px;
  border: 1px solid #dcdfe6;
  width: 100%;
  padding-left: 200px;
}

.log-data {
  margin: 0;
  font-size: 12px;
  color: #606266;
  white-space: pre-wrap;
  word-break: break-all;
}

/* 日志级别样式 */
.log-debug {
  border-left-color: #909399;
  color: #909399;
}

.log-info {
  border-left-color: #409eff;
  color: #303133;
}

.log-warn {
  border-left-color: #e6a23c;
  color: #303133;
}

.log-error {
  border-left-color: #f56c6c;
  color: #303133;
}

/* 分页控件 */
.log-pagination {
  margin-top: 16px;
  display: flex;
  justify-content: center;
}

/* 滚动条样式 */
.log-window::-webkit-scrollbar,
.log-list::-webkit-scrollbar {
  width: 8px;
}

.log-window::-webkit-scrollbar-track,
.log-list::-webkit-scrollbar-track {
  background: #f1f1f1;
  border-radius: 4px;
}

.log-window::-webkit-scrollbar-thumb,
.log-list::-webkit-scrollbar-thumb {
  background: #c1c1c1;
  border-radius: 4px;
}

.log-window::-webkit-scrollbar-thumb:hover,
.log-list::-webkit-scrollbar-thumb:hover {
  background: #a8a8a8;
}

/* 响应式设计 */
@media (max-width: 768px) {
  .log-toolbar {
    flex-direction: column;
    align-items: stretch;
  }
  
  .filter-section {
    justify-content: space-between;
  }
  
  .level-filter,
  .module-filter {
    flex: 1;
  }
  
  .log-content {
    max-height: 400px;
  }
  
  .log-stats {
    gap: 12px;
    padding: 12px;
  }
  
  .stat-item {
    flex: 1;
    min-width: 100px;
  }
  
  .log-entry {
    padding: 6px 8px;
    flex-direction: column;
    align-items: flex-start;
    gap: 4px;
  }
  
  .log-timestamp {
    display: block;
    margin-bottom: 4px;
  }
  
  .log-actions,
  .log-details {
    margin-left: 0;
    padding-left: 0;
  }
  
  .log-message {
    min-width: 100%;
  }
  
  .log-list {
    max-height: 300px;
  }
}

/* 深色模式适配 */
@media (prefers-color-scheme: dark) {
  .log-panel-card {
    background-color: #1f2937 !important;
    border-color: #374151 !important;
  }
  
  .header-title {
    color: #f3f4f6;
  }
  
  .log-stats {
    background-color: #111827;
  }
  
  .stat-label {
    color: #9ca3af;
  }
  
  .log-toolbar {
    background-color: #111827;
  }
  
  .log-window {
    background-color: #111827;
    color: #f3f4f6;
    border-color: #374151;
  }
  
  .log-entry:hover {
    background-color: rgba(255, 255, 255, 0.05);
  }
  
  .log-details {
    background-color: #1f2937;
    border-color: #374151;
  }
  
  .log-data {
    color: #d1d5db;
  }
}
</style>