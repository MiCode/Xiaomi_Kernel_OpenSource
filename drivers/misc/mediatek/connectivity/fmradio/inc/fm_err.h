/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __FM_ERR_H__
#define __FM_ERR_H__

#include <linux/kernel.h>	/* for printk() */

#define FM_ERR_BASE 1000
typedef enum fm_drv_err_t {
	FM_EOK = FM_ERR_BASE,
	FM_EBUF,
	FM_EPARA,
	FM_ELINK,
	FM_ELOCK,
	FM_EFW,
	FM_ECRC,
	FM_EWRST,		/* wholechip reset */
	FM_ESRST,		/* subsystem reset */
	FM_EPATCH,
	FM_ENOMEM,
	FM_EINUSE,		/* other client is using this object */
	FM_EMAX
} fm_drv_err_t;

#endif /* __FM_ERR_H__ */
