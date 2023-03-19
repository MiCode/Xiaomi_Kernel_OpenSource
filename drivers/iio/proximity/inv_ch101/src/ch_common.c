// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2016-2019, Chirp Microsystems.  All rights reserved.
 *
 * Chirp Microsystems CONFIDENTIAL
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL CHIRP MICROSYSTEMS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "soniclib.h"
#include "ch_common.h"
#include "chirp_bsp.h"

static u16 chirp_rx_samples = {0};

void set_output_samples(int ind, int val)
{
		chirp_rx_samples = val;
}

u8 ch_common_set_mode(struct ch_dev_t *dev_ptr, enum ch_mode_t mode)
{
	u8 ret_val = 0;
	u8 opmode_reg;
	u8 period_reg;
	u8 tick_interval_reg;

	if (dev_ptr->part_number == CH101_PART_NUMBER) {
		opmode_reg = CH101_COMMON_REG_OPMODE;
		period_reg = CH101_COMMON_REG_PERIOD;
		tick_interval_reg = CH101_COMMON_REG_TICK_INTERVAL;
	} else {
		opmode_reg = CH201_COMMON_REG_OPMODE;
		period_reg = CH201_COMMON_REG_PERIOD;
		tick_interval_reg = CH201_COMMON_REG_TICK_INTERVAL;
	}

	printf("API: %s: mode: %02x dev_num: %d\n",
		__func__, mode, ch_get_dev_num(dev_ptr));

	if (dev_ptr->sensor_connected) {
		switch (mode) {
		case CH_MODE_IDLE:
			chdrv_write_byte(dev_ptr, opmode_reg, CH_MODE_IDLE);
			chdrv_write_byte(dev_ptr, period_reg, 0);
			// XXX need define
			chdrv_write_word(dev_ptr, tick_interval_reg, 2048);
			break;

		case CH_MODE_FREERUN:
			chdrv_write_byte(dev_ptr, opmode_reg, CH_MODE_FREERUN);
			// XXX need to set period / tick interval (?)
			break;

		case CH_MODE_TRIGGERED_TX_RX:
			chdrv_write_byte(dev_ptr, opmode_reg,
				CH_MODE_TRIGGERED_TX_RX);
			break;

		case CH_MODE_TRIGGERED_RX_ONLY:
			chdrv_write_byte(dev_ptr, opmode_reg,
				CH_MODE_TRIGGERED_RX_ONLY);
			break;

		default:
			ret_val = RET_ERR; // return non-zero to indicate error
			break;
		}
	}

	return ret_val;
}

u8 ch_common_fw_load(struct ch_dev_t *dev_ptr)
{
	u8 ch_err = 0;
	u16 prog_mem_addr;
	u16 fw_size;

	if (dev_ptr->part_number == CH101_PART_NUMBER) {
		prog_mem_addr = CH101_PROG_MEM_ADDR;
		fw_size = CH101_FW_SIZE;
	} else {
		prog_mem_addr = CH201_PROG_MEM_ADDR;
		fw_size = CH201_FW_SIZE;
	}

	printf("API: %s: addr: %02x\n", __func__, prog_mem_addr);

	ch_err = chdrv_prog_mem_write(dev_ptr, prog_mem_addr,
			(u8 *)dev_ptr->firmware, fw_size);
	return ch_err;
}

