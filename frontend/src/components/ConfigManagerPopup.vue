<template>
  <el-dialog
    v-model="dialogVisible"
    title="配置管理"
    width="800px"
    :before-close="handleClose"
  >
    <div class="config-content">
      <el-form :model="configForm" label-width="120px">
        <el-form-item label="目标设备">
          <el-tag type="info">{{ deviceIp }}</el-tag>
        </el-form-item>
        
        <el-form-item label="用户名">
          <el-input 
            v-model="configForm.username" 
            placeholder="设备上的用户名"
          />
        </el-form-item>
        
        <!-- 相机设置 -->
        <el-form-item>
          <template #label>
            <div class="section-title">相机设置</div>
          </template>
        </el-form-item>
        
        <el-form-item label="帧宽度">
          <el-input-number 
            v-model="configForm.camera.frame_width" 
            :min="640" 
            :max="4096" 
            style="width: 100%"
          />
          <div class="param-info">范围：640-4096，默认值：1280</div>
        </el-form-item>
        
        <el-form-item label="帧高度">
          <el-input-number 
            v-model="configForm.camera.frame_height" 
            :min="480" 
            :max="2160" 
            style="width: 100%"
          />
          <div class="param-info">范围：480-2160，默认值：720</div>
        </el-form-item>
        
        <!-- 跟踪设置 -->
        <el-form-item>
          <template #label>
            <div class="section-title">跟踪设置</div>
          </template>
        </el-form-item>
        
        <el-form-item label="最大年龄">
          <el-input-number 
            v-model="configForm.tracking.max_age" 
            :min="1" 
            :max="100" 
            style="width: 100%"
          />
          <div class="param-info">范围：1-100，默认值：30</div>
        </el-form-item>
        
        <el-form-item label="最小命中">
          <el-input-number 
            v-model="configForm.tracking.min_hits" 
            :min="1" 
            :max="10" 
            style="width: 100%"
          />
          <div class="param-info">范围：1-10，默认值：3</div>
        </el-form-item>
        
        <el-form-item label="IOU阈值">
          <el-slider 
            v-model="configForm.tracking.iou_threshold" 
            :min="0" 
            :max="1" 
            :step="0.01" 
            :format-tooltip="formatTooltip"
          />
          <div class="slider-value">{{ configForm.tracking.iou_threshold.toFixed(2) }}</div>
          <div class="param-info">范围：0-1，默认值：0.3</div>
        </el-form-item>
        
        <el-form-item label="时间步长">
          <el-input-number 
            v-model="configForm.tracking.delta_t" 
            :min="1" 
            :max="10" 
            style="width: 100%"
          />
          <div class="param-info">范围：1-10，默认值：3</div>
        </el-form-item>
        
        <el-form-item label="惯性系数">
          <el-slider 
            v-model="configForm.tracking.inertia" 
            :min="0" 
            :max="1" 
            :step="0.01" 
            :format-tooltip="formatTooltip"
          />
          <div class="slider-value">{{ configForm.tracking.inertia.toFixed(2) }}</div>
          <div class="param-info">范围：0-1，默认值：0.2</div>
        </el-form-item>
        
        <el-form-item label="关联函数">
          <el-select v-model="configForm.tracking.asso_func" placeholder="选择关联函数" style="width: 100%">
            <el-option label="IOU" value="iou" />
            <el-option label="GIOU" value="giou" />
            <el-option label="DIOU" value="diou" />
            <el-option label="CIOU" value="ciou" />
          </el-select>
          <div class="param-info">可选值：iou、giou、diou、ciou，默认值：giou</div>
        </el-form-item>
        
        <!-- 手势检测设置 -->
        <el-form-item>
          <template #label>
            <div class="section-title">手势检测设置</div>
          </template>
        </el-form-item>
        
        <el-form-item label="最大长度">
          <el-input-number 
            v-model="configForm.gesture_detection.maxlen" 
            :min="10" 
            :max="100" 
            style="width: 100%"
          />
          <div class="param-info">范围：10-100，默认值：30</div>
        </el-form-item>
        
        <el-form-item label="最小帧数">
          <el-input-number 
            v-model="configForm.gesture_detection.min_frames" 
            :min="5" 
            :max="50" 
            style="width: 100%"
          />
          <div class="param-info">范围：5-50，默认值：20</div>
        </el-form-item>
        
        <el-form-item label="最小距离">
          <el-slider 
            v-model="configForm.gesture_detection.min_absolute_distance" 
            :min="0" 
            :max="5" 
            :step="0.1" 
            :format-tooltip="formatTooltip"
          />
          <div class="slider-value">{{ configForm.gesture_detection.min_absolute_distance.toFixed(1) }}</div>
          <div class="param-info">范围：0-5，默认值：1.5</div>
        </el-form-item>
        
        <el-form-item label="动作队列长度">
          <el-input-number 
            v-model="configForm.gesture_detection.action_deque_maxlen" 
            :min="1" 
            :max="20" 
            style="width: 100%"
          />
          <div class="param-info">范围：1-20，默认值：5</div>
        </el-form-item>
        
        <!-- 高级设置 -->
        <el-form-item>
          <template #label>
            <div class="section-title">高级设置</div>
          </template>
        </el-form-item>
        
        <el-form-item label="快速滑动最小帧数">
          <el-input-number 
            v-model="configForm.advanced.fast_swipe_min_frames" 
            :min="5" 
            :max="50" 
            style="width: 100%"
          />
          <div class="param-info">范围：5-50，默认值：20</div>
        </el-form-item>
        
        <el-form-item label="三指滑动最小帧数">
          <el-input-number 
            v-model="configForm.advanced.swipe3_min_frames" 
            :min="5" 
            :max="50" 
            style="width: 100%"
          />
          <div class="param-info">范围：5-50，默认值：15</div>
        </el-form-item>
        
        <el-form-item label="点击最小帧数">
          <el-input-number 
            v-model="configForm.advanced.tap_min_frames" 
            :min="1" 
            :max="20" 
            style="width: 100%"
          />
          <div class="param-info">范围：1-20，默认值：8</div>
        </el-form-item>
        
        <el-form-item label="双击最大帧数">
          <el-input-number 
            v-model="configForm.advanced.double_tap_max_frames" 
            :min="1" 
            :max="20" 
            style="width: 100%"
          />
          <div class="param-info">范围：1-20，默认值：8</div>
        </el-form-item>
        
        <el-form-item label="握拳最大帧数">
          <el-input-number 
            v-model="configForm.advanced.fist_max_frames" 
            :min="1" 
            :max="20" 
            style="width: 100%"
          />
          <div class="param-info">范围：1-20，默认值：10</div>
        </el-form-item>
        
        <!-- 系统设置 -->
        <el-form-item>
          <template #label>
            <div class="section-title">系统设置</div>
          </template>
        </el-form-item>
        
        <el-form-item label="调试模式">
          <el-switch v-model="configForm.system.debug_mode" />
          <div class="param-info">默认值：false</div>
        </el-form-item>
      </el-form>
      
      <div class="config-actions">
        <el-button 
          type="primary" 
          @click="readConfig" 
          :loading="loading"
        >
          读取配置
        </el-button>
        <el-button 
          type="success" 
          @click="sendConfig" 
          :loading="loading"
        >
          发送配置
        </el-button>
        <el-button @click="resetConfig">
          重置默认值
        </el-button>
      </div>
      
      <div class="config-status" v-if="statusMessage">
        <el-alert
          :title="statusMessage"
          :type="statusType"
          show-icon
          :closable="false"
        />
      </div>
    </div>
    
    <template #footer>
      <div class="dialog-footer">
        <el-button @click="handleClose">取消</el-button>
        <el-button type="primary" @click="handleSave">保存</el-button>
      </div>
    </template>
  </el-dialog>
