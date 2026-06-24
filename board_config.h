#pragma once

// 板级硬件配置
// Board-level hardware configuration
// 通过条件编译自动区分 ESP32、ESP32-C3 与 ESP32-S3，无需手动修改

#ifdef CONFIG_IDF_TARGET_ESP32C3
// ---------------------- ESP32C3 ------------------------- 

// ---- 电机引脚（数组顺序：RL, RR, FR, FL，与 motors.ino 中常量定义一致）----
// GPIO0/1/3/10 均无内置上拉，无 Strapping 功能（C3 的 Strapping 引脚仅 GPIO2/8/9）
#define BOARD_MOTOR_PINS   {3, 10, 0, 1}    // RL=GPIO3, RR=GPIO10, FR=GPIO0, FL=GPIO1

// ---- 电池 ADC ----
// GPIO2 为 Strapping 引脚（内置弱上拉），但 ADC 输入模式下上拉电阻不影响测量精度
// 启动后 Strapping 采样完成，analogRead() 调用时引脚为正常输入态
#define BOARD_VBAT_ADC_PIN 2               // ADC1_CH2（GPIO2）

// ---- RC 串口（CRSF/ELRS 默认；SBUS 可通过参数 RC_PROTOCOL=0 切换）----
// GPIO9 作 CRSF RX，标准电平，无需反相器
#define BOARD_RC_SERIAL    Serial1
#define BOARD_RC_RX_PIN    9
#define BOARD_RC_TX_PIN    -1          // 遥测回传预留，-1=暂不启用
#define BOARD_RC_PROTOCOL  1           // 0=SBUS, 1=CRSF(ELRS)
#define BOARD_RC_BAUD      420000      // CRSF 标准波特率默认420000；SBUS为100000

// ---- IMU SPI（使用文档 / Arduino pins_arduino.h 默认引脚）----
// 与 Arduino ESP32-C3 库 SPI 默认定义完全一致，SPI.begin() 可无参调用
#define BOARD_SPI_SCK      4               // 文档 SCK  = GPIO4
#define BOARD_SPI_MISO     5               // 文档 MISO = GPIO5
#define BOARD_SPI_MOSI     6               // 文档 MOSI = GPIO6
#define BOARD_SPI_CS       7               // 文档 SS   = GPIO7

// ---- LED：GPIO8，蓝色状态灯，低电平有效 ----
#define BOARD_LED_ENABLED  1
#define BOARD_LED_PIN      8
#define BOARD_LED_INVERTED 1               // 低电平点亮

// ---- 性能与资源配置（C3 轻量化）----
#define BOARD_VBAT_ADC_SAMPLES       8    // ADC采样次数 减少 ADC 阻塞时间，ESP32/S3 默认 16
#define BOARD_LOG_DURATION           4    // 日志 4 秒缓冲，节省约 22 KB RAM，ESP32/S3 默认 8 秒
#define BOARD_CONSOLE_LINES          20   // 控制台行数，节省约 7 KB RAM，ESP32/S3 默认 50
#define BOARD_CONSOLE_LINE_LEN       160  // 每行字符数，ESP32/S3 默认 240
#define BOARD_MAVLINK_TELEM_FAST_HZ  5    // MAVLink 快速遥测降速，降低 WiFi 占用，ESP32/S3 默认 10 Hz
#define BOARD_WIFI_ENABLED           0    // C3 单核：WiFi 与飞控共享 CPU，关闭可彻底释放资源给飞控
#define BOARD_WEB_RC_ENABLED         0    // Web 遥控器依赖 WiFi，随 BOARD_WIFI_ENABLED 同步关闭

#elif defined(CONFIG_IDF_TARGET_ESP32S3)
// ---------------------- ESP32S3 -------------------------

// ---- 电机引脚：GPIO4-7，远离 FSPI 区（GPIO10-13），无 Strapping 约束 ----
#define BOARD_MOTOR_PINS   {4, 5, 6, 7}    // RL=GPIO4, RR=GPIO5, FR=GPIO6, FL=GPIO7

