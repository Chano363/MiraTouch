import { describe, it, expect, vi, beforeEach } from 'vitest';
import { mount } from '@vue/test-utils';
import MultiDeviceManager from '../../components/MultiDeviceManager.vue';

// Mock Element Plus components
vi.mock('element-plus', () => ({
  ElCard: { template: '<div><slot /></div>' },
  ElButton: { template: '<button><slot /></button>' },
  ElSelect: { template: '<div><slot /></div>' },
  ElOption: { template: '<div><slot /></div>' },
  ElTable: { template: '<div><slot /></div>' },
  ElTableColumn: { template: '<div><slot /></div>' },
  ElTag: { template: '<div><slot /></div>' },
  ElDialog: { template: '<div><slot /></div>' },
  ElForm: { template: '<div><slot /></div>' },
  ElFormItem: { template: '<div><slot /></div>' },
  ElInput: { template: '<input><slot /></input>' },
  ElInputNumber: { template: '<input type="number" />' },
  ElAlert: { template: '<div><slot /></div>' },
  ElMessage: {
    error: vi.fn(),
    success: vi.fn(),
    info: vi.fn()
  },
  ElEmpty: { template: '<div>Empty</div>' }
}));

vi.mock('@element-plus/icons-vue', () => ({
  Search: { template: '<div>Search</div>' },
  Monitor: { template: '<div>Monitor</div>' },
  Connection: { template: '<div>Connection</div>' },
  RefreshRight: { template: '<div>RefreshRight</div>' },
  VideoPause: { template: '<div>VideoPause</div>' }
}));

// Mock other components
vi.mock('../../components/DeviceList.vue', () => ({
  default: { template: '<div>DeviceList</div>' }
}));

vi.mock('../../components/HistoryDevicesPanel.vue', () => ({
  default: { template: '<div>HistoryDevicesPanel</div>' }
}));

vi.mock('../../components/PerformanceChartModal.vue', () => ({
  default: { template: '<div>PerformanceChartModal</div>' }
}));

vi.mock('../../components/PerformanceChartPopup.vue', () => ({
  default: { template: '<div>PerformanceChartPopup</div>' }
}));

vi.mock('../../components/PerformanceMonitorManager.vue', () => ({
  default: { template: '<div>PerformanceMonitorManager</div>' }
}));

// Mock stores and composables
vi.mock('../../stores/deviceStore', () => ({
  useDeviceStore: vi.fn(() => ({
    selectedDevices: [],
    monitoringDevices: [],
    historyDevices: [],
    connectedDevices: [],
    discoveredDevices: [],
    setSelectedDevices: vi.fn(),
    toggleDeviceSelection: vi.fn(),
    toggleDeviceMonitoring: vi.fn(),
    addHistoryDevice: vi.fn(),
    setDiscoveredDevices: vi.fn(),
    setConnectedDevices: vi.fn(),
    updateDeviceConnectionStatus: vi.fn(),
    removeMonitoringDevice: vi.fn(),
    clearHistory: vi.fn(),
    loadFromLocalStorage: vi.fn()
  }))
}));

// Create mock functions for WebSocket
const mockRegisterMessageHandler = vi.fn();
const mockUnregisterMessageHandler = vi.fn();
const mockConnectWebSocket = vi.fn();

vi.mock('../../composables/useWebSocket', () => ({
  useWebSocket: vi.fn(() => ({
    wsConnected: { value: true },
    registerMessageHandler: mockRegisterMessageHandler,
    unregisterMessageHandler: mockUnregisterMessageHandler,
    connectWebSocket: mockConnectWebSocket
  }))
}));

vi.mock('../../utils/logger', () => ({
  createLogger: vi.fn(() => ({
    debug: vi.fn(),
    info: vi.fn(),
    warn: vi.fn(),
    error: vi.fn()
  }))
}));

