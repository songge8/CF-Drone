// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
// Work with the IMU sensor

#include <SPI.h>
#include <FlixPeriph.h>
#include "vector.h"
#include "lpf.h"
#include "util.h"

MPU9250 imu(SPI);

Vector accBias;
Vector accScale(1, 1, 1);
Vector gyroBias;

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
	// rotate
	rotateIMU(acc);
	rotateIMU(gyro);
}

// 在飞控中，我们通常使用FLU（Front-Left-Up）坐标系：
  //- X轴：向前（Front，即飞行器前进方向）
  //- Y轴：向左（Left）
  //- Z轴：向上（Up）
  //我的样机MPU6500安装方式是：
  //- 芯片丝印朝上（即芯片正面朝上）
  //- X丝印指向飞行器右侧
  //- Y丝印指向飞行器前方
  //安装方式与FLU坐标系的对应关系是：
  //- 传感器Y轴(向前)对应FLU坐标系的X轴(向前）
  //- 传感器X轴(向右)对应FLU坐标系的Y轴的反方向(因为FLU的Y轴向左,而传感器X轴向右,所以需要取反）
  //- 传感器Z轴(向上)对应FLU坐标系的Z轴(向上),注意:芯片朝上安装,那么传感器的Z轴是朝上的,而FLU坐标系的Z轴也是朝上的,所以Z轴方向一致。
  //如果你的IMU模块与我制作的样机不同，那么需要设置IMU的指向！

void rotateIMU(Vector& data) {
	// Rotate from LFD（Left-Forward-Up） to FLU（Front-Left-Up）
  // PCB上IMU的安装 X=右, Y=前, Z=上 → 转换为 X=前, Y=左, Z=上
	// NOTE: In case of using other IMU orientation, change this line:
	data = Vector(data.y, -data.x, data.z);
	// Axes orientation for various boards: https://github.com/okalachev/flixperiph#imu-axes-orientation
}
//所有旋转情况的完整映射表:（我让AI写的，仅供参考，请物理验证）
//旋转角度	旋转方向	X轴最终方向	Y轴最终方向	转换公式
//0°	无旋转	右 →	前 ↑	Vector(data.y, -data.x, data.z) 我设计的PCB上丝印就是这个方向！
//90°	顺时针	下 ↓	右 →	Vector(-data.x, -data.y, data.z)
//180°	顺时针	左 ←	后 ↓	Vector(-data.y, data.x, data.z)
//270°	顺时针	上 ↑	左 ←	Vector(data.x, data.y, data.z)
//90°	逆时针	上 ↑	左 ←	Vector(data.x, data.y, data.z)
//270°	逆时针	下 ↓	右 →	Vector(-data.x, -data.y, data.z)

void calibrateGyroOnce() {
	static Delay landedDelay(2);
	if (!landedDelay.update(landed)) return; // calibrate only if definitely stationary

	static LowPassFilter<Vector> gyroBiasFilter(0.001);
	gyroBias = gyroBiasFilter.update(gyro);
}

void calibrateAccel() {
	print("校准陀螺仪加速计Calibrating accelerometer\n");
	imu.setAccelRange(imu.ACCEL_RANGE_2G); // the most sensitive mode

	print("1/6 水平放置[8 sec]将飞行器机头朝前（正常飞行方向），底部朝下水平放置在平坦表面，确保完全水平无倾斜\n");
	pause(8);
	calibrateAccelOnce();
	print("2/6 机头朝上[8 sec]保持机头朝前,将飞行器前端抬起约90°,使机头指向天空，尾部接触支撑面\n");
	pause(8);
	calibrateAccelOnce();
	print("3/6 机头朝下[8 sec]机头朝前,将飞行器前端下压约90°,使机头指向地面，尾部朝上\n");
	pause(8);
	calibrateAccelOnce();
	print("4/6 右侧朝下[8 sec]机头朝前,将飞行器整体向右侧倾斜,直至右侧机臂垂直向下,左侧机臂朝上,机身呈90°侧倾\n");
	pause(8);
	calibrateAccelOnce();
	print("5/6 左侧朝下[8 sec]与右侧相反，机头朝前时左侧机臂垂直向下，右侧朝上，保持机身稳定无晃动\n");
	pause(8);
	calibrateAccelOnce();
	print("6/6 倒置放置[8 sec]将飞行器完全翻转，顶部朝下、底部朝上，机头方向保持不变，整体呈水平倒置状态\n");
	pause(8);
	calibrateAccelOnce();

	printIMUCalibration();
	print("✓校准完成Calibration done!\n");
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
