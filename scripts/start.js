// scripts/start.js
const shell = require('shelljs');
const path = require('path');
const fs = require('fs');
const { spawn, exec } = require('child_process');
const net = require('net');

// Project path configuration
const rootDir = path.join(__dirname, '..');
const buildDir = path.join(rootDir, 'build');
const frontendDir = path.join(rootDir, 'frontend');
const serverExe = path.join(buildDir, 'Release', 'remote_debugger_server.exe');

console.log('=== Remote Debugger Startup Script ===\n');

// Check for required tools
if (!shell.which('npm')) {
  console.error('Error: npm is not installed or not in PATH');
  process.exit(1);
}

// Verify critical build files exist
function verifyBuildPrerequisites() {
  const requiredFiles = [
    { name: 'conanfile.txt', path: path.join(rootDir, 'conanfile.txt') },
    { name: 'CMakeLists.txt', path: path.join(rootDir, 'CMakeLists.txt') },
    { name: 'remote_debugger_profile', path: path.join(rootDir, 'remote_debugger_profile') }
  ];
  
  let allExist = true;
  requiredFiles.forEach(file => {
    if (!fs.existsSync(file.path)) {
      console.error(`❌ CRITICAL: ${file.name} is missing!`);
      allExist = false;
    }
  });
  
  return allExist;
}

// 从服务器配置或源代码读取端口配置
function getServerPorts() {
  const backendPort = 8082; // 修改为你实际的端口
  const frontendPort = 3000;
  
  // 可以尝试从配置文件读取
  try {
    const configPath = path.join(rootDir, 'config.json');
    if (fs.existsSync(configPath)) {
      const config = JSON.parse(fs.readFileSync(configPath, 'utf8'));
      if (config.backendPort) backendPort = config.backendPort;
      if (config.frontendPort) frontendPort = config.frontendPort;
    }
  } catch (error) {
    console.log('Using default ports');
  }
  
  return { backendPort, frontendPort };
}

// 检测特定端口的服务是否响应
function checkPortService(port, timeout = 5000) {
  return new Promise((resolve) => {
    const socket = new net.Socket();
    
    socket.setTimeout(timeout);
    
    socket.on('connect', () => {
      socket.destroy();
      resolve(true);
    });
    
    socket.on('timeout', () => {
      socket.destroy();
      resolve(false);
    });
    
    socket.on('error', () => {
      resolve(false);
    });
    
    socket.connect(port, 'localhost');
  });
}

// 检查进程是否正在运行（通过端口和进程名）
function checkProcessRunning(processName, port) {
  return new Promise((resolve) => {
    // 方法1：检查端口服务
    checkPortService(port, 2000).then(portAvailable => {
      if (portAvailable) {
        // 方法2：检查进程列表中是否有相关进程
        exec('tasklist /FI "IMAGENAME eq remote_debugger_server.exe"', (error, stdout) => {
          if (stdout.includes('remote_debugger_server.exe')) {
            resolve({ running: true, method: 'process' });
          } else {
            // 方法3：通过 netstat 查看哪个进程在使用该端口
            exec(`netstat -ano | findstr :${port}`, (netstatError, netstatStdout) => {
              if (netstatStdout) {
                // 提取 PID
                const lines = netstatStdout.trim().split('\n');
                const pidLine = lines.find(line => line.includes('LISTENING'));
                if (pidLine) {
                  const pid = pidLine.trim().split(/\s+/).pop();
                  // 检查该 PID 对应的进程
                  exec(`tasklist /FI "PID eq ${pid}"`, (taskError, taskStdout) => {
                    if (taskStdout && !taskStdout.includes('No tasks')) {
                      resolve({ running: true, method: 'port', pid });
                    } else {
                      resolve({ running: false, method: 'port_no_process' });
                    }
                  });
                } else {
                  resolve({ running: false, method: 'no_listening' });
                }
              } else {
                resolve({ running: portAvailable, method: 'port_only' });
              }
            });
          }
        });
      } else {
        resolve({ running: false, method: 'no_connection' });
      }
    });
  });
}

// 等待服务真正启动
async function waitForService(port, serviceName, maxAttempts = 30, interval = 1000) {
  console.log(`Waiting for ${serviceName} (port ${port})...`);
  
  for (let attempt = 1; attempt <= maxAttempts; attempt++) {
    const result = await checkProcessRunning(serviceName, port);
    
    if (result.running) {
      console.log(`  ✓ ${serviceName} is running (found via ${result.method})`);
      return true;
    }
    
    if (attempt < maxAttempts) {
      process.stdout.write('.');
      await new Promise(resolve => setTimeout(resolve, interval));
    }
  }
  
  console.log(`\n  ✗ ${serviceName} failed to start`);
  return false;
}

// Step 1: Ensure backend is built
if (!fs.existsSync(serverExe)) {
  console.log('Warning: Backend server executable not found.');
  console.log('Please run "node scripts/build.js" to build the project first.');
  console.log('Attempting to verify if manual build is possible...');
  
  if (!verifyBuildPrerequisites()) {
    console.error('Critical files missing. Cannot proceed with automatic build.');
    process.exit(1);
  }
  
  console.log('All prerequisites found. Running build script...');
  const buildResult = shell.exec('node scripts/build.js');
  if (buildResult.code !== 0) {
    console.error('Build failed. Please check manually and ensure all dependencies are installed.');
    process.exit(1);
  }
  
  // Double-check that executable was created
  if (!fs.existsSync(serverExe)) {
    console.error('Build completed but executable still not found. Check build configuration.');
    process.exit(1);
  }
}

