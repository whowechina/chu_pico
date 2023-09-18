/*
 * VL53L0X Distance measurement sensor
 * WHowe <github.com/whowechina>
 */

#ifndef VL53L0X_H
#define VL53L0X_H

#include <stdint.h>
#include <string.h>

#include "hardware/i2c.h"
#include "board_defs.h"

#define VL53L0X_DEF_ADDR 0x29

//
// VL53L0X time of flight range sensor
// Library to read the distance
// from the I2C bus
//
// by Larry Bank
//
// This code is based on Pololu's Arduino library
// https://github.com/pololu/vl53l0x-arduino
// (see LICENSE.txt for more info)
//
// My version is an attempt to simplify that code and 
// create a generic C library for Linux
//

static unsigned char stop_variable;
static uint32_t measurement_timing_budget_us;

static uint8_t vl53l0x_read8(uint8_t reg)
{
    uint8_t value;
    i2c_write_blocking(I2C_PORT, VL53L0X_DEF_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, VL53L0X_DEF_ADDR, &value, 1, false);
    return value;
}

static uint16_t vl53l0x_read16(uint8_t reg)
{
    uint8_t value[2];
    i2c_write_blocking(I2C_PORT, VL53L0X_DEF_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, VL53L0X_DEF_ADDR, value, 2, false);
    return value[0] << 8 | value[1];
}

static void vl53l0x_write8(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = {reg, value};
    i2c_write_blocking(I2C_PORT, VL53L0X_DEF_ADDR, data, 2, false);
}

static void vl53l0x_write16(uint8_t reg, uint16_t value)
{
    uint8_t data[3] = {reg, value >> 8, value & 0xff};
    i2c_write_blocking(I2C_PORT, VL53L0X_DEF_ADDR, data, 3, false);
}

static void vl53l0x_readmore(uint8_t reg, uint8_t *dst, unsigned len)
{
    i2c_write_blocking(I2C_PORT, VL53L0X_DEF_ADDR, &reg, 1, true);
    i2c_read_blocking(I2C_PORT, VL53L0X_DEF_ADDR, dst, len, false);
}

static void vl53l0x_writemore(uint8_t reg, const uint8_t *src, unsigned len)
{
    uint8_t buf[32];
    buf[0] = reg;
    memcpy(buf + 1, src, len);
    i2c_write_blocking(I2C_PORT, VL53L0X_DEF_ADDR, buf, len + 1, false);
}

static void vl53l0x_write_list(uint8_t *list)
{
    for (int i = 0; i < list[0]; i++) {
        vl53l0x_write8(list[i * 2 + 1], list[i * 2 + 2]); 
    }
}

static int single_ref_cal(uint8_t vhv_init_byte);
static int set_time_budget(uint32_t budget_us);

#define calcMacroPeriod(vcsel_period_pclks) ((((uint32_t)2304 * (vcsel_period_pclks) * 1655) + 500) / 1000)
// Encode VCSEL pulse period register value from period in PCLKs
// based on VL53L0X_encode_vcsel_period()
#define encodeVcselPeriod(period_pclks) (((period_pclks) >> 1) - 1)

#define SEQUENCE_ENABLE_FINAL_RANGE 0x80
#define SEQUENCE_ENABLE_PRE_RANGE   0x40
#define SEQUENCE_ENABLE_TCC         0x10
#define SEQUENCE_ENABLE_DSS         0x08
#define SEQUENCE_ENABLE_MSRC        0x04

typedef enum vcselperiodtype { VcselPeriodPreRange, VcselPeriodFinalRange } vcselPeriodType;
static int setVcselPulsePeriod(vcselPeriodType type, uint8_t period_pclks);

typedef struct {
    uint16_t pre_range_vcsel_period_pclks, final_range_vcsel_period_pclks;
    uint16_t msrc_dss_tcc_mclks, pre_range_mclks, final_range_mclks;
    uint32_t msrc_dss_tcc_us,    pre_range_us,    final_range_us;
} SequenceStepTimeouts;

