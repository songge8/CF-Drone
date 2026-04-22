#pragma once

// 板级硬件配置
// Board-level hardware configuration
// 通过条件编译自动区分 ESP32 与 ESP32-C3，无需手动修改

#ifdef CONFIG_IDF_TARGET_ESP32C3

// ---- 电机引脚（数组顺序：RL, RR, FR, FL，与 motors.ino 中常量定义一致）----
// FL=GPIO3（左前），RL=GPIO4（左后），RR=GPIO5（右后），FR=GPIO8（右前）
// GPIO8 为 Strapping pin（内置上拉），无需外接电阻，PWM 输出正常工作
#define BOARD_MOTOR_PINS   {4, 5, 8, 3}    // RL=GPIO4, RR=GPIO5, FR=GPIO8, FL=GPIO3

// ---- 电池 ADC ----
#define BOARD_VBAT_ADC_PIN 1               // ADC1_CH1（ESP32-C3 ADC 仅 GPIO0~4）

// ---- RC 串口（SBUS）----
// ESP32-C3 无 Serial2，使用 Serial1；引脚可重映射至任意 GPIO
// GPIO9 为 Boot 按键（内置上拉），SBUS 信号由接收机驱动，不影响正常使用
#define BOARD_RC_SERIAL    Serial1
#define BOARD_RC_RX_PIN    9

// ---- IMU SPI（全部使用 ESP32-C3 硬件默认引脚）----
// SCK=6（默认），MISO=2（默认），MOSI=7（默认），CS=10（默认 SS）
#define BOARD_SPI_SCK      6
#define BOARD_SPI_MISO     2
#define BOARD_SPI_MOSI     7
#define BOARD_SPI_CS       10

// ---- LED：ESP32-C3 Mini 无板载 LED，禁用以节省引脚 ----
#define BOARD_LED_ENABLED  0

#else  // ---- ESP32 默认配置 ----

#define BOARD_MOTOR_PINS   {12, 13, 15, 14}  // RL=GPIO12, RR=GPIO13, FR=GPIO15, FL=GPIO14
#define BOARD_VBAT_ADC_PIN 36
#define BOARD_RC_SERIAL    Serial2
#define BOARD_RC_RX_PIN    16
#define BOARD_SPI_SCK      18
#define BOARD_SPI_MISO     19
#define BOARD_SPI_MOSI     23
#define BOARD_SPI_CS       5
#define BOARD_LED_ENABLED  1

#endif
