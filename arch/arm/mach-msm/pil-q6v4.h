/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#ifndef __MSM_PIL_Q6V4_H
#define __MSM_PIL_Q6V4_H

struct pil_q6v4_pdata {
	const unsigned long strap_tcm_base;
	const unsigned long strap_ahb_upper;
	const unsigned long strap_ahb_lower;
	void __iomem *aclk_reg;
	void __iomem *jtag_clk_reg;
	const char *name;
	const char *depends;
	const unsigned pas_id;
	int bus_port;
};
#endif