u8 ch_common_set_sample_interval(struct ch_dev_t *dev_ptr, u16 interval_ms)
{
	u8 period_reg;
	u8 tick_interval_reg;
	u8 ret_val = 0;

	if (dev_ptr->part_number == CH101_PART_NUMBER) {
		period_reg = CH101_COMMON_REG_PERIOD;
		tick_interval_reg = CH101_COMMON_REG_TICK_INTERVAL;
	} else {
		period_reg = CH201_COMMON_REG_PERIOD;
		tick_interval_reg = CH201_COMMON_REG_TICK_INTERVAL;
	}

	printf("API: %s: interval_ms: %u\n", __func__, interval_ms);

	if (dev_ptr->sensor_connected) {
		u32 sample_interval = dev_ptr->rtc_cal_result * interval_ms
			/ dev_ptr->group->rtc_cal_pulse_ms;
		u32 period = (sample_interval / 2048) + 1;   // XXX need define

		if (period > UINT8_MAX) { /* check if result fits in register */
			ret_val = 1;
		}

		if (ret_val == 0) {
			u32 tick_interval = sample_interval / period;

#ifdef CHDRV_DEBUG
			printf("Set period=%u, tick_interval=%u\n",
				period, tick_interval);
#endif

			printf("API: %s: tick_interval: %u dev_num: %d\n",
				__func__, tick_interval,
				ch_get_dev_num(dev_ptr));

			chdrv_write_byte(dev_ptr, period_reg, (u8)period);
			chdrv_write_word(dev_ptr, tick_interval_reg,
				(u16)tick_interval);
		}
	}

	return ret_val;
}

// XXX need comment block
// XXX    note uses actual num_samples, even for CH201
u8 ch_common_set_num_samples(struct ch_dev_t *dev_ptr, u16 num_samples)
{
	u8 max_range_reg;
	// default is error (not connected or num_samples too big)
	u8 ret_val = 1;

	printf("API: %s: num_samples: %u dev_num: %d\n",
		__func__, num_samples, ch_get_dev_num(dev_ptr));


	if (dev_ptr->part_number == CH101_PART_NUMBER) {
		max_range_reg = CH101_COMMON_REG_MAX_RANGE;
	} else {
		max_range_reg = CH201_COMMON_REG_MAX_RANGE;
		// each internal count for CH201 represents 2 physical samples
		num_samples /= 2;
	}

	if (dev_ptr->sensor_connected && num_samples <= UINT8_MAX) {
		ret_val = chdrv_write_byte(dev_ptr, max_range_reg,
			(u8)num_samples);
	}

	return ret_val;
}

u8 ch_common_set_max_range(struct ch_dev_t *dev_ptr, u16 max_range_mm)
{
	u8 ret_val;
	u16 num_samples = 0;
	u16 max_num_samples = 0;

	printf("API: %s: max_range_mm: %u dev_num: %d\n",
		__func__, max_range_mm, ch_get_dev_num(dev_ptr));

	if (dev_ptr->part_number == CH101_PART_NUMBER)
		max_num_samples = CH101_COMMON_NUM_SAMPLES;
	else
		max_num_samples = CH201_COMMON_NUM_SAMPLES;

	ret_val = (!dev_ptr->sensor_connected);

	printf("part_number=%u sensor_connected=%u\n",
		dev_ptr->part_number, dev_ptr->sensor_connected);
	printf("max_num_samples=%u\n", max_num_samples);

	if (!ret_val) {
		num_samples = ch_common_mm_to_samples(dev_ptr, max_range_mm);

		if (chirp_rx_samples != 0)
			num_samples = chirp_rx_samples;

		printf("num_samples=%u max_range_mm=%u\n",
			num_samples, max_range_mm);

		if (num_samples > max_num_samples) {
			num_samples = max_num_samples;
			dev_ptr->max_range = ch_samples_to_mm(dev_ptr,
				num_samples);	// store reduced max range
			printf("max_range=%u num_samples=%u\n",
				dev_ptr->max_range, num_samples);
		} else {
			// store user-specified max range
			dev_ptr->max_range = max_range_mm;
		}

#ifdef CHDRV_DEBUG
		printf("num_samples=%u\n", num_samples);
#endif
	}

	if (dev_ptr->part_number == CH201_PART_NUMBER)
		num_samples *= 2;

	if (!ret_val)
		ret_val = ch_common_set_num_samples(dev_ptr, num_samples);

	if (!ret_val)
		dev_ptr->num_rx_samples = num_samples;
	else
		dev_ptr->num_rx_samples = 0;

#ifdef CHDRV_DEBUG
	printf("Set samples: ret_val: %u  dev_ptr->num_rx_samples: %u\n",
		ret_val, dev_ptr->num_rx_samples);
#endif
	return ret_val;
}

