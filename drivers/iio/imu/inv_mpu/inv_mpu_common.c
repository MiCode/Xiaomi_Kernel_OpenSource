/*
 * Copyright (C) 2012-2017 InvenSense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "inv_mpu: " fmt
#include "inv_mpu_iio.h"
#ifdef CONFIG_RTC_INTF_ALARM
#include <linux/android_alarm.h>
#endif
#include <linux/export.h>

#ifdef CONFIG_RTC_INTF_ALARM
s64 get_time_ns(void)
{
	struct timespec ts;

	/* get_monotonic_boottime(&ts); */

	/* Workaround for some platform on which monotonic clock and
	 * Android SystemClock has a gap.
	 * Use ktime_to_timespec(alarm_get_elapsed_realtime()) instead of
	 * get_monotonic_boottime() for these platform
	 */

	ts = ktime_to_timespec(alarm_get_elapsed_realtime());

	return timespec_to_ns(&ts);
}
#else
s64 get_time_ns(void)
{
	struct timespec ts;

	get_monotonic_boottime(&ts);

	/* Workaround for some platform on which monotonic clock and
	 * Android SystemClock has a gap.
	 * Use ktime_to_timespec(alarm_get_elapsed_realtime()) instead of
	 * get_monotonic_boottime() for these platform
	 */
	return timespec_to_ns(&ts);
}

#endif

#ifdef ACCEL_BIAS_TEST
int inv_get_3axis_average(s16 src[], s16 dst[], s16 reset)
{
#define BUFFER_SIZE 200
	static s16 buffer[BUFFER_SIZE][3];
	static s16 current_position = 0;
	static s16 ready = 0;
	int sum[3]= {0,};
	int i;

	if(reset){
		current_position = 0;
		ready = 0;
	}
	buffer[current_position][0] = src[0];
	buffer[current_position][1] = src[1];
	buffer[current_position][2] = src[2];
	current_position++;
	if(current_position == BUFFER_SIZE){
		ready = 1;
		current_position = 0;
	}
	if(ready){
		for(i = 0 ; i < BUFFER_SIZE ; i++){
			sum[0] += buffer[i][0];
			sum[1] += buffer[i][1];
			sum[2] += buffer[i][2];
		}
		dst[0] = sum[0]/BUFFER_SIZE;
		dst[1] = sum[1]/BUFFER_SIZE;
		dst[2] = sum[2]/BUFFER_SIZE;
		return 1;
	}
	return 0;
}
#endif

int inv_q30_mult(int a, int b)
{
#define DMP_MULTI_SHIFT                 30
	u64 temp;
	int result;

	temp = ((u64)a) * b;
	result = (int)(temp >> DMP_MULTI_SHIFT);

	return result;
}
#if defined(CONFIG_INV_MPU_IIO_ICM20648) || \
					defined(CONFIG_INV_MPU_IIO_ICM20690)
/* inv_read_secondary(): set secondary registers for reading.
   The chip must be set as bank 3 before calling.
 */
int inv_read_secondary(struct inv_mpu_state *st, int ind, int addr,
		       int reg, int len)
{
	int result;

	result = inv_plat_single_write(st, st->slv_reg[ind].addr,
				       INV_MPU_BIT_I2C_READ | addr);
	if (result)
		return result;
	result = inv_plat_single_write(st, st->slv_reg[ind].reg, reg);
	if (result)
		return result;
	result = inv_plat_single_write(st, st->slv_reg[ind].ctrl,
				       INV_MPU_BIT_SLV_EN | len);

	return result;
}

int inv_execute_read_secondary(struct inv_mpu_state *st, int ind, int addr,
			       int reg, int len, u8 *d)
{
	int result;

	inv_set_bank(st, BANK_SEL_3);
	result = inv_read_secondary(st, ind, addr, reg, len);
	if (result)
		return result;
	inv_set_bank(st, BANK_SEL_0);
	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis |
				       BIT_I2C_MST_EN);
	msleep(SECONDARY_INIT_WAIT);
	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);
	if (result)
		return result;
	result = inv_plat_read(st, REG_EXT_SLV_SENS_DATA_00, len, d);

	return result;
}

/* inv_write_secondary(): set secondary registers for writing.
   The chip must be set as bank 3 before calling.
 */