// ---- 电池 ADC：GPIO1（ADC1_CH0，S3 的 ADC1 范围为 GPIO1-10）----
#define BOARD_VBAT_ADC_PIN 1

// ---- RC 串口（CRSF/ELRS 默认）：GPIO8，与 FSPI/UART0 无冲突 ----
#define BOARD_RC_SERIAL    Serial2
#define BOARD_RC_RX_PIN    8
#define BOARD_RC_TX_PIN    -1
#define BOARD_RC_PROTOCOL  1
#define BOARD_RC_BAUD      420000

// ---- IMU SPI：使用 ESP32-S3 FSPI 默认管脚 ----
// SCK=12（默认），MISO=13（默认），MOSI=11（默认），CS=10（默认 SS）
#define BOARD_SPI_SCK      12
#define BOARD_SPI_MISO     13
#define BOARD_SPI_MOSI     11
#define BOARD_SPI_CS       10

// ---- LED：新 PCB 接 GPIO2 普通 LED，驱动方式与 ESP32 相同 ----
#define BOARD_LED_ENABLED  1
#define BOARD_LED_PIN      2
#define BOARD_LED_INVERTED 0

// ---- 性能与资源配置（S3 标准）----
#define BOARD_VBAT_ADC_SAMPLES       16 // ADC采样次数
#define BOARD_LOG_DURATION           8  // 日志缓冲秒数
#define BOARD_CONSOLE_LINES          50  // 控制台行数
#define BOARD_CONSOLE_LINE_LEN       240  // 每行字符数
#define BOARD_MAVLINK_TELEM_FAST_HZ  10  // MAVLink 快速遥测降速
#define BOARD_WIFI_ENABLED           1
#define BOARD_WEB_RC_ENABLED         1

#else  // ---- ESP32 默认配置 ----
// ---------------------- 默认ESP32 -------------------------

#define BOARD_MOTOR_PINS   {12, 13, 15, 14}  // 电机引脚 RL=GPIO12, RR=GPIO13, FR=GPIO15, FL=GPIO14
#define BOARD_VBAT_ADC_PIN 36              // 电池电压 ADC 引脚（GPIO36，仅输入）
#define BOARD_RC_SERIAL    Serial2         // RC 串口（CRSF/ELRS 默认）
#define BOARD_RC_RX_PIN    4               // RC RX 引脚（GPIO4）
#define BOARD_RC_TX_PIN    -1              // 遥测回传预留，-1=暂不启用
#define BOARD_RC_PROTOCOL  1               // 0=SBUS, 1=CRSF(ELRS)
#define BOARD_RC_BAUD      420000          // CRSF 标准波特率默认420000；SBUS为100000
#define BOARD_SPI_SCK      18              // SPI 时钟（GPIO18）
#define BOARD_SPI_MISO     19              // SPI 主入从出（GPIO19）
#define BOARD_SPI_MOSI     23              // SPI 主出从入（GPIO23）
#define BOARD_SPI_CS       5               // SPI 片选（GPIO5）
#define BOARD_LED_ENABLED  1               // 启用板载 LED
#define BOARD_LED_PIN      2               // LED 引脚（GPIO2）
#define BOARD_LED_INVERTED 0               // 高电平点亮

// ---- 性能与资源配置（ESP32 标准）----
#define BOARD_VBAT_ADC_SAMPLES       16 // ADC采样次数
#define BOARD_LOG_DURATION           8  // 日志缓冲秒数
#define BOARD_CONSOLE_LINES          50  // 控制台行数
#define BOARD_CONSOLE_LINE_LEN       240  // 每行字符数
#define BOARD_MAVLINK_TELEM_FAST_HZ  10  // MAVLink 快速遥测降速
#define BOARD_WIFI_ENABLED           1  // WIFI开关
#define BOARD_WEB_RC_ENABLED         1  // Web遥控器开关

#endif

// ---- WiFi / Web RC 开关充底（未知芯片默认全开）----
#if !defined(BOARD_WIFI_ENABLED)
#define BOARD_WIFI_ENABLED   1
#endif
#if !defined(BOARD_WEB_RC_ENABLED)
#define BOARD_WEB_RC_ENABLED 1
#endif
