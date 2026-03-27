// Copyright (c) 2024 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 故障安全保护功能 - Web RC版本
// Fail-safe functions

#define RC_LOSS_TIMEOUT 1
#define WEB_RC_LOSS_TIMEOUT 2  // Web遥控器超时时间稍长
#define DESCEND_TIME 10

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

void failsafe() {
	rcLossFailsafe();
#if WEB_RC_ENABLED
	webRCLossFailsafe();
#endif
	autoFailsafe();
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
	
	// 检查Web遥控器是否超时
	if (millis() / 1000.0f - webRCLastUpdate / 1000.0f > WEB_RC_LOSS_TIMEOUT) {
		print("Web RC丢失，启动下降\n");
		descend();
		webRCEnabled = false;
		useWebRC = false;
	}
}
#endif
