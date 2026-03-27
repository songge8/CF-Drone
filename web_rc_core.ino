// web_rc_core.ino
// Web RC 核心模块 - 基础版（无气压定高功能）

#if WEB_RC_ENABLED

#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include "web_rc_global.h"
#include "web_rc_html.h"

// 使用与wifi.ino相同的SSID和密码
extern const char* WIFI_SSID;
extern const char* WIFI_PASSWORD;

const char* WEB_RC_SSID = WIFI_SSID;
const char* WEB_RC_PASSWORD = WIFI_PASSWORD;

// Web服务器实例 - 使用8080端口避免冲突
WebServer webRCServer(8080);

// ==================== 性能优化常量 ====================
#define COMMAND_JOYSTICK 0x01    // 使用单字节命令标识
#define COMMAND_BUTTON   0x02
#define COMMAND_PARAMS   0x03
#define COMMAND_HEARTBEAT 0x04
#define COMMAND_RESET    0x06

#define PROTOCOL_BINARY  0x01    // 二进制协议标识
#define PROTOCOL_JSON    0x02    // JSON协议标识

#define MAX_PACKET_SIZE  64      // 限制包大小

// P0修复: 网络超时和心跳配置
#define WEB_RC_TIMEOUT_MS      10000  // 连接超时阈值 (10秒，适应高延迟)
#define WEB_RC_HEARTBEAT_MS    5000   // 心跳间隔 (5秒，小于超时阈值一半)

// ==================== 代码配置常量（替代前端参数调节面板）====================
#define CONFIG_THROTTLE_SCALE   1.0f    // 油门灵敏度系数（0.0~1.0）
#define CONFIG_STICK_SCALE      0.85f   // 摇杆灵敏度系数（0.0~1.0）
#define CONFIG_YAW_SCALE        0.68f   // 偏航灵敏度系数（0.0~1.0）
#define CONFIG_STICK_DEADZONE   0.10f   // 摇杆死区（相对0~1）
#define CONFIG_THROTTLE_DEADZONE 0.15f  // 油门死区（相对0~1）

// ==================== 电池电压 ADC ====================
#define VBAT_ADC_PIN     36             // GPIO36 (VP) 通过分压采集电池电压
#define VBAT_ADC_SAMPLES 16             // 多次采样取均值降低噪声
#define VBAT_DIVIDER     (43.0f / 33.0f)// 分压比: 10kΩ+33kΩ, Vbat=(Vadc*43/33)

// ==================== 全局变量定义 ====================
bool webRCEnabled = false;
bool useWebRC = false;
bool webRCUpdated = false;
float webRCRoll = 0.0f, webRCPitch = 0.0f, webRCYaw = 0.0f, webRCThrottle = 0.0f;
uint16_t webRCButtons = 0;
unsigned long webRCLastUpdate = 0;

// Web遥控器参数
float webRCThrottleScale = 1.0f;
float webRCStickScale = 1.0f;
float webRCYawScale = 1.0f;

// ==================== 摇杆校准参数 ====================
// 重新校准的摇杆映射：
// 左摇杆Y轴 → 油门 (Throttle) [0~100%]，最低位置为0，最高位置为100%
// 左摇杆X轴 → 偏航 (Yaw) [-30~+30]，中心为0，左为负，右为正
// 右摇杆Y轴 → 俯仰 (Pitch) [-30~+30]，中心为0，上为负，下为正
// 右摇杆X轴 → 横滚 (Roll) [-30~+30]，中心为0，左为负，右为正

// 死区设置 - 减少死区到10%避免操作迟钝
float stickDeadzone = 0.10f;      // 10%死区（所有摇杆）
float throttleDeadzone = 0.15f;   // 15%死区（油门）- 只在低端

// 摇杆校准范围
const float THROTTLE_MIN = 0.0f;     // 油门最低值
const float THROTTLE_MAX = 100.0f;   // 油门最高值
const float STICK_MIN = -30.0f;      // 摇杆最小值
const float STICK_MAX = 30.0f;       // 摇杆最大值
const float STICK_CENTER = 0.0f;     // 摇杆中心值

// 原始输入范围（前端发送的范围）
const float RAW_STICK_MIN = -100.0f;  // 前端发送的最小值
const float RAW_STICK_MAX = 100.0f;   // 前端发送的最大值

// 缩放因子：将±100范围缩放到±30范围
const float STICK_SCALE_FACTOR = 30.0f / 100.0f;  // 0.3

// 上次有效值
float lastValidThrottle = 0.0f;
float lastValidRoll = 0.0f;
float lastValidPitch = 0.0f;
float lastValidYaw = 0.0f;

// 异常检测
bool dataErrorDetected = false;
unsigned long lastDataErrorTime = 0;

// 补充缺失的变量
float throttleExpo = 0.2f;

// ====================== 内部使用的优化变量 ======================
struct OptimizedRCData {
    int16_t roll;
    int16_t pitch;
    int16_t yaw;
    int16_t throttle;
    uint16_t buttons;
    uint32_t timestamp;
    bool initialized;
    uint8_t seq;  // 序列号用于增量更新
};

OptimizedRCData internalData = {0, 0, 0, 0, 0, 0, false, 0};

