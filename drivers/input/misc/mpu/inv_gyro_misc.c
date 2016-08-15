/*
* Copyright (C) 2012 Invensense, Inc.
* Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
*/

/**
 *  @addtogroup  DRIVERS
 *  @brief       Hardware drivers.
 *
 *  @{
 *      @file    inv_gyro_misc.c
 *      @brief   A sysfs device driver for Invensense gyroscopes.
 *      @details This file is part of inv_gyro driver code
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/sysfs.h>
#include <linux/jiffies.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/kfifo.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>

#include "inv_gyro.h"

/*
    Defines
*/

/*--- Test parameters defaults --- */

#define DEF_OLDEST_SUPP_PROD_REV    (8)
#define DEF_OLDEST_SUPP_SW_REV      (2)

/* sample rate */
#define DEF_SELFTEST_SAMPLE_RATE             (0)
/* LPF parameter */
#define DEF_SELFTEST_LPF_PARA                (1)
/* full scale setting dps */
#define DEF_SELFTEST_GYRO_FULL_SCALE         (0 << 3)
#define DEF_SELFTEST_ACCL_FULL_SCALE         (2 << 3)
/* wait time before collecting data */
#define DEF_GYRO_WAIT_TIME          (50)
#define DEF_GYRO_PACKET_THRESH      DEF_GYRO_WAIT_TIME
#define DEF_GYRO_THRESH             (10)
#define DEF_GYRO_PRECISION          (1000)
#define X                           0
#define Y                           1
#define Z                           2
/*---- MPU6050 notable product revisions ----*/
#define MPU_PRODUCT_KEY_B1_E1_5      105
#define MPU_PRODUCT_KEY_B2_F1        431
/* accelerometer Hw self test min and max bias shift (mg) */
#define DEF_ACCEL_ST_SHIFT_MIN      (300)
#define DEF_ACCEL_ST_SHIFT_MAX      (950)

#define DEF_ACCEL_ST_SHIFT_DELTA    (500)
#define DEF_GYRO_CT_SHIFT_DELTA     (500)
/* gyroscope Coriolis self test min and max bias shift (dps) */
#define DEF_GYRO_CT_SHIFT_MIN       (10)
#define DEF_GYRO_CT_SHIFT_MAX       (105)

static struct test_setup_t test_setup = {
	.gyro_sens	= 32768 / 250,
	.sample_rate	= DEF_SELFTEST_SAMPLE_RATE,
	.lpf		= DEF_SELFTEST_LPF_PARA,
	.gyro_fsr	= DEF_SELFTEST_GYRO_FULL_SCALE,
	.accl_fsr	= DEF_SELFTEST_ACCL_FULL_SCALE
};

/* NOTE: product entries are in chronological order */
static struct prod_rev_map_t prod_rev_map[] = {
	/* prod_ver = 0 */
	{MPL_PROD_KEY(0,   1), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   2), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   3), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   4), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   5), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   6), MPU_SILICON_REV_A2, 131, 16384},	/* (A2/C2-1) */
	/* prod_ver = 1, forced to 0 for MPU6050 A2 */
	{MPL_PROD_KEY(0,   7), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   8), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,   9), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,  10), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,  11), MPU_SILICON_REV_A2, 131, 16384},	/* (A2/D2-1) */
	{MPL_PROD_KEY(0,  12), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,  13), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,  14), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,  15), MPU_SILICON_REV_A2, 131, 16384},
	{MPL_PROD_KEY(0,  27), MPU_SILICON_REV_A2, 131, 16384},	/* (A2/D4)   */
	/* prod_ver = 1 */
	{MPL_PROD_KEY(1,  16), MPU_SILICON_REV_B1, 131, 16384},	/* (B1/D2-1) */
	{MPL_PROD_KEY(1,  17), MPU_SILICON_REV_B1, 131, 16384},	/* (B1/D2-2) */
	{MPL_PROD_KEY(1,  18), MPU_SILICON_REV_B1, 131, 16384},	/* (B1/D2-3) */
	{MPL_PROD_KEY(1,  19), MPU_SILICON_REV_B1, 131, 16384},	/* (B1/D2-4) */
	{MPL_PROD_KEY(1,  20), MPU_SILICON_REV_B1, 131, 16384},	/* (B1/D2-5) */
	{MPL_PROD_KEY(1,  28), MPU_SILICON_REV_B1, 131, 16384},	/* (B1/D4)   */
	{MPL_PROD_KEY(1,   1), MPU_SILICON_REV_B1, 131, 16384},	/* (B1/E1-1) */
	{MPL_PROD_KEY(1,   2), MPU_SILICON_REV_B1, 131, 16384},	/* (B1/E1-2) */
	{MPL_PROD_KEY(1,   3), MPU_SILICON_REV_B1, 131, 16384},	/* (B1/E1-3) */
	{MPL_PROD_KEY(1,   4), MPU_SILICON_REV_B1, 131, 16384},	/* (B1/E1-4) */
	{MPL_PROD_KEY(1,   5), MPU_SILICON_REV_B1, 131, 16384},	/* (B1/E1-5) */
	{MPL_PROD_KEY(1,   6), MPU_SILICON_REV_B1, 131, 16384},	/* (B1/E1-6) */
	/* prod_ver = 2 */
	{MPL_PROD_KEY(2,   7), MPU_SILICON_REV_B1, 131, 16384},	/* (B2/E1-1) */
	{MPL_PROD_KEY(2,   8), MPU_SILICON_REV_B1, 131, 16384},	/* (B2/E1-2) */
	{MPL_PROD_KEY(2,   9), MPU_SILICON_REV_B1, 131, 16384},	/* (B2/E1-3) */
	{MPL_PROD_KEY(2,  10), MPU_SILICON_REV_B1, 131, 16384},	/* (B2/E1-4) */
	{MPL_PROD_KEY(2,  11), MPU_SILICON_REV_B1, 131, 16384},	/* (B2/E1-5) */
	{MPL_PROD_KEY(2,  12), MPU_SILICON_REV_B1, 131, 16384},	/* (B2/E1-6) */
	{MPL_PROD_KEY(2,  29), MPU_SILICON_REV_B1, 131, 16384},	/* (B2/D4)   */
	/* prod_ver = 3 */
	{MPL_PROD_KEY(3,  30), MPU_SILICON_REV_B1, 131, 16384},	/* (B2/E2)   */
	/* prod_ver = 4 */
	{MPL_PROD_KEY(4,  31), MPU_SILICON_REV_B1, 131,  8192},	/* (B2/F1)   */
	{MPL_PROD_KEY(4,   1), MPU_SILICON_REV_B1, 131,  8192},	/* (B3/F1)   */
	{MPL_PROD_KEY(4,   3), MPU_SILICON_REV_B1, 131,  8192},	/* (B4/F1)   */
	/* prod_ver = 5 */
	{MPL_PROD_KEY(5,   3), MPU_SILICON_REV_B1, 131, 16384},	/* (B4/F1)   */
	/* prod_ver = 6 */
	{MPL_PROD_KEY(6,  19), MPU_SILICON_REV_B1, 131, 16384},	/* (B5/E2)   */
	/* prod_ver = 7 */
	{MPL_PROD_KEY(7,  19), MPU_SILICON_REV_B1, 131, 16384},	/* (B5/E2)   */
	/* prod_ver = 8 */
	{MPL_PROD_KEY(8,  19), MPU_SILICON_REV_B1, 131, 16384},	/* (B5/E2)   */
	/* prod_ver = 9 */
	{MPL_PROD_KEY(9,  19), MPU_SILICON_REV_B1, 131, 16384},	/* (B5/E2)   */
	/* prod_ver = 10 */
	{MPL_PROD_KEY(10, 19), MPU_SILICON_REV_B1, 131, 16384}	/* (B5/E2)   */
};

