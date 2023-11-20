/*
 * VL53L0X Distance measurement sensor
 * WHowe <github.com/whowechina>
 *
 * Most of this VL53L0X code is from https://github.com/pololu/vl53l0x-arduino
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "hardware/i2c.h"
#include "board_defs.h"

#include "vl53l0x.h"

#define VL53L0X_DEF_ADDR 0x29

#define IO_TIMEOUT_US 1000
#define TOF_WAIT_US 200000

// Decode VCSEL (vertical cavity surface emitting laser) pulse period in PCLKs
#define decodeVcselPeriod(reg_val) (((reg_val) + 1) << 1)

// Encode VCSEL pulse period register value from period in PCLKs
#define encodeVcselPeriod(period_pclks) (((period_pclks) >> 1) - 1)

// Calculate macro period in *nanoseconds* from VCSEL period in PCLKs
// PLL_period_ps = 1655; macro_period_vclks = 2304
#define calcMacroPeriod(vcsel_period_pclks) ((((uint32_t)2304 * (vcsel_period_pclks) * 1655) + 500) / 1000)

static i2c_inst_t *port = i2c0;
static uint8_t addr = VL53L0X_DEF_ADDR;

static struct {
    uint8_t stop_variable; // read by init and used when starting measurement
    uint16_t range;
    uint32_t timing_budget_us;
} instances[16];

#define INSTANCE_NUM (sizeof(instances) / sizeof(instances[0]))

// Write an 8-bit register
void write_reg(uint8_t reg, uint8_t value)
{
    uint8_t data[2] = { reg, value };
    i2c_write_blocking_until(I2C_PORT, addr, data, 2, false, time_us_64() + IO_TIMEOUT_US);
}

// Write a 16-bit register
void write_reg16(uint8_t reg, uint16_t value)
{
    uint8_t data[3] = { reg, value >> 8, value & 0xff };
    i2c_write_blocking_until(I2C_PORT, addr, data, 3, false, time_us_64() + IO_TIMEOUT_US);
}

static void write_reg_list(const uint16_t *list)
{
    const uint16_t *regs = list + 1;
    for (int i = 0; i < *list; i++) {
        write_reg(regs[i] >> 8, regs[i] & 0xff);
    }
}

// Read an 8-bit register
uint8_t read_reg(uint8_t reg)
{
    uint8_t value;
    i2c_write_blocking_until(I2C_PORT, addr, &reg, 1, true, time_us_64() + IO_TIMEOUT_US);
    i2c_read_blocking_until(I2C_PORT, addr, &value, 1, false, time_us_64() + IO_TIMEOUT_US);
    return value;
}

// Read a 16-bit register
uint16_t read_reg16(uint8_t reg)
{
    uint8_t value[2];
    i2c_write_blocking_until(I2C_PORT, addr, &reg, 1, true, time_us_64() + IO_TIMEOUT_US);
    i2c_read_blocking_until(I2C_PORT, addr, value, 2, false, time_us_64() + IO_TIMEOUT_US);
    return (value[0] << 8) | value[1];
}

// Write an arbitrary number of bytes from the given array to the sensor,
// starting at the given register
void write_many(uint8_t reg, const uint8_t *src, uint8_t len)
{
    i2c_write_blocking_until(I2C_PORT, addr, &reg, 1, true, time_us_64() + IO_TIMEOUT_US);
    i2c_write_blocking_until(I2C_PORT, addr, src, len, false, time_us_64() + IO_TIMEOUT_US);
}

// Read an arbitrary number of bytes from the sensor, starting at the given
// register, into the given array
void read_many(uint8_t reg, uint8_t *dst, uint8_t len)
{
    i2c_write_blocking_until(I2C_PORT, addr, &reg, 1, true, time_us_64() + IO_TIMEOUT_US);
    i2c_read_blocking_until(I2C_PORT, addr, dst, len, false, time_us_64() + IO_TIMEOUT_US * len);
}


const uint16_t reg_mode1[] = { 4, 0x8800, 0x8001, 0xff01, 0x0000 };
const uint16_t reg_mode2[] = { 3, 0x0001, 0xff00, 0x8000 };
const uint16_t reg_spad0[] = { 4, 0x8001, 0xff01, 0x0000, 0xff06 };
const uint16_t reg_spad1[] = { 5, 0xff07, 0x8101, 0x8001, 0x946b, 0x8300 };
const uint16_t reg_spad2[] = { 4, 0xff01, 0x0001, 0xff00, 0x8000 };
const uint16_t reg_spad[] = { 5, 0xff01, 0x4f00, 0x4e2c, 0xff00, 0xb6b4 };
const uint16_t reg_tuning[] = { 80, 
        0xff01, 0x0000, 0xff00, 0x0900, 0x1000, 0x1100, 0x2401, 0x25ff,
        0x7500, 0xff01, 0x4e2c, 0x4800, 0x3020, 0xff00, 0x3009, 0x5400,
        0x3104, 0x3203, 0x4083, 0x4625, 0x6000, 0x2700, 0x5006, 0x5100,
        0x5296, 0x5608, 0x5730, 0x6100, 0x6200, 0x6400, 0x6500, 0x66a0,
        0xff01, 0x2232, 0x4714, 0x49ff, 0x4a00, 0xff00, 0x7a0a, 0x7b00,
        0x7821, 0xff01, 0x2334, 0x4200, 0x44ff, 0x4526, 0x4605, 0x4040,
        0x0e06, 0x201a, 0x4340, 0xff00, 0x3403, 0x3544, 0xff01, 0x3104,
        0x4b09, 0x4c05, 0x4d04, 0xff00, 0x4400, 0x4520, 0x4708, 0x4828,
        0x6700, 0x7004, 0x7101, 0x72fe, 0x7600, 0x7700, 0xff01, 0x0d01,
        0xff00, 0x8001, 0x01f8, 0xff01, 0x8e01, 0x0001, 0xff00, 0x8000,
    };

void vl53l0x_init(i2c_inst_t *i2c_port, uint8_t i2c_addr)
{
    port = i2c_port;
    if (i2c_addr != 0) {
        addr = i2c_addr;
    }
}

bool vl53l0x_is_present()
{
    return read_reg(IDENTIFICATION_MODEL_ID) == 0xEE;
}

bool vl53l0x_init_tof(int index)
{
    write_reg(VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV,
    read_reg(VHV_CONFIG_PAD_SCL_SDA__EXTSUP_HV) | 0x01);

    write_reg_list(reg_mode1);
    instances[index].stop_variable = read_reg(0x91);
    write_reg_list(reg_mode2);

    // disable SIGNAL_RATE_MSRC (bit 1) and SIGNAL_RATE_PRE_RANGE (bit 4) limit checks
    write_reg(MSRC_CONFIG_CONTROL, read_reg(MSRC_CONFIG_CONTROL) | 0x12);

    // Q9.7 fixed point format (9 integer bits, 7 fractional bits)
    write_reg16(FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT, 32);

    write_reg(SYSTEM_SEQUENCE_CONFIG, 0xFF);

    uint8_t spad_count;
    bool is_aperture;
    if (!getSpadInfo(&spad_count, &is_aperture)) {
        printf("%d\n", __LINE__);
        return false;
    }

    // The SPAD map (RefGoodSpadMap) is read by VL53L0X_get_info_from_device() in
    // the API, but the same data seems to be more easily readable from
    // GLOBAL_CONFIG_SPAD_ENABLES_REF_0 through _6, so read it from there
    uint8_t ref_spad_map[6];
    read_many(GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6);
    write_reg_list(reg_spad);

    uint8_t first_spad = is_aperture ? 12 : 0; // 12 is the first aperture spad
    uint8_t spads_enabled = 0;

    for (int i = 0; i < 48; i++) {
        if (i < first_spad || spads_enabled == spad_count) {
            // This bit is lower than the first one that should be enabled, or
            // (reference_spad_count) bits have already been enabled, so zero this bit
            ref_spad_map[i / 8] &= ~(1 << (i % 8));
        } else if (ref_spad_map[i / 8] & (1 << i % 8)) {
            spads_enabled++;
        }
    }

    write_many(GLOBAL_CONFIG_SPAD_ENABLES_REF_0, ref_spad_map, 6);

    write_reg_list(reg_tuning);

    write_reg(SYSTEM_INTERRUPT_CONFIG_GPIO, 0x04);
    write_reg(GPIO_HV_MUX_ACTIVE_HIGH, read_reg(GPIO_HV_MUX_ACTIVE_HIGH) & ~0x10); // active low
    write_reg(SYSTEM_INTERRUPT_CLEAR, 0x01);

    instances[index].timing_budget_us = getMeasurementTimingBudget(index);
    write_reg(SYSTEM_SEQUENCE_CONFIG, 0xE8);
    setMeasurementTimingBudget(index, instances[index].timing_budget_us);

    write_reg(SYSTEM_SEQUENCE_CONFIG, 0x01);
    if (!performSingleRefCalibration(0x40)) {
        printf("%d\n", __LINE__);
        return false;
    }

    write_reg(SYSTEM_SEQUENCE_CONFIG, 0x02);
    if (!performSingleRefCalibration(0x00)) {
        printf("%d\n", __LINE__);
        return false;
    }

    write_reg(SYSTEM_SEQUENCE_CONFIG, 0xE8);

    return true;
}

// Get the return signal rate limit check value in MCPS
float getSignalRateLimit()
{
    return (float)read_reg16(FINAL_RANGE_CONFIG_MIN_COUNT_RATE_RTN_LIMIT) / (1 << 7);
}

// Decode sequence step timeout in MCLKs from register value
// based on VL53L0X_decode_timeout()
// Note: the original function returned a uint32_t, but the return value is
// always stored in a uint16_t.
uint16_t decodeTimeout(uint16_t reg_val)
{
    // format: "(LSByte * 2^MSByte) + 1"
    return (uint16_t)((reg_val & 0x00FF) <<
           (uint16_t)((reg_val & 0xFF00) >> 8)) + 1;
}

// Encode sequence step timeout register value from timeout in MCLKs
// based on VL53L0X_encode_timeout()
static uint16_t encodeTimeout(uint16_t timeout_mclks)
{
    // format: "(LSByte * 2^MSByte) + 1"
    uint32_t ls_byte = 0;
    uint16_t ms_byte = 0;

    if (timeout_mclks > 0) {
        ls_byte = timeout_mclks - 1;

        while ((ls_byte & 0xFFFFFF00) > 0) {
            ls_byte >>= 1;
            ms_byte++;
        }

        return (ms_byte << 8) | (ls_byte & 0xFF);
    }

    return 0;
}

// Convert sequence step timeout from MCLKs to microseconds with given VCSEL period in PCLKs
// based on VL53L0X_calc_timeout_us()
uint32_t timeoutMclksToMicroseconds(uint16_t timeout_period_mclks, uint8_t vcsel_period_pclks)
{
    uint32_t macro_period_ns = calcMacroPeriod(vcsel_period_pclks);

    return ((timeout_period_mclks * macro_period_ns) + 500) / 1000;
}

// Convert sequence step timeout from microseconds to MCLKs with given VCSEL period in PCLKs
// based on VL53L0X_calc_timeout_mclks()
uint32_t timeoutMicrosecondsToMclks(uint32_t timeout_period_us, uint8_t vcsel_period_pclks)
{
    uint32_t macro_period_ns = calcMacroPeriod(vcsel_period_pclks);

    return (((timeout_period_us * 1000) + (macro_period_ns / 2)) / macro_period_ns);
}

// Set the measurement timing budget in microseconds, which is the time allowed
// for one measurement; the ST API and this library take care of splitting the
// timing budget among the sub-steps in the ranging sequence. A longer timing
// budget allows for more accurate measurements. Increasing the budget by a
// factor of N decreases the range measurement standard deviation by a factor of
// sqrt(N). Defaults to about 33 milliseconds; the minimum is 20 ms.
// based on VL53L0X_set_measurement_timing_budget_micro_seconds()
bool setMeasurementTimingBudget(int index, uint32_t budget_us)
{
    SequenceStepEnables enables;
    SequenceStepTimeouts timeouts;

    uint16_t const StartOverhead = 1320; // different than the value in get_
    uint16_t const EndOverhead = 960;
    uint16_t const MsrcOverhead = 660;
    uint16_t const TccOverhead = 590;
    uint16_t const DssOverhead = 690;
    uint16_t const PreRangeOverhead = 660;
    uint16_t const FinalRangeOverhead = 550;

    uint32_t const MinTimingBudget = 20000;

    if (budget_us < MinTimingBudget) {
        return false;
    }

    uint32_t used_budget_us = StartOverhead + EndOverhead;

    getSequenceStepEnables(&enables);
    getSequenceStepTimeouts(&enables, &timeouts);

    if (enables.tcc) {
        used_budget_us += (timeouts.msrc_dss_tcc_us + TccOverhead);
    }

    if (enables.dss) {
        used_budget_us += 2 * (timeouts.msrc_dss_tcc_us + DssOverhead);
    } else if (enables.msrc) {
        used_budget_us += (timeouts.msrc_dss_tcc_us + MsrcOverhead);
    }

    if (enables.pre_range) {
        used_budget_us += (timeouts.pre_range_us + PreRangeOverhead);
    }

    if (enables.final_range) {
        used_budget_us += FinalRangeOverhead;

        // "Note that the final range timeout is determined by the timing
        // budget and the sum of all other timeouts within the sequence.
        // If there is no room for the final range timeout, then an error
        // will be set. Otherwise the remaining time will be applied to
        // the final range."

        if (used_budget_us > budget_us)
        {
            // "Requested timeout too big."
            return false;
        }

        uint32_t final_range_timeout_us = budget_us - used_budget_us;

        // set_sequence_step_timeout() begin
        // (SequenceStepId == VL53L0X_SEQUENCESTEP_FINAL_RANGE)

        // "For the final range timeout, the pre-range timeout
        //    must be added. To do this both final and pre-range
        //    timeouts must be expressed in macro periods MClks
        //    because they have different vcsel periods."

        uint32_t final_range_timeout_mclks =
            timeoutMicrosecondsToMclks(final_range_timeout_us,
                                       timeouts.final_range_vcsel_period_pclks);

        if (enables.pre_range) {
            final_range_timeout_mclks += timeouts.pre_range_mclks;
        }

        write_reg16(FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
            encodeTimeout(final_range_timeout_mclks));

        // set_sequence_step_timeout() end

        instances[index].timing_budget_us = budget_us; // store for internal reuse
    }
    return true;
}

// Get the measurement timing budget in microseconds
// based on VL53L0X_get_measurement_timing_budget_micro_seconds()
// in us
uint32_t getMeasurementTimingBudget(int index)
{
    SequenceStepEnables enables;
    SequenceStepTimeouts timeouts;

    uint16_t const StartOverhead = 1910;
    uint16_t const EndOverhead = 960;
    uint16_t const MsrcOverhead = 660;
    uint16_t const TccOverhead = 590;
    uint16_t const DssOverhead = 690;
    uint16_t const PreRangeOverhead = 660;
    uint16_t const FinalRangeOverhead = 550;

    // "Start and end overhead times always present"
    uint32_t budget_us = StartOverhead + EndOverhead;

    getSequenceStepEnables(&enables);
    getSequenceStepTimeouts(&enables, &timeouts);

    if (enables.tcc) {
        budget_us += (timeouts.msrc_dss_tcc_us + TccOverhead);
    }

    if (enables.dss) {
        budget_us += 2 * (timeouts.msrc_dss_tcc_us + DssOverhead);
    } else if (enables.msrc) {
        budget_us += (timeouts.msrc_dss_tcc_us + MsrcOverhead);
    }

    if (enables.pre_range) {
        budget_us += (timeouts.pre_range_us + PreRangeOverhead);
    }

    if (enables.final_range) {
        budget_us += (timeouts.final_range_us + FinalRangeOverhead);
    }

    instances[index].timing_budget_us = budget_us; // cache for reuse
    return budget_us;
}

// Set the VCSEL (vertical cavity surface emitting laser) pulse period for the
// given period type (pre-range or final range) to the given value in PCLKs.
// Longer periods seem to increase the potential range of the sensor.
// Valid values are (even numbers only):
//    pre:    12 to 18 (initialized default: 14)
//    final: 8 to 14 (initialized default: 10)
// based on VL53L0X_set_vcsel_pulse_period()
bool setVcselPulsePeriod(int index, vcselPeriodType type, uint8_t period_pclks)
{
    uint8_t vcsel_period_reg = encodeVcselPeriod(period_pclks);

    SequenceStepEnables enables;
    SequenceStepTimeouts timeouts;

    getSequenceStepEnables(&enables);
    getSequenceStepTimeouts(&enables, &timeouts);

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


    if (type == VcselPeriodPreRange) {
        // "Set phase check limits"
        switch (period_pclks) {
            case 12:
                write_reg(PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x18);
                break;

            case 14:
                write_reg(PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x30);
                break;

            case 16:
                write_reg(PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x40);
                break;

            case 18:
                write_reg(PRE_RANGE_CONFIG_VALID_PHASE_HIGH, 0x50);
                break;

            default:
                // invalid period
                return false;
        }
        write_reg(PRE_RANGE_CONFIG_VALID_PHASE_LOW, 0x08);

        // apply new VCSEL period
        write_reg(PRE_RANGE_CONFIG_VCSEL_PERIOD, vcsel_period_reg);

        // update timeouts

        // set_sequence_step_timeout() begin
        // (SequenceStepId == VL53L0X_SEQUENCESTEP_PRE_RANGE)

        uint16_t new_pre_range_timeout_mclks =
            timeoutMicrosecondsToMclks(timeouts.pre_range_us, period_pclks);

        write_reg16(PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI,
            encodeTimeout(new_pre_range_timeout_mclks));

        // set_sequence_step_timeout() end

        // set_sequence_step_timeout() begin
        // (SequenceStepId == VL53L0X_SEQUENCESTEP_MSRC)

        uint16_t new_msrc_timeout_mclks =
            timeoutMicrosecondsToMclks(timeouts.msrc_dss_tcc_us, period_pclks);

        write_reg(MSRC_CONFIG_TIMEOUT_MACROP,
            (new_msrc_timeout_mclks > 256) ? 255 : (new_msrc_timeout_mclks - 1));

        // set_sequence_step_timeout() end
    } else if (type == VcselPeriodFinalRange) {
        switch (period_pclks) {
            case 8:
                write_reg(FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x10);
                write_reg(FINAL_RANGE_CONFIG_VALID_PHASE_LOW,    0x08);
                write_reg(GLOBAL_CONFIG_VCSEL_WIDTH, 0x02);
                write_reg(ALGO_PHASECAL_CONFIG_TIMEOUT, 0x0C);
                write_reg(0xFF, 0x01);
                write_reg(ALGO_PHASECAL_LIM, 0x30);
                write_reg(0xFF, 0x00);
                break;

            case 10:
                write_reg(FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x28);
                write_reg(FINAL_RANGE_CONFIG_VALID_PHASE_LOW,    0x08);
                write_reg(GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
                write_reg(ALGO_PHASECAL_CONFIG_TIMEOUT, 0x09);
                write_reg(0xFF, 0x01);
                write_reg(ALGO_PHASECAL_LIM, 0x20);
                write_reg(0xFF, 0x00);
                break;

            case 12:
                write_reg(FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x38);
                write_reg(FINAL_RANGE_CONFIG_VALID_PHASE_LOW,    0x08);
                write_reg(GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
                write_reg(ALGO_PHASECAL_CONFIG_TIMEOUT, 0x08);
                write_reg(0xFF, 0x01);
                write_reg(ALGO_PHASECAL_LIM, 0x20);
                write_reg(0xFF, 0x00);
                break;

            case 14:
                write_reg(FINAL_RANGE_CONFIG_VALID_PHASE_HIGH, 0x48);
                write_reg(FINAL_RANGE_CONFIG_VALID_PHASE_LOW,    0x08);
                write_reg(GLOBAL_CONFIG_VCSEL_WIDTH, 0x03);
                write_reg(ALGO_PHASECAL_CONFIG_TIMEOUT, 0x07);
                write_reg(0xFF, 0x01);
                write_reg(ALGO_PHASECAL_LIM, 0x20);
                write_reg(0xFF, 0x00);
                break;

            default:
                // invalid period
                return false;
        }

        // apply new VCSEL period
        write_reg(FINAL_RANGE_CONFIG_VCSEL_PERIOD, vcsel_period_reg);

        // update timeouts

        // set_sequence_step_timeout() begin
        // (SequenceStepId == VL53L0X_SEQUENCESTEP_FINAL_RANGE)

        // "For the final range timeout, the pre-range timeout
        //    must be added. To do this both final and pre-range
        //    timeouts must be expressed in macro periods MClks
        //    because they have different vcsel periods."

        uint16_t new_final_range_timeout_mclks =
            timeoutMicrosecondsToMclks(timeouts.final_range_us, period_pclks);

        if (enables.pre_range) {
            new_final_range_timeout_mclks += timeouts.pre_range_mclks;
        }

        write_reg16(FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI,
            encodeTimeout(new_final_range_timeout_mclks));

        // set_sequence_step_timeout end
    }
    else {
        // invalid type
        return false;
    }

    // "Finally, the timing budget must be re-applied"

    setMeasurementTimingBudget(index, instances[index].timing_budget_us);

    // "Perform the phase calibration. This is needed after changing on vcsel period."
    // VL53L0X_perform_phase_calibration() begin

    uint8_t sequence_config = read_reg(SYSTEM_SEQUENCE_CONFIG);
    write_reg(SYSTEM_SEQUENCE_CONFIG, 0x02);
    performSingleRefCalibration(0x0);
    write_reg(SYSTEM_SEQUENCE_CONFIG, sequence_config);

    // VL53L0X_perform_phase_calibration() end

    return true;
}

// Get the VCSEL pulse period in PCLKs for the given period type.
// based on VL53L0X_get_vcsel_pulse_period()
uint8_t getVcselPulsePeriod(vcselPeriodType type)
{
    if (type == VcselPeriodPreRange)
    {
        return decodeVcselPeriod(read_reg(PRE_RANGE_CONFIG_VCSEL_PERIOD));
    }
    else if (type == VcselPeriodFinalRange)
    {
        return decodeVcselPeriod(read_reg(FINAL_RANGE_CONFIG_VCSEL_PERIOD));
    }
    else { return 255; }
}

// Start continuous ranging measurements.
// based on VL53L0X_StartMeasurement()
void vl53l0x_start_continuous(int index)
{
    if (index >= INSTANCE_NUM) {
        return;
    }

    write_reg(0x80, 0x01);
    write_reg(0xFF, 0x01);
    write_reg(0x00, 0x00);
    write_reg(0x91, instances[index].stop_variable);
    write_reg(0x00, 0x01);
    write_reg(0xFF, 0x00);
    write_reg(0x80, 0x00);

    write_reg(SYSRANGE_START, 0x02); // VL53L0X_REG_SYSRANGE_MODE_BACKTOBACK
}

// Stop continuous measurements
// based on VL53L0X_StopMeasurement()
void vl53l0x_stop_continuous(int index)
{
    write_reg(SYSRANGE_START, 0x01); // VL53L0X_REG_SYSRANGE_MODE_SINGLESHOT

    write_reg(0xFF, 0x01);
    write_reg(0x00, 0x00);
    write_reg(0x91, 0x00);
    write_reg(0x00, 0x01);
    write_reg(0xFF, 0x00);
}

// Returns a range reading in millimeters when continuous mode is active
// (readRangeSingleMillimeters() also calls this function after starting a
// single-shot range measurement)
uint16_t readRangeContinuousMillimeters(int index)
{
    if (index >= INSTANCE_NUM) {
        return 65535;
    }

    if ((read_reg(RESULT_INTERRUPT_STATUS) & 0x07) == 0) {
        return instances[index].range; // use last result
    }

    // assumptions: Linearity Corrective Gain is 1000 (default);
    // fractional ranging is not enabled
    instances[index].range = read_reg16(RESULT_RANGE_STATUS + 10);

    write_reg(SYSTEM_INTERRUPT_CLEAR, 0x01);

    return instances[index].range;
}

#if 0
// Performs a single-shot range measurement and returns the reading in
// millimeters
// based on VL53L0X_PerformSingleRangingMeasurement()
uint16_t readRangeSingleMillimeters(int index)
{
    static uint16_t range = 65535;
    static bool reading = false;
    static uint64_t start_time = 0;

    uint64_t now = time_us_64();
    if (now - start_time > TOF_WAIT_US) {
        reading = false;
    }

    if (reading) {
        if ((read_reg(SYSRANGE_START) & 0x01) == 0) {
            range = readRangeContinuousMillimeters(index);
            reading = false;
        }
    } else {
        write_reg(0x80, 0x01);
        write_reg(0xFF, 0x01);
        write_reg(0x00, 0x00);
        write_reg(0x91, instances[index].stop_variable);
        write_reg(0x00, 0x01);
        write_reg(0xFF, 0x00);
        write_reg(0x80, 0x00);

        write_reg(SYSRANGE_START, 0x01);
        start_time = now;
        reading = true;
    }
    return range;
}
#endif

// Private Methods /////////////////////////////////////////////////////////////

// Get reference SPAD (single photon avalanche diode) count and type
// based on VL53L0X_get_info_from_device(),
// but only gets reference SPAD count and type
bool getSpadInfo(uint8_t *count, bool *type_is_aperture)
{
    write_reg_list(reg_spad0);
    write_reg(0x83, read_reg(0x83) | 0x04);
    write_reg_list(reg_spad1);

    uint64_t start = time_us_64();
    while (read_reg(0x83) == 0x00) {
        if (time_us_64() - start > TOF_WAIT_US) {
            return false;
        }
        sleep_ms(1);
    }

    write_reg(0x83, 0x01);

    uint8_t tmp = read_reg(0x92);
    *count = tmp & 0x7f;
    *type_is_aperture = (tmp & 0x80);

    write_reg(0x81, 0x00);
    write_reg(0xFF, 0x06);

    write_reg(0x83, read_reg(0x83) & ~0x04);

    write_reg_list(reg_spad2);

    return true;
}

// Get sequence step enables
// based on VL53L0X_GetSequenceStepEnables()
void getSequenceStepEnables(SequenceStepEnables * enables)
{
    uint8_t seq_cfg = read_reg(SYSTEM_SEQUENCE_CONFIG);

    enables->tcc         = (seq_cfg >> 4) & 0x1;
    enables->dss         = (seq_cfg >> 3) & 0x1;
    enables->msrc        = (seq_cfg >> 2) & 0x1;
    enables->pre_range   = (seq_cfg >> 6) & 0x1;
    enables->final_range = (seq_cfg >> 7) & 0x1;
}

// Get sequence step timeouts
// based on get_sequence_step_timeout(),
// but gets all timeouts instead of just the requested one, and also stores
// intermediate values
void getSequenceStepTimeouts(SequenceStepEnables const * enables, SequenceStepTimeouts * timeouts)
{
    timeouts->pre_range_vcsel_period_pclks = getVcselPulsePeriod(VcselPeriodPreRange);

    timeouts->msrc_dss_tcc_mclks = read_reg(MSRC_CONFIG_TIMEOUT_MACROP) + 1;
    timeouts->msrc_dss_tcc_us =
        timeoutMclksToMicroseconds(timeouts->msrc_dss_tcc_mclks,
                                   timeouts->pre_range_vcsel_period_pclks);

    timeouts->pre_range_mclks =
        decodeTimeout(read_reg16(PRE_RANGE_CONFIG_TIMEOUT_MACROP_HI));
    timeouts->pre_range_us =
        timeoutMclksToMicroseconds(timeouts->pre_range_mclks,
                                   timeouts->pre_range_vcsel_period_pclks);

    timeouts->final_range_vcsel_period_pclks = getVcselPulsePeriod(VcselPeriodFinalRange);

    timeouts->final_range_mclks =
        decodeTimeout(read_reg16(FINAL_RANGE_CONFIG_TIMEOUT_MACROP_HI));

    if (enables->pre_range) {
        timeouts->final_range_mclks -= timeouts->pre_range_mclks;
    }

    timeouts->final_range_us =
        timeoutMclksToMicroseconds(timeouts->final_range_mclks,
                                   timeouts->final_range_vcsel_period_pclks);
}

// based on VL53L0X_perform_single_ref_calibration()
bool performSingleRefCalibration(uint8_t vhv_init_byte)
{
    write_reg(SYSRANGE_START, 0x01 | vhv_init_byte); // VL53L0X_REG_SYSRANGE_MODE_START_STOP

    uint64_t start = time_us_64();
    while ((read_reg(RESULT_INTERRUPT_STATUS) & 0x07) == 0) {
        if (time_us_64() - start > TOF_WAIT_US) {
            return false;
        }
        sleep_ms(1);
    }

    write_reg(SYSTEM_INTERRUPT_CLEAR, 0x01);
    write_reg(SYSRANGE_START, 0x00);

    return true;
}