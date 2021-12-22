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

#ifndef _H_HIFI4DSP_SPI_H_
#define _H_HIFI4DSP_SPI_H_

#include <linux/types.h>

/* HIFI4DSP SPI xfer speed */
#define SPI_LOAD_IMAGE_SPEED		(52*1000*1000)
#define SPI_SPEED_LOW				(13*1000*1000)
#define SPI_SPEED_HIGH				(52*1000*1000)

/*
 * Public function API for audio system
 */
extern int dsp_spi_write(u32 addr, void *value, int len, u32 speed);
extern int dsp_spi_write_ex(u32 addr, void *value, int len, u32 speed);
extern int dsp_spi_read(u32 addr, void *value, int len, u32 speed);
extern int dsp_spi_read_ex(u32 addr, void *value, int len, u32 speed);

extern int spi_read_register(u32 addr, u32 *val, u32 speed);
extern int spi_write_register(u32 addr, u32 val, u32 speed);
extern int spi_set_register32(u32 addr, u32 val, u32 speed);
extern int spi_clr_register32(u32 addr, u32 val, u32 speed);
extern int spi_write_register_mask(u32 addr, u32 val, u32 msk, u32 speed);

extern int hifi4dsp_spi_get_status(void);

#endif /*_H_HIFI4DSP_SPI_H_*/