/*
   List of product software revisions

   NOTE :
   software revision 0 falls back to the old detection method
   based off the product version and product revision per the
   table above
*/
static struct prod_rev_map_t sw_rev_map[] = {
	{0,		     0,   0,     0},
	{1, MPU_SILICON_REV_B1, 131,  8192},	/* rev C */
	{2, MPU_SILICON_REV_B1, 131, 16384}	/* rev D */
};

static int accl_st_tb[31] = {
	340, 351, 363, 375, 388, 401, 414, 428,
	443, 458, 473, 489, 506, 523, 541, 559,
	578, 597, 617, 638, 660, 682, 705, 729,
	753, 779, 805, 832, 860, 889, 919};

static int gyro_6050_st_tb[31] = {
	3275, 3425, 3583, 3748, 3920, 4100, 4289, 4486,
	4693, 4909, 5134, 5371, 5618, 5876, 6146, 6429,
	6725, 7034, 7358, 7696, 8050, 8421, 8808, 9213,
	9637, 10080, 10544, 11029, 11537, 12067, 12622};

static int gyro_3500_st_tb[255] = {
	2620, 2646, 2672, 2699, 2726, 2753, 2781, 2808,
	2837, 2865, 2894, 2923, 2952, 2981, 3011, 3041,
	3072, 3102, 3133, 3165, 3196, 3228, 3261, 3293,
	3326, 3359, 3393, 3427, 3461, 3496, 3531, 3566,
	3602, 3638, 3674, 3711, 3748, 3786, 3823, 3862,
	3900, 3939, 3979, 4019, 4059, 4099, 4140, 4182,
	4224, 4266, 4308, 4352, 4395, 4439, 4483, 4528,
	4574, 4619, 4665, 4712, 4759, 4807, 4855, 4903,
	4953, 5002, 5052, 5103, 5154, 5205, 5257, 5310,
	5363, 5417, 5471, 5525, 5581, 5636, 5693, 5750,
	5807, 5865, 5924, 5983, 6043, 6104, 6165, 6226,
	6289, 6351, 6415, 6479, 6544, 6609, 6675, 6742,
	6810, 6878, 6946, 7016, 7086, 7157, 7229, 7301,
	7374, 7448, 7522, 7597, 7673, 7750, 7828, 7906,
	7985, 8065, 8145, 8227, 8309, 8392, 8476, 8561,
	8647, 8733, 8820, 8909, 8998, 9088, 9178, 9270,
	9363, 9457, 9551, 9647, 9743, 9841, 9939, 10038,
	10139, 10240, 10343, 10446, 10550, 10656, 10763, 10870,
	10979, 11089, 11200, 11312, 11425, 11539, 11654, 11771,
	11889, 12008, 12128, 12249, 12371, 12495, 12620, 12746,
	12874, 13002, 13132, 13264, 13396, 13530, 13666, 13802,
	13940, 14080, 14221, 14363, 14506, 14652, 14798, 14946,
	15096, 15247, 15399, 15553, 15709, 15866, 16024, 16184,
	16346, 16510, 16675, 16842, 17010, 17180, 17352, 17526,
	17701, 17878, 18057, 18237, 18420, 18604, 18790, 18978,
	19167, 19359, 19553, 19748, 19946, 20145, 20347, 20550,
	20756, 20963, 21173, 21385, 21598, 21814, 22033, 22253,
	22475, 22700, 22927, 23156, 23388, 23622, 23858, 24097,
	24338, 24581, 24827, 25075, 25326, 25579, 25835, 26093,
	26354, 26618, 26884, 27153, 27424, 27699, 27976, 28255,
	28538, 28823, 29112, 29403, 29697, 29994, 30294, 30597,
	30903, 31212, 31524, 31839, 32157, 32479, 32804};

int mpu_memory_write(struct i2c_adapter *i2c_adap,
		     unsigned char mpu_addr,
		     unsigned short mem_addr,
		     unsigned int len, unsigned char const *data)
{
	unsigned char bank[2];
	unsigned char addr[2];
	unsigned char buf[513];
	struct i2c_msg msgs[3];
	int res;

	if (!data || !i2c_adap)
		return -EINVAL;

	if (len >= (sizeof(buf) - 1))
		return -ENOMEM;

	bank[0] = REG_BANK_SEL;
	bank[1] = mem_addr >> 8;
	addr[0] = REG_MEM_START;
	addr[1] = mem_addr & 0xFF;
	buf[0] = REG_MEM_RW;
	memcpy(buf + 1, data, len);
	/* write message */
	msgs[0].addr = mpu_addr;
	msgs[0].flags = 0;
	msgs[0].buf = bank;
	msgs[0].len = sizeof(bank);
	msgs[1].addr = mpu_addr;
	msgs[1].flags = 0;
	msgs[1].buf = addr;
	msgs[1].len = sizeof(addr);
	msgs[2].addr = mpu_addr;
	msgs[2].flags = 0;
	msgs[2].buf = (unsigned char *)buf;
	msgs[2].len = len + 1;
	res = i2c_transfer(i2c_adap, msgs, 3);
	if (res != 3) {
		if (res >= 0)
			res = -EIO;
		return res;
	}

	return 0;
}

int mpu_memory_read(struct i2c_adapter *i2c_adap,
		    unsigned char mpu_addr,
		    unsigned short mem_addr,
		    unsigned int len, unsigned char *data)
{
	unsigned char bank[2];
	unsigned char addr[2];
	unsigned char buf;
	struct i2c_msg msgs[4];
	int res;

	if (!data || !i2c_adap)
		return -EINVAL;

	bank[0] = REG_BANK_SEL;
	bank[1] = mem_addr >> 8;
	addr[0] = REG_MEM_START;
	addr[1] = mem_addr & 0xFF;
	buf = REG_MEM_RW;
	/* write message */
	msgs[0].addr = mpu_addr;
	msgs[0].flags = 0;
	msgs[0].buf = bank;
	msgs[0].len = sizeof(bank);
	msgs[1].addr = mpu_addr;
	msgs[1].flags = 0;
	msgs[1].buf = addr;
	msgs[1].len = sizeof(addr);
	msgs[2].addr = mpu_addr;
	msgs[2].flags = 0;
	msgs[2].buf = &buf;
	msgs[2].len = 1;
	msgs[3].addr = mpu_addr;
	msgs[3].flags = I2C_M_RD;
	msgs[3].buf = data;
	msgs[3].len = len;
	res = i2c_transfer(i2c_adap, msgs, 4);
	if (res != 4) {
		if (res >= 0)
			res = -EIO;
		return res;
	}

	return 0;
}