// VL53L0X internal registers
#define REG_IDENTIFICATION_MODEL_ID		0xc0
#define REG_IDENTIFICATION_REVISION_ID		0xc2
#define REG_SYSRANGE_START			0x00

#define REG_RESULT_INTERRUPT_STATUS 		0x13
#define RESULT_RANGE_STATUS      		0x14
#define ALGO_PHASECAL_LIM                       0x30
#define ALGO_PHASECAL_CONFIG_TIMEOUT            0x30

#define GLOBAL_CONFIG_VCSEL_WIDTH               0x32
#define FINAL_RANGE_CONFIG_VALID_PHASE_LOW      0x47
#define FINAL_RANGE_CONFIG_VALID_PHASE_HIGH     0x48

#define PRE_RANGE_CONFIG_VCSEL_PERIOD           0x50
#define PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI      0x51
#define PRE_RANGE_CONFIG_VALID_PHASE_LOW        0x56
#define PRE_RANGE_CONFIG_VALID_PHASE_HIGH       0x57

#define REG_MSRC_CONFIG_CONTROL                 0x60
#define FINAL_RANGE_CONFIG_VCSEL_PERIOD         0x70
#define FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI    0x71
#define MSRC_CONFIG_TIMEOUT_MACROP              0x46
#define FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT  0x44
#define SYSRANGE_START                          0x00
#define SYSTEM_SEQUENCE_CONFIG                  0x01
#define SYSTEM_INTERRUPT_CONFIG_GPIO            0x0A
#define RESULT_INTERRUPT_STATUS                 0x13
#define VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV       0x89
#define GLOBAL_CONFIG_SPAD_ENABLES_REF_0        0xB0
#define GPIO_HV_MUX_ACTIVE_HIGH                 0x84
#define SYSTEM_INTERRUPT_CLEAR                  0x0B

static int setMeasurementTimingBudget(uint32_t budget_us);
static int performSingleRefCalibration(uint8_t vhv_init_byte);


//
// Register init lists consist of the count followed by register/value pairs
//
unsigned char ucI2CMode[] = {4, 0x88,0x00, 0x80,0x01, 0xff,0x01, 0x00,0x00};
unsigned char ucI2CMode2[] = {3, 0x00,0x01, 0xff,0x00, 0x80,0x00};
unsigned char ucSPAD0[] = {4, 0x80,0x01, 0xff,0x01, 0x00,0x00, 0xff,0x06};
unsigned char ucSPAD1[] = {5, 0xff,0x07, 0x81,0x01, 0x80,0x01, 0x94,0x6b, 0x83,0x00};
unsigned char ucSPAD2[] = {4, 0xff,0x01, 0x00,0x01, 0xff,0x00, 0x80,0x00};
unsigned char ucSPAD[] = {5, 0xff,0x01, 0x4f,0x00, 0x4e,0x2c, 0xff,0x00, 0xb6,0xb4};
unsigned char ucDefTuning[] = {80, 0xff,0x01, 0x00,0x00, 0xff,0x00, 0x09,0x00,
0x10,0x00, 0x11,0x00, 0x24,0x01, 0x25,0xff, 0x75,0x00, 0xff,0x01, 0x4e,0x2c,
0x48,0x00, 0x30,0x20, 0xff,0x00, 0x30,0x09, 0x54,0x00, 0x31,0x04, 0x32,0x03,
0x40,0x83, 0x46,0x25, 0x60,0x00, 0x27,0x00, 0x50,0x06, 0x51,0x00, 0x52,0x96,
0x56,0x08, 0x57,0x30, 0x61,0x00, 0x62,0x00, 0x64,0x00, 0x65,0x00, 0x66,0xa0,
0xff,0x01, 0x22,0x32, 0x47,0x14, 0x49,0xff, 0x4a,0x00, 0xff,0x00, 0x7a,0x0a,
0x7b,0x00, 0x78,0x21, 0xff,0x01, 0x23,0x34, 0x42,0x00, 0x44,0xff, 0x45,0x26,
0x46,0x05, 0x40,0x40, 0x0e,0x06, 0x20,0x1a, 0x43,0x40, 0xff,0x00, 0x34,0x03,
0x35,0x44, 0xff,0x01, 0x31,0x04, 0x4b,0x09, 0x4c,0x05, 0x4d,0x04, 0xff,0x00,
0x44,0x00, 0x45,0x20, 0x47,0x08, 0x48,0x28, 0x67,0x00, 0x70,0x04, 0x71,0x01,
0x72,0xfe, 0x76,0x00, 0x77,0x00, 0xff,0x01, 0x0d,0x01, 0xff,0x00, 0x80,0x01,
0x01,0xf8, 0xff,0x01, 0x8e,0x01, 0x00,0x01, 0xff,0x00, 0x80,0x00};

