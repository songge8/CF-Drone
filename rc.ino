// 使用RC接收器
// Work with the RC receiver

#include <SBUS.h>
#include "util.h"
#include "board_config.h"

SBUS rc(BOARD_RC_SERIAL); // Serial2 on ESP32, Serial1 on ESP32-C3
int rcRxPin = BOARD_RC_RX_PIN; // SBUS RX 引脚，-1 表示禁用 RC，可通过参数 RC_RX_PIN 配置
uint16_t channels[16]; // raw rc channels
float controlTime; // time of the last controls update
float channelZero[16]; // calibration zero values
float channelMax[16]; // calibration max values

// Channels mapping (using float to store in parameters):
float rollChannel = NAN, pitchChannel = NAN, throttleChannel = NAN, yawChannel = NAN, modeChannel = NAN;

void setupRC() {
	if (rcRxPin < 0) return; // Pin 未配置则跳过 RC 初始化
	print("Setup RC\n");
	rc.begin(rcRxPin);
}

bool readRC() {
	if (rcRxPin < 0) return false; // RC 未启用
	if (rc.read()) {
		SBUSData data = rc.data();
		for (int i = 0; i < 16; i++) channels[i] = data.ch[i]; // copy channels data
		normalizeRC();
		controlTime = t;
		return true;
	}
	return false;
}

void normalizeRC() {
	float controls[16];
	for (int i = 0; i < 16; i++) {
		controls[i] = mapf(channels[i], channelZero[i], channelMax[i], 0, 1);
	}
	// Update control values
	controlRoll = rollChannel >= 0 ? controls[(int)rollChannel] : NAN;
	controlPitch = pitchChannel >= 0 ? controls[(int)pitchChannel] : NAN;
	controlYaw = yawChannel >= 0 ? controls[(int)yawChannel] : NAN;
	controlThrottle = throttleChannel >= 0 ? controls[(int)throttleChannel] : NAN;
	controlMode = modeChannel >= 0 ? controls[(int)modeChannel] : NAN;
}

void calibrateRC() {
	uint16_t zero[16];
	uint16_t center[16];
	uint16_t max[16];
	print("1/8 校准遥控,所有摇杆归中位置[3秒]\n");
	pause(8);
	calibrateRCChannel(NULL, zero, zero, "2/8 左摇杆:向下,右摇杆:居中[3秒]\n...     ...\n...     .o.\n.o.     ...\n");
	calibrateRCChannel(NULL, center, center, "3/8 左摇杆:居中,右摇杆:居中[3秒]\n...     ...\n.o.     .o.\n...     ...\n");
	calibrateRCChannel(&throttleChannel, zero, max, "4/8 油门通道识别,左摇杆:向上推到底(油门最大),右摇杆：居中[3秒]\n.o.     ...\n...     .o.\n...     ...\n");
	calibrateRCChannel(&yawChannel, center, max, "5/8 偏航通道识别,左摇杆:向右推到底(偏航右转)右摇杆:居中[3秒]\n...     ...\n..o     .o.\n...     ...\n");
	calibrateRCChannel(&pitchChannel, zero, max, "6/8 俯仰通道识别,左摇杆:向下推到底,右摇杆:向上推到底(俯仰前进)[3秒]\n...     .o.\n...     ...\n.o.     ...\n");
	calibrateRCChannel(&rollChannel, zero, max, "7/8 横滚通道识别,左摇杆:向下推到底,右摇杆:向右推到底(横滚右转)[3秒]\n...     ...\n...     ..o\n.o.     ...\n");
	calibrateRCChannel(&modeChannel, zero, max, "8/8 模式通道识别,先将解锁开关拨回锁定位置,然后将模式开关拨到最高档位(如手动模式)[3秒]\n");
	printRCCalibration();
}

//第1步：初始化位置,操作：
// ✓ 所有摇杆归中位置
// ✓ 所有开关拨到默认位置（通常是锁定/中间位置）
// ✓ 保持3秒不动
//第2步：基础摇杆移动,操作：
// ✓ 左摇杆：向下
// ✓ 右摇杆：居中
// ✓ 保持3秒
//第3步：摇杆居中,操作：
// ✓ 左摇杆：居中
// ✓ 右摇杆：居中  
// ✓ 保持3秒
//第4步：油门通道识别,操作：
// ✓ 左摇杆：向上推到底（油门最大）
// ✓ 右摇杆：居中
// ✓ 保持3秒
//→ 系统自动识别油门通道
//第5步：偏航通道识别,操作：
// ✓ 左摇杆：向右推到底（偏航右转）
// ✓ 右摇杆：居中
// ✓ 保持3秒
// → 系统自动识别偏航通道
//第6步：俯仰通道识别,操作：
// ✓ 左摇杆：向下推到底
// ✓ 右摇杆：向上推到底（俯仰前进）
// ✓ 保持3秒
// → 系统自动识别俯仰通道
//第7步：横滚通道识别,操作：
// ✓ 左摇杆：向下推到底
// ✓ 右摇杆：向右推到底（横滚右转）
// ✓ 保持3秒
// → 系统自动识别横滚通道
//第8步：模式通道识别,操作：
// ✓ 先将解锁开关拨回锁定位置
// ✓ 然后将模式开关拨到最高档位（如手动模式）
// ✓ 保持3秒
// → 系统自动识别模式通道

void calibrateRCChannel(float *channel, uint16_t in[16], uint16_t out[16], const char *str) {
	print("%s", str);
	pause(8);
	for (int i = 0; i < 30; i++) readRC(); // try update 30 times max
	memcpy(out, channels, sizeof(channels));

	if (channel == NULL) return; // no channel to calibrate

	// Find channel that changed the most between in and out
	int ch = -1, diff = 0;
	for (int i = 0; i < 16; i++) {
		if (abs(out[i] - in[i]) > diff) {
			ch = i;
			diff = abs(out[i] - in[i]);
		}
	}
	if (ch >= 0 && diff > 10) { // difference threshold is 10
		*channel = ch;
		channelZero[ch] = in[ch];
		channelMax[ch] = out[ch];
	} else {
		*channel = NAN;
	}
}

void printRCCalibration() {
	print("Control   Ch     Zero   Max\n");
	print("Roll      %-7g%-7g%-7g\n", rollChannel, rollChannel >= 0 ? channelZero[(int)rollChannel] : NAN, rollChannel >= 0 ? channelMax[(int)rollChannel] : NAN);
	print("Pitch     %-7g%-7g%-7g\n", pitchChannel, pitchChannel >= 0 ? channelZero[(int)pitchChannel] : NAN, pitchChannel >= 0 ? channelMax[(int)pitchChannel] : NAN);
	print("Yaw       %-7g%-7g%-7g\n", yawChannel, yawChannel >= 0 ? channelZero[(int)yawChannel] : NAN, yawChannel >= 0 ? channelMax[(int)yawChannel] : NAN);
	print("Throttle  %-7g%-7g%-7g\n", throttleChannel, throttleChannel >= 0 ? channelZero[(int)throttleChannel] : NAN, throttleChannel >= 0 ? channelMax[(int)throttleChannel] : NAN);
	print("Mode      %-7g%-7g%-7g\n", modeChannel, modeChannel >= 0 ? channelZero[(int)modeChannel] : NAN, modeChannel >= 0 ? channelMax[(int)modeChannel] : NAN);
}