int mpu_memory_read_6500(struct inv_gyro_state_s *st, u8 mpu_addr, u16 mem_addr,
			 u32 len, u8 *data)
{
	u8 bank[2];
	u8 addr[2];
	u8 buf;

	struct i2c_msg msgs[4];
	int res;

	if (!data || !st)
		return -EINVAL;

	bank[0] = REG_BANK_SEL;
	bank[1] = mem_addr >> 8;

	addr[0] = REG_MEM_START;
	addr[1] = mem_addr & 0xFF;

	buf = REG_MEM_RW;

	/* write message */
	msgs[0].addr = mpu_addr;
	msgs[0].flags = 0;
	msgs[0].buf = bank;
	msgs[0].len = sizeof(bank);

	msgs[1].addr = mpu_addr;
	msgs[1].flags = 0;
	msgs[1].buf = addr;
	msgs[1].len = sizeof(addr);

	msgs[2].addr = mpu_addr;
	msgs[2].flags = 0;
	msgs[2].buf = &buf;
	msgs[2].len = 1;

	msgs[3].addr = mpu_addr;
	msgs[3].flags = I2C_M_RD;
	msgs[3].buf = data;
	msgs[3].len = len;

	res = i2c_transfer(st->sl_handle, msgs, 4);
	if (res != 4) {
		if (res >= 0)
			res = -EIO;
	} else
		res = 0;
	return res;
}

/**
 *  @internal
 *  @brief  Inverse lookup of the index of an MPL product key .
 *  @param  key
 *              the MPL product indentifier also referred to as 'key'.
 *  @return the index position of the key in the array, -1 if not found.
 */
static short index_of_key(unsigned short key)
{
	int i;

	for (i = 0; i < NUM_OF_PROD_REVS; i++)
		if (prod_rev_map[i].mpl_product_key == key)
			return (short)i;

	return -1;
}

int inv_get_silicon_rev_mpu6500(struct inv_gyro_state_s *st)
{
	struct inv_chip_info_s *chip_info = &st->chip_info;
	int result;
	u8 sw_rev;

	/*memory read need more time after power up */
	msleep(POWER_UP_TIME);
	result = mpu_memory_read_6500(st, st->i2c_addr,
			MPU6500_MEM_REV_ADDR, 1, &sw_rev);
	if (sw_rev == 0) {
		pr_warning("Rev 0 of MPU6500\n");
		pr_warning("can't sit with other devices in same I2C bus\n");
	}
	if (result)
		return result;

	/* these values are place holders and not real values */
	chip_info->product_id = MPU6500_PRODUCT_REVISION;
	chip_info->product_revision = MPU6500_PRODUCT_REVISION;
	chip_info->silicon_revision = MPU6500_PRODUCT_REVISION;
	chip_info->software_revision = sw_rev;
	chip_info->gyro_sens_trim = DEFAULT_GYRO_TRIM;
	chip_info->accl_sens_trim = DEFAULT_ACCL_TRIM;
	chip_info->multi = 1;
	return 0;
}

int inv_get_silicon_rev_mpu6050(struct inv_gyro_state_s *st)
{
	int result;
	struct inv_reg_map_s *reg;
	unsigned char prod_ver = 0x00, prod_rev = 0x00;
	struct prod_rev_map_t *p_rev;
	unsigned char bank =
	    (BIT_PRFTCH_EN | BIT_CFG_USER_BANK | MPU_MEM_OTP_BANK_0);
	unsigned short mem_addr = ((bank << 8) | 0x06);
	unsigned short key;
	unsigned char regs[5];
	unsigned short sw_rev;
	short index;
	struct inv_chip_info_s *chip_info = &st->chip_info;

	reg = st->reg;
	result = inv_i2c_read(st, reg->product_id, 1, &prod_ver);
	if (result)
		return result;

	prod_ver &= 0xf;
	result = mpu_memory_read(st->sl_handle, st->i2c_addr, mem_addr,
			1, &prod_rev);
	mdelay(100);
	result = mpu_memory_read(st->sl_handle, st->i2c_addr, mem_addr,
			1, &prod_rev);
	if (result)
		return result;

	prod_rev >>= 2;
	/* clean the prefetch and cfg user bank bits */
	result = inv_i2c_single_write(st, reg->bank_sel, 0);
	if (result)
		return result;

	/* get the software-product version, read from XA_OFFS_L */
	result = inv_i2c_read(st, 0x7, 5, regs);
	if (result)
		return result;

	sw_rev = (regs[4] & 0x01) << 2 |	/* 0x0b, bit 0 */
		 (regs[2] & 0x01) << 1 |	/* 0x09, bit 0 */
		 (regs[0] & 0x01);		/* 0x07, bit 0 */
	/* if 0, use the product key to determine the type of part */
	if (sw_rev == 0) {
		key = MPL_PROD_KEY(prod_ver, prod_rev);
		if (key == 0)
			return -1;

		index = index_of_key(key);
		if (index == -1 || index >= NUM_OF_PROD_REVS)
			return -1;

		/* check MPL is compiled for this device */
		if (prod_rev_map[index].silicon_rev != MPU_SILICON_REV_B1)
			return -1;

		p_rev = &prod_rev_map[index];
	/* if valid, use the software product key */
	} else if (sw_rev < ARRAY_SIZE(sw_rev_map)) {
		p_rev = &sw_rev_map[sw_rev];
	} else {
		return -1;
	}

	chip_info->product_id = prod_ver;
	chip_info->product_revision = prod_rev;
	chip_info->silicon_revision = p_rev->silicon_rev;
	chip_info->software_revision = sw_rev;
	chip_info->gyro_sens_trim = p_rev->gyro_trim;
	chip_info->accl_sens_trim = p_rev->accel_trim;
	if (chip_info->accl_sens_trim == 0)
		chip_info->accl_sens_trim = DEFAULT_ACCL_TRIM;
	chip_info->multi = DEFAULT_ACCL_TRIM/chip_info->accl_sens_trim;
	if (chip_info->multi != 1)
		printk(KERN_ERR"multi is %d\n", chip_info->multi);
	return result;
}

/**
 *  @internal
 *  @brief  read the accelerometer hardware self-test bias shift calculated
 *          during final production test and stored in chip non-volatile memory.
 *  @param  mlsl_handle
 *              serial interface handle to allow serial communication with the
 *              device, both gyro and accelerometer.
 *  @param  ct_shift_prod
 *              A pointer to an array of 3 float elements to hold the values
 *              for production hardware self-test bias shifts returned to the
 *              user.
 *  @return INV_SUCCESS on success, or a non-zero error code otherwise.
 */
static int read_accel_hw_self_test_prod_shift(struct inv_gyro_state_s *st,
					int *st_prod)
{
	unsigned char regs[4];
	unsigned char shift_code[3];
	int result, i;
	st_prod[0] = st_prod[1] = st_prod[2] = 0;

	result = inv_i2c_read(st, 0x0d, 4, regs);
	if (result)
		return result;

	if ((0 == regs[0])  && (0 == regs[1]) &&
		(0 == regs[2]) && (0 == regs[3]))
		return -1;

	shift_code[X] = ((regs[0] & 0xE0) >> 3) | ((regs[3] & 0x30) >> 4);
	shift_code[Y] = ((regs[1] & 0xE0) >> 3) | ((regs[3] & 0x0C) >> 2);
	shift_code[Z] = ((regs[2] & 0xE0) >> 3) |  (regs[3] & 0x03);
	for (i = 0; i < 3; i++) {
		if (shift_code[i] != 0)
			st_prod[i] = test_setup.accl_sens[i]*
				accl_st_tb[shift_code[i] - 1];
	}
	return 0;
}

