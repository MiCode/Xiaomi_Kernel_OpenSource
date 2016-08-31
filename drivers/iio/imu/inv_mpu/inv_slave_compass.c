/*
* Copyright (C) 2012 Invensense, Inc.
* Copyright (C) 2016 XiaoMi, Inc.
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>

#include "inv_mpu_iio.h"
/* AKM definitions */
#define REG_AKM_ID               0x00
#define REG_AKM_INFO             0x01
#define REG_AKM_STATUS           0x02
#define REG_AKM_MEASURE_DATA     0x03
#define REG_AKM_MODE             0x0A
#define REG_AKM_ST_CTRL          0x0C
#define REG_AKM_SENSITIVITY      0x10
#define REG_AKM8963_CNTL1        0x0A

/* AK09911 register definition */
#define REG_AK09911_DMP_READ    0x10
#define REG_AK09911_STATUS1     0x10
#define REG_AK09911_CNTL2       0x31
#define REG_AK09911_SENSITIVITY 0x60

#define DATA_AKM_ID              0x48
#define DATA_AKM_MODE_PD	 0x00
#define DATA_AKM_MODE_SM	 0x01
#define DATA_AKM_MODE_ST	 0x08
#define DATA_AKM_MODE_FR	 0x0F
#define DATA_AK09911_MODE_FR     0x1F
#define DATA_AKM_SELF_TEST       0x40
#define DATA_AKM_DRDY            0x01
#define DATA_AKM8963_BIT         0x10
#define DATA_AKM_STAT_MASK       0x0C

#define DATA_AKM8975_SCALE       (9830 * (1L << 15))
#define DATA_AKM8972_SCALE       (19661 * (1L << 15))
#define DATA_AKM8963_SCALE0      (19661 * (1L << 15))
#define DATA_AKM8963_SCALE1      (4915 * (1L << 15))
#define DATA_AK09911_SCALE       (19661 * (1L << 15))
#define DATA_MLX_SCALE           (4915 * (1L << 15))
#define DATA_MLX_SCALE_EMPIRICAL (26214 * (1L << 15))

#define DATA_AKM8963_SCALE_SHIFT      4
#define DATA_AKM_BYTES_DMP  10
#define DATA_AKM_BYTES      8
#define DATA_AKM_MIN_READ_TIME            (9 * NSEC_PER_MSEC)

#define DEF_ST_COMPASS_WAIT_MIN     (10 * 1000)
#define DEF_ST_COMPASS_WAIT_MAX     (15 * 1000)
#define DEF_ST_COMPASS_TRY_TIMES    10
#define DEF_ST_COMPASS_8963_SHIFT   2
#define X                           0
#define Y                           1
#define Z                           2

/* milliseconds between each access */
#define AKM_RATE_SCALE       10
#define MLX_RATE_SCALE       50

/* MLX90399 compass definition */
#define DATA_MLX_CMD_READ_MEASURE         0x4F
#define DATA_MLX_CMD_SINGLE_MEASURE       0x3F
#define DATA_MLX_READ_DATA_BYTES          9
#define DATA_MLX_STATUS_DATA              3
#define DATA_MLX_MIN_READ_TIME            (95 * NSEC_PER_MSEC)

static const short AKM8975_ST_Lower[3] = {-100, -100, -1000};
static const short AKM8975_ST_Upper[3] = {100, 100, -300};

static const short AKM8972_ST_Lower[3] = {-50, -50, -500};
static const short AKM8972_ST_Upper[3] = {50, 50, -100};

static const short AKM8963_ST_Lower[3] = {-200, -200, -3200};
static const short AKM8963_ST_Upper[3] = {200, 200, -800};

/*
 *  inv_setup_compass_akm() - Configure akm series compass.
 */