</template>

<script setup>
import { ref, watch } from 'vue'
import { ElDialog, ElForm, ElFormItem, ElInput, ElSelect, ElOption, ElInputNumber, ElSlider, ElButton, ElTag, ElAlert } from 'element-plus'

const props = defineProps({
  show: {
    type: Boolean,
    required: true
  },
  deviceIp: {
    type: String,
    required: true
  },
  isConnected: {
    type: Boolean,
    required: true
  }
})

const emit = defineEmits([
  'close',
  'save'
])

const dialogVisible = ref(false)
const loading = ref(false)
const statusMessage = ref('')
const statusType = ref('info')

const configForm = ref({
  username: 'chano',
  camera: {
    frame_width: 1280,
    frame_height: 720
  },
  tracking: {
    max_age: 30,
    min_hits: 3,
    iou_threshold: 0.3,
    delta_t: 3,
    inertia: 0.2,
    asso_func: 'giou'
  },
  gesture_detection: {
    maxlen: 30,
    min_frames: 20,
    min_absolute_distance: 1.5,
    action_deque_maxlen: 5
  },
  advanced: {
    fast_swipe_min_frames: 20,
    swipe3_min_frames: 15,
    tap_min_frames: 8,
    double_tap_max_frames: 8,
    fist_max_frames: 10
  },
  system: {
    debug_mode: false
  }
})

