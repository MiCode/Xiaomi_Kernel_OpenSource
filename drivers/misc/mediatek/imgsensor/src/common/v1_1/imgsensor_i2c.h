/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __IMGSENSOR_I2C_H__
#define __IMGSENSOR_I2C_H__

#include <linux/i2c.h>
#include <linux/mutex.h>

#include "i2c-mtk.h"

#include "imgsensor_cfg_table.h"
#include "imgsensor_common.h"

#define IMGSENSOR_I2C_MSG_SIZE_READ      2
#define IMGSENSOR_I2C_BURST_WRITE_LENGTH MAX_DMA_TRANS_SIZE
#define IMGSENSOR_I2C_CMD_LENGTH_MAX     255

#define IMGSENSOR_I2C_BUFF_MODE_DEV      IMGSENSOR_I2C_DEV_2

#ifdef IMGSENSOR_I2C_1000K
#define IMGSENSOR_I2C_SPEED              1000
#else
#define IMGSENSOR_I2C_SPEED              400
#endif

struct IMGSENSOR_I2C_STATUS {
	u8 reserved:7;
	u8 filter_msg:1;
};

struct IMGSENSOR_I2C_INST {
	struct IMGSENSOR_I2C_STATUS status;
	struct i2c_client          *pi2c_client;
};

struct IMGSENSOR_I2C_CFG {
	struct IMGSENSOR_I2C_INST *pinst;
	struct i2c_driver         *pi2c_driver;
	struct i2c_msg             msg[IMGSENSOR_I2C_CMD_LENGTH_MAX];
	struct mutex               i2c_mutex;
};

struct IMGSENSOR_I2C {
	struct IMGSENSOR_I2C_INST inst[IMGSENSOR_I2C_DEV_MAX_NUM];
};

enum IMGSENSOR_RETURN imgsensor_i2c_create(void);
enum IMGSENSOR_RETURN imgsensor_i2c_delete(void);
enum IMGSENSOR_RETURN imgsensor_i2c_init(
	struct IMGSENSOR_I2C_CFG *pi2c_cfg,
	enum IMGSENSOR_I2C_DEV device);
enum IMGSENSOR_RETURN imgsensor_i2c_buffer_mode(int enable);
enum IMGSENSOR_RETURN imgsensor_i2c_read(
	struct IMGSENSOR_I2C_CFG *pi2c_cfg,
	u8 *pwrite_data,
	u16 write_length,
	u8 *pread_data,
	u16 read_length,
	u16 id,
	int speed);
enum IMGSENSOR_RETURN imgsensor_i2c_write(
	struct IMGSENSOR_I2C_CFG *pi2c_cfg,
	u8 *pwrite_data,
	u16 write_length,
	u16 write_per_cycle,
	u16 id,
	int speed);

void imgsensor_i2c_filter_msg(struct IMGSENSOR_I2C_CFG *pi2c_cfg, bool en);

#ifdef IMGSENSOR_LEGACY_COMPAT
void imgsensor_i2c_set_device(struct IMGSENSOR_I2C_CFG *pi2c_cfg);
struct IMGSENSOR_I2C_CFG *imgsensor_i2c_get_device(void);
#endif

#endif