static int inv_setup_compass_akm(struct inv_mpu_state *st)
{
	int result;
	u8 data[4];
	u8 sens, mode, cmd;

	/* set to bypass mode */
	result = inv_i2c_single_write(st, REG_INT_PIN_CFG,
				st->plat_data.int_config | BIT_BYPASS_EN);
	if (result)
		return result;
	/* read secondary i2c ID register */
	result = inv_secondary_read(REG_AKM_ID, 1, data);
	if (result)
		return result;
	if (data[0] != DATA_AKM_ID)
		return -ENXIO;
	/* set AKM to Fuse ROM access mode */
	if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id) {
		mode = REG_AK09911_CNTL2;
		sens = REG_AK09911_SENSITIVITY;
		cmd = DATA_AK09911_MODE_FR;
	} else {
		mode = REG_AKM_MODE;
		sens = REG_AKM_SENSITIVITY;
		cmd = DATA_AKM_MODE_FR;
	}

	result = inv_secondary_write(mode, cmd);
	if (result)
		return result;
	result = inv_secondary_read(sens, THREE_AXIS,
						st->chip_info.compass_sens);
	if (result)
		return result;
	/* revert to power down mode */
	result = inv_secondary_write(mode, DATA_AKM_MODE_PD);
	if (result)
		return result;
	pr_debug("%s senx=%d, seny=%d, senz=%d\n",
		 st->hw->name,
		 st->chip_info.compass_sens[0],
		 st->chip_info.compass_sens[1],
		 st->chip_info.compass_sens[2]);
	/* restore to non-bypass mode */
	result = inv_i2c_single_write(st, REG_INT_PIN_CFG,
			st->plat_data.int_config);
	if (result)
		return result;

	/* setup master mode and master clock and ES bit */
	result = inv_i2c_single_write(st, REG_I2C_MST_CTRL, BIT_WAIT_FOR_ES);
	if (result)
		return result;
	/* slave 1 is used for AKM mode change only */
	result = inv_i2c_single_write(st, REG_I2C_SLV1_ADDR,
		st->plat_data.secondary_i2c_addr);
	if (result)
		return result;
	/* AKM mode register address */
	result = inv_i2c_single_write(st, REG_I2C_SLV1_REG, mode);
	if (result)
		return result;
	/* output data for slave 1 is fixed, single measure mode */
	st->slave_compass->scale = 1;
	if (COMPASS_ID_AK8975 == st->plat_data.sec_slave_id) {
		st->slave_compass->st_upper = AKM8975_ST_Upper;
		st->slave_compass->st_lower = AKM8975_ST_Lower;
		data[0] = DATA_AKM_MODE_SM;
	} else if (COMPASS_ID_AK8972 == st->plat_data.sec_slave_id) {
		st->slave_compass->st_upper = AKM8972_ST_Upper;
		st->slave_compass->st_lower = AKM8972_ST_Lower;
		data[0] = DATA_AKM_MODE_SM;
	} else if (COMPASS_ID_AK8963 == st->plat_data.sec_slave_id) {
		st->slave_compass->st_upper = AKM8963_ST_Upper;
		st->slave_compass->st_lower = AKM8963_ST_Lower;
		data[0] = DATA_AKM_MODE_SM |
			(st->slave_compass->scale << DATA_AKM8963_SCALE_SHIFT);
	}  else if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id) {
		st->slave_compass->st_upper = AKM8963_ST_Upper;
		st->slave_compass->st_lower = AKM8963_ST_Lower;
		data[0] = DATA_AKM_MODE_SM;
	} else {
		return -EINVAL;
	}

	result = inv_i2c_single_write(st, INV_MPU_REG_I2C_SLV1_DO, data[0]);

	return result;
}

static int inv_akm_read_data(struct inv_mpu_state *st, short *o)
{
	int result, shift;
	int i;
	u8 d[DATA_AKM_BYTES];
	u8 *sens;

	sens = st->chip_info.compass_sens;
	result = 0;
	if (st->chip_config.dmp_on &&
			(COMPASS_ID_AK09911 != st->plat_data.sec_slave_id)) {
		for (i = 0; i < 6; i++)
			d[1 + i] = st->fifo_data[i];
	} else {
		result = inv_i2c_read(st, REG_EXT_SENS_DATA_00,
						DATA_AKM_BYTES, d);
		if ((DATA_AKM_DRDY != d[0]) || result)
			result = -EINVAL;
	}
	if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id)
		shift = 7;
	else
		shift = 8;
	for (i = 0; i < 3; i++) {
		o[i] = (short)((d[i * 2 + 2] << 8) | d[i * 2 + 1]);
		o[i] = (short)(((int)o[i] * (sens[i] + 128)) >> shift);
	}

	return result;
}