// 默认配置值
const defaultConfig = {
  camera: {
    frame_width: 1280,
    frame_height: 720
  },
  tracking: {
    max_age: 30,
    min_hits: 3,
    iou_threshold: 0.3,
    delta_t: 3,
    inertia: 0.2,
    asso_func: 'giou'
  },
  gesture_detection: {
    maxlen: 30,
    min_frames: 20,
    min_absolute_distance: 1.5,
    action_deque_maxlen: 5
  },
  advanced: {
    fast_swipe_min_frames: 20,
    swipe3_min_frames: 15,
    tap_min_frames: 8,
    double_tap_max_frames: 8,
    fist_max_frames: 10
  },
  system: {
    debug_mode: false
  }
}

// 监听props变化并更新本地数据
watch(() => props.show, (newVal) => {
  dialogVisible.value = newVal
  if (newVal && props.isConnected) {
    // 自动读取配置
    setTimeout(() => {
      readConfig()
    }, 500)
  }
})

const handleClose = () => {
  dialogVisible.value = false
  emit('close')
}

const handleSave = () => {
  sendConfig()
  handleClose()
}

const formatTooltip = (val) => {
  return `${val.toFixed(2)}`
}

const resetConfig = () => {
  configForm.value = { ...defaultConfig }
  showStatus('已重置为默认值', 'success')
}

const showStatus = (message, type = 'info') => {
  statusMessage.value = message
  statusType.value = type
  setTimeout(() => {
    statusMessage.value = ''
  }, 3000)
}

const readConfig = async () => {
  if (!props.isConnected) {
    showStatus('设备未连接，无法读取配置', 'warning')
    return
  }
  
  loading.value = true
  try {
    const response = await fetch('/api/config/receive', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({
        ip_address: props.deviceIp,
        username: configForm.value.username
      })
    })
    
    const result = await response.json()
    if (result.success && result.has_config) {
      // 更新配置表单
      configForm.value = {
        ...configForm.value,
        camera: {
          frame_width: result.config.camera?.frame_width || defaultConfig.camera.frame_width,
          frame_height: result.config.camera?.frame_height || defaultConfig.camera.frame_height
        },
        tracking: {
          max_age: result.config.tracking?.max_age || defaultConfig.tracking.max_age,
          min_hits: result.config.tracking?.min_hits || defaultConfig.tracking.min_hits,
          iou_threshold: result.config.tracking?.iou_threshold || defaultConfig.tracking.iou_threshold,
          delta_t: result.config.tracking?.delta_t || defaultConfig.tracking.delta_t,
          inertia: result.config.tracking?.inertia || defaultConfig.tracking.inertia,
          asso_func: result.config.tracking?.asso_func || defaultConfig.tracking.asso_func
        },
        gesture_detection: {
          maxlen: result.config.gesture_detection?.maxlen || defaultConfig.gesture_detection.maxlen,
          min_frames: result.config.gesture_detection?.min_frames || defaultConfig.gesture_detection.min_frames,
          min_absolute_distance: result.config.gesture_detection?.min_absolute_distance || defaultConfig.gesture_detection.min_absolute_distance,
          action_deque_maxlen: result.config.gesture_detection?.action_deque_maxlen || defaultConfig.gesture_detection.action_deque_maxlen
        },
        advanced: {
          fast_swipe_min_frames: result.config.advanced?.fast_swipe_min_frames || defaultConfig.advanced.fast_swipe_min_frames,
          swipe3_min_frames: result.config.advanced?.swipe3_min_frames || defaultConfig.advanced.swipe3_min_frames,
          tap_min_frames: result.config.advanced?.tap_min_frames || defaultConfig.advanced.tap_min_frames,
          double_tap_max_frames: result.config.advanced?.double_tap_max_frames || defaultConfig.advanced.double_tap_max_frames,
          fist_max_frames: result.config.advanced?.fist_max_frames || defaultConfig.advanced.fist_max_frames
        },
        system: {
          debug_mode: result.config.system?.debug_mode ?? defaultConfig.system.debug_mode
        }
      }
      showStatus('配置读取成功', 'success')
    } else {
      showStatus('配置文件不存在，使用默认值', 'warning')
    }
  } catch (error) {
    console.error('读取配置失败:', error)
    showStatus('读取配置失败: ' + error.message, 'error')
  } finally {
    loading.value = false
  }
}

