// web_rc.ino
// Web RC 服务端

#if WEB_RC_ENABLED

#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include "web_rc_html.h"

// 飞控统一控制变量（供协议适配层写入，与 SBUS/MAVLink 共用）
extern float t;
extern float controlTime;
extern float controlRoll, controlPitch, controlYaw, controlThrottle, controlMode;

// ==================== 配置常量 ====================
#define WEB_RC_TIMEOUT_MS    10000          // 连接超时：最后一次收包超过此时间(ms)视为断连；需大于心跳间隔
#define VBAT_ADC_PIN         36             // 电池电压采样引脚（GPIO36/VP，只读ADC）
#define VBAT_ADC_SAMPLES     16             // ADC多次采样取均值，越大噪声越低、响应越慢
#define VBAT_DIVIDER         (43.0f / 33.0f)// 分压比：上桥10kΩ+下桥33kΩ，公式 Vbat = Vadc × 43/33

// ==================== 连接状态标志 ====================
bool webRCEnabled    = false;  // Web RC 当前有有效连接（由 readWebRC() 每帧更新）
bool useWebRC        = false;  // 当前正在使用 Web RC 控制（与 webRCEnabled 保持同步）
bool webRCUpdated    = false;  // 收到过至少一次摇杆数据（首次连接前为 false）
bool webConsoleEnabled = false; // Web 调试控制台开关：POST /console/enable 开启，开启后 print() 写入缓冲区
static char webRCWarnMsg[64] = ""; // 待发送给前端的警告消息，发送一次后自动清空

// ==================== 摇杆暂存值（供状态端点读取）====================
// 单位：油门 0~100（%），姿态轴 ±30（°），按下=1/松开=0
float webRCRoll     = 0.0f;
float webRCPitch    = 0.0f;
float webRCYaw      = 0.0f;
float webRCThrottle = 0.0f;
uint16_t webRCButtons    = 0;       // 16位按钮位掩码，bit0=解锁 bit1=上锁 bit2=急停 bit6=STAB bit7=ACRO bit8=ALTHOLD
unsigned long webRCLastUpdate = 0;  // 最后一次收包的 millis() 时间戳

// ==================== 灵敏度缩放 ====================
// 最终输出 = 处理后角度 × scale / STICK_MAX，结果写入 control* ([-1,1])
// 减小 scale → 飞机响应更柔和；增大 → 更灵敏
float webRCThrottleScale = 1.0f;    // 油门最大功率限制，1.0=100%，0.8=最多只能推到80%油门
float webRCStickScale    = 0.85f;   // 横滚/俯仰灵敏度，建议范围 0.5~1.0
float webRCYawScale      = 0.68f;   // 偏航灵敏度，偏航惯性小故单独偏低，建议范围 0.3~0.8

// ==================== 死区 ====================
// 归一化空间（0~1 比例），摇杆归中误差在死区内输出恒为 0
// 增大 → 中立区更宽、误触更少；减小 → 响应更灵敏但易漂移
float stickDeadzone    = 0.15f;     // 横滚/俯仰/偏航共用，15% 归一化死区
float throttleDeadzone = 0.15f;     // 油门低端死区，推杆低于此比例时输出 0%（防抖）

// ==================== 输入值域（勿随意修改，需与前端保持一致）====================
static const float THROTTLE_MIN = 0.0f;    // 油门输出下限（%）
static const float THROTTLE_MAX = 100.0f;  // 油门输出上限（%）
static const float STICK_MAX    = 30.0f;   // 姿态轴最大角度（°），对应前端满偏；修改需同步调整 PID TILT_MAX
static const float RAW_MAX      = 100.0f;  // 前端摇杆归一化满偏值，前端 JS 固定输出 ±100

// ==================== 内部状态（运行时，勿手动修改）====================
static float lastValidThrottle = 0.0f;     // 上次通过验证的油门值，异常时回退使用
static float lastValidRoll     = 0.0f;     // 上次通过验证的横滚值
static float lastValidPitch    = 0.0f;     // 上次通过验证的俯仰值
static float lastValidYaw      = 0.0f;     // 上次通过验证的偏航值
static unsigned long lastDataErrorTime = 0; // 上次数据异常时间，用于限速错误日志输出

// ==================== 网络监控 ====================
static uint32_t totalBytesReceived = 0;  // 累计接收字节数，用于计算实时带宽（bps）

