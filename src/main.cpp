/**
 * =======================================================================
 * UNIHIKER K10 蓝牙手柄直接控制麦昆小车一体化程序 (BLE + 小车控制 + 摄像头录像)
 * -----------------------------------------------------------------------
 * 功能说明:
 * 1. 自动配对连接蓝牙游戏手柄 (GamepadSpace-Q34B)。
 * 2. 独占解析手柄数据：
 * - 按下【LT键】：触发 Huskylens 开启录像，车灯闪烁。
 * - 按下【RT键】：触发 Huskylens 停止录像并停车。
 * - 【左摇杆Y轴】：直控制小车前进速度。
 * - 【右摇杆X轴】：直控制小车左右转弯。
 * =======================================================================
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <NimBLEDevice.h>
#include <DFRobot_MaqueenPlusV2.h>
#include "unihiker_k10.h"
#include "DFRobot_HuskylensV2.h"

// 函数声明（C++需要显式声明）
void scanDevices();
bool connectToServer();
void controlCarWithSpeed(uint16_t x, uint16_t y);
void handleOta();
void handleOtaUpload();
void scheduleRestart();
void processOtaModeRequest();
void startWifiMaintenance();
void connectSta();
void loadPairCode();
void handleRoot();
void handleWifiSave();
bool handleGamepadOtaCommand(bool ltPressed, bool rtPressed);
String htmlEscape(const String& value);
int applyDeadband(int value, int deadband);
int rampToward(int current, int target, int maxStep);
int updateTractionLimit(int leftTarget, int rightTarget, int forwardAbs, int turnAbs);
void findVibrationCharacteristic();
bool writeGamepadVibration(uint8_t left, uint8_t right, bool force = false);
void stopGamepadVibration();
void startSensorVibration(uint8_t left, uint8_t right, unsigned long durationMs);
void updateGamepadVibration();
void driveMotor(DFRobot_MaqueenPlusV2::Dir motor, int speed);
void resetDriveState();

// ====== 硬件及通信参数 ======
#define SERIAL_BAUD      115200
#define LED_STATUS       2                     // 板载状态指示灯
#define SCAN_DURATION    0                     // 持续扫描
#define RECONNECT_FAIL_LIMIT 3                 // 重连失败上限
#define TARGET_NAME      "GamepadSpace-Q34B"   // 目标手柄广播名称
static const uint16_t HID_SERVICE_UUID_val = 0x1812;

// ====== OTA 参数 ======
#define OTA_SSID         "Maqueen-OTA"         // OTA WiFi AP 名称
#define OTA_PASSWORD     "12345678"            // OTA WiFi 密码
#define OTA_TOKEN        "80794"               // OTA 上传令牌
#define OTA_PORT         80                     // HTTP 服务端口
#define DEFAULT_STA_SSID "DFRobot-guest"       // 默认 STA WiFi 名称
#define DEFAULT_STA_PASS "dfrobot@2017"        // 默认 STA WiFi 密码

// ====== 绑定配置 ======
#define PAIR_CODE   32652                      // 默认5位哈希绑定码
volatile uint32_t pairCode = PAIR_CODE;

// ====== 实体对象声明 ======
HuskylensV2           huskylens;
UNIHIKER_K10          k10;
DFRobot_MaqueenPlusV2  maqueenPlus;
Preferences           wifiPrefs;
NimBLEClient* pClient = nullptr;
NimBLERemoteCharacteristic* vibrationChar = nullptr;
WebServer             server(OTA_PORT);        // OTA Web 服务器

// ====== 状态全局变量 ======
volatile bool deviceConnected = false;
volatile bool targetFound = false;
int reconnectFailCount = 0;
std::string targetAddress = "";
uint8_t targetAddrType = 0;
static String annotation = "";


// ====== OTA 模式变量 ======
volatile bool otaUploadActive = false;         // OTA upload in progress
bool wifiMaintenanceStarted = false;
volatile bool otaControlRequested = false;
bool otaHoldActive = false;
bool restartPending = false;
unsigned long restartAtMs = 0;
unsigned long otaHoldStartMs = 0;
String staSsid = DEFAULT_STA_SSID;
String staPass = DEFAULT_STA_PASS;

// Car running state machine
static String lastGAMEPAD = "STOP"; 
static unsigned long lastLedTime = 0;
static bool ledOn = false;

// Temporary storage for joystick mapping (mapped to car's original 0~1023 system)
#define JOY_MID 128
// #define MOTOR_MAX 150
uint16_t MOTOR_MAX = 150;
uint16_t joyX = JOY_MID;
uint16_t joyY = JOY_MID;
static int currentLeftSpeed = 0;
static int currentRightSpeed = 0;
static int filteredForward = 0;
static int filteredTurn = 0;
static const int HIGH_SPEED_TURN_START = 180;
static const int HIGH_SPEED_MIN_TURN_RATIO = 65;
static const int HIGH_SPEED_INNER_MIN_RATIO = 35;
static const int PIVOT_HIGH_SPEED_LIMIT = 180;
static const int PIVOT_TRACTION_PERCENT = 90;
static const int PIVOT_RAMP_STEP = 14;
static const int TRACTION_RELEASE_PERCENT = 78;
static const int TRACTION_RECOVER_STEP = 3;
static const int ACCEL_JOLT_THRESHOLD = 180;
static const unsigned long TRACTION_SAMPLE_MS = 50;
static const unsigned long GAMEPAD_OTA_HOLD_MS = 3000;
static const char* VIBRATION_CHAR_UUID = "91680002-1111-6666-8888-0123456789ab";
static const unsigned long VIBRATION_MIN_INTERVAL_MS = 180;
static const unsigned long VIBRATION_PULSE_MS = 90;
static const uint8_t VIBRATION_MAX_SENSOR_STRENGTH = 180;
static int tractionLimitPercent = 100;
static unsigned long lastTractionSampleMs = 0;
static float lastLeftWheelSpeed = 0.0f;
static float lastRightWheelSpeed = 0.0f;
static int lastAccelStrength = 0;
static bool tractionSampleReady = false;
static bool vibrationActive = false;
static uint8_t currentVibrationLeft = 0;
static uint8_t currentVibrationRight = 0;
static unsigned long vibrationStopAtMs = 0;
static unsigned long lastVibrationWriteMs = 0;
static unsigned long lastVibrationStartMs = 0;

// ====== 函数声明 ======
void scanDevices();
bool connectToServer();
void controlCarWithSpeed(uint16_t x, uint16_t y);

// ==================== 蓝牙 MAC 哈希算法 ====================
uint32_t macTo5DigitCode(const char* macStr) {
    if (!macStr) return 0;
    String cleanMac = "";
    while (*macStr) {
        char c = *macStr++;
        if (c != ':' && c != '-' && c != ' ') {
            if (c >= 'A' && c <= 'Z') c = c + 32;
            cleanMac += c;
        }
    }
    String targetPart = cleanMac;
    if (cleanMac.length() >= 12) {
        targetPart = cleanMac.substring(cleanMac.length() - 6); 
    }
    unsigned long hash = 5381;
    for (size_t i = 0; i < targetPart.length(); i++) {
        hash = ((hash << 5) + hash) + targetPart[i];
    }
    return (hash % 90000) + 10000;
}

// ==================== 蓝牙连接与安全状态回调 ====================
class MyClientCallback : public NimBLEClientCallbacks {
    void onConnect(NimBLEClient* pClient) {
        deviceConnected = true;
        reconnectFailCount = 0;
        digitalWrite(LED_STATUS, HIGH);
        Serial.println("\n>>> [SUCCESS] Bluetooth controller connected, ready for control <<<");
    }

    void onDisconnect(NimBLEClient* pClient, int reason) {
        deviceConnected = false;
        vibrationChar = nullptr;
        vibrationActive = false;
        currentVibrationLeft = 0;
        currentVibrationRight = 0;
        digitalWrite(LED_STATUS, LOW);
        resetDriveState();
        maqueenPlus.motorStop(maqueenPlus.ALL); // Safety protection: emergency stop on disconnect
        Serial.printf(">>> [INFO] Controller disconnected (reason: %d), safety stop, retrying... <<<\n", reason);
    }
};

// ==================== 扫描回调：精准定向拦截 ====================
class MyAdvertisedDeviceCallbacks : public NimBLEScanCallbacks {
    void onResult(const NimBLEAdvertisedDevice* advertisedDevice) override {
        if (targetFound || deviceConnected) return;

        std::string name = advertisedDevice->getName();
        if (name != TARGET_NAME) return;

        std::string mac = advertisedDevice->getAddress().toString();
        uint32_t calculatedCode = macTo5DigitCode(mac.c_str());

        if (calculatedCode == pairCode) {
            Serial.println("\n>>> [MATCH] Found matching controller signature! <<<");
            targetAddress = mac;
            targetAddrType = advertisedDevice->getAddress().getType();
            targetFound = true;
            NimBLEDevice::getScan()->stop();
        }
    }
};
static MyAdvertisedDeviceCallbacks gScanCallbacks;

// ==================== 【接收解析及逻辑控制核心】 ====================
void notifyCallback(NimBLERemoteCharacteristic* pChar, uint8_t* pData, size_t length, bool isNotify) {
    if (length < 10) return; // 标准手柄报告长度校验

    // ---- 1. 解析主要动作按键 (Byte 5) ----
    bool btn_LT_digital  = (pData[6] & 0x01) != 0;  // LT 触底按键
    bool btn_RT_digital  = (pData[6] & 0x02) != 0;  // RT 触底按键
    if (handleGamepadOtaCommand(btn_LT_digital, btn_RT_digital)) return;
    if (otaUploadActive) return;

    // ---- 2. 解析左摇杆模拟量 (Byte 0 和 Byte 1，范围 0~255) ----
    uint8_t right_stick_x  = pData[2];
    uint8_t left_stick_y  = pData[1];
    uint8_t right_stick_y = pData[3];

    // ---- 3. 业务逻辑控制：录像与动作状态机 ----
    // 按下 A 键：开始录像
    if (btn_LT_digital && (lastGAMEPAD == "STOP")) {
        maqueenPlus.set_rgbLed(2, 0xFF0000); // 点亮红灯
        lastLedTime = millis();
        ledOn = true;
        // huskylens.startRecording(MEDIA_TYPE_VIDEO, 0, String("aicar_") + String(millis()) + ".mp4");
        huskylens.e2eStartRecording();
        lastGAMEPAD = "START";
        Serial.println("手柄触发：开始录像");
    }
    // 按下 B 键：停止录像并停车
    else if (btn_RT_digital && (lastGAMEPAD == "START")) {
        maqueenPlus.set_rgbLed(2, 0xFF0000);
        lastLedTime = millis();
        ledOn = true;
        // huskylens.stopRecording(MEDIA_TYPE_VIDEO);
        huskylens.e2eStopRecording();
        lastGAMEPAD = "STOP";

        // 恢复中位值并停车
        joyX = JOY_MID;
        joyY = JOY_MID;
        stopGamepadVibration();
        resetDriveState();
        maqueenPlus.motorStop(maqueenPlus.ALL);
        Serial.println("手柄触发：停止录像 & 紧急停车");
    }

    // ---- 4. 业务逻辑控制：运动摇杆数值映射 ----
    if (lastGAMEPAD == "START") {
        // 将手柄的 0 ~ 255 线性映射到小车原程序所需的 0 ~ 255
        joyX = map(right_stick_x, 0, 255, 0, 255);
        joyY = map(left_stick_y, 255, 0, 0, 255);
        
        annotation = String(joyX) + "," + String(joyY);
		huskylens.e2eSendAnnotation(annotation);

        if (right_stick_y > 200){
          MOTOR_MAX = 150;
        }else if (right_stick_y < 50){
          MOTOR_MAX = 255;
        }
        // MOTOR_MAX = map(right_stick_y, 255, 0, 50, 255);
    }
}

bool handleGamepadOtaCommand(bool ltPressed, bool rtPressed) {
    if (wifiMaintenanceStarted || otaUploadActive) return ltPressed && rtPressed;

    if (ltPressed && rtPressed) {
        unsigned long now = millis();
        if (!otaHoldActive) {
            otaHoldActive = true;
            otaHoldStartMs = now;
            lastGAMEPAD = "STOP";
            joyX = JOY_MID;
            joyY = JOY_MID;
            stopGamepadVibration();
            resetDriveState();
            maqueenPlus.motorStop(maqueenPlus.ALL);
            maqueenPlus.set_rgbLed(2, 0x00FFFF);
            ledOn = false;
            Serial.println("[OTA] Hold LT+RT for 3 seconds to enable WiFi/OTA mode.");
        }

        if (!otaControlRequested && now - otaHoldStartMs >= GAMEPAD_OTA_HOLD_MS) {
            otaControlRequested = true;
            otaHoldActive = false;
            maqueenPlus.set_rgbLed(2, 0x00FF00);
            Serial.println("[OTA] Gamepad command accepted; enabling WiFi/OTA mode.");
        }
        return true;
    }

    if (otaHoldActive) {
        otaHoldActive = false;
        maqueenPlus.set_rgbLed(2, 0x000000);
        Serial.println("[OTA] Gamepad OTA command cancelled.");
    }
    return false;
}

// ==================== 主初始化程序 ====================
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(1000);
    Serial.println("\n==================================================");
    Serial.println("  MaqueenPlus V2 & BLE Gamepad Integrated System  ");
    Serial.println("==================================================");

    k10.begin();

    // 初始化麦昆小车底盘与车灯
    maqueenPlus.set_rgbLed(2, 0xFF0000); // 启动初始化红灯指示
    maqueenPlus.motorStop(maqueenPlus.ALL);
    
    // 初始化 Huskylens 摄像头
    Wire.begin();
    while (!huskylens.begin(Wire)) {
        Serial.println("等待 Huskylens 摄像头连接...");
        delay(500);
    }
    Serial.println("Huskylens 初始化成功！");
    
    pinMode(LED_STATUS, OUTPUT);
    digitalWrite(LED_STATUS, LOW);

    // 初始化 BLE 蓝牙客户端
    NimBLEDevice::init("");
    NimBLEDevice::deleteAllBonds();
    NimBLEDevice::setSecurityAuth(true, true, true);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_DISPLAY_YESNO);

    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback(), false);

    loadPairCode();
    Serial.printf("[系统就绪] 当前设定手柄绑定码: %05u\n", pairCode);
    WiFi.mode(WIFI_OFF);
    Serial.println("[系统就绪] WiFi is off. Hold LT+RT for 3 seconds to enable OTA/config mode.");
    
    delay(1000);
    maqueenPlus.set_rgbLed(2, 0x000000); // 初始化完成，熄灭麦昆车灯
}

// ==================== 主循环调度程序 ====================
void loop() {
    if (wifiMaintenanceStarted) {
        server.handleClient();
    }
    scheduleRestart();
    processOtaModeRequest();
    updateGamepadVibration();

    if (wifiMaintenanceStarted || otaUploadActive) {
        stopGamepadVibration();
        maqueenPlus.motorStop(maqueenPlus.ALL);
        delay(5);
        return;
    }

    // 1. 处理蓝牙底层的扫描、连接与重连状态机
    if (!deviceConnected) {
        if (!targetFound) {
            scanDevices();
        } else {
            if (!connectToServer()) {
                reconnectFailCount++;
                Serial.printf("[WARNING] Bluetooth connection failed, attempt %d...\n", reconnectFailCount);
                if (reconnectFailCount >= RECONNECT_FAIL_LIMIT) {
                    Serial.println("[TIMEOUT] Connection limit reached, rescanning...");
                    targetFound = false;
                    reconnectFailCount = 0;
                }
                delay(200);
            }
        }
    }

    // 2. Auto-off indicator LED (non-blocking timer)
    if (ledOn && (millis() - lastLedTime > 300)) {
        maqueenPlus.set_rgbLed(2, 0x000000);
        ledOn = false;
    }

    // 3. Only allow joystick to drive car when in “START” state
    if (deviceConnected && (lastGAMEPAD == "START")) {
        controlCarWithSpeed(joyX, joyY);
    }

    delay(10); // Maintain underlying task stability
}

// ==================== BLE 驱动核心方法实现 ====================
void scanDevices() {
    if (targetFound) return;
    Serial.printf("[SCANNING] Looking for controller [%s] with hash code [%05u]...\n", TARGET_NAME, pairCode);
    
    NimBLEScan* pNimBLEScan = NimBLEDevice::getScan();
    pNimBLEScan->setScanCallbacks(&gScanCallbacks, false);
    pNimBLEScan->setActiveScan(true);
    pNimBLEScan->setInterval(45);
    pNimBLEScan->setWindow(30);
    pNimBLEScan->setMaxResults(0);
    pNimBLEScan->start(SCAN_DURATION, false);
}

bool connectToServer() {
    Serial.printf("Initiating connection with controller [%s]...\n", targetAddress.c_str());
    if (!pClient->connect(NimBLEAddress(targetAddress, targetAddrType))) {
        return false;
    }

    pClient->secureConnection();
    delay(1000);
    pClient->exchangeMTU();
    pClient->discoverAttributes();
    findVibrationCharacteristic();

    NimBLERemoteService* pHidService = pClient->getService(NimBLEUUID(HID_SERVICE_UUID_val));
    if (pHidService == nullptr) {
        Serial.println(" -> ERROR: Target device is not a standard HID controller");
        pClient->disconnect();
        return false;
    }

    const std::vector<NimBLERemoteCharacteristic*>& charList = pHidService->getCharacteristics();

    int successCount = 0;
    for (size_t i = 0; i < charList.size(); i++) {
        NimBLERemoteCharacteristic* pChar = charList[i];

        if (!pChar->canNotify() && !pChar->canIndicate()) continue;

        NimBLERemoteDescriptor* pCcDescriptor = pChar->getDescriptor(NimBLEUUID((uint16_t)0x2902));
        if (pCcDescriptor != nullptr) {
            uint8_t val[2] = {0x01, 0x00};
            pCcDescriptor->writeValue(val, 2, true);
        }
        if (pChar->subscribe(true, notifyCallback, true)) {
            successCount++;
        }
    }
    return (successCount > 0);
}

// ==================== 动力补偿版极简差分算法 ====================
// void controlCarWithSpeed(uint16_t x, uint16_t y) {
//     static int lastLeftSpeed = 0;
//     static int lastRightSpeed = 0;
    
//     // 1. 将手柄的 0~1023 正确转化为实际物理速度基准
//     // 注意：根据你之前的数据，X是转向，Y是前进后退
//     int speedForward = map(y, 0, 255, -MOTOR_MAX, MOTOR_MAX); // 前进/后退基准
//     int speedTurn    = map(x, 0, 255, -MOTOR_MAX, MOTOR_MAX); // 左右转向基准
    
//     // 2. 摇杆硬件中位死区（防止手柄回弹不准导致微动）
//     if (abs(speedForward) < 5) speedForward = 0;
//     if (abs(speedTurn) < 5)    speedTurn = 0;
    
//     // 3. 核心混控公式（标准差分）
//     // 纯推 X 轴时，speedForward=0，leftSpeed 和 rightSpeed 将直接获得完整的 speedTurn 动力！
//     int leftSpeed  = speedForward + speedTurn;
//     int rightSpeed = speedForward - speedTurn;
    
//     // 5. 限制整体输出幅值
//     leftSpeed  = constrain(leftSpeed, -MOTOR_MAX, MOTOR_MAX);
//     rightSpeed = constrain(rightSpeed, -MOTOR_MAX, MOTOR_MAX);
    
//     // 6. 物理输出死区限制（低于20的输出直接归零，防止电机嗡嗡响却不转）
//     if (abs(leftSpeed) < 5)  leftSpeed = 0;
//     if (abs(rightSpeed) < 5) rightSpeed = 0;
    
//     // 7. 缓动平滑滤波（从 50 降低到 30，既保证原地旋转响应快，又保证行驶不剧烈抖动）
//     int maxSpeedChange = 30;  
//     if (abs(leftSpeed - lastLeftSpeed) > maxSpeedChange) {
//         leftSpeed = (leftSpeed > lastLeftSpeed) ? (lastLeftSpeed + maxSpeedChange) : (lastLeftSpeed - maxSpeedChange);
//     }
//     if (abs(rightSpeed - lastRightSpeed) > maxSpeedChange) {
//         rightSpeed = (rightSpeed > lastRightSpeed) ? (lastRightSpeed + maxSpeedChange) : (lastRightSpeed - maxSpeedChange);
//     }
    
//     // 8. 驱动电机
//     int speedThreshold = 4;
//     // 左电机驱动
//     if (abs(leftSpeed - lastLeftSpeed) > speedThreshold || (leftSpeed == 0 && lastLeftSpeed != 0)) {
//         if (leftSpeed > 0) {
//             maqueenPlus.motorRun(maqueenPlus.LEFT, maqueenPlus.CW, leftSpeed);
//         } else if (leftSpeed < 0) {
//             maqueenPlus.motorRun(maqueenPlus.LEFT, maqueenPlus.CCW, -leftSpeed);
//         } else {
//             maqueenPlus.motorStop(maqueenPlus.LEFT);
//         }
//         lastLeftSpeed = leftSpeed;
//     }
//     // 右电机驱动
//     if (abs(rightSpeed - lastRightSpeed) > speedThreshold || (rightSpeed == 0 && lastRightSpeed != 0)) {
//         if (rightSpeed > 0) {
//             maqueenPlus.motorRun(maqueenPlus.RIGHT, maqueenPlus.CW, rightSpeed);
//         } else if (rightSpeed < 0) {
//             maqueenPlus.motorRun(maqueenPlus.RIGHT, maqueenPlus.CCW, -rightSpeed);
//         } else {
//             maqueenPlus.motorStop(maqueenPlus.RIGHT);
//         }
//         lastRightSpeed = rightSpeed;
//     }
// }


int applyDeadband(int value, int deadband) {
    return abs(value) < deadband ? 0 : value;
}

int rampToward(int current, int target, int maxStep) {
    int delta = target - current;
    if (abs(delta) <= maxStep) return target;
    return current + (delta > 0 ? maxStep : -maxStep);
}

void findVibrationCharacteristic() {
    vibrationChar = nullptr;
    if (pClient == nullptr || !pClient->isConnected()) return;

    NimBLEUUID targetUuid(VIBRATION_CHAR_UUID);
    const std::vector<NimBLERemoteService*>& serviceList = pClient->getServices(false);

    for (size_t i = 0; i < serviceList.size(); i++) {
        NimBLERemoteService* service = serviceList[i];
        if (service == nullptr) continue;

        const std::vector<NimBLERemoteCharacteristic*>& charList = service->getCharacteristics(false);
        for (size_t j = 0; j < charList.size(); j++) {
            NimBLERemoteCharacteristic* pChar = charList[j];
            if (pChar == nullptr) continue;
            if (!pChar->getUUID().equals(targetUuid)) continue;
            if (!pChar->canWrite() && !pChar->canWriteNoResponse()) continue;

            vibrationChar = pChar;
            Serial.printf("[INFO] Vibration characteristic ready: uuid=%s handle=0x%04x\n",
                          pChar->getUUID().toString().c_str(), pChar->getHandle());
            return;
        }
    }

    Serial.println("[INFO] Vibration characteristic not found; sensor feedback disabled.");
}

bool writeGamepadVibration(uint8_t left, uint8_t right, bool force) {
    if (!deviceConnected || otaUploadActive || vibrationChar == nullptr) return false;

    unsigned long now = millis();
    if (!force &&
        left == currentVibrationLeft &&
        right == currentVibrationRight &&
        now - lastVibrationWriteMs < VIBRATION_MIN_INTERVAL_MS) {
        return false;
    }

    uint8_t packet[6] = {0x10, 0x04, 0x08, 0x00, left, right};
    if (!vibrationChar->writeValue(packet, sizeof(packet), false)) {
        return false;
    }

    currentVibrationLeft = left;
    currentVibrationRight = right;
    lastVibrationWriteMs = now;
    return true;
}

void stopGamepadVibration() {
    vibrationActive = false;
    vibrationStopAtMs = 0;
    if (currentVibrationLeft == 0 && currentVibrationRight == 0) return;
    writeGamepadVibration(0, 0, true);
}

void startSensorVibration(uint8_t left, uint8_t right, unsigned long durationMs) {
    if (!deviceConnected || vibrationChar == nullptr || otaUploadActive) return;

    unsigned long now = millis();
    if (now - lastVibrationStartMs < VIBRATION_MIN_INTERVAL_MS) return;

    if (writeGamepadVibration(left, right, true)) {
        vibrationActive = true;
        vibrationStopAtMs = now + durationMs;
        lastVibrationStartMs = now;
    }
}

void updateGamepadVibration() {
    if (!vibrationActive) return;
    if (!deviceConnected || otaUploadActive || millis() >= vibrationStopAtMs) {
        stopGamepadVibration();
    }
}

void driveMotor(DFRobot_MaqueenPlusV2::Dir motor, int speed) {
    if (speed > 0) {
        maqueenPlus.motorRun(motor, maqueenPlus.CW, speed);
    } else if (speed < 0) {
        maqueenPlus.motorRun(motor, maqueenPlus.CCW, -speed);
    } else {
        maqueenPlus.motorStop(motor);
    }
}

void resetDriveState() {
    currentLeftSpeed = 0;
    currentRightSpeed = 0;
    filteredForward = 0;
    filteredTurn = 0;
    tractionLimitPercent = 100;
    tractionSampleReady = false;
}

int updateTractionLimit(int leftTarget, int rightTarget, int forwardAbs, int turnAbs) {
    unsigned long now = millis();
    if (now - lastTractionSampleMs < TRACTION_SAMPLE_MS) {
        return tractionLimitPercent;
    }

    lastTractionSampleMs = now;
    float leftWheelSpeed = maqueenPlus.Get_speed(1);
    float rightWheelSpeed = maqueenPlus.Get_speed(0);
    int accelStrength = k10.getStrength();

    int commandAbs = max(abs(leftTarget), abs(rightTarget));
    bool highCommand = commandAbs > HIGH_SPEED_TURN_START;
    bool pivotingFast = forwardAbs <= 20 && turnAbs > HIGH_SPEED_TURN_START;
    bool steeringFast = forwardAbs > HIGH_SPEED_TURN_START && turnAbs > (MOTOR_MAX * 45 / 100);
    bool slipRisk = false;
    uint8_t vibrationLeft = 0;
    uint8_t vibrationRight = 0;

    if (tractionSampleReady && highCommand) {
        float leftDelta = fabs(leftWheelSpeed - lastLeftWheelSpeed);
        float rightDelta = fabs(rightWheelSpeed - lastRightWheelSpeed);
        float wheelDelta = max(leftDelta, rightDelta);
        float wheelMismatch = fabs(leftWheelSpeed - rightWheelSpeed);
        float wheelPeak = max(leftWheelSpeed, rightWheelSpeed);
        int accelDelta = abs(accelStrength - lastAccelStrength);

        if (wheelDelta > 12.0f && (pivotingFast || steeringFast)) {
            slipRisk = true;
        }
        if (pivotingFast && wheelMismatch > 8.0f && wheelPeak > 12.0f) {
            slipRisk = true;
        }
        if (accelDelta > ACCEL_JOLT_THRESHOLD && (pivotingFast || steeringFast)) {
            slipRisk = true;
        }

        if (slipRisk) {
            int wheelFeedback = constrain(map(static_cast<int>(wheelDelta), 8, 35, 45, VIBRATION_MAX_SENSOR_STRENGTH),
                                          45, VIBRATION_MAX_SENSOR_STRENGTH);
            int accelFeedback = constrain(map(accelDelta, ACCEL_JOLT_THRESHOLD, ACCEL_JOLT_THRESHOLD * 3,
                                          45, VIBRATION_MAX_SENSOR_STRENGTH),
                                          45, VIBRATION_MAX_SENSOR_STRENGTH);
            uint8_t strength = static_cast<uint8_t>(max(wheelFeedback, accelFeedback));

            if (leftDelta > rightDelta + 3.0f || leftWheelSpeed > rightWheelSpeed + 8.0f) {
                vibrationLeft = strength;
                vibrationRight = strength / 2;
            } else if (rightDelta > leftDelta + 3.0f || rightWheelSpeed > leftWheelSpeed + 8.0f) {
                vibrationLeft = strength / 2;
                vibrationRight = strength;
            } else {
                vibrationLeft = strength;
                vibrationRight = strength;
            }
        }
    }

    lastLeftWheelSpeed = leftWheelSpeed;
    lastRightWheelSpeed = rightWheelSpeed;
    lastAccelStrength = accelStrength;
    tractionSampleReady = true;

    if (slipRisk) {
        startSensorVibration(vibrationLeft, vibrationRight, VIBRATION_PULSE_MS);
        tractionLimitPercent = TRACTION_RELEASE_PERCENT;
    } else if (tractionLimitPercent < 100) {
        tractionLimitPercent = min(100, tractionLimitPercent + TRACTION_RECOVER_STEP);
    }

    return tractionLimitPercent;
}

// Curvature drive: forward motion steers by slowing the inside wheel.
// This avoids sudden wheel reversal when changing direction while moving.
void controlCarWithSpeed(uint16_t x, uint16_t y) {
    int targetForward = map(y, 0, 255, -MOTOR_MAX, MOTOR_MAX);
    int targetTurn = map(x, 0, 255, -MOTOR_MAX, MOTOR_MAX);

    targetForward = applyDeadband(targetForward, 12);
    targetTurn = applyDeadband(targetTurn, 12);

    bool pivotCommand = abs(targetForward) <= 20 && abs(targetTurn) > HIGH_SPEED_TURN_START;
    filteredForward = rampToward(filteredForward, targetForward, 18);
    int turnStep = pivotCommand ? PIVOT_RAMP_STEP : (abs(filteredForward) > HIGH_SPEED_TURN_START ? 14 : 22);
    filteredTurn = rampToward(filteredTurn, targetTurn, turnStep);

    int leftTarget = 0;
    int rightTarget = 0;
    int forwardAbs = abs(filteredForward);
    int effectiveTurn = filteredTurn;

    if (forwardAbs > HIGH_SPEED_TURN_START) {
        int minTurnLimit = MOTOR_MAX * HIGH_SPEED_MIN_TURN_RATIO / 100;
        int maxTurn = map(forwardAbs, HIGH_SPEED_TURN_START, MOTOR_MAX, MOTOR_MAX, minTurnLimit);
        maxTurn = constrain(maxTurn, minTurnLimit, MOTOR_MAX);
        effectiveTurn = constrain(effectiveTurn, -maxTurn, maxTurn);
    }

    if (forwardAbs > 20) {
        int turnAbs = abs(effectiveTurn);
        int innerSpeed = map(turnAbs, 0, MOTOR_MAX, forwardAbs, 0);
        int outerSpeed = forwardAbs;

        if (forwardAbs > HIGH_SPEED_TURN_START) {
            int innerFloor = map(forwardAbs, HIGH_SPEED_TURN_START, MOTOR_MAX, 0,
                                 forwardAbs * HIGH_SPEED_INNER_MIN_RATIO / 100);
            innerSpeed = max(innerSpeed, innerFloor);
        }

        if (filteredForward > 0) {
            if (effectiveTurn > 0) {
                leftTarget = outerSpeed;
                rightTarget = innerSpeed;
            } else if (effectiveTurn < 0) {
                leftTarget = innerSpeed;
                rightTarget = outerSpeed;
            } else {
                leftTarget = outerSpeed;
                rightTarget = outerSpeed;
            }
        } else {
            if (effectiveTurn > 0) {
                leftTarget = -outerSpeed;
                rightTarget = -innerSpeed;
            } else if (effectiveTurn < 0) {
                leftTarget = -innerSpeed;
                rightTarget = -outerSpeed;
            } else {
                leftTarget = -outerSpeed;
                rightTarget = -outerSpeed;
            }
        }
    } else {
        int pivotLimit = abs(effectiveTurn) > HIGH_SPEED_TURN_START ? PIVOT_HIGH_SPEED_LIMIT : MOTOR_MAX;
        leftTarget = constrain(effectiveTurn, -pivotLimit, pivotLimit);
        rightTarget = -leftTarget;
    }

    int tractionLimit = updateTractionLimit(leftTarget, rightTarget, forwardAbs, abs(effectiveTurn));
    if (forwardAbs <= 20 && abs(effectiveTurn) > HIGH_SPEED_TURN_START) {
        tractionLimit = min(tractionLimit, PIVOT_TRACTION_PERCENT);
    }
    leftTarget = leftTarget * tractionLimit / 100;
    rightTarget = rightTarget * tractionLimit / 100;

    leftTarget = constrain(leftTarget, -MOTOR_MAX, MOTOR_MAX);
    rightTarget = constrain(rightTarget, -MOTOR_MAX, MOTOR_MAX);
    if (abs(leftTarget) < 5) leftTarget = 0;
    if (abs(rightTarget) < 5) rightTarget = 0;

    int outputStep = forwardAbs <= 20 && abs(effectiveTurn) > HIGH_SPEED_TURN_START ? PIVOT_RAMP_STEP : 24;
    int nextLeftSpeed = rampToward(currentLeftSpeed, leftTarget, outputStep);
    int nextRightSpeed = rampToward(currentRightSpeed, rightTarget, outputStep);

    if (abs(nextLeftSpeed - currentLeftSpeed) > 3 || (nextLeftSpeed == 0 && currentLeftSpeed != 0)) {
        driveMotor(maqueenPlus.LEFT, nextLeftSpeed);
        currentLeftSpeed = nextLeftSpeed;
    }
    if (abs(nextRightSpeed - currentRightSpeed) > 3 || (nextRightSpeed == 0 && currentRightSpeed != 0)) {
        driveMotor(maqueenPlus.RIGHT, nextRightSpeed);
        currentRightSpeed = nextRightSpeed;
    }
}

// ==================== OTA 无线更新功能 ====================

void scheduleRestart() {
  if (restartPending && millis() >= restartAtMs) {
    ESP.restart();
  }
}

void processOtaModeRequest() {
    if (!otaControlRequested || wifiMaintenanceStarted) return;

    otaControlRequested = false;
    lastGAMEPAD = "STOP";
    joyX = JOY_MID;
    joyY = JOY_MID;
    stopGamepadVibration();
    resetDriveState();
    maqueenPlus.motorStop(maqueenPlus.ALL);

    if (pClient != nullptr && pClient->isConnected()) {
        pClient->disconnect();
    }
    targetFound = false;
    reconnectFailCount = 0;

    startWifiMaintenance();
}

bool isOtaAuthorized() {
  return server.arg("token") == OTA_TOKEN;
}

String htmlEscape(const String& value) {
  String escaped = value;
  escaped.replace("&", "&amp;");
  escaped.replace("\"", "&quot;");
  escaped.replace("<", "&lt;");
  escaped.replace(">", "&gt;");
  return escaped;
}

void connectSta() {
    wifiPrefs.begin("wifi", true);
    staSsid = wifiPrefs.getString("ssid", DEFAULT_STA_SSID);
    staPass = wifiPrefs.getString("pass", DEFAULT_STA_PASS);
    wifiPrefs.end();

    Serial.printf("Connecting STA WiFi: %s\n", staSsid.c_str());
    WiFi.begin(staSsid.c_str(), staPass.c_str());
}

void loadPairCode() {
    wifiPrefs.begin("wifi", true);
    pairCode = wifiPrefs.getUInt("pair", PAIR_CODE);
    wifiPrefs.end();
}

void handleRoot() {
    String html = "<html><head><title>Maqueen WiFi</title>";
    html += "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">";
    html += "</head><body><h1>Maqueen K10</h1>";
    html += "<p>AP: ";
    html += OTA_SSID;
    html += " / ";
    html += WiFi.softAPIP().toString();
    html += "</p><p>STA: ";
    html += htmlEscape(staSsid);
    html += " / ";
    html += (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String("connecting"));
    html += "</p><form method=\"POST\" action=\"/wifi\">";
    html += "<p><label>SSID <input name=\"ssid\" value=\"";
    html += htmlEscape(staSsid);
    html += "\"></label></p><p><label>Password <input name=\"password\" type=\"password\" value=\"";
    html += htmlEscape(staPass);
    html += "\"></label></p><p><label>Pair Code <input name=\"pair\" inputmode=\"numeric\" pattern=\"[0-9]{5}\" maxlength=\"5\" value=\"";
    html += String(pairCode);
    html += "\"></label></p><button type=\"submit\">Save</button></form>";
    html += "<p>OTA: POST firmware to /ota?token=&lt;token&gt;</p></body></html>";
    server.send(200, "text/html", html);
}

void handleWifiSave() {
    if (server.hasArg("ssid")) {
        staSsid = server.arg("ssid");
        staPass = server.arg("password");
        wifiPrefs.begin("wifi", false);
        wifiPrefs.putString("ssid", staSsid);
        wifiPrefs.putString("pass", staPass);
        if (server.hasArg("pair")) {
            uint32_t newPairCode = server.arg("pair").toInt();
            if (newPairCode >= 10000 && newPairCode <= 99999) {
                pairCode = newPairCode;
                wifiPrefs.putUInt("pair", pairCode);
                targetFound = false;
                reconnectFailCount = 0;
                if (pClient != nullptr && pClient->isConnected()) {
                    pClient->disconnect();
                }
            }
        }
        wifiPrefs.end();
        connectSta();
    }
    server.sendHeader("Location", "/");
    server.send(303);
}

void startWifiMaintenance() {
    if (wifiMaintenanceStarted) return;
    wifiMaintenanceStarted = true;

    Serial.println("Starting WiFi AP+STA...");
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP(OTA_SSID, OTA_PASSWORD);
    connectSta();
    IPAddress IP = WiFi.softAPIP();
    Serial.printf("OTA service started\n");
    Serial.printf("Connect to WiFi: %s\n", OTA_SSID);
    Serial.println("Use the configured AP password and OTA token.");
    Serial.printf("Visit: http://%d.%d.%d.%d/ota?token=%s\n", IP[0], IP[1], IP[2], IP[3], OTA_TOKEN);

    // 设置 OTA HTTP 端点
    server.on("/ota", HTTP_POST, handleOta, handleOtaUpload);

    // 添加状态查询端点
    server.on("/status", HTTP_GET, []() {
        String json = "{\"status\":\"ready\",\"device\":\"Maqueen\",\"ap_ip\":\"";
        json += WiFi.softAPIP().toString();
        json += "\",\"sta_ip\":\"";
        json += (WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : String(""));
        json += "\",\"uploading\":";
        json += otaUploadActive ? "true" : "false";
        json += "}";
        server.send(200, "application/json", json);
    });

    // Add root path description
    server.on("/", HTTP_GET, handleRoot);
    server.on("/wifi", HTTP_POST, handleWifiSave);

    server.begin();
    Serial.println("HTTP server started.");
}

// OTA 上传完成处理
void handleOta() {
  if (!isOtaAuthorized()) {
    server.send(403, "text/plain", "FORBIDDEN");
    return;
  }
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", Update.hasError() ? "FAIL" : "OK");
  if (!Update.hasError()) {
    Serial.println("OTA update successful, restarting soon...");
    restartPending = true;
    restartAtMs = millis() + 1200;
  } else {
    Serial.println("OTA update failed!");
    otaUploadActive = false;
  }
}

// OTA upload data processing
void handleOtaUpload() {
  HTTPUpload &upload = server.upload();

  if (!isOtaAuthorized()) {
    if (upload.status == UPLOAD_FILE_START || upload.status == UPLOAD_FILE_WRITE) {
      Update.abort();
    }
    otaUploadActive = false;
    return;
  }

  if (upload.status == UPLOAD_FILE_START) {
    Serial.printf("Starting OTA upload: %s (%u bytes)\n", upload.filename.c_str(), upload.totalSize);
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      Update.printError(Serial);
    } else {
      otaUploadActive = true;
    }
  }
  else if (upload.status == UPLOAD_FILE_WRITE) {
    if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
      Update.printError(Serial);
    }
    // Print progress every 500ms
    static unsigned long lastProgress = 0;
    if (millis() - lastProgress > 500 || upload.currentSize == upload.totalSize) {
      Serial.printf("OTA progress: %u / %u bytes\n", upload.totalSize, upload.currentSize);
      lastProgress = millis();
    }
  }
  else if (upload.status == UPLOAD_FILE_END) {
    if (Update.end(true)) {
      Serial.printf("OTA upload successful: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
    otaUploadActive = false;
  }
  else if (upload.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
    otaUploadActive = false;
  }
}
