/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#ifndef __MSM_PIL_Q6V5_H
#define __MSM_PIL_Q6V5_H

struct regulator;
struct clk;
struct pil_device;
struct pil_desc;
struct platform_device;

struct q6v5_data {
	void __iomem *reg_base;
	struct clk *xo;		/* XO clock source */
	struct clk *ahb_clk;	/* PIL access to registers */
	struct clk *axi_clk;	/* CPU access to memory */
	struct clk *core_clk;	/* CPU core */
	struct clk *reg_clk;	/* CPU access registers */
	struct clk *rom_clk;	/* Boot ROM */
	void __iomem *axi_halt_base;
	void __iomem *rmb_base;
	void __iomem *restart_reg;
	void __iomem *io_clamp_reg;
	unsigned long start_addr;
	struct regulator *vreg;
	bool is_booted;
	int self_auth;
	struct pil_device *pil;
};

int pil_q6v5_make_proxy_votes(struct pil_desc *pil);
void pil_q6v5_remove_proxy_votes(struct pil_desc *pil);
void pil_q6v5_halt_axi_port(struct pil_desc *pil, void __iomem *halt_base);
int pil_q6v5_init_image(struct pil_desc *pil, const u8 *metadata,
			size_t size);
void pil_q6v5_shutdown(struct pil_desc *pil);
int pil_q6v5_reset(struct pil_desc *pil);
struct pil_desc *pil_q6v5_init(struct platform_device *pdev);

#endif
