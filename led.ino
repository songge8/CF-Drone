// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 板载LED灯控制
// Board's LED control

#define BLINK_PERIOD      500000  // 慢闪：500ms 半周期 → 1 Hz
#define BLINK_FAST_PERIOD  62500  // 快闪：62.5ms 半周期 → 8 Hz

#ifndef LED_BUILTIN
#define LED_BUILTIN 2 // for ESP32 Dev Module
#endif

// ---- 外部依赖声明 ----
extern bool armed;
extern float t;
extern float controlTime;
extern float rcLossTimeout;
extern bool isInverted; // safety.ino
#if WEB_RC_ENABLED
extern bool webRCEnabled;
extern bool useWebRC;
bool isUsingWebRC();
#endif
// readBatteryVoltage() 和 VBAT_LOW_THRESHOLD 由 battery.ino 提供

void setupLED() {
	pinMode(LED_BUILTIN, OUTPUT);
}

void setLED(bool on) {
	static bool state = false;
	if (on == state) {
		return; // don't call digitalWrite if the state is the same
	}
	digitalWrite(LED_BUILTIN, on ? HIGH : LOW);
	state = on;
}

void blinkLED() {
	setLED(micros() / BLINK_PERIOD % 2);
}

// 检测是否有任意告警（低电 / 遥控失联 / 倒置）
bool ledAlertActive() {
	// 倒置检测
	if (isInverted) return true;

	// 遥控失联检测（SBUS RC）
	if (controlTime != 0 && armed && (t - controlTime > rcLossTimeout)) return true;

#if WEB_RC_ENABLED
	// Web RC 失联检测：已激活但超时
	if (webRCEnabled && useWebRC && !isUsingWebRC()) return true;
#endif

	// 低电量检测（每秒采样一次）
	static float lastVbatCheck = 0;
	static bool lowBat = false;
	if (t - lastVbatCheck > 1.0f) {
		lastVbatCheck = t;
		float vbat = readBatteryVoltage();
		lowBat = (vbat > 0.5f && vbat < VBAT_LOW_THRESHOLD); // 0.5V 以下视为未接电池
	}
	if (lowBat) return true;

	return false;
}

// 主循环调用：根据飞行状态驱动 LED
void updateLED() {
	if (!armed) {
		setLED(false); // 未解锁：常灭
		return;
	}
	if (ledAlertActive()) {
		setLED(micros() / BLINK_FAST_PERIOD % 2); // 告警：快闪 8Hz
	} else {
		setLED(micros() / BLINK_PERIOD % 2); // 正常飞行：慢闪 1Hz
	}
}
