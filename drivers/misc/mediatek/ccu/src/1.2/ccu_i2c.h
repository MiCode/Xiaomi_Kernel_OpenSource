/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2016 MediaTek Inc.
 */

#ifndef __CCU_I2C_H__
#define __CCU_I2C_H__

/*---------------------------------------------------------------------------*/
/*        i2c interface from ccu_drv.c */
/*---------------------------------------------------------------------------*/
struct ccu_i2c_arg {
	unsigned int i2c_write_id;
	unsigned int transfer_len;
};

struct ccu_i2c_info {
	uint32_t slave_addr;
	uint32_t i2c_id;
};

extern int ccu_i2c_register_driver(void);
extern int ccu_i2c_delete_driver(void);
extern int ccu_i2c_set_channel(uint32_t i2c_id);
extern int i2c_get_dma_buffer_addr(void **va,
	uint32_t *pa_h, uint32_t *pa_l, uint32_t *i2c_id);
void ccu_i2c_dump_errr(void);
extern int ccu_i2c_buf_mode_init(unsigned char i2c_write_id, int transfer_len);
extern int ccu_i2c_buf_mode_en(int enable);

#endif