// 增量更新数据结构
struct DeltaData {
    int8_t deltaRoll;
    int8_t deltaPitch;
    int8_t deltaYaw;
    int8_t deltaThrottle;
    uint8_t changedFlags;  // 位掩码表示哪些字段变化
};

DeltaData lastDelta = {0, 0, 0, 0, 0};

// 数据验证标志
bool dataValidationEnabled = true;

// 性能统计
uint32_t totalBytesReceived = 0;
uint32_t totalPacketsReceived = 0;
uint32_t jsonPackets = 0;
uint32_t binaryPackets = 0;

// ====================== 网络质量监控类 ======================
class EnhancedNetworkMonitor {
private:
    unsigned long lastReceived;
    uint16_t packetLoss;
    uint32_t packetCount;
    uint16_t avgLatency;
    uint32_t bytesPerSecond;
    unsigned long lastByteCountTime;
    uint32_t lastByteCount;
    
public:
    EnhancedNetworkMonitor() : 
        packetLoss(0), packetCount(0), avgLatency(0), bytesPerSecond(0) {
        lastReceived = millis();
        lastByteCountTime = millis();
        lastByteCount = 0;
    }
    
    void update(unsigned long jsTimestamp, uint16_t bytes = 0) {
        unsigned long now = millis();
        
        if (now - lastReceived > 150) {
            packetLoss++;
        }
        
        if (jsTimestamp > 0) {
            uint16_t latency = (uint16_t)(now - jsTimestamp);
            avgLatency = (avgLatency * 9 + latency) / 10;
        }
        
        // 计算字节率
        if (bytes > 0) {
            totalBytesReceived += bytes;
            if (now - lastByteCountTime >= 1000) {
                bytesPerSecond = totalBytesReceived - lastByteCount;
                lastByteCount = totalBytesReceived;
                lastByteCountTime = now;
            }
        }
        
        lastReceived = now;
        packetCount++;
    }
    
    uint16_t getLatency() const { return avgLatency; }
    
    uint16_t getPacketLossRate() const { 
        if (packetCount == 0) return 0;
        return (packetLoss * 1000) / packetCount;
    }
    
    uint32_t getPacketCount() const { return packetCount; }
    uint32_t getBytesPerSecond() const { return bytesPerSecond; }
    unsigned long getLastReceived() const { return lastReceived; }
    
    void reset() {
        packetLoss = 0;
        packetCount = 0;
        avgLatency = 0;
        bytesPerSecond = 0;
        lastReceived = millis();
    }
};

// ====================== 创建全局实例 ======================
EnhancedNetworkMonitor netMonitor;

// ====================== 内部函数声明 ======================
void handleWebRCClient();
void checkWebRCConnectionStatus();
void logWebRCNetworkStatus();
bool handleBinaryProtocol(uint8_t* data, size_t len);
bool handleJSONProtocol(String& body);
void handleJoystickCommandBinary(uint8_t* data, size_t len);
void handleDeltaCommand(uint8_t* data, size_t len);
void processJoystickData(float throttle, float roll, float pitch, float yaw, uint32_t timestamp);
void handleButtonCommandBinary(uint8_t* data, size_t len);
void sendBinaryResponse(uint8_t command, bool success, uint16_t latency);

// 工具函数声明
int16_t floatToInt16(float value);
float int16ToFloat(int16_t value);
bool validateWebRCData(float& throttle, float& roll, float& pitch, float& yaw);
const char* findJsonValue(const char* json, const char* key);

// ==================== 重新校准的摇杆处理函数 ====================
// 应用死区 - 10%死区避免干扰
float applyDeadzone(float value, float deadzone, float center = 0.0f) {
    float distance = fabs(value - center);
    if (distance < deadzone) {
        return center;  // 在死区内，返回中心值
    }
    
    // 应用死区缩放
    if (value > center) {
        return center + (value - center - deadzone) / (1.0f - deadzone);
    } else {
        return center - (center - value - deadzone) / (1.0f - deadzone);
    }
}

// 缩放函数：将±100范围缩放到±30范围
float scaleStickValue(float rawValue) {
    // 首先确保在±100范围内
    rawValue = constrain(rawValue, RAW_STICK_MIN, RAW_STICK_MAX);
    
    // 应用缩放因子（±100 → ±30）
    return rawValue * STICK_SCALE_FACTOR;
}