static int inv_mlx_read_data(struct inv_mpu_state *st, short *o)
{
	int result;
	int i, z;
	u8 d[DATA_MLX_READ_DATA_BYTES];

	result = inv_i2c_read(st, REG_EXT_SENS_DATA_00,
			     DATA_MLX_READ_DATA_BYTES, d);
	if ((!(d[0] & ~DATA_MLX_STATUS_DATA)) && (!result)) {
		for (i = 0; i < 3; i++)
			o[i] = (short)((d[i * 2 + 3] << 8) + d[i * 2 + 4]);
	} else {
		for (i = 0; i < 3; i++)
			o[i] = 0;
	}
	z = o[2];
	/* axis sensitivity conversion. Z axis has different sensitiviy from
	   x and y */
	z *= 26;
	z /= 15;
	o[2] = z;

	return 0;
}

static int inv_check_akm_self_test(struct inv_mpu_state *st)
{
	int result;
	u8 data[6], mode;
	u8 counter, cntl;
	short x, y, z;
	u8 *sens;
	sens = st->chip_info.compass_sens;

	/* set to bypass mode */
	result = inv_i2c_single_write(st, REG_INT_PIN_CFG,
				st->plat_data.int_config | BIT_BYPASS_EN);
	if (result) {
		result = inv_i2c_single_write(st, REG_INT_PIN_CFG,
				st->plat_data.int_config);
		return result;
	}
	if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id)
		mode = REG_AK09911_CNTL2;
	else
		mode = REG_AKM_MODE;
	/* set to power down mode */
	result = inv_secondary_write(mode, DATA_AKM_MODE_PD);
	if (result)
		goto AKM_fail;

	/* write 1 to ASTC register */
	result = inv_secondary_write(REG_AKM_ST_CTRL, DATA_AKM_SELF_TEST);
	if (result)
		goto AKM_fail;
	/* set self test mode */
	result = inv_secondary_write(mode, DATA_AKM_MODE_ST);
	if (result)
		goto AKM_fail;
	counter = DEF_ST_COMPASS_TRY_TIMES;
	while (counter > 0) {
		usleep_range(DEF_ST_COMPASS_WAIT_MIN, DEF_ST_COMPASS_WAIT_MAX);
		result = inv_secondary_read(REG_AKM_STATUS, 1, data);
		if (result)
			goto AKM_fail;
		if ((data[0] & DATA_AKM_DRDY) == 0)
			counter--;
		else
			counter = 0;
	}
	if ((data[0] & DATA_AKM_DRDY) == 0) {
		result = -EINVAL;
		goto AKM_fail;
	}
	result = inv_secondary_read(REG_AKM_MEASURE_DATA,
					BYTES_PER_SENSOR, data);
	if (result)
		goto AKM_fail;

	x = le16_to_cpup((__le16 *)(&data[0]));
	y = le16_to_cpup((__le16 *)(&data[2]));
	z = le16_to_cpup((__le16 *)(&data[4]));
	x = ((x * (sens[0] + 128)) >> 8);
	y = ((y * (sens[1] + 128)) >> 8);
	z = ((z * (sens[2] + 128)) >> 8);
	if (COMPASS_ID_AK8963 == st->plat_data.sec_slave_id) {
		result = inv_secondary_read(REG_AKM8963_CNTL1, 1, &cntl);
		if (result)
			goto AKM_fail;
		if (0 == (cntl & DATA_AKM8963_BIT)) {
			x <<= DEF_ST_COMPASS_8963_SHIFT;
			y <<= DEF_ST_COMPASS_8963_SHIFT;
			z <<= DEF_ST_COMPASS_8963_SHIFT;
		}
	}
	result = -EINVAL;
	if (x > st->slave_compass->st_upper[X] ||
					x < st->slave_compass->st_lower[X])
		goto AKM_fail;
	if (y > st->slave_compass->st_upper[Y] ||
					y < st->slave_compass->st_lower[Y])
		goto AKM_fail;
	if (z > st->slave_compass->st_upper[Z] ||
					z < st->slave_compass->st_lower[Z])
		goto AKM_fail;
	result = 0;
