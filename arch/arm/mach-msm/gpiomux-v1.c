/* Copyright (c) 2010-2011 The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <mach/gpiomux.h>
#include <mach/proc_comm.h>
#include "gpio.h"

void __msm_gpiomux_write(unsigned gpio, struct gpiomux_setting val)
{
	unsigned tlmm_config;
	unsigned tlmm_disable = 0;
	void __iomem *out_reg;
	unsigned offset;
	uint32_t bits;
	int rc;

	tlmm_config  = (val.drv << 17) |
		(val.pull << 15) |
		((gpio & 0x3ff) << 4) |
		val.func;
	if (val.func == GPIOMUX_FUNC_GPIO) {
		tlmm_config |= (val.dir > GPIOMUX_IN ? BIT(14) : 0);
		msm_gpio_find_out(gpio, &out_reg, &offset);
		bits = __raw_readl(out_reg);
		if (val.dir == GPIOMUX_OUT_HIGH)
			__raw_writel(bits | BIT(offset), out_reg);
		else
			__raw_writel(bits & ~BIT(offset), out_reg);
	}
	mb();
	rc = msm_proc_comm(PCOM_RPC_GPIO_TLMM_CONFIG_EX,
			   &tlmm_config, &tlmm_disable);
	if (rc)
		pr_err("%s: unexpected proc_comm failure %d: %08x %08x\n",
		       __func__, rc, tlmm_config, tlmm_disable);
}