// 处理摇杆值（修复油门处理 + 正确的死区应用）
float processStickValue(float rawValue, float& lastValid, const char* axisName, bool isThrottle = false) {
    // 第一步：检查是否为NaN或无穷大
    if (isnan(rawValue) || isinf(rawValue)) {
        if (millis() - lastDataErrorTime > 2000) {
            print("⚠️ %s值异常: NaN/Inf，使用上次有效值 %.1f\n", axisName, lastValid);
            lastDataErrorTime = millis();
        }
        return lastValid;
    }
    
    // 第二步：检查是否为JSON解析错误（极大值）- 修复这里！
    if (fabs(rawValue) > 1000.0f) {  // 降低阈值从10000到1000
        if (millis() - lastDataErrorTime > 2000) {
            print("⚠️ %s JSON解析错误: %.1f，重置为0\n", axisName, rawValue);
            lastDataErrorTime = millis();
        }
        if (isThrottle) {
            lastValid = THROTTLE_MIN;  // 油门归零
            return THROTTLE_MIN;
        } else {
            lastValid = STICK_CENTER;  // 摇杆归中
            return STICK_CENTER;
        }
    }
    
    // 第三步：根据摇杆类型应用不同的处理
    float processedValue;
    
    if (isThrottle) {
        // ================ 修复：油门处理 ================
        // 处理前端发送的±100范围，正确转换到0~100%
        // 但也要处理已经是0~100范围的情况
        
        // 先限制范围
        rawValue = constrain(rawValue, RAW_STICK_MIN, RAW_STICK_MAX);
        
        // 调试输出原始值
        static unsigned long lastDebug = 0;
        static float lastRawDebug = 0;
        if (fabs(rawValue - lastRawDebug) > 5.0f && millis() - lastDebug > 1000) {
            print("油门原始值: %.1f (范围: -100~+100)\n", rawValue);
            lastRawDebug = rawValue;
            lastDebug = millis();
        }
        
        // 方案1：如果是0~100范围，直接使用
        if (rawValue >= 0.0f && rawValue <= 100.0f) {
            processedValue = rawValue;
        }
        // 方案2：如果是±100范围，转换到0~100
        else if (rawValue >= -100.0f && rawValue <= 100.0f) {
            // 转换公式：-100 → 0, 0 → 50, +100 → 100
            processedValue = (rawValue + 100.0f) / 2.0f;
        }
        // 方案3：其他情况，安全处理
        else {
            processedValue = THROTTLE_MIN;
        }
        
        // ================ 修复：油门死区处理 ================
        // 只在低端应用死区：如果油门值小于15%，则视为0
        if (processedValue < throttleDeadzone * 100.0f) {
            processedValue = THROTTLE_MIN;
        }
        
        // 限制范围到0~100%
        processedValue = constrain(processedValue, THROTTLE_MIN, THROTTLE_MAX);
        
    } else {
        // ================ 修复：其他摇杆处理 ================
        // 确保输入在合理范围内
        rawValue = constrain(rawValue, RAW_STICK_MIN, RAW_STICK_MAX);
        
        // 缩放到±30范围
        float scaledValue = rawValue * STICK_SCALE_FACTOR;
        
        // 应用死区（以0为中心）
        processedValue = applyDeadzone(scaledValue, stickDeadzone * 30.0f, STICK_CENTER);
        
        // 限制到±30范围
        processedValue = constrain(processedValue, STICK_MIN, STICK_MAX);
        
        // 调试输出
        static unsigned long lastStickDebug = 0;
        if (millis() - lastStickDebug > 2000) {
            print("%s处理: 原始=%.1f, 缩放=%.1f, 处理后=%.1f\n", 
                  axisName, rawValue, scaledValue, processedValue);
            lastStickDebug = millis();
        }
    }
    
    // 保存有效值
    lastValid = processedValue;
    
    return processedValue;
}

// 处理横滚（右摇杆X轴）- 简化函数
float processRoll(float rawRoll) {
    float result = processStickValue(rawRoll, lastValidRoll, "横滚", false);
    return result;
}

// 处理俯仰（右摇杆Y轴）
float processPitch(float rawPitch) {
    // 注意：前端通常上推为负值，下拉为正值
    float result = processStickValue(rawPitch, lastValidPitch, "俯仰", false);
    return result;
}

// 处理偏航（左摇杆X轴）
float processYaw(float rawYaw) {
    float result = processStickValue(rawYaw, lastValidYaw, "偏航", false);
    return result;
}

// 处理油门（左摇杆Y轴）- 应用死区后乘以功率限制系数
float processThrottle(float rawThrottle) {
    float result = processStickValue(rawThrottle, lastValidThrottle, "油门", true);
    
    // 应用功率限制（webRCThrottleScale: 0~1，默认1.0即无限制）
    result = constrain(result * webRCThrottleScale, THROTTLE_MIN, THROTTLE_MAX);
    
    // 调试输出
    static unsigned long lastThrottleDebug = 0;
    static float lastThrottleValue = 0;
    if (fabs(result - lastThrottleValue) > 5.0f && millis() - lastThrottleDebug > 1000) {
        print("油门处理完成: 原始=%.1f, 限制系数=%.2f, 结果=%.1f%%\n", rawThrottle, webRCThrottleScale, result);
        lastThrottleValue = result;
        lastThrottleDebug = millis();
    }
    
    return result;
}

// ====================== 数据转换工具函数 ======================
int16_t floatToInt16(float value) {
    return (int16_t)(value * 100.0f);
}

float int16ToFloat(int16_t value) {
    return value / 100.0f;
}

bool validateWebRCData(float& throttle, float& roll, float& pitch, float& yaw) {
    bool valid = true;
    
    // 更严格的JSON解析错误检查
    if (fabs(roll) > 1000.0f || fabs(pitch) > 1000.0f || fabs(yaw) > 1000.0f) {
        print("⚠️ JSON解析错误检测到: R=%.1f P=%.1f Y=%.1f\n", roll, pitch, yaw);
        roll = lastValidRoll;
        pitch = lastValidPitch;
        yaw = lastValidYaw;
        valid = false;
    }
    
    // 检查NaN和无穷大
    if (isnan(throttle) || isinf(throttle)) {
        throttle = lastValidThrottle;
        valid = false;
    }
    if (isnan(roll) || isinf(roll)) {
        roll = lastValidRoll;
        valid = false;
    }
    if (isnan(pitch) || isinf(pitch)) {
        pitch = lastValidPitch;
        valid = false;
    }
    if (isnan(yaw) || isinf(yaw)) {
        yaw = lastValidYaw;
        valid = false;
    }
    
    // 记录错误
    if (!valid && millis() - lastDataErrorTime > 2000) {
        print("⚠️ 数据验证失败: T=%.1f R=%.1f P=%.1f Y=%.1f\n", 
              throttle, roll, pitch, yaw);
        lastDataErrorTime = millis();
    }
    
    return valid;
}