AKM_fail:
	/*write 0 to ASTC register */
	result |= inv_secondary_write(REG_AKM_ST_CTRL, 0);
	/*set to power down mode */
	result |= inv_secondary_write(mode, DATA_AKM_MODE_PD);
	/*restore to non-bypass mode */
	result |= inv_i2c_single_write(st, REG_INT_PIN_CFG,
			st->plat_data.int_config);
	return result;
}

/*
 *  inv_write_akm_scale() - Configure the akm scale range.
 */
static int inv_write_akm_scale(struct inv_mpu_state *st, int data)
{
	char d, en;
	int result;

	if (COMPASS_ID_AK8963 != st->plat_data.sec_slave_id)
		return 0;
	en = !!data;
	if (st->slave_compass->scale == en)
		return 0;
	d = (DATA_AKM_MODE_SM | (en << DATA_AKM8963_SCALE_SHIFT));
	result = inv_i2c_single_write(st, INV_MPU_REG_I2C_SLV1_DO, d);
	if (result)
		return result;
	st->slave_compass->scale = en;

	return 0;
}

/*
 *  inv_read_akm_scale() - show AKM scale.
 */
static int inv_read_akm_scale(struct inv_mpu_state *st, int *scale)
{
	if (COMPASS_ID_AK8975 == st->plat_data.sec_slave_id)
		*scale = DATA_AKM8975_SCALE;
	else if (COMPASS_ID_AK8972 == st->plat_data.sec_slave_id)
		*scale = DATA_AKM8972_SCALE;
	else if (COMPASS_ID_AK8963 == st->plat_data.sec_slave_id)
		if (st->slave_compass->scale)
			*scale = DATA_AKM8963_SCALE1;
		else
			*scale = DATA_AKM8963_SCALE0;
	else if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id)
		*scale = DATA_AK09911_SCALE;
	else
		return -EINVAL;

	return IIO_VAL_INT;
}

static int inv_suspend_akm(struct inv_mpu_state *st)
{
	int result;

	/* slave 0 is disabled */
	result = inv_i2c_single_write(st, REG_I2C_SLV0_CTRL, 0);
	if (result)
		return result;
	/* slave 1 is disabled */
	result = inv_i2c_single_write(st, REG_I2C_SLV1_CTRL, 0);

	return result;
}

static int inv_resume_akm(struct inv_mpu_state *st)
{
	int result;
	u8 reg_addr, bytes;

	/* slave 0 is used to read data from compass */
	/*read mode */
	result = inv_i2c_single_write(st, REG_I2C_SLV0_ADDR,
					INV_MPU_BIT_I2C_READ |
					st->plat_data.secondary_i2c_addr);
	if (result)
		return result;
	/* AKM status register address is 1 */
	if (COMPASS_ID_AK09911 == st->plat_data.sec_slave_id) {
		if (st->chip_config.dmp_on) {
			reg_addr = REG_AK09911_DMP_READ;
			bytes = DATA_AKM_BYTES_DMP;
		} else {
			reg_addr = REG_AK09911_STATUS1;
			bytes = DATA_AKM_BYTES;
		}
	} else {
		if (st->chip_config.dmp_on) {
			reg_addr = REG_AKM_INFO;
			bytes = DATA_AKM_BYTES_DMP;
		} else {
			reg_addr = REG_AKM_STATUS;
			bytes = DATA_AKM_BYTES;
		}
	}
	result = inv_i2c_single_write(st, REG_I2C_SLV0_REG, reg_addr);
	if (result)
		return result;

	/* slave 0 is enabled, read 10 or 8 bytes from here */
	result = inv_i2c_single_write(st, REG_I2C_SLV0_CTRL,
						INV_MPU_BIT_SLV_EN | bytes);
	if (result)
		return result;
	/* slave 1 is enabled, write byte length is 1 */
	result = inv_i2c_single_write(st, REG_I2C_SLV1_CTRL,
						INV_MPU_BIT_SLV_EN | 1);

	return result;
}

/*
 *  inv_write_mlx_scale() - Configure the mlx90399 scale range.
 */
