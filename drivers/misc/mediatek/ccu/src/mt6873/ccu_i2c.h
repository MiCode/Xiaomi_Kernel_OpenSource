/*
 * Copyright (C) 2016 MediaTek Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CCU_I2C_H__
#define __CCU_I2C_H__

enum CCU_I2C_CHANNEL {
	CCU_I2C_CHANNEL_UNDEF    = 0x0,
	CCU_I2C_CHANNEL_MIN      = 0x1,
	CCU_I2C_CHANNEL_MAINCAM  = 0x1,
	CCU_I2C_CHANNEL_MAINCAM2 = 0x2,
	CCU_I2C_CHANNEL_MAINCAM3 = 0x3,
	CCU_I2C_CHANNEL_SUBCAM   = 0x4,
	CCU_I2C_CHANNEL_SUBCAM2  = 0x5,
	CCU_I2C_CHANNEL_MAX      = 0x6
};

struct ccu_i2c_buf_mva_ioarg {
	enum CCU_I2C_CHANNEL i2c_controller_id;
	uint32_t mva;
	uint32_t va_h;
	uint32_t va_l;
	uint32_t i2c_id;
};

/*i2c driver hook*/
int ccu_i2c_register_driver(void);
int ccu_i2c_delete_driver(void);

/*ccu i2c operation*/
int ccu_i2c_set_channel(enum CCU_I2C_CHANNEL);
int ccu_get_i2c_dma_buf_addr(struct ccu_device_s *g_ccu_device,
	struct ccu_i2c_buf_mva_ioarg *ioarg);
int ccu_i2c_controller_init(enum CCU_I2C_CHANNEL i2c_controller_id);
int ccu_i2c_controller_uninit_all(void);
int ccu_i2c_free_dma_buf_mva_all(void);
void ccu_i2c_dump_errr(void);
#endif
