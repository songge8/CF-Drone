// Copyright (c) 2023 Oleg Kalachev <okalachev@gmail.com>
// Repository: https://github.com/okalachev/flix
//实用函数
// Utility functions

#pragma once

#include <math.h>
#ifdef CONFIG_IDF_TARGET_ESP32
#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
#include "esp_private/brownout.h"
#endif

const float ONE_G = 9.80665;
extern float t;

float mapf(float x, float in_min, float in_max, float out_min, float out_max) {
	return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

bool invalid(float x) {
	return !isfinite(x);
}

bool valid(float x) {
	return isfinite(x);
}

// Wrap angle to [-PI, PI)
float wrapAngle(float angle) {
	angle = fmodf(angle, 2 * PI);
	if (angle > PI) {
		angle -= 2 * PI;
	} else if (angle < -PI) {
		angle += 2 * PI;
	}
	return angle;
}

// Disable reset on low voltage
void disableBrownOut() {
#ifdef CONFIG_IDF_TARGET_ESP32
	REG_CLR_BIT(RTC_CNTL_BROWN_OUT_REG, RTC_CNTL_BROWN_OUT_ENA);
#elif defined(CONFIG_IDF_TARGET_ESP32S3)
	esp_brownout_disable();
#endif
	// ESP32-C3 欠压复位阈值通过 menuconfig 配置，无需代码操作
}

// Trim and split string by spaces
void splitString(String& str, String& token0, String& token1, String& token2) {
	str.trim();
	char chars[str.length() + 1];
	str.toCharArray(chars, str.length() + 1);
	token0 = strtok(chars, " ");
	token1 = strtok(NULL, " "); // String(NULL) creates empty string
	token2 = strtok(NULL, "");
}

// Rate limiter
class Rate {
public:
	float rate;
	float last = 0;
	Rate(float rate) : rate(rate) {}

	operator bool() {
		if (t - last >= 1 / rate) {
			last = t;
			return true;
		}
		return false;
	}
};

// Delay filter for boolean signals - ensures the signal is on for at least 'delay' seconds
class Delay {
public:
	float delay;
	float start = NAN;
	Delay(float delay) : delay(delay) {}

	bool update(bool on) {
		if (!on) {
			start = NAN;
			return false;
		} else if (isnan(start)) {
			start = t;
		}
		return t - start >= delay;
	}
};
