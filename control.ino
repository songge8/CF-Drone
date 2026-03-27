// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// 飞行控制Flight control，Web RC版本

#include "vector.h"
#include "quaternion.h"
#include "pid.h"
#include "lpf.h"
#include "util.h"

#if WEB_RC_ENABLED
#include "web_rc_global.h"
#include "control.h"
#endif

// ============== 角速率环（内环）参数 ==============
#define PITCHRATE_P 0.05 // 增大P值提高响应速度
#define PITCHRATE_I 0.2 // 中等I值补偿电机差异
#define PITCHRATE_D 0.001 // 小D值抑制震荡
#define PITCHRATE_I_LIM 0.35 // 限制积分积累
#define ROLLRATE_P 0.06 // 横滚和俯仰使用相同参数
#define ROLLRATE_I PITCHRATE_I 
#define ROLLRATE_D 0.002 
#define ROLLRATE_I_LIM PITCHRATE_I_LIM
#define YAWRATE_P 0.3 // 偏航需要更高的P值（惯性较小）
#define YAWRATE_I 0.01 // 中等I值补偿
#define YAWRATE_D 0.01 // 小D值
#define YAWRATE_I_LIM 0.3
// ============== 角度环（外环）参数 ==============
#define ROLL_P 7 // 较高的P值快速响应
#define ROLL_I 0 // 角度环通常不需要I项
#define ROLL_D 0 // 角度环通常不需要D项
#define PITCH_P ROLL_P // 横滚和俯仰相同
#define PITCH_I ROLL_I
#define PITCH_D ROLL_D
#define YAW_P 3 // 偏航响应稍慢
// ============== 限制值 ==============
#define PITCHRATE_MAX radians(360) // 高转速限制（1000°/s）
#define ROLLRATE_MAX radians(360)
#define YAWRATE_MAX radians(300) // 偏航转速稍低
#define TILT_MAX radians(30) // 最大倾斜角30°
#define RATES_D_LPF_ALPHA 0.2 // cutoff frequency ~ 40 Hz

const int RAW = 0, ACRO = 1, STAB = 2, AUTO = 3; // flight modes
int mode = STAB;
bool armed = false;

#if WEB_RC_ENABLED
// Web RC变量声明
extern bool webRCEnabled;
extern bool useWebRC;
extern bool webRCUpdated;
extern float webRCRoll, webRCPitch, webRCYaw, webRCThrottle;
extern uint16_t webRCButtons;
extern unsigned long webRCLastUpdate;
extern float webRCThrottleScale;
extern float webRCStickScale;
extern float webRCYawScale;

// Web RC函数声明
bool isWebRCEnabled();
bool isUsingWebRC();
#endif

PID rollRatePID(ROLLRATE_P, ROLLRATE_I, ROLLRATE_D, ROLLRATE_I_LIM, RATES_D_LPF_ALPHA);
PID pitchRatePID(PITCHRATE_P, PITCHRATE_I, PITCHRATE_D, PITCHRATE_I_LIM, RATES_D_LPF_ALPHA);
PID yawRatePID(YAWRATE_P, YAWRATE_I, YAWRATE_D);
PID rollPID(ROLL_P, ROLL_I, ROLL_D);
PID pitchPID(PITCH_P, PITCH_I, PITCH_D);
PID yawPID(YAW_P, 0, 0);
Vector maxRate(ROLLRATE_MAX, PITCHRATE_MAX, YAWRATE_MAX);
float tiltMax = TILT_MAX;

Quaternion attitudeTarget;
Vector ratesTarget;
Vector ratesExtra; // feedforward rates
Vector torqueTarget;
float thrustTarget;

extern const int MOTOR_REAR_LEFT, MOTOR_REAR_RIGHT, MOTOR_FRONT_RIGHT, MOTOR_FRONT_LEFT;
extern float controlRoll, controlPitch, controlThrottle, controlYaw, controlMode;

void control() {
	interpretControls();
#if WEB_RC_ENABLED
	interpretWebRC();
	combineInputs();
#endif
	failsafe();
	controlAttitude();
	controlRates();
	controlTorque();
}

