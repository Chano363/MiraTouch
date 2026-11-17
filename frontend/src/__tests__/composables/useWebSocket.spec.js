import { describe, it, expect, vi, beforeEach, afterEach } from 'vitest';
import { useWebSocket } from '../../composables/useWebSocket';

// Mock WebSocket
class MockWebSocket {
  constructor(url) {
    this.url = url;
    this.readyState = 0; // CONNECTING
    this.onopen = null;
    this.onmessage = null;
    this.onclose = null;
    this.onerror = null;
    this.messages = [];
  }

  connect() {
    this.readyState = 1; // OPEN
    if (this.onopen) {
      this.onopen();
    }
  }

  send(data) {
    this.messages.push(data);
  }

  close(code = 1000, reason = '') {
    this.readyState = 3; // CLOSED
    if (this.onclose) {
      this.onclose({ code, reason });
    }
  }

  simulateMessage(data) {
    if (this.onmessage) {
      this.onmessage({ data: JSON.stringify(data) });
    }
  }

  simulateError(error) {
    if (this.onerror) {
      this.onerror(error);
    }
  }
}

// Mock global WebSocket
vi.mock('ws', () => ({
  WebSocket: MockWebSocket
}));

global.WebSocket = MockWebSocket;

describe('useWebSocket.js', () => {
  let wsInstance;
  let mockWebSocket;

  beforeEach(() => {
    // Create a mock WebSocket instance
    mockWebSocket = new MockWebSocket('ws://localhost:8083');
    
    // Mock the WebSocket constructor to return our mock instance
    vi.spyOn(global, 'WebSocket').mockImplementation(() => mockWebSocket);
    
    // Create the useWebSocket instance
    wsInstance = useWebSocket();
  });

  afterEach(() => {
    vi.restoreAllMocks();
  });

  describe('Connection management', () => {
    it('should establish WebSocket connection', () => {
      // Call connectWebSocket
      wsInstance.connectWebSocket();
      
      // Simulate connection success
      mockWebSocket.connect();
      
      // Verify connection status
      expect(wsInstance.wsConnected.value).toBe(true);
    });

    it('should handle connection error', () => {
      // Call connectWebSocket
      wsInstance.connectWebSocket();
      
      // Simulate connection error
      mockWebSocket.simulateError(new Error('Connection error'));
      
      // Verify connection status
      expect(wsInstance.wsConnected.value).toBe(false);
    });

    it('should handle connection close', () => {
      // Call connectWebSocket
      wsInstance.connectWebSocket();
      mockWebSocket.connect();
      expect(wsInstance.wsConnected.value).toBe(true);
      
      // Simulate connection close
      mockWebSocket.close();
      
      // Verify connection status
      expect(wsInstance.wsConnected.value).toBe(false);
    });
  });

  describe('Message handling', () => {
    it('should register message handler', () => {
      const mockHandler = vi.fn();
      
      // Register message handler
      wsInstance.registerMessageHandler('test-handler', mockHandler);
      
      // Connect WebSocket
      wsInstance.connectWebSocket();
      mockWebSocket.connect();
      
      // Simulate message
      const testMessage = { type: 'test', data: 'test data' };
      mockWebSocket.simulateMessage(testMessage);
      
      // Verify handler was called
      expect(mockHandler).toHaveBeenCalledWith(testMessage);
    });

    it('should unregister message handler', () => {
      const mockHandler = vi.fn();
      
      // Register message handler
      wsInstance.registerMessageHandler('test-handler', mockHandler);
      
      // Unregister message handler
      wsInstance.unregisterMessageHandler('test-handler');
      
      // Connect WebSocket
      wsInstance.connectWebSocket();
      mockWebSocket.connect();
      
      // Simulate message
      const testMessage = { type: 'test', data: 'test data' };
      mockWebSocket.simulateMessage(testMessage);
      
      // Verify handler was not called
      expect(mockHandler).not.toHaveBeenCalled();
    });

    it('should handle scan progress messages', () => {
      // Connect WebSocket
      wsInstance.connectWebSocket();
      mockWebSocket.connect();
      
      // Simulate scan progress message
      const scanMessage = {
        type: 'scan_progress',
        status: 'completed',
        device_count: 5
      };
      
      mockWebSocket.simulateMessage(scanMessage);
      
      // Verify the message was processed (logs should be updated)
      expect(wsInstance.logs.value.length).toBeGreaterThan(0);
      expect(wsInstance.logs.value[wsInstance.logs.value.length - 1].message).toContain('设备扫描完成');
    });

    it('should handle device found messages', () => {
      // Connect WebSocket
      wsInstance.connectWebSocket();
      mockWebSocket.connect();
      
      // Simulate device found message
      const deviceMessage = {
        type: 'device_found',
        device: {
          ip_address: '192.168.1.100',
          hostname: 'test-device'
        }
      };
      
      mockWebSocket.simulateMessage(deviceMessage);
      
      // Verify the message was processed (logs should be updated)
      expect(wsInstance.logs.value.length).toBeGreaterThan(0);
      expect(wsInstance.logs.value[wsInstance.logs.value.length - 1].message).toContain('发现新设备');
    });
  });

  describe('Heartbeat mechanism', () => {
    it('should send heartbeat messages', () => {
      // Connect WebSocket
      wsInstance.connectWebSocket();
      mockWebSocket.connect();
      
      // Manually send heartbeat
      wsInstance.sendHeartbeat();
      
      // Verify heartbeat message was sent
      expect(mockWebSocket.messages.length).toBeGreaterThan(0);
      const heartbeatMessage = JSON.parse(mockWebSocket.messages[0]);
      expect(heartbeatMessage.type).toBe('heartbeat');
    });
  });

  describe('Reconnection logic', () => {
    it('should attempt reconnection on unexpected close', () => {
      // Mock setTimeout
      const setTimeoutSpy = vi.spyOn(global, 'setTimeout').mockImplementation((fn, delay) => {
        // Execute immediately for testing
        fn();
        return 1;
      });
      
      // Connect WebSocket
      wsInstance.connectWebSocket();
      mockWebSocket.connect();
      
      // Simulate unexpected close (code !== 1000)
      mockWebSocket.close(1006, 'Connection lost');
      
      // Verify reconnection was attempted
      expect(setTimeoutSpy).toHaveBeenCalled();
      
      setTimeoutSpy.mockRestore();
    });

    it('should not attempt reconnection on normal close', () => {
      // Mock setTimeout
      const setTimeoutSpy = vi.spyOn(global, 'setTimeout');
      
      // Connect WebSocket
      wsInstance.connectWebSocket();
      mockWebSocket.connect();
      
      // Simulate normal close (code === 1000)
      mockWebSocket.close(1000, 'Normal close');
      
      // Verify reconnection was not attempted
      expect(setTimeoutSpy).not.toHaveBeenCalled();
      
      setTimeoutSpy.mockRestore();
    });
  });

  describe('Connection status anomaly detection', () => {
    it('should detect connected: false as anomaly when all metrics are zero', () => {
      // Mock console.warn
      const consoleWarnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {});
      
      // Create test performance data with connected: false and all metrics zero
      const testData = {
        type: 'performance_data',
        device_ip: '192.168.140.129',
        timestamp: Date.now(),
        connected: false,
        cpu_usage: 0,
        memory_usage: 0,
        npu_usage: 0,
        disk_usage: 0,
        load_average_1m: 0,
        load_average_5m: 0,
        load_average_15m: 0,
        memory_available: 0,
        memory_total: 0,
        npu_memory_usage: 0,
        npu_power: 0,
        npu_temperature: 0,
        system_temperature: 0
      };
      
      // Call checkPerformanceDataAbnormal directly
      const isAbnormal = wsInstance.checkPerformanceDataAbnormal(testData);
      
      // Verify anomaly was detected
      expect(isAbnormal).toBe(true);
      
      // Verify warning was logged
      expect(consoleWarnSpy).toHaveBeenCalled();
      
      consoleWarnSpy.mockRestore();
    });

    it('should not detect connected: false as anomaly when metrics are not zero', () => {
      // Create test performance data with connected: false but some metrics non-zero
      const testData = {
        type: 'performance_data',
        device_ip: '192.168.140.129',
        timestamp: Date.now(),
        connected: false,
        cpu_usage: 50,
        memory_usage: 60,
        npu_usage: 0,
        disk_usage: 0,
        load_average_1m: 1,
        load_average_5m: 0,
        load_average_15m: 0,
        memory_available: 1024,
        memory_total: 4096,
        npu_memory_usage: 0,
        npu_power: 0,
        npu_temperature: 40,
        system_temperature: 35
      };
      
      // Call checkPerformanceDataAbnormal directly
      const isAbnormal = wsInstance.checkPerformanceDataAbnormal(testData);
      
      // Verify anomaly was not detected
      expect(isAbnormal).toBe(false);
    });

    it('should handle performance data with connected: true normally', () => {
      // Create test performance data with connected: true
      const testData = {
        type: 'performance_data',
        device_ip: '192.168.140.129',
        timestamp: Date.now(),
        connected: true,
        cpu_usage: 50,
        memory_usage: 60,
        npu_usage: 30,
        disk_usage: 40,
        load_average_1m: 1,
        load_average_5m: 0.8,
        load_average_15m: 0.5,
        memory_available: 1024,
        memory_total: 4096,
        npu_memory_usage: 20,
        npu_power: 15,
        npu_temperature: 40,
        system_temperature: 35
      };
      
      // Call checkPerformanceDataAbnormal directly
      const isAbnormal = wsInstance.checkPerformanceDataAbnormal(testData);
      
      // Verify no anomaly was detected
      expect(isAbnormal).toBe(false);
    });
  });
});