u16 ch_common_mm_to_samples(struct ch_dev_t *dev_ptr, u16 num_mm)
{
	u8 err;
	u16 scale_factor;
	u32 num_samples = 0;
	u32 divisor1;
	u32 divisor2;

	err = (!dev_ptr) || (!dev_ptr->sensor_connected);

	printf("dev_ptr->rtc_cal_result=%u\n", dev_ptr->rtc_cal_result);
	printf("dev_ptr->group->rtc_cal_pulse_ms=%u\n",
		dev_ptr->group->rtc_cal_pulse_ms);

	if (!err) {
		if (dev_ptr->part_number == CH101_PART_NUMBER)
			divisor1 = 0x2000;// (4*16*128)  XXX need define(s)
		else
			divisor1 = 0x4000;// (4*16*128*2)  XXX need define(s)

		printf("dev_ptr->scale_factor=%u\n", dev_ptr->scale_factor);
		if (dev_ptr->scale_factor == 0)
			ch_common_store_scale_factor(dev_ptr);

		printf("dev_ptr->scale_factor=%u\n", dev_ptr->scale_factor);
		scale_factor = dev_ptr->scale_factor;
	}

	if (!err) {
		divisor2 = (dev_ptr->group->rtc_cal_pulse_ms
			* CH_SPEEDOFSOUND_MPS);
		// Two steps of division to avoid needing a type larger
		// than 32 bits
		// Ceiling division to ensure result is at least enough samples
		// to meet specified range
		num_samples = ((dev_ptr->rtc_cal_result * scale_factor)
			+ (divisor1 - 1)) / divisor1;
		num_samples = ((num_samples * num_mm) + (divisor2 - 1))
			/ divisor2;
		err = (num_samples > UINT16_MAX);
		printf("scale_factor=%u\n", scale_factor);
	}

	if (!err) {
		// each internal count for CH201 represents 2 physical samples
		if (dev_ptr->part_number == CH201_PART_NUMBER)
			num_samples *= 2;

		/* Adjust for oversampling, if used */
		num_samples <<= dev_ptr->oversample;
	}
	if (err)
		num_samples = 0;		// return zero if error

	return (u16)num_samples;
}

u16 ch_common_samples_to_mm(struct ch_dev_t *dev_ptr, u16 num_samples)
{
	u32 num_mm = 0;
	u32 op_freq = dev_ptr->op_frequency;

	if (op_freq != 0) {
		num_mm = ((u32)num_samples * CH_SPEEDOFSOUND_MPS * 8
			* 1000) / (op_freq * 2);
	}

	/* Adjust for oversampling, if used */
	num_mm >>= dev_ptr->oversample;

	return (u16)num_mm;
}

u8 ch_common_set_static_range(struct ch_dev_t *dev_ptr, u16 samples)
{
	u8 ret_val = 1;		// default is error return

	printf("API: %s: samples: %u dev_num: %d\n",
		__func__, samples, ch_get_dev_num(dev_ptr));

	if (dev_ptr->part_number == CH101_PART_NUMBER) {	// CH101 only
		if (dev_ptr->sensor_connected) {
			ret_val = chdrv_write_byte(dev_ptr,
				CH101_COMMON_REG_STAT_RANGE, (u8)samples);

			if (!ret_val)
				dev_ptr->static_range = samples;
		}
	}
	return ret_val;
}

