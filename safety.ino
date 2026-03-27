// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 故障安全保护功能 - Web RC版本
// Fail-safe functions

#define RC_LOSS_TIMEOUT 1
#define WEB_RC_LOSS_TIMEOUT_MS 8000UL  // Web遥控器失联阈值(ms)，必须大于心跳间隔2000ms
#define DESCEND_TIME 10

// 倒置保护参数
#define INVERTED_COS_THRESHOLD -0.5f   // cos(120°)，倾角超过120°视为倒置
#define INVERTED_TIMEOUT        1.5f   // 持续倒置超过1.5秒触发停机

extern float controlTime;
extern float controlRoll, controlPitch, controlThrottle, controlYaw;

#if WEB_RC_ENABLED
// Web RC变量声明
extern bool webRCEnabled;
extern bool useWebRC;
extern unsigned long webRCLastUpdate;
extern float webRCRoll, webRCPitch, webRCYaw, webRCThrottle;
#endif

extern bool armed;
extern int mode;
extern float dt;
extern float thrustTarget;
extern float t;
extern Quaternion attitudeTarget;
extern Quaternion attitude;

void failsafe() {
	rcLossFailsafe();
#if WEB_RC_ENABLED
	webRCLossFailsafe();
#endif
	autoFailsafe();
	invertedFailsafe();
}

// RC loss failsafe
void rcLossFailsafe() {
	if (controlTime == 0) return; // no RC at all
	if (!armed) return;
	if (t - controlTime > RC_LOSS_TIMEOUT) {
		descend();
	}
}

// Smooth descend on RC lost
void descend() {
	mode = AUTO;
	attitudeTarget = Quaternion();
	thrustTarget -= dt / DESCEND_TIME;
	if (thrustTarget < 0) {
		thrustTarget = 0;
		armed = false;
	}
}

// Allow pilot to interrupt automatic flight
void autoFailsafe() {
	static float roll, pitch, yaw, throttle;
	
	// 检查传统RC输入是否有变化
	if (roll != controlRoll || pitch != controlPitch || yaw != controlYaw || abs(throttle - controlThrottle) > 0.05) {
		if (mode == AUTO) mode = STAB;
	}
	
#if WEB_RC_ENABLED
	// 检查Web遥控器输入是否有变化
	static float webRoll = 0, webPitch = 0, webYaw = 0, webThrottle = 0;
	if (useWebRC && webRCEnabled) {
		if (webRoll != webRCRoll || webPitch != webRCPitch || 
		    webYaw != webRCYaw || abs(webThrottle - webRCThrottle) > 5) {
			if (mode == AUTO) mode = STAB;
		}
		webRoll = webRCRoll;
		webPitch = webRCPitch;
		webYaw = webRCYaw;
		webThrottle = webRCThrottle;
	}
#endif
	
	roll = controlRoll;
	pitch = controlPitch;
	yaw = controlYaw;
	throttle = controlThrottle;
}

#if WEB_RC_ENABLED
// Web遥控器丢失保护
void webRCLossFailsafe() {
	if (!webRCEnabled || !useWebRC) return;
	if (!armed) return;

	// 使用毫秒直接比较，避免整数除法引入的最大1秒误差
	if (millis() - webRCLastUpdate > WEB_RC_LOSS_TIMEOUT_MS) {
		print("Web RC连接丢失，启动下降\n");
		descend();
		webRCEnabled = false;
		useWebRC = false;
	}
}
#endif

// 倒置保护：机体Z轴与世界Z轴夹角超过120°持续1.5秒则停机
void invertedFailsafe() {
	if (!armed) return;

	// 取机体Z轴在世界系的Z分量：正立时≈+1，倒置时≈-1
	Vector worldUp = Quaternion::rotateVector(Vector(0, 0, 1), attitude);

	static float invertedStartTime = 0;
	if (worldUp.z < INVERTED_COS_THRESHOLD) {
		if (invertedStartTime == 0) invertedStartTime = t;
		if (t - invertedStartTime > INVERTED_TIMEOUT) {
			armed = false;
			thrustTarget = 0.0f;
			print("倒置保护：停机\n");
		}
	} else {
		invertedStartTime = 0;
	}
}