// ====================== 快速JSON解析函数 ======================
const char* findJsonValue(const char* json, const char* key) {
    const char* keyPos = strstr(json, key);
    if (!keyPos) return nullptr;
    
    const char* colonPos = strchr(keyPos, ':');
    if (!colonPos) return nullptr;
    
    const char* valueStart = colonPos + 1;
    while (*valueStart == ' ' || *valueStart == '\t' || *valueStart == '\"') valueStart++;
    
    return valueStart;
}

// ==================== 电池电压读取 ====================
float readBatteryVoltage() {
    long sum = 0;
    for (int i = 0; i < VBAT_ADC_SAMPLES; i++) {
        sum += analogRead(VBAT_ADC_PIN);
    }
    float vADC = (sum / (float)VBAT_ADC_SAMPLES / 4095.0f) * 3.3f;
    return vADC * VBAT_DIVIDER;
}

// ==================== 控制台日志缓冲区 ====================
#define CONSOLE_LINES   30
#define CONSOLE_LINE_LEN 80
static char consoleBuf[CONSOLE_LINES][CONSOLE_LINE_LEN];
static int  consoleTail = 0;   // 下一条写入的位置（环形）
static int  consoleFilled = 0; // 已填充行数（最大CONSOLE_LINES）

void webLog(const char* msg) {
    strncpy(consoleBuf[consoleTail], msg, CONSOLE_LINE_LEN - 1);
    consoleBuf[consoleTail][CONSOLE_LINE_LEN - 1] = '\0';
    consoleTail = (consoleTail + 1) % CONSOLE_LINES;
    if (consoleFilled < CONSOLE_LINES) consoleFilled++;
}

// ====================== 核心功能函数 ======================
void setWebRCInput(float roll, float pitch, float yaw, float throttle, uint16_t buttons) {
    // 首先进行基本验证 - 修复：在验证前先约束范围
    roll = constrain(roll, -1000.0f, 1000.0f);
    pitch = constrain(pitch, -1000.0f, 1000.0f);
    yaw = constrain(yaw, -1000.0f, 1000.0f);
    throttle = constrain(throttle, -1000.0f, 1000.0f);
    
    if (!validateWebRCData(throttle, roll, pitch, yaw)) {
        dataErrorDetected = true;
        // 即使验证失败，也尝试处理，但使用上次有效值
    }
    
    // ==================== 处理四个轴的数据 ====================
    float processedThrottle = processThrottle(throttle);  // 左摇杆Y轴 → 0~100%
    float processedYaw = processYaw(yaw);                // 左摇杆X轴 → -30~+30
    float processedPitch = processPitch(pitch);          // 右摇杆Y轴 → -30~+30
    float processedRoll = processRoll(roll);             // 右摇杆X轴 → -30~+30
    
    // 存储处理后的值
    webRCRoll = processedRoll;
    webRCPitch = processedPitch;
    webRCYaw = processedYaw;
    webRCThrottle = processedThrottle;
    webRCButtons = buttons;
    webRCLastUpdate = millis();
    webRCUpdated = true;
    
    internalData.roll = floatToInt16(processedRoll);
    internalData.pitch = floatToInt16(processedPitch);
    internalData.yaw = floatToInt16(processedYaw);
    internalData.throttle = floatToInt16(processedThrottle);
    internalData.buttons = buttons;
    internalData.timestamp = millis();
    internalData.initialized = true;
    
    // 调试输出 - 修复输出逻辑
    static unsigned long lastPrint = 0;
    static uint8_t printCount = 0;
    static float lastPrintedValues[4] = {0, 0, 0, 0};
    static const char* axisNames[4] = {"R", "P", "Y", "T"};
    
    printCount++;
    
    // 检查是否有显著变化
    float changes[4] = {
        fabs(processedRoll - lastPrintedValues[0]),
        fabs(processedPitch - lastPrintedValues[1]),
        fabs(processedYaw - lastPrintedValues[2]),
        fabs(processedThrottle - lastPrintedValues[3])
    };
    
    bool shouldPrint = false;
    
    // 条件1：超过200ms且变化大于阈值
    if (millis() - lastPrint > 200) {
        shouldPrint = true;
    }
    // 条件2：任何轴变化超过5个单位
    for (int i = 0; i < 4; i++) {
        if (changes[i] > 5.0f) {
            shouldPrint = true;
            break;
        }
    }
    // 条件3：每20次更新输出一次（防止过于频繁）
    if (printCount >= 20) {
        shouldPrint = true;
    }
    
    if (shouldPrint) {
        print("✓ Web RC: T=%.1f%% R=%.1f P=%.1f Y=%.1f | Buttons: 0x%04X\n",
              processedThrottle, processedRoll, processedPitch, processedYaw, buttons);
        
        lastPrintedValues[0] = processedRoll;
        lastPrintedValues[1] = processedPitch;
        lastPrintedValues[2] = processedYaw;
        lastPrintedValues[3] = processedThrottle;
        lastPrint = millis();
        printCount = 0;
    }
}