static int inv_check_accl_self_test(struct inv_gyro_state_s *st,
	int *reg_avg, int *st_avg){
	int gravity, reg_z_avg, g_z_sign, fs, j, ret_val;
	int tmp1;
	int st_shift_prod[3], st_shift_cust[3], st_shift_ratio[3];

	if (st->chip_info.software_revision < DEF_OLDEST_SUPP_SW_REV &&
		st->chip_info.product_revision < DEF_OLDEST_SUPP_PROD_REV)
		return 0;

	fs = 8000UL;    /* assume +/- 2 mg as typical */
	g_z_sign = 1;
	ret_val = 0;
	test_setup.accl_sens[X] = (unsigned int)((1L << 15) * 1000 / fs);
	test_setup.accl_sens[Y] = (unsigned int)((1L << 15) * 1000 / fs);
	test_setup.accl_sens[Z] = (unsigned int)((1L << 15) * 1000 / fs);
	if (MPL_PROD_KEY(st->chip_info.product_id,
		st->chip_info.product_revision) ==
		MPU_PRODUCT_KEY_B1_E1_5) {
		/* half sensitivity Z accelerometer parts */
		test_setup.accl_sens[Z] /= 2;
	} else {
		/* half sensitivity X, Y, Z accelerometer parts */
		test_setup.accl_sens[X] /= st->chip_info.multi;
		test_setup.accl_sens[Y] /= st->chip_info.multi;
		test_setup.accl_sens[Z] /= st->chip_info.multi;
	}
	gravity = test_setup.accl_sens[Z];
	reg_z_avg = reg_avg[Z] - g_z_sign * gravity * DEF_GYRO_PRECISION;
	read_accel_hw_self_test_prod_shift(st, st_shift_prod);
	for (j = 0; j < 3; j++) {
		st_shift_cust[j] = abs(reg_avg[j] - st_avg[j]);
		if (st_shift_prod[j]) {
			tmp1 = st_shift_prod[j]/1000;
			st_shift_ratio[j] = st_shift_cust[j]/tmp1
				- 1000;
			if (st_shift_ratio[j] > DEF_ACCEL_ST_SHIFT_DELTA)
				ret_val |= 1 << j;
			if (st_shift_ratio[j] < -DEF_ACCEL_ST_SHIFT_DELTA)
				ret_val |= 1 << j;
		} else {
			if (st_shift_cust[j] <
				DEF_ACCEL_ST_SHIFT_MIN*gravity)
				ret_val |= 1 << j;
			if (st_shift_cust[j] >
				DEF_ACCEL_ST_SHIFT_MAX*gravity)
				ret_val |= 1 << j;
		}
	}
	return ret_val;
}

static int inv_check_3500_gyro_self_test(struct inv_gyro_state_s *st,
	int *reg_avg, int *st_avg){
	int result;
	int gst[3], ret_val;
	int gst_otp[3], i;
	unsigned char st_code[3];

	ret_val = 0;
	for (i = 0; i < 3; i++)
		gst[i] = st_avg[i] - reg_avg[i];
	result = inv_i2c_read(st, REG_3500_OTP, 3, st_code);
	if (result)
		return result;

	gst_otp[0] = gst_otp[1] = gst_otp[2] = 0;
	for (i = 0; i < 3; i++) {
		if (st_code[i] != 0)
			gst_otp[i] = gyro_3500_st_tb[st_code[i] - 1];
	}
	for (i = 0; i < 3; i++) {
		if (gst_otp[i] == 0) {
			if (abs(gst[i])*4 < 60*2*1000*131)
				ret_val |= (1<<i);
		} else {
			if (abs(gst[i]/gst_otp[i] - 1000) > 140)
				ret_val |= (1<<i);
		}
	}
	for (i = 0; i < 3; i++) {
		if (abs(reg_avg[i])*4 > 20*2*1000*131)
			ret_val |= (1<<i);
	}

	return ret_val;
}
static int inv_check_6050_gyro_self_test(struct inv_gyro_state_s *st,
					 int *reg_avg, int *st_avg)
{
	int result;
	int ret_val;
	int ct_shift_prod[3], st_shift_cust[3], st_shift_ratio[3], i;
	unsigned char regs[3];

	if (st->chip_info.software_revision < DEF_OLDEST_SUPP_SW_REV &&
		st->chip_info.product_revision < DEF_OLDEST_SUPP_PROD_REV)
		return 0;

	ret_val = 0;
	result = inv_i2c_read(st, REG_ST_GCT_X, 3, regs);
	regs[X] &= 0x1f;
	regs[Y] &= 0x1f;
	regs[Z] &= 0x1f;
	for (i = 0; i < 3; i++) {
		if (regs[i] != 0)
			ct_shift_prod[i] = gyro_6050_st_tb[regs[i] - 1];
		else
			ct_shift_prod[i] = 0;
	}
	for (i = 0; i < 3; i++) {
		st_shift_cust[i] = abs(reg_avg[i] - st_avg[i]);
		if (ct_shift_prod[i]) {
			st_shift_ratio[i] = st_shift_cust[i]/
				ct_shift_prod[i] - 1000;
			if (st_shift_ratio[i] > DEF_GYRO_CT_SHIFT_DELTA)
				ret_val |= 1 << i;
			if (st_shift_ratio[i] < -DEF_GYRO_CT_SHIFT_DELTA)
				ret_val |= 1 << i;
		} else {
			if (st_shift_cust[i] < 1000*DEF_GYRO_CT_SHIFT_MIN*
				test_setup.gyro_sens)
				ret_val |= 1 << i;
			if (st_shift_cust[i] > 1000*DEF_GYRO_CT_SHIFT_MAX*
				test_setup.gyro_sens)
				ret_val |= 1 << i;
		}
	}
	for (i = 0; i < 3; i++) {
		if (abs(reg_avg[i]) > 20*2*1000*131)
			ret_val |= (1<<i);
	}
	return ret_val;
}

/**
 *  inv_do_test() - do the actual test of self testing
 */
