/*
 * Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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

#include "peripheral-loader.h"

struct regulator;
struct clk;
struct pil_device;
struct platform_device;

struct q6v5_data {
	void __iomem *reg_base;
	void __iomem *rmb_base;
	void __iomem *cxrail_bhs;  /* External BHS register */
	struct clk *xo;		   /* XO clock source */
	struct clk *pnoc_clk;	   /* PNOC bus clock source */
	struct clk *ahb_clk;	   /* PIL access to registers */
	struct clk *axi_clk;	   /* CPU access to memory */
	struct clk *core_clk;	   /* CPU core */
	struct clk *reg_clk;	   /* CPU access registers */
	struct clk *gpll0_mss_clk; /* GPLL0 to MSS connection */
	struct clk *rom_clk;	   /* Boot ROM */
	struct clk *snoc_axi_clk;
	struct clk *mnoc_axi_clk;
	struct clk *qdss_clk;
	struct clk *prng_clk;
	struct clk *axis2_clk;
	void __iomem *axi_halt_base; /* Halt base of q6, mss,
				      * nc are in same 4K page
				      */
	void __iomem *axi_halt_q6;
	void __iomem *axi_halt_mss;
	void __iomem *axi_halt_nc;
	void __iomem *restart_reg;
	void __iomem *pdc_sync;
	void __iomem *alt_reset;
	struct regulator *vreg;
	struct regulator *vreg_cx;
	struct regulator *vreg_mx;
	struct regulator *vreg_pll;
	bool is_booted;
	struct pil_desc desc;
	bool self_auth;
	phys_addr_t mba_dp_phys;
	void *mba_dp_virt;
	size_t mba_dp_size;
	size_t dp_size;
	bool qdsp6v55;
	bool qdsp6v5_2_0;
	bool qdsp6v56;
	bool qdsp6v56_1_3;
	bool qdsp6v56_1_5;
	bool qdsp6v56_1_8;
	bool qdsp6v56_1_8_inrush_current;
	bool qdsp6v56_1_10;
	bool qdsp6v61_1_1;
	bool qdsp6v62_1_2;
	bool qdsp6v62_1_4;
	bool qdsp6v62_1_5;
	bool qdsp6v65_1_0;
	bool non_elf_image;
	bool restart_reg_sec;
	bool override_acc;
	int override_acc_1;
	int mss_pdc_offset;
	bool ahb_clk_vote;
	bool mx_spike_wa;
};

int pil_q6v5_make_proxy_votes(struct pil_desc *pil);
void pil_q6v5_remove_proxy_votes(struct pil_desc *pil);
void pil_q6v5_halt_axi_port(struct pil_desc *pil, void __iomem *halt_base);
void pil_q6v5_shutdown(struct pil_desc *pil);
int pil_q6v5_reset(struct pil_desc *pil);
void assert_clamps(struct pil_desc *pil);
struct q6v5_data *pil_q6v5_init(struct platform_device *pdev);

#endif
