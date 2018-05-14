/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/* register high bit=1 for read */
#define ICM20602_READ_REG(reg) (reg | 0x80)

/* register high bit=0 for write */
#define ICM20602_WRITE_REG(reg) (reg & (~0x80))
#define ICM20602_WHO_AM_I 0x12
#define ICM20602_INTERNAL_SAMPLE_RATE_HZ 1000

#define SELFTEST_COUNT 200
#define ST_PRECISION 1000
#define ACC_ST_SHIFT_MAX	150
#define ACC_ST_SHIFT_MIN	50
#define ACC_ST_AL_MIN		225
#define ACC_ST_AL_MAX		675
#define GYRO_ST_SHIFT	50
#define GYRO_ST_AL		60
#define GYRO_OFFSET_MAX	20

static const uint16_t mpu_st_tb[256] = {
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
	30903, 31212, 31524, 31839, 32157, 32479, 32804
};

enum inv_icm20602_reg_addr {
	ADDR_XG_OFFS_TC_H = 0x04,
	ADDR_XG_OFFS_TC_L,
	ADDR_YG_OFFS_TC_H = 0x07,
	ADDR_YG_OFFS_TC_L,
	ADDR_ZG_OFFS_TC_H = 0x0A,
	ADDR_ZG_OFFS_TC_L,

	ADDR_SELF_TEST_X_ACCEL = 0x0D,
	ADDR_SELF_TEST_Y_ACCEL,
	ADDR_SELF_TEST_Z_ACCEL,

	ADDR_XG_OFFS_USRH = 0x13,
	ADDR_XG_OFFS_USRL,
	ADDR_YG_OFFS_USRH,
	ADDR_YG_OFFS_USRL,
	ADDR_ZG_OFFS_USRH,
	ADDR_ZG_OFFS_USRL,

	ADDR_SMPLRT_DIV,
	ADDR_CONFIG,

	ADDR_GYRO_CONFIG,

	ADDR_ACCEL_CONFIG,
	ADDR_ACCEL_CONFIG2,

	ADDR_LP_MODE_CFG,

	ADDR_ACCEL_WOM_X_THR = 0x20,
	ADDR_ACCEL_WOM_Y_THR,
	ADDR_ACCEL_WOM_Z_THR,
	ADDR_FIFO_EN,

	ADDR_FSYNC_INT = 0x36,
	ADDR_INT_PIN_CFG,
	ADDR_INT_ENABLE,
	ADDR_FIFO_WM_INT_STATUS,
	ADDR_INT_STATUS,

	ADDR_ACCEL_XOUT_H,
	ADDR_ACCEL_XOUT_L,
	ADDR_ACCEL_YOUT_H,
	ADDR_ACCEL_YOUT_L,
	ADDR_ACCEL_ZOUT_H,
	ADDR_ACCEL_ZOUT_L,

	ADDR_TEMP_OUT_H,
	ADDR_TEMP_OUT_L,

	ADDR_GYRO_XOUT_H,
	ADDR_GYRO_XOUT_L,
	ADDR_GYRO_YOUT_H,
	ADDR_GYRO_YOUT_L,
	ADDR_GYRO_ZOUT_H,
	ADDR_GYRO_ZOUT_L,

	ADDR_SELF_TEST_X_GYRO = 0x50,
	ADDR_SELF_TEST_Y_GYRO,
	ADDR_SELF_TEST_Z_GYRO,

	ADDR_FIFO_WM_TH1 = 0x60,
	ADDR_FIFO_WM_TH2,

	ADDR_SIGNAL_PATH_RESET = 0x68,
	ADDR_ACCEL_INTEL_CTRL,
	ADDR_USER_CTRL,

	ADDR_PWR_MGMT_1,
	ADDR_PWR_MGMT_2,

	ADDR_I2C_IF = 0x70,

	ADDR_FIFO_COUNTH = 0x72,
	ADDR_FIFO_COUNTL,
	ADDR_FIFO_R_W,

	ADDR_WHO_AM_I,

	ADDR_XA_OFFSET_H,
	ADDR_XA_OFFSET_L,
	ADDR_YA_OFFSET_H = 0x7A,
	ADDR_YA_OFFSET_L,
	ADDR_ZA_OFFSET_H = 0x7D,
	ADDR_ZA_OFFSET_L
};