u8 ch_common_get_range(struct ch_dev_t *dev_ptr, enum ch_range_t range_type,
	u32 *range)
{
	u8 tof_reg;
	u16 time_of_flight;
	u16 scale_factor;
	u8 err = 1;

	*range = CH_NO_TARGET;

	if (dev_ptr->sensor_connected) {
		if (dev_ptr->part_number == CH101_PART_NUMBER)
			tof_reg = CH101_COMMON_REG_TOF;
		else
			tof_reg = CH201_COMMON_REG_TOF;

		err = chdrv_read_word(dev_ptr, tof_reg, &time_of_flight);

		// If object detected
		if (!err && time_of_flight != UINT16_MAX) {
			if (dev_ptr->scale_factor == 0)
				ch_common_store_scale_factor(dev_ptr);

			scale_factor = dev_ptr->scale_factor;

			if (scale_factor != 0) {
				u32 tmp_range;
				u32 num = (CH_SPEEDOFSOUND_MPS
					* dev_ptr->group->rtc_cal_pulse_ms
					* (u32)time_of_flight);
				u32 den = ((u32)dev_ptr->rtc_cal_result
						* (u32)scale_factor)
						>> 11; // XXX need define

				tmp_range = (num / den);

				if (dev_ptr->part_number == CH201_PART_NUMBER)
					tmp_range *= 2;

				if (range_type == CH_RANGE_ECHO_ONE_WAY)
					tmp_range /= 2;

				/* Adjust for oversampling, if used */
				tmp_range >>= dev_ptr->oversample;
				*range = tmp_range;
			}
		}
	}

	printf("API: %s: range: %u dev_num: %d\n",
		__func__, *range, ch_get_dev_num(dev_ptr));

	return err;
}

u8 ch_common_get_amplitude(struct ch_dev_t *dev_ptr, u16 *amplitude)
{
	u8 amplitude_reg;
	u8 err = 1;

	if (dev_ptr->sensor_connected) {
		if (dev_ptr->part_number == CH101_PART_NUMBER)
			amplitude_reg = CH101_COMMON_REG_AMPLITUDE;
		else
			amplitude_reg = CH201_COMMON_REG_AMPLITUDE;

		err = chdrv_read_word(dev_ptr, amplitude_reg, amplitude);
	}

	printf("API: %s: amplitude: %u dev_num: %d\n",
		__func__, *amplitude, ch_get_dev_num(dev_ptr));

	return err;
}

u8 ch_common_get_locked_state(struct ch_dev_t *dev_ptr)
{
	u8 ready_reg;
	u8 lock_mask;
	u8 ret_val = 0;

	if (dev_ptr->part_number == CH101_PART_NUMBER) {
		ready_reg = CH101_COMMON_REG_READY;
		lock_mask = CH101_COMMON_READY_FREQ_LOCKED;
	} else {
		ready_reg = CH201_COMMON_REG_READY;
		lock_mask = CH201_COMMON_READY_FREQ_LOCKED;
	}

	if (dev_ptr->sensor_connected) {
		u8 ready_value = 0;

		chdrv_read_byte(dev_ptr, ready_reg, &ready_value);
		if (ready_value & lock_mask)
			ret_val = 1;
	}

	printf("API: %s: lock_mask: %u dev_num: %d\n",
		__func__, lock_mask, ch_get_dev_num(dev_ptr));

	return ret_val;
}

void ch_common_prepare_pulse_timer(struct ch_dev_t *dev_ptr)
{
	u8 cal_trig_reg;

	printf("API: %s: dev_num: %d\n", __func__, ch_get_dev_num(dev_ptr));

	if (dev_ptr->part_number == CH101_PART_NUMBER)
		cal_trig_reg = CH101_COMMON_REG_CAL_TRIG;
	else
		cal_trig_reg = CH201_COMMON_REG_CAL_TRIG;

	chdrv_write_byte(dev_ptr, cal_trig_reg, 0);
}

void ch_common_store_pt_result(struct ch_dev_t *dev_ptr)
{
	u8 pt_result_reg;
	u16 rtc_cal_result;

	printf("API: %s: dev_num: %d\n", __func__, ch_get_dev_num(dev_ptr));

	if (dev_ptr->part_number == CH101_PART_NUMBER)
		pt_result_reg = CH101_COMMON_REG_CAL_RESULT;
	else
		pt_result_reg = CH201_COMMON_REG_CAL_RESULT;

	chdrv_read_word(dev_ptr, pt_result_reg, &rtc_cal_result);
	dev_ptr->rtc_cal_result = rtc_cal_result;
}

