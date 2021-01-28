/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __CCU_I2C_H__
#define __CCU_I2C_H__

struct ccu_i2c_buf_mva_ioarg {
	uint32_t sensor_idx;
	uint32_t mva;
	uint32_t va_h;
	uint32_t va_l;
	uint32_t i2c_id;
};

/*i2c driver hook*/
int ccu_i2c_register_driver(void);
int ccu_i2c_delete_driver(void);

/*ccu i2c operation*/
int ccu_get_i2c_dma_buf_addr(struct ccu_device_s *g_ccu_device, struct ccu_i2c_buf_mva_ioarg *ioarg);
int ccu_i2c_controller_init(uint32_t i2c_id);
int ccu_i2c_controller_uninit_all(void);
int ccu_i2c_free_dma_buf_mva_all(void);
void ccu_i2c_dump_errr(void);
#endif