static int inv_do_test(struct inv_gyro_state_s *st, int self_test_flag,
		       int *gyro_result, int *accl_result)
{
	struct inv_reg_map_s *reg;
	int result, i, packet_size;
	unsigned char data[12], has_accl;
	int fifo_count, packet_count, ind;

	reg = st->reg;
	has_accl = (st->chip_type != INV_ITG3500);
	packet_size = 6 + 6 * has_accl;

	result = nvi_pm_wr(st, INV_CLK_PLL, 0, 0);
	if (result)
		return result;

	mdelay(POWER_UP_TIME << 1);
	result = nvi_int_enable_wr(st, false);
	if (result)
		return result;

	/* disable the sensor output to FIFO */
	/* disable fifo reading */
	result = nvi_user_ctrl_en(st, false, false);
	if (result)
		return result;

	/* clear FIFO */
	result = nvi_user_ctrl_reset_wr(st, st->hw.user_ctrl |
					st->reg->fifo_reset);
	if (result)
		return result;

	mdelay(15);
	/* setup parameters */
	result = nvi_config_wr(st, test_setup.lpf);
	if (result < 0)
		return result;

	result = nvi_smplrt_div_wr(st, test_setup.sample_rate);
	if (result)
		return result;

	result = nvi_gyro_config_wr(st, (self_test_flag >> 5),
				    (test_setup.gyro_fsr >> 3));
	if (result < 0)
		return result;

	if (has_accl) {
		result = nvi_accel_config_wr(st, (self_test_flag >> 5),
					     (test_setup.accl_fsr >> 3), 0);
		if (result < 0)
			return result;
	}

	/*wait for the output to stable*/
	if (self_test_flag)
		mdelay(200);
	/* enable FIFO reading */
	result = nvi_user_ctrl_en_wr(st, st->hw.user_ctrl | BIT_FIFO_EN);
	if (result)
		return result;

	/* enable sensor output to FIFO */
	result = nvi_fifo_en_wr(st, BITS_GYRO_OUT | (has_accl << 3));
	if (result)
		return result;

	mdelay(DEF_GYRO_WAIT_TIME);
	/* stop sending data to FIFO */
	result = nvi_fifo_en_wr(st, 0);
	if (result)
		return result;

	result = inv_i2c_read(st, reg->fifo_count_h, 2, data);
	if (result)
		return result;

	fifo_count = (data[0] << 8) + data[1];
	packet_count = fifo_count / packet_size;
	gyro_result[0] = gyro_result[1] = gyro_result[2] = 0;
	accl_result[0] = accl_result[1] = accl_result[2] = 0;
	if (abs(packet_count - DEF_GYRO_PACKET_THRESH)
		<= DEF_GYRO_THRESH) {
		for (i = 0; i < packet_count; i++) {
			/* getting FIFO data */
			result = inv_i2c_read(st, reg->fifo_r_w,
				packet_size, data);
			if (result)
				return result;

			ind = 0;
			if (has_accl) {
				accl_result[0] += (short)((data[ind] << 8)
						| data[ind + 1]);
				accl_result[1] += (short)((data[ind + 2] << 8)
						| data[ind + 3]);
				accl_result[2] += (short)((data[ind + 4] << 8)
						| data[ind + 5]);
				ind += 6;
			}
			gyro_result[0] += (short)((data[ind] << 8) |
				data[ind + 1]);
			gyro_result[1] += (short)((data[ind + 2] << 8) |
				data[ind + 3]);
			gyro_result[2] += (short)((data[ind + 4] << 8) |
				data[ind + 5]);
		}
	} else {
		return -EAGAIN;
	}

	gyro_result[0] = gyro_result[0] * DEF_GYRO_PRECISION / packet_count;
	gyro_result[1] = gyro_result[1] * DEF_GYRO_PRECISION / packet_count;
	gyro_result[2] = gyro_result[2] * DEF_GYRO_PRECISION / packet_count;
	if (has_accl) {
		accl_result[0] =
			accl_result[0] * DEF_GYRO_PRECISION / packet_count;
		accl_result[1] =
			accl_result[1] * DEF_GYRO_PRECISION / packet_count;
		accl_result[2] =
			accl_result[2] * DEF_GYRO_PRECISION / packet_count;
	}
	result = inv_i2c_read(st, st->reg->temperature, 2, data);
	if (!result)
		nvi_report_temp(st, data, nvi_ts_ns());
	return 0;
}

/**
 *  inv_recover_setting() recover the old settings after everything is done
 */
static void inv_recover_setting(struct inv_gyro_state_s *st)
{
	unsigned char enable;
	unsigned char fifo_enable;

	nvi_accel_config_wr(st, 0, st->chip_config.accl_fsr, 0);
	nvi_gyro_config_wr(st, 0, st->chip_config.gyro_fsr);
	enable = st->chip_config.accl_enable;
	fifo_enable = st->chip_config.accl_fifo_enable;
	st->chip_config.accl_enable ^= 7;
	st->chip_config.accl_fifo_enable ^= 7;
	nvi_accl_enable(st, enable, fifo_enable);
	enable = st->chip_config.gyro_enable;
	fifo_enable = st->chip_config.gyro_fifo_enable;
	st->chip_config.gyro_enable ^= 7;
	st->chip_config.gyro_fifo_enable ^= 7;
	nvi_gyro_enable(st, enable, fifo_enable);
}

/**
 *  inv_hw_self_test() - main function to do hardware self test
 */
int inv_hw_self_test(struct inv_gyro_state_s *st,
		     int *gyro_bias_regular)
{
	int result;
	int gyro_bias_st[3];
	int accl_bias_st[3], accl_bias_regular[3];
	int test_times;
	char accel_result, gyro_result;

	accel_result = gyro_result = 0;
	test_times = 2;
	while (test_times > 0) {
		result = inv_do_test(st, 0, gyro_bias_regular,
				     accl_bias_regular);
		if (result == -EAGAIN)
			test_times--;
		else
			test_times = 0;
	}
	if (result)
		goto test_fail;

	test_times = 2;
	while (test_times > 0) {
		result = inv_do_test(st, BITS_SELF_TEST_EN, gyro_bias_st,
				     accl_bias_st);
		if (result == -EAGAIN)
			test_times--;
		else
			break;
	}
	if (result)
		goto test_fail;

	if (st->chip_type == INV_ITG3500) {
		gyro_result = !inv_check_3500_gyro_self_test(st,
					      gyro_bias_regular, gyro_bias_st);
	} else {
		accel_result = !inv_check_accl_self_test(st,
					      accl_bias_regular, accl_bias_st);
		gyro_result = !inv_check_6050_gyro_self_test(st,
					      gyro_bias_regular, gyro_bias_st);
	}
test_fail:
	inv_recover_setting(st);
	return (accel_result << 1) | gyro_result;
}

/**
 *  inv_get_accl_bias() - main function to do hardware self test
 */
int inv_get_accl_bias(struct inv_gyro_state_s *st, int *accl_bias_regular)
{
	int result;
	int gyro_bias_regular[3];
	int test_times;

	test_times = 2;
	while (test_times > 0) {
		result = inv_do_test(st, 0,  gyro_bias_regular,
			accl_bias_regular);
		if (result == -EAGAIN)
			test_times--;
		else
			test_times = 0;
	}
	inv_recover_setting(st);
	return result;
}

static int inv_load_firmware(struct inv_gyro_state_s *st,
			     unsigned char *data, int size)
{
	int bank, write_size;
	int result;
	unsigned short memaddr;

	/* Write and verify memory */
	for (bank = 0; size > 0; bank++,
		size -= write_size,
		data += write_size) {
		if (size > MPU_MEM_BANK_SIZE)
			write_size = MPU_MEM_BANK_SIZE;
		else
			write_size = size;

		memaddr = ((bank << 8) | 0x00);
		result = mem_w(memaddr, write_size, data);
		if (result)
			return result;
	}

	return 0;
}