// Step 2: Ensure frontend dependencies are installed and built
console.log('Checking frontend setup...');
if (!fs.existsSync(path.join(frontendDir, 'node_modules'))) {
  console.log('Installing frontend dependencies...');
  shell.cd(frontendDir);
  const installResult = shell.exec('npm install');
  if (installResult.code !== 0) {
    console.error('Frontend dependency installation failed');
    process.exit(1);
  }
  shell.cd(rootDir);
}

// Ensure frontend is built
if (!fs.existsSync(path.join(frontendDir, 'dist', 'index.html'))) {
  console.log('Building frontend files...');
  shell.cd(frontendDir);
  const buildFrontend = shell.exec('npm run build');
  if (buildFrontend.code !== 0) {
    console.error('Frontend build failed');
    process.exit(1);
  }
  shell.cd(rootDir);
  
  // Verify build result
  if (!fs.existsSync(path.join(frontendDir, 'dist', 'index.html'))) {
    console.error('Frontend build completed but dist/index.html still not found');
    process.exit(1);
  }
}

// 获取端口配置
const { backendPort, frontendPort } = getServerPorts();
console.log(`\n📡 Using ports - Backend: ${backendPort}, Frontend: ${frontendPort}`);

// 先检查端口是否已被占用
async function checkPortAvailability() {
  console.log('\n🔍 Checking port availability...');
  
  const backendCheck = await checkPortService(backendPort, 1000);
  const frontendCheck = await checkPortService(frontendPort, 1000);
  
  if (backendCheck) {
    console.log(`  ⚠️  Port ${backendPort} (backend) is already in use`);
  } else {
    console.log(`  ✓ Port ${backendPort} (backend) is available`);
  }
  
  if (frontendCheck) {
    console.log(`  ⚠️  Port ${frontendPort} (frontend) is already in use`);
  } else {
    console.log(`  ✓ Port ${frontendPort} (frontend) is available`);
  }
  
  return { backendCheck, frontendCheck };
}

// Step 3: Start both backend and frontend servers
console.log('\n🚀 Starting backend and frontend servers...');

// 启动后端服务器
console.log(`\nStarting backend server (port ${backendPort})...`);
const backendProcess = spawn('cmd', [
  '/c', 
  'start',
  `"Remote Debugger Backend (${backendPort})"`,
  'cmd',
  '/c',
  serverExe + ' > backend.log 2>&1'
], {
  cwd: rootDir,
  shell: true,
  detached: true,
  stdio: 'ignore'
});

backendProcess.on('error', (err) => {
  console.error('❌ Failed to start backend:', err.message);
});

console.log(`Backend process started (PID: ${backendProcess.pid})`);

// 启动前端服务器
console.log(`\nStarting frontend development server (port ${frontendPort})...`);
const frontendProcess = spawn('cmd', [
  '/c',
  'start',
  `"Remote Debugger Frontend (${frontendPort})"`,
  'cmd',
  '/k',
  'npm run dev'
], {
  cwd: frontendDir,
  shell: true,
  detached: true,
  stdio: 'ignore'
});

frontendProcess.on('error', (err) => {
  console.error('❌ Failed to start frontend:', err.message);
});

console.log(`Frontend process started (PID: ${frontendProcess.pid})`);

// 让子进程独立运行
backendProcess.unref();
frontendProcess.unref();

// 等待并验证服务器状态
setTimeout(async () => {
  console.log('\n=== Verifying Server Status ===');
  
  // 检查端口占用情况
  await checkPortAvailability();
  
  console.log('\n⏳ Waiting for services to start...');
  
  // 等待后端服务
  const backendRunning = await waitForService(backendPort, 'Backend Server', 20, 1500);
  
  // 等待前端服务
  const frontendRunning = await waitForService(frontendPort, 'Frontend Server', 10, 1000);
  
  console.log('\n=== Final Server Status ===');
  console.log(`Backend server (port ${backendPort}): ${backendRunning ? '✅ RUNNING' : '❌ FAILED'}`);
  console.log(`Frontend server (port ${frontendPort}): ${frontendRunning ? '✅ RUNNING' : '❌ FAILED'}`);
  
  // 最终状态处理
  if (backendRunning && frontendRunning) {
    console.log(`
🎉 SUCCESS! Remote Debugging System is fully operational!

🌐 Access Points:
  🔗 Frontend Interface: http://localhost:${frontendPort}
  ⚙️  Backend API: http://localhost:${backendPort}
  📁 Static Files: http://localhost:${backendPort}

🚀 Opening browser...`);
    
    // 只有两个服务器都启动成功时才打开浏览器
    setTimeout(() => {
      shell.exec(`start http://localhost:${frontendPort}`);
    }, 1000);
  } else {
    console.log(`
⚠️  PARTIAL OR FAILED STARTUP

🔍 Diagnostics:
${!backendRunning ? `  • Backend on port ${backendPort} is not responding\n` : ''}
${!frontendRunning ? `  • Frontend on port ${frontendPort} is not responding\n` : ''}

🔧 Troubleshooting:
  1. Check the opened console windows for error messages
  2. Verify ports are not blocked by firewall
  3. For backend: Ensure ${serverExe} starts without errors
  4. For frontend: Check npm dependencies and dev server config
  
🔄 Manual verification:
  • Backend test: curl http://localhost:${backendPort} or open in browser
  • Frontend test: curl http://localhost:${frontendPort} or open in browser
  
❌ Browser will NOT open due to failed services
`);
  }
  
  // 显示运行信息
  console.log(`
📋 System Information:
  • Backend executable: ${serverExe}
  • Frontend directory: ${frontendDir}
  • Both servers are running in separate windows
  
🛑 To stop:
  • Press Ctrl+C in each server window
  • Or close the command windows
  • Or use Task Manager to end processes
  
✅ Startup script completed at ${new Date().toLocaleTimeString()}
`);
  
}, 2000);