class NetworkMonitor {
    unsigned long lastReceived, lastByteCountTime;
    uint16_t packetLoss, packetCount;
    uint16_t avgLatency;
    uint32_t bytesPerSecond, lastByteCount;
public:
    NetworkMonitor() : packetLoss(0), packetCount(0), avgLatency(0),
                       bytesPerSecond(0), lastByteCount(0) {
        lastReceived = lastByteCountTime = millis();
    }
    void update(unsigned long jsTs, uint16_t bytes = 0) {
        unsigned long now = millis();
        if (now - lastReceived > 150) packetLoss++;
        if (jsTs > 0) avgLatency = (avgLatency * 9 + (uint16_t)(now - jsTs)) / 10;
        if (bytes > 0) {
            totalBytesReceived += bytes;
            if (now - lastByteCountTime >= 1000) {
                bytesPerSecond    = totalBytesReceived - lastByteCount;
                lastByteCount     = totalBytesReceived;
                lastByteCountTime = now;
            }
        }
        lastReceived = now;
        packetCount++;
    }
    uint16_t getLatency()        const { return avgLatency; }
    uint16_t getPacketLossRate() const { return packetCount ? (packetLoss * 1000) / packetCount : 0; }
    uint32_t getPacketCount()    const { return packetCount; }
    uint32_t getBytesPerSecond() const { return bytesPerSecond; }
} netMonitor;

// ==================== Web 服务器 ====================
WebServer webRCServer(8080);

// ==================== 控制台日志缓冲区 ====================
#define CONSOLE_LINES    30
#define CONSOLE_LINE_LEN 80
static char consoleBuf[CONSOLE_LINES][CONSOLE_LINE_LEN];
static int  consoleTail   = 0;
static int  consoleFilled = 0;

void webLog(const char* msg) {
    strncpy(consoleBuf[consoleTail], msg, CONSOLE_LINE_LEN - 1);
    consoleBuf[consoleTail][CONSOLE_LINE_LEN - 1] = '\0';
    consoleTail = (consoleTail + 1) % CONSOLE_LINES;
    if (consoleFilled < CONSOLE_LINES) consoleFilled++;
}

// ==================== 摇杆处理 ====================

// 归一化空间死区（输入/输出均为 [-1, 1]）
// 超出死区的部分线性重映射到满幅，满推摇杆=满输出
float applyDeadzone(float norm, float deadzone) {
    if (fabsf(norm) < deadzone) return 0.0f;
    float sign = (norm > 0.0f) ? 1.0f : -1.0f;
    return sign * (fabsf(norm) - deadzone) / (1.0f - deadzone);
}

// 处理姿态轴（横滚/俯仰/偏航）
// 输入 raw ∈ [-100, +100]，输出 ∈ [-STICK_MAX, +STICK_MAX] (°)
float processAxis(float raw, float& lastValid) {
    if (isnan(raw) || isinf(raw) || fabsf(raw) > 1000.0f) {
        if (millis() - lastDataErrorTime > 2000) lastDataErrorTime = millis();
        return lastValid;
    }
    float norm = constrain(raw, -RAW_MAX, RAW_MAX) / RAW_MAX;
    norm = applyDeadzone(norm, stickDeadzone);
    lastValid = norm * STICK_MAX;
    return lastValid;
}

// 处理油门轴
// 前端：左摇杆Y轴，底部=-100，顶部=+100
// 输出：pct ∈ [0, 100]
float processThrottle(float raw) {
    if (isnan(raw) || isinf(raw) || fabsf(raw) > 1000.0f) return lastValidThrottle;
    raw = constrain(raw, -RAW_MAX, RAW_MAX);
    // 线性映射：-100→0%，0→50%，+100→100%
    float pct = (raw + RAW_MAX) / (2.0f * RAW_MAX) * THROTTLE_MAX;
    if (pct < throttleDeadzone * THROTTLE_MAX) pct = 0.0f;
    pct = constrain(pct * webRCThrottleScale, THROTTLE_MIN, THROTTLE_MAX);
    lastValidThrottle = pct;
    return pct;
}

// ==================== 核心数据处理 ====================

