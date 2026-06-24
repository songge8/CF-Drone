// 陀螺仪加速度计相关设置
// Work with the IMU sensor

#include <SPI.h>
#include "drivers/MPU9250.h"
#include "vector.h"
#include "lpf.h"
#include "util.h"
#include "board_config.h"

MPU9250 imu(SPI, BOARD_SPI_CS);

bool imuOK = false; // IMU 初始化是否成功；false 时禁止解锁，readIMU() 跳过等待

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
	SPI.begin(BOARD_SPI_SCK, BOARD_SPI_MISO, BOARD_SPI_MOSI, BOARD_SPI_CS);
	if (!imu.begin()) {
		print("⚠ IMU初始化失败！请检查 IMU 硬件连接！\n");
		return; // 不调用 configureIMU()，不启动定时器中断，imuOK 保持 false
	}
	imuOK = true;
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
	if (!imuOK) return; // IMU 故障时跳过，gyro/acc 保持零值，主循环继续运行
	imu.waitForData();
	imu.getGyro(gyro.x, gyro.y, gyro.z);
	imu.getAccel(acc.x, acc.y, acc.z);
	calibrateGyroOnce();
	// apply scale and bias
	acc = (acc - accBias) / accScale;
	gyro = gyro - gyroBias;
	// rotate to body frame using imuRotation Euler angles
	// 旋转四元数缓存：imuRotation 为运行时常量（仅参数变更时改变），
	// 缓存后每帧仅做 3 次浮点比较，消除原先每帧 6 次 sinf/cosf 调用。
	// 参数更新后首次 readIMU() 调用自动重建缓存，延迟仅 1 帧（~1 ms）。
	static Quaternion _imuRotQuat;
	static float _cachedRotX = NAN, _cachedRotY = NAN, _cachedRotZ = NAN;
	if (imuRotation.x != _cachedRotX || imuRotation.y != _cachedRotY || imuRotation.z != _cachedRotZ) {
		_imuRotQuat = Quaternion::fromEuler(imuRotation).inversed();
		_cachedRotX = imuRotation.x;
		_cachedRotY = imuRotation.y;
		_cachedRotZ = imuRotation.z;
	}
	acc  = Quaternion::rotateVector(acc,  _imuRotQuat);
	gyro = Quaternion::rotateVector(gyro, _imuRotQuat);
}

void calibrateGyroOnce() {
	static Delay landedDelay(2);
	if (!landedDelay.update(landed)) return; // calibrate only if definitely stationary

	gyroBias = gyroBiasFilter.update(gyro);
}

void calibrateAccel() {
	print("校准陀螺仪加速计 Calibrating accelerometer\n");
	imu.setAccelRange(imu.ACCEL_RANGE_2G); // the most sensitive mode

	print("1/6 水平放置：机头朝前（正常飞行方向），底部朝下水平放置在平坦表面，确保完全水平无倾斜。保持不动，8秒后开始校准；\n");
	pause(8);
	calibrateAccelOnce();
	print("水平校准完成。请继续。\n");
	pause(1);
	print("2/6 机头朝上：保持机头朝前,将机头指向天空，尾部朝下并接触支撑面，与地面垂直。保持不动，8秒后开始校准；\n");
	pause(8);
	calibrateAccelOnce();
	print("机头向上校准完成。请继续。\n");
	pause(1);
	print("3/6 机头朝下：保持机头朝前,将机头指向地面并接触支撑面，尾部朝上，与地面垂直。保持不动，8秒后开始校准；\n");
	pause(8);
	calibrateAccelOnce();
	print("机头向下校准完成。请继续。\n");
	pause(1);
	print("4/6 右侧朝下：保持机头朝前,将飞行器右侧朝下并接触支撑面，左侧机臂朝上，与地面垂直。保持不动，8秒后开始校准；\n");
	pause(8);
	calibrateAccelOnce();
	print("右侧向下校准完成。请继续。\n");
	pause(1);
	print("5/6 左侧朝下：保持机头朝前,将飞行器左侧朝下并接触支撑面，右侧机臂朝上，与地面垂直。保持不动，8秒后开始校准；\n");
	pause(8);
	calibrateAccelOnce();
	print("左侧向下校准完成。请继续。\n");
	pause(1);
	print("6/6 倒置放置：保持机头朝前,将飞行器完全翻转，顶部朝下、底部朝上，整体呈水平倒置状态。保持不动，8秒后开始校准；\n");
	pause(8);
	calibrateAccelOnce();

	printIMUCalibration();
	print("✓全部校准完成！将机身放正，执行ps命令查看校准结果。\n");
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

	#ifdef CONFIG_IDF_TARGET_ESP32C3
		// 每10帧主动让出1ms，防止单核ESP32-C3上WiFi任务因连续采样而饿死导致Beacon丢失
		if (i % 10 == 0) delay(1);
	#endif
	
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
	print("gyro: %f %f %f\n", gyro.x, gyro.y, gyro.z);
	print("acc: %f %f %f\n", acc.x, acc.y, acc.z);
	imu.waitForData();
	Vector rawGyro, rawAcc;
	imu.getGyro(rawGyro.x, rawGyro.y, rawGyro.z);
	imu.getAccel(rawAcc.x, rawAcc.y, rawAcc.z);
	print("raw gyro: %f %f %f\n", rawGyro.x, rawGyro.y, rawGyro.z);
	print("raw acc: %f %f %f\n", rawAcc.x, rawAcc.y, rawAcc.z);
}
