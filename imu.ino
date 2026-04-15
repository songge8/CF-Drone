// 陀螺仪加速度计相关设置
// Work with the IMU sensor

#include <SPI.h>
#include <FlixPeriph.h>
#include "vector.h"
#include "lpf.h"
#include "util.h"

MPU9250 imu(SPI);

// IMU 安装方向（欧拉角，单位 rad）。默认值 (0, 0, -PI/2) 对应本 PCB 的安装方式：
// 芯片正面朝上，X 丝印→飞行器右侧，Y 丝印→飞行器前方 → 转换公式 Vector(data.y, -data.x, data.z)
// 如需适配其他安装方向，修改此值并执行 `preset` 重置参数存储。
Vector imuRotation(0, 0, -PI / 2);

Vector accBias;
Vector accScale(1, 1, 1);
Vector gyroBias;
LowPassFilter<Vector> gyroBiasFilter(0.001); // 陀螺仪偏置低通估计滤波器

void setupIMU() {
	print("Setup IMU\n");
	imu.begin();
	configureIMU();
}

void configureIMU() {
	imu.setAccelRange(imu.ACCEL_RANGE_4G);
	imu.setGyroRange(imu.GYRO_RANGE_2000DPS);
	imu.setDLPF(imu.DLPF_MAX);
	imu.setRate(imu.RATE_1KHZ_APPROX);
	imu.setupInterrupt();
}

void readIMU() {
	imu.waitForData();
	imu.getGyro(gyro.x, gyro.y, gyro.z);
	imu.getAccel(acc.x, acc.y, acc.z);
	calibrateGyroOnce();
	// apply scale and bias
	acc = (acc - accBias) / accScale;
	gyro = gyro - gyroBias;
	// rotate to body frame using imuRotation Euler angles
	Quaternion rotation = Quaternion::fromEuler(imuRotation);
	acc  = Quaternion::rotateVector(acc,  rotation.inversed());
	gyro = Quaternion::rotateVector(gyro, rotation.inversed());
}

void calibrateGyroOnce() {
	static Delay landedDelay(2);
	if (!landedDelay.update(landed)) return; // calibrate only if definitely stationary

	gyroBias = gyroBiasFilter.update(gyro);
}

void calibrateAccel() {
	print("校准陀螺仪加速计 Calibrating accelerometer\n");
	imu.setAccelRange(imu.ACCEL_RANGE_2G); // the most sensitive mode

	print("1/6 水平放置：机头朝前（正常飞行方向），底部朝下水平放置在平坦表面，确保完全水平无倾斜。保持不动，8秒后开始校准\n");
	pause(8);
	calibrateAccelOnce();
	print("2/6 机头朝上：保持机头朝前,将飞行器前端抬起约90°,使机头指向天空，尾部接触支撑面。保持不动，8秒后开始校准\n");
	pause(8);
	calibrateAccelOnce();
	print("3/6 机头朝下：保持机头朝前,将飞行器前端下压约90°,使机头指向地面，尾部朝上。保持不动，8秒后开始校准\n");
	pause(8);
	calibrateAccelOnce();
	print("4/6 右侧朝下：保持机头朝前,将飞行器向右侧倾斜90°,右侧机臂垂直向下,左侧机臂朝上。保持不动，8秒后开始校准\n");
	pause(8);
	calibrateAccelOnce();
	print("5/6 左侧朝下：保持机头朝前,与右侧相反，左侧机臂垂直向下，右侧朝上。保持不动，8秒后开始校准\n");
	pause(8);
	calibrateAccelOnce();
	print("6/6 倒置放置：保持机头朝前,将飞行器完全翻转，顶部朝下、底部朝上，整体呈水平倒置状态。保持不动，8秒后开始校准\n");
	pause(8);
	calibrateAccelOnce();

	printIMUCalibration();
	print("✓校准完成 Calibration done!\n");
	configureIMU();
}

void calibrateAccelOnce() {
	const int samples = 1000;
	static Vector accMax(-INFINITY, -INFINITY, -INFINITY);
	static Vector accMin(INFINITY, INFINITY, INFINITY);

	// Compute the average of the accelerometer readings
	acc = Vector(0, 0, 0);
	for (int i = 0; i < samples; i++) {
		imu.waitForData();
		Vector sample;
		imu.getAccel(sample.x, sample.y, sample.z);
		acc = acc + sample;
	}
	acc = acc / samples;

	// Update the maximum and minimum values
	if (acc.x > accMax.x) accMax.x = acc.x;
	if (acc.y > accMax.y) accMax.y = acc.y;
	if (acc.z > accMax.z) accMax.z = acc.z;
	if (acc.x < accMin.x) accMin.x = acc.x;
	if (acc.y < accMin.y) accMin.y = acc.y;
	if (acc.z < accMin.z) accMin.z = acc.z;
	// Compute scale and bias
	accScale = (accMax - accMin) / 2 / ONE_G;
	accBias = (accMax + accMin) / 2;
}

void printIMUCalibration() {
	print("gyro bias: %f %f %f\n", gyroBias.x, gyroBias.y, gyroBias.z);
	print("accel bias: %f %f %f\n", accBias.x, accBias.y, accBias.z);
	print("accel scale: %f %f %f\n", accScale.x, accScale.y, accScale.z);
}

void printIMUInfo() {
	imu.status() ? print("status: ERROR %d\n", imu.status()) : print("status: OK\n");
	print("model: %s\n", imu.getModel());
	print("who am I: 0x%02X\n", imu.whoAmI());
	print("rate: %.0f\n", loopRate);
	print("gyro: %f %f %f\n", rates.x, rates.y, rates.z);
	print("acc: %f %f %f\n", acc.x, acc.y, acc.z);
	imu.waitForData();
	Vector rawGyro, rawAcc;
	imu.getGyro(rawGyro.x, rawGyro.y, rawGyro.z);
	imu.getAccel(rawAcc.x, rawAcc.y, rawAcc.z);
	print("raw gyro: %f %f %f\n", rawGyro.x, rawGyro.y, rawGyro.z);
	print("raw acc: %f %f %f\n", rawAcc.x, rawAcc.y, rawAcc.z);
}
