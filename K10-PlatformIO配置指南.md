# UNIHIKER K10 PlatformIO 配置指南

## 🎯 你的项目架构
- **主控板**: UNIHIKER K10（ESP32-S3）
- **底盘**: 麦昆小车
- **通信**: BLE 蓝牙手柄 → K10 → 麦昆小车

这是一个很棒的集成项目！K10 作为中央控制器，通过蓝牙接收手柄信号，然后控制麦昆小车。

## 🚀 快速安装 PlatformIO

### 方法 1：使用 Python pip（推荐）

```bash
# 安装 PlatformIO Core
pip install platformio

# 验证安装
pio --version
```

### 方法 2：使用 VS Code 扩展（最简单）

1. 打开 VS Code
2. 安装扩展：`PlatformIO IDE`
3. 重启 VS Code
4. PlatformIO 会自动配置环境

## 📋 项目结构已配置

你的 `platformio.ini` 已配置为：
- ✅ 平台：DFRobot UNIHIKER K10
- ✅ 开发板：unihiker_k10
- ✅ OTA 分区表支持
- ✅ 必需库：NimBLEDevice, MaqueenPlusV2, HuskylensV2

## 🔧 编译步骤

### 使用命令行：

```bash
cd "d:\1、文件\1、工作文件\0、项目支持\5、无人驾驶\3、软件\03_固件代码\3、山弯遥控控制小车\Maqueen_ShanWan"

# 首次编译（会下载K10 SDK和工具链，约500MB）
pio run

# 上传到K10
pio run -t upload --upload-port COM4

# 串口监控
pio device monitor -p COM4 --baud 115200
```

### 使用 VS Code：

1. 打开项目文件夹
2. 等待 PlatformIO 自动初始化
3. 点击底部状态栏的 ✓ 图标编译
4. 点击 → 图标上传

## ⚡ 首次编译注意

第一次编译会下载：
- DFRobot UNIHIKER 平台（~1MB）
- K10 Arduino 框架（~500MB）
- ESP32-S3 工具链（~300MB）
- 其他工具（~100MB）

**总下载量：约900MB**
**预计时间：10-30分钟**（取决于网络速度）

## 🎯 项目文件检查

确保以下文件在同一目录：
```
Maqueen_ShanWan/
├── platformio.ini           # PlatformIO 配置（已更新）
├── src/
│   └── main.cpp            # 将 .ino 内容移到这里
├── partitions.csv           # OTA 分区表
├── lib/                    # 项目专用库（可选）
└── test/                   # 测试文件（可选）
```

## 🔄 从 Arduino .ino 转换为 PlatformIO

需要将 `Maqueen_ShanWan.ino` 转换为 `src/main.cpp`：

1. 创建 `src` 目录
2. 将 `.ino` 文件内容移到 `src/main.cpp`
3. 添加必要的函数声明
4. 修改为标准 C++ 格式

## 🚨 K10 特殊注意事项

1. **USB CDC**: 必须启用 `-DARDUINO_USB_CDC_ON_BOOT=1`
2. **OTA 分区**: 使用自定义 `partitions.csv`
3. **库依赖**: DFRobot 专用库版本

## 📞 下一步

选择安装方式：

**A. 命令行安装**：
```bash
pip install platformio
```

**B. VS Code 扩展**：
在 VS Code 中安装 "PlatformIO IDE" 扩展

安装完成后告诉我，我可以帮你：
- 验证 PlatformIO 环境
- 转换 .ino 为 PlatformIO 项目
- 进行首次编译测试