static int getSpadInfo(unsigned char *pCount, unsigned char *pTypeIsAperture)
{
    int iTimeout;
    unsigned char ucTemp;
    #define MAX_TIMEOUT 50

    vl53l0x_write_list(ucSPAD0);
    vl53l0x_write8(0x83, vl53l0x_read8(0x83) | 0x04);
    vl53l0x_write_list(ucSPAD1);
    iTimeout = 0;
    while(iTimeout < MAX_TIMEOUT)
    {
        if (vl53l0x_read8(0x83) != 0x00) break;
        iTimeout++;
    }
    vl53l0x_write8(0x83,0x01);
    ucTemp = vl53l0x_read8(0x92);
    *pCount = (ucTemp & 0x7f);
    *pTypeIsAperture = (ucTemp & 0x80);
    vl53l0x_write8(0x81,0x00);
    vl53l0x_write8(0xff,0x06);
    vl53l0x_write8(0x83, vl53l0x_read8(0x83) & ~0x04);
    vl53l0x_write_list(ucSPAD2);

    return 1;
} /* getSpadInfo() */

// Decode sequence step timeout in MCLKs from register value
// based on VL53L0X_decode_timeout()
// Note: the original function returned a uint32_t, but the return value is
// always stored in a uint16_t.
static uint16_t decodeTimeout(uint16_t reg_val)
{
  // format: "(LSByte * 2^MSByte) + 1"
  return (uint16_t)((reg_val & 0x00FF) <<
         (uint16_t)((reg_val & 0xFF00) >> 8)) + 1;
}

// Convert sequence step timeout from MCLKs to microseconds with given VCSEL period in PCLKs
// based on VL53L0X_calc_timeout_us()
static uint32_t timeoutMclksToMicroseconds(uint16_t timeout_period_mclks, uint8_t vcsel_period_pclks)
{
  uint32_t macro_period_ns = calcMacroPeriod(vcsel_period_pclks);

  return ((timeout_period_mclks * macro_period_ns) + (macro_period_ns / 2)) / 1000;
}

// Convert sequence step timeout from microseconds to MCLKs with given VCSEL period in PCLKs
// based on VL53L0X_calc_timeout_mclks()
static uint32_t timeoutMicrosecondsToMclks(uint32_t timeout_period_us, uint8_t vcsel_period_pclks)
{
  uint32_t macro_period_ns = calcMacroPeriod(vcsel_period_pclks);

  return (((timeout_period_us * 1000) + (macro_period_ns / 2)) / macro_period_ns);
}

// Encode sequence step timeout register value from timeout in MCLKs
// based on VL53L0X_encode_timeout()
// Note: the original function took a uint16_t, but the argument passed to it
// is always a uint16_t.
static uint16_t encodeTimeout(uint16_t timeout_mclks)
{
  // format: "(LSByte * 2^MSByte) + 1"

  uint32_t ls_byte = 0;
  uint16_t ms_byte = 0;

  if (timeout_mclks > 0)
  {
    ls_byte = timeout_mclks - 1;

    while ((ls_byte & 0xFFFFFF00) > 0)
    {
      ls_byte >>= 1;
      ms_byte++;
    }

    return (ms_byte << 8) | (ls_byte & 0xFF);
  }
  else { return 0; }
}