int inv_write_secondary(struct inv_mpu_state *st, int ind, int addr,
			int reg, int v)
{
	int result;

	result = inv_plat_single_write(st, st->slv_reg[ind].addr, addr);
	if (result)
		return result;
	result = inv_plat_single_write(st, st->slv_reg[ind].reg, reg);
	if (result)
		return result;
	result = inv_plat_single_write(st, st->slv_reg[ind].ctrl,
				       INV_MPU_BIT_SLV_EN | 1);

	result = inv_plat_single_write(st, st->slv_reg[ind].d0, v);

	return result;
}

int inv_execute_write_secondary(struct inv_mpu_state *st, int ind, int addr,
				int reg, int v)
{
	int result;

	inv_set_bank(st, BANK_SEL_3);
	result = inv_write_secondary(st, ind, addr, reg, v);
	if (result)
		return result;
	inv_set_bank(st, BANK_SEL_0);
	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis |
				       BIT_I2C_MST_EN);
	msleep(SECONDARY_INIT_WAIT);
	result = inv_plat_single_write(st, REG_USER_CTRL, st->i2c_dis);

	return result;
}

int inv_set_bank(struct inv_mpu_state *st, u8 bank)
{
#ifdef CONFIG_INV_MPU_IIO_ICM20648
	int r;

	r = inv_plat_single_write(st, REG_BANK_SEL, bank);

	return r;
#else
	return 0;
#endif
}
#endif

#ifdef CONFIG_INV_MPU_IIO_ICM20648
/**
 *  inv_write_cntl() - Write control word to designated address.
 *  @st:	Device driver instance.
 *  @wd:        control word.
 *  @en:	enable/disable.
 *  @cntl:	control address to be written.
 */
int inv_write_cntl(struct inv_mpu_state *st, u16 wd, bool en, int cntl)
{
	int result;
	u8 reg[2], d_out[2];

	result = mem_r(cntl, 2, d_out);
	if (result)
		return result;
	reg[0] = ((wd >> 8) & 0xff);
	reg[1] = (wd & 0xff);
	if (!en) {
		d_out[0] &= ~reg[0];
		d_out[1] &= ~reg[1];
	} else {
		d_out[0] |= reg[0];
		d_out[1] |= reg[1];
	}
	result = mem_w(cntl, 2, d_out);

	return result;
}
#endif

int inv_set_power(struct inv_mpu_state *st, bool power_on)
{
	u8 d;
	int r;

	if ((!power_on) == st->chip_config.is_asleep)
		return 0;

	d = BIT_CLK_PLL;
	if (!power_on)
		d |= BIT_SLEEP;

	r = inv_plat_single_write(st, REG_PWR_MGMT_1, d);
	if (r)
		return r;

	if (power_on)
		usleep_range(REG_UP_TIME_USEC, REG_UP_TIME_USEC);

	st->chip_config.is_asleep = !power_on;

	return 0;
}
EXPORT_SYMBOL_GPL(inv_set_power);

