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

#ifndef __CCU_I2C_HW_H__
#define __CCU_I2C_HW_H__

/*---------------------------------------------------------------------------*/
/*        i2c interface from ccu_i2c_hw.c */
/*---------------------------------------------------------------------------*/
extern int ccu_i2c_set_n3d_base(unsigned long n3d_base);
extern int ccu_trigger_i2c_hw(enum CCU_I2C_CHANNEL channel,
	int transac_len, MBOOL do_dma_en);

#endif
