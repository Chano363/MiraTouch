// 日志工具类 - 统一的日志处理

const LOG_LEVELS = {
  DEBUG: 0,
  INFO: 1,
  WARN: 2,
  ERROR: 3,
  OFF: 4
};

// 日志面板回调存储
let logPanelCallback = null;

// 设置日志面板回调
export function setLogPanelCallback(callback) {
  logPanelCallback = callback;
  console.debug('日志面板回调已设置');
}

// 获取日志面板回调
export function getLogPanelCallback() {
  return logPanelCallback;
}

class Logger {
  constructor(moduleName = 'unknown', options = {}) {
    this.moduleName = moduleName;
    this.logLevel = options.logLevel || LOG_LEVELS.INFO; // 默认日志级别改为INFO，记录信息和错误
    
    // 日志分类配置
    this.categoryConfig = {
      enableHeartbeat: options.enableHeartbeat || false,
      enablePerformance: options.enablePerformance || true, // 默认启用性能日志
      enableScan: options.enableScan || true, // 默认启用扫描日志
      enableDevice: options.enableDevice || true, // 默认启用设备日志
      enableConfig: options.enableConfig || true, // 默认启用配置日志
      enableApi: options.enableApi || true, // 默认启用API日志
      enableWebSocket: options.enableWebSocket || true,
      enableDebug: options.enableDebug || false,
      enableSystem: options.enableSystem || true, // 默认启用系统日志
      enableSecurity: options.enableSecurity || false, // 默认禁用安全日志
      ...options.categoryConfig
    };
    
    // 日志轮转配置
    this.rotationConfig = {
      maxLogCount: options.maxLogCount || 500, // 增加日志数量限制
      enableBatching: options.enableBatching || true, // 默认启用批处理
      batchInterval: options.batchInterval || 5000, // 减少批处理间隔到5秒
      batchSize: options.batchSize || 20, // 调整批处理大小
      enableCompression: options.enableCompression || false,
      enableConsoleOutput: options.enableConsoleOutput || true, // 默认启用控制台输出
      enableLogPanel: options.enableLogPanel || true, // 默认启用日志面板推送
      ...options.rotationConfig
    };
    
    // 日志缓冲区
    this.logBuffer = [];
    // 批处理定时器
    this.batchTimer = null;
    // 日志面板推送节流
    this.panelThrottleCache = new Map();
    this.panelThrottleInterval = 500; // 日志面板推送节流间隔：500毫秒，提高响应速度
    this.panelThrottleTimer = null;
    // 日志计数和限制
    this.logCount = 0;
    this.maxLogCountPerMinute = 100; // 每分钟最多100条日志
    this.logTimestamps = [];
    // 启动批处理机制
    this.startBatchProcessing();
    // 启动日志清理
    this.startLogCleanup();
  }
  
  // 启动批处理机制
  startBatchProcessing() {
    if (this.rotationConfig.enableBatching) {
      this.batchTimer = setInterval(() => {
        this.flushLogBuffer();
      }, this.rotationConfig.batchInterval);
    }
  }
  
  // 启动日志清理
  startLogCleanup() {
    // 每10分钟清理一次过期日志时间戳
    this.logCleanupTimer = setInterval(() => {
      const now = Date.now();
      this.logTimestamps = this.logTimestamps.filter(timestamp => now - timestamp < 60000);
    }, 10 * 60 * 1000);
  }
  
  // 停止批处理机制
  stopBatchProcessing() {
    if (this.batchTimer) {
      clearInterval(this.batchTimer);
      this.batchTimer = null;
    }
  }
  
  // 停止日志清理
  stopLogCleanup() {
    if (this.logCleanupTimer) {
      clearInterval(this.logCleanupTimer);
      this.logCleanupTimer = null;
    }
  }
  
