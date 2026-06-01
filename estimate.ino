// 基于陀螺仪和加速度计的姿态计算
// Attitude estimation from gyro and accelerometer

#include "quaternion.h"
#include "vector.h"
#include "lpf.h"
#include "util.h"

float accWeight = 0.003;
float levelWeight = 0.0002;
float levelMaxTilt = radians(30); // rad, level correction fades out at this tilt angle (matches TILT_MAX)
// ============== 水平修正 摇杆感知门控阈值 ==============
// 当飞手摇杆偏转量（横滚或俯仰取最大值，范围 0~1）超过此阈值时，
// applyLevel 的修正权重线性衰减至 0，避免与 PID 积分项产生耦合导致松杆后漂移。
// 调整方法：
//   - 值越小，门控越灵敏（轻微打杆即停止修正），悬停抑漂效果减弱。
//   - 值越大，打杆时 applyLevel 干扰更久，现象三改善减弱。
//   - 推荐范围：0.2（灵敏）~ 0.4（宽松），默认 0.3（摇杆 30% 行程时权重归零）。
//   - 可通过 CLI 在线修改：p EST_LVL_GATE_THR <值>
float levelGateThreshold = 0.3f; // 摇杆门控阈值，参数名：EST_LVL_GATE_THR

extern float controlRoll, controlPitch; // 飞手摇杆输入，定义于 CF-Drone.ino
LowPassFilter<Vector> ratesFilter(0.2); // cutoff frequency ~ 40 Hz

void estimate() {
	applyGyro();
	applyAcc();
	applyLevel();
}

void applyGyro() {
	// filter gyro to get angular rates
	rates = ratesFilter.update(gyro);

	// apply rates to attitude
	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(rates * dt));
}

void applyAcc() {
	// test should we apply accelerometer gravity correction
	float accNorm = acc.norm();
	landed = !motorsActive() && abs(accNorm - ONE_G) < ONE_G * 0.1f;

	if (!landed) return;

	// calculate accelerometer correction
	Vector up = Quaternion::rotateVector(Vector(0, 0, 1), attitude);
	Vector correction = Vector::rotationVectorBetween(acc, up) * accWeight;

	// apply correction
	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(correction));
}

// 水平修正
void applyLevel() {
	if (landed) return;

	// assume the pilot keeps the drone more or less level in flight
	// weight fades to zero as tilt approaches levelMaxTilt, so correction
	// does not fight intentional large-angle flight while still suppressing
	// gyro drift during normal hover
	Vector up = Quaternion::rotateVector(Vector(0, 0, 1), attitude);
	float tilt = acos(constrain(up.z, -1.0f, 1.0f));
	float dynamicWeight = levelWeight * constrain(1.0f - tilt / levelMaxTilt, 0.0f, 1.0f);

	// ---- 摇杆感知门控 ----
	// 飞手主动打杆时，applyLevel 会"压低"倾角估计，导致角速率 PID I 项过度积累，
	// 松杆后 I 项释放推着飞机继续朝打杆方向漂移。
	// 解决方案：摇杆偏转量越大，dynamicWeight 越小，在 levelGateThreshold 处归零，
	// 完全断开与 I 项的正反馈耦合；松杆后自动恢复，悬停抑漂能力不受影响。
	float stickDeflection = max(abs(controlRoll), abs(controlPitch));
	float stickGate = constrain(1.0f - stickDeflection / levelGateThreshold, 0.0f, 1.0f);
	dynamicWeight *= stickGate;
	// ------------------------------------

	Vector correction = Vector::rotationVectorBetween(Vector(0, 0, 1), up) * dynamicWeight;
	attitude = Quaternion::rotate(attitude, Quaternion::fromRotationVector(correction));
}
