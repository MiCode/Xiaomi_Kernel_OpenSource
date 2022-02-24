/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __CCU_I2C_H__
#define __CCU_I2C_H__
#include "kd_camera_feature.h"

struct ccu_i2c_buf_mva_ioarg {
	uint32_t sensor_idx;
	uint32_t mva;
	uint32_t va_h;
	uint32_t va_l;
	uint32_t i2c_id;
};

struct ccu_i2c_info {
	uint32_t slave_addr;
	uint32_t i2c_id;
};

/*i2c driver hook*/
int ccu_i2c_register_driver(void);
int ccu_i2c_delete_driver(void);

/*ccu i2c operation*/
//int ccu_i2c_set_channel(enum CCU_I2C_CHANNEL);
int ccu_get_i2c_dma_buf_addr(struct ccu_i2c_buf_mva_ioarg *ioarg);
int ccu_i2c_controller_init(uint32_t i2c_id);
int ccu_i2c_controller_uninit_all(void);
int ccu_i2c_free_dma_buf_mva_all(void);
#endif
