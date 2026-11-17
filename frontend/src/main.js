import { createApp } from 'vue'
import { createPinia } from 'pinia'
import ElementPlus from 'element-plus'
import 'element-plus/dist/index.css'
import App from './App.vue'
import './variables.css'
import './style.css'

// 为了更好的浏览器兼容性，引入polyfill
import 'core-js/stable'
import 'regenerator-runtime/runtime'

const app = createApp(App)
const pinia = createPinia()

app.use(ElementPlus)
app.use(pinia)

app.mount('#app')