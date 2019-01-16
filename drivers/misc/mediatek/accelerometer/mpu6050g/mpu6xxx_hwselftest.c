#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <asm/uaccess.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/workqueue.h>
#include <linux/kobject.h>
#include <linux/earlysuspend.h>
#include <linux/platform_device.h>
#include <asm/atomic.h>

#include <cust_acc.h>
#include <linux/hwmsensor.h>
#include <linux/hwmsen_dev.h>
#include <linux/sensors_io.h>
#include "mpu6050.h"
#include "mpu6xxx_hwselftest.h"
#include <linux/hwmsen_helper.h>

#include <mach/mt_typedefs.h>
#include <mach/mt_gpio.h>
#include <mach/mt_pm_ldo.h>

extern int MPU6050_i2c_master_send(u8 *buf, u8 len);
extern int MPU6050_hwmsen_read_block(u8 addr, u8 *buf, u8 len);

//#define pr_debug printk

#if MPU6XXX_HWSELFTEST==1

#define inv_i2c_read(st, reg, len, data) \
	MPU6050_hwmsen_read_block(reg, data, len)
#define inv_i2c_single_write(st, reg, data) \
	inv_i2c_single_write_base(reg, data)

int inv_i2c_single_write_base(u8 reg, u8 data)
{
    u8 databuf[2] = {0};
    int res = 0;

    databuf[1] = data;
    databuf[0] = reg;

//#ifdef MPU6050_ACCESS_BY_GSE_I2C
    res = MPU6050_i2c_master_send(databuf, 0x2);
//#else
//    res = i2c_master_send(client, databuf, 0x2);
//#endif

    if (res <= 0)
    {
        return MPU6050_ERR_I2C;
    }
    else
    {
        return MPU6050_SUCCESS;
    }

}

/**
* inv_check_6500_gyro_self_test() - check 6500 gyro self test. this function
*                                   returns zero as success. A non-zero return
*                                   value indicates failure in self test.
*  @*st: main data structure.
*  @*reg_avg: average value of normal test.
*  @*st_avg:  average value of self test
*/
static int inv_check_6500_gyro_self_test(struct inv_selftest_device *st,
						int *reg_avg, int *st_avg) {
	u8 regs[3];
	int ret_val, result;
	int otp_value_zero = 0;
	int st_shift_prod[3], st_shift_cust[3], i;

	ret_val = 0;
	result = inv_i2c_read(st, REG_6500_XG_ST_DATA, 3, regs);
	if (result)
		return result;
	pr_debug("%s self_test gyro shift_code - %02x %02x %02x\n",
		 st->name, regs[0], regs[1], regs[2]);

	for (i = 0; i < 3; i++) {
		if (regs[i] != 0) {
			st_shift_prod[i] = mpu_6500_st_tb[regs[i] - 1];
		} else {
			st_shift_prod[i] = 0;
			otp_value_zero = 1;
		}
	}
	pr_debug("%s self_test gyro st_shift_prod - %+d %+d %+d\n",
		 st->name, st_shift_prod[0], st_shift_prod[1],
		 st_shift_prod[2]);

	for (i = 0; i < 3; i++) {
		st_shift_cust[i] = st_avg[i] - reg_avg[i];
		if (!otp_value_zero) {
			/* Self Test Pass/Fail Criteria A */
			if (st_shift_cust[i] < DEF_6500_GYRO_CT_SHIFT_DELTA
						* st_shift_prod[i])
			{
					ret_val = 1;
					pr_debug("%s, Fail, A\n", __func__);
			}
		} else {
			/* Self Test Pass/Fail Criteria B */
			if (st_shift_cust[i] < DEF_GYRO_ST_AL *
						DEF_SELFTEST_GYRO_SENS *
						DEF_ST_PRECISION)
			{
				ret_val = 1;
				pr_debug("%s, Fail, B\n", __func__);
			}
		}
	}
	pr_debug("%s self_test gyro st_shift_cust - %+d %+d %+d\n",
		 st->name, st_shift_cust[0], st_shift_cust[1],
		 st_shift_cust[2]);

	if (ret_val == 0) {
		/* Self Test Pass/Fail Criteria C */
		for (i = 0; i < 3; i++)
			if (abs(reg_avg[i]) > DEF_GYRO_OFFSET_MAX *
						DEF_SELFTEST_GYRO_SENS *
						DEF_ST_PRECISION)
			{
				ret_val = 1;
				pr_debug("%s, Fail, C\n", __func__);
			}
	}

	return ret_val;
}