  // 刷新日志缓冲区
  flushLogBuffer() {
    if (this.logBuffer.length > 0) {
      const batchLogs = [...this.logBuffer];
      this.logBuffer = [];
      
      // 只在启用控制台输出时才输出批处理日志
      if (this.rotationConfig.enableConsoleOutput) {
        // 按级别分组批处理
        const logsByLevel = {};
        batchLogs.forEach(log => {
          if (!logsByLevel[log.level]) {
            logsByLevel[log.level] = [];
          }
          logsByLevel[log.level].push(log);
        });
        
        // 按级别输出
        Object.entries(logsByLevel).forEach(([level, logs]) => {
          console.log(`[${this.moduleName}] 批量处理 ${logs.length} 条${level}日志:`);
          // 只输出前5条日志，避免过多输出
          const limitedLogs = logs.slice(0, 5);
          limitedLogs.forEach(log => {
            console.log(`  ${log.message}`);
            if (log.data && Object.keys(log.data).length > 0) {
              console.log(`    详情:`, log.data);
            }
          });
          // 如果有更多日志，只输出数量
          if (logs.length > 5) {
            console.log(`  ... 还有 ${logs.length - 5} 条日志未显示`);
          }
        });
      }
    }
  }
  
  // 销毁实例
  destroy() {
    this.stopBatchProcessing();
    this.stopLogCleanup();
    this.flushLogBuffer();
    
    // 清除节流定时器
    if (this.panelThrottleTimer) {
      clearTimeout(this.panelThrottleTimer);
      this.panelThrottleTimer = null;
    }
    
    // 清空节流缓存
    this.panelThrottleCache.clear();
  }

  // 设置日志级别
  setLogLevel(level) {
    this.logLevel = level;
  }

  // 设置日志分类配置
  setCategoryConfig(config) {
    this.categoryConfig = {
      ...this.categoryConfig,
      ...config
    };
  }

  // 获取当前日志分类配置
  getCategoryConfig() {
    return { ...this.categoryConfig };
  }

  // 生成结构化日志消息
  _formatMessage(level, message, data = null) {
    const timestamp = new Date().toLocaleString('zh-CN', {
      year: 'numeric',
      month: '2-digit',
      day: '2-digit',
      hour: '2-digit',
      minute: '2-digit',
      second: '2-digit',
      hour12: false
    });
    const levelName = Object.keys(LOG_LEVELS).find(key => LOG_LEVELS[key] === level);
    
    // 优化日志消息，去除冗余技术细节
    let optimizedMessage = message;
    let optimizedData = data;
    
    // 处理WebSocket消息，去除冗余技术细节
    if (data && data.type === 'websocket') {
      optimizedData = {
        type: data.subtype || data.type,
        device: data.device_ip || data.device,
        ...(data.message ? { message: data.message } : {})
      };
    }
    
    // 处理性能数据，只保留关键指标
    if (data && data.type === 'performance') {
      optimizedData = {
        device: data.device_ip,
        cpu: data.cpu_usage,
        memory: data.memory_usage,
        ...(data.npu_usage !== undefined ? { npu: data.npu_usage } : {}),
        ...(data.system_temperature !== undefined ? { temp: data.system_temperature } : {})
      };
    }
    
    // 处理API请求，去除敏感信息
    if (data && data.type === 'api') {
      optimizedData = {
        url: data.url,
        method: data.method,
        status: data.status,
        ...(data.params ? { params: this._sanitizeParams(data.params) } : {})
      };
    }
    
    // 处理设备相关消息
    if (data && data.type === 'device') {
      optimizedData = {
        device: data.device_ip || data.device,
        action: data.action,
        status: data.status
      };
    }
    
    // 处理系统消息
    if (data && data.type === 'system') {
      optimizedData = {
        event: data.event,
        status: data.status,
        ...(data.details ? { details: data.details } : {})
      };
    }
    
    const logMessage = {
      timestamp,
      level: levelName,
      module: this.moduleName,
      message: optimizedMessage,
      data: optimizedData
    };

    return logMessage;
  }

