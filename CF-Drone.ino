// 主程序
// 使用Arduino IDE 2.3.x以上版本编译，不支持老版本的1.8.19！
// 必须安装ESP32开发板核心库（即esp32 by Espressif Systems）和依赖（MAVLINK等）

#include "vector.h"
#include "quaternion.h"
#include "util.h"
#include "board_config.h"

// WiFi 和 Web 遥控器开关由 board_config.h 按芯片自动设置：
// 如需手动覆盖，在此处 #undef 后重新 #define
#define WIFI_ENABLED    BOARD_WIFI_ENABLED
#define WEB_RC_ENABLED  BOARD_WEB_RC_ENABLED

float t = NAN; // current step time, s
float dt; // time delta from previous step, s
float controlRoll, controlPitch, controlYaw, controlThrottle; // pilot's inputs, range [-1, 1]
float controlMode = NAN;
Vector gyro; // gyroscope data
Vector acc; // accelerometer data, m/s/s
Vector rates; // filtered angular rates, rad/s
Quaternion attitude; // estimated attitude
bool landed; // are we landed and stationary

void setup() {
	Serial.begin(115200);
	print("程序开始初始化！\n");
	disableBrownOut();
	setupParameters();
	setupLED();
	setupMotors();
	setLED(true);
#if WIFI_ENABLED
	setupWiFi();
#endif
#if WEB_RC_ENABLED
	setupWebRC();  // 初始化Web遥控器
#endif
	setupIMU();
	setupRC();
	setLED(false);
	print("程序初始化完成！\n");
	print("================================\n");
}

void loop() {
	readIMU();
	step();
	readRC();
#if WEB_RC_ENABLED
	readWebRC();  // 读取Web遥控器输入
	processConsoleCommandQueue(); // 将网页命令放到主循环执行，避免阻塞HTTP回调
#endif
	estimate();
	updateBatteryVoltage();
	control();
	sendMotors();
	handleInput();
#if WIFI_ENABLED
	processMavlink();
#endif
	logData();
	syncParameters();
	updateLED();
}