describe('MultiDeviceManager.vue', () => {
  // Test deviceDiscoveryHandler function
  describe('deviceDiscoveryHandler function', () => {
    it('should handle single device message', () => {
      const wrapper = mount(MultiDeviceManager);
      
      const { deviceDiscoveryHandler } = wrapper.vm;
      
      // Mock device data
      const deviceData = {
        type: 'device_found',
        device: {
          ip_address: '192.168.1.100',
          hostname: 'test-device',
          mac_address: '00:11:22:33:44:55',
          is_online: true,
          is_connected: false
        }
      };
      
      deviceDiscoveryHandler(deviceData);
      
      // Verify the device was added to the list
      expect(wrapper.vm.discoveredDevices.length).toBe(1);
      expect(wrapper.vm.discoveredDevices[0].ip_address).toBe('192.168.1.100');
      expect(wrapper.vm.discoveredDevices[0].hostname).toBe('test-device');
    });

    it('should handle batch devices message (array)', () => {
      const wrapper = mount(MultiDeviceManager);
      
      const { deviceDiscoveryHandler } = wrapper.vm;
      
      // Mock batch device data
      const batchData = {
        devices: [
          {
            ip_address: '192.168.1.100',
            hostname: 'test-device-1',
            mac_address: '00:11:22:33:44:55'
          },
          {
            ip_address: '192.168.1.101',
            hostname: 'test-device-2',
            mac_address: 'AA:BB:CC:DD:EE:FF'
          }
        ]
      };
      
      deviceDiscoveryHandler(batchData);
      
      // Verify both devices were added to the list
      expect(wrapper.vm.discoveredDevices.length).toBe(2);
      expect(wrapper.vm.discoveredDevices[0].ip_address).toBe('192.168.1.100');
      expect(wrapper.vm.discoveredDevices[1].ip_address).toBe('192.168.1.101');
    });

    it('should handle incomplete device data', () => {
      const wrapper = mount(MultiDeviceManager);
      
      const { deviceDiscoveryHandler } = wrapper.vm;
      
      // Mock incomplete device data
      const incompleteData = {
        devices: [
          {
            ip_address: '192.168.1.100',
            // Missing hostname and mac_address
          }
        ]
      };
      
      deviceDiscoveryHandler(incompleteData);
      
      // Verify the device was added with default values
      expect(wrapper.vm.discoveredDevices.length).toBe(1);
      expect(wrapper.vm.discoveredDevices[0].ip_address).toBe('192.168.1.100');
      expect(wrapper.vm.discoveredDevices[0].hostname).toBe('192.168.1.100'); // Should default to ip_address
      expect(wrapper.vm.discoveredDevices[0].mac_address).toBe(''); // Should default to empty string
    });

    it('should handle invalid message format', () => {
      const wrapper = mount(MultiDeviceManager);
      
      const { deviceDiscoveryHandler } = wrapper.vm;
      
      // Mock invalid message
      const invalidData = {
        type: 'invalid_type',
        some_other_property: 'value'
      };
      
      // Should not throw an error
      expect(() => deviceDiscoveryHandler(invalidData)).not.toThrow();
      
      // Should not add any devices
      expect(wrapper.vm.discoveredDevices.length).toBe(0);
    });

    it('should update existing device instead of adding duplicate', () => {
      const wrapper = mount(MultiDeviceManager);
      
      const { deviceDiscoveryHandler } = wrapper.vm;
      
      // First, add a device
      const initialData = {
        device: {
          ip_address: '192.168.1.100',
          hostname: 'old-hostname',
          mac_address: '00:11:22:33:44:55'
        }
      };
      
      deviceDiscoveryHandler(initialData);
      expect(wrapper.vm.discoveredDevices.length).toBe(1);
      expect(wrapper.vm.discoveredDevices[0].hostname).toBe('old-hostname');
      
      // Then, update the same device with new data
      const updateData = {
        device: {
          ip_address: '192.168.1.100',
          hostname: 'new-hostname',
          mac_address: '00:11:22:33:44:55'
        }
      };
      
      deviceDiscoveryHandler(updateData);
      
      // Should still have only one device, but with updated hostname
      expect(wrapper.vm.discoveredDevices.length).toBe(1);
      expect(wrapper.vm.discoveredDevices[0].hostname).toBe('new-hostname');
    });
  });

  // Test WebSocket connection
  describe('WebSocket connection', () => {
    it('should establish WebSocket connection on mount', () => {
      mount(MultiDeviceManager);
      
      expect(mockConnectWebSocket).toHaveBeenCalled();
    });

    it('should register message handlers', () => {
      mount(MultiDeviceManager);
      
      expect(mockRegisterMessageHandler).toHaveBeenCalledWith('scan_progress', expect.any(Function));
      expect(mockRegisterMessageHandler).toHaveBeenCalledWith('device_list_update', expect.any(Function));
      expect(mockRegisterMessageHandler).toHaveBeenCalledWith('device_discovery', expect.any(Function));
    });
  });
});