static int inv_verify_firmware(struct inv_gyro_state_s *st,
			       unsigned char *data, int size)
{
	int bank, write_size;
	int result;
	unsigned short memaddr;
	unsigned char firmware[MPU_MEM_BANK_SIZE];

	/* Write and verify memory */
	for (bank = 0; size > 0; bank++,
		size -= write_size,
		data += write_size) {
		if (size > MPU_MEM_BANK_SIZE)
			write_size = MPU_MEM_BANK_SIZE;
		else
			write_size = size;
		memaddr = ((bank << 8) | 0x00);
		result = mpu_memory_read(st->sl_handle,
			st->i2c_addr, memaddr, write_size, firmware);
		if (result)
			return result;

		if (0 != memcmp(firmware, data, write_size))
			return -EINVAL;
	}

	return 0;
}

static int inv_set_fifo_div(struct inv_gyro_state_s *st,
			    unsigned short fifoRate)
{
	unsigned char regs[2];
	int result = 0;
	/* For some reason DINAC4 is defined as 0xb8,
	   but DINBC4 is not defined*/
	unsigned char regs_end[8] = {DINAFE, DINAF2, DINAAB,
		0xC4, DINAAA, DINAF1, DINADF, DINADF};

	regs[0] = (unsigned char)((fifoRate >> 8) & 0xff);
	regs[1] = (unsigned char)(fifoRate & 0xff);
	result = mem_w_key(KEY_D_0_22, 2, regs);
	if (result)
		return result;

	/*Modify the FIFO handler to reset the tap/orient interrupt flags*/
	/* each time the FIFO handler runs*/
	result = mem_w_key(KEY_CFG_6, 8, regs_end);

	return result;
}

int inv_set_fifo_rate(struct inv_gyro_state_s *st, unsigned long fifo_rate)
{
	unsigned char divider;
	int result;

	divider = (unsigned char)(1000/fifo_rate) - 1;
	if (divider > 4) {
		st->sample_divider = 4;
		st->fifo_divider = (unsigned char)(200/fifo_rate) - 1;
	} else {
		st->sample_divider = divider;
		st->fifo_divider = 0;
	}
	result = inv_set_fifo_div(st, st->fifo_divider);
	return result;
}

static int inv_set_tap_interrupt_dmp(struct inv_gyro_state_s *st,
				     unsigned char on)
{
	int result;
	unsigned char  regs[2] = {0};

	if (on)
		regs[0] = 0xf8;
	else
		regs[0] = DINAD8;
	result = mem_w_key(KEY_CFG_20, 1, regs);
	if (result)
		return result;

	return result;
}

static int inv_set_orientation_interrupt_dmp(struct inv_gyro_state_s *st,
					     unsigned char on)
{
	int result;
	unsigned char  regs[2] = {0};

	if (on) {
		regs[0] = DINBF8;
		regs[1] = DINBF8;
	} else {
		regs[0] = DINAD8;
		regs[1] = DINAD8;
	}
	result = mem_w_key(KEY_CFG_ORIENT_IRQ_1, 2, regs);
	if (result)
		return result;

	return result;
}

static int inv_set_tap_threshold_dmp(struct inv_gyro_state_s *st,
				     unsigned int axis,
				     unsigned short threshold)
{
	/* Sets the tap threshold in the dmp
	Simultaneously sets secondary tap threshold to help correct the tap
	direction for soft taps */
	int result;
	/* DMP Algorithm */
	unsigned char data[2];
	int sampleDivider;
	int scaledThreshold;
	unsigned int dmpThreshold;
	unsigned char sample_div;
#define accel_sens  (0x20000000/(0x00010000))

	if ((axis & ~(INV_TAP_AXIS_ALL)) || (threshold > (1<<15)))
		return -EINVAL;

	sample_div = st->sample_divider;
	sampleDivider = (1 + sample_div);
	/* Scale factor corresponds linearly using
	* 0  : 0
	* 25 : 0.0250  g/ms
	* 50 : 0.0500  g/ms
	* 100: 1.0000  g/ms
	* 200: 2.0000  g/ms
	* 400: 4.0000  g/ms
	* 800: 8.0000  g/ms
	*/
	/*multiply by 1000 to avoid floating point 1000/1000*/
	scaledThreshold = threshold;
	/* Convert to per sample */
	scaledThreshold *= sampleDivider;
	/* Scale to DMP 16 bit value */
	if (accel_sens != 0)
		dmpThreshold = (unsigned int)(scaledThreshold*accel_sens);
	else
		return -EINVAL;

	dmpThreshold = dmpThreshold/1000;
	data[0] = dmpThreshold >> 8;
	data[1] = dmpThreshold & 0xFF;
	/* MPL algorithm */
	if (axis & INV_TAP_AXIS_X) {
		result = mem_w_key(KEY_DMP_TAP_THR_X, 2, data);
		if (result)
			return result;

		/*Also set additional threshold for correcting the direction
		of taps that were very near the threshold. */
		data[0] = (dmpThreshold*3/4) >> 8;
		data[1] = (dmpThreshold*3/4) & 0xFF;
		result = mem_w_key(KEY_D_1_36, 2, data);
		if (result)
			return result;
	}

	if (axis & INV_TAP_AXIS_Y) {
		result = mem_w_key(KEY_DMP_TAP_THR_Y, 2, data);
		if (result)
			return result;

		data[0] = (dmpThreshold*3/4) >> 8;
		data[1] = (dmpThreshold*3/4) & 0xFF;
		result = mem_w_key(KEY_D_1_40, 2, data);
		if (result)
			return result;
	}

	if (axis & INV_TAP_AXIS_Z) {
		result = mem_w_key(KEY_DMP_TAP_THR_Z, 2, data);
		if (result)
			return result;

		data[0] = (dmpThreshold*3/4) >> 8;
		data[1] = (dmpThreshold*3/4) & 0xFF;
		result = mem_w_key(KEY_D_1_44, 2, data);
		if (result)
			return result;
	}

	return 0;
}

static int inv_set_tap_axes_dmp(struct inv_gyro_state_s *st,
				unsigned int axes)
{
	/* Sets a mask in the DMP that indicates what tap events
	should result in an interrupt */
	unsigned char regs[4];
	unsigned char result;

	/* check if any spurious bit other the ones expected are set */
	if (axes & (~(INV_TAP_ALL_DIRECTIONS)))
		return -EINVAL;

	regs[0] = (unsigned char)axes;
	result = mem_w_key(KEY_D_1_72, 1, regs);
	return result;
}

static int inv_set_min_taps_dmp(struct inv_gyro_state_s *st,
				unsigned int min_taps) {
	/*Indicates the minimum number of consecutive taps required
		before the DMP will generate an interrupt */
	unsigned char regs[1];
	unsigned char result;

	/* check if any spurious bit other the ones expected are set */
	if ((min_taps > 4) || (min_taps < 1))
		return -EINVAL;

	regs[0] = (unsigned char)(min_taps-1);
	result = mem_w_key(KEY_D_1_79, 1, regs);
	return result;
}