  // 检查日志数量限制
  _checkLogLimit() {
    const now = Date.now();
    // 清理1分钟前的日志时间戳
    this.logTimestamps = this.logTimestamps.filter(timestamp => now - timestamp < 60000);
    
    // 检查是否超过每分钟日志限制
    if (this.logTimestamps.length >= this.maxLogCountPerMinute) {
      return false;
    }
    
    // 记录当前日志时间戳
    this.logTimestamps.push(now);
    return true;
  }

  // 输出日志
  _log(level, message, data = null, category = null) {
    if (level < this.logLevel) return;

    // 检查日志分类是否启用
    if (category) {
      const categoryKey = `enable${category.charAt(0).toUpperCase() + category.slice(1)}`;
      if (!this.categoryConfig[categoryKey]) {
        return;
      }
    }

    // 检查日志数量限制
    if (!this._checkLogLimit()) {
      return;
    }

    const logMessage = this._formatMessage(level, message, data);

    // 错误级别日志直接输出，不进行批处理
    if (level >= LOG_LEVELS.ERROR) {
      // 只在启用控制台输出时才输出
      if (this.rotationConfig.enableConsoleOutput) {
        console.error(`\x1b[31m[${logMessage.level}] [${this.moduleName}] ${message}\x1b[0m`);
        if (logMessage.data) {
          console.error('\x1b[31m  详情:\x1b[0m', logMessage.data);
        }
      }
      
      // 只在启用日志面板时才推送
      if (this.rotationConfig.enableLogPanel) {
        this._sendToLogPanel(logMessage, category);
      }
      return;
    }

    // 批处理模式
    if (this.rotationConfig.enableBatching && level < LOG_LEVELS.ERROR) {
      // 将日志添加到缓冲区
      this.logBuffer.push({
        level: logMessage.level,
        message: logMessage.message,
        data: logMessage.data,
        timestamp: logMessage.timestamp,
        category: category
      });
      
      // 如果缓冲区达到批处理大小，立即处理
      if (this.logBuffer.length >= this.rotationConfig.batchSize) {
        this.flushLogBuffer();
      }
    } else {
      // 非批处理模式，直接输出
      if (this.rotationConfig.enableConsoleOutput) {
        switch (level) {
          case LOG_LEVELS.DEBUG:
            console.debug(`\x1b[36m[${logMessage.level}] [${this.moduleName}] ${message}\x1b[0m`);
            if (logMessage.data) {
              console.debug('\x1b[36m  详情:\x1b[0m', logMessage.data);
            }
            break;
          case LOG_LEVELS.INFO:
            console.info(`\x1b[32m[${logMessage.level}] [${this.moduleName}] ${message}\x1b[0m`);
            if (logMessage.data) {
              console.info('\x1b[32m  详情:\x1b[0m', logMessage.data);
            }
            break;
          case LOG_LEVELS.WARN:
            console.warn(`\x1b[33m[${logMessage.level}] [${this.moduleName}] ${message}\x1b[0m`);
            if (logMessage.data) {
              console.warn('\x1b[33m  详情:\x1b[0m', logMessage.data);
            }
            break;
        }
      }
    }

    // 只在启用日志面板时才推送
    if (this.rotationConfig.enableLogPanel) {
      this._sendToLogPanel(logMessage, category);
    }
  }

  // 发送日志到日志面板
  _sendToLogPanel(logMessage, category) {
    if (logPanelCallback && typeof logPanelCallback === 'function') {
      try {
        const panelMessage = {
          ...logMessage,
          category
        };
        
        // 错误级别日志直接推送，不进行节流
        if (logMessage.level === 'ERROR') {
          logPanelCallback(panelMessage);
          return;
        }
        
        // 其他级别的日志使用节流
        const cacheKey = `${category || 'default'}_${logMessage.level}`;
        
        // 将日志添加到缓存
        this.panelThrottleCache.set(cacheKey, panelMessage);
        
        // 清除之前的定时器
        if (this.panelThrottleTimer) {
          clearTimeout(this.panelThrottleTimer);
        }
        
        // 设置新的定时器，在节流间隔后批量推送
        this.panelThrottleTimer = setTimeout(() => {
          try {
            // 批量推送缓存的日志
            this.panelThrottleCache.forEach((message) => {
              logPanelCallback(message);
            });
            // 清空缓存
            this.panelThrottleCache.clear();
          } catch (error) {
            console.error('批量推送日志失败:', error);
          }
        }, this.panelThrottleInterval);
      } catch (error) {
        console.error('日志面板回调执行失败:', error);
      }
    }
  }