static int inv_write_mlx_scale(struct inv_mpu_state *st, int data)
{
	st->slave_compass->scale = data;
	return 0;
}

/*
 *  inv_read_mlx_scale() - show mlx90399 scale.
 */
static int inv_read_mlx_scale(struct inv_mpu_state *st, int *scale)
{
	*scale = st->slave_compass->scale;
	return IIO_VAL_INT;
}

static int inv_i2c_read_mlx(struct inv_mpu_state *st, u16 i2c_addr,
			    u16 length, u8 *data)
{
	struct i2c_msg msgs[1];
	int res;

	if (!data)
		return -EINVAL;

	msgs[0].addr = i2c_addr;
	msgs[0].flags = I2C_M_RD;
	msgs[0].buf = data;
	msgs[0].len = length;

	res = i2c_transfer(st->sl_handle, msgs, 1);

	if (res < 1) {
		if (res >= 0)
			res = -EIO;
	} else
		res = 0;

	return res;
}

static int inv_i2c_write_mlx(struct inv_mpu_state *st,
	u16 i2c_addr, u8 data)
{
	u8 tmp[1];
	struct i2c_msg msg;
	int res;

	tmp[0] = data;
	msg.addr = i2c_addr;
	msg.flags = 0;	/* write */
	msg.buf = tmp;
	msg.len = 1;

	res = i2c_transfer(st->sl_handle, &msg, 1);
	if (res < 1) {
		if (res == 0)
			res = -EIO;
		return res;
	} else
		return 0;
}

static int inv_i2c_read_reg_mlx(struct inv_mpu_state *st,
	u16 i2c_addr, u8 reg, u16 *val)
{
	u8 tmp[10];
	struct i2c_msg msg;
	int res;

	tmp[0] = 0x50;
	tmp[1] = (reg << 2);
	msg.addr = i2c_addr;
	msg.flags = 0;	/* write */
	msg.buf = tmp;
	msg.len = 2;

	res = i2c_transfer(st->sl_handle, &msg, 1);
	if (res < 1) {
		if (res == 0)
			res = -EIO;
		return res;
	}
	res = inv_i2c_read_mlx(st, i2c_addr, 10, tmp);
	if (res)
		return res;
	*val = ((tmp[1] << 8) | tmp[2]);

	return res;
}

static int inv_i2c_write_mlx_reg(struct inv_mpu_state *st,
	u16 i2c_addr, int reg, u16 d)
{
	u8 tmp[10];
	struct i2c_msg msg;
	int res;

	/* write register command, writing volatile memory */
	tmp[0] = 0x60;
	tmp[1] = ((d >> 8) & 0xff);
	tmp[2] = (d & 0xff);
	tmp[3] = (reg << 2);
	msg.addr = i2c_addr;
	msg.flags = 0;	/* write */
	msg.buf = tmp;
	msg.len = 4;

	res = i2c_transfer(st->sl_handle, &msg, 1);
	if (res < 1) {
		if (res == 0)
			res = -EIO;
		return res;
	}
	/* read status */
	res = inv_i2c_read_mlx(st, i2c_addr, 10, tmp);

	return res;
}

static int inv_write_mlx_cmd(struct inv_mpu_state *st, u8 cmd)
{
	int result;
	u8 d[10];
	int addr;

	addr = st->plat_data.secondary_i2c_addr;
	result = inv_i2c_write_mlx(st, addr, cmd);
	if (result)
		return result;
	/* read back status byte */
	result = inv_i2c_read_mlx(st, addr, 10, d);

	return result;
}

static int inv_read_mlx_z_axis(struct inv_mpu_state *st, s16 *z)
{
	int result;
	u8 d[10];
	int addr;

	addr = st->plat_data.secondary_i2c_addr;

	/* measure z axis */
	result = inv_write_mlx_cmd(st, 0x39);
	if (result)
		return result;
	msleep(100);
	/* read z axis */
	result = inv_i2c_write_mlx(st, addr, 0x49);
	if (result)
		return result;
	/* read back status byte */
	result = inv_i2c_read_mlx(st, addr, 10, d);
	if (result)
		return result;
	if ((d[0] & 0x3) == 1)
		*z = (short)((d[3] << 8) + d[4]);
	else
		return -EINVAL;

	return 0;
}

