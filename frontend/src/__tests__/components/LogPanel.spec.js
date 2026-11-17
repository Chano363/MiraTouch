import { describe, it, expect } from 'vitest';
import { mount } from '@vue/test-utils';
import LogPanel from '../../components/LogPanel.vue';

// Mock Element Plus components
vi.mock('element-plus', () => ({
  ElCard: { template: '<div><slot /></div>' },
  ElButton: { template: '<button><slot /></button>' },
  ElEmpty: { template: '<div class="el-empty">Empty</div>' },
  ElSelect: { template: '<div><slot /></div>' },
  ElOption: { template: '<div><slot /></div>' },
  ElInput: { template: '<input><slot /></input>' },
  ElIcon: { template: '<div><slot /></div>' },
  ElMessage: {
    error: vi.fn(),
    success: vi.fn(),
    info: vi.fn()
  }
}));

vi.mock('@element-plus/icons-vue', () => ({
  Close: { template: '<div>Close</div>' }
}));

describe('LogPanel.vue', () => {
  // Test getLogClass function
  describe('getLogClass function', () => {
    it('should return log-info when log is undefined', () => {
      // Mount the component to access its methods
      const wrapper = mount(LogPanel, {
        props: {
          logs: []
        }
      });
      
      // Extract the getLogClass function from the component
      const { getLogClass } = wrapper.vm;
      
      expect(getLogClass(undefined)).toBe('log-info');
    });

    it('should return log-info when log has no level property', () => {
      const wrapper = mount(LogPanel, {
        props: {
          logs: []
        }
      });
      
      const { getLogClass } = wrapper.vm;
      
      expect(getLogClass({ message: 'Test message' })).toBe('log-info');
    });

    it('should return log-info when log level is empty', () => {
      const wrapper = mount(LogPanel, {
        props: {
          logs: []
        }
      });
      
      const { getLogClass } = wrapper.vm;
      
      expect(getLogClass({ level: '', message: 'Test message' })).toBe('log-info');
    });

    it('should return correct class for DEBUG level', () => {
      const wrapper = mount(LogPanel, {
        props: {
          logs: []
        }
      });
      
      const { getLogClass } = wrapper.vm;
      
      expect(getLogClass({ level: 'DEBUG', message: 'Test message' })).toBe('log-debug');
    });

    it('should return correct class for INFO level', () => {
      const wrapper = mount(LogPanel, {
        props: {
          logs: []
        }
      });
      
      const { getLogClass } = wrapper.vm;
      
      expect(getLogClass({ level: 'INFO', message: 'Test message' })).toBe('log-info');
    });

    it('should return correct class for WARN level', () => {
      const wrapper = mount(LogPanel, {
        props: {
          logs: []
        }
      });
      
      const { getLogClass } = wrapper.vm;
      
      expect(getLogClass({ level: 'WARN', message: 'Test message' })).toBe('log-warn');
    });

    it('should return correct class for ERROR level', () => {
      const wrapper = mount(LogPanel, {
        props: {
          logs: []
        }
      });
      
      const { getLogClass } = wrapper.vm;
      
      expect(getLogClass({ level: 'ERROR', message: 'Test message' })).toBe('log-error');
    });
  });

  // Test component rendering
  describe('Component rendering', () => {
    it('should render basic structure', () => {
      const wrapper = mount(LogPanel, {
        props: {
          logs: []
        }
      });
      
      expect(wrapper.exists()).toBe(true);
      expect(wrapper.find('.log-panel-card').exists()).toBe(true);
    });

    it('should display empty state when no logs', () => {
      const wrapper = mount(LogPanel, {
        props: {
          logs: []
        }
      });
      
      expect(wrapper.find('.el-empty').exists()).toBe(true);
    });

    it('should display logs when provided', () => {
      const testLogs = [
        {
          timestamp: '2026-01-30 23:00:00',
          level: 'INFO',
          message: 'Test log message',
          module: 'test'
        }
      ];
      
      const wrapper = mount(LogPanel, {
        props: {
          logs: testLogs
        }
      });
      
      expect(wrapper.find('.log-entry').exists()).toBe(true);
      expect(wrapper.text()).toContain('Test log message');
    });
  });
});
