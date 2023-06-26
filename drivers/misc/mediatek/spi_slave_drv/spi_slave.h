/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef _H_SPI_SLAVE_H_
#define _H_SPI_SLAVE_H_

#include <linux/types.h>

#define SPI_TX_LOW_SPEED_HZ (6500000)
#define SPI_RX_LOW_SPEED_HZ (6500000)
#define SPI_TX_MAX_SPEED_HZ (55000000)
#define SPI_RX_MAX_SPEED_HZ (28000000)

/*
 * Public function API for user
 */
extern int spislv_init(void);
extern int spislv_switch_speed_hz(u32 tx_speed_hz, u32 rx_speed_hz);

extern int spislv_write(u32 addr, void *val, u32 len);
extern int spislv_read(u32 addr, void *val, u32 len);
extern int spislv_read_register(u32 addr, u32 *val);

/*
 * NOTE: multi threads cann't write the same addr concurrently when using belowing APIs.
 */
extern int spislv_write_register(u32 addr, u32 val);
extern int spislv_write_register_mask(u32 addr, u32 val, u32 msk);

#endif /*_H_SPI_SLAVE_H_*/
