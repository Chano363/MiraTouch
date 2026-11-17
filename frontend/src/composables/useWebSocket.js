// useWebSocket.js
import { ref } from 'vue'
import Logger, { setLogPanelCallback } from '../utils/logger'

export function useWebSocket(options = {}) {
  const wsConnected = ref(false)
  const websocket = ref(null)
  const logs = ref([])

  let isConnecting = false
  let reconnectTimer = null
  let onScanProgress = null
  let connectionId = 0 // 用于跟踪当前连接
  let heartbeatTimer = null // 心跳定时器
  
  // 创建日志实例
  const logger = new Logger('WebSocket', {
    logLevel: options.logLevel || 3, // ERROR级别，只记录错误
    enableHeartbeat: options.enableHeartbeatLogs || false,
    enablePerformance: options.enablePerformanceLogs || false, // 禁用性能日志
    enableScan: options.enableScanLogs || false, // 禁用扫描日志
    enableDevice: options.enableDeviceLogs || false, // 禁用设备日志
    enableConfig: options.enableConfigLogs || false, // 禁用配置日志
    enableDebug: options.enableDebugLogs || false,
    enableWebSocket: options.enableWebSocketLogs || false,
    enableSystem: options.enableSystemLogs || false, // 禁用系统日志
    enableSecurity: options.enableSecurityLogs || false, // 禁用安全日志
    enableConsoleOutput: options.enableConsoleOutput || false, // 禁用控制台输出
    enableLogPanel: options.enableLogPanel || false, // 禁用日志面板推送
    maxLogCount: options.maxLogCount || 100, // 进一步限制日志数量
    batchInterval: options.batchInterval || 10000, // 增加批处理间隔到10秒
    batchSize: options.batchSize || 30 // 增加批处理大小
  })
  
  // 设置日志面板回调
  setLogPanelCallback((logMessage) => {
    // 只推送ERROR级别的日志到日志面板
    if (logMessage.level === 'ERROR') {
      logs.value.push(logMessage)
      // 进一步限制日志数量
      if (logs.value.length > (options.maxLogCount || 100)) {
        logs.value = logs.value.slice(-(options.maxLogCount || 100))
      }
    }
  })
  
  // 日志过滤配置
  const logConfig = {
    enableHeartbeatLogs: options.enableHeartbeatLogs || false,
    enablePerformanceLogs: options.enablePerformanceLogs || false,
    enableScanLogs: options.enableScanLogs || false,
    enableDeviceLogs: options.enableDeviceLogs || false,
    enableConfigLogs: options.enableConfigLogs || false,
    enableDebugLogs: options.enableDebugLogs || false,
    enableThrottling: options.enableThrottling || true,
    maxLogCount: options.maxLogCount || 100,
    logThrottleInterval: options.logThrottleInterval || 2000,
    ...options.logConfig
  }
  
  // 日志节流机制
  const logThrottleCache = new Map()
  
  // 日志节流函数
  const throttleLog = (key, message, type = 'info') => {
    if (!logConfig.enableThrottling) {
      addLog(message, type)
      return
    }
    
    const now = Date.now()
    const lastLog = logThrottleCache.get(key)
    
    if (!lastLog || now - lastLog > logConfig.logThrottleInterval) {
      addLog(message, type)
      logThrottleCache.set(key, now)
    }
  }
  
  // 清理过期的节流缓存
  const cleanupThrottleCache = () => {
    const now = Date.now()
    for (const [key, timestamp] of logThrottleCache.entries()) {
      if (now - timestamp > logConfig.logThrottleInterval * 2) {
        logThrottleCache.delete(key)
      }
    }
  }
  
  // 每30秒清理一次节流缓存
  setInterval(cleanupThrottleCache, 30000)
  
  // 设备性能历史数据存储
  const devicePerformanceHistory = new Map()
  const MAX_HISTORY_POINTS = 60 // 存储60个历史数据点
  
  // 设备动态阈值存储
  const deviceDynamicThresholds = new Map()
  const THRESHOLD_ADJUSTMENT_INTERVAL = 30000 // 每30秒调整一次阈值
  const DEFAULT_THRESHOLDS = {
    cpu_usage: 80,
    memory_usage: 85,
    npu_usage: 90,
    disk_usage: 90,
    npu_temperature: 80,
    system_temperature: 75,
    load_average_1m: 5
  }
  
  // 异常预警配置
  const alertConfig = {
    enableAlerts: true,
    alertLevels: {
      warning: 0.7,  // 阈值的70%作为警告级别
      critical: 0.9, // 阈值的90%作为严重级别
      emergency: 1.0  // 达到阈值作为紧急级别
    },
    alertCooldown: 60000 // 同一设备同一类型的警报冷却时间：60秒
  }
  
  // 警报历史存储
  const alertHistory = new Map()
  
  // 性能数据异常检测函数
  const checkPerformanceDataAbnormal = (data) => {
    const deviceIp = data.device_ip
    if (!deviceIp) return false
    
    // 更新设备性能历史数据
    updateDevicePerformanceHistory(deviceIp, data)
    
    // 保存到本地存储
    savePerformanceDataToLocalStorage(deviceIp, data)
    
    // 多维度异常检测
    const abnormalChecks = {
      connectionStatus: checkConnectionStatusAbnormal(data),
      absoluteValues: checkAbsoluteValuesAbnormal(data),
      trends: checkTrendsAbnormal(deviceIp, data),
      correlations: checkCorrelationsAbnormal(data)
    }
    
    // 检查是否有任何维度检测到异常
    const isAbnormal = Object.values(abnormalChecks).some(result => result.isAbnormal)
    
    // 检查警报级别
    const alerts = checkAlertLevels(deviceIp, data)
    if (alerts) {
      // 触发警报
      alerts.forEach(alert => {
        triggerAlert(alert)
      })
    }
    
    // 如果检测到异常，记录详细信息
    if (isAbnormal) {
      logger.warn('性能数据异常', {
        device: deviceIp,
        timestamp: data.timestamp,
        checks: abnormalChecks,
        alerts: alerts
      }, 'performance')
      
      // 生成异常报告
      generateAbnormalityReport(deviceIp, data, abnormalChecks)
    }
    
    return isAbnormal
  }
  
  // 更新设备性能历史数据
  const updateDevicePerformanceHistory = (deviceIp, data) => {
    if (!devicePerformanceHistory.has(deviceIp)) {
      devicePerformanceHistory.set(deviceIp, [])
    }
    
    const history = devicePerformanceHistory.get(deviceIp)
    history.push({
      ...data,
      timestamp: Date.now()
    })
    
    // 限制历史数据点数量
    if (history.length > MAX_HISTORY_POINTS) {
      history.shift()
    }
    
    devicePerformanceHistory.set(deviceIp, history)
  }
  
  // 检查连接状态异常
  const checkConnectionStatusAbnormal = (data) => {
    if (data.connected === false) {
      // 检查所有性能指标是否为0
      const metrics = [
        'cpu_usage', 'memory_usage', 'npu_usage', 'disk_usage',
        'load_average_1m', 'load_average_5m', 'load_average_15m',
        'memory_available', 'memory_total', 'npu_memory_usage',
        'npu_power', 'npu_temperature', 'system_temperature'
      ]
      
      const allZero = metrics.every(metric => {
        return data[metric] === 0 || data[metric] === undefined
      })
      
      if (allZero) {
        logger.warn('设备连接状态异常', {
          device: data.device_ip,
          connected: data.connected,
          reason: '设备连接状态为false且所有性能指标为0'
        }, 'device')
      }
      
      return {
        isAbnormal: allZero,
        reason: '设备连接状态为false且所有性能指标为0'
      }
    }
    
    return {
      isAbnormal: false,
      reason: '设备连接状态正常'
    }
  }
  
  // 检查绝对值异常
  const checkAbsoluteValuesAbnormal = (data) => {
    const abnormalMetrics = []
    
    // 定义正常范围
    const normalRanges = {
      cpu_usage: { min: 0, max: 100, warning: 80 },
      memory_usage: { min: 0, max: 100, warning: 85 },
      npu_usage: { min: 0, max: 100, warning: 90 },
      disk_usage: { min: 0, max: 100, warning: 90 },
      npu_temperature: { min: 0, max: 100, warning: 80 },
      system_temperature: { min: 0, max: 100, warning: 75 }
    }
    
    // 检查每个指标
    Object.entries(normalRanges).forEach(([metric, range]) => {
      if (data[metric] !== undefined) {
        if (data[metric] > range.warning) {
          abnormalMetrics.push({
            metric,
            value: data[metric],
            threshold: range.warning,
            reason: `${metric} 超过警告阈值`
          })
        } else if (data[metric] < range.min || data[metric] > range.max) {
          abnormalMetrics.push({
            metric,
            value: data[metric],
            range: range,
            reason: `${metric} 超出正常范围`
          })
        }
      }
    })
    
    return {
      isAbnormal: abnormalMetrics.length > 0,
      reason: `检测到 ${abnormalMetrics.length} 个指标异常`,
      details: abnormalMetrics
    }
  }
  
  // 检查趋势异常
  const checkTrendsAbnormal = (deviceIp, data) => {
    const history = devicePerformanceHistory.get(deviceIp)
    if (!history || history.length < 5) {
      return {
        isAbnormal: false,
        reason: '历史数据不足，无法检测趋势异常'
      }
    }
    
    const abnormalTrends = []
    const significantChangeThreshold = 15 // 15% 变化视为显著
    
    // 检查关键指标的趋势
    const keyMetrics = ['cpu_usage', 'memory_usage', 'npu_usage', 'disk_usage']
    
    keyMetrics.forEach(metric => {
      if (data[metric] !== undefined) {
        // 计算最近5个数据点的趋势
        const recentHistory = history.slice(-5)
        const firstValue = recentHistory[0][metric] || 0
        const lastValue = data[metric]
        
        if (firstValue > 0) {
          const changePercent = Math.abs((lastValue - firstValue) / firstValue * 100)
          
          if (changePercent > significantChangeThreshold) {
            abnormalTrends.push({
              metric,
              startValue: firstValue,
              endValue: lastValue,
              changePercent: changePercent.toFixed(2),
              reason: `${metric} 在短时间内变化显著`
            })
          }
        }
      }
    })
    
    return {
      isAbnormal: abnormalTrends.length > 0,
      reason: `检测到 ${abnormalTrends.length} 个趋势异常`,
      details: abnormalTrends
    }
  }
  
  // 检查相关性异常
  const checkCorrelationsAbnormal = (data) => {
    const abnormalCorrelations = []
    
    // 检查CPU使用率与负载的相关性
    if (data.cpu_usage !== undefined && data.load_average_1m !== undefined) {
      const cpuUsage = data.cpu_usage
      const loadAverage = data.load_average_1m
      
      // 正常情况下，负载应该与CPU使用率相关
      if (cpuUsage < 10 && loadAverage > 5) {
        abnormalCorrelations.push({
          metrics: ['cpu_usage', 'load_average_1m'],
          values: { cpu_usage: cpuUsage, load_average_1m: loadAverage },
          reason: 'CPU使用率低但系统负载高，可能存在I/O等待'
        })
      }
    }
    
    // 检查温度与使用率的相关性
    if (data.cpu_usage !== undefined && data.system_temperature !== undefined) {
      const cpuUsage = data.cpu_usage
      const temperature = data.system_temperature
      
      if (cpuUsage < 20 && temperature > 70) {
        abnormalCorrelations.push({
          metrics: ['cpu_usage', 'system_temperature'],
          values: { cpu_usage: cpuUsage, system_temperature: temperature },
          reason: 'CPU使用率低但系统温度高，可能存在散热问题'
        })
      }
    }
    
    return {
      isAbnormal: abnormalCorrelations.length > 0,
      reason: `检测到 ${abnormalCorrelations.length} 个相关性异常`,
      details: abnormalCorrelations
    }
  }
  
  // 生成异常报告
  const generateAbnormalityReport = (deviceIp, data, checks) => {
    const report = {
      deviceIp,
      timestamp: data.timestamp || Date.now(),
      abnormalChecks: checks,
      currentData: data,
      history: devicePerformanceHistory.get(deviceIp)?.slice(-10) || []
    }
    
    // 可以将报告发送到服务器或存储到本地
    logger.error('[性能异常报告]', report)
    
    // 触发异常事件，供其他组件使用
    if (messageHandlers.has('abnormality_detected')) {
      messageHandlers.get('abnormality_detected')(report)
    }
  }
  
  // 获取设备性能历史数据
  const getDevicePerformanceHistory = (deviceIp) => {
    return devicePerformanceHistory.get(deviceIp) || []
  }
  
  // 清除设备性能历史数据
  const clearDevicePerformanceHistory = (deviceIp) => {
    if (devicePerformanceHistory.has(deviceIp)) {
      devicePerformanceHistory.delete(deviceIp)
    }
  }
  
  // 动态阈值调整函数
  const adjustDynamicThresholds = () => {
    devicePerformanceHistory.forEach((history, deviceIp) => {
      if (history.length < 10) {
        // 历史数据不足，使用默认阈值
        return
      }
      
      // 计算历史数据的统计值
      const stats = calculatePerformanceStats(history)
      
      // 基于统计值调整阈值
      const dynamicThresholds = {
        ...DEFAULT_THRESHOLDS
      }
      
      // 调整CPU使用率阈值
      if (stats.cpu_usage) {
        // 基于平均值和标准差调整阈值
        const avg = stats.cpu_usage.avg
        const std = stats.cpu_usage.std
        // 设置阈值为平均值加上2个标准差，但不超过默认阈值的110%
        dynamicThresholds.cpu_usage = Math.min(
          DEFAULT_THRESHOLDS.cpu_usage * 1.1,
          Math.max(70, avg + 2 * std)
        )
      }
      
      // 调整内存使用率阈值
      if (stats.memory_usage) {
        const avg = stats.memory_usage.avg
        dynamicThresholds.memory_usage = Math.min(
          DEFAULT_THRESHOLDS.memory_usage * 1.1,
          Math.max(80, avg + 10)
        )
      }
      
      // 调整温度阈值
      if (stats.system_temperature) {
        const avg = stats.system_temperature.avg
        dynamicThresholds.system_temperature = Math.min(
          DEFAULT_THRESHOLDS.system_temperature * 1.1,
          Math.max(65, avg + 15)
        )
      }
      
      // 调整负载阈值
      if (stats.load_average_1m) {
        const avg = stats.load_average_1m.avg
        dynamicThresholds.load_average_1m = Math.min(
          DEFAULT_THRESHOLDS.load_average_1m * 1.2,
          Math.max(2, avg + 2)
        )
      }
      
      // 存储动态阈值
      deviceDynamicThresholds.set(deviceIp, {
        thresholds: dynamicThresholds,
        timestamp: Date.now(),
        stats: stats
      })
    })
  }
  
  // 启动动态阈值调整定时器
  setInterval(adjustDynamicThresholds, THRESHOLD_ADJUSTMENT_INTERVAL)
  
  // 计算性能数据统计值
  const calculatePerformanceStats = (history) => {
    const stats = {}
    const metrics = ['cpu_usage', 'memory_usage', 'npu_usage', 'disk_usage', 'system_temperature', 'load_average_1m']
    
    metrics.forEach(metric => {
      const values = history
        .map(entry => entry[metric])
        .filter(value => value !== undefined && value !== null && !isNaN(value))
      
      if (values.length > 0) {
        const avg = values.reduce((sum, val) => sum + val, 0) / values.length
        const std = Math.sqrt(
          values.reduce((sum, val) => sum + Math.pow(val - avg, 2), 0) / values.length
        )
        const max = Math.max(...values)
        const min = Math.min(...values)
        
        stats[metric] = {
          avg: avg,
          std: std,
          max: max,
          min: min,
          count: values.length
        }
      }
    })
    
    return stats
  }
  
  // 获取设备的当前阈值
  const getDeviceThresholds = (deviceIp) => {
    const dynamic = deviceDynamicThresholds.get(deviceIp)
    if (dynamic) {
      return dynamic.thresholds
    }
    return DEFAULT_THRESHOLDS
  }
  
  // 检查警报级别
  const checkAlertLevels = (deviceIp, data) => {
    if (!alertConfig.enableAlerts) {
      return null
    }
    
    const thresholds = getDeviceThresholds(deviceIp)
    const alerts = []
    
    // 检查每个指标的警报级别
    Object.entries(thresholds).forEach(([metric, threshold]) => {
      const value = data[metric]
      if (value === undefined) {
        return
      }
      
      // 计算警报级别
      const ratio = value / threshold
      let alertLevel = null
      
      if (ratio >= alertConfig.alertLevels.emergency) {
        alertLevel = 'emergency'
      } else if (ratio >= alertConfig.alertLevels.critical) {
        alertLevel = 'critical'
      } else if (ratio >= alertConfig.alertLevels.warning) {
        alertLevel = 'warning'
      }
      
      if (alertLevel) {
        alerts.push({
          deviceIp,
          metric,
          value,
          threshold,
          ratio,
          level: alertLevel,
          timestamp: data.timestamp || Date.now()
        })
      }
    })
    
    return alerts.length > 0 ? alerts : null
  }
  
  // 触发警报
  const triggerAlert = (alert) => {
    const key = `${alert.deviceIp}_${alert.metric}_${alert.level}`
    const now = Date.now()
    
    // 检查冷却时间
    const lastAlert = alertHistory.get(key)
    if (lastAlert && now - lastAlert.timestamp < alertConfig.alertCooldown) {
      return false
    }
    
    // 记录警报
    alertHistory.set(key, {
      ...alert,
      timestamp: now
    })
    
    // 生成警报消息
    const alertMessages = {
      warning: `[性能警告] 设备 ${alert.deviceIp} 的 ${alert.metric} 达到警告级别: ${alert.value} (阈值: ${alert.threshold})`,
      critical: `[性能严重警告] 设备 ${alert.deviceIp} 的 ${alert.metric} 达到严重级别: ${alert.value} (阈值: ${alert.threshold})`,
      emergency: `[性能紧急警告] 设备 ${alert.deviceIp} 的 ${alert.metric} 达到紧急级别: ${alert.value} (阈值: ${alert.threshold})`
    }
    
    const message = alertMessages[alert.level]
    
    // 添加到日志
    throttleLog(`alert_${alert.deviceIp}_${alert.metric}`, message, 'error')
    
    // 触发警报事件
    if (messageHandlers.has('alert_triggered')) {
      messageHandlers.get('alert_triggered')(alert)
    }
    
    return true
  }
  
  // 清理警报历史
  const cleanupAlertHistory = () => {
    const now = Date.now()
    const expiredKeys = []
    
    alertHistory.forEach((alert, key) => {
      if (now - alert.timestamp > alertConfig.alertCooldown * 2) {
        expiredKeys.push(key)
      }
    })
    
    expiredKeys.forEach(key => {
      alertHistory.delete(key)
    })
  }
  
  // 每2分钟清理一次警报历史
  setInterval(cleanupAlertHistory, 120000)
  
  // 本地存储配置
  const storageConfig = {
    enableLocalStorage: true,
    storageKeyPrefix: 'ws_performance_',
    maxStorageDays: 7, // 最多存储7天的数据
    maxStorageSize: 5 * 1024 * 1024, // 最大存储大小：5MB
    storageInterval: 5000, // 存储间隔：5秒
    batchStorageSize: 10 // 批量存储大小
  }
  
  // 本地存储缓存
  const storageCache = new Map()
  let storageTimer = null
  
  // 保存性能数据到本地存储
  const savePerformanceDataToLocalStorage = (deviceIp, data) => {
    if (!storageConfig.enableLocalStorage) {
      return
    }
    
    // 添加到存储缓存
    if (!storageCache.has(deviceIp)) {
      storageCache.set(deviceIp, [])
    }
    
    const deviceData = storageCache.get(deviceIp)
    deviceData.push({
      ...data,
      stored_at: Date.now()
    })
    
    // 如果缓存达到批量大小，立即保存
    if (deviceData.length >= storageConfig.batchStorageSize) {
      flushStorageCache()
    } else if (!storageTimer) {
      // 否则启动定时器，延迟保存
      storageTimer = setTimeout(flushStorageCache, storageConfig.storageInterval)
    }
  }
  
  // 刷新存储缓存到本地存储
  const flushStorageCache = () => {
    if (storageCache.size === 0) {
      if (storageTimer) {
        clearTimeout(storageTimer)
        storageTimer = null
      }
      return
    }
    
    storageCache.forEach((dataList, deviceIp) => {
      if (dataList.length === 0) {
        return
      }
      
      try {
        // 获取现有数据
        const storageKey = `${storageConfig.storageKeyPrefix}${deviceIp}`
        const existingData = localStorage.getItem(storageKey)
        const historyData = existingData ? JSON.parse(existingData) : []
        
        // 合并新数据
        historyData.push(...dataList)
        
        // 清理过期数据
        const now = Date.now()
        const cutoffTime = now - (storageConfig.maxStorageDays * 24 * 60 * 60 * 1000)
        const filteredData = historyData.filter(item => item.stored_at > cutoffTime)
        
        // 限制数据量，只保留最近的1000条
        const limitedData = filteredData.slice(-1000)
        
        // 保存到本地存储
        localStorage.setItem(storageKey, JSON.stringify(limitedData))
        
        // 清空缓存
        storageCache.set(deviceIp, [])
      } catch (error) {
        logger.error('保存性能数据到本地存储失败:', error)
      }
    })
    
    // 清理空缓存
    const emptyKeys = []
    storageCache.forEach((data, key) => {
      if (data.length === 0) {
        emptyKeys.push(key)
      }
    })
    emptyKeys.forEach(key => storageCache.delete(key))
    
    // 清除定时器
    if (storageTimer) {
      clearTimeout(storageTimer)
      storageTimer = null
    }
  }
  
  // 从本地存储加载性能数据
  const loadPerformanceDataFromLocalStorage = (deviceIp) => {
    if (!storageConfig.enableLocalStorage) {
      return []
    }
    
    try {
      const storageKey = `${storageConfig.storageKeyPrefix}${deviceIp}`
      const data = localStorage.getItem(storageKey)
      return data ? JSON.parse(data) : []
    } catch (error) {
      logger.error('从本地存储加载性能数据失败:', error)
      return []
    }
  }
  
  // 获取设备历史性能数据
  const getDeviceHistoricalData = (deviceIp, timeRange = 24 * 60 * 60 * 1000) => {
    const allData = loadPerformanceDataFromLocalStorage(deviceIp)
    const now = Date.now()
    const startTime = now - timeRange
    
    return allData.filter(item => item.stored_at >= startTime)
  }
  
  // 分析设备历史性能数据
  const analyzeDeviceHistoricalData = (deviceIp, timeRange = 24 * 60 * 60 * 1000) => {
    const historicalData = getDeviceHistoricalData(deviceIp, timeRange)
    
    if (historicalData.length === 0) {
      return {
        deviceIp,
        timeRange,
        dataPoints: 0,
        analysis: '无历史数据'
      }
    }
    
    // 计算统计值
    const stats = calculatePerformanceStats(historicalData)
    
    // 分析趋势
    const trends = analyzePerformanceTrends(historicalData)
    
    // 分析异常
    const anomalyAnalysis = analyzePerformanceAnomalies(historicalData)
    
    return {
      deviceIp,
      timeRange,
      dataPoints: historicalData.length,
      stats,
      trends,
      anomalyAnalysis,
      analysisTime: Date.now()
    }
  }
  
  // 分析性能趋势
  const analyzePerformanceTrends = (historicalData) => {
    if (historicalData.length < 2) {
      return { hasTrend: false, message: '数据点不足' }
    }
    
    const trends = {}
    const metrics = ['cpu_usage', 'memory_usage', 'npu_usage', 'disk_usage', 'npu_temperature', 'system_temperature']
    
    metrics.forEach(metric => {
      const values = historicalData.map(item => item[metric]).filter(val => val !== undefined && !isNaN(val))
      
      if (values.length >= 2) {
        const firstValue = values[0]
        const lastValue = values[values.length - 1]
        const change = lastValue - firstValue
        const changePercent = firstValue > 0 ? (change / firstValue) * 100 : 0
        
        trends[metric] = {
          start: firstValue,
          end: lastValue,
          change: change,
          changePercent: changePercent,
          trend: change > 5 ? '上升' : change < -5 ? '下降' : '稳定'
        }
      }
    })
    
    return { hasTrend: Object.keys(trends).length > 0, trends }
  }
  
  // 分析性能异常
  const analyzePerformanceAnomalies = (historicalData) => {
    const anomalies = []
    
    historicalData.forEach(data => {
      const isAbnormal = checkPerformanceDataAbnormal(data)
      if (isAbnormal) {
        anomalies.push({
          timestamp: data.timestamp,
          stored_at: data.stored_at,
          data: data
        })
      }
    })
    
    return {
      anomalyCount: anomalies.length,
      anomalyRate: historicalData.length > 0 ? (anomalies.length / historicalData.length) * 100 : 0,
      anomalies: anomalies.slice(-10) // 只返回最近的10个异常
    }
  }
  
  // 清理过期的本地存储数据
  const cleanupLocalStorage = () => {
    if (!storageConfig.enableLocalStorage) {
      return
    }
    
    try {
      const now = Date.now()
      const cutoffTime = now - (storageConfig.maxStorageDays * 24 * 60 * 60 * 1000)
      const keysToRemove = []
      
      // 遍历所有本地存储键
      for (let i = 0; i < localStorage.length; i++) {
        const key = localStorage.key(i)
        
        // 检查是否是性能数据键
        if (key.startsWith(storageConfig.storageKeyPrefix)) {
          try {
            const data = JSON.parse(localStorage.getItem(key))
            if (Array.isArray(data)) {
              // 过滤过期数据
              const filteredData = data.filter(item => item.stored_at > cutoffTime)
              
              if (filteredData.length === 0) {
                // 如果没有数据，删除键
                keysToRemove.push(key)
              } else if (filteredData.length < data.length) {
                // 更新数据
                localStorage.setItem(key, JSON.stringify(filteredData))
              }
            }
          } catch (error) {
            logger.error(`清理本地存储键 ${key} 失败:`, error)
            keysToRemove.push(key)
          }
        }
      }
      
      // 删除过期键
      keysToRemove.forEach(key => {
        localStorage.removeItem(key)
      })
      
      
    } catch (error) {
      logger.error('清理本地存储失败:', error)
    }
  }
  
  // 每小时清理一次本地存储
  setInterval(cleanupLocalStorage, 60 * 60 * 1000)
  
  // 故障容错配置
  const faultToleranceConfig = {
    enableFaultTolerance: true,
    enableAutoRecovery: true,
    enableDataBackup: true,
    enableDegradedMode: true,
    maxRetryAttempts: 5,
    retryDelay: 2000,
    degradedModeThreshold: 3, // 连续失败3次后进入降级模式
    autoRecoveryInterval: 30000 // 自动恢复检查间隔：30秒
  }
  
  // 系统状态
  const systemState = {
    isHealthy: true,
    currentMode: 'normal', // normal, degraded, emergency
    consecutiveFailures: 0,
    lastFailureTime: 0,
    lastRecoveryTime: Date.now(),
    activeRecoveries: 0
  }
  
  // 数据备份存储
  const dataBackup = new Map()
  
  // 故障检测函数
  const detectSystemFaults = () => {
    if (!faultToleranceConfig.enableFaultTolerance) {
      return
    }
    
    const faults = []
    
    // 检查WebSocket连接状态
    if (!wsConnected.value) {
      faults.push('websocket_disconnected')
    }
    
    // 检查性能数据缓存状态
    if (performanceDataCache.size > 50) {
      faults.push('performance_cache_overload')
    }
    
    // 检查本地存储状态
    if (faultToleranceConfig.enableDataBackup) {
      try {
        // 测试本地存储是否可用
        localStorage.setItem('ws_health_check', Date.now().toString())
        localStorage.removeItem('ws_health_check')
      } catch (error) {
        faults.push('local_storage_error')
      }
    }
    
    return faults
  }
  
  // 处理系统故障
  const handleSystemFaults = (faults) => {
    if (faults.length === 0) {
      // 系统健康，重置故障计数
      systemState.consecutiveFailures = 0
      systemState.isHealthy = true
      
      if (systemState.currentMode !== 'normal') {
        switchToNormalMode()
      }
      
      return
    }
    
    // 系统存在故障
    systemState.consecutiveFailures++
    systemState.lastFailureTime = Date.now()
    systemState.isHealthy = false
    
    // 根据故障情况切换到相应模式
    if (systemState.consecutiveFailures >= faultToleranceConfig.degradedModeThreshold) {
      switchToDegradedMode(faults)
    }
    
    // 尝试自动恢复
    if (faultToleranceConfig.enableAutoRecovery) {
      attemptAutoRecovery(faults)
    }
  }
  
  // 切换到正常模式
  const switchToNormalMode = () => {
    systemState.currentMode = 'normal'
    systemState.lastRecoveryTime = Date.now()
    
    // 触发模式切换事件
    if (messageHandlers.has('mode_switched')) {
      messageHandlers.get('mode_switched')({ mode: 'normal' })
    }
  }
  
  // 切换到降级模式
  const switchToDegradedMode = (faults) => {
    systemState.currentMode = 'degraded'
    
    // 启用降级策略
    enableDegradedModeStrategies()
    
    // 触发模式切换事件
    if (messageHandlers.has('mode_switched')) {
      messageHandlers.get('mode_switched')({ mode: 'degraded', faults })
    }
  }
  
  // 启用降级模式策略
  const enableDegradedModeStrategies = () => {
    if (!faultToleranceConfig.enableDegradedMode) {
      return
    }
    
    // 1. 减少日志输出
    logConfig.enableDebugLogs = false
    logConfig.enableHeartbeatLogs = false
    logConfig.enableThrottling = true
    
    // 2. 减少数据处理频率
    storageConfig.storageInterval = 10000 // 增加存储间隔到10秒
    storageConfig.batchStorageSize = 20 // 增加批量存储大小
    
    // 3. 减少历史数据保留
    storageConfig.maxStorageDays = 3 // 减少存储天数到3天
    
    // 4. 简化异常检测
    // 这里可以添加简化异常检测的逻辑
  }
  
  // 尝试自动恢复
  const attemptAutoRecovery = (faults) => {
    if (systemState.activeRecoveries >= faultToleranceConfig.maxRetryAttempts) {
      return
    }
    
    systemState.activeRecoveries++
    
    // 根据不同故障类型尝试恢复
    faults.forEach(fault => {
      switch (fault) {
        case 'websocket_disconnected':
          // 尝试重新连接WebSocket
          setTimeout(() => {
            if (!wsConnected.value) {
              connectWebSocket()
            }
          }, faultToleranceConfig.retryDelay)
          break
          
        case 'performance_cache_overload':
          // 清理性能数据缓存
          performanceDataCache.clear()
          break
          
        case 'local_storage_error':
          // 尝试清理本地存储
          cleanupLocalStorage()
          break
      }
    })
    
    // 恢复尝试后，重置活动恢复计数
    setTimeout(() => {
      systemState.activeRecoveries = 0
    }, faultToleranceConfig.autoRecoveryInterval)
  }
  
  // 数据备份函数
  const backupPerformanceData = (data) => {
    if (!faultToleranceConfig.enableDataBackup) {
      return
    }
    
    const deviceIp = data.device_ip
    if (!deviceIp) {
      return
    }
    
    // 添加到备份存储
    if (!dataBackup.has(deviceIp)) {
      dataBackup.set(deviceIp, [])
    }
    
    const deviceBackup = dataBackup.get(deviceIp)
    deviceBackup.push({
      ...data,
      backed_up_at: Date.now()
    })
    
    // 限制备份大小
    if (deviceBackup.length > 100) {
      deviceBackup.shift()
    }
  }
  
  // 恢复性能数据
  const restorePerformanceData = (deviceIp) => {
    if (!faultToleranceConfig.enableDataBackup) {
      return []
    }
    
    const backup = dataBackup.get(deviceIp)
    if (!backup || backup.length === 0) {
      return []
    }
    
    return backup
  }
  
  // 清理数据备份
  const cleanupDataBackup = () => {
    const now = Date.now()
    const backupExpiryTime = 24 * 60 * 60 * 1000 // 备份过期时间：24小时
    
    dataBackup.forEach((backup, deviceIp) => {
      const filteredBackup = backup.filter(item => now - item.backed_up_at < backupExpiryTime)
      
      if (filteredBackup.length === 0) {
        dataBackup.delete(deviceIp)
      } else {
        dataBackup.set(deviceIp, filteredBackup)
      }
    })
  }
  
  // 每小时清理一次数据备份
  setInterval(cleanupDataBackup, 60 * 60 * 1000)
  
  // 系统健康检查
  const checkSystemHealth = () => {
    const faults = detectSystemFaults()
    handleSystemFaults(faults)
    
    return {
      isHealthy: systemState.isHealthy,
      currentMode: systemState.currentMode,
      consecutiveFailures: systemState.consecutiveFailures,
      faults: faults,
      lastFailureTime: systemState.lastFailureTime,
      lastRecoveryTime: systemState.lastRecoveryTime
    }
  }
  
  // 启动系统健康检查定时器
  setInterval(checkSystemHealth, faultToleranceConfig.autoRecoveryInterval)
  
  // 扫描状态跟踪
  let isScanning = false
  
  // 新增：存储消息处理器
  const messageHandlers = new Map()
  
  // 性能数据缓存，用于批量处理
  const performanceDataCache = new Map()
  let performanceBatchTimer = null
  const BATCH_INTERVAL = 500 // 批处理间隔：500ms
  
  // 消息去重机制
  const processedMessages = new Set()
  const MESSAGE_EXPIRY_TIME = 5 * 60 * 1000 // 消息过期时间：5分钟
  let messageCleanupTimer = null
  
  // 生成消息唯一标识符
  const generateMessageId = (data) => {
    if (!data) return null
    
    // 对于性能数据，使用设备IP和时间戳作为唯一标识
    if (data.type === 'performance_data' || data.device_ip) {
      const timestamp = data.timestamp || Date.now()
      return `${data.device_ip || 'unknown'}_${timestamp}_${data.type || 'performance'}`
    }
    
    // 对于扫描消息，使用类型、状态和时间戳
    if (data.type === 'scan_progress' || data.type === 'scan_result') {
      const timestamp = Date.now()
      return `${data.type}_${data.status || 'unknown'}_${timestamp}`
    }
    
    // 对于设备连接消息，使用设备IP和类型
    if (data.type === 'device_connect' || data.type === 'device_disconnect') {
      return `${data.type}_${data.ip_address || 'unknown'}_${Date.now()}`
    }
    
    // 对于其他消息，使用类型和时间戳
    return `${data.type || 'unknown'}_${Date.now()}`
  }
  
  // 清理过期消息
  const cleanupExpiredMessages = () => {
    const now = Date.now()
    const expiredMessages = []
    
    processedMessages.forEach((messageId) => {
      // 提取时间戳（如果有的话）
      const timestampMatch = messageId.match(/_([0-9]+)_/)
      if (timestampMatch) {
        const timestamp = parseInt(timestampMatch[1])
        if (now - timestamp > MESSAGE_EXPIRY_TIME) {
          expiredMessages.push(messageId)
        }
      }
    })
    
    expiredMessages.forEach((messageId) => {
      processedMessages.delete(messageId)
    })
  }
  
  // 启动消息清理定时器
  const startMessageCleanup = () => {
    if (messageCleanupTimer) {
      clearInterval(messageCleanupTimer)
    }
    
    // 每30秒清理一次过期消息
    messageCleanupTimer = setInterval(cleanupExpiredMessages, 30 * 1000)
  }
  
  // 停止消息清理定时器
  const stopMessageCleanup = () => {
    if (messageCleanupTimer) {
      clearInterval(messageCleanupTimer)
      messageCleanupTimer = null
    }
  }

  const addLog = (message, type = 'info') => {
    const levelMap = {
      error: 'error',
      warn: 'warn',
      info: 'info',
      success: 'info',
      debug: 'debug'
    }
    
    const level = levelMap[type] || 'info'
    const category = type === 'error' ? 'system' : type === 'success' ? 'system' : 'info'
    
    // 使用新的Logger实例记录日志
    logger[level](message, null, category)
  }

  const getLogClass = (log) => {
    return `log-${log.type}`
  }

  const connectWebSocket = () => {
    if (isConnecting) {
      logger.debug('WebSocket连接正在进行中，跳过')
      return
    }

    if (websocket.value && websocket.value.readyState === WebSocket.OPEN) {
      logger.debug('WebSocket已连接，无需重新连接')
      wsConnected.value = true
      return
    }

    if (websocket.value && websocket.value.readyState === WebSocket.CONNECTING) {
      logger.debug('WebSocket正在连接中...')
      isConnecting = true
      return
    }

    if (reconnectTimer) {
      clearTimeout(reconnectTimer)
      reconnectTimer = null
    }

    if (websocket.value) {
      websocket.value.onclose = null
      websocket.value.onerror = null
      websocket.value.onopen = null
      websocket.value.onmessage = null
      websocket.value.close()
      websocket.value = null
    }

    isConnecting = true
    const currentConnectionId = ++connectionId

    try {
      const wsUrl = `ws://${window.location.hostname}:8083`
      logger.info(`[连接 ${currentConnectionId}] 正在连接WebSocket: ${wsUrl}`, null, 'websocket')
      websocket.value = new WebSocket(wsUrl)

      websocket.value.onopen = () => {
        if (currentConnectionId === connectionId) {
          isConnecting = false
          wsConnected.value = true
          logger.info(`[连接 ${currentConnectionId}] WebSocket连接已建立`, null, 'websocket')
          addLog('WebSocket连接已建立', 'success')

          // 启动心跳机制
          startHeartbeat()
          
          // 启动消息清理定时器
          startMessageCleanup()
          
          // 清理之前可能的消息缓存
          processedMessages.clear()
          logThrottleCache.clear()
          logger.debug('WebSocket连接建立，已清理消息缓存')
        }
      }

      websocket.value.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data)
          
          // 生成消息唯一标识符
          const messageId = generateMessageId(data)
          
          // 检查消息是否已处理过
          if (processedMessages.has(messageId)) {
            logger.debug(`[连接 ${currentConnectionId}] 消息已处理，跳过重复消息:`, messageId)
            return
          }
          
          // 标记消息为已处理
          processedMessages.add(messageId)
          
          // 性能数据链路第三节点：前端WebSocket onmessage 回调入口
          if (logConfig.enableDebugLogs) {
            logger.debug(`[连接 ${currentConnectionId}] 收到WebSocket消息:`, data)
            logger.debug(`[连接 ${currentConnectionId}] 消息ID:`, messageId)
          }

          // 检查是否为性能数据消息（包含"type": "performance_data"标识）
          if (data.type === 'performance_data') {
            // 检查设备连接状态，如果未连接则跳过异常检测
            if (!data.connected) {
              logger.debug(`[性能数据] 设备未连接，跳过异常检测:`, data.device_ip)
              // 性能数据消息，不添加到日志，只用于更新性能图表
              if (logConfig.enablePerformanceLogs) {
                logger.debug(`[性能数据] 设备未连接，性能数据为0:`, data.device_ip)
              }
            } else {
              // 只有当设备连接时才进行异常检测
              const isAbnormal = checkPerformanceDataAbnormal(data)
              
              // 性能数据消息，不添加到日志，只用于更新性能图表
              if (logConfig.enablePerformanceLogs) {
                if (isAbnormal) {
                  logger.warn('[性能数据异常] 收到异常性能数据:', data, 'performance')
                  throttleLog(`performance_abnormal_${data.device_ip}`, `设备 ${data.device_ip} 性能数据异常`, 'error')
                } else {
                  logger.debug('[性能数据] 收到性能数据，已转发至相应处理器:', data)
                }
              }
            }
            
            // 将性能数据添加到缓存中，准备批量处理
            addToPerformanceCache(data)
            // 但也要确保分发给注册的处理器
            messageHandlers.forEach((handler, key) => {
              try {
                handler(data)
              } catch (error) {
                logger.error(`[连接 ${currentConnectionId}] 消息处理器 ${key} 错误:`, error)
              }
            })
            // 不返回，让消息继续进行后续处理
          }
          // 检查是否为性能数据消息（包含device_ip字段和性能指标）
          else if (data.device_ip && (data.hasOwnProperty('cpu_usage') || data.hasOwnProperty('memory_usage') || 
                   data.hasOwnProperty('npu_usage') || data.hasOwnProperty('system_temperature'))) {
            // 检查设备连接状态，如果未连接则减少日志输出
            if (!data.connected) {
              if (logConfig.enablePerformanceLogs) {
                logger.debug(`[性能数据] 设备未连接，性能数据为0:`, data.device_ip)
              }
            } else if (logConfig.enablePerformanceLogs) {
              logger.debug('[性能数据] 收到性能数据，已转发至相应处理器:', data)
            }
            
            // 将性能数据添加到缓存中，准备批量处理
            addToPerformanceCache(data)
            // 但也要确保分发给注册的处理器
            messageHandlers.forEach((handler, key) => {
              try {
                handler(data)
              } catch (error) {
                logger.error(`[连接 ${currentConnectionId}] 消息处理器 ${key} 错误:`, error)
              }
            })
            // 不返回，让消息继续进行后续处理
          }
          // 特别检查是否包含性能指标字段
          else if (data.hasOwnProperty('cpu_usage') || data.hasOwnProperty('memory_usage') || 
                   data.hasOwnProperty('npu_usage') || data.hasOwnProperty('system_temperature')) {
            // 检查设备连接状态，如果未连接则减少日志输出
            if (!data.connected) {
              if (logConfig.enablePerformanceLogs) {
                logger.debug('[性能指标] 设备未连接，性能数据为0')
              }
            } else if (logConfig.enablePerformanceLogs) {
              logger.debug('[性能指标] 收到性能指标数据，已转发至相应处理器:', data)
            }
            // 不添加到日志，只用于更新性能图表
            addToPerformanceCache(data)
            // 但也要确保分发给注册的处理器
            messageHandlers.forEach((handler, key) => {
              try {
                handler(data)
              } catch (error) {
                logger.error(`[连接 ${currentConnectionId}] 消息处理器 ${key} 错误:`, error)
              }
            })
            // 不返回，让消息继续进行后续处理
          }
          // 添加额外的调试日志
          else if (data.type === 'frontend_debug') {
            if (logConfig.enableDebugLogs) {
              logger.debug('[前端调试] 收到后端调试信息:', {
                message: data.message,
                device: data.device_ip,
                cpu: data.cpu_usage,
                memory: data.memory_usage
              })
            }
          }
          // 处理设备扫描相关的消息
          else if (data.type === 'scan_progress' || data.type === 'scan_result' || data.type === 'device_found') {
            if (data.status === 'completed') {
              const deviceCount = data.device_count || 0;
              addLog(`设备扫描完成，共发现 ${deviceCount} 个设备`, 'success');
              isScanning = false; // 重置扫描状态，以便下次扫描能输出开始日志
            } else if (data.status === 'starting') {
              // 只在扫描刚开始时输出一次，避免每个子网扫描都输出
              if (!isScanning) {
                isScanning = true;
                addLog('设备扫描开始', 'info');
              }
            } else if (data.status === 'device_found' || data.type === 'device_found') {
              const deviceIp = data.device?.ip_address || data.device?.ip || data.ip_address || '未知IP';
              const hostname = data.device?.hostname || '未知主机';
              addLog(`发现新设备: ${hostname} [${deviceIp}]`, 'info');
            } else {
              // 显示扫描进度
              if (data.current !== undefined && data.total !== undefined) {
                addLog(`设备扫描进度: ${data.current}/${data.total}`, 'info');
              }
            }
          }
          // 处理设备连接相关的消息
          else if (data.type === 'device_connect' || data.type === 'connection_status') {
            const status = data.status || data.message || '设备连接';
            const deviceIp = data.ip_address || data.device_ip || '未知IP';
            const level = data.success === false ? 'error' : (data.success === true ? 'success' : 'info');
            addLog(`设备连接: ${status} [${deviceIp}]`, level);
          }
          // 处理设备断开相关的消息
          else if (data.type === 'device_disconnect') {
            const status = data.status || data.message || '设备断开';
            const deviceIp = data.ip_address || data.device_ip || '未知IP';
            const level = data.success === false ? 'error' : 'info';
            addLog(`设备断开: ${status} [${deviceIp}]`, level);
          }
          // 处理性能监控相关的消息
          else if (data.type === 'performance_monitor') {
            const action = data.action || data.status || '性能监控';
            const deviceIp = data.device_ip || '未知IP';
            addLog(`性能监控: ${action} [${deviceIp}]`, 'info');
          }
          // 处理参数配置相关的消息
          else if (data.type === 'config_operation') {
            const operation = data.operation || data.action || '参数配置';
            const deviceIp = data.device_ip || '未知IP';
            const status = data.success !== undefined ? (data.success ? '成功' : '失败') : '';
            addLog(`参数配置: ${operation} [${deviceIp}] ${status}`, data.success === false ? 'error' : 'info');
          }
          // 处理模型状态检查相关的消息
          else if (data.type === 'model_check') {
            const message = data.message || data.status || '模型状态';
            const deviceIp = data.device_ip || '未知IP';
            addLog(`模型状态: ${message} [${deviceIp}]`, 'info');
          }
          // 只记录包含JSON配置的重要消息
          else if (data.type === 'config_status' || data.type === 'model_status') {
            const status = data.status || ''

            // 只记录包含JSON配置的重要消息
            if (status.includes('{') && status.includes('}')) {
              try {
                const jsonStart = status.indexOf('{')
                const jsonEnd = status.lastIndexOf('}') + 1
                const jsonStr = status.substring(jsonStart, jsonEnd)
                const parsed = JSON.parse(jsonStr)
                addLog(`配置已接收: ${Object.keys(parsed).length} 项`, 'success')
              } catch (e) {
                addLog('配置消息接收', 'info')
              }
            }
            // 配置文件不存在也只记录一次
            else if (status.includes('Configuration file does not exist')) {
              addLog('配置文件不存在', 'info')
            }
            // 模型运行状态
            else if (data.type === 'model_status' && (
              status.includes('running') ||
              status.includes('进程') ||
              status.includes('model') ||
              status.includes('Model')
            )) {
              const shortStatus = status.length > 80 ? status.substring(0, 80) + '...' : status
              addLog(`模型状态: ${shortStatus}`, 'info')
            }
            // 其他终端回显不记录到日志
          }

          // 分发消息给所有注册的处理器
          messageHandlers.forEach((handler, key) => {
            try {
              handler(data)
            } catch (error) {
              logger.error(`[连接 ${currentConnectionId}] 消息处理器 ${key} 错误:`, error)
            }
          })

        } catch (error) {
          logger.error(`[连接 ${currentConnectionId}] 解析消息失败:`, error)
          addLog(`解析WebSocket消息失败: ${error.message}`, 'error')
        }
      }

      websocket.value.onclose = (event) => {
        if (currentConnectionId === connectionId) {
          isConnecting = false
          wsConnected.value = false
          logger.warn(`[连接 ${currentConnectionId}] WebSocket连接已断开, code: ${event.code}, reason: ${event.reason}`, null, 'websocket')
          addLog(`WebSocket连接已断开 (code: ${event.code})`, 'error')

          // 停止心跳
          stopHeartbeat()
          
          // 停止消息清理定时器
          stopMessageCleanup()
          
          // 清理消息缓存
          processedMessages.clear()
          logThrottleCache.clear()
          logger.debug('WebSocket连接关闭，已清理消息缓存')

          if (event.code === 1000) {
            logger.info('正常关闭，不重连')
            return
          }

          // 指数退避重连策略
          const reconnectDelay = Math.min(30000, Math.pow(2, connectionId) * 1000)
          logger.info(`[连接 ${currentConnectionId}] 准备在 ${reconnectDelay}ms 后尝试重新连接...`)
          
          reconnectTimer = setTimeout(() => {
            logger.info(`[连接 ${currentConnectionId}] 尝试重新连接...`)
            throttleLog('websocket_reconnect', '尝试重新连接WebSocket...', 'info')
            connectWebSocket()
          }, reconnectDelay)
        }
      }

      websocket.value.onerror = (error) => {
        if (currentConnectionId === connectionId) {
          isConnecting = false
          wsConnected.value = false
          logger.error(`[连接 ${currentConnectionId}] WebSocket连接错误:`, error)
          addLog('WebSocket连接错误: ' + error.message, 'error')
        }
      }

    } catch (error) {
      if (currentConnectionId === connectionId) {
        isConnecting = false
        wsConnected.value = false
        logger.error(`[连接 ${currentConnectionId}] WebSocket创建失败:`, error)
        addLog('WebSocket连接失败: ' + error.message, 'error')

        reconnectTimer = setTimeout(() => {
          logger.info(`[连接 ${currentConnectionId}] 创建失败后尝试重连...`)
          connectWebSocket()
        }, 5000)
      }
    }
  }

  const closeWebSocket = () => {
    if (websocket.value) {
      logger.info(`[连接 ${connectionId}] 主动关闭WebSocket连接`, null, 'websocket')

      websocket.value.onclose = null
      websocket.value.onerror = null
      websocket.value.onopen = null
      websocket.value.onmessage = null

      // 清除重连定时器
      if (reconnectTimer) {
        clearTimeout(reconnectTimer)
        reconnectTimer = null
      }

      // 停止心跳
      stopHeartbeat()

      // 清除消息处理器
      messageHandlers.clear()

      // 正常关闭连接
      websocket.value.close(1000, '主动关闭')
      websocket.value = null
      wsConnected.value = false
      isConnecting = false

      addLog('WebSocket连接已主动关闭', 'info')
    }
  }

  // 设置扫描进度回调函数
  const setScanProgressCallback = (callback) => {
    onScanProgress = callback
  }

  // 获取连接状态
  const getConnectionStatus = () => {
    if (!websocket.value) return '未连接'

    switch (websocket.value.readyState) {
      case WebSocket.CONNECTING:
        return '连接中'
      case WebSocket.OPEN:
        return '已连接'
      case WebSocket.CLOSING:
        return '关闭中'
      case WebSocket.CLOSED:
        return '已关闭'
      default:
        return '未知'
    }
  }

  // 发送消息
  const sendMessage = (message) => {
    if (websocket.value && websocket.value.readyState === WebSocket.OPEN) {
      websocket.value.send(message)
      return true
    }
    addLog('WebSocket未连接，无法发送消息', 'error')
    return false
  }

  // 启动心跳机制
  const startHeartbeat = () => {
    if (heartbeatTimer) {
      clearInterval(heartbeatTimer)
    }

    heartbeatTimer = setInterval(() => {
      if (websocket.value && websocket.value.readyState === 1) {
        try {
          const heartbeatMessage = JSON.stringify({ type: 'heartbeat' })
          websocket.value.send(heartbeatMessage)
        } catch (error) {
          logger.error('发送心跳失败:', error)
        }
      }
    }, 30000) // 每30秒发送一次心跳
  }
  
  // 手动发送心跳消息
  const sendHeartbeat = () => {
    if (websocket.value && websocket.value.readyState === 1) {
      try {
        const heartbeatMessage = JSON.stringify({ type: 'heartbeat' })
        websocket.value.send(heartbeatMessage)
        return true
      } catch (error) {
        logger.error('手动发送心跳失败:', error)
        return false
      }
    }
    return false
  }

  // 停止心跳机制
  const stopHeartbeat = () => {
    if (heartbeatTimer) {
      clearInterval(heartbeatTimer)
      heartbeatTimer = null
    }
  }

  // 新增：注册消息处理器
  const registerMessageHandler = (handlerName, handler) => {
    messageHandlers.set(handlerName, handler)
  }

  // 新增：移除消息处理器
  const unregisterMessageHandler = (handlerName) => {
    if (messageHandlers.has(handlerName)) {
      messageHandlers.delete(handlerName)
    }
  }
  
  // 性能数据验证函数
  const validatePerformanceData = (data) => {
    // 检查必需字段
    if (!data.device_ip) {

      return false
    }
    
    // 验证时间戳
    if (!data.timestamp) {
      data.timestamp = Date.now()
    }
    
    // 验证性能指标范围
    const metrics = [
      'cpu_usage', 'memory_usage', 'npu_usage', 'disk_usage',
      'load_average_1m', 'load_average_5m', 'load_average_15m',
      'memory_available', 'memory_total', 'npu_memory_usage',
      'npu_power', 'npu_temperature', 'system_temperature'
    ]
    
    metrics.forEach(metric => {
      if (data[metric] !== undefined) {
        // 确保数值类型
        data[metric] = parseFloat(data[metric])
        
        // 对于百分比指标，确保在0-100之间
        if (['cpu_usage', 'memory_usage', 'npu_usage', 'disk_usage', 'npu_memory_usage'].includes(metric)) {
          data[metric] = Math.max(0, Math.min(100, data[metric]))
        }
        
        // 对于温度指标，确保合理范围
        if (['npu_temperature', 'system_temperature'].includes(metric)) {
          data[metric] = Math.max(-273, Math.min(200, data[metric]))
        }
        
        // 对于负值指标，确保非负
        if (data[metric] < 0) {
          data[metric] = 0
        }
      }
    })
    
    return true
  }
  
  // 性能数据标准化函数
  const normalizePerformanceData = (data) => {
    // 确保数据结构一致
    const normalizedData = {
      type: 'performance_data',
      device_ip: data.device_ip,
      timestamp: data.timestamp || Date.now(),
      connected: data.connected !== undefined ? data.connected : true,
      cpu_usage: data.cpu_usage || 0,
      memory_usage: data.memory_usage || 0,
      npu_usage: data.npu_usage || 0,
      disk_usage: data.disk_usage || 0,
      load_average_1m: data.load_average_1m || 0,
      load_average_5m: data.load_average_5m || 0,
      load_average_15m: data.load_average_15m || 0,
      memory_available: data.memory_available || 0,
      memory_total: data.memory_total || 0,
      npu_memory_usage: data.npu_memory_usage || 0,
      npu_power: data.npu_power || 0,
      npu_temperature: data.npu_temperature || 0,
      system_temperature: data.system_temperature || 0,
      ...data
    }
    
    return normalizedData
  }
  
  // 添加性能数据到缓存
  const addToPerformanceCache = (data) => {
    // 验证数据
    if (!validatePerformanceData(data)) {
      return
    }
    
    // 标准化数据
    const normalizedData = normalizePerformanceData(data)
    
    const deviceIp = normalizedData.device_ip
    
    // 检查是否需要更新缓存（只在数据有意义变化时更新）
    const existingData = performanceDataCache.get(deviceIp)
    if (existingData) {
      // 检查数据是否有显著变化
      const hasSignificantChange = checkPerformanceDataChange(normalizedData, existingData)
      if (!hasSignificantChange) {
        if (logConfig.enableDebugLogs) {

        }
        return
      }
    }
    
    // 备份性能数据
    backupPerformanceData(normalizedData)
    
    // 更新缓存中的设备数据
    performanceDataCache.set(deviceIp, {
      ...normalizedData,
      timestamp: Date.now() // 添加时间戳，便于后续处理
    })
    
    // 如果还没有启动批处理定时器，则启动它
    if (!performanceBatchTimer) {
      performanceBatchTimer = setTimeout(processPerformanceBatch, BATCH_INTERVAL)
    }
  }
  
  // 检查性能数据变化是否显著
  const checkPerformanceDataChange = (newData, oldData) => {
    const significantMetrics = ['cpu_usage', 'memory_usage', 'npu_usage', 'disk_usage']
    const changeThreshold = 5 // 变化阈值：5%
    
    // 检查是否有任何显著指标发生了显著变化
    for (const metric of significantMetrics) {
      const newValue = newData[metric] || 0
      const oldValue = oldData[metric] || 0
      const change = Math.abs(newValue - oldValue)
      
      if (change >= changeThreshold) {
        return true
      }
    }
    
    // 检查连接状态是否变化
    if (newData.connected !== oldData.connected) {
      return true
    }
    
    // 检查是否是新设备
    if (!oldData.timestamp) {
      return true
    }
    
    // 检查时间戳，确保至少每30秒更新一次
    const timeDiff = newData.timestamp - oldData.timestamp
    if (timeDiff >= 30000) {
      return true
    }
    
    return false
  }
  
  // 批量处理性能数据
  const processPerformanceBatch = () => {
    if (performanceDataCache.size === 0) {
      performanceBatchTimer = null
      return
    }
    
    // 将缓存中的所有性能数据打包成一个批次
    const batchData = []
    const deviceIps = []
    
    performanceDataCache.forEach((data, ip) => {
      batchData.push(data)
      deviceIps.push(ip)
    })
    
    if (logConfig.enablePerformanceLogs) {

    }
    
    // 优化：按处理器类型分组处理
    const performanceHandlers = []
    const otherHandlers = []
    
    messageHandlers.forEach((handler, key) => {
      if (key.includes('performance')) {
        performanceHandlers.push(handler)
      } else {
        otherHandlers.push(handler)
      }
    })
    
    // 处理性能数据处理器
    if (performanceHandlers.length > 0) {
      const batchMessage = { 
        type: 'performance_batch', 
        devices: batchData,
        timestamp: Date.now(),
        deviceCount: batchData.length
      }
      
      performanceHandlers.forEach(handler => {
        try {
          handler(batchMessage)
        } catch (error) {
          logger.error(`[连接 ${connectionId}] 性能数据处理器错误:`, error)
        }
      })
    }
    
    // 处理其他处理器
    if (otherHandlers.length > 0) {
      batchData.forEach(data => {
        otherHandlers.forEach(handler => {
          try {
            handler(data)
          } catch (error) {
            logger.error(`[连接 ${connectionId}] 消息处理器错误:`, error)
          }
        })
      })
    }
    
    // 清空缓存
    performanceDataCache.clear()
    
    // 重置定时器
    performanceBatchTimer = null
  }

  // 更新日志配置
  const updateLogConfig = (newConfig) => {
    Object.assign(logConfig, newConfig)
  }

  // 获取当前日志配置
  const getLogConfig = () => {
    return { ...logConfig }
  }

  return {
    // 核心WebSocket功能
    wsConnected,
    websocket,
    logs,
    addLog,
    getLogClass,
    connectWebSocket,
    closeWebSocket,
    setScanProgressCallback,
    getConnectionStatus,
    sendMessage,
    registerMessageHandler,
    unregisterMessageHandler,
    sendHeartbeat,
    startHeartbeat,
    updateLogConfig,
    getLogConfig,
    
    // 性能数据管理
    addToPerformanceCache,
    processPerformanceBatch,
    checkPerformanceDataAbnormal,
    
    // 历史数据和分析
    getDevicePerformanceHistory,
    clearDevicePerformanceHistory,
    getDeviceHistoricalData,
    analyzeDeviceHistoricalData,
    
    // 阈值管理
    adjustDynamicThresholds,
    getDeviceThresholds,
    
    // 本地存储
    savePerformanceDataToLocalStorage,
    loadPerformanceDataFromLocalStorage,
    cleanupLocalStorage,
    
    // 故障容错
    checkSystemHealth,
    backupPerformanceData,
    restorePerformanceData,
    
    // 配置
    storageConfig,
    faultToleranceConfig,
    alertConfig
  }
}