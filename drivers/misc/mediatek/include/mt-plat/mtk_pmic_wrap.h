/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef __MT_PMIC_WRAP_H__
#define __MT_PMIC_WRAP_H__

#include <linux/types.h>
#include <linux/device.h>

#define PWRAP_READ 0
#define PWRAP_WRITE 1

struct mt_pmic_wrap_driver {

	struct device_driver driver;
	s32 (*wacs2_hal)(u32 write, u32 adr, u32 wdata, u32 *rdata);
	s32 (*show_hal)(char *buf);
	s32 (*store_hal)(const char *buf, size_t count);

	s32 (*suspend)(void);
	void (*resume)(void);
};

/* ------external API for pmic_wrap user-------------------------------*/
s32 pwrap_read(u32 adr, u32 *rdata);
s32 pwrap_write(u32 adr, u32 wdata);

s32 pwrap_wacs2(u32 write, u32 adr, u32 wdata, u32 *rdata);
/*_____________ROME only_____________________________________________*/
/********************************************************************/
/* return value : EINT_STA: [0]: CPU IRQ status in MT6331 */
/* [1]: MD32 IRQ status in MT6331 */
/* [2]: CPU IRQ status in MT6332 */
/* [3]: RESERVED */
/********************************************************************/
u32 pmic_wrap_eint_status(void);
/********************************************************************/
/* set value(W1C) : EINT_CLR:       [0]: CPU IRQ status in MT6331 */
/* [1]: MD32 IRQ status in MT6331 */
/* [2]: CPU IRQ status in MT6332 */
/* [3]: RESERVED */
/* para: offset is shift of clear bit which needs to clear */
/********************************************************************/
void pmic_wrap_eint_clr(int offset);
/*--------------------------------------------------------------------*/
u32 mt_pmic_wrap_eint_status(void);
void mt_pmic_wrap_eint_clr(int offset);
s32 pwrap_init(void);
struct mt_pmic_wrap_driver *get_mt_pmic_wrap_drv(void);

#endif				/* __MT_PMIC_WRAP_H__ */