struct struct_XG_OFFS_TC_H {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 REG_XG_OFFS_TC_H :2;
			u8 REG_XG_OFFS_LP	:6;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_XG_OFFS_TC_L {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 REG_XG_OFFS_TC_L	:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_YG_OFFS_TC_H {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 REG_YG_OFFS_TC_H	:2;
			u8 REG_YG_OFFS_LP	:6;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_YG_OFFS_TC_L {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 REG_YG_OFFS_TC_L :8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ZG_OFFS_TC_H {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 REG_ZG_OFFS_TC_H :2;
			u8 REG_ZG_OFFS_LP	:6;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ZG_OFFS_TC_L {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 REG_ZG_OFFS_TC_L :8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_SELF_TEST_X_ACCEL {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 XA_ST_DATA	:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_SELF_TEST_Y_ACCEL {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 YA_ST_DATA	:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_SELF_TEST_Z_ACCEL {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 ZA_ST_DATA	:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_XG_OFFS_USRH {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 X_OFFS_USR	:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_XG_OFFS_USRL {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 X_OFFS_USR	:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_YG_OFFS_USRH {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 Y_OFFS_USR	:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_YG_OFFS_USRL {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 Y_OFFS_USR	:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ZG_OFFS_USRH {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 Z_OFFS_USR	:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ZG_OFFS_USRL {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 Z_OFFS_USR	:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_SMPLRT_DIV {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 SMPLRT_DIV	:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_CONFIG {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 DLFP_CFG		:3;
			u8 EXT_SYNC_SET	:3;
			u8 FIFO_MODE	:1;
			u8 USER_SET_BIT	:1;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_GYRO_CONFIG {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 FCHOICE_B	:2;
			u8 RESERVE0		:1;
			u8 FS_SEL		:2;
			u8 ZG_ST		:1;
			u8 YG_ST		:1;
			u8 XG_ST		:1;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ACCEL_CONFIG {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 RESERVE0			:3;
			u8 ACCEL_FS_SEL		:2;
			u8 ZG_ST			:1;
			u8 YG_ST			:1;
			u8 XG_ST			:1;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ACCEL_CONFIG2 {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 A_DLPF_CFG		:3;
			u8 ACCEL_FCHOICE_B	:1;
			u8 DEC2_CFG			:2;
			u8 RESERVE0			:2;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_LP_MODE_CFG {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 RESERVE0			:4;
			u8 G_AVGCFG			:3;
			u8 GYRO_CYCLE		:1;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ACCEL_WOM_X_THR {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 WOM_X_TH			:8;
		} REG;
		u8 reg;
	} reg_u;
};


struct struct_ACCEL_WOM_Y_THR {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 WOM_Y_TH			:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ACCEL_WOM_Z_THR {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 WOM_Z_TH			:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_FIFO_EN {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 RESERVE1			:3;
			u8 ACCEL_FIFO_EN	:1;
			u8 GYRO_FIFO_EN		:1;
			u8 RESERVE0			:3;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_FSYNC_INT {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 RESERVE0			:7;
			u8 FSYNC_INT		:1;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_INT_PIN_CFG {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 RESERVE0			:2;
			u8 FSYNC_INT_MODE_EN:1;
			u8 FSYNC_INT_LEVEL	:1;
			u8 INT_RD_CLEAR		:1;
			u8 LATCH_INT_EN		:1;
			u8 INT_OPEN			:1;
			u8 INT_LEVEL		:1;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_INT_ENABLE {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 DATA_RDY_INT_EN	:1;
			u8 RESERVE0			:1;
			u8 GDRIVE_INT_EN	:1;
			u8 FSYNC_INT_EN		:1;
			u8 FIFO_OFLOW_EN	:1;
			u8 WOM_Z_INT_EN		:1;
			u8 WOM_Y_INT_EN		:1;
			u8 WOM_X_INT_EN		:1;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_FIFO_WM_INT_STATUS {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 RESERVE1			:6;
			u8 FIFO_WM_INT		:1;
			u8 RESERVE0			:1;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_INT_STATUS {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 DATA_RDY_INT		:1;
			u8 RESERVE1			:1;
			u8 GDRIVE_INT		:1;
			u8 RESERVE0			:1;
			u8 FIFO_OFLOW_INT	:1;
			u8 WOM_Z_INT		:1;
			u8 WOM_Y_INT		:1;
			u8 WOM_X_INT		:1;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ACCEL_XOUT_H {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 ACCEL_XOUT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ACCEL_XOUT_L {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 ACCEL_XOUT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ACCEL_YOUT_H {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 ACCEL_YOUT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ACCEL_YOUT_L {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 ACCEL_YOUT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ACCEL_ZOUT_H {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 ACCEL_ZOUT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ACCEL_ZOUT_L {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 ACCEL_ZOUT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_TEMP_OUT_H {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 TEMP_OUT			:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_TEMP_OUT_L {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 TEMP_OUT			:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_GYRO_XOUT_H {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 GYRO_XOUT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_GYRO_XOUT_L {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 GYRO_XOUT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_GYRO_YOUT_H {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 GYRO_YOUT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_GYRO_YOUT_L {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 GYRO_YOUT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_GYRO_ZOUT_H {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 GYRO_ZOUT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_GYRO_ZOUT_L {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 GYRO_ZOUT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_SELF_TEST_X_GYRO {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 XG_ST_DATA		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_SELF_TEST_Y_GYRO {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 YG_ST_DATA		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_SELF_TEST_Z_GYRO {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 ZG_ST_DATA		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_FIFO_WM_TH1 {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 RESERVE0			:6;
			u8 FIFO_WM_TH		:2;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_FIFO_WM_TH2 {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 FIFO_WM_TH		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_SIGNAL_PATH_RESET {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 TEMP_RST			:1;
			u8 ACCEL_RST		:1;
			u8 FIFO_WM_TH		:6;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ACCEL_INTEL_CTRL {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 WOM_TH_MODE		:1;
			u8 OUTPUT_LIMIT		:1;
			u8 RESERVE0			:4;
			u8 ACCEL_INTEL_MODE	:1;
			u8 ACCEL_INTEL_EN	:1;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_USER_CTRL {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 SIG_COND_RST		:1;
			u8 RESERVE2			:1;
			u8 FIFO_RST			:1;
			u8 RESERVE1			:3;
			u8 FIFO_EN			:1;
			u8 RESERVE0			:1;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_PWR_MGMT_1 {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 CLKSEL			:3;
			u8 TEMP_DIS			:1;
			u8 GYRO_STANDBY		:1;
			u8 CYCLE			:1;
			u8 SLEEP			:1;
			u8 DEVICE_RESET		:1;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_PWR_MGMT_2 {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 STBY_ZG			:1;
			u8 STBY_YG			:1;
			u8 STBY_XG			:1;
			u8 STBY_ZA			:1;
			u8 STBY_YA			:1;
			u8 STBY_XA			:1;
			u8 RESERVE0			:2;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_I2C_IF {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 RESERVE1			:6;
			u8 I2C_IF_DIS		:1;
			u8 RESERVE0			:1;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_FIFO_COUNTH {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 FIFO_COUNT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_FIFO_COUNTL {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 FIFO_COUNT		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_FIFO_R_W {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 FIFO_DATA		:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_WHO_AM_I {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 WHOAMI			:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_XA_OFFSET_H {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 XA_OFFS			:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_XA_OFFSET_L {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 XA_OFFS			:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_YA_OFFSET_H {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 YA_OFFS			:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_YA_OFFSET_L {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 YA_OFFS			:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ZA_OFFSET_H {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 ZA_OFFS			:8;
		} REG;
		u8 reg;
	} reg_u;
};

struct struct_ZA_OFFSET_L {
	enum inv_icm20602_reg_addr address;
	union {
		struct {
			u8 ZA_OFFS			:8;
		} REG;
		u8 reg;
	} reg_u;
};

/*
 *  struct inv_icm20602_reg_map - Notable registers.
 *  @sample_rate_div:	Divider applied to gyro output rate.
 *  @lpf:		Configures internal low pass filter.
 *  @user_ctrl:		Enables/resets the FIFO.
 *  @fifo_en:		Determines which data will appear in FIFO.
 *  @gyro_config:	gyro config register.
 *  @accl_config:	accel config register
 *  @fifo_count_h:	Upper byte of FIFO count.
 *  @fifo_r_w:		FIFO register.
 *  @raw_gyro:		Address of first gyro register.
 *  @raw_accl:		Address of first accel register.
 *  @temperature:	temperature register
 *  @int_enable:	Interrupt enable register.
 *  @pwr_mgmt_1:	Controls chip's power state and clock source.
 *  @pwr_mgmt_2:	Controls power state of individual sensors.
 */
struct inv_icm20602_reg_map {
	struct struct_XG_OFFS_TC_H XG_OFFS_TC_H;
	struct struct_XG_OFFS_TC_L XG_OFFS_TC_L;
	struct struct_YG_OFFS_TC_H YG_OFFS_TC_H;
	struct struct_YG_OFFS_TC_L YG_OFFS_TC_L;
	struct struct_ZG_OFFS_TC_H ZG_OFFS_TC_H;
	struct struct_ZG_OFFS_TC_L ZG_OFFS_TC_L;

	struct struct_SELF_TEST_X_ACCEL SELF_TEST_X_ACCEL;
	struct struct_SELF_TEST_Y_ACCEL SELF_TEST_Y_ACCEL;
	struct struct_SELF_TEST_Z_ACCEL SELF_TEST_Z_ACCEL;

	struct struct_XG_OFFS_USRH XG_OFFS_USRH;
	struct struct_XG_OFFS_USRL XG_OFFS_USRL;
	struct struct_YG_OFFS_USRH YG_OFFS_USRH;
	struct struct_YG_OFFS_USRL YG_OFFS_USRL;
	struct struct_ZG_OFFS_USRH ZG_OFFS_USRH;
	struct struct_ZG_OFFS_USRL ZG_OFFS_USRL;

	struct struct_SMPLRT_DIV SMPLRT_DIV;
	struct struct_CONFIG CONFIG;

	struct struct_GYRO_CONFIG GYRO_CONFIG;

	struct struct_ACCEL_CONFIG ACCEL_CONFIG;
	struct struct_ACCEL_CONFIG2 ACCEL_CONFIG2;

	struct struct_LP_MODE_CFG LP_MODE_CFG;

	struct struct_ACCEL_WOM_X_THR ACCEL_WOM_X_THR;
	struct struct_ACCEL_WOM_Y_THR ACCEL_WOM_Y_THR;
	struct struct_ACCEL_WOM_Z_THR ACCEL_WOM_Z_THR;

	struct struct_FIFO_EN FIFO_EN;
	struct struct_FSYNC_INT FSYNC_INT;
	struct struct_INT_PIN_CFG INT_PIN_CFG;
	struct struct_INT_ENABLE INT_ENABLE;
	struct struct_FIFO_WM_INT_STATUS FIFO_WM_INT_STATUS;
	struct struct_INT_STATUS INT_STATUS;

	struct struct_ACCEL_XOUT_H ACCEL_XOUT_H;
	struct struct_ACCEL_XOUT_L ACCEL_XOUT_L;
	struct struct_ACCEL_YOUT_H ACCEL_YOUT_H;
	struct struct_ACCEL_YOUT_L ACCEL_YOUT_L;
	struct struct_ACCEL_ZOUT_H ACCEL_ZOUT_H;
	struct struct_ACCEL_ZOUT_L ACCEL_ZOUT_L;

	struct struct_TEMP_OUT_H TEMP_OUT_H;
	struct struct_TEMP_OUT_L TEMP_OUT_L;

	struct struct_GYRO_XOUT_H GYRO_XOUT_H;
	struct struct_GYRO_XOUT_L GYRO_XOUT_L;
	struct struct_GYRO_YOUT_H GYRO_YOUT_H;
	struct struct_GYRO_YOUT_L GYRO_YOUT_L;
	struct struct_GYRO_ZOUT_H GYRO_ZOUT_H;
	struct struct_GYRO_ZOUT_L GYRO_ZOUT_L;

	struct struct_SELF_TEST_X_GYRO SELF_TEST_X_GYRO;
	struct struct_SELF_TEST_Y_GYRO SELF_TEST_Y_GYRO;
	struct struct_SELF_TEST_Z_GYRO SELF_TEST_Z_GYRO;
	struct struct_FIFO_WM_TH1 FIFO_WM_TH1;
	struct struct_FIFO_WM_TH2 FIFO_WM_TH2;
	struct struct_SIGNAL_PATH_RESET SIGNAL_PATH_RESET;
	struct struct_ACCEL_INTEL_CTRL ACCEL_INTEL_CTRL;
	struct struct_USER_CTRL USER_CTRL;

	struct struct_PWR_MGMT_1 PWR_MGMT_1;
	struct struct_PWR_MGMT_2 PWR_MGMT_2;

	struct struct_I2C_IF I2C_IF;

	struct struct_FIFO_COUNTH FIFO_COUNTH;
	struct struct_FIFO_COUNTL FIFO_COUNTL;
	struct struct_FIFO_R_W FIFO_R_W;

	struct struct_WHO_AM_I WHO_AM_I;

	struct struct_XA_OFFSET_H XA_OFFSET_H;
	struct struct_XA_OFFSET_L XA_OFFSET_L;
	struct struct_YA_OFFSET_H YA_OFFSET_H;
	struct struct_YA_OFFSET_L YA_OFFSET_L;
	struct struct_ZA_OFFSET_H ZA_OFFSET_H;
	struct struct_ZA_OFFSET_L ZA_OFFSET_L;
};


