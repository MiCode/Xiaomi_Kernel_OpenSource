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

#include "peripheral-loader.h"

struct pil_q6v4_pdata {
	const unsigned long strap_tcm_base;
	const unsigned long strap_ahb_upper;
	const unsigned long strap_ahb_lower;
	void __iomem *aclk_reg;
	void __iomem *jtag_clk_reg;
	const char *name;
	const unsigned pas_id;
	int bus_port;
};

struct clk;
struct pil_device;
struct regulator;

/**
 * struct q6v4_data - Q6 processor
 */
struct q6v4_data {
	void __iomem *base;
	void __iomem *wdog_base;
	unsigned long strap_tcm_base;
	unsigned long strap_ahb_upper;
	unsigned long strap_ahb_lower;
	void __iomem *aclk_reg;
	void __iomem *jtag_clk_reg;
	unsigned pas_id;
	int bus_port;
	int wdog_irq;

	struct regulator *vreg;
	struct regulator *pll_supply;
	bool vreg_enabled;
	struct clk *xo;

	struct pil_desc desc;
};

#define pil_to_q6v4_data(p) container_of(p, struct q6v4_data, desc)

extern int pil_q6v4_make_proxy_votes(struct pil_desc *pil);
extern void pil_q6v4_remove_proxy_votes(struct pil_desc *pil);
extern int pil_q6v4_power_up(struct q6v4_data *drv);
extern void pil_q6v4_power_down(struct q6v4_data *drv);
extern int pil_q6v4_boot(struct pil_desc *pil);
extern int pil_q6v4_shutdown(struct pil_desc *pil);

extern int pil_q6v4_init_image_trusted(struct pil_desc *pil,
		const u8 *metadata, size_t size);
extern int pil_q6v4_boot_trusted(struct pil_desc *pil);
extern int pil_q6v4_shutdown_trusted(struct pil_desc *pil);
extern void __devinit
pil_q6v4_init(struct q6v4_data *drv, const struct pil_q6v4_pdata *p);

#endif
