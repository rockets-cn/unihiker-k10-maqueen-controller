# Maqueen OTA 无线更新使用说明

## 已完成的修改

✅ **1. 创建了自定义分区表** (`partitions.csv`)
- 支持 OTA 双分区（app0 + app1）
- 标准 ESP32 OTA 布局，无需 K10 AI 模型分区

✅ **2. 添加了 OTA 功能代码**
- WiFi AP 热点模式（`Maqueen-OTA`，密码：`12345678`）
- HTTP OTA 端点：`/ota`
- 串口命令触发 OTA 模式
- OTA 状态指示（绿灯）

✅ **3. 修改了主循环逻辑**
- OTA 模式优先级高于蓝牙控制
- 支持 OTA 上传过程中的状态监控

---

## 编译和上传步骤

### 第一步：首次 USB 上传（必须）

由于修改了分区表，**第一次必须通过 USB 上传**：

```bash
# 使用 Arduino CLI 编译（带自定义分区表）
arduino-cli compile --fqbn esp32:esp32:maqueenplus . \
  --output-dir build \
  --build-property "build.partitions=custom"

# 通过 USB 上传（请替换 COM 端口）
arduino-cli upload -p COM4 --fqbn esp32:esp32:maqueenplus . \
  --input-dir build
```

或者使用 Arduino IDE：
1. 选择开发板：`ESP32 -> Maqueen Plus V2`
2. 上传代码到小车

### 第二步：使用 OTA 无线更新

上传成功后，后续可以通过 WiFi 无线更新：

#### 方法：手柄菜单键触发 OTA 模式

1. **确保小车已启动并连接蓝牙手柄**

2. **长按菜单键 3 秒**：
   - 车灯显示黄色 → 正在计数
   - 保持按住 3 秒后，车灯变绿 → OTA 模式已启动
   - 松开菜单键可取消

3. **小车进入 OTA 模式**：
   - 停止蓝牙连接
   - 停止小车运动（安全停车）
   - 启动 WiFi AP：`Maqueen-OTA`（密码：`12345678`）
   - 车灯显示绿色

4. **电脑连接到 WiFi**: `Maqueen-OTA`

5. **上传固件**：

```bash
# 使用 curl
curl -F "file=@build/Maqueen_ShanWan.ino.bin" http://192.168.4.1/ota

# 或使用 Python 脚本（来自 unihiker-k10-ota）
python C:\Users\zrz15\.claude\skills\unihiker-k10-ota\scripts\ota_upload.py build/Maqueen_ShanWan.ino.bin --ip 192.168.4.1
```

6. **等待上传完成**，小车会自动重启

---

## 功能特性

### 📡 OTA 模式特点

- **安全保护**：进入 OTA 模式自动停车、断开蓝牙
- **进度显示**：串口实时打印上传进度
- **状态指示**：绿灯表示 OTA 模式，红灯表示正常模式
- **Web 界面**：访问 `http://192.168.4.1` 查看 OTA 状态

### 🔧 WiFi 配置

默认配置：
- **AP 名称**: `Maqueen-OTA`
- **密码**: `12345678`
- **IP 地址**: `192.168.4.1`
- **OTA 端点**: `http://192.168.4.1/ota`

### 📝 其他 HTTP 端点

- `GET /` - OTA 说明页面
- `GET /status` - JSON 状态查询
- `POST /ota` - 固件上传端点

---

## 故障排查

| 问题 | 可能原因 | 解决方案 |
|------|---------|---------|
| `BEGIN_FAIL` | 分区表未生效 | 确认第一次上传时使用了 `partitions.csv` |
| 无法连接 WiFi | 小车未进入 OTA 模式 | 检查串口是否收到 `OTA` 命令 |
| 上传后不重启 | 更新失败 | 检查串口日志，确认上传无错误 |
| 蓝牙连接失败 | OTA 模式下蓝牙已关闭 | 重启小车退出 OTA 模式 |

---

## 注意事项

⚠️ **重要提醒**：

1. **首次必须 USB 上传** - 修改分区表后必须通过 USB 一次性上传
2. **保留 OTA 代码** - 后续更新的固件也必须包含 OTA 功能，否则会失去无线更新能力
3. **WiFi 干扰** - OTA 模式下蓝牙会断开，确保小车安全停放
4. **串口监控** - 建议打开串口监视器查看 OTA 进度

---

## 自定义配置

如需修改 OTA 参数，编辑代码中的宏定义：

```cpp
#define OTA_SSID         "Maqueen-OTA"    // WiFi AP 名称
#define OTA_PASSWORD     "12345678"        // WiFi 密码（至少8位）
#define OTA_PORT         80                // HTTP 端口
```

---

## 示例脚本

### PowerShell 上传脚本

```powershell
$ip = "192.168.4.1"
$bin = "build/Maqueen_ShanWan.ino.bin"

Write-Host "正在上传固件到 $ip..."
curl -F "file=@$bin" "http://$ip/ota"
Write-Host "上传完成，小车将在3秒后重启"
```

### Python 上传脚本

```python
import requests

ip = "192.168.4.1"
bin_file = "build/Maqueen_ShanWan.ino.bin"

with open(bin_file, 'rb') as f:
    files = {'file': f}
    response = requests.post(f"http://{ip}/ota", files=files)

print(f"上传结果: {response.text}")
```

---

**技术支持**: 基于 unihiker-k10-ota 技能实现