void interpretControls() {
#if WEB_RC_ENABLED
	// 如果Web遥控器可用且正在使用，跳过传统RC解析
	if (isUsingWebRC()) {
		return;
	}
#endif
	
	if (controlMode < 0.25) mode = STAB;
	if (controlMode < 0.75) mode = STAB;
	if (controlMode > 0.75) mode = STAB;

	if (mode == AUTO) return; // pilot is not effective in AUTO mode

	if (controlThrottle < 0.05 && controlYaw > 0.95) armed = true; // arm gesture
	if (controlThrottle < 0.05 && controlYaw < -0.95) armed = false; // disarm gesture

	if (abs(controlYaw) < 0.1) controlYaw = 0; // yaw dead zone

	thrustTarget = controlThrottle;

	if (mode == STAB) {
		float yawTarget = attitudeTarget.getYaw();
		if (!armed || invalid(yawTarget) || controlYaw != 0) yawTarget = attitude.getYaw(); // reset yaw target
		attitudeTarget = Quaternion::fromEuler(Vector(controlRoll * tiltMax, controlPitch * tiltMax, yawTarget));
		ratesExtra = Vector(0, 0, -controlYaw * maxRate.z); // positive yaw stick means clockwise rotation in FLU
	}

	if (mode == ACRO) {
		attitudeTarget.invalidate(); // skip attitude control
		ratesTarget.x = controlRoll * maxRate.x;
		ratesTarget.y = controlPitch * maxRate.y;
		ratesTarget.z = -controlYaw * maxRate.z; // positive yaw stick means clockwise rotation in FLU
	}

	if (mode == RAW) { // direct torque control
		attitudeTarget.invalidate(); // skip attitude control
		ratesTarget.invalidate(); // skip rate control
		torqueTarget = Vector(controlRoll, controlPitch, -controlYaw) * 0.1;
	}
}

void controlAttitude() {
	if (!armed || attitudeTarget.invalid() || thrustTarget < 0.1) return; // skip attitude control

	const Vector up(0, 0, 1);
	Vector upActual = Quaternion::rotateVector(up, attitude);
	Vector upTarget = Quaternion::rotateVector(up, attitudeTarget);

	Vector error = Vector::rotationVectorBetween(upTarget, upActual);

	ratesTarget.x = rollPID.update(error.x) + ratesExtra.x;
	ratesTarget.y = pitchPID.update(error.y) + ratesExtra.y;

	float yawError = wrapAngle(attitudeTarget.getYaw() - attitude.getYaw());
	ratesTarget.z = yawPID.update(yawError) + ratesExtra.z;
}


void controlRates() {
	if (!armed || ratesTarget.invalid() || thrustTarget < 0.1) return; // skip rates control

	Vector error = ratesTarget - rates;

	// Calculate desired torque, where 0 - no torque, 1 - maximum possible torque
	torqueTarget.x = rollRatePID.update(error.x);
	torqueTarget.y = pitchRatePID.update(error.y);
	torqueTarget.z = yawRatePID.update(error.z);
}

void controlTorque() {
	if (!torqueTarget.valid()) return; // skip torque control

	if (!armed) {
		memset(motors, 0, sizeof(motors)); // stop motors if disarmed
		return;
	}

	if (thrustTarget < 0.1) {
		motors[0] = 0.1; // idle thrust
		motors[1] = 0.1;
		motors[2] = 0.1;
		motors[3] = 0.1;
		return;
	}

	motors[MOTOR_FRONT_LEFT] = thrustTarget + torqueTarget.x - torqueTarget.y + torqueTarget.z;
	motors[MOTOR_FRONT_RIGHT] = thrustTarget - torqueTarget.x - torqueTarget.y - torqueTarget.z;
	motors[MOTOR_REAR_LEFT] = thrustTarget + torqueTarget.x + torqueTarget.y - torqueTarget.z;
	motors[MOTOR_REAR_RIGHT] = thrustTarget - torqueTarget.x + torqueTarget.y + torqueTarget.z;

	motors[0] = constrain(motors[0], 0, 1);
	motors[1] = constrain(motors[1], 0, 1);
	motors[2] = constrain(motors[2], 0, 1);
	motors[3] = constrain(motors[3], 0, 1);
}