void setWebRCInput(float roll, float pitch, float yaw, float throttle, uint16_t buttons) {
    float pThrottle = processThrottle(throttle);
    float pYaw      = processAxis(yaw,   lastValidYaw);
    float pPitch    = processAxis(pitch, lastValidPitch);
    float pRoll     = processAxis(roll,  lastValidRoll);

    // 暂存处理后的值（供状态端点读取）
    webRCThrottle = pThrottle;
    webRCYaw      = pYaw;
    webRCPitch    = pPitch;
    webRCRoll     = pRoll;
    webRCButtons  = buttons;
    webRCLastUpdate = millis();
    webRCUpdated  = true;

    // 写入统一控制变量（与 SBUS/MAVLink 同路径）
    controlRoll     = constrain(pRoll  * webRCStickScale / STICK_MAX, -1.0f, 1.0f);
    controlPitch    = constrain(pPitch * webRCStickScale / STICK_MAX, -1.0f, 1.0f);
    controlYaw      = constrain(pYaw   * webRCYawScale   / STICK_MAX, -1.0f, 1.0f);
    controlThrottle = pThrottle / THROTTLE_MAX;
    controlMode     = NAN;
    controlTime     = t;

    static unsigned long lastPrint = 0;
    static float lastPrintedThrottle = -1.0f;
    if (millis() - lastPrint > 500 || fabsf(pThrottle - lastPrintedThrottle) > 5.0f) {
        print("WebRC T=%.0f%% R=%.1f P=%.1f Y=%.1f Btn=0x%04X\n",
              pThrottle, pRoll, pPitch, pYaw, buttons);
        lastPrintedThrottle = pThrottle;
        lastPrint = millis();
    }
}

// ==================== JSON 协议处理器 ====================

const char* findJsonValue(const char* json, const char* key) {
    const char* p = strstr(json, key);
    if (!p) return nullptr;
    p = strchr(p, ':');
    if (!p) return nullptr;
    p++;
    while (*p == ' ' || *p == '"') p++;
    return p;
}

bool handleJSONProtocol(String& body) {
    if (body.length() == 0 || body.indexOf('{') == -1) return false;
    const char* json = body.c_str();
    const char* typePos = strstr(json, "\"t\":");
    if (!typePos) return false;
    int type = atoi(typePos + 4);
    const char* v;

    switch (type) {
        case 1: { // 摇杆数据
            float th = 0, r = 0, p = 0, y = 0;
            uint32_t ts = 0;
            if ((v = findJsonValue(json, "\"th\""))) th = atof(v);
            if ((v = findJsonValue(json, "\"r\"")))  r  = atof(v);
            if ((v = findJsonValue(json, "\"p\"")))  p  = atof(v);
            if ((v = findJsonValue(json, "\"y\"")))  y  = atof(v);
            if ((v = findJsonValue(json, "\"ts\""))) ts = atol(v);
            setWebRCInput(r, p, y, th, webRCButtons);
            netMonitor.update(ts);
            break;
        }
        case 2: { // 按钮事件
            int idx = 0, state = 0;
            uint32_t ts = 0;
            if ((v = findJsonValue(json, "\"b\"")))  idx   = atoi(v);
            if ((v = findJsonValue(json, "\"s\"")))  state = atoi(v);
            if ((v = findJsonValue(json, "\"ts\""))) ts    = atol(v);
            if (idx >= 0 && idx < 16) {
                if (state) webRCButtons |=  (1 << idx);
                else       webRCButtons &= ~(1 << idx);
                webRCLastUpdate = millis();
                webRCUpdated    = true;
            }
            netMonitor.update(ts, body.length());
            break;
        }
        case 4: // 心跳
            netMonitor.update(0, body.length());
            webRCLastUpdate = millis();
            webRCUpdated    = true;
            break;
    }
    return true;
}

void setWebRCWarn(const char* msg) {
    strncpy(webRCWarnMsg, msg, sizeof(webRCWarnMsg) - 1);
    webRCWarnMsg[sizeof(webRCWarnMsg) - 1] = '\0';
}

// ==================== HTTP 请求处理 ====================

void handleWebRCRequest() {
    if (!webRCServer.hasArg("plain")) {
        webRCServer.send(400, "application/json", "{\"e\":\"no data\"}");
        return;
    }
    String body = webRCServer.arg("plain");
    if (handleJSONProtocol(body)) {
        char resp[256];
        if (webRCWarnMsg[0]) {
            snprintf(resp, sizeof(resp),
                "{\"s\":\"ok\",\"l\":%u,\"pl\":%.1f,\"m\":%d,\"arm\":%d,\"warn\":\"%s\"}",
                netMonitor.getLatency(),
                netMonitor.getPacketLossRate() / 10.0f,
                mode, (int)armed, webRCWarnMsg);
            webRCWarnMsg[0] = '\0'; // 发送后立即清空
        } else {
            snprintf(resp, sizeof(resp),
                "{\"s\":\"ok\",\"l\":%u,\"pl\":%.1f,\"m\":%d,\"arm\":%d}",
                netMonitor.getLatency(),
                netMonitor.getPacketLossRate() / 10.0f,
                mode, (int)armed);
        }
        webRCServer.send(200, "application/json", resp);
    } else {
        webRCServer.send(400, "application/json", "{\"e\":\"parse failed\"}");
    }
}