int inv_stop_interrupt(struct inv_mpu_state *st)
{
	int res;
#if defined(CONFIG_INV_MPU_IIO_ICM20648)
	/* disable_irq_wake alone should work already. However,
	   it might need system configuration change. From driver side,
	   we will disable IRQ altogether for non-wakeup sensors. */
	res = inv_plat_read(st, REG_INT_ENABLE, 1, &st->int_en);
	if (res)
		return res;
	res = inv_plat_read(st, REG_INT_ENABLE_2, 1, &st->int_en_2);
	if (res)
		return res;
	res = inv_plat_single_write(st, REG_INT_ENABLE, 0);
	if (res)
		return res;
	res = inv_plat_single_write(st, REG_INT_ENABLE_2, 0);
	if (res)
		return res;
#endif
#if defined(CONFIG_INV_MPU_IIO_ICM20608D)
	res = inv_plat_read(st, REG_INT_ENABLE, 1, &st->int_en);
	if (res)
		return res;
	res = inv_plat_single_write(st, REG_INT_ENABLE, 0);
	if (res)
		return res;
#endif
#if defined(CONFIG_INV_MPU_IIO_ICM20602) \
	|| defined(CONFIG_INV_MPU_IIO_ICM20690) \
	|| defined(CONFIG_INV_MPU_IIO_IAM20680)	
	res = inv_plat_read(st, REG_INT_ENABLE, 1, &st->int_en);
	if (res)
		return res;
	res = inv_plat_single_write(st, REG_INT_ENABLE, 0);
	if (res)
		return res;
#endif
	return 0;
}
int inv_reenable_interrupt(struct inv_mpu_state *st)
{
	int res = 0;
#if defined(CONFIG_INV_MPU_IIO_ICM20648)
	res = inv_plat_single_write(st, REG_INT_ENABLE, st->int_en);
	if (res)
		return res;
	res = inv_plat_single_write(st, REG_INT_ENABLE_2, st->int_en_2);
	if (res)
		return res;
#elif defined(CONFIG_INV_MPU_IIO_ICM20608D)
	res = inv_plat_single_write(st, REG_INT_ENABLE, st->int_en);
	if (res)
		return res;
#endif
#if defined(CONFIG_INV_MPU_IIO_ICM20602) \
	|| defined(CONFIG_INV_MPU_IIO_ICM20690) \
	|| defined(CONFIG_INV_MPU_IIO_IAM20680)	
	res = inv_plat_single_write(st, REG_INT_ENABLE, st->int_en);
	if (res)
		return res;
#endif
	return res;
}

static int inv_lp_en_off_mode(struct inv_mpu_state *st, bool on)
{
	int r;

	if (!st->chip_config.is_asleep)
		return 0;

	r = inv_plat_single_write(st, REG_PWR_MGMT_1, BIT_CLK_PLL);
	st->chip_config.is_asleep = 0;

	return r;
}
#ifdef CONFIG_INV_MPU_IIO_ICM20648
static int inv_lp_en_on_mode(struct inv_mpu_state *st, bool on)
{
	int r = 0;
	u8 w;

	if ((!st->chip_config.is_asleep) &&
	    ((!on) == st->chip_config.lp_en_set))
		return 0;

	w = BIT_CLK_PLL;
	if ((!on) && (!st->eis.eis_triggered))
		w |= BIT_LP_EN;
	r = inv_plat_single_write(st, REG_PWR_MGMT_1, w);
	st->chip_config.is_asleep = 0;
	st->chip_config.lp_en_set = (!on);
	return r;
}
#endif
#if defined(CONFIG_INV_MPU_IIO_ICM20602) \
	|| defined(CONFIG_INV_MPU_IIO_ICM20690) \
	|| defined(CONFIG_INV_MPU_IIO_IAM20680)	