void processJoystickData(float throttle, float roll, float pitch, float yaw, uint32_t timestamp) {
    // 应用基础限制 - 修复：先约束到合理范围
    throttle = constrain(throttle, -200.0f, 200.0f);
    roll = constrain(roll, -200.0f, 200.0f);
    pitch = constrain(pitch, -200.0f, 200.0f);
    yaw = constrain(yaw, -200.0f, 200.0f);
    
    // 调试输出原始数据
    static unsigned long lastProcessDebug = 0;
    static float lastValues[4] = {0, 0, 0, 0};
    
    float changes[4] = {
        fabs(throttle - lastValues[0]),
        fabs(roll - lastValues[1]),
        fabs(pitch - lastValues[2]),
        fabs(yaw - lastValues[3])
    };
    
    bool shouldDebug = false;
    for (int i = 0; i < 4; i++) {
        if (changes[i] > 50.0f) {  // 变化超过50才输出调试
            shouldDebug = true;
            break;
        }
    }
    
    if (shouldDebug && millis() - lastProcessDebug > 1000) {
        print("处理前数据: T=%.1f R=%.1f P=%.1f Y=%.1f\n", 
              throttle, roll, pitch, yaw);
        lastValues[0] = throttle;
        lastValues[1] = roll;
        lastValues[2] = pitch;
        lastValues[3] = yaw;
        lastProcessDebug = millis();
    }
    
    setWebRCInput(roll, pitch, yaw, throttle, webRCButtons);
    netMonitor.update(timestamp);
}

bool isWebRCEnabled() {
    unsigned long now = millis();
    return webRCUpdated && (now - webRCLastUpdate < 10000);  // P0修复: 2s→10s 适应高延迟网络
}

bool isUsingWebRC() {
    return useWebRC && isWebRCEnabled();
}

// ====================== 二进制协议处理器 ======================
bool handleBinaryProtocol(uint8_t* data, size_t len) {
    if (len < 2) return false;  // 至少需要协议标识和命令
    
    uint8_t protocol = data[0];
    uint8_t command = data[1];
    
    if (protocol != PROTOCOL_BINARY) return false;
    
    totalPacketsReceived++;
    binaryPackets++;
    
    switch(command) {
        case COMMAND_JOYSTICK:
            handleJoystickCommandBinary(data, len);
            break;
        case COMMAND_BUTTON:
            handleButtonCommandBinary(data, len);
            break;
        case COMMAND_HEARTBEAT:
            netMonitor.update(0, len);
            webRCLastUpdate = millis();
            webRCUpdated = true;
            sendBinaryResponse(command, true, netMonitor.getLatency());
            break;
        case 0x80:  // Delta命令 (0x80 = 128)
            handleDeltaCommand(data, len);
            break;
        default:
            print("未知二进制命令: 0x%02X\n", command);
            sendBinaryResponse(command, false, 0);
            return false;
    }
    
    return true;
}

void handleJoystickCommandBinary(uint8_t* data, size_t len) {
    // 协议格式: [协议标识][命令][油门][滚转][俯仰][偏航][时间戳]
    if (len >= 14) {  // 1+1+2+2+2+2+4=14字节
        int16_t throttle = (data[2] << 8) | data[3];
        int16_t roll = (data[4] << 8) | data[5];
        int16_t pitch = (data[6] << 8) | data[7];
        int16_t yaw = (data[8] << 8) | data[9];
        uint32_t timestamp = (data[10] << 24) | (data[11] << 16) | (data[12] << 8) | data[13];
        
        float fThrottle = int16ToFloat(throttle);
        float fRoll = int16ToFloat(roll);
        float fPitch = int16ToFloat(pitch);
        float fYaw = int16ToFloat(yaw);
        
        // 记录原始值用于调试
        static unsigned long lastRawDebug = 0;
        if (millis() - lastRawDebug > 2000) {
            print("Web RC二进制原始值: T=%d(%.1f) R=%d(%.1f) P=%d(%.1f) Y=%d(%.1f)\n", 
                  throttle, fThrottle, roll, fRoll, pitch, fPitch, yaw, fYaw);
            lastRawDebug = millis();
        }
        
        processJoystickData(fThrottle, fRoll, fPitch, fYaw, timestamp);
        sendBinaryResponse(COMMAND_JOYSTICK, true, netMonitor.getLatency());
    }
}

