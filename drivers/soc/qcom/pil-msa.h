/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_PIL_MSA_H
#define __MSM_PIL_MSA_H

#include <soc/qcom/subsystem_restart.h>

#include "peripheral-loader.h"

#define VDD_MSS_UV	1000000

struct modem_data {
	struct q6v5_data *q6;
	struct subsys_device *subsys;
	struct subsys_desc subsys_desc;
	void *ramdump_dev;
	bool crash_shutdown;
	bool ignore_errors;
	struct completion stop_ack;
	void __iomem *rmb_base;
	struct clk *xo;
	struct pil_desc desc;
	struct device mba_mem_dev;
	struct dma_attrs attrs_dma;
	struct device *mba_mem_dev_fixed;
};

extern struct pil_reset_ops pil_msa_mss_ops;
extern struct pil_reset_ops pil_msa_mss_ops_selfauth;
extern struct pil_reset_ops pil_msa_femto_mba_ops;

int pil_mss_reset_load_mba(struct pil_desc *pil);
int pil_mss_make_proxy_votes(struct pil_desc *pil);
void pil_mss_remove_proxy_votes(struct pil_desc *pil);
int pil_mss_shutdown(struct pil_desc *pil);
int pil_mss_deinit_image(struct pil_desc *pil);
#endif
