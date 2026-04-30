// 板载LED灯控制
// Board's LED control

#include "board_config.h"

#if BOARD_LED_ENABLED

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
extern float thrustTarget;    // control.ino
extern float batteryVoltage;  // battery.ino
extern bool isInverted; // safety.ino
#if WEB_RC_ENABLED
extern bool webRCEnabled;
extern bool useWebRC;
bool isUsingWebRC();
#endif

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

// 电池告警：按飞行状态选择阈值
// 飞行中（thrustTarget >= 0.15）→ L2（2.8V），L1 在飞行中不适用
// 未解锁 / 解锁怠速 → L1（3.4V）
bool batteryAlertActive() {
	if (batteryVoltage <= VBAT_ABSENT_THRESHOLD) return false;
	bool flying = armed && thrustTarget >= 0.15f;
	if (flying) return batteryVoltage < VBAT_LOW_THRESHOLD;   // L2：飞行中
	else        return batteryVoltage < VBAT_WARN_THRESHOLD;   // L1：未解锁/怠速
}

// 检测是否有任意告警（低电 / 遥控失联 / 倒置）
bool ledAlertActive() {
	// 倒置检测
	if (isInverted) return true;

	// 遥控失联检测（SBUS RC，仅解锁后）
	if (controlTime != 0 && armed && (t - controlTime > rcLossTimeout)) return true;

#if WEB_RC_ENABLED
	// Web RC 失联检测：已激活但超时
	if (webRCEnabled && useWebRC && !isUsingWebRC()) return true;
#endif

	if (batteryAlertActive()) return true;

	return false;
}

// 主循环调用：根据飞行状态驱动 LED
void updateLED() {
	if (!armed) {
		if (batteryAlertActive()) {
			setLED(micros() / BLINK_FAST_PERIOD % 2); // 解锁前低电：快闪
		} else {
			setLED(false); // 正常待机：常灭
		}
		return;
	}
	if (ledAlertActive()) {
		setLED(micros() / BLINK_FAST_PERIOD % 2); // 任何告警：快闪 8Hz
	} else {
		setLED(micros() / BLINK_PERIOD % 2); // 正常飞行：慢闪 1Hz
	}
}

#else // BOARD_LED_ENABLED == 0（ESP32-C3 无板载 LED）

void setupLED() {}
void setLED(bool on) {}
void updateLED() {}

#endif // BOARD_LED_ENABLED
