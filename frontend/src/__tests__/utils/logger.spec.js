import { describe, it, expect, vi, beforeEach } from 'vitest';
import { createLogger, setLogPanelCallback, getLogPanelCallback } from '../../utils/logger';

describe('logger.js', () => {
  let logger;
  let mockCallback;

  beforeEach(() => {
    // Create a mock callback function
    mockCallback = vi.fn();
    
    // Create a logger instance
    logger = createLogger('test-module');
  });

  describe('Basic logging functionality', () => {
    it('should create a logger instance with module name', () => {
      expect(logger).toBeDefined();
      expect(typeof logger.debug).toBe('function');
      expect(typeof logger.info).toBe('function');
      expect(typeof logger.warn).toBe('function');
      expect(typeof logger.error).toBe('function');
    });

    it('should log debug messages', () => {
      // Mock console.log
      const consoleLogSpy = vi.spyOn(console, 'log').mockImplementation();
      
      logger.debug('Debug message');
      
      expect(consoleLogSpy).toHaveBeenCalled();
      
      consoleLogSpy.mockRestore();
    });

    it('should log info messages', () => {
      // Mock console.log
      const consoleLogSpy = vi.spyOn(console, 'log').mockImplementation();
      
      logger.info('Info message');
      
      expect(consoleLogSpy).toHaveBeenCalled();
      
      consoleLogSpy.mockRestore();
    });

    it('should log warn messages', () => {
      // Mock console.warn
      const consoleWarnSpy = vi.spyOn(console, 'warn').mockImplementation();
      
      logger.warn('Warn message');
      
      expect(consoleWarnSpy).toHaveBeenCalled();
      
      consoleWarnSpy.mockRestore();
    });

    it('should log error messages', () => {
      // Mock console.error
      const consoleErrorSpy = vi.spyOn(console, 'error').mockImplementation();
      
      logger.error('Error message');
      
      expect(consoleErrorSpy).toHaveBeenCalled();
      
      consoleErrorSpy.mockRestore();
    });

    it('should log messages with data object', () => {
      // Mock console.log
      const consoleLogSpy = vi.spyOn(console, 'log').mockImplementation();
      
      logger.info('Message with data', { key: 'value', number: 42 });
      
      expect(consoleLogSpy).toHaveBeenCalled();
      
      consoleLogSpy.mockRestore();
    });
  });

  describe('Log panel integration', () => {
    it('should set log panel callback', () => {
      const { setLogPanelCallback, getLogPanelCallback } = require('../../utils/logger');
      
      setLogPanelCallback(mockCallback);
      
      expect(getLogPanelCallback()).toBe(mockCallback);
    });

    it('should call log panel callback when logging', () => {
      // Set the callback
      setLogPanelCallback(mockCallback);
      
      // Log a message
      logger.info('Test message for log panel');
      
      // Verify the callback was called
      expect(mockCallback).toHaveBeenCalled();
      
      // Check the structure of the logged message
      const logMessage = mockCallback.mock.calls[0][0];
      expect(logMessage).toHaveProperty('timestamp');
      expect(logMessage).toHaveProperty('level');
      expect(logMessage).toHaveProperty('message');
      expect(logMessage).toHaveProperty('module');
      expect(logMessage.level).toBe('INFO');
      expect(logMessage.message).toBe('Test message for log panel');
      expect(logMessage.module).toBe('test-module');
    });

    it('should handle log panel callback errors gracefully', () => {
      // Create a callback that throws an error
      const errorCallback = vi.fn(() => {
        throw new Error('Callback error');
      });
      
      setLogPanelCallback(errorCallback);
      
      // Mock console.error to capture the error
      const consoleErrorSpy = vi.spyOn(console, 'error').mockImplementation();
      
      // Log a message - should not throw
      expect(() => logger.info('Message with error callback')).not.toThrow();
      
      // Verify the error was caught and logged
      expect(consoleErrorSpy).toHaveBeenCalled();
      
      consoleErrorSpy.mockRestore();
    });
  });

  describe('Log formatting', () => {
    it('should format log messages correctly', () => {
      // Mock console.log to capture the formatted message
      const consoleLogSpy = vi.spyOn(console, 'log').mockImplementation((...args) => {
        // Check the format of the log message
        expect(args.length).toBeGreaterThan(0);
        // The first argument should be a string with timestamp, level, and module
        expect(typeof args[0]).toBe('string');
        expect(args[0]).toContain('DEBUG');
        expect(args[0]).toContain('test-module');
      });
      
      logger.debug('Formatted message');
      
      expect(consoleLogSpy).toHaveBeenCalled();
      
      consoleLogSpy.mockRestore();
    });
  });
});
