/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MTK_QOS_COMMON_H__
#define __MTK_QOS_COMMON_H__


struct mtk_qos;
struct platform_device;

struct mtk_qos_soc {
	const struct qos_ipi_cmd *ipi_pin;
	const struct qos_sram_addr *sram_pin;
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
extern void qos_ipi_init(struct mtk_qos *qos);
extern void qos_ipi_recv_init(struct mtk_qos *qos);
extern int qos_get_ipi_cmd(int idx);
extern unsigned int is_mtk_qos_enable(void);
#endif

