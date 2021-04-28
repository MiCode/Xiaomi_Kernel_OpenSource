/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
EXPORT_SYMBOL(qos_sram_read);

void qos_sram_write(u32 offset, u32 val)
{
	if (!qos_sram_base || offset >= QOS_SRAM_MAX_SIZE)
		return;

	writel(val, qos_sram_base + offset);
}
EXPORT_SYMBOL(qos_sram_write);

void qos_sram_init(void __iomem *regs)
{
	int i;

	qos_sram_base = regs;
	for (i = 0; i < QOS_SRAM_MAX_SIZE; i += 4)
		qos_sram_write(i, 0x0);
}

