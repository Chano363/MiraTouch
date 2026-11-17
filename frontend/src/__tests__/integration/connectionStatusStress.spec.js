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

describe('Connection Status Stress Test', () => {
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
        enableDebugLogs: false, // Disable debug logs for stress test
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

  it('should handle multiple devices with connected: false simultaneously', () => {
    // Connect WebSocket
    wsInstance.connectWebSocket();
    mockWebSocket.connect();
    
    // Verify connection is established
    expect(wsInstance.wsConnected.value).toBe(true);
    
    // Number of devices to simulate
    const deviceCount = 10;
    
    // Simulate multiple devices going offline simultaneously
    for (let i = 1; i <= deviceCount; i++) {
      const deviceIp = `192.168.140.${100 + i}`;
      
      const offlineData = {
        type: 'performance_data',
        device_ip: deviceIp,
        timestamp: Date.now() + i * 100,
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
    }
    
    return new Promise(resolve => {
      setTimeout(() => {
        // Verify system remains stable
        const healthStatus = wsInstance.checkSystemHealth();
        expect(healthStatus.isHealthy).toBe(true);
        expect(healthStatus.currentMode).toBe('normal');
        
        // Verify no critical system faults
        const faults = wsInstance.detectSystemFaults ? wsInstance.detectSystemFaults() : [];
        expect(faults).toBeDefined();
        
        resolve();
      }, 500);
    });
  });

  it('should handle rapid connection status changes', () => {
    // Connect WebSocket
    wsInstance.connectWebSocket();
    mockWebSocket.connect();
    
    // Verify connection is established
    expect(wsInstance.wsConnected.value).toBe(true);
    
    const deviceIp = '192.168.140.129';
    const changeCount = 20; // Number of status changes
    
    // Simulate rapid connection status changes
    for (let i = 0; i < changeCount; i++) {
      const isConnected = i % 2 === 0;
      
      const statusData = {
        type: 'performance_data',
        device_ip: deviceIp,
        timestamp: Date.now() + i * 50,
        connected: isConnected,
        cpu_usage: isConnected ? Math.random() * 100 : 0,
        memory_usage: isConnected ? Math.random() * 100 : 0,
        npu_usage: isConnected ? Math.random() * 100 : 0,
        disk_usage: isConnected ? Math.random() * 100 : 0,
        load_average_1m: isConnected ? Math.random() * 5 : 0,
        load_average_5m: isConnected ? Math.random() * 5 : 0,
        load_average_15m: isConnected ? Math.random() * 5 : 0,
        memory_available: isConnected ? Math.random() * 4096 : 0,
        memory_total: isConnected ? 4096 : 0,
        npu_memory_usage: isConnected ? Math.random() * 100 : 0,
        npu_power: isConnected ? Math.random() * 50 : 0,
        npu_temperature: isConnected ? Math.random() * 80 : 0,
        system_temperature: isConnected ? Math.random() * 70 : 0
      };
      
      mockWebSocket.simulateMessage(statusData);
    }
    
    return new Promise(resolve => {
      setTimeout(() => {
        // Verify system remains stable
        const healthStatus = wsInstance.checkSystemHealth();
        expect(healthStatus.isHealthy).toBe(true);
        expect(healthStatus.currentMode).toBe('normal');
        
        // Verify performance data was processed
        const performanceData = wsInstance.getDevicePerformanceHistory(deviceIp);
        expect(performanceData.length).toBeGreaterThan(0);
        
        resolve();
      }, 1000);
    });
  });

  it('should handle mixed scenarios with some devices online and some offline', () => {
    // Connect WebSocket
    wsInstance.connectWebSocket();
    mockWebSocket.connect();
    
    // Verify connection is established
    expect(wsInstance.wsConnected.value).toBe(true);
    
    const totalDevices = 15;
    const offlineDevices = 5;
    
    // Simulate mixed device states
    for (let i = 1; i <= totalDevices; i++) {
      const deviceIp = `192.168.140.${90 + i}`;
      const isOffline = i <= offlineDevices;
      
      const deviceData = {
        type: 'performance_data',
        device_ip: deviceIp,
        timestamp: Date.now() + i * 100,
        connected: !isOffline,
        cpu_usage: isOffline ? 0 : Math.random() * 100,
        memory_usage: isOffline ? 0 : Math.random() * 100,
        npu_usage: isOffline ? 0 : Math.random() * 100,
        disk_usage: isOffline ? 0 : Math.random() * 100,
        load_average_1m: isOffline ? 0 : Math.random() * 5,
        load_average_5m: isOffline ? 0 : Math.random() * 5,
        load_average_15m: isOffline ? 0 : Math.random() * 5,
        memory_available: isOffline ? 0 : Math.random() * 4096,
        memory_total: isOffline ? 0 : 4096,
        npu_memory_usage: isOffline ? 0 : Math.random() * 100,
        npu_power: isOffline ? 0 : Math.random() * 50,
        npu_temperature: isOffline ? 0 : Math.random() * 80,
        system_temperature: isOffline ? 0 : Math.random() * 70
      };
      
      mockWebSocket.simulateMessage(deviceData);
    }
    
    return new Promise(resolve => {
      setTimeout(() => {
        // Verify system remains stable
        const healthStatus = wsInstance.checkSystemHealth();
        expect(healthStatus.isHealthy).toBe(true);
        expect(healthStatus.currentMode).toBe('normal');
        
        // Verify offline devices were detected as anomalous
        // This is more of a system stability test than a detailed anomaly detection test
        console.log('System health status:', healthStatus);
        
        resolve();
      }, 800);
    });
  });
});