int inv_set_accel_config2(struct inv_mpu_state *st, bool cycle_mode)
{
	int cycle_freq[] = {275, 192, 111, 59};
	int cont_freq[] = {219, 219, 99, 45, 22, 11, 6};
	int i, r, rate;
	u8 v;

	v = 0;
#ifdef CONFIG_INV_MPU_IIO_ICM20690
	v |= BIT_FIFO_SIZE_1K;
#endif
	if (cycle_mode) {
		rate = (st->eng_info[ENGINE_ACCEL].running_rate << 1);
		i = ARRAY_SIZE(cycle_freq) - 1;
		while (i > 0) {
			if (rate < cycle_freq[i]) {
				break;
			}
			i--;
		}
		r = inv_plat_single_write(st, REG_ACCEL_CONFIG_2, v |
								(i << 4) | 7);
		if (r)
			return r;
	} else {
		rate = (st->eng_info[ENGINE_ACCEL].running_rate >> 1);
		for (i = 1; i < ARRAY_SIZE(cont_freq); i++) {
			if (rate >= cont_freq[i])
				break;
		}
		if (i > 6)
			i = 6;
		r = inv_plat_single_write(st, REG_ACCEL_CONFIG_2, v | i);
		if (r)
			return r;
	}

	return 0;
}
static int inv_lp_en_on_mode(struct inv_mpu_state *st, bool on)
{
	int r = 0;
	u8 w;
	bool cond_check;

	if ((!st->chip_config.is_asleep) &&
					((!on) == st->chip_config.lp_en_set))
		return 0;
	cond_check = (!on) && st->cycle_on;

	w = BIT_CLK_PLL;
	r = inv_plat_single_write(st, REG_PWR_MGMT_1, w);
	if (cond_check) {
		w |= BIT_LP_EN;
		inv_set_accel_config2(st, true);
		st->chip_config.lp_en_set = true;
		r = inv_plat_single_write(st, REG_PWR_MGMT_1, w);
	} else {
		inv_set_accel_config2(st, false);
#ifdef CONFIG_INV_MPU_IIO_ICM20690
		r = inv_plat_single_write(st, REG_PWR_MGMT_1, w | BIT_SLEEP);
		if (r)
			return r;
#endif
		st->chip_config.lp_en_set = false;
		r = inv_plat_single_write(st, REG_PWR_MGMT_1, w);
		msleep(10);
	}
	st->chip_config.is_asleep = 0;

	return r;
}
#endif
#ifdef CONFIG_INV_MPU_IIO_ICM20608D
static int inv_set_accel_config2(struct inv_mpu_state *st)
{
	int cont_freq[] = {219, 219, 99, 45, 22, 11, 6};
	int dec2_cfg = 0;
	int i, r, rate;

	rate = (st->eng_info[ENGINE_ACCEL].running_rate << 1);
	i = 0;
	if (!st->chip_config.eis_enable){
		while ((rate < cont_freq[i]) && (i < ARRAY_SIZE(cont_freq) - 1))
			i++;
		dec2_cfg = 2<<4; //4x
	}
	r = inv_plat_single_write(st, REG_ACCEL_CONFIG_2, i | dec2_cfg);
	if (r)
		return r;
	return 0;
}
static int inv_lp_en_on_mode(struct inv_mpu_state *st, bool on)
{
	int r = 0;
	u8 w;

	w = BIT_CLK_PLL;
	if ((!on) && (!st->chip_config.eis_enable))
		w |= BIT_LP_EN;
	inv_set_accel_config2(st);
	r = inv_plat_single_write(st, REG_PWR_MGMT_1, w);

	return r;
}
#endif
int inv_switch_power_in_lp(struct inv_mpu_state *st, bool on)
{
	int r;

	if (st->chip_config.lp_en_mode_off)
		r = inv_lp_en_off_mode(st, on);
	else
		r = inv_lp_en_on_mode(st, on);

	return r;
}
EXPORT_SYMBOL_GPL(inv_switch_power_in_lp);

int write_be16_to_mem(struct inv_mpu_state *st, u16 data, int addr)
{
	u8 d[2];

	d[0] = (data >> 8) & 0xff;
	d[1] = data & 0xff;

	return mem_w(addr, sizeof(d), d);
}

int write_be32_to_mem(struct inv_mpu_state *st, u32 data, int addr)
{
	cpu_to_be32s(&data);
	return mem_w(addr, sizeof(data), (u8 *)&data);
}

int read_be16_from_mem(struct inv_mpu_state *st, u16 *o, int addr)
{
	int result;
	u8 d[2];

	result = mem_r(addr, 2, (u8 *) &d);
	*o = d[0] << 8 | d[1];

	return result;
}

int read_be32_from_mem(struct inv_mpu_state *st, u32 *o, int addr)
{
	int result;
	u32 d = 0;

	result = mem_r(addr, 4, (u8 *) &d);
	*o = be32_to_cpup((__be32 *)(&d));

	return result;
}

int be32_to_int(u8 *d)
{
	return (d[0] << 24) | (d[1] << 16) | (d[2] << 8) | d[3];
}

u32 inv_get_cntr_diff(u32 curr_counter, u32 prev)
{
	u32 diff;

	if (curr_counter > prev)
		diff = curr_counter - prev;
	else
		diff = 0xffffffff - prev + curr_counter + 1;

	return diff;
}

int inv_write_2bytes(struct inv_mpu_state *st, int addr, int data)
{
	u8 d[2];

	if (data < 0 || data > USHRT_MAX)
		return -EINVAL;

	d[0] = (u8) ((data >> 8) & 0xff);
	d[1] = (u8) (data & 0xff);

	return mem_w(addr, ARRAY_SIZE(d), d);
}



