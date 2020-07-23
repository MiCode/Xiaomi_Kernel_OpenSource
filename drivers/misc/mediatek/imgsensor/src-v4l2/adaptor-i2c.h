/* SPDX-License-Identifier: GPL-2.0 */
/* Copyright (c) 2019 MediaTek Inc. */

#ifndef __ADAPTOR_I2C_H__
#define __ADAPTOR_I2C_H__

#define IMGSENSOR_I2C_MSG_SIZE_READ 2
#define IMGSENSOR_I2C_BURST_WRITE_LENGTH MAX_DMA_TRANS_SIZE
#define IMGSENSOR_I2C_CMD_LENGTH_MAX 255

#ifdef IMGSENSOR_I2C_1000K
#define IMGSENSOR_I2C_SPEED 1000
#else
#define IMGSENSOR_I2C_SPEED 400
#endif

struct IMGSENSOR_I2C_STATUS {
	u8 reserved:7;
	u8 filter_msg:1;
};

struct IMGSENSOR_I2C_INST {
	struct IMGSENSOR_I2C_STATUS status;
	struct i2c_client *pi2c_client;
	struct i2c_msg msg[IMGSENSOR_I2C_CMD_LENGTH_MAX];
};

struct IMGSENSOR_I2C_CFG {
	struct IMGSENSOR_I2C_INST *pinst;
	struct i2c_driver *pi2c_driver;
	struct mutex i2c_mutex;
};

void adaptor_legacy_set_i2c_client(struct i2c_client *i2c_client);
void adaptor_legacy_lock(void);
void adaptor_legacy_unlock(void);

#endif
