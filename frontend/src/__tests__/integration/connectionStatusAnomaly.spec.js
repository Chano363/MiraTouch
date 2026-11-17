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
global.WebSocket = MockWebSocket;

// Mock localStorage
const localStorageMock = {
  getItem: vi.fn(),
  setItem: vi.fn(),
  removeItem: vi.fn(),
  clear: vi.fn(),
  length: 0,
  key: vi.fn()
};

Object.defineProperty(global, 'localStorage', {
  value: localStorageMock
});

describe('Connection Status Anomaly Integration Test', () => {
  let wsInstance;
  let mockWebSocket;

  beforeEach(() => {
    // Create a mock WebSocket instance
    mockWebSocket = new MockWebSocket('ws://localhost:8083');
    
    // Mock the WebSocket constructor to return our mock instance
    vi.spyOn(global, 'WebSocket').mockImplementation(() => mockWebSocket);
    
    // Create the useWebSocket instance
    wsInstance = useWebSocket({
      logConfig: {
        enableDebugLogs: true,
        enablePerformanceLogs: true
      }
    });

    // Clear localStorage mock
    Object.keys(localStorageMock).forEach(key => {
      if (typeof localStorageMock[key] === 'function') {
        localStorageMock[key].mockClear();
      }
    });
  });

  afterEach(() => {
    vi.restoreAllMocks();
  });

  it('should handle connected: false anomaly in end-to-end flow', () => {
    // Mock console.warn and console.log
    const consoleWarnSpy = vi.spyOn(console, 'warn').mockImplementation(() => {});
    const consoleLogSpy = vi.spyOn(console, 'log').mockImplementation(() => {});
    
    // Connect WebSocket
    wsInstance.connectWebSocket();
    mockWebSocket.connect();
    
    // Verify connection is established
    expect(wsInstance.wsConnected.value).toBe(true);
    
    // Simulate performance data with connected: false and all metrics zero
    const anomalyData = {
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
    
    // Send the anomaly data through WebSocket
    mockWebSocket.simulateMessage(anomalyData);
    
    // Wait for processing
    return new Promise(resolve => {
      setTimeout(() => {
        // Verify anomaly was detected and logged
        expect(consoleWarnSpy).toHaveBeenCalled();
        
        // Verify data was saved to localStorage
        expect(localStorageMock.setItem).toHaveBeenCalled();
        
        // Verify system health check shows no critical issues
        const healthStatus = wsInstance.checkSystemHealth();
        expect(healthStatus.isHealthy).toBe(true);
        
        consoleWarnSpy.mockRestore();
        consoleLogSpy.mockRestore();
        resolve();
      }, 100);
    });
  });

  it('should handle device reconnection after connected: false', () => {
    // Connect WebSocket
    wsInstance.connectWebSocket();
    mockWebSocket.connect();
    
    // Simulate performance data with connected: false
    const offlineData = {
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
    
    mockWebSocket.simulateMessage(offlineData);
    
    // Wait for offline processing
    return new Promise(resolve => {
      setTimeout(() => {
        // Now simulate device reconnection with normal data
        const onlineData = {
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
        
        mockWebSocket.simulateMessage(onlineData);
        
        setTimeout(() => {
          // Verify system returns to normal state
          const healthStatus = wsInstance.checkSystemHealth();
          expect(healthStatus.isHealthy).toBe(true);
          resolve();
        }, 100);
      }, 100);
    });
  });

  it('should maintain system stability during multiple connection status changes', () => {
    // Connect WebSocket
    wsInstance.connectWebSocket();
    mockWebSocket.connect();
    
    // Simulate multiple connection status changes
    const statusChanges = [
      // Online -> Offline
      {
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
      },
      // Offline
      {
        type: 'performance_data',
        device_ip: '192.168.140.129',
        timestamp: Date.now() + 1000,
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
      },
      // Back online
      {
        type: 'performance_data',
        device_ip: '192.168.140.129',
        timestamp: Date.now() + 2000,
        connected: true,
        cpu_usage: 45,
        memory_usage: 55,
        npu_usage: 25,
        disk_usage: 35,
        load_average_1m: 0.9,
        load_average_5m: 0.7,
        load_average_15m: 0.4,
        memory_available: 1200,
        memory_total: 4096,
        npu_memory_usage: 18,
        npu_power: 14,
        npu_temperature: 38,
        system_temperature: 33
      }
    ];
    
    statusChanges.forEach(data => {
      mockWebSocket.simulateMessage(data);
    });
    
    return new Promise(resolve => {
      setTimeout(() => {
        // Verify system remains stable
        const healthStatus = wsInstance.checkSystemHealth();
        expect(healthStatus.isHealthy).toBe(true);
        expect(healthStatus.currentMode).toBe('normal');
        
        // Verify performance data was processed
        const historicalData = wsInstance.getDevicePerformanceHistory('192.168.140.129');
        expect(historicalData.length).toBeGreaterThan(0);
        
        resolve();
      }, 200);
    });
  });
});