int inv_process_eis(struct inv_mpu_state *st, u16 delay)
{
	int tmp1, tmp2, tmp3;

	switch (st->eis.voting_state) {
	case 0:
		st->eis.gyro_counter_s[0] = st->eis.gyro_counter;
		st->eis.fsync_delay_s[0] = delay - st->eis.fsync_delay;
		st->eis.voting_count = 1;
		st->eis.voting_count_sub = 0;
		st->eis.voting_state = 1;
		break;
	case 1:
		if (abs(st->eis.gyro_counter_s[0] -
						st->eis.gyro_counter) <= 1) {
			st->eis.voting_count++;
		} else {
			st->eis.gyro_counter_s[2] = st->eis.gyro_counter;
			st->eis.voting_count_sub++;
			st->eis.voting_state = 2;
		}
		if (st->eis.voting_count > 5)
			st->eis.voting_state = 3;
		break;
	case 2:
		tmp1 = abs(st->eis.gyro_counter_s[0] - st->eis.gyro_counter);
		tmp2 = abs(st->eis.gyro_counter_s[2] - st->eis.gyro_counter);

		if ((tmp1 < tmp2) && (tmp1 <= 1))
			st->eis.voting_count++;
		else
			st->eis.voting_count_sub++;
		if (st->eis.voting_count > 5) {
			st->eis.voting_state = 3;
			st->eis.voting_count = 0;
			st->eis.voting_count_sub = 0;
		}

		if (st->eis.voting_count_sub > 5) {
			st->eis.gyro_counter_s[0] = st->eis.gyro_counter;
			st->eis.fsync_delay_s[0] = delay - st->eis.fsync_delay;
			st->eis.voting_state = 1;
			st->eis.voting_count = 1;
			st->eis.voting_count_sub = 0;
		}
		break;
	case 3:
		tmp1 = abs(st->eis.gyro_counter_s[0] - st->eis.gyro_counter);
		if (tmp1 == 1) {
			st->eis.gyro_counter_s[1] = st->eis.gyro_counter;
			st->eis.fsync_delay_s[1] = delay - st->eis.fsync_delay;
			st->eis.voting_state = 4;
			st->eis.voting_count_sub = 1;
			st->eis.voting_count = 1;
		}
		break;
	case 4:
		if (st->eis.gyro_counter == st->eis.gyro_counter_s[0]) {
			tmp1 = delay - st->eis.fsync_delay;
			tmp2 = abs(tmp1 - st->eis.fsync_delay_s[0]);
			if (tmp2 < 3) {
				st->eis.voting_count++;
			} else {
				st->eis.fsync_delay_s[2] = tmp1;
				st->eis.voting_count_sub = 1;
				st->eis.voting_state = 5;
			}
			if (st->eis.voting_count > 5) {
				st->eis.voting_count = 1;
				st->eis.voting_state = 6;
			}
		}
		break;
	case 5:
		if (st->eis.gyro_counter == st->eis.gyro_counter_s[0]) {
			tmp1 = delay - st->eis.fsync_delay;

			tmp2 = abs(tmp1 - st->eis.fsync_delay_s[0]);
			tmp3 = abs(tmp1 - st->eis.fsync_delay_s[2]);
			if ((tmp2 < tmp3) && (tmp2 < 3))
				st->eis.voting_count++;
			else
				st->eis.voting_count_sub++;
			if ((st->eis.voting_count > 5) &&
					(st->eis.voting_count_sub
					< st->eis.voting_count)) {
				st->eis.voting_state = 6;
				st->eis.voting_count = 1;
			} else if (st->eis.voting_count_sub > 5) {
				st->eis.fsync_delay_s[0] = tmp1;
				st->eis.voting_state = 4;
				st->eis.voting_count = 1;
			}

		}
		break;
	case 6:
		if (st->eis.gyro_counter == st->eis.gyro_counter_s[1]) {
			tmp1 = delay - st->eis.fsync_delay;
			tmp2 = abs(tmp1 - st->eis.fsync_delay_s[1]);
			if (tmp2 < 3) {
				st->eis.voting_count++;
			} else {
				st->eis.fsync_delay_s[2] = tmp1;
				st->eis.voting_count_sub = 1;
				st->eis.voting_count = 1;
				st->eis.voting_state = 7;
			}
			if (st->eis.voting_count > 5)
				st->eis.voting_state = 8;
		}
		break;
	case 7:
		if (st->eis.gyro_counter == st->eis.gyro_counter_s[1]) {
			tmp1 = delay - st->eis.fsync_delay;

			tmp2 = abs(tmp1 - st->eis.fsync_delay_s[1]);
			tmp3 = abs(tmp1 - st->eis.fsync_delay_s[2]);
			if ((tmp2 < tmp3) && (tmp2 < 3))
				st->eis.voting_count++;
			else
				st->eis.voting_count_sub++;
			if ((st->eis.voting_count > 5) &&
					(st->eis.voting_count_sub
					< st->eis.voting_count)) {
				st->eis.voting_state = 8;
			} else if (st->eis.voting_count_sub > 5) {
				st->eis.fsync_delay_s[1] = tmp1;
				st->eis.voting_state = 6;
				st->eis.voting_count = 1;
			}

		}
		break;
	default:
		break;
	}

	pr_debug("de= %d gc= %d\n", delay, st->eis.gyro_counter);
	st->eis.fsync_delay = delay;
	st->eis.gyro_counter = 0;

	pr_debug("state=%d g1= %d d1= %d g2= %d d2= %d\n",
			st->eis.voting_state,
			st->eis.gyro_counter_s[0],
			st->eis.fsync_delay_s[0],
			st->eis.gyro_counter_s[1],
			st->eis.fsync_delay_s[1]);

	return 0;
}