static int  inv_set_tap_time_dmp(struct inv_gyro_state_s *st,
				 unsigned int time)
{
	/* Determines how long after a tap the DMP requires before
	  another tap can be registered*/
	int result;
	/* DMP Algorithm */
	unsigned short dmpTime;
	unsigned char data[2];
	unsigned char sampleDivider;

	sampleDivider = st->sample_divider;
	sampleDivider++;
	/* 60 ms minimum time added */
	dmpTime = ((time) / sampleDivider);
	data[0] = dmpTime >> 8;
	data[1] = dmpTime & 0xFF;
	result = mem_w_key(KEY_DMP_TAPW_MIN, 2, data);
	return result;
}

static int inv_set_multiple_tap_time_dmp(struct inv_gyro_state_s *st,
					 unsigned int time)
{
	/*Determines how close together consecutive taps must occur
	to be considered double/triple taps*/
	int result;
	/* DMP Algorithm */
	unsigned short dmpTime;
	unsigned char data[2];
	unsigned char sampleDivider;

	sampleDivider = st->sample_divider;
	sampleDivider++;
	/* 60 ms minimum time added */
	dmpTime = ((time) / sampleDivider);
	data[0] = dmpTime >> 8;
	data[1] = dmpTime & 0xFF;
	result = mem_w_key(KEY_D_1_218, 2, data);
	return result;
}

static long inv_q30_mult(long a, long b)
{
	long long temp;
	long result;

	temp = (long long)a * b;
	result = (long)(temp >> 30);
	return result;
}

#define gyro_sens 0x03e80000

static int inv_set_gyro_sf_dmp(struct inv_gyro_state_s *st)
{
	/*The gyro threshold, in dps, above which taps will be rejected*/
	int result, out;
	/* DMP Algorithm */
	unsigned char sampleDivider;
	unsigned char *regs;
	int gyro_sf;

	sampleDivider = st->sample_divider;
	gyro_sf = inv_q30_mult(gyro_sens,
			(int)((767603923/5) * (sampleDivider+1)));
	out = cpu_to_be32p(&gyro_sf);
	regs = (unsigned char *)&out;
	result = mem_w_key(KEY_D_0_104, 4, regs);
	return result;
}

static int inv_set_shake_reject_thresh_dmp(struct inv_gyro_state_s *st,
					   int thresh)
{/*THIS FUNCTION FAILS MEM_W*/
	/*The gyro threshold, in dps, above which taps will be rejected */
	int result, out;
	/* DMP Algorithm */
	unsigned char sampleDivider;
	int thresh_scaled;
	unsigned char *regs;
	long gyro_sf;

	sampleDivider = st->sample_divider;
	gyro_sf = inv_q30_mult(gyro_sens, (int)((767603923/5) *
			(sampleDivider+1)));
	/* We're in units of DPS, convert it back to chip units*/
	/*split the operation to aviod overflow of integer*/
	thresh_scaled = gyro_sens/(1L<<16);
	thresh_scaled = thresh_scaled/thresh;
	thresh_scaled = gyro_sf / thresh_scaled;
	out = cpu_to_be32p(&thresh_scaled);
	regs = (unsigned char *)&out;
	result = mem_w_key(KEY_D_1_92, 4, regs);
	return result;
}

static int inv_set_shake_reject_time_dmp(struct inv_gyro_state_s *st,
					 unsigned int time)
{
	/* How long a gyro axis must remain above its threshold
	before taps are rejected */
	int result;
	/* DMP Algorithm */
	unsigned short dmpTime;
	unsigned char data[2];
	unsigned char sampleDivider;

	sampleDivider = st->sample_divider;
	sampleDivider++;
	/* 60 ms minimum time added */
	dmpTime = ((time) / sampleDivider);
	data[0] = dmpTime >> 8;
	data[1] = dmpTime & 0xFF;
	result = mem_w_key(KEY_D_1_88, 2, data);
	return result;
}

static int inv_set_shake_reject_timeout_dmp(struct inv_gyro_state_s *st,
					    unsigned int time)
{
	/*How long the gyros must remain below their threshold,
	after taps have been rejected, before taps can be detected again*/
	int result;
	/* DMP Algorithm */
	unsigned short dmpTime;
	unsigned char data[2];
	unsigned char sampleDivider;

	sampleDivider = st->sample_divider;
	sampleDivider++;
	/* 60 ms minimum time added */
	dmpTime = ((time) / sampleDivider);
	data[0] = dmpTime >> 8;
	data[1] = dmpTime & 0xFF;
	result = mem_w_key(KEY_D_1_90, 2, data);
	return result;
}

static int inv_set_interrupt_on_gesture_event(struct inv_gyro_state_s *st,
					      char on)
{
	unsigned char result;
	unsigned char regs_on[12] = {DINADA, DINADA, DINAB1, DINAB9,
					DINAF3, DINA8B, DINAA3, DINA91,
					DINAB6, DINADA, DINAB4, DINADA};
	unsigned char regs_off[12] = {0xd8, 0xd8, 0xb1, 0xb9, 0xf3, 0x8b,
					0xa3, 0x91, 0xb6, 0x09, 0xb4, 0xd9};
	/*For some reason DINAC4 is defined as 0xb8,
	but DINBC4 is not defined.*/
	unsigned char regs_end[8] = {DINAFE, DINAF2, DINAAB, 0xc4,
					DINAAA, DINAF1, DINADF, DINADF};

	if (on) {
		/*Sets the DMP to send an interrupt and put a FIFO packet
		in the FIFO if and only if a tap/orientation event
		just occurred*/
		result = mem_w_key(KEY_CFG_FIFO_ON_EVENT, 12, regs_on);
		if (result)
			return result;

	} else {
		/*Sets the DMP to send an interrupt and put a FIFO packet
		in the FIFO at the rate specified by the FIFO div.
		see inv_set_fifo_div in hw_setup.c to set the FIFO div.*/
		result = mem_w_key(KEY_CFG_FIFO_ON_EVENT, 12, regs_off);
		if (result)
			return result;
	}

	result = mem_w_key(KEY_CFG_6, 8, regs_end);
	return result;
}

/**
 * inv_enable_tap_dmp() -  calling this function will enable/disable tap function.
 */
int inv_enable_tap_dmp(struct inv_gyro_state_s *st, unsigned char on)
{
	int result;

	result = inv_set_tap_interrupt_dmp(st, on);
	if (result)
		return result;

	if (on) {
		result = inv_set_tap_threshold_dmp(st, INV_TAP_AXIS_X, 100) ;
		if (result)
			return result;

		result = inv_set_tap_threshold_dmp(st, INV_TAP_AXIS_Y, 100) ;
		if (result)
			return result;

		result = inv_set_tap_threshold_dmp(st, INV_TAP_AXIS_Z, 100) ;
		if (result)
			return result;
	}

	result = inv_set_tap_axes_dmp(st, INV_TAP_ALL_DIRECTIONS) ;
	if (result)
		return result;

	result = inv_set_min_taps_dmp(st, 2);
	if (result)
		return result;

	result = inv_set_tap_time_dmp(st, 100);
	if (result)
		return result;

	result = inv_set_multiple_tap_time_dmp(st, 500) ;
	if (result)
		return result;

	result = inv_set_gyro_sf_dmp(st);
	if (result)
		return result;

	result = inv_set_shake_reject_thresh_dmp(st, 100) ;
	if (result)
		return result;

	result = inv_set_shake_reject_time_dmp(st, 10) ;
	if (result)
		return result;

	result = inv_set_shake_reject_timeout_dmp(st, 10);
	if (result)
		return result;

	result = inv_set_interrupt_on_gesture_event(st, 0);
	return result;
}

