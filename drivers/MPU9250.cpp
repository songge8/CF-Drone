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

#include "MPU9250.h"  // NOLINT
#if defined(ARDUINO)
#include <Arduino.h>
#include "Wire.h"
#include "SPI.h"
#else
#include <cstddef>
#include <cstdint>
#include <algorithm>
#include "core/core.h"
#endif

static const char *errorFmt = "Error: %d";

void MPU9250::config(TwoWire *i2c, const I2cAddr addr) {
  imu_.Config(i2c, static_cast<uint8_t>(addr));
}
void MPU9250::config(SPIClass *spi, const uint8_t cs) {
  imu_.Config(spi, cs);
}
bool MPU9250::begin() {
  setLogName("IMU");

  imu_.Begin();
  /* 1 MHz for config */
  spi_clock_ = SPI_CFG_CLOCK_;
  /* Select clock source to gyro */
  if (!WriteRegister(PWR_MGMNT_1_, CLKSEL_PLL_)) {
    log(errorFmt, status_ = 1);
    return false;
  }
  /* Enable I2C master mode */
  if (!WriteRegister(USER_CTRL_, I2C_MST_EN_)) {
    log(errorFmt, status_ = 2);
    return false;
  }
  /* Set the I2C bus speed to 400 kHz */
  if (!WriteRegister(I2C_MST_CTRL_, I2C_MST_CLK_)) {
    log(errorFmt, status_ = 3);
    return false;
  }
  /* Reset the MPU9250 */
  WriteRegister(PWR_MGMNT_1_, H_RESET_);
  /* Wait for MPU-9250 to come back up */
  delay(1);
  /* Check the WHO AM I byte */
  if (!ReadRegisters(WHOAMI_, sizeof(who_am_i_), &who_am_i_)) {
    log(errorFmt, status_ = 4);
    return false;
  }
  if (who_am_i_ == WHOAMI_MPU6500) {
    // disable AK8963 functions
    is_mpu6500_ = true;
    log("MPU-6500 detected");
  } else if (who_am_i_ == WHOAMI_MPU9250) {
    log("MPU-9250 detected");
  } else if (who_am_i_ == WHOAMI_MPU9255) {
    log("MPU-9255 detected");
  } else {
    status_ = 5;
    log("Unknown WHO_AM_I: 0x%02X", who_am_i_);
    return false;
  }
  /* Set AK8963 to power down */
  WriteAk8963Register(AK8963_CNTL1_, AK8963_PWR_DOWN_);
  /* Reset the AK8963 */
  WriteAk8963Register(AK8963_CNTL2_, AK8963_RESET_);
  /* Select clock source to gyro */
  if (!WriteRegister(PWR_MGMNT_1_, CLKSEL_PLL_)) {
    log(errorFmt, status_ = 6);
    return false;
  }
  /* Enable I2C master mode */
  if (!WriteRegister(USER_CTRL_, I2C_MST_EN_)) {
    log(errorFmt, status_ = 7);
    return false;
  }
  /* Set the I2C bus speed to 400 kHz */
  if (!WriteRegister(I2C_MST_CTRL_, I2C_MST_CLK_)) {
    log(errorFmt, status_ = 8);
    return false;
  }
  /* Check the AK8963 WHOAMI */
  if (!ReadAk8963Registers(AK8963_WHOAMI_, sizeof(who_am_i_ak8963_), &who_am_i_ak8963_)) {
    log(errorFmt, status_ = 9);
    return false;
  }
  if (!is_mpu6500_ && who_am_i_ak8963_ != WHOAMI_AK8963) {
    status_ = 10;
    log("Wrong AK8963 WHO_AM_I: 0x%02X", who_am_i_ak8963_);
    return false;
  }
  /* Get the magnetometer calibration */
  /* Set AK8963 to power down */
  if (!WriteAk8963Register(AK8963_CNTL1_, AK8963_PWR_DOWN_)) {
    log(errorFmt, status_ = 11);
    return false;
  }
  delay(100);  // long wait between AK8963 mode changes
  /* Set AK8963 to FUSE ROM access */
  if (!WriteAk8963Register(AK8963_CNTL1_, AK8963_FUSE_ROM_)) {
    log(errorFmt, status_ = 12);
    return false;
  }
  delay(100);  // long wait between AK8963 mode changes
  /* Read the AK8963 ASA registers and compute magnetometer scale factors */
  if (!ReadAk8963Registers(AK8963_ASA_, sizeof(asa_buff_), asa_buff_)) {
    log(errorFmt, status_ = 13);
    return false;
  }
  mag_scale_[0] = ((static_cast<float>(asa_buff_[0]) - 128.0f)
    / 256.0f + 1.0f) * 4912.0f / 32760.0f;
  mag_scale_[1] = ((static_cast<float>(asa_buff_[1]) - 128.0f)
    / 256.0f + 1.0f) * 4912.0f / 32760.0f;
  mag_scale_[2] = ((static_cast<float>(asa_buff_[2]) - 128.0f)
    / 256.0f + 1.0f) * 4912.0f / 32760.0f;
  /* Set AK8963 to power down */
  if (!WriteAk8963Register(AK8963_CNTL1_, AK8963_PWR_DOWN_)) {
    log(errorFmt, status_ = 14);
    return false;
  }
  /* Set AK8963 to 16 bit resolution, 100 Hz update rate */
  if (!WriteAk8963Register(AK8963_CNTL1_, AK8963_CNT_MEAS2_)) {
    log(errorFmt, status_ = 15);
    return false;
  }
  delay(100);  // long wait between AK8963 mode changes
  /* Select clock source to gyro */
  if (!WriteRegister(PWR_MGMNT_1_, CLKSEL_PLL_)) {
    log(errorFmt, status_ = 16);
    return false;
  }
  /* Set the accel range to 16G by default */
  if (!setAccelRange(ACCEL_RANGE_16G)) {
    log(errorFmt, status_ = 17);
    return false;
  }
  /* Set the gyro range to 2000DPS by default*/
  if (!setGyroRange(GYRO_RANGE_2000DPS)) {
    log(errorFmt, status_ = 18);
    return false;
  }
  /* Set the DLPF to 184HZ by default */
  if (!setDlpfBandwidth(DLPF_BANDWIDTH_184HZ)) {
    log(errorFmt, status_ = 19);
    return false;
  }
  /* Set the SRD to 0 by default */
  if (!setSrd(0)) {
    log(errorFmt, status_ = 20);
    return false;
  }
  return true;
}
bool MPU9250::enableDrdyInt() {
  spi_clock_ = SPI_CFG_CLOCK_;
  if (!WriteRegister(INT_PIN_CFG_, INT_PULSE_50US_)) {
    return false;
  }
  if (!WriteRegister(INT_ENABLE_, INT_RAW_RDY_EN_)) {
    return false;
  }
  return true;
}
bool MPU9250::disableDrdyInt() {
  spi_clock_ = SPI_CFG_CLOCK_;
  if (!WriteRegister(INT_ENABLE_, INT_DISABLE_)) {
    return false;
  }
  return true;
}
bool MPU9250::setAccelRange(const AccelRange range) {
  spi_clock_ = SPI_CFG_CLOCK_;
  /* Check input is valid and set requested range and scale */
  switch (range) {
    case ACCEL_RANGE_MIN:
    case ACCEL_RANGE_2G:
      requested_accel_range_ = 0x00;
      requested_accel_scale_ = 2.0f / 32767.5f;
      break;
    case ACCEL_RANGE_4G:
      requested_accel_range_ = 0x08;
      requested_accel_scale_ = 4.0f / 32767.5f;
      break;
    case ACCEL_RANGE_8G:
      requested_accel_range_ = 0x10;
      requested_accel_scale_ = 8.0f / 32767.5f;
      break;
    case ACCEL_RANGE_MAX:
    case ACCEL_RANGE_16G:
      requested_accel_range_ = 0x18;
      requested_accel_scale_ = 16.0f / 32767.5f;
      break;
    default:
      log("Unsupported accel range: %d", range);
      return false;
  }
  /* Try setting the requested range */
  if (!WriteRegister(ACCEL_CONFIG_, requested_accel_range_)) {
    log("Failed to set accel range");
    return false;
  }
  /* Update stored range and scale */
  accel_range_ = range;
  accel_scale_ = requested_accel_scale_;
  return true;
}
bool MPU9250::setGyroRange(const GyroRange range) {
  spi_clock_ = SPI_CFG_CLOCK_;
  /* Check input is valid and set requested range and scale */
  switch (range) {
    case GYRO_RANGE_MIN:
    case GYRO_RANGE_250DPS:
      requested_gyro_range_ = 0x00;
      requested_gyro_scale_ = 250.0f / 32767.5f;
      break;
    case GYRO_RANGE_500DPS:
      requested_gyro_range_ = 0x08;
      requested_gyro_scale_ = 500.0f / 32767.5f;
      break;
    case GYRO_RANGE_1000DPS:
      requested_gyro_range_ = 0x10;
      requested_gyro_scale_ = 1000.0f / 32767.5f;
      break;
    case GYRO_RANGE_MAX:
    case GYRO_RANGE_2000DPS:
      requested_gyro_range_ = 0x18;
      requested_gyro_scale_ = 2000.0f / 32767.5f;
      break;
    default:
      log("Unsupported gyro range: %d", range);
      return false;
  }
  /* Try setting the requested range */
  if (!WriteRegister(GYRO_CONFIG_, requested_gyro_range_)) {
    log("Failed to set gyro range");
    return false;
  }
  /* Update stored range and scale */
  gyro_range_ = range;
  gyro_scale_ = requested_gyro_scale_;
  return true;
}
bool MPU9250::setSrd(const uint8_t srd) {
  spi_clock_ = SPI_CFG_CLOCK_;
  /* Changing the SRD to allow us to set the magnetometer successfully */
  if (!WriteRegister(SMPLRT_DIV_, 19)) {
    return false;
  }
  /* Set the magnetometer sample rate */
  if (srd > 9) {
    /* Set AK8963 to power down */
    WriteAk8963Register(AK8963_CNTL1_, AK8963_PWR_DOWN_);
    delay(100);  // long wait between AK8963 mode changes
    /* Set AK8963 to 16 bit resolution, 8 Hz update rate */
    if (!WriteAk8963Register(AK8963_CNTL1_, AK8963_CNT_MEAS1_)) {
      return false;
    }
    delay(100);  // long wait between AK8963 mode changes
    if (!ReadAk8963Registers(AK8963_ST1_, sizeof(mag_data_), mag_data_)) {
      return false;
    }
  } else {
    /* Set AK8963 to power down */
    WriteAk8963Register(AK8963_CNTL1_, AK8963_PWR_DOWN_);
    delay(100);  // long wait between AK8963 mode changes
    /* Set AK8963 to 16 bit resolution, 100 Hz update rate */
    if (!WriteAk8963Register(AK8963_CNTL1_, AK8963_CNT_MEAS2_)) {
      return false;
    }
    delay(100);  // long wait between AK8963 mode changes
    if (!ReadAk8963Registers(AK8963_ST1_, sizeof(mag_data_), mag_data_)) {
      return false;
    }
  }
  /* Set the IMU sample rate */
  if (!WriteRegister(SMPLRT_DIV_, srd)) {
    return false;
  }
  srd_ = srd;
  return true;
}
bool MPU9250::setDLPF(const DLPF dlpf) {
  DlpfBandwidth val;
  switch (dlpf) {
    case DLPF_OFF:
      log("Disabling DLPF is not yet implemented");
      return false;
    case DLPF_MAX:
      val = DLPF_BANDWIDTH_184HZ;
      break;
    case DLPF_100HZ_APPROX:
      val = DLPF_BANDWIDTH_92HZ;
      break;
    case DLPF_50HZ_APPROX:
      val = DLPF_BANDWIDTH_41HZ;
      break;
    case DLPF_MIN:
    case DLPF_5HZ_APPROX:
      val = DLPF_BANDWIDTH_5HZ;
      break;
  }
  return setDlpfBandwidth(val);
}
bool MPU9250::setDlpfBandwidth(const DlpfBandwidth dlpf) {
  spi_clock_ = SPI_CFG_CLOCK_;
  /* Check input is valid and set requested dlpf */
  switch (dlpf) {
    case DLPF_BANDWIDTH_184HZ: {
      requested_dlpf_ = dlpf;
      break;
    }
    case DLPF_BANDWIDTH_92HZ: {
      requested_dlpf_ = dlpf;
      break;
    }
    case DLPF_BANDWIDTH_41HZ: {
      requested_dlpf_ = dlpf;
      break;
    }
    case DLPF_BANDWIDTH_20HZ: {
      requested_dlpf_ = dlpf;
      break;
    }
    case DLPF_BANDWIDTH_10HZ: {
      requested_dlpf_ = dlpf;
      break;
    }
    case DLPF_BANDWIDTH_5HZ: {
      requested_dlpf_ = dlpf;
      break;
    }
    default: {
      return false;
    }
  }
  /* Try setting the dlpf */
  if (!WriteRegister(ACCEL_CONFIG2_, requested_dlpf_)) {
    return false;
  }
  if (!WriteRegister(CONFIG_, requested_dlpf_)) {
    return false;
  }
  /* Update stored dlpf */
  dlpf_bandwidth_ = requested_dlpf_;
  return true;
}
bool MPU9250::enableWom(int16_t threshold_mg, const WomRate wom_rate) {
  /* Check threshold in limits, 4 - 1020 mg */
  if ((threshold_mg < 4) || (threshold_mg > 1020)) {return false;}
  spi_clock_ = SPI_CFG_CLOCK_;
  /* Set AK8963 to power down */
  WriteAk8963Register(AK8963_CNTL1_, AK8963_PWR_DOWN_);
  /* Reset the MPU9250 */
  WriteRegister(PWR_MGMNT_1_, H_RESET_);
  /* Wait for MPU-9250 to come back up */
  delay(1);
  /* Cycle 0, Sleep 0, Standby 0, Internal Clock */
  if (!WriteRegister(PWR_MGMNT_1_, 0x00)) {
    return false;
  }
  /* Disable gyro measurements */
  if (!WriteRegister(PWR_MGMNT_2_, DISABLE_GYRO_)) {
    return false;
  }
  /* Set accel bandwidth to 184 Hz */
  if (!WriteRegister(ACCEL_CONFIG2_, DLPF_BANDWIDTH_184HZ)) {
    return false;
  }
  /* Set interrupt to wake on motion */
  if (!WriteRegister(INT_ENABLE_, INT_WOM_EN_)) {
    return false;
  }
  /* Enable accel hardware intelligence */
  if (!WriteRegister(MOT_DETECT_CTRL_, (ACCEL_INTEL_EN_ | ACCEL_INTEL_MODE_))) {
    return false;
  }
  /* Set the wake on motion threshold, LSB is 4 mg */
  uint8_t wom_threshold = static_cast<uint8_t>(threshold_mg /
                                               static_cast<int8_t>(4));
  if (!WriteRegister(WOM_THR_, wom_threshold)) {
    return false;
  }
  /* Set the accel wakeup frequency */
  if (!WriteRegister(LP_ACCEL_ODR_, wom_rate)) {
    return false;
  }
  /* Switch to low power mode */
  if (!WriteRegister(PWR_MGMNT_1_, PWR_CYCLE_WOM_)) {
    return false;
  }
  return true;
}
void MPU9250::reset() {
  spi_clock_ = SPI_CFG_CLOCK_;
  /* Set AK8963 to power down */
  WriteAk8963Register(AK8963_CNTL1_, AK8963_PWR_DOWN_);
  /* Reset the MPU9250 */
  WriteRegister(PWR_MGMNT_1_, H_RESET_);
  /* Wait for MPU-9250 to come back up */
  delay(1);
}
bool MPU9250::read() {
  spi_clock_ = SPI_READ_CLOCK_;
  /* Reset the new data flags */
  new_mag_data_ = false;
  new_imu_data_ = false;
  /* Read the data registers */
  if (!ReadRegisters(INT_STATUS_, sizeof(data_buf_), data_buf_)) {
    return false;
  }
  /* Check if data is ready */
  new_imu_data_ = (data_buf_[0] & RAW_DATA_RDY_INT_);
  if (!new_imu_data_) {
    return false;
  }
  /* Unpack the buffer */
  accel_cnts_[0] = static_cast<int16_t>(data_buf_[1])  << 8 | data_buf_[2];
  accel_cnts_[1] = static_cast<int16_t>(data_buf_[3])  << 8 | data_buf_[4];
  accel_cnts_[2] = static_cast<int16_t>(data_buf_[5])  << 8 | data_buf_[6];
  temp_cnts_ =     static_cast<int16_t>(data_buf_[7])  << 8 | data_buf_[8];
  gyro_cnts_[0] =  static_cast<int16_t>(data_buf_[9])  << 8 | data_buf_[10];
  gyro_cnts_[1] =  static_cast<int16_t>(data_buf_[11]) << 8 | data_buf_[12];
  gyro_cnts_[2] =  static_cast<int16_t>(data_buf_[13]) << 8 | data_buf_[14];
  if (!is_mpu6500_) {
    new_mag_data_ = (data_buf_[15] & AK8963_DATA_RDY_INT_);
    mag_cnts_[0] =   static_cast<int16_t>(data_buf_[17]) << 8 | data_buf_[16];
    mag_cnts_[1] =   static_cast<int16_t>(data_buf_[19]) << 8 | data_buf_[18];
    mag_cnts_[2] =   static_cast<int16_t>(data_buf_[21]) << 8 | data_buf_[20];
    /* Check for mag overflow */
    mag_sensor_overflow_ = (data_buf_[22] & AK8963_HOFL_);
    if (mag_sensor_overflow_) {
      new_mag_data_ = false;
    }
  }
  /* Convert to float values */
  accel_[0] = static_cast<float>(accel_cnts_[0]) * accel_scale_ * G_MPS2_;
  accel_[1] = static_cast<float>(accel_cnts_[1]) * accel_scale_ * G_MPS2_;
  accel_[2] = static_cast<float>(accel_cnts_[2]) * accel_scale_ * G_MPS2_;
  temp_ = (static_cast<float>(temp_cnts_) - 21.0f) / TEMP_SCALE_ + 21.0f;
  gyro_[0] = static_cast<float>(gyro_cnts_[0]) * gyro_scale_ * DEG2RAD_;
  gyro_[1] = static_cast<float>(gyro_cnts_[1]) * gyro_scale_ * DEG2RAD_;
  gyro_[2] = static_cast<float>(gyro_cnts_[2]) * gyro_scale_ * DEG2RAD_;
  /* Only update on new data */
  if (new_mag_data_) {
    /* Orient magnetometer axes to match accel and gyro */
    mag_[0] =   static_cast<float>(mag_cnts_[1]) * mag_scale_[0];
    mag_[1] =   static_cast<float>(mag_cnts_[0]) * mag_scale_[1];
    mag_[2] =   static_cast<float>(mag_cnts_[2]) * mag_scale_[2] * -1.0f;
  }
  return true;
}
void MPU9250::getAccel(float& x, float& y, float& z) const {
  x = accel_[0];
  y = accel_[1];
  z = accel_[2];
}
void MPU9250::getGyro(float& x, float& y, float& z) const {
  x = gyro_[0];
  y = gyro_[1];
  z = gyro_[2];
}
void MPU9250::getMag(float& x, float& y, float& z) const {
  x = mag_[0];
  y = mag_[1];
  z = mag_[2];
}
bool MPU9250::setRate(const Rate rate) {
  // TODO: consider DLPF setting (8 Khz possible without DLPF)
  switch (rate) {
    case RATE_MIN:
      return setSrd(255); // ~4 Hz
    case RATE_50HZ_APPROX:
      return setSrd(19); // 50 Hz
    case RATE_MAX:
    case RATE_1KHZ_APPROX:
      return setSrd(0); // 1 kHz
    default:
      log("Unsupported rate setting");
      return false;
  }
}
float MPU9250::getRate() {
  return 1000.0f / (srd_ + 1);
}
bool MPU9250::setupInterrupt() {
  if (!IMUBase::setupInterrupt(int_pin_)) {
    log(errorFmt, status_ = 22);
    return false;
  }
  if (int_pin_ != -1 && !enableDrdyInt()) {
    log(errorFmt, status_ = 21);
    return false;
  }
  return true;
}
const char* MPU9250::getModel() const {
  switch (who_am_i_) {
    case WHOAMI_MPU6500: return "MPU-6500";
    case WHOAMI_MPU9250: return "MPU-9250";
    case WHOAMI_MPU9255: return "MPU-9255";
    default: return "UNKNOWN";
  }
}
bool MPU9250::WriteRegister(const uint8_t reg, const uint8_t data) {
  return imu_.WriteRegister(reg, data, spi_clock_);
}
bool MPU9250::ReadRegisters(const uint8_t reg, const uint8_t count,
                            uint8_t * const data) {
  return imu_.ReadRegisters(reg, count, spi_clock_, data);
}
bool MPU9250::WriteAk8963Register(const uint8_t reg, const uint8_t data) {
  if (is_mpu6500_) return true;

  uint8_t ret_val;
  if (!WriteRegister(I2C_SLV0_ADDR_, AK8963_I2C_ADDR_)) {
    return false;
  }
  if (!WriteRegister(I2C_SLV0_REG_, reg)) {
    return false;
  }
  if (!WriteRegister(I2C_SLV0_DO_, data)) {
    return false;
  }
  if (!WriteRegister(I2C_SLV0_CTRL_, I2C_SLV0_EN_ | sizeof(data))) {
    return false;
  }
  if (!ReadAk8963Registers(reg, sizeof(ret_val), &ret_val)) {
    return false;
  }
  if (data == ret_val) {
    return true;
  } else {
    return false;
  }
}
bool MPU9250::ReadAk8963Registers(const uint8_t reg, const uint8_t count,
                                  uint8_t * const data) {
  if (is_mpu6500_) return true;

  if (!WriteRegister(I2C_SLV0_ADDR_, AK8963_I2C_ADDR_ | I2C_READ_FLAG_)) {
    return false;
  }
  if (!WriteRegister(I2C_SLV0_REG_, reg)) {
    return false;
  }
  if (!WriteRegister(I2C_SLV0_CTRL_, I2C_SLV0_EN_ | count)) {
    return false;
  }
  delay(1);
  return ReadRegisters(EXT_SENS_DATA_00_, count, data);
}
