/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/cdev.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include "seninf.h"


#define SENINF_RD32(addr)          ioread32((void *)addr)
#ifdef SENINF_IRQ
extern MINT32 _seninf_irq(MINT32 Irq, void *DeviceId, struct SENINF *pseninf)
{
	pr_debug("seninf crc/ecc erorr irq 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x\n",
	SENINF_RD32(pseninf->pseninf_base[0] + 0x0Ac8),
	SENINF_RD32(pseninf->pseninf_base[1] + 0x0Ac8),
	SENINF_RD32(pseninf->pseninf_base[2] + 0x0Ac8),
	SENINF_RD32(pseninf->pseninf_base[3] + 0x0Ac8),
	SENINF_RD32(pseninf->pseninf_base[4] + 0x0Ac8),
	SENINF_RD32(pseninf->pseninf_base[5] + 0x0Ac8));
	return 0;
}
#endif