static int inv_set_orientation_dmp(struct inv_gyro_state_s *st,
				   int orientation)
{
	/*Set a mask in the DMP determining what orientations
			will trigger interrupts*/
	unsigned char regs[4];
	unsigned char result;

	/* check if any spurious bit other the ones expected are set */
	if (orientation & (~(INV_ORIENTATION_ALL | INV_ORIENTATION_FLIP)))
		return -EINVAL;

	regs[0] = (unsigned char)orientation;
	result = mem_w_key(KEY_D_1_74, 1, regs);
	return result;
}

static int inv_set_orientation_thresh_dmp(struct inv_gyro_state_s *st,
					  int angle)
{
	/*Set an angle threshold in the DMP determining
		when orientations change*/
	unsigned char *regs;
	unsigned char result;
	unsigned int out;
	unsigned int threshold;

	/*threshold = (long)((1<<29) * sin((angle * M_PI) / 180.));*/
	threshold = 464943848;
	out = cpu_to_be32p(&threshold);
	regs = (unsigned char *)&out;
	result = mem_w_key(KEY_D_1_232, 4, regs);
	return result;
}

static int inv_set_orientation_time_dmp(struct inv_gyro_state_s *st,
					unsigned int time)
{
	/*Determines the stability time required before a
	new orientation can be adopted */
	unsigned short dmpTime;
	unsigned char data[2];
	unsigned char sampleDivider;
	unsigned char result;

	/* First check if we are allowed to call this function here */
	sampleDivider = st->sample_divider;
	sampleDivider++;
	/* 60 ms minimum time added */
	dmpTime = ((time) / sampleDivider);
	data[0] = dmpTime >> 8;
	data[1] = dmpTime & 0xFF;
	result = mem_w_key(KEY_D_1_250, 2, data);
	return result;
}

/**
 * inv_enable_orientation_dmp() -  calling this function will
 *                  enable/disable orientation function.
 */
int inv_enable_orientation_dmp(struct inv_gyro_state_s *st)
{
	int result;

	result = inv_set_orientation_interrupt_dmp(st, 1);
	if (result)
		return result;

	result = inv_set_orientation_dmp(st, 0x40 | INV_ORIENTATION_ALL);
	if (result)
		return result;

	result = inv_set_gyro_sf_dmp(st);
	if (result)
		return result;

	result = inv_set_orientation_thresh_dmp(st, 60);
	if (result)
		return result;

	result = inv_set_orientation_time_dmp(st, 500);
	return result;
}

static int inv_send_sensor_data(struct inv_gyro_state_s *st,
				unsigned short elements)
{
	int result;
	unsigned char regs[10] = { DINAA0 + 3, DINAA0 + 3, DINAA0 + 3,
				  DINAA0 + 3, DINAA0 + 3, DINAA0 + 3,
				  DINAA0 + 3, DINAA0 + 3, DINAA0 + 3,
				  DINAA0 + 3 };

	if (elements & INV_ELEMENT_1)
		regs[0] = DINACA;
	if (elements & INV_ELEMENT_2)
		regs[4] = DINBC4;
	if (elements & INV_ELEMENT_3)
		regs[5] = DINACC;
	if (elements & INV_ELEMENT_4)
		regs[6] = DINBC6;
	if ((elements & INV_ELEMENT_5) || (elements & INV_ELEMENT_6)
		|| (elements & INV_ELEMENT_7)) {
		regs[1] = DINBC0;
		regs[2] = DINAC8;
		regs[3] = DINBC2;
	}
	result = mem_w_key(KEY_CFG_15, 10, regs);
	return result;
}

static int inv_send_interrupt_word(struct inv_gyro_state_s *st)
{
	unsigned char regs[1] = { DINA20 };
	unsigned char result;

	result = mem_w_key(KEY_CFG_27, 1, regs);
	return result;
}

/**
 * inv_dmp_firmware_write() -  calling this function will load the firmware.
 *                        This is the write function of file "dmp_firmware".
 */
ssize_t inv_dmp_firmware_write(struct file *fp, struct kobject *kobj,
	struct bin_attribute *attr,
	char *buf, loff_t pos, size_t size)
{
	struct inv_gyro_state_s *st;
	unsigned char *firmware;
	int result;
	struct inv_reg_map_s *reg;

	st = dev_get_drvdata(container_of(kobj, struct device, kobj));
	if (1 == st->chip_config.firmware_loaded)
		return -EINVAL;

	reg = st->reg;
	firmware = kmalloc(size, GFP_KERNEL);
	if (!firmware)
		return -ENOMEM;

	memcpy(firmware, buf, size);
	result = inv_load_firmware(st, firmware, size);
	if (result)
		goto firmware_write_fail;

	result = inv_verify_firmware(st, firmware, size);
	if (result)
		goto firmware_write_fail;

	result = inv_i2c_single_write(st, reg->prgm_strt_addrh,
	st->chip_config.prog_start_addr >> 8);
	if (result)
		goto firmware_write_fail;

	result = inv_i2c_single_write(st, reg->prgm_strt_addrh + 1,
	st->chip_config.prog_start_addr & 0xff);
	if (result)
		goto firmware_write_fail;

	result = inv_verify_firmware(st, firmware, size);
	if (result)
		goto firmware_write_fail;

	result = inv_set_fifo_rate(st, 200);
	if (result)
		goto firmware_write_fail;

	result = inv_enable_tap_dmp(st, 1);
	if (result)
		goto firmware_write_fail;

	result = inv_enable_orientation_dmp(st);
	if (result)
		goto firmware_write_fail;

	result = inv_send_sensor_data(st, INV_GYRO_ACC_MASK);
	if (result)
		goto firmware_write_fail;

	result = inv_send_interrupt_word(st);
	if (result)
		goto firmware_write_fail;

	st->chip_config.firmware_loaded = 1;
	st->chip_config.dmp_on = 1;
	result = size;
firmware_write_fail:
	kfree(firmware);
	return result;
}

ssize_t inv_dmp_firmware_read(struct file *filp, struct kobject *kobj,
			      struct bin_attribute *bin_attr,
			      char *buf, loff_t off, size_t count)
{
	struct inv_gyro_state_s *st;
	int bank, write_size, size, data, result;
	unsigned short memaddr;
	size = count;

	st = dev_get_drvdata(container_of(kobj, struct device, kobj));
	data = 0;
	for (bank = 0; size > 0; bank++, size -= write_size,
				 data += write_size) {
		if (size > MPU_MEM_BANK_SIZE)
			write_size = MPU_MEM_BANK_SIZE;
		else
			write_size = size;
		memaddr = ((bank << 8) | 0x00);
		result = mpu_memory_read(st->sl_handle,
			st->i2c_addr, memaddr, write_size, &buf[data]);
		if (result)
			return result;
	}

	return count;
}