int inv_rate_convert(struct inv_mpu_state *st, int ind, int data)
{
	int t, out, out1, out2;
	int base_freq;

	if (data <= MPU_DEFAULT_DMP_FREQ)
		base_freq = MPU_DEFAULT_DMP_FREQ;
	else
		base_freq = BASE_SAMPLE_RATE;

	t = base_freq / data;
	if (!t)
		t = 1;
	out1 = base_freq / (t + 1);
	out2 = base_freq / t;
	if ((data - out1) * INV_ODR_BUFFER_MULTI < data)
		out = out1;
	else
		out = out2;

	return out;
}

static void inv_check_wake_non_wake(struct inv_mpu_state *st,
			enum SENSOR_L wake, enum SENSOR_L non_wake)
{
	int tmp_rate;

	if (!st->sensor_l[wake].on && !st->sensor_l[non_wake].on)
		return;

	tmp_rate = MPU_INIT_SENSOR_RATE;
	if (st->sensor_l[wake].on)
		tmp_rate = st->sensor_l[wake].rate;
	if (st->sensor_l[non_wake].on)
		tmp_rate = max(tmp_rate, st->sensor_l[non_wake].rate);
	st->sensor_l[wake].rate = tmp_rate;
	st->sensor_l[non_wake].rate = tmp_rate;
}

static void inv_check_wake_non_wake_divider(struct inv_mpu_state *st,
			enum SENSOR_L wake, enum SENSOR_L non_wake)
{
	if (st->sensor_l[wake].on && st->sensor_l[non_wake].on)
		st->sensor_l[non_wake].div = 0xffff;

}

#if defined(CONFIG_INV_MPU_IIO_ICM20602) \
	|| defined(CONFIG_INV_MPU_IIO_ICM20690) \
	|| defined(CONFIG_INV_MPU_IIO_IAM20680)	
