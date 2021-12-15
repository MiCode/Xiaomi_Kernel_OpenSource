/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
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