static int inv_write_mlx_reg(struct inv_mpu_state *st)
{
	int result;
	int addr;
	u16 r_val;

	addr = st->plat_data.secondary_i2c_addr;

	/* write register 0.
	   set GAIN_SEL as 7;
	   set HALL_CONF as 0xC. */
	result = inv_i2c_write_mlx_reg(st, addr, 0, 0x7c);
	if (result)
		return result;
	/* write register 2.
	   set resolution is zero for all axes;
	   set DIGI filter as 6.
	   set OSR as 0.
	   set OSR2 as 0. */
	result = inv_i2c_write_mlx_reg(st, addr, 2, 0x18);
	if (result)
		return result;
	/* read register 1 */
	result = inv_i2c_read_reg_mlx(st, addr, 1, &r_val);
	if (result)
		return result;
	/* enable temp comp */
	r_val |= 0x400;
	result = inv_i2c_write_mlx_reg(st, addr, 1, r_val);
	/* the value should be kept in the volatile memory */

	return result;
}

static int inv_check_mlx_self_test(struct inv_mpu_state *st)
{
	int result;
	int addr;
	s16 meas_ref, meas_coil;
	u16 diff, r_val;

	/* set to bypass mode */
	result = inv_i2c_single_write(st, REG_INT_PIN_CFG,
				st->plat_data.int_config | BIT_BYPASS_EN);
	if (result) {
		result = inv_i2c_single_write(st, REG_INT_PIN_CFG,
				st->plat_data.int_config);
		return result;
	}

	addr = st->plat_data.secondary_i2c_addr;

	/* fake read to flush the previous data */
	result = inv_read_mlx_z_axis(st, &meas_ref);

	result = inv_read_mlx_z_axis(st, &meas_ref);
	if (result)
		return result;

	/* read register 1 */
	result = inv_i2c_read_reg_mlx(st, addr, 0, &r_val);
	if (result)
		return result;
	/* enable self test */
	r_val |= 0x100;
	result = inv_i2c_write_mlx_reg(st, addr, 0, r_val);
	if (result)
		return result;
	msleep(200);
	result = inv_read_mlx_z_axis(st, &meas_coil);
	if (result)
		return result;
	result = inv_write_mlx_cmd(st, 0xD0);
	if (result)
		return result;
	result = inv_write_mlx_reg(st);
	if (result)
		return result;
	diff = abs(meas_ref - meas_coil);
	if (diff < 25 || diff > 300)
		result = 1;

	/*restore to non-bypass mode */
	result |= inv_i2c_single_write(st, REG_INT_PIN_CFG,
			st->plat_data.int_config);

	return result;
}

/*
 *  inv_setup_compass_mlx() - Configure akm series compass.
 */
static int inv_setup_compass_mlx(struct inv_mpu_state *st)
{
	int result;
	int addr;

	addr = st->plat_data.secondary_i2c_addr;
	/* set to bypass mode */
	result = inv_i2c_single_write(st, REG_INT_PIN_CFG,
				st->plat_data.int_config | BIT_BYPASS_EN);
	if (result)
		return result;
	result = inv_write_mlx_reg(st);
	if (result)
		return result;

	/*restore to non-bypass mode */
	result = inv_i2c_single_write(st, REG_INT_PIN_CFG,
						st->plat_data.int_config);
	if (result)
		return result;

	/*setup master mode and master clock and ES bit*/
	result = inv_i2c_single_write(st, REG_I2C_MST_CTRL, BIT_WAIT_FOR_ES);
	if (result)
		return result;

	/* slave 0 used to write read measurement command, write mode */
	result = inv_i2c_single_write(st, REG_I2C_SLV0_ADDR, addr);
	if (result)
		return result;
	/* ignore the register address, send out data only */
	result = inv_i2c_single_write(st, INV_MPU_REG_I2C_SLV0_DO,
					DATA_MLX_CMD_READ_MEASURE);
	if (result)
		return result;

	/* slave 1 used to read status bytes and data of read measurement */
	result = inv_i2c_single_write(st, REG_I2C_SLV1_ADDR,
						INV_MPU_BIT_I2C_READ | addr);
	if (result)
		return result;
	/* slave 2 used to write single measurement command, write mode */
	result = inv_i2c_single_write(st, REG_I2C_SLV2_ADDR, addr);
	if (result)
		return result;
	/* ignore the register address, send out data only */
	result = inv_i2c_single_write(st, INV_MPU_REG_I2C_SLV2_DO,
					DATA_MLX_CMD_SINGLE_MEASURE);
	if (result)
		return result;
	/* slave 3 used to read status bytes and data of read measurement */
	result = inv_i2c_single_write(st, REG_I2C_SLV3_ADDR,
					INV_MPU_BIT_I2C_READ | addr);

	st->slave_compass->scale = DATA_MLX_SCALE;

	return result;
}