int inv_check_sensor_on(struct inv_mpu_state *st)
{
	int i, max_rate;
	enum SENSOR_L wake[] = {SENSOR_L_GYRO_WAKE, SENSOR_L_ACCEL_WAKE,
					SENSOR_L_MAG_WAKE};
	enum SENSOR_L non_wake[] = {SENSOR_L_GYRO, SENSOR_L_ACCEL,
					SENSOR_L_MAG};

	st->sensor_l[SENSOR_L_GESTURE_ACCEL].rate = GESTURE_ACCEL_RATE;
	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].on = false;
	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].rate = MPU_INIT_SENSOR_RATE;

	if ((st->step_detector_l_on
			|| st->step_detector_wake_l_on
			|| st->step_counter_l_on
			|| st->step_counter_wake_l_on
			|| st->chip_config.pick_up_enable
			|| st->chip_config.tilt_enable)
			&& (!st->sensor_l[SENSOR_L_ACCEL].on)
			&& (!st->sensor_l[SENSOR_L_ACCEL_WAKE].on))
		st->sensor_l[SENSOR_L_GESTURE_ACCEL].on = true;
	else
		st->sensor_l[SENSOR_L_GESTURE_ACCEL].on = false;


	st->chip_config.wake_on = false;
	for (i = 0; i < SENSOR_L_NUM_MAX; i++) {
		if (st->sensor_l[i].on && st->sensor_l[i].rate) {
			st->sensor[st->sensor_l[i].base].on = true;
			st->chip_config.wake_on |= st->sensor_l[i].wake_on;
		}
	}
	if (st->sensor_l[SENSOR_L_GESTURE_ACCEL].on &&
				(!st->sensor[SENSOR_GYRO].on) &&
				(!st->sensor[SENSOR_COMPASS].on))
		st->gesture_only_on = true;
	else
		st->gesture_only_on = false;

	for (i = 0; i < SENSOR_L_NUM_MAX; i++) {
		if (st->sensor_l[i].on) {
			st->sensor[st->sensor_l[i].base].rate =
			    max(st->sensor[st->sensor_l[i].base].rate,
							st->sensor_l[i].rate);
		}
	}
	max_rate = MPU_INIT_SENSOR_RATE;
	if (st->chip_config.eis_enable) {
		max_rate = ESI_GYRO_RATE;
		st->sensor_l[SENSOR_L_EIS_GYRO].rate = ESI_GYRO_RATE;
	}

	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			max_rate = max(max_rate, st->sensor[i].rate);
		}
	}
	for (i = 0; i < SENSOR_NUM_MAX; i++) {
		if (st->sensor[i].on) {
			st->sensor[i].rate = max_rate;
		}
	}
	for (i = 0; i < ARRAY_SIZE(wake); i++)
		inv_check_wake_non_wake(st, wake[i], non_wake[i]);

	for (i = 0; i < SENSOR_L_NUM_MAX; i++) {
		if (st->sensor_l[i].on) {
			if (st->sensor_l[i].rate)
				st->sensor_l[i].div =
				    st->sensor[st->sensor_l[i].base].rate
							/ st->sensor_l[i].rate;
			else
				st->sensor_l[i].div = 0xffff;
			pr_debug("sensor= %d, div= %d\n",
						i, st->sensor_l[i].div);
		}
	}
	for (i = 0; i < ARRAY_SIZE(wake); i++)
		inv_check_wake_non_wake_divider(st, wake[i], non_wake[i]);

	if (st->step_detector_wake_l_on ||
			st->step_counter_wake_l_on ||
			st->chip_config.pick_up_enable ||
			st->chip_config.tilt_enable)
		st->chip_config.wake_on = true;

	return 0;
}
#else
static void inv_do_check_sensor_on(struct inv_mpu_state *st,
				enum SENSOR_L *wake,
				enum SENSOR_L *non_wake, int sensor_size)
{
	int i;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].on = false;

	for (i = 0; i < SENSOR_NUM_MAX; i++)
		st->sensor[i].rate = MPU_INIT_SENSOR_RATE;

	st->chip_config.wake_on = false;
	for (i = 0; i < SENSOR_L_NUM_MAX; i++) {
		if (st->sensor_l[i].on && st->sensor_l[i].rate) {
			st->sensor[st->sensor_l[i].base].on = true;
			st->chip_config.wake_on |= st->sensor_l[i].wake_on;
		}
	}

	for (i = 0; i < SENSOR_L_NUM_MAX; i++) {
		if (st->sensor_l[i].on) {
			st->sensor[st->sensor_l[i].base].rate =
			    max(st->sensor[st->sensor_l[i].base].rate,
				st->sensor_l[i].rate);
		}
	}
	for (i = 0; i < sensor_size; i++)
		inv_check_wake_non_wake(st, wake[i], non_wake[i]);

	for (i = 0; i < SENSOR_L_NUM_MAX; i++) {
		if (st->sensor_l[i].on) {
			if (st->sensor_l[i].rate)
				st->sensor_l[i].div =
				    st->sensor[st->sensor_l[i].base].rate
				    / st->sensor_l[i].rate;
			else
				st->sensor_l[i].div = 0xffff;
		}
	}
	for (i = 0; i < sensor_size; i++)
		inv_check_wake_non_wake_divider(st, wake[i], non_wake[i]);

	if (st->step_detector_wake_l_on ||
			st->step_counter_wake_l_on ||
			st->chip_config.pick_up_enable ||
			st->chip_config.tilt_enable ||
			st->smd.on)
		st->chip_config.wake_on = true;

}
#endif