const char* getModeName() {
	switch (mode) {
		case RAW: return "RAW";
		case ACRO: return "ACRO";
		case STAB: return "STAB";
		case AUTO: return "AUTO";
		default: return "UNKNOWN";
	}
}

#if WEB_RC_ENABLED
// Web遥控器控制解析
void interpretWebRC() {
	if (!isWebRCEnabled()) {
		useWebRC = false;
		webRCEnabled = false;
		return;
	}
	
	webRCEnabled = true;
	useWebRC = true;
	
	// 检查Web RC超时（2秒）
	if (millis() - webRCLastUpdate > 2000) {
		useWebRC = false;
		thrustTarget = 0.0f;
		armed = false;
		print("Web RC超时 → 失联保护\n");
		return;
	}
	
	// 处理解锁/上锁按钮
	static bool lastArmedState = false;
	if (armed != lastArmedState) {
		print(armed ? "Web RC: 已解锁\n" : "Web RC: 已上锁\n");
		lastArmedState = armed;
	}
	
	// 按钮0：解锁（油门不超过30%时允许解锁）
	if (webRCButtons & 0x0001) {
		if (webRCThrottle <= 30.0f) {
			armed = true;
		}
		// 油门过高时不解锁，前端负责给出提示
	}
	
	// 按钮1：上锁
	if (webRCButtons & 0x0002) {
		armed = false;
	}

	// 按钮2：急停
	if (webRCButtons & 0x0004) {
		armed = false;
		thrustTarget = 0.0f;
	}
	
	// 按钮6：STAB模式
	if (webRCButtons & 0x0040) {
		mode = STAB;
	}
	
	// 按钮7：ACRO模式
	if (webRCButtons & 0x0080) {
		mode = ACRO;
	}
	
	// 处理模式切换
	static int lastMode = STAB;
	if (mode != lastMode) {
		print("Web RC: 模式切换到 %s\n", getModeName());
		lastMode = mode;
	}
}

// 合并Web遥控器输入
void combineInputs() {
	if (!isUsingWebRC()) {
		return;
	}
	
	// 验证数据
	float roll = webRCRoll;
	float pitch = webRCPitch;
	float yaw = webRCYaw;
	float throttle = webRCThrottle;
	
	// 油门处理：从0-100%转换到0-1范围
	thrustTarget = constrain(throttle / 100.0f, 0.0f, 1.0f);
	
	// 摇杆处理：从±30范围转换到±1范围（用于倾角和角速度）
	float rollNorm = constrain(roll / 30.0f, -1.0f, 1.0f);
	float pitchNorm = constrain(pitch / 30.0f, -1.0f, 1.0f);
	float yawNorm = constrain(yaw / 30.0f, -1.0f, 1.0f);
	
	// 应用灵敏度缩放
	rollNorm *= webRCStickScale;
	pitchNorm *= webRCStickScale;
	yawNorm *= webRCYawScale;
	
	// 根据飞行模式设置控制目标
	if (mode == STAB) {
		float yawTarget = attitudeTarget.getYaw();
		if (!armed || invalid(yawTarget) || abs(yawNorm) > 0.05) {
			yawTarget = attitude.getYaw();
		}
		attitudeTarget = Quaternion::fromEuler(Vector(rollNorm * tiltMax, pitchNorm * tiltMax, yawTarget));
		ratesExtra = Vector(0, 0, -yawNorm * maxRate.z);
	}
	
	if (mode == ACRO) {
		attitudeTarget.invalidate();
		ratesTarget.x = rollNorm * maxRate.x;
		ratesTarget.y = pitchNorm * maxRate.y;
		ratesTarget.z = -yawNorm * maxRate.z;
	}
	
	if (mode == RAW) {
		attitudeTarget.invalidate();
		ratesTarget.invalidate();
		torqueTarget = Vector(rollNorm, pitchNorm, -yawNorm) * 0.1;
	}
	
	// 调试输出
	static unsigned long lastDebug = 0;
	if (millis() - lastDebug > 1000) {
		print("Web RC: T=%.1f%% R=%.2f P=%.2f Y=%.2f | 模式=%s\n",
		      throttle, roll, pitch, yaw, getModeName());
		lastDebug = millis();
	}
}
#endif