void ch_common_store_op_freq(struct ch_dev_t *dev_ptr)
{
	u8 tof_sf_reg;
	u16 raw_freq;		// aka scale factor
	u32 freq_counter_cycles;
	u32 num;
	u32 den;
	u32 op_freq;

	printf("API: %s: dev_num: %d\n", __func__, ch_get_dev_num(dev_ptr));

	if (dev_ptr->part_number == CH101_PART_NUMBER) {
		tof_sf_reg = CH101_COMMON_REG_TOF_SF;
		freq_counter_cycles = CH101_FREQCOUNTERCYCLES;
	} else {
		tof_sf_reg = CH201_COMMON_REG_TOF_SF;
		freq_counter_cycles = CH201_FREQCOUNTERCYCLES;
	}

	chdrv_read_word(dev_ptr, tof_sf_reg, &raw_freq);

	num = (u32)(((dev_ptr->rtc_cal_result) * 1000U)
		/ (16U * freq_counter_cycles)) * (u32)(raw_freq);
	den = (u32)(dev_ptr->group->rtc_cal_pulse_ms);
	op_freq = (num / den);

	dev_ptr->op_frequency = op_freq;
}

void ch_common_store_bandwidth(struct ch_dev_t *dev_ptr)
{
	/*
	 * Not supported in current GPR firmware
	 */
}

void ch_common_store_scale_factor(struct ch_dev_t *dev_ptr)
{
	u8 err;
	u8 tof_sf_reg;
	u16 scale_factor;

	printf("API: %s: dev_num: %d\n", __func__, ch_get_dev_num(dev_ptr));

	if (dev_ptr->part_number == CH101_PART_NUMBER)
		tof_sf_reg = CH101_COMMON_REG_TOF_SF;
	else
		tof_sf_reg = CH201_COMMON_REG_TOF_SF;

	err = chdrv_read_word(dev_ptr, tof_sf_reg, &scale_factor);
	printf("tof_sf_reg=%02x scale_factor=%u err=%u\n",
		tof_sf_reg, scale_factor, err);

	if (!err)
		dev_ptr->scale_factor = scale_factor;
	else
		dev_ptr->scale_factor = 0;
}

// XXX  Need comment block
u8 ch_common_set_thresholds(struct ch_dev_t *dev_ptr,
	struct ch_thresholds_t *thresholds_ptr)
{
	u8 thresh_len_reg = 0;// offset of register for this threshold's length
	u8 thresh_level_reg;	// threshold level reg (first in array)
	u8 max_num_thresholds;
	int ret_val = 1;		// default return = error
	u8 thresh_num;
	u8 thresh_len;
	u16 thresh_level;
	u16 start_sample = 0;

	printf("API: %s: dev_num: %d\n", __func__, ch_get_dev_num(dev_ptr));

	if (dev_ptr->sensor_connected) {
		if (dev_ptr->part_number == CH101_PART_NUMBER) {
			return ret_val;		// NOT SUPPORTED in CH101

		} else {
			thresh_level_reg = CH201_COMMON_REG_THRESHOLDS;
			max_num_thresholds = CH201_COMMON_NUM_THRESHOLDS;
		}

		for (thresh_num = 0; thresh_num < max_num_thresholds;
			thresh_num++) {
			if (thresh_num < (max_num_thresholds - 1)) {
				u16 next_start_sample =
					thresholds_ptr->threshold
						[thresh_num + 1].start_sample;

				thresh_len = (next_start_sample - start_sample);
				start_sample = next_start_sample;
			} else {
				thresh_len = 0;
			}

			if (dev_ptr->part_number == CH201_PART_NUMBER) {
				if (thresh_num == 0) {
					thresh_len_reg =
					CH201_COMMON_REG_THRESH_LEN_0;
				} else if (thresh_num == 1) {
					thresh_len_reg =
					CH201_COMMON_REG_THRESH_LEN_1;
				} else if (thresh_num == 2) {
					thresh_len_reg =
					CH201_COMMON_REG_THRESH_LEN_2;
				} else if (thresh_num == 3) {
					thresh_len_reg =
					CH201_COMMON_REG_THRESH_LEN_3;
				} else if (thresh_num == 4) {
					thresh_len_reg =
					CH201_COMMON_REG_THRESH_LEN_4;
				} else if (thresh_num == 5) {
					// last threshold does not have length
					// field - assumed to extend to end
					// of data
					thresh_len_reg = 0;
				}
			}

			// set the length field (if any) for this threshold
			if (thresh_len_reg != 0) {
				chdrv_write_byte(dev_ptr, thresh_len_reg,
					thresh_len);
			}

			// write level to this threshold's entry in
			// register array
			thresh_level =
				thresholds_ptr->threshold[thresh_num].level;
			chdrv_write_word(dev_ptr,
				(thresh_level_reg
					+ (thresh_num * sizeof(u16))),
				thresh_level);
		}

		ret_val = 0;	// return OK
	}
	return ret_val;
}