#if defined(CONFIG_INV_MPU_IIO_ICM20608D)
int inv_check_sensor_on(struct inv_mpu_state *st)
{
	enum SENSOR_L wake[] = {SENSOR_L_GYRO_WAKE, SENSOR_L_ACCEL_WAKE,
				SENSOR_L_SIXQ_WAKE, SENSOR_L_PEDQ_WAKE,
				SENSOR_L_GYRO_CAL_WAKE};
	enum SENSOR_L non_wake[] = {SENSOR_L_GYRO, SENSOR_L_ACCEL,
				SENSOR_L_SIXQ, SENSOR_L_PEDQ,
				SENSOR_L_GYRO_CAL};

	inv_do_check_sensor_on(st, wake, non_wake, ARRAY_SIZE(wake));

	return 0;
}
#endif

#if defined(CONFIG_INV_MPU_IIO_ICM20648)
int inv_check_sensor_on(struct inv_mpu_state *st)
{
	enum SENSOR_L wake[] = {SENSOR_L_GYRO_WAKE, SENSOR_L_ACCEL_WAKE,
				SENSOR_L_MAG_WAKE, SENSOR_L_ALS_WAKE,
				SENSOR_L_SIXQ_WAKE, SENSOR_L_PEDQ_WAKE,
				SENSOR_L_NINEQ_WAKE, SENSOR_L_GEOMAG_WAKE,
				SENSOR_L_PRESSURE_WAKE,
				SENSOR_L_GYRO_CAL_WAKE,
				SENSOR_L_MAG_CAL_WAKE};
	enum SENSOR_L non_wake[] = {SENSOR_L_GYRO, SENSOR_L_ACCEL,
					SENSOR_L_MAG, SENSOR_L_ALS,
					SENSOR_L_SIXQ, SENSOR_L_PEDQ,
					SENSOR_L_NINEQ, SENSOR_L_GEOMAG,
					SENSOR_L_PRESSURE,
					SENSOR_L_GYRO_CAL,
					SENSOR_L_MAG_CAL};

	inv_do_check_sensor_on(st, wake, non_wake, ARRAY_SIZE(wake));

	return 0;
}
#endif

#ifdef CONFIG_PM_SLEEP
int inv_mpu_suspend(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);

	/* add code according to different request Start */
	dev_info(st->dev, "%s suspend\n", st->hw->name);
	mutex_lock(&indio_dev->mlock);

	st->resume_state = false;
	if (st->chip_config.wake_on) {
		enable_irq_wake(st->irq);
	} else {
		inv_stop_interrupt(st);
	}

	mutex_unlock(&indio_dev->mlock);

	return 0;
}
EXPORT_SYMBOL_GPL(inv_mpu_suspend);

/*
 * inv_mpu_complete(): complete method for this driver.
 *    This method can be modified according to the request of different
 *    customers. It basically undo everything suspend is doing
 *    and recover the chip to what it was before suspend. We use complete to
 *    make sure that alarm clock resume is finished. If we use resume, the
 *    alarm clock may not resume yet and get incorrect clock reading.
 */
void inv_mpu_complete(struct iio_dev *indio_dev)
{
	struct inv_mpu_state *st = iio_priv(indio_dev);

	dev_info(st->dev, "%s resume\n", st->hw->name);
	if (st->resume_state)
		return;

	mutex_lock(&indio_dev->mlock);

	if (!st->chip_config.wake_on) {
		inv_reenable_interrupt(st);
	} else {
		disable_irq_wake(st->irq);
	}
	/*	resume state is used to synchronize read_fifo such that it won't
		proceed unless resume is finished. */
	st->resume_state = true;
	/*	resume flag is indicating that current clock reading is from resume,
		it has up to 1 second drift and should do proper processing */
	st->ts_algo.resume_flag  = true;
	mutex_unlock(&indio_dev->mlock);
	wake_up_interruptible(&st->wait_queue);

	return;
}
EXPORT_SYMBOL_GPL(inv_mpu_complete);
#endif
