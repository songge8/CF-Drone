// 电池电压检测
// Battery voltage monitoring

#define VBAT_ADC_PIN       36               // 电池电压采样引脚（GPIO36/VP，只读ADC）
#define VBAT_ADC_SAMPLES   16               // ADC多次采样取均值，越大噪声越低、响应越慢
#define VBAT_DIVIDER       (43.0f / 33.0f)  // 分压比：上桥10kΩ+下桥33kΩ，公式 Vbat = Vadc × 43/33
#define VBAT_LOW_THRESHOLD 3.0f             // 1S LiPo 低电告警门限（V）

float readBatteryVoltage() {
	long sum = 0;
	for (int i = 0; i < VBAT_ADC_SAMPLES; i++) sum += analogRead(VBAT_ADC_PIN);
	return (sum / (float)VBAT_ADC_SAMPLES / 4095.0f) * 3.3f * VBAT_DIVIDER;
}