void handleDeltaCommand(uint8_t* data, size_t len) {
    // Delta协议格式: [协议标识][命令][变化标志][增量数据...][时间戳]
    if (len >= 8) {
        uint8_t changedFlags = data[2];
        uint32_t timestamp = (data[len-4] << 24) | (data[len-3] << 16) | (data[len-2] << 8) | data[len-1];
        
        float newThrottle = webRCThrottle;
        float newRoll = webRCRoll;
        float newPitch = webRCPitch;
        float newYaw = webRCYaw;
        
        int dataIndex = 3;
        
        if (changedFlags & 0x01) { // 油门变化
            int8_t delta = (int8_t)data[dataIndex++];
            newThrottle += delta * 0.5f; // 缩小增量范围
        }
        if (changedFlags & 0x02) { // 滚转变化
            int8_t delta = (int8_t)data[dataIndex++];
            newRoll += delta * 0.5f;
        }
        if (changedFlags & 0x04) { // 俯仰变化
            int8_t delta = (int8_t)data[dataIndex++];
            newPitch += delta * 0.5f;
        }
        if (changedFlags & 0x08) { // 偏航变化
            int8_t delta = (int8_t)data[dataIndex++];
            newYaw += delta * 0.5f;
        }
        
        processJoystickData(newThrottle, newRoll, newPitch, newYaw, timestamp);
        sendBinaryResponse(0x80, true, netMonitor.getLatency());
    }
}

void handleButtonCommandBinary(uint8_t* data, size_t len) {
    if (len >= 8) {  // 1+1+1+1+4=8字节
        uint8_t buttonIndex = data[2];
        uint8_t state = data[3];
        uint32_t timestamp = (data[4] << 24) | (data[5] << 16) | (data[6] << 8) | data[7];
        
        netMonitor.update(timestamp, len);
        
        if (buttonIndex < 16) {
            if (state == 1) {
                webRCButtons |= (1 << buttonIndex);
            } else {
                webRCButtons &= ~(1 << buttonIndex);
            }
            
            webRCLastUpdate = millis();
            webRCUpdated = true;
        }
        
        sendBinaryResponse(COMMAND_BUTTON, true, netMonitor.getLatency());
    }
}

void sendBinaryResponse(uint8_t command, bool success, uint16_t latency) {
    char response[16];
    uint8_t index = 0;
    
    response[index++] = PROTOCOL_BINARY;
    response[index++] = command;
    response[index++] = success ? 0x01 : 0x00;
    
    // 添加延迟数据
    response[index++] = (latency >> 8) & 0xFF;
    response[index++] = latency & 0xFF;
    
    // 添加性能统计
    uint16_t packetLoss = netMonitor.getPacketLossRate();
    response[index++] = (packetLoss >> 8) & 0xFF;
    response[index++] = packetLoss & 0xFF;
    
    // 添加当前数据
    int16_t throttle = floatToInt16(webRCThrottle);
    response[index++] = (throttle >> 8) & 0xFF;
    response[index++] = throttle & 0xFF;
    
    uint32_t timestamp = millis();
    for (int i = 0; i < 4; i++) {
        response[index++] = (timestamp >> (24 - i*8)) & 0xFF;
    }
    
    // 创建String对象发送
    String responseStr;
    responseStr.reserve(index);
    for (uint8_t i = 0; i < index; i++) {
        responseStr += response[i];
    }
    
    webRCServer.send(200, "application/octet-stream", responseStr);
}

// ====================== JSON协议处理器 ======================
bool handleJSONProtocol(String& body) {
    if (body.length() == 0 || body.indexOf('{') == -1) return false;
    
    totalPacketsReceived++;
    jsonPackets++;
    
    const char* json = body.c_str();
    
    // 快速判断命令类型
    const char* typePos = strstr(json, "\"t\":");
    if (!typePos) return false;
    
    int type = atoi(typePos + 4);
    
    switch(type) {
        case 1: { // 摇杆命令
            float throttle = 0, roll = 0, pitch = 0, yaw = 0;
            uint32_t timestamp = 0;
            
            const char* val;
            if ((val = findJsonValue(json, "\"th\""))) throttle = atof(val);
            if ((val = findJsonValue(json, "\"r\""))) roll = atof(val);
            if ((val = findJsonValue(json, "\"p\""))) pitch = atof(val);
            if ((val = findJsonValue(json, "\"y\""))) yaw = atof(val);
            if ((val = findJsonValue(json, "\"ts\""))) timestamp = atol(val);
            
            // 记录JSON原始值（特别是异常值）
            static unsigned long lastJsonDebug = 0;
            static float lastValues[4] = {0, 0, 0, 0};
            
            float changes[4] = {
                fabs(throttle - lastValues[0]),
                fabs(roll - lastValues[1]),
                fabs(pitch - lastValues[2]),
                fabs(yaw - lastValues[3])
            };
            
            bool shouldDebug = false;
            for (int i = 0; i < 4; i++) {
                if (changes[i] > 50.0f) {  // 显著变化才输出
                    shouldDebug = true;
                    break;
                }
            }
            
            if (shouldDebug && millis() - lastJsonDebug > 1000) {
                print("Web RC JSON原始值: T=%.1f R=%.1f P=%.1f Y=%.1f\n", 
                      throttle, roll, pitch, yaw);
                lastValues[0] = throttle;
                lastValues[1] = roll;
                lastValues[2] = pitch;
                lastValues[3] = yaw;
                lastJsonDebug = millis();
            }
            
            processJoystickData(throttle, roll, pitch, yaw, timestamp);
            break;
        }
        case 2: { // 按钮命令
            int buttonIndex = 0, state = 0;
            uint32_t timestamp = 0;
            
            const char* val;
            if ((val = findJsonValue(json, "\"b\""))) buttonIndex = atoi(val);
            if ((val = findJsonValue(json, "\"s\""))) state = atoi(val);
            if ((val = findJsonValue(json, "\"ts\""))) timestamp = atol(val);
            
            netMonitor.update(timestamp, body.length());
            
            if (buttonIndex >= 0 && buttonIndex < 16) {
                if (state == 1) {
                    webRCButtons |= (1 << buttonIndex);
                } else {
                    webRCButtons &= ~(1 << buttonIndex);
                }
                webRCLastUpdate = millis();
                webRCUpdated = true;
            }
            break;
        }
        case 5: // 参数命令已移除，由代码中的CONFIG_常量替代
            break;
        case 4: // 心跳
            netMonitor.update(0, body.length());
            webRCLastUpdate = millis();
            webRCUpdated = true;
            break;
    }
    
    return true;
}

