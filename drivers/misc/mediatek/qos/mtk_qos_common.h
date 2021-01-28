/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_QOS_COMMON_H__
#define __MTK_QOS_COMMON_H__

#include <sspm_ipi.h>
#include <sspm_ipi_pin.h>

struct mtk_qos;

struct mtk_qos_soc {
	const struct qos_ipi_cmd *ipi_pin;
	const struct qos_sram_addr *sram_pin;
	int (*qos_sspm_init)(void);
	int (*qos_ipi_recv_handler)(void *arg);
};

struct mtk_qos {
	struct device *dev;
	const struct mtk_qos_soc *soc;
	int dram_type;
	void __iomem *regs;
	unsigned int regsize;
};

extern int mtk_qos_probe(struct platform_device *pdev,
			const struct mtk_qos_soc *soc);
#endif

