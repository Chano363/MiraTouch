// scripts/build.js - SANE VERSION
const { execSync } = require('child_process');
const path = require('path');
const fs = require('fs');

const rootDir = path.join(__dirname, '..');
const buildDir = path.join(rootDir, 'build');
const frontendDir = path.join(rootDir, 'frontend');

console.log('=== Remote Debugger Build Script ===\n');

function runCommand(cmd, cwd) {
  console.log(`$ ${cmd}`);
  try {
    execSync(cmd, { 
      cwd: cwd, 
      stdio: 'inherit'
    });
    return true;
  } catch (error) {
    console.error(`Command failed with code ${error.status}`);
    return false;
  }
}

// 1. 清理build目录（只清理构建文件，不清理缓存）
console.log('🧹 Cleaning build directory...');
if (fs.existsSync(buildDir)) {
  // 只删除CMake生成的文件，保留Conan文件
  const items = fs.readdirSync(buildDir);
  items.forEach(item => {
    const itemPath = path.join(buildDir, item);
    if (fs.statSync(itemPath).isFile() && 
        !item.includes('conan') && 
        !item.includes('toolchain')) {
      fs.unlinkSync(itemPath);
    }
  });
} else {
  fs.mkdirSync(buildDir, { recursive: true });
}

// 删除CMake缓存
const cacheFiles = ['CMakeCache.txt', 'CMakeFiles', '*.sln', '*.vcxproj'];
cacheFiles.forEach(pattern => {
  try {
    const cmd = `del /q "${path.join(buildDir, pattern)}" 2>nul`;
    execSync(cmd, { cwd: buildDir, shell: true, stdio: 'ignore' });
  } catch {}
});

// 2. 构建前端
console.log('\n📦 Building frontend...');

// 确保依赖已安装
const packageLockPath = path.join(frontendDir, 'package-lock.json');
const nodeModulesPath = path.join(frontendDir, 'node_modules');

// 如果没有node_modules或package-lock.json，运行npm install
if (!fs.existsSync(nodeModulesPath) || !fs.statSync(nodeModulesPath).isDirectory()) {
  console.log('📁 node_modules not found, installing dependencies...');
  if (!runCommand('npm install', frontendDir)) {
    console.log('❌ npm install failed');
    process.exit(1);
  }
} else {
  // 检查package-lock.json是否存在，如果不存在则重新安装
  if (!fs.existsSync(packageLockPath)) {
    console.log('📄 package-lock.json not found, reinstalling dependencies...');
    if (!runCommand('npm install', frontendDir)) {
      console.log('❌ npm install failed');
      process.exit(1);
    }
  }
}

// 运行构建
if (!runCommand('npm run build', frontendDir)) {
  console.log('❌ Frontend build failed');
  
  // 如果构建失败，尝试清理并重新安装
  console.log('🧹 Cleaning and reinstalling dependencies...');
  if (fs.existsSync(nodeModulesPath)) {
    fs.rmSync(nodeModulesPath, { recursive: true, force: true });
  }
  if (fs.existsSync(packageLockPath)) {
    fs.unlinkSync(packageLockPath);
  }
  
  if (!runCommand('npm install', frontendDir)) {
    console.log('❌ Dependency reinstallation failed');
    process.exit(1);
  }
  
  // 再次尝试构建
  if (!runCommand('npm run build', frontendDir)) {
    console.log('❌ Frontend build failed again after reinstallation');
    process.exit(1);
  }
}

console.log('\n🔧 Installing C++ dependencies...');

const profilePath = path.join(rootDir, 'remote_debugger_profile');
if (fs.existsSync(profilePath)) {
  let profileContent = fs.readFileSync(profilePath, 'utf8');
  if (profileContent.includes('compiler.runtime=MD')) {
    profileContent = profileContent.replace('compiler.runtime=MD', 'compiler.runtime=dynamic');
    fs.writeFileSync(profilePath, profileContent);
    console.log('Fixed profile: changed MD to dynamic');
  }
}

// 简单的Conan安装命令
const conanCmd = `conan install .. --profile=../remote_debugger_profile --build=missing`;
if (!runCommand(conanCmd, buildDir)) {
  console.log('Trying with explicit settings...');
  
  // 如果profile有问题，直接用命令行参数
  const conanCmd2 = `conan install .. \
    --build=missing \
    -s compiler=msvc \
    -s compiler.version=193 \
    -s compiler.runtime=dynamic \
    -s arch=x86_64 \
    -s build_type=Release`;
    
  if (!runCommand(conanCmd2, buildDir)) {
    console.error('Conan install failed. Check your network and try again.');
    process.exit(1);
  }
}

// 4. 配置CMake
console.log('\n⚙️  Configuring CMake...');

// 检查toolchain文件的正确位置
const toolchainFile1 = path.join(buildDir, 'conan_toolchain.cmake');
const toolchainFile2 = path.join(buildDir, 'generators', 'conan_toolchain.cmake');
let cmakeCmd;

if (fs.existsSync(toolchainFile1)) {
  // 文件在build根目录
  cmakeCmd = `cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="conan_toolchain.cmake"`;
} else if (fs.existsSync(toolchainFile2)) {
  // 文件在generators子目录
  cmakeCmd = `cmake .. -G "Visual Studio 17 2022" -A x64 -DCMAKE_TOOLCHAIN_FILE="generators/conan_toolchain.cmake"`;
} else {
  // 直接使用Conan预设
  cmakeCmd = `cmake --preset conan-default`;
}

if (!runCommand(cmakeCmd, buildDir)) {
  process.exit(1);
}

// 5. 构建
console.log('\n🔨 Building C++ backend...');
if (!runCommand('cmake --build . --config Release -- /m', buildDir)) {
  process.exit(1);
}

// 6. 复制前端文件
console.log('\n📁 Setting up frontend...');
const frontendDist = path.join(frontendDir, 'dist');
const htmlDir = path.join(buildDir, 'html');

if (fs.existsSync(frontendDist)) {
  if (fs.existsSync(htmlDir)) {
    fs.rmSync(htmlDir, { recursive: true });
  }
  fs.cpSync(frontendDist, htmlDir, { recursive: true });
  console.log('Frontend files copied');
}

// 7. 验证结果
console.log('\n✅ Build completed!');
console.log('\n📊 Output files:');

// 查找可执行文件
const exeLocations = [
  path.join(buildDir, 'Release/remote_debugger_server.exe'),
  path.join(buildDir, 'remote_debugger_server.exe'),
  path.join(buildDir, 'bin/Release/remote_debugger_server.exe')
];

let exeFound = false;
exeLocations.forEach(location => {
  if (fs.existsSync(location)) {
    console.log(`✓ Executable: ${path.relative(rootDir, location)}`);
    exeFound = true;
  }
});

if (!exeFound) {
  console.log('⚠️  Executable not found in expected locations');
  console.log('Searching for .exe files...');
  const searchCmd = `dir /s /b *.exe`;
  try {
    execSync(searchCmd, { cwd: buildDir, stdio: 'inherit' });
  } catch {}
}

console.log(`
🚀 To run:
1. cd build
2. remote_debugger_server.exe  (or .\\Release\\remote_debugger_server.exe)

🌐 Then open: http://localhost:8082
`);