// ==================== 电池电压 ====================

float readBatteryVoltage() {
    long sum = 0;
    for (int i = 0; i < VBAT_ADC_SAMPLES; i++) sum += analogRead(VBAT_ADC_PIN);
    return (sum / (float)VBAT_ADC_SAMPLES / 4095.0f) * 3.3f * VBAT_DIVIDER;
}

// ==================== 连接状态 ====================

bool isWebRCEnabled() {
    return webRCUpdated && (millis() - webRCLastUpdate < WEB_RC_TIMEOUT_MS);
}

bool isUsingWebRC() {
    return useWebRC && isWebRCEnabled();
}

// ==================== 主设置函数 ====================

void setupWebRC() {
    lastValidThrottle = THROTTLE_MIN;
    lastValidRoll = lastValidPitch = lastValidYaw = 0.0f;

    webRCServer.on("/", HTTP_GET, []() {
        webRCServer.send(200, "text/html", webRCIndexHtml);
    });
    webRCServer.on("/web_rc",           HTTP_POST, handleWebRCRequest);
    webRCServer.on("/web_rc/heartbeat", HTTP_POST, handleWebRCRequest);

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
            json += line + "\"";
        }
        json += "]}";
        webRCServer.send(200, "application/json", json);
    });

    webRCServer.on("/console/cmd", HTTP_POST, []() {
        String cmd = webRCServer.arg("plain");
        cmd.trim();
        if (cmd.length() > 0) {
            char buf[CONSOLE_LINE_LEN];
            snprintf(buf, sizeof(buf), "> %s", cmd.c_str());
            webLog(buf);
            doCommand(cmd, false);
        }
        webRCServer.send(200, "application/json", "{\"ok\":1}");
    });

    webRCServer.on("/console/enable", HTTP_POST, []() {
        webConsoleEnabled = true;
        webRCServer.send(200, "application/json", "{\"ok\":1}");
    });

    webRCServer.on("/console/disable", HTTP_POST, []() {
        webConsoleEnabled = false;
        webRCServer.send(200, "application/json", "{\"ok\":1}");
    });

    webRCServer.on("/web_rc/status", HTTP_GET, []() {
        float vbat = readBatteryVoltage();
        if (isnan(vbat) || vbat < 0.0f) vbat = 0.0f;
        char json[384];
        snprintf(json, sizeof(json),
            "{\"enabled\":%s,\"active\":%s,"
            "\"voltage\":%.2f,"
            "\"throttle\":%.1f,\"roll\":%.1f,\"pitch\":%.1f,\"yaw\":%.1f,"
            "\"latency\":%u,\"loss_rate\":%.1f,\"packets\":%lu}",
            isWebRCEnabled() ? "true" : "false",
            isUsingWebRC()   ? "true" : "false",
            vbat,
            webRCThrottle, webRCRoll, webRCPitch, webRCYaw,
            netMonitor.getLatency(),
            netMonitor.getPacketLossRate() / 10.0f,
            (unsigned long)netMonitor.getPacketCount());
        webRCServer.send(200, "application/json", json);
    });

    webRCServer.begin();
    print("✓ Web RC 已启动: http://192.168.4.1:8080\n");
    print("  死区 摇杆=%.0f%% 油门=%.0f%% | 缩放 摇杆=%.2f 偏航=%.2f\n",
          stickDeadzone * 100.0f, throttleDeadzone * 100.0f,
          webRCStickScale, webRCYawScale);
}

// ==================== 主循环函数 ====================

void readWebRC() {
    webRCServer.handleClient();
    if (isWebRCEnabled()) {
        webRCEnabled = useWebRC = true;
    } else {
        webRCEnabled = useWebRC = false;
    }
    static unsigned long lastLog = 0;
    if (millis() - lastLog > 30000) {
        print("WebRC 延迟=%ums 丢包=%.1f%% 包数=%lu\n",
              netMonitor.getLatency(),
              netMonitor.getPacketLossRate() / 10.0f,
              (unsigned long)netMonitor.getPacketCount());
        lastLog = millis();
    }
}

#else
void setupWebRC() { print("Web RC已禁用\n"); }
void readWebRC()  {}
#endif