static void getSequenceStepTimeouts(uint8_t enables, SequenceStepTimeouts * timeouts)
{
  timeouts->pre_range_vcsel_period_pclks = ((vl53l0x_read8(PRE_RANGE_CONFIG_VCSEL_PERIOD) +1) << 1);

  timeouts->msrc_dss_tcc_mclks = vl53l0x_read8(MSRC_CONFIG_TIMEOUT_MACROP) + 1;
  timeouts->msrc_dss_tcc_us =
    timeoutMclksToMicroseconds(timeouts->msrc_dss_tcc_mclks,
                               timeouts->pre_range_vcsel_period_pclks);

  timeouts->pre_range_mclks =
    decodeTimeout(vl53l0x_read16(PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI));
  timeouts->pre_range_us =
    timeoutMclksToMicroseconds(timeouts->pre_range_mclks,
                               timeouts->pre_range_vcsel_period_pclks);

  timeouts->final_range_vcsel_period_pclks = ((vl53l0x_read8(FINAL_RANGE_CONFIG_VCSEL_PERIOD) +1) << 1);

  timeouts->final_range_mclks =
    decodeTimeout(vl53l0x_read16(FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI));

  if (enables & SEQUENCE_ENABLE_PRE_RANGE)
  {
    timeouts->final_range_mclks -= timeouts->pre_range_mclks;
  }

  timeouts->final_range_us =
    timeoutMclksToMicroseconds(timeouts->final_range_mclks,
                               timeouts->final_range_vcsel_period_pclks);
} /* getSequenceStepTimeouts() */

