/*
* Brian R Taylor
* brian.taylor@bolderflight.com
*
* Copyright (c) 2022 Bolder Flight Systems Inc
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the “Software”), to
* deal in the Software without restriction, including without limitation the
* rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
* sell copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
* IN THE SOFTWARE.
*/

#ifndef SRC_SBUS_H_
#define SRC_SBUS_H_

#if defined(ARDUINO)
#include <Arduino.h>
#else
#include <cstddef>
#include <cstdint>
#include "core/core.h"
#endif

struct SBUSData {
	bool lost_frame;
	bool failsafe;
	bool ch17, ch18;
	static constexpr int8_t NUM_CH = 16;
	int16_t ch[NUM_CH];
};

class SBUS {
 public:
	#if defined(ESP32)
	SBUS(HardwareSerial& bus, const int rxpin, const int txpin, const bool inv = true, const bool fast = false) : uart_(&bus), inv_(inv), fast_(fast), rxpin_(rxpin), txpin_(txpin) {}
	#endif
	explicit SBUS(HardwareSerial& bus, const bool inv = true, const bool fast = false) : uart_(&bus), inv_(inv), fast_(fast) {}
	void begin();
	void begin(int rxpin, int txpin = -1, bool inv = true, bool fast = false);
	bool read();
	inline SBUSData data() const {return data_;}

 private:
	/* Communication */
	HardwareSerial *uart_;
	bool inv_ = true;
	bool fast_ = false;
	int8_t rxpin_ = -1, txpin_ = -1; // if pin < 0, default for the bus is used
	int32_t baud_ = 100000;
	/* Message len */
	static constexpr int8_t PAYLOAD_LEN_ = 23;
	static constexpr int8_t HEADER_LEN_ = 1;
	static constexpr int8_t FOOTER_LEN_ = 1;
	/* SBUS message defs */
	static constexpr int8_t NUM_SBUS_CH_ = 16;
	static constexpr uint8_t HEADER_ = 0x0F;
	static constexpr uint8_t FOOTER_ = 0x00;
	static constexpr uint8_t FOOTER2_ = 0x04;
	static constexpr uint8_t CH17_MASK_ = 0x01;
	static constexpr uint8_t CH18_MASK_ = 0x02;
	static constexpr uint8_t LOST_FRAME_MASK_ = 0x04;
	static constexpr uint8_t FAILSAFE_MASK_ = 0x08;
	/* Parsing state tracking */
	int8_t state_ = 0;
	uint8_t prev_byte_ = FOOTER_;
	uint8_t cur_byte_;
	/* Buffer for storing messages */
	uint8_t buf_[25];
	/* Data */
	bool new_data_;
	SBUSData data_;
	bool Parse();
};

class SBUSTx {
 public:
	#if defined(ESP32)
	SBUSTx(HardwareSerial& bus, const int rxpin, const int txpin, const bool inv = true, const bool fast = false) : uart_(&bus), inv_(inv), fast_(fast), rxpin_(rxpin), txpin_(txpin) {}
	#endif
	explicit SBUSTx(HardwareSerial& bus, const bool inv = true, const bool fast = false) : uart_(&bus), inv_(inv), fast_(fast) {}
	void begin();
	void begin(int txpin, int rxpin = -1, bool inv = true, bool fast = false);
	void write();
	inline void data(const SBUSData &data) {data_ = data;}
	inline SBUSData data() const {return data_;}

 private:
	/* Communication */
	HardwareSerial *uart_;
	bool inv_ = true;
	bool fast_ = false;
	int8_t rxpin_ = -1, txpin_ = -1; // if pin < 0, default for the bus is used
	int32_t baud_ = 100000;
	/* Message len */
	static constexpr int8_t BUF_LEN_ = 25;
	/* SBUS message defs */
	static constexpr int8_t NUM_SBUS_CH_ = 16;
	static constexpr uint8_t HEADER_ = 0x0F;
	static constexpr uint8_t FOOTER_ = 0x00;
	static constexpr uint8_t FOOTER2_ = 0x04;
	static constexpr uint8_t CH17_MASK_ = 0x01;
	static constexpr uint8_t CH18_MASK_ = 0x02;
	static constexpr uint8_t LOST_FRAME_MASK_ = 0x04;
	static constexpr uint8_t FAILSAFE_MASK_ = 0x08;
	/* Data */
	uint8_t buf_[BUF_LEN_];
	SBUSData data_;
};

#endif  // SRC_SBUS_H_