/**
* inv_check_6500_accel_self_test() - check 6500 accel self test. this function
*                                   returns zero as success. A non-zero return
*                                   value indicates failure in self test.
*  @*st: main data structure.
*  @*reg_avg: average value of normal test.
*  @*st_avg:  average value of self test
*/
static int inv_check_6500_accel_self_test(struct inv_selftest_device *st,
						int *reg_avg, int *st_avg) {
	int ret_val, result;
	int st_shift_prod[3], st_shift_cust[3], st_shift_ratio[3], i;
	u8 regs[3];
	int otp_value_zero = 0;

#define ACCEL_ST_AL_MIN ((DEF_ACCEL_ST_AL_MIN * DEF_ST_SCALE \
				 / DEF_ST_6500_ACCEL_FS_MG) * DEF_ST_PRECISION)
#define ACCEL_ST_AL_MAX ((DEF_ACCEL_ST_AL_MAX * DEF_ST_SCALE \
				 / DEF_ST_6500_ACCEL_FS_MG) * DEF_ST_PRECISION)

	ret_val = 0;
	result = inv_i2c_read(st, REG_6500_XA_ST_DATA, 3, regs);
	if (result)
		return result;
	pr_debug("%s self_test accel shift_code - %02x %02x %02x\n",
		 st->name, regs[0], regs[1], regs[2]);

	for (i = 0; i < 3; i++) {
		if (regs[i] != 0) {
			st_shift_prod[i] = mpu_6500_st_tb[regs[i] - 1];
		} else {
			st_shift_prod[i] = 0;
			otp_value_zero = 1;
		}
	}
	pr_debug("%s self_test accel st_shift_prod - %+d %+d %+d\n",
		 st->name, st_shift_prod[0], st_shift_prod[1],
		 st_shift_prod[2]);

	if (!otp_value_zero) {
		/* Self Test Pass/Fail Criteria A */
		for (i = 0; i < 3; i++) {
			st_shift_cust[i] = st_avg[i] - reg_avg[i];
			st_shift_ratio[i] = abs(st_shift_cust[i] /
					st_shift_prod[i] - DEF_ST_PRECISION);
			if (st_shift_ratio[i] > DEF_6500_ACCEL_ST_SHIFT_DELTA)
			{
				ret_val = 1;
				pr_debug("%s, Fail, A\n", __func__);
			}
		}
	} else {
		/* Self Test Pass/Fail Criteria B */
		for (i = 0; i < 3; i++) {
			st_shift_cust[i] = abs(st_avg[i] - reg_avg[i]);
			if (st_shift_cust[i] < ACCEL_ST_AL_MIN ||
					st_shift_cust[i] > ACCEL_ST_AL_MAX)
			{
				ret_val = 1;
				pr_debug("%s, Fail, B\n", __func__);
			}
		}
	}
	pr_debug("%s self_test accel st_shift_cust - %+d %+d %+d\n",
		 st->name, st_shift_cust[0], st_shift_cust[1],
		 st_shift_cust[2]);

	return ret_val;
}

/*
 *  inv_do_test() - do the actual test of self testing
 */
