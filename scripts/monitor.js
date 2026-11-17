const http = require('http');
const fs = require('fs');
const path = require('path');

// 监控配置
const config = {
  serverUrl: 'http://localhost:8082',
  checkInterval: 30000, // 30秒检查一次
  logFile: path.join(__dirname, '../logs/monitor.log'),
  alertThreshold: 5000, // 响应时间超过5秒告警
};

// 确保日志目录存在
const logDir = path.dirname(config.logFile);
if (!fs.existsSync(logDir)) {
  fs.mkdirSync(logDir, { recursive: true });
}

// 日志记录函数
function log(message, level = 'info') {
  const timestamp = new Date().toISOString();
  const logMessage = `[${timestamp}] [${level.toUpperCase()}] ${message}\n`;
  console.log(logMessage);
  fs.appendFileSync(config.logFile, logMessage);
}

// 检查服务器健康状态
function checkServerHealth() {
  log('Starting server health check');
  
  // 检查服务器是否可访问
  const start = Date.now();
  
  http.get(`${config.serverUrl}/api/devices/list`, (res) => {
    const end = Date.now();
    const responseTime = end - start;
    
    if (res.statusCode === 200) {
      log(`Server health check passed. Response time: ${responseTime}ms`);
      
      if (responseTime > config.alertThreshold) {
        log(`ALERT: Server response time is high: ${responseTime}ms`, 'warning');
      }
    } else {
      log(`Server health check failed. Status code: ${res.statusCode}`, 'error');
    }
  }).on('error', (err) => {
    log(`Server health check failed. Error: ${err.message}`, 'error');
  });
}

// 检查配置API
function checkConfigApi() {
  log('Starting config API check');
  
  // 测试配置发送API
  const testData = {
    ip_address: '192.168.1.100',
    username: 'chano',
    config: {
      batchSize: 1
    }
  };
  
  const options = {
    hostname: 'localhost',
    port: 8082,
    path: '/api/config/send',
    method: 'POST',
    headers: {
      'Content-Type': 'application/json',
      'Content-Length': Buffer.byteLength(JSON.stringify(testData))
    }
  };
  
  const start = Date.now();
  const req = http.request(options, (res) => {
    const end = Date.now();
    const responseTime = end - start;
    
    let data = '';
    res.on('data', (chunk) => {
      data += chunk;
    });
    
    res.on('end', () => {
      try {
        const result = JSON.parse(data);
        log(`Config send API check completed. Response time: ${responseTime}ms, Success: ${result.success}`);
        
        if (!result.success) {
          log(`Config send API returned error: ${result.error}`, 'warning');
        }
      } catch (error) {
        log(`Failed to parse config send API response: ${error.message}`, 'error');
      }
    });
  });
  
  req.on('error', (err) => {
    log(`Config send API check failed: ${err.message}`, 'error');
  });
  
  req.write(JSON.stringify(testData));
  req.end();
  
  // 测试配置读取API
  setTimeout(() => {
    const readOptions = {
      hostname: 'localhost',
      port: 8082,
      path: '/api/config/status',
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        'Content-Length': Buffer.byteLength(JSON.stringify({ ip_address: '192.168.1.100', username: 'chano' }))
      }
    };
    
    const readStart = Date.now();
    const readReq = http.request(readOptions, (res) => {
      const readEnd = Date.now();
      const readResponseTime = readEnd - readStart;
      
      let data = '';
      res.on('data', (chunk) => {
        data += chunk;
      });
      
      res.on('end', () => {
        try {
          const result = JSON.parse(data);
          log(`Config status API check completed. Response time: ${readResponseTime}ms, Success: ${result.success}`);
          
          if (!result.success) {
            log(`Config status API returned error: ${result.error}`, 'warning');
          }
        } catch (error) {
          log(`Failed to parse config status API response: ${error.message}`, 'error');
        }
      });
    });
    
    readReq.on('error', (err) => {
      log(`Config status API check failed: ${err.message}`, 'error');
    });
    
    readReq.write(JSON.stringify({ ip_address: '192.168.1.100', username: 'chano' }));
    readReq.end();
  }, 1000);
}

// 执行完整监控
function runMonitoring() {
  log('====================================');
  log('Running complete system monitoring');
  log('====================================');
  
  checkServerHealth();
  checkConfigApi();
  
  log('Monitoring cycle completed');
  log('====================================');
}

// 初始运行
runMonitoring();

// 设置定时任务
setInterval(runMonitoring, config.checkInterval);

log(`Monitoring started. Checking every ${config.checkInterval / 1000} seconds`);
log(`Monitoring log file: ${config.logFile}`);