// XXX Need comment block

u8 ch_common_get_thresholds(struct ch_dev_t *dev_ptr,
	struct ch_thresholds_t *thresholds_ptr)
{
	u8 thresh_len_reg = 0;	// offset of register for this threshold length
	u8 thresh_level_reg;	// threshold level reg (first in array)
	u8 max_num_thresholds;
	u8 ret_val = 1;		// default = error return
	u8 thresh_num;
	u8 thresh_len = 0;	// number of samples described by each threshold
	u16 start_sample = 0;	// calculated start sample for each threshold

	printf("API: %s: dev_num: %d\n", __func__, ch_get_dev_num(dev_ptr));

	if (dev_ptr->sensor_connected && thresholds_ptr != NULL) {
		if (dev_ptr->part_number == CH101_PART_NUMBER)
			return ret_val;		// NOT SUPPORTED in CH101

		thresh_level_reg = CH201_COMMON_REG_THRESHOLDS;
		max_num_thresholds = CH201_COMMON_NUM_THRESHOLDS;

		for (thresh_num = 0; thresh_num < max_num_thresholds;
			thresh_num++) {
			if (dev_ptr->part_number == CH201_PART_NUMBER) {
				if (thresh_num == 0) {
					thresh_len_reg =
					CH201_COMMON_REG_THRESH_LEN_0;
				} else if (thresh_num == 1) {
					thresh_len_reg =
					CH201_COMMON_REG_THRESH_LEN_1;
				} else if (thresh_num == 2) {
					thresh_len_reg =
					CH201_COMMON_REG_THRESH_LEN_2;
				} else if (thresh_num == 3) {
					thresh_len_reg =
					CH201_COMMON_REG_THRESH_LEN_3;
				} else if (thresh_num == 4) {
					thresh_len_reg =
					CH201_COMMON_REG_THRESH_LEN_4;
				} else if (thresh_num == 5) {
					// last threshold does not have length
					// field - assumed to extend to end
					// of data
					thresh_len_reg = 0;
				}
			}

			// read the length field register for this threshold
			if (thresh_len_reg != 0) {
				chdrv_read_byte(dev_ptr, thresh_len_reg,
					&thresh_len);
			} else {
				thresh_len = 0;
			}

			thresholds_ptr->threshold[thresh_num].start_sample =
				start_sample;
			// increment start sample for next threshold
			start_sample += thresh_len;

			// get level from this threshold's entry in
			// register array
			chdrv_read_word(dev_ptr,
				(thresh_level_reg + (thresh_num * sizeof(u16))),
				&thresholds_ptr->threshold[thresh_num].level);
		}
		ret_val = 0;	// return OK
	}
	return ret_val;
}

// XXX need comment block
u8 ch_common_get_iq_data(struct ch_dev_t *dev_ptr,
	struct ch_iq_sample_t *buf_ptr, u16 start_sample, u16 num_samples,
	enum ch_io_mode_t mode)
{
	u16 iq_data_addr;
	u16 max_samples;
	struct ch_group_t *grp_ptr = dev_ptr->group;
	int error = 1;

	printf("API: %s: dev_num: %d\n", __func__, ch_get_dev_num(dev_ptr));