static int inv_do_test(struct inv_selftest_device *st, int self_test_flag,
		int *gyro_result, int *accel_result)
{
	int result, i, j, packet_size;
	u8 data[BYTES_PER_SENSOR * 2], d;
	int fifo_count, packet_count, ind, s;

	packet_size = BYTES_PER_SENSOR * 2;

	result = inv_i2c_single_write(st, REG_INT_ENABLE, 0);
	if (result)
		return result;
	/* disable the sensor output to FIFO */
	result = inv_i2c_single_write(st, REG_FIFO_EN, 0);
	if (result)
		return result;
	/* disable fifo reading */
	result = inv_i2c_single_write(st, REG_USER_CTRL, 0);
	if (result)
		return result;
	/* clear FIFO */
	result = inv_i2c_single_write(st, REG_USER_CTRL, BIT_FIFO_RST);
	if (result)
		return result;
	/* setup parameters */
	result = inv_i2c_single_write(st, REG_CONFIG, INV_FILTER_98HZ);
	if (result)
		return result;

	if (INV_MPU6500 == st->chip_type) {
		/* config accel LPF register for MPU6500 */
		result = inv_i2c_single_write(st, REG_6500_ACCEL_CONFIG2,
						DEF_ST_MPU6500_ACCEL_LPF |
						BIT_FIFO_SIZE_1K);
		if (result)
			return result;
	}

	result = inv_i2c_single_write(st, REG_SAMPLE_RATE_DIV,
			DEF_SELFTEST_SAMPLE_RATE);
	if (result)
		return result;
	/* wait for the sampling rate change to stabilize */
	mdelay(INV_MPU_SAMPLE_RATE_CHANGE_STABLE);
	result = inv_i2c_single_write(st, REG_GYRO_CONFIG,
		self_test_flag | DEF_SELFTEST_GYRO_FS);
	if (result)
		return result;
	if (INV_MPU6500 == st->chip_type)
		d = DEF_SELFTEST_6500_ACCEL_FS;
	else
		d = DEF_SELFTEST_ACCEL_FS;
	d |= self_test_flag;
	result = inv_i2c_single_write(st, REG_ACCEL_CONFIG, d);
	if (result)
		return result;
	/* wait for the output to get stable */
	if (self_test_flag) {
		if (INV_MPU6500 == st->chip_type)
			msleep(DEF_ST_6500_STABLE_TIME);
		else
			msleep(DEF_ST_STABLE_TIME);
	}

	/* enable FIFO reading */
	result = inv_i2c_single_write(st, REG_USER_CTRL, BIT_FIFO_EN);
	if (result)
		return result;
	/* enable sensor output to FIFO */
	d = BITS_GYRO_OUT | BIT_ACCEL_OUT;
	for (i = 0; i < THREE_AXIS; i++) {
		gyro_result[i] = 0;
		accel_result[i] = 0;
	}
	s = 0;
	while (s < st->samples) {
		result = inv_i2c_single_write(st, REG_FIFO_EN, d);
		if (result)
			return result;
		mdelay(DEF_GYRO_WAIT_TIME);
		result = inv_i2c_single_write(st, REG_FIFO_EN, 0);
		if (result)
			return result;

		result = inv_i2c_read(st, REG_FIFO_COUNT_H,
					FIFO_COUNT_BYTE, data);
		if (result)
			return result;
		fifo_count = be16_to_cpup((__be16 *)(&data[0]));
		pr_debug("%s self_test fifo_count - %d\n",
			 st->name, fifo_count);
		packet_count = fifo_count / packet_size;
		i = 0;
		while ((i < packet_count) && (s < st->samples)) {
			short vals[3];
			result = inv_i2c_read(st, REG_FIFO_R_W,
				packet_size/2, data);
			if (result)
				return result;
			ind = 0;
			for (j = 0; j < THREE_AXIS; j++) {
				vals[j] = (short)be16_to_cpup(
				    (__be16 *)(&data[ind + 2 * j]));
				accel_result[j] += vals[j];
			}
			ind += BYTES_PER_SENSOR;
//			pr_debug(
//			    "%s self_test accel data - %d %+d %+d %+d",
//			    st->name, s, vals[0], vals[1], vals[2]);

			result = inv_i2c_read(st, REG_FIFO_R_W,
				packet_size/2, data+packet_size/2);
			if (result)
				return result;
			for (j = 0; j < THREE_AXIS; j++) {
				vals[j] = (short)be16_to_cpup(
					(__be16 *)(&data[ind + 2 * j]));
				gyro_result[j] += vals[j];
			}
//			pr_debug("%s self_test gyro data - %d %+d %+d %+d",
//				 st->name, s, vals[0], vals[1], vals[2]);

			s++;
			i++;
		}
	}

	for (j = 0; j < THREE_AXIS; j++) {
		accel_result[j] = accel_result[j] / s;
		accel_result[j] *= DEF_ST_PRECISION;
	}
	for (j = 0; j < THREE_AXIS; j++) {
		gyro_result[j] = gyro_result[j] / s;
		gyro_result[j] *= DEF_ST_PRECISION;
	}

	return 0;
}

/*
 *  inv_store_setting() store the old settings before selftest
 */
static void inv_store_setting(struct inv_selftest_device *st)
{
	int result;
	u8 data;

	result = inv_i2c_read(st, REG_SAMPLE_RATE_DIV, 1, &data);
	if (result)
		pr_debug("%s read REG_SAMPLE_RATE_DIV fail\n",st->name);
	else st->sample_rate_div = data;

	result = inv_i2c_read(st, REG_CONFIG, 1, &data);
	if (result)
		pr_debug("%s read REG_CONFIG fail\n",st->name);
	else st->config = data;

	result = inv_i2c_read(st, REG_GYRO_CONFIG, 1, &data);
	if (result)
		pr_debug("%s read REG_GYRO_CONFIG fail\n",st->name);
	else st->gyro_config = data;

	result = inv_i2c_read(st, REG_ACCEL_CONFIG, 1, &data);
	if (result)
		pr_debug("%s read REG_ACCEL_CONFIG fail\n",st->name);
	else st->accel_config = data;
}