// ====================== Web服务器处理器 ======================
void handleWebRCRequest() {
    String contentType = webRCServer.header("Content-Type");
    
    // 检查是否是二进制协议
    if (contentType == "application/octet-stream" || contentType == "application/cbor") {
        String body = webRCServer.arg("plain");
        if (body.length() > 0) {
            uint8_t* data = (uint8_t*)body.c_str();
            if (handleBinaryProtocol(data, body.length())) {
                return; // 二进制响应已发送
            }
        }
    }
    
    // 回退到JSON处理
    if (webRCServer.hasArg("plain")) {
        String body = webRCServer.arg("plain");
        
        if (body.length() == 0) {
            webRCServer.send(400, "application/json", "{\"e\":\"no data\"}");
            return;
        }
        
        // 尝试二进制协议 (检查首字节)
        if (body.length() > 1) {
            uint8_t firstByte = body[0];
            if (firstByte == PROTOCOL_BINARY) {
                if (handleBinaryProtocol((uint8_t*)body.c_str(), body.length())) {
                    return;
                }
            }
        }
        
        // 处理JSON协议
        if (handleJSONProtocol(body)) {
            // 构建精简JSON响应（含当前飞控模式和解锁状态，供前端状态栏更新）
            char jsonResp[128];
            snprintf(jsonResp, sizeof(jsonResp),
                "{\"s\":\"ok\",\"l\":%u,\"pl\":%.1f,\"m\":%d,\"arm\":%d}",
                netMonitor.getLatency(),
                netMonitor.getPacketLossRate() / 10.0,
                mode,
                (int)armed
            );
            
            webRCServer.send(200, "application/json", jsonResp);
        } else {
            webRCServer.send(400, "application/json", "{\"e\":\"parse failed\"}");
        }
    } else {
        webRCServer.send(400, "application/json", "{\"e\":\"no data\"}");
    }
}

// ====================== 调度任务函数 ======================
void handleWebRCClient() {
    webRCServer.handleClient();
}

void checkWebRCConnectionStatus() {
    if (isWebRCEnabled()) {
        webRCEnabled = true;
        useWebRC = true;
    } else {
        webRCEnabled = false;
        useWebRC = false;
    }
}

void logWebRCNetworkStatus() {
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 30000) {
        print("网络状态: 延迟=%ums 丢包=%.1f%% 数据包=%lu | ",
              netMonitor.getLatency(),
              netMonitor.getPacketLossRate() / 10.0,
              netMonitor.getPacketCount());
        
        print("协议统计: 二进制=%lu JSON=%lu 总字节=%lu B/s=%lu | ",
              binaryPackets, jsonPackets, totalBytesReceived, netMonitor.getBytesPerSecond());
        
        print("Web RC数据: T=%.1f%% R=%.1f P=%.1f Y=%.1f\n",
              webRCThrottle, webRCRoll, webRCPitch, webRCYaw);
        
        lastLog = millis();
    }
}