// Set the VCSEL (vertical cavity surface emitting laser) pulse period for the
// given period type (pre-range or final range) to the given value in PCLKs.
// Longer periods seem to increase the potential range of the sensor.
// Valid values are (even numbers only):
//  pre:  12 to 18 (initialized default: 14)
//  final: 8 to 14 (initialized default: 10)
// based on VL53L0X_set_vcsel_pulse_period()
static int setVcselPulsePeriod(vcselPeriodType type, uint8_t period_pclks)
{
  uint8_t vcsel_period_reg = encodeVcselPeriod(period_pclks);

  uint8_t enables;
  SequenceStepTimeouts timeouts;

  enables = vl53l0x_read8(SYSTEM_SEQUENCE_CONFIG);
  getSequenceStepTimeouts(enables, &timeouts);

  // "Apply specific settings for the requested clock period"
  // "Re-calculate and apply timeouts, in macro periods"

  // "When the VCSEL period for the pre or final range is changed,
  // the corresponding timeout must be read from the device using
  // the current VCSEL period, then the new VCSEL period can be
  // applied. The timeout then must be written back to the device
  // using the new VCSEL period.
  //
  // For the MSRC timeout, the same applies - this timeout being
  // dependant on the pre-range vcsel period."


  if (type == VcselPeriodPreRange)
  {
    // "Set phase check limits"
    switch (period_pclks)
    {
      case 12:
        vl53l0x_write8(PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x18);
        break;

      case 14:
        vl53l0x_write8(PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x30);
        break;

      case 16:
        vl53l0x_write8(PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x40);
        break;

      case 18:
        vl53l0x_write8(PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x50);
        break;

      default:
        // invalid period
        return 0;
    }
    vl53l0x_write8(PRE_RANGE_CONFIG_VALID_PHASE_LOW, 0x08);

    // apply new VCSEL period
    vl53l0x_write8(PRE_RANGE_CONFIG_VCSEL_PERIOD, vcsel_period_reg);

    // update timeouts

    // set_sequence_step_timeout() begin
    // (SequenceStepId == VL53L0X_SEQUENCESTEP_PRE_RANGE)

    uint16_t new_pre_range_timeout_mclks =
      timeoutMicrosecondsToMclks(timeouts.pre_range_us, period_pclks);

    vl53l0x_write16(PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI,
      encodeTimeout(new_pre_range_timeout_mclks));

    // set_sequence_step_timeout() end

    // set_sequence_step_timeout() begin
    // (SequenceStepId == VL53L0X_SEQUENCESTEP_MSRC)

    uint16_t new_msrc_timeout_mclks =
      timeoutMicrosecondsToMclks(timeouts.msrc_dss_tcc_us, period_pclks);

    vl53l0x_write8(MSRC_CONFIG_TIMEOUT_MACROP,
      (new_msrc_timeout_mclks > 256) ? 255 : (new_msrc_timeout_mclks - 1));

    // set_sequence_step_timeout() end
  }
  else if (type == VcselPeriodFinalRange)
  {
    switch (period_pclks)
    {
      case 8:
        vl53l0x_write8(FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x10);
        vl53l0x_write8(FINAL_RANGE_CONFIG_VALID_PHASE_LOW,  0x08);
        vl53l0x_write8(GLOBAL_CONFIG_VCSEL_WIDTH, 0x02);
        vl53l0x_write8(ALGO_PHASECAL_CONFIG_TIMEOUT, 0x0C);
        vl53l0x_write8(0xFF, 0x01);
        vl53l0x_write8(ALGO_PHASECAL_LIM, 0x30);
        vl53l0x_write8(0xFF, 0x00);
        break;

      case 10:
        vl53l0x_write8(FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x28);
        vl53l0x_write8(FINAL_RANGE_CONFIG_VALID_PHASE_LOW,  0x08);
        vl53l0x_write8(GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
        vl53l0x_write8(ALGO_PHASECAL_CONFIG_TIMEOUT, 0x09);
        vl53l0x_write8(0xFF, 0x01);
        vl53l0x_write8(ALGO_PHASECAL_LIM, 0x20);
        vl53l0x_write8(0xFF, 0x00);
        break;

      case 12:
        vl53l0x_write8(FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x38);
        vl53l0x_write8(FINAL_RANGE_CONFIG_VALID_PHASE_LOW,  0x08);
        vl53l0x_write8(GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
        vl53l0x_write8(ALGO_PHASECAL_CONFIG_TIMEOUT, 0x08);
        vl53l0x_write8(0xFF, 0x01);
        vl53l0x_write8(ALGO_PHASECAL_LIM, 0x20);
        vl53l0x_write8(0xFF, 0x00);
        break;

      case 14:
        vl53l0x_write8(FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x48);
        vl53l0x_write8(FINAL_RANGE_CONFIG_VALID_PHASE_LOW,  0x08);
        vl53l0x_write8(GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
        vl53l0x_write8(ALGO_PHASECAL_CONFIG_TIMEOUT, 0x07);
        vl53l0x_write8(0xFF, 0x01);
        vl53l0x_write8(ALGO_PHASECAL_LIM, 0x20);
        vl53l0x_write8(0xFF, 0x00);
        break;

      default:
        // invalid period
        return 0;
    }

    // apply new VCSEL period
    vl53l0x_write8(FINAL_RANGE_CONFIG_VCSEL_PERIOD, vcsel_period_reg);

    // update timeouts

    // set_sequence_step_timeout() begin
    // (SequenceStepId == VL53L0X_SEQUENCESTEP_FINAL_RANGE)

    // "For the final range timeout, the pre-range timeout
    //  must be added. To do this both final and pre-range
    //  timeouts must be expressed in macro periods MClks
    //  because they have different vcsel periods."

    uint16_t new_final_range_timeout_mclks =
      timeoutMicrosecondsToMclks(timeouts.final_range_us, period_pclks);

    if (enables & SEQUENCE_ENABLE_PRE_RANGE)
    {
      new_final_range_timeout_mclks += timeouts.pre_range_mclks;
    }

    vl53l0x_write16(FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
    encodeTimeout(new_final_range_timeout_mclks));

    // set_sequence_step_timeout end
  }
  else
  {
    // invalid type
    return 0;
  }

  // "Finally, the timing budget must be re-applied"

  setMeasurementTimingBudget(measurement_timing_budget_us);

  // "Perform the phase calibration. This is needed after changing on vcsel period."
  // VL53L0X_perform_phase_calibration() begin

  uint8_t sequence_config = vl53l0x_read8(SYSTEM_SEQUENCE_CONFIG);
  vl53l0x_write8(SYSTEM_SEQUENCE_CONFIG, 0x02);
  performSingleRefCalibration(0x0);
  vl53l0x_write8(SYSTEM_SEQUENCE_CONFIG, sequence_config);

  // VL53L0X_perform_phase_calibration() end

  return 1;
}

// Set the measurement timing budget in microseconds, which is the time allowed
// for one measurement; the ST API and this library take care of splitting the
// timing budget among the sub-steps in the ranging sequence. A longer timing
// budget allows for more accurate measurements. Increasing the budget by a
// factor of N decreases the range measurement standard deviation by a factor of
// sqrt(N). Defaults to about 33 milliseconds; the minimum is 20 ms.
// based on VL53L0X_set_measurement_timing_budget_micro_seconds()
static int setMeasurementTimingBudget(uint32_t budget_us)
{
uint32_t used_budget_us;
uint32_t final_range_timeout_us;
uint16_t final_range_timeout_mclks;

  uint8_t enables;
  SequenceStepTimeouts timeouts;

  uint16_t const StartOverhead      = 1320; // note that this is different than the value in get_
  uint16_t const EndOverhead        = 960;
  uint16_t const MsrcOverhead       = 660;
  uint16_t const TccOverhead        = 590;
  uint16_t const DssOverhead        = 690;
  uint16_t const PreRangeOverhead   = 660;
  uint16_t const FinalRangeOverhead = 550;

  uint32_t const MinTimingBudget = 20000;

  if (budget_us < MinTimingBudget) { return 0; }

  used_budget_us = StartOverhead + EndOverhead;

  enables = vl53l0x_read8(SYSTEM_SEQUENCE_CONFIG);
  getSequenceStepTimeouts(enables, &timeouts);

  if (enables & SEQUENCE_ENABLE_TCC)
  {
    used_budget_us += (timeouts.msrc_dss_tcc_us + TccOverhead);
  }

  if (enables & SEQUENCE_ENABLE_DSS)
  {
    used_budget_us += 2 * (timeouts.msrc_dss_tcc_us + DssOverhead);
  }
  else if (enables & SEQUENCE_ENABLE_MSRC)
  {
    used_budget_us += (timeouts.msrc_dss_tcc_us + MsrcOverhead);
  }

  if (enables & SEQUENCE_ENABLE_PRE_RANGE)
  {
    used_budget_us += (timeouts.pre_range_us + PreRangeOverhead);
  }

  if (enables & SEQUENCE_ENABLE_FINAL_RANGE)
  {
    used_budget_us += FinalRangeOverhead;

    // "Note that the final range timeout is determined by the timing
    // budget and the sum of all other timeouts within the sequence.
    // If there is no room for the final range timeout, then an error
    // will be set. Otherwise the remaining time will be applied to
    // the final range."

    if (used_budget_us > budget_us)
    {
      // "Requested timeout too big."
      return 0;
    }

    final_range_timeout_us = budget_us - used_budget_us;

    // set_sequence_step_timeout() begin
    // (SequenceStepId == VL53L0X_SEQUENCESTEP_FINAL_RANGE)

    // "For the final range timeout, the pre-range timeout
    //  must be added. To do this both final and pre-range
    //  timeouts must be expressed in macro periods MClks
    //  because they have different vcsel periods."

    final_range_timeout_mclks =
      timeoutMicrosecondsToMclks(final_range_timeout_us,
                                 timeouts.final_range_vcsel_period_pclks);

    if (enables & SEQUENCE_ENABLE_PRE_RANGE)
    {
      final_range_timeout_mclks += timeouts.pre_range_mclks;
    }

    vl53l0x_write16(FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
      encodeTimeout(final_range_timeout_mclks));

    // set_sequence_step_timeout() end

    measurement_timing_budget_us = budget_us; // store for internal reuse
  }
  return 1;
}

static uint32_t getMeasurementTimingBudget(void)
{
  uint8_t enables;
  SequenceStepTimeouts timeouts;

  uint16_t const StartOverhead     = 1910; // note that this is different than the value in set_
  uint16_t const EndOverhead        = 960;
  uint16_t const MsrcOverhead       = 660;
  uint16_t const TccOverhead        = 590;
  uint16_t const DssOverhead        = 690;
  uint16_t const PreRangeOverhead   = 660;
  uint16_t const FinalRangeOverhead = 550;

  // "Start and end overhead times always present"
  uint32_t budget_us = StartOverhead + EndOverhead;

  enables = vl53l0x_read8(SYSTEM_SEQUENCE_CONFIG);
  getSequenceStepTimeouts(enables, &timeouts);

  if (enables & SEQUENCE_ENABLE_TCC)
  {
    budget_us += (timeouts.msrc_dss_tcc_us + TccOverhead);
  }

  if (enables & SEQUENCE_ENABLE_DSS)
  {
    budget_us += 2 * (timeouts.msrc_dss_tcc_us + DssOverhead);
  }
  else if (enables & SEQUENCE_ENABLE_MSRC)
  {
    budget_us += (timeouts.msrc_dss_tcc_us + MsrcOverhead);
  }

  if (enables & SEQUENCE_ENABLE_PRE_RANGE)
  {
    budget_us += (timeouts.pre_range_us + PreRangeOverhead);
  }

  if (enables & SEQUENCE_ENABLE_FINAL_RANGE)
  {
    budget_us += (timeouts.final_range_us + FinalRangeOverhead);
  }

  measurement_timing_budget_us = budget_us; // store for internal reuse
  return budget_us;
}

static int performSingleRefCalibration(uint8_t vhv_init_byte)
{
int iTimeout;
  vl53l0x_write8(SYSRANGE_START, 0x01 | vhv_init_byte); // VL53L0X_REG_SYSRANGE_MODE_START_STOP

  iTimeout = 0;
  while ((vl53l0x_read8(RESULT_INTERRUPT_STATUS) & 0x07) == 0)
  {
    iTimeout++;
    if (iTimeout > 100) { return 0; }
  }

  vl53l0x_write8(SYSTEM_INTERRUPT_CLEAR, 0x01);

  vl53l0x_write8(SYSRANGE_START, 0x00);

  return 1;
} /* performSingleRefCalibration() */


int vl53l0x_init()
{
    unsigned char spad_count=0, spad_type_is_aperture=0, ref_spad_map[6];
    unsigned char ucFirstSPAD, ucSPADsEnabled;
    int i;

    // set 2.8V mode
    vl53l0x_write8(VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV,
    vl53l0x_read8(VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV) | 0x01); // set bit 0
    // Set I2C standard mode
    vl53l0x_write_list(ucI2CMode);
    stop_variable = vl53l0x_read8(0x91);
    vl53l0x_write_list(ucI2CMode2);
    // disable SIGNAL_RATE_MSRC (bit 1) and SIGNAL_RATE_PRE_RANGE (bit 4) limit checks
    vl53l0x_write8(REG_MSRC_CONFIG_CONTROL, vl53l0x_read8(REG_MSRC_CONFIG_CONTROL) | 0x12);
    // Q9.7 fixed point format (9 integer bits, 7 fractional bits)
    vl53l0x_write16(FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT, 32); // 0.25
    vl53l0x_write8(SYSTEM_SEQUENCE_CONFIG, 0xFF);
    getSpadInfo(&spad_count, &spad_type_is_aperture);

    vl53l0x_readmore(GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6);
    //printf("initial spad map: %02x,%02x,%02x,%02x,%02x,%02x\n", ref_spad_map[0], ref_spad_map[1], ref_spad_map[2], ref_spad_map[3], ref_spad_map[4], ref_spad_map[5]);
    vl53l0x_write_list(ucSPAD);
    ucFirstSPAD = (spad_type_is_aperture) ? 12: 0;
    ucSPADsEnabled = 0;
    // clear bits for unused SPADs
    for (i=0; i<48; i++)
    {
    if (i < ucFirstSPAD || ucSPADsEnabled == spad_count)
    {
        ref_spad_map[i>>3] &= ~(1<<(i & 7));
    }
    else if (ref_spad_map[i>>3] & (1<< (i & 7)))
    {
        ucSPADsEnabled++;
    }
    } // for i
    vl53l0x_writemore(GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6);
    //printf("final spad map: %02x,%02x,%02x,%02x,%02x,%02x\n", ref_spad_map[0], 
    //ref_spad_map[1], ref_spad_map[2], ref_spad_map[3], ref_spad_map[4], ref_spad_map[5]);

    // load default tuning settings
    vl53l0x_write_list(ucDefTuning); // long list of magic numbers

    // set interrupt configuration to "new sample ready"
    vl53l0x_write8(SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
    vl53l0x_write8(GPIO_HV_MUX_ACTIVE_HIGH, vl53l0x_read8(GPIO_HV_MUX_ACTIVE_HIGH) & ~0x10); // active low
    vl53l0x_write8(SYSTEM_INTERRUPT_CLEAR, 0x01);
    measurement_timing_budget_us = getMeasurementTimingBudget();
    vl53l0x_write8(SYSTEM_SEQUENCE_CONFIG, 0xe8);
    setMeasurementTimingBudget(measurement_timing_budget_us);
    vl53l0x_write8(SYSTEM_SEQUENCE_CONFIG, 0x01);
    if (!performSingleRefCalibration(0x40)) { return 0; }
    vl53l0x_write8(SYSTEM_SEQUENCE_CONFIG, 0x02);
    if (!performSingleRefCalibration(0x00)) { return 0; }
    vl53l0x_write8(SYSTEM_SEQUENCE_CONFIG, 0xe8);
    return 1;
} /* tofInit() */


uint16_t readRangeContinuousMillimeters(void)
{
int iTimeout = 0;
uint16_t range;

  while ((vl53l0x_read8(RESULT_INTERRUPT_STATUS) & 0x07) == 0)
  {
    iTimeout++;
    if (iTimeout > 50)
    {
      return -1;
    }
  }

  // assumptions: Linearity Corrective Gain is 1000 (default);
  // fractional ranging is not enabled
  range = vl53l0x_read16(RESULT_RANGE_STATUS + 10);

  vl53l0x_write8(SYSTEM_INTERRUPT_CLEAR, 0x01);

  return range;
}
//
// Read the current distance in mm
//
int vl53l0x_distance()
{
int iTimeout;

  vl53l0x_write8(0x80, 0x01);
  vl53l0x_write8(0xFF, 0x01);
  vl53l0x_write8(0x00, 0x00);
  vl53l0x_write8(0x91, stop_variable);
  vl53l0x_write8(0x00, 0x01);
  vl53l0x_write8(0xFF, 0x00);
  vl53l0x_write8(0x80, 0x00);

  vl53l0x_write8(SYSRANGE_START, 0x01);

  // "Wait until start bit has been cleared"
  iTimeout = 0;
  while (vl53l0x_read8(SYSRANGE_START) & 0x01)
  {
    iTimeout++;
    if (iTimeout > 50)
    {
      return -1;
    }
  }

  return readRangeContinuousMillimeters();

} /* tofReadDistance() */

int tofGetModel(int *model, int *revision)
{
unsigned char ucTemp[2];
int i;

	if (model)
	{
    *model = vl53l0x_read8(REG_IDENTIFICATION_MODEL_ID);
	}
	if (revision)
	{
		*revision = vl53l0x_read8(REG_IDENTIFICATION_REVISION_ID);
	}
	return 1;

}

#endif