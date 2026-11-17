const assert = require('assert');

describe('Log Panel Logs Tests', function() {
  // 模拟logger.js中的相关功能
  const LOG_LEVELS = {
    DEBUG: 0,
    INFO: 1,
    WARN: 2,
    ERROR: 3,
    OFF: 4
  };

  // 模拟日志面板回调存储
  let logPanelCallback = null;

  // 模拟设置日志面板回调
  const setLogPanelCallback = (callback) => {
    logPanelCallback = callback;
  };

  // 模拟获取日志面板回调
  const getLogPanelCallback = () => {
    return logPanelCallback;
  };

  // 模拟Logger类
  class Logger {
    constructor(moduleName = 'unknown', options = {}) {
      this.moduleName = moduleName;
      this.logLevel = options.logLevel || LOG_LEVELS.DEBUG; // 改为DEBUG级别，以便测试所有级别
      this.logs = [];
    }

    // 模拟日志输出
    _log(level, message, data = null, category = null) {
      if (level < this.logLevel) return;

      const timestamp = new Date().toLocaleString('zh-CN');
      const levelName = Object.keys(LOG_LEVELS).find(key => LOG_LEVELS[key] === level);

      const logMessage = {
        timestamp,
        level: levelName,
        module: this.moduleName,
        message,
        data: data !== undefined ? data : null, // 保持与实际实现一致
        category
      };

      this.logs.push(logMessage);

      // 调用日志面板回调
      if (logPanelCallback) {
        logPanelCallback(logMessage);
      }

      return logMessage;
    }

    debug(message, data = null, category = null) {
      return this._log(LOG_LEVELS.DEBUG, message, data, category);
    }

    info(message, data = null, category = null) {
      return this._log(LOG_LEVELS.INFO, message, data, category);
    }

    warn(message, data = null, category = null) {
      return this._log(LOG_LEVELS.WARN, message, data, category);
    }

    error(message, data = null, category = null) {
      return this._log(LOG_LEVELS.ERROR, message, data, category);
    }

    getLogs() {
      return this.logs;
    }
  }

  // 测试日志生成和传递
  describe('Log Generation and Delivery', function() {
    it('should generate debug logs correctly', function() {
      const logger = new Logger('test-module');
      const log = logger.debug('Debug message', { key: 'value' }, 'debug');

      assert.strictEqual(log.level, 'DEBUG');
      assert.strictEqual(log.message, 'Debug message');
      assert.deepStrictEqual(log.data, { key: 'value' });
      assert.strictEqual(log.category, 'debug');
      assert.strictEqual(log.module, 'test-module');
    });

    it('should generate info logs correctly', function() {
      const logger = new Logger('test-module');
      const log = logger.info('Info message', { key: 'value' }, 'info');

      assert.strictEqual(log.level, 'INFO');
      assert.strictEqual(log.message, 'Info message');
      assert.deepStrictEqual(log.data, { key: 'value' });
      assert.strictEqual(log.category, 'info');
      assert.strictEqual(log.module, 'test-module');
    });

    it('should generate warn logs correctly', function() {
      const logger = new Logger('test-module');
      const log = logger.warn('Warn message', { key: 'value' }, 'warn');

      assert.strictEqual(log.level, 'WARN');
      assert.strictEqual(log.message, 'Warn message');
      assert.deepStrictEqual(log.data, { key: 'value' });
      assert.strictEqual(log.category, 'warn');
      assert.strictEqual(log.module, 'test-module');
    });

    it('should generate error logs correctly', function() {
      const logger = new Logger('test-module');
      const log = logger.error('Error message', { key: 'value' }, 'error');

      assert.strictEqual(log.level, 'ERROR');
      assert.strictEqual(log.message, 'Error message');
      assert.deepStrictEqual(log.data, { key: 'value' });
      assert.strictEqual(log.category, 'error');
      assert.strictEqual(log.module, 'test-module');
    });

    it('should call log panel callback when logs are generated', function() {
      const logger = new Logger('test-module');
      const logs = [];

      // 设置日志面板回调
      setLogPanelCallback((log) => {
        logs.push(log);
      });

      // 生成日志
      logger.info('Test message');

      // 验证回调被调用
      assert.strictEqual(logs.length, 1);
      assert.strictEqual(logs[0].message, 'Test message');

      // 清理
      setLogPanelCallback(null);
    });
  });

  // 测试日志过滤和搜索
  describe('Log Filtering and Search', function() {
    let logger;
    let logs;

    beforeEach(function() {
      logger = new Logger('test-module');
      logs = [];

      // 设置日志面板回调
      setLogPanelCallback((log) => {
        logs.push(log);
      });

      // 生成测试日志
      logger.info('Info message 1');
      logger.warn('Warn message 1');
      logger.error('Error message 1');
      logger.info('Info message 2');
      logger.debug('Debug message 1');
    });

    afterEach(function() {
      // 清理
      setLogPanelCallback(null);
    });

    it('should filter logs by level', function() {
      // 过滤INFO级别日志
      const infoLogs = logs.filter(log => log.level === 'INFO');
      assert.strictEqual(infoLogs.length, 2);
      assert.strictEqual(infoLogs[0].message, 'Info message 1');
      assert.strictEqual(infoLogs[1].message, 'Info message 2');

      // 过滤ERROR级别日志
      const errorLogs = logs.filter(log => log.level === 'ERROR');
      assert.strictEqual(errorLogs.length, 1);
      assert.strictEqual(errorLogs[0].message, 'Error message 1');
    });

    it('should search logs by message content', function() {
      // 搜索包含'message 1'的日志
      const filteredLogs = logs.filter(log => log.message.includes('message 1'));
      assert.strictEqual(filteredLogs.length, 4); // Info, Warn, Error, Debug

      // 搜索包含'Info'的日志
      const infoLogs = logs.filter(log => log.message.includes('Info'));
      assert.strictEqual(infoLogs.length, 2);
    });

    it('should sort logs by timestamp', function() {
      // 验证日志按时间顺序排列
      for (let i = 0; i < logs.length - 1; i++) {
        assert(logs[i].timestamp <= logs[i + 1].timestamp, 'Logs should be sorted by timestamp');
      }
    });
  });

  // 测试日志级别控制
  describe('Log Level Control', function() {
    it('should respect log level settings', function() {
      // 创建只记录ERROR级别的logger
      const logger = new Logger('test-module', { logLevel: LOG_LEVELS.ERROR });
      const logs = [];

      // 设置日志面板回调
      setLogPanelCallback((log) => {
        logs.push(log);
      });

      // 生成不同级别的日志
      logger.debug('Debug message'); // 应该被过滤
      logger.info('Info message');   // 应该被过滤
      logger.warn('Warn message');   // 应该被过滤
      logger.error('Error message'); // 应该被记录

      // 验证只有ERROR级别日志被记录
      assert.strictEqual(logs.length, 1);
      assert.strictEqual(logs[0].level, 'ERROR');
      assert.strictEqual(logs[0].message, 'Error message');

      // 清理
      setLogPanelCallback(null);
    });
  });

  // 测试日志数据结构
  describe('Log Data Structure', function() {
    it('should include all required fields in log objects', function() {
      const logger = new Logger('test-module');
      const log = logger.info('Test message', { data: 'value' }, 'test');

      assert(log.hasOwnProperty('timestamp'), 'Log should have timestamp property');
      assert(log.hasOwnProperty('level'), 'Log should have level property');
      assert(log.hasOwnProperty('module'), 'Log should have module property');
      assert(log.hasOwnProperty('message'), 'Log should have message property');
      assert(log.hasOwnProperty('data'), 'Log should have data property');
      assert(log.hasOwnProperty('category'), 'Log should have category property');
    });

    it('should handle null data correctly', function() {
      const logger = new Logger('test-module');
      const log = logger.info('Test message', null);

      assert.strictEqual(log.data, null);
    });

    it('should handle undefined data correctly', function() {
      const logger = new Logger('test-module');
      const log = logger.info('Test message');

      assert.strictEqual(log.data, null); // 与实际实现一致，undefined会被转换为null
    });
  });
});