// ====================== 主设置函数 ======================
void setupWebRC() {
    print("==========================================\n");
    print("  Web遥控器初始化 - 基础版（无气压定高）\n");
    print("  修复横滚锁定和油门问题版\n");
    print("==========================================\n");
    
    // 使用CONFIG_常量初始化参数（代替前端参数面板）
    webRCThrottleScale = CONFIG_THROTTLE_SCALE;
    webRCStickScale    = CONFIG_STICK_SCALE;
    webRCYawScale      = CONFIG_YAW_SCALE;
    stickDeadzone      = CONFIG_STICK_DEADZONE;
    throttleDeadzone   = CONFIG_THROTTLE_DEADZONE;
    
    // 初始化上次有效值
    lastValidThrottle = THROTTLE_MIN;  // 油门从0开始
    lastValidRoll = STICK_CENTER;      // 其他摇杆从0开始
    lastValidPitch = STICK_CENTER;
    lastValidYaw = STICK_CENTER;
    
    print("  摇杆映射（±30范围限制）:\n");
    print("  左摇杆Y轴 → 油门 (Throttle) [%.0f~%.0f%%]\n", THROTTLE_MIN, THROTTLE_MAX);
    print("  左摇杆X轴 → 偏航 (Yaw) [%.0f~+%.0f]\n", STICK_MIN, STICK_MAX);
    print("  右摇杆Y轴 → 俯仰 (Pitch) [%.0f~+%.0f]\n", STICK_MIN, STICK_MAX);
    print("  右摇杆X轴 → 横滚 (Roll) [%.0f~+%.0f]\n", STICK_MIN, STICK_MAX);
    print("  死区设置: 摇杆=10%%，油门=15%%（低端）\n");
    print("  中心值: 油门=%.0f，其他摇杆=%.0f\n", THROTTLE_MIN, STICK_CENTER);
    print("  缩放因子: %.2f (前端±100→实际±30)\n", STICK_SCALE_FACTOR);
    print("  异常值过滤: 已启用（阈值降低到1000）\n");
    print("  油门修复: 自动检测范围，0值正确处理为0%%\n");
    print("  横滚修复: 修复锁定在±7.3的问题\n");
    print("  功能版本: 基础版（无气压定高功能）\n");
    
    // 设置服务器回调
    webRCServer.on("/", HTTP_GET, []() {
        webRCServer.send(200, "text/html", webRCIndexHtml);
    });
    
    webRCServer.on("/web_rc", HTTP_POST, handleWebRCRequest);

    // ---- 控制台日志接口 ----
    webRCServer.on("/console", HTTP_GET, []() {
        String json = "{\"lines\":[";
        int start = (consoleFilled < CONSOLE_LINES) ? 0 : consoleTail;
        for (int i = 0; i < consoleFilled; i++) {
            int idx = (start + i) % CONSOLE_LINES;
            if (i > 0) json += ",";
            json += "\"";
            String line = consoleBuf[idx];
            line.replace("\\", "\\\\");
            line.replace("\"", "\\\"");
            json += line;
            json += "\"";
        }
        json += "]}";
        webRCServer.send(200, "application/json", json);
    });

    webRCServer.on("/console/cmd", HTTP_POST, []() {
        String cmd = webRCServer.arg("plain");
        cmd.trim();
        if (cmd.length() > 0) {
            char logLine[CONSOLE_LINE_LEN];
            snprintf(logLine, sizeof(logLine), "> %s", cmd.c_str());
            webLog(logLine);
            // 解析简单命令
            if (cmd == "arm")              { armed = true;  webLog("CMD: armed"); }
            else if (cmd == "disarm")      { armed = false; webLog("CMD: disarmed"); }
            else if (cmd == "mode stab")   { mode = STAB;   webLog("CMD: mode=STAB"); }
            else if (cmd == "mode acro")   { mode = ACRO;   webLog("CMD: mode=ACRO"); }
            else if (cmd == "status") {
                char s[CONSOLE_LINE_LEN];
                snprintf(s, sizeof(s), "T=%.1f R=%.1f P=%.1f Y=%.1f arm=%d mode=%d",
                    webRCThrottle, webRCRoll, webRCPitch, webRCYaw, (int)armed, (int)mode);
                webLog(s);
            } else { webLog("Unknown cmd"); }
        }
        webRCServer.send(200, "application/json", "{\"ok\":1}");
    });

    webRCServer.on("/web_rc/status", HTTP_GET, []() {
        String json = "{";
        json += "\"enabled\":" + String(isWebRCEnabled() ? "true" : "false") + ",";
        json += "\"active\":" + String(isUsingWebRC() ? "true" : "false") + ",";
        json += "\"voltage\":" + String(readBatteryVoltage(), 2) + ",";
        json += "\"clients\":0,";  // 简化，不依赖WiFi类
        json += "\"current_data\":{";
        json += "\"throttle\":" + String(webRCThrottle, 1) + ",";
        json += "\"roll\":" + String(webRCRoll, 1) + ",";
        json += "\"pitch\":" + String(webRCPitch, 1) + ",";
        json += "\"yaw\":" + String(webRCYaw, 1);
        json += "},";
        
        json += "\"stick_mapping\":{";
        json += "\"left_y\":\"throttle (0~100%)\",";
        json += "\"left_x\":\"yaw (-30~+30)\",";
        json += "\"right_y\":\"pitch (-30~+30)\",";
        json += "\"right_x\":\"roll (-30~+30)\",";
        json += "\"deadzone\":\"" + String(stickDeadzone, 2) + "\",";
        json += "\"throttle_center\":\"" + String(THROTTLE_MIN, 1) + "\",";
        json += "\"stick_center\":\"" + String(STICK_CENTER, 1) + "\",";
        json += "\"stick_range\":\"" + String(STICK_MAX, 1) + "\",";
        json += "\"scale_factor\":\"" + String(STICK_SCALE_FACTOR, 2) + "\"";
        json += "},";
        
        json += "\"latency\":" + String(netMonitor.getLatency()) + ",";
        json += "\"loss_rate\":" + String(netMonitor.getPacketLossRate() / 10.0, 1) + ",";
        json += "\"packets\":" + String(netMonitor.getPacketCount());
        json += "}";
        
        webRCServer.send(200, "application/json", json);
    });
    
    webRCServer.begin();
    
    print("✓ Web RC 服务端已启动\n");
    print("  访问地址: http://192.168.4.1:8080\n");
    print("==========================================\n");
}

// ====================== 公共API函数 ======================
void processWebRC() {
    handleWebRCClient();
    checkWebRCConnectionStatus();
    
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 5000) {
        logWebRCNetworkStatus();
        lastLog = millis();
    }
}

void readWebRC() {
    processWebRC();
}

#else
// 当WEB_RC_ENABLED为0时的空函数定义
float throttleDeadzone = 0.15f;  // 统一使用15%死区
float throttleExpo = 0.2f;

void setupWebRC() {
    print("Web RC已禁用\n");
}

void readWebRC() {
}

void processWebRC() {
}
#endif
