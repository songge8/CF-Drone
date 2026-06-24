// 电池电压检测
// Battery voltage monitoring

#include "board_config.h"

#define VBAT_ADC_PIN       BOARD_VBAT_ADC_PIN     // 电池电压采样引脚
#define VBAT_ADC_SAMPLES   BOARD_VBAT_ADC_SAMPLES  // ADC采样数：C3=8（减少阻塞）/ ESP32&S3=16（更低噪声）
#define VBAT_DIVIDER       (43.0f / 33.0f)  // 分压比：上桥10kΩ+下桥33kΩ，公式 Vbat = Vadc × 43/33
#define VBAT_WARN_THRESHOLD     3.5f  // L1：空载/怠速预警门限（V）
#define VBAT_LOW_THRESHOLD      3.3f  // L2：飞行中低电告警门限（V）
#define VBAT_CRITICAL_THRESHOLD 3.0f  // L3：飞行中危急门限，自动降落（V）
#define VBAT_ABSENT_THRESHOLD   0.5f  // 低于此值视为未接电池，忽略所有告警

#define BATTERY_FLYING_THRUST_MIN    0.15f  // 推力≥此值视为飞行中，L1不适用
#define BATTERY_ACTION_DEBOUNCE_TIME  0.9f  // 低压连续判定防抖时间（秒）

extern float t;
float batteryVoltage = 0.0f;  // 全局最新电压，由 updateBatteryVoltage() 定期刷新

void updateBatteryVoltage() {
	static float lastCheck = 0;
	if (t - lastCheck < 0.5f) return;  // 每 0.5 秒采样一次
	lastCheck = t;
	batteryVoltage = readBatteryVoltage();
}

float readBatteryVoltage() {
	long sum = 0;
	for (int i = 0; i < VBAT_ADC_SAMPLES; i++) sum += analogReadMilliVolts(VBAT_ADC_PIN);
	return (sum / (float)VBAT_ADC_SAMPLES / 1000.0f) * VBAT_DIVIDER;
}