/*
 *  inv_recover_setting() recover the old settings after everything is done
 */
static void inv_recover_setting(struct inv_selftest_device *st)
{
	inv_i2c_single_write(st, REG_SAMPLE_RATE_DIV, st->sample_rate_div);
	inv_i2c_single_write(st, REG_CONFIG, st->config);
	inv_i2c_single_write(st, REG_GYRO_CONFIG, st->gyro_config);
	inv_i2c_single_write(st, REG_ACCEL_CONFIG, st->accel_config);
}

/*
 *  inv_hw_self_test() - main function to do hardware self test
 */
int inv_hw_self_test(struct inv_selftest_device *st)
{
	int result;
	int gyro_bias_st[THREE_AXIS], gyro_bias_regular[THREE_AXIS];
	int accel_bias_st[THREE_AXIS], accel_bias_regular[THREE_AXIS];
	int test_times, i;
	char compass_result, accel_result, gyro_result;

	inv_store_setting(st);
//	result = inv_power_up_self_test(st);
//	if (result)
//		return result;
	compass_result = 0;
	accel_result = 0;
	gyro_result = 0;
	test_times = DEF_ST_TRY_TIMES;
	while (test_times > 0) {
		result = inv_do_test(st, 0, gyro_bias_regular,
			accel_bias_regular);
		if (result == -EAGAIN)
			test_times--;
		else
			test_times = 0;
	}
	if (result)
		goto test_fail;
	pr_debug("%s self_test accel bias_regular - %+d %+d %+d\n",
		 st->name, accel_bias_regular[0],
		 accel_bias_regular[1], accel_bias_regular[2]);
	pr_debug("%s self_test gyro bias_regular - %+d %+d %+d\n",
		 st->name, gyro_bias_regular[0], gyro_bias_regular[1],
		 gyro_bias_regular[2]);

	for (i = 0; i < 3; i++) {
		st->gyro_bias[i] = gyro_bias_regular[i];
		st->accel_bias[i] = accel_bias_regular[i];
	}

	test_times = DEF_ST_TRY_TIMES;
	while (test_times > 0) {
		result = inv_do_test(st, BITS_SELF_TEST_EN, gyro_bias_st,
					accel_bias_st);
		if (result == -EAGAIN)
			test_times--;
		else
			break;
	}
	if (result)
		goto test_fail;
	pr_debug("%s self_test accel bias_st - %+d %+d %+d\n",
		 st->name, accel_bias_st[0], accel_bias_st[1],
		 accel_bias_st[2]);
	pr_debug("%s self_test gyro bias_st - %+d %+d %+d\n",
		 st->name, gyro_bias_st[0], gyro_bias_st[1],
		 gyro_bias_st[2]);

	for (i = 0; i < 3; i++) {
		st->gyro_bias_st[i] = gyro_bias_st[i];
		st->accel_bias_st[i] = accel_bias_st[i];
	}
    //TBD compass selftest
//	if (st->chip_config.has_compass)
//		compass_result = !st->slave_compass->self_test(st);

	 if (INV_MPU6050 == st->chip_type) {
//		accel_result = !inv_check_accel_self_test(st,
//			accel_bias_regular, accel_bias_st);
//		gyro_result = !inv_check_6050_gyro_self_test(st,
//			gyro_bias_regular, gyro_bias_st);
	} else if (INV_MPU6500 == st->chip_type) {
		accel_result = !inv_check_6500_accel_self_test(st,
			accel_bias_regular, accel_bias_st);
		gyro_result = !inv_check_6500_gyro_self_test(st,
			gyro_bias_regular, gyro_bias_st);
	}

test_fail:
	inv_recover_setting(st);

	result = (compass_result << DEF_ST_COMPASS_RESULT_SHIFT) |
		(accel_result << DEF_ST_ACCEL_RESULT_SHIFT) | gyro_result;
	pr_debug("%s, result=0x%x\n", __func__, result);
	//1:PASS 0:FAIL
	return result;
}

#endif//#if MPU6XXX_HWSELFTEST==1