  // 调试级别日志
  debug(message, data = null, category = null) {
    this._log(LOG_LEVELS.DEBUG, message, data, category);
  }

  // 信息级别日志
  info(message, data = null, category = null) {
    this._log(LOG_LEVELS.INFO, message, data, category);
  }

  // 警告级别日志
  warn(message, data = null, category = null) {
    this._log(LOG_LEVELS.WARN, message, data, category);
  }

  // 错误级别日志
  error(message, data = null, category = null) {
    this._log(LOG_LEVELS.ERROR, message, data, category);
  }

  // 记录API请求
  apiRequest(url, method, params = null) {
    // 优化API请求日志，去除敏感参数
    const safeParams = this._sanitizeParams(params);
    this.info(`API请求: ${method} ${url}`, { params: safeParams }, 'api');
  }

  // 记录API响应
  apiResponse(url, method, status, data = null) {
    if (status >= 200 && status < 300) {
      // 成功响应只记录状态，不记录详细数据
      this.info(`API响应: ${method} ${url} ${status}`, null, 'api');
    } else {
      // 错误响应记录详细信息
      this.error(`API响应错误: ${method} ${url} ${status}`, { data }, 'api');
    }
  }

  // 记录设备扫描
  scanStart(mode) {
    this.info('设备扫描开始', { mode }, 'scan');
  }

  scanComplete(deviceCount, duration) {
    this.info(`设备扫描完成，发现 ${deviceCount} 个设备`, { duration }, 'scan');
  }

  scanError(error) {
    this.error('设备扫描失败', { error: error.message || error }, 'scan');
  }

  // 记录设备连接
  deviceConnectStart(ip) {
    this.info(`设备连接开始: ${ip}`, null, 'device');
  }

  deviceConnectComplete(ip, success, message) {
    if (success) {
      this.info(`设备连接成功: ${ip}`, null, 'device');
    } else {
      this.error(`设备连接失败: ${ip}`, { message }, 'device');
    }
  }

  // 记录WebSocket消息
  websocketMessage(type, data = null) {
    // 特别处理心跳消息
    if (type === 'heartbeat') {
      this.debug('WebSocket心跳', null, 'heartbeat');
    } else {
      // 优化WebSocket消息，去除冗余技术细节
      const optimizedData = data ? {
        type: type,
        device: data.device_ip || data.device,
        ...(data.message ? { message: data.message } : {})
      } : null;
      this.debug('WebSocket消息', optimizedData, 'websocket');
    }
  }

  // 记录性能数据
  performanceData(data) {
    // 优化性能数据日志，只保留关键指标
    const optimizedData = {
      device: data.device_ip,
      cpu: data.cpu_usage,
      memory: data.memory_usage,
      ...(data.npu_usage !== undefined ? { npu: data.npu_usage } : {}),
      ...(data.system_temperature !== undefined ? { temp: data.system_temperature } : {})
    };
    this.debug('性能数据', optimizedData, 'performance');
  }

  // 记录系统事件
  systemEvent(event, data = null) {
    this.info(`系统事件: ${event}`, data, 'system');
  }

  // 清理敏感参数
  _sanitizeParams(params) {
    if (!params || typeof params !== 'object') {
      return params;
    }
    
    const sanitized = { ...params };
    const sensitiveFields = ['password', 'token', 'secret', 'key'];
    
    sensitiveFields.forEach(field => {
      if (sanitized[field]) {
        sanitized[field] = '***';
      }
    });
    
    return sanitized;
  }
}

// 导出日志级别常量
export { LOG_LEVELS };

// 导出默认实例
export default Logger;

// 导出便捷函数 - 创建日志实例
export function createLogger(moduleName) {
  return new Logger(moduleName);
}
