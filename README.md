# UNIHIKER K10 Maqueen Controller

这是一个基于 UNIHIKER K10 的麦昆小车控制固件。K10 同时负责连接 BLE 手柄、控制 Maqueen Plus V2 底盘、触发 Huskylens 录像，并提供 AP/STA 双模式 HTTP OTA 和网页配置入口。

## 当前功能

- 自动扫描并连接指定绑定码的 `GamepadSpace-Q34B` 蓝牙手柄。
- 通过手柄控制小车前进、后退、转向和原地转向。
- 通过 LT/RT 控制 Huskylens 开始录像、停止录像和停车。
- 启动后同时开启 WiFi AP 和 STA：
  - AP: `Maqueen-OTA`
  - AP 密码: `12345678`
  - 默认 STA: `DFRobot-guest`
  - 默认 STA 密码: `dfrobot@2017`
- 提供网页配置入口，可修改 STA WiFi 和手柄绑定码。
- 提供 HTTP OTA 上传入口，上传令牌为 `80794`。
- 使用轮速和 K10 加速度强度做高速转向/原地转向的牵引限制，减少打滑和跳动。
- 不向手柄写入震动/电机反馈数据，避免影响正常控制链路。

## 硬件

- UNIHIKER K10
- DFRobot Maqueen Plus V2
- DFRobot Huskylens V2
- BLE 手柄，广播名为 `GamepadSpace-Q34B`

## 软件环境

推荐使用 PlatformIO Core 或 VS Code PlatformIO 插件。

首次编译会下载 DFRobot UNIHIKER 平台、K10 Arduino 框架和 ESP32-S3 工具链，耗时取决于网络环境。

常用命令：

```powershell
pio run
pio device list
pio run -t upload --upload-port COM26
pio device monitor --port COM26 --baud 115200
```

`COM26` 只是示例，实际端口以 `pio device list` 显示为准。

## 快速使用

1. 连接 K10 到电脑。
2. 编译固件：

```powershell
pio run
```

3. 查看串口：

```powershell
pio device list
```

4. 通过串口上传：

```powershell
pio run -t upload --upload-port COM26
```

5. 启动后，K10 会自动开启 AP 和连接默认 STA。
6. 访问网页配置入口：
   - 连接 AP 后访问 `http://192.168.4.1`
   - 或在同一 STA 网络内访问 K10 的 STA 地址
7. 在网页中填写 WiFi 和 5 位手柄绑定码，保存后固件会重新扫描并连接对应手柄。

默认手柄绑定码为 `32652`。

## 手柄操作

- LT: 开始录像并进入可驾驶状态。
- RT: 停止录像、停车并退出驾驶状态。
- 左摇杆 Y: 前进/后退。
- 右摇杆 X: 左右转向。
- 右摇杆 Y 推上: 普通速度上限。
- 右摇杆 Y 拉下: 最高速度上限。

只有进入可驾驶状态后，摇杆才会驱动车辆。蓝牙断开或 OTA 上传中，车辆会停车。

## OTA 上传

OTA 服务常驻运行，不需要按键进入 OTA 模式。

AP 模式上传：

```powershell
curl.exe --noproxy "*" -f -S -m 180 -F "file=@.pio/build/unihiker_k10/firmware.bin" "http://192.168.4.1/ota?token=80794"
```

STA 模式上传：

```powershell
curl.exe --noproxy "*" -f -S -m 180 -F "file=@.pio/build/unihiker_k10/firmware.bin" "http://<K10_STA_IP>/ota?token=80794"
```

上传成功后设备会自动重启。

## 更多说明

完整操作流程见 [操作说明.md](操作说明.md)。