	if (dev_ptr->part_number == CH101_PART_NUMBER) {
		iq_data_addr = CH101_COMMON_REG_DATA;
		max_samples = CH101_COMMON_NUM_SAMPLES;
	} else {
		iq_data_addr = CH201_COMMON_REG_DATA;
		max_samples = CH201_COMMON_NUM_SAMPLES;
	}

	iq_data_addr += (start_sample * sizeof(struct ch_iq_sample_t));

	if (num_samples != 0 && (start_sample + num_samples) <= max_samples) {
		u16 num_bytes = (num_samples * sizeof(struct ch_iq_sample_t));

		if (mode == CH_IO_MODE_BLOCK) {
#ifdef USE_STD_I2C_FOR_IQ
			/* blocking transfer - use standard I2C interface */
			error = chdrv_burst_read(dev_ptr, iq_data_addr,
				(u8 *)buf_ptr, num_bytes);
#else
			/* blocking transfer - use low-level programming
			 * interface for speed
			 */
			int num_transfers = (num_bytes + CH_PROG_XFER_SIZE - 1)
				/ CH_PROG_XFER_SIZE;
			int bytes_left = num_bytes;   // remaining bytes to read

			/* Convert register offsets to full memory addresses */
			if (dev_ptr->part_number == CH101_PART_NUMBER)
				iq_data_addr += CH101_DATA_MEM_ADDR +
					CH101_COMMON_I2CREGS_OFFSET;
			else
				iq_data_addr += CH201_DATA_MEM_ADDR +
					CH201_COMMON_I2CREGS_OFFSET;

			// assert PROG pin
			chbsp_program_enable(dev_ptr);

			for (int xfer = 0; xfer < num_transfers; xfer++) {
				int bytes_to_read;

				// read burst command
				u8 message[] = {(0x80 | CH_PROG_REG_CTL), 0x09};

				if (bytes_left > CH_PROG_XFER_SIZE)
					bytes_to_read = CH_PROG_XFER_SIZE;
				else
					bytes_to_read = bytes_left;

				chdrv_prog_write(dev_ptr, CH_PROG_REG_ADDR,
					(iq_data_addr +
						(xfer * CH_PROG_XFER_SIZE)));
				chdrv_prog_write(dev_ptr, CH_PROG_REG_CNT,
					bytes_to_read - 1);
				error = chdrv_prog_i2c_write(dev_ptr, message,
					sizeof(message));
				error |= chdrv_prog_i2c_read(dev_ptr,
					(u8 *)(buf_ptr) +
					(xfer * CH_PROG_XFER_SIZE),
					bytes_to_read);

				bytes_left -= bytes_to_read;
			}

			// de-assert PROG pin
			chbsp_program_disable(dev_ptr);
#endif	// USE_STD_I2C_FOR_IQ

		} else {
			/* non-blocking transfer - queue a read transaction
			 * (must be started using ch_io_start_nb() )
			 */

			if (grp_ptr->i2c_drv_flags & I2C_DRV_FLAG_USE_PROG_NB) {
				/* Use low-level programming interface
				 * to read data
				 */

				/* Convert register offsets to full memory
				 * addresses
				 */
				if (dev_ptr->part_number == CH101_PART_NUMBER) {
					iq_data_addr += (CH101_DATA_MEM_ADDR
						+ CH101_COMMON_I2CREGS_OFFSET);
				} else {
					iq_data_addr += (CH201_DATA_MEM_ADDR
						+ CH201_COMMON_I2CREGS_OFFSET);
				}

				error = chdrv_group_i2c_queue(grp_ptr, dev_ptr,
					1, CHDRV_NB_TRANS_TYPE_PROG,
					iq_data_addr, num_bytes,
					(u8 *)buf_ptr);
			} else {
				/* Use regular I2C register interface
				 * to read data
				 */
				error = chdrv_group_i2c_queue(grp_ptr, dev_ptr,
					1, CHDRV_NB_TRANS_TYPE_STD,
					iq_data_addr, num_bytes,
					(u8 *)buf_ptr);
			}
		}
	}

	return error;
}