const sendConfig = async () => {
  if (!props.isConnected) {
    showStatus('设备未连接，无法发送配置', 'warning')
    return
  }
  
  loading.value = true
  try {
    const response = await fetch('/api/config/send', {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json'
      },
      body: JSON.stringify({
        ip_address: props.deviceIp,
        username: configForm.value.username,
        config: {
          camera: {
            frame_width: configForm.value.camera.frame_width,
            frame_height: configForm.value.camera.frame_height
          },
          tracking: {
            max_age: configForm.value.tracking.max_age,
            min_hits: configForm.value.tracking.min_hits,
            iou_threshold: configForm.value.tracking.iou_threshold,
            delta_t: configForm.value.tracking.delta_t,
            inertia: configForm.value.tracking.inertia,
            asso_func: configForm.value.tracking.asso_func
          },
          gesture_detection: {
            maxlen: configForm.value.gesture_detection.maxlen,
            min_frames: configForm.value.gesture_detection.min_frames,
            min_absolute_distance: configForm.value.gesture_detection.min_absolute_distance,
            action_deque_maxlen: configForm.value.gesture_detection.action_deque_maxlen
          },
          advanced: {
            fast_swipe_min_frames: configForm.value.advanced.fast_swipe_min_frames,
            swipe3_min_frames: configForm.value.advanced.swipe3_min_frames,
            tap_min_frames: configForm.value.advanced.tap_min_frames,
            double_tap_max_frames: configForm.value.advanced.double_tap_max_frames,
            fist_max_frames: configForm.value.advanced.fist_max_frames
          },
          system: {
            debug_mode: configForm.value.system.debug_mode
          }
        }
      })
    })
    
    const result = await response.json()
    if (result.success) {
      showStatus('配置发送成功', 'success')
      emit('save', configForm.value)
    } else {
      showStatus('配置发送失败: ' + (result.error || result.message), 'error')
    }
  } catch (error) {
    console.error('发送配置失败:', error)
    showStatus('发送配置失败: ' + error.message, 'error')
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
.config-content {
  padding: 20px 0;
}

.slider-value {
  text-align: center;
  margin-top: 8px;
  font-size: 14px;
  color: #606266;
}

.dialog-footer {
  display: flex;
  justify-content: flex-end;
  gap: 12px;
}

.param-info {
  font-size: 0.8rem;
  color: #909399;
  margin-top: 4px;
}

.config-actions {
  display: flex;
  gap: 10px;
  margin: 20px 0;
  justify-content: center;
}

.config-status {
  margin-top: 20px;
}

.section-title {
  font-size: 16px;
  font-weight: 600;
  color: #303133;
  margin-bottom: 10px;
}
</style>