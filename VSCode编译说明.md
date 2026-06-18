# VS Code Arduino 开发快速配置

## 🚀 推荐方案：VS Code + Arduino IDE

由于 Arduino CLI 安装速度太慢，推荐使用 **Arduino IDE 2.0** 配合 **VS Code** 开发。

## 📋 步骤 1：安装 Arduino IDE 2.0

### 快速下载安装
1. **访问官网**：https://www.arduino.cc/en/software
2. **下载 Windows 版本**：选择 "ZIP file" 或 "EXE installer"
3. **解压或安装**：到默认位置 `C:\Users\你的用户名\AppData\Local\Arduino`

### 首次启动配置
1. 打开 Arduino IDE
2. 同意许可协议
3. **重要**：在偏好设置中添加 ESP32 支持
   - 点击 `文件` → `首选项`
   - 在 "附加开发板管理器网址" 中添加：
     ```
     https://dl.espressif.com/dl/package_esp32_index.json
     ```

## ⚙️ 步骤 2：安装 ESP32 核心

1. 点击 `工具` → `开发板` → `开发板管理器`
2. 搜索 "ESP32"
3. 找到 "esp32 by Espressif Systems"
4. 点击 "安装"（约200MB，会快一些）

## 🔌 步骤 3：安装必需库

1. 点击 `工具` → `管理库`
2. 搜索并安装：
   - `NimBLEDevice`（蓝牙库）
   - `DFRobot_MaqueenPlusV2`（麦昆小车库）
   - `DFRobot_HuskylensV2`（AI 摄像头库）

## 💻 步骤 4：VS Code 开发设置

### 配置 VS Code
1. **安装 VS Code 扩展**（可选但推荐）：
   - `C/C++`（Microsoft）
   - `Arduino`（Microsoft）

2. **设置编译任务**：
   创建 `.vscode/tasks.json`：
   ```json
   {
     "version": "2.0.0",
     "tasks": [
       {
         "label": "Arduino Verify",
         "type": "shell",
         "command": "arduino-cli",
         "args": [
           "compile",
           "--fqbn",
           "esp32:esp32:esp32",
           "${fileDirname}"
         ],
         "group": {
           "kind": "build",
           "isDefault": true
         }
       }
     ]
   }
   ```

## 🎯 步骤 5：编译项目

### 方法 A：使用 Arduino IDE（推荐）
1. 在 Arduino IDE 中打开 `Maqueen_ShanWan.ino`
2. 选择开发板：`ESP32 Dev Module`
3. 配置分区表：`工具` → `Partition Scheme` → `OTA (1MB APP/1MB SPIFFS)`
4. 点击 "✓ 验证" 按钮编译

### 方法 B：使用命令行（安装 Arduino IDE 后）
Arduino IDE 安装后会自带 `arduino-cli`，位置：
```
C:\Users\你的用户名\AppData\Local\Arduino\resources\app\arduino-cli.exe
```

编译命令：
```bash
cd "d:\1、文件\1、工作文件\0、项目支持\5、无人驾驶\3、软件\03_固件代码\3、山弯遥控控制小车\Maqueen_ShanWan"

"C:\Users\你的用户名\AppData\Local\Arduino\resources\app\arduino-cli.exe" compile --fqbn esp32:esp32:esp32 --build-property "build.partitions=custom" .
```

## 📦 项目文件清单

确保项目目录包含：
```
Maqueen_ShanWan/
├── Maqueen_ShanWan.ino    # 主程序文件
├── partitions.csv          # OTA分区表（重要！）
├── OTA使用说明.md          # OTA功能说明
└── VSCode编译说明.md      # 本文件
```

## ⚡ VS Code 开发技巧

### 1. 语法高亮
在 `.ino` 文件顶部添加：
```cpp
#if defined(ARDUINO) && ARDUINO >= 100
  #include "Arduino.h"
#else
  #include "WProgram.h"
#endif
```

### 2. 智能提示
安装 `C/C++` 扩展后，VS Code 会自动提供代码补全。

### 3. 编译快捷键
- 在 Arduino IDE 中：`Ctrl+R`（验证）
- 上传到硬件：`Ctrl+U`

## 🚨 常见问题解决

### 问题 1：找不到开发板
**解决**：在 Arduino IDE 中重新安装 ESP32 核心

### 问题 2：库缺失
**解决**：使用 Arduino IDE 的库管理器搜索安装

### 问题 3：编译错误
**解决**：确保 `partitions.csv` 在 `.ino` 同级目录

## 🎉 快速开始（最简单流程）

1. **下载 Arduino IDE 2.0**
2. **添加 ESP32 网址**：`https://dl.espressif.com/dl/package_esp32_index.json`
3. **安装 ESP32 核心**
4. **打开项目文件**：`Maqueen_ShanWan.ino`
5. **选择 OTA 分区表**
6. **点击编译按钮**

**预计耗时：20-30分钟**（主要是下载和安装时间）

## 📞 需要帮助？

完成 Arduino IDE 安装后，告诉我，我可以帮你：
- 验证安装是否成功
- 配置项目编译设置
- 解决编译过程中的错误
- 指导 OTA 功能测试