static int inv_suspend_mlx(struct inv_mpu_state *st)
{
	int result;

	result = inv_i2c_single_write(st, REG_I2C_SLV0_CTRL, 0);
	if (result)
		return result;
	result = inv_i2c_single_write(st, REG_I2C_SLV1_CTRL, 0);
	if (result)
		return result;
	result = inv_i2c_single_write(st, REG_I2C_SLV2_CTRL, 0);
	if (result)
		return result;
	result = inv_i2c_single_write(st, REG_I2C_SLV3_CTRL, 0);

	return result;
}

static int inv_resume_mlx(struct inv_mpu_state *st)
{
	int result;

	/* enable, ignore register, write 1 bytes */
	result = inv_i2c_single_write(st, REG_I2C_SLV0_CTRL,
						INV_MPU_BIT_SLV_EN |
						INV_MPU_BIT_REG_DIS |
						1);
	if (result)
		return result;

	/* enable, ignore register, read 9 bytes */
	result = inv_i2c_single_write(st, REG_I2C_SLV1_CTRL,
						INV_MPU_BIT_SLV_EN |
						INV_MPU_BIT_REG_DIS |
						DATA_MLX_READ_DATA_BYTES);
	if (result)
		return result;
	/* enable, ignore register, write 1 bytes */
	result = inv_i2c_single_write(st, REG_I2C_SLV2_CTRL,
						INV_MPU_BIT_SLV_EN |
						INV_MPU_BIT_REG_DIS |
						1);
	if (result)
		return result;

	/* enable, ignore register, read 1 bytes */
	result = inv_i2c_single_write(st, REG_I2C_SLV3_CTRL,
						INV_MPU_BIT_SLV_EN |
						INV_MPU_BIT_REG_DIS |
						1);

	return result;
}

static struct inv_mpu_slave slave_akm = {
	.suspend   = inv_suspend_akm,
	.resume    = inv_resume_akm,
	.get_scale = inv_read_akm_scale,
	.set_scale = inv_write_akm_scale,
	.self_test = inv_check_akm_self_test,
	.setup     = inv_setup_compass_akm,
	.read_data = inv_akm_read_data,
	.rate_scale = AKM_RATE_SCALE,
	.min_read_time = DATA_AKM_MIN_READ_TIME,
};

static struct inv_mpu_slave slave_mlx90399 = {
	.suspend   = inv_suspend_mlx,
	.resume    = inv_resume_mlx,
	.get_scale = inv_read_mlx_scale,
	.set_scale = inv_write_mlx_scale,
	.self_test = inv_check_mlx_self_test,
	.setup     = inv_setup_compass_mlx,
	.read_data = inv_mlx_read_data,
	.rate_scale = MLX_RATE_SCALE,
	.min_read_time = DATA_MLX_MIN_READ_TIME,
};

int inv_mpu_setup_compass_slave(struct inv_mpu_state *st)
{
	switch (st->plat_data.sec_slave_id) {
	case COMPASS_ID_AK8975:
	case COMPASS_ID_AK8972:
	case COMPASS_ID_AK8963:
	case COMPASS_ID_AK09911:
		st->slave_compass = &slave_akm;
		break;
	case COMPASS_ID_MLX90399:
		st->slave_compass = &slave_mlx90399;
		break;
	default:
		return -EINVAL;
	}

	return st->slave_compass->setup(st);
}

