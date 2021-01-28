// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#include <linux/io.h>

#include "mtk_qos_sram.h"

static void __iomem *qos_sram_base;

u32 qos_sram_read(u32 offset)
{
	if (!qos_sram_base || offset >= QOS_SRAM_MAX_SIZE)
		return 0;

	return readl(qos_sram_base + offset);
}

void qos_sram_write(u32 offset, u32 val)
{
	if (!qos_sram_base || offset >= QOS_SRAM_MAX_SIZE)
		return;

	writel(val, qos_sram_base + offset);
}

void qos_sram_init(void __iomem *regs)
{
	int i;

	qos_sram_base = regs;
	for (i = 0; i < QOS_SRAM_MAX_SIZE; i += 4)
		qos_sram_write(i, 0x0);
}

