/*
 * arch/arm/mach-tegra/tegra2_speedo.c
 *
 * Copyright (c) 2010, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/bug.h>

#include <mach/iomap.h>

#include "fuse.h"

#define CPU_SPEEDO_LSBIT		20
#define CPU_SPEEDO_MSBIT		29
#define CPU_SPEEDO_REDUND_LSBIT		30
#define CPU_SPEEDO_REDUND_MSBIT		39
#define CPU_SPEEDO_REDUND_OFFS	(CPU_SPEEDO_REDUND_MSBIT - CPU_SPEEDO_MSBIT)

#define CORE_SPEEDO_LSBIT		40
#define CORE_SPEEDO_MSBIT		47
#define CORE_SPEEDO_REDUND_LSBIT	48
#define CORE_SPEEDO_REDUND_MSBIT	55
#define CORE_SPEEDO_REDUND_OFFS	(CORE_SPEEDO_REDUND_MSBIT - CORE_SPEEDO_MSBIT)

#define SPEEDO_MULT			4

#define CHIP_ID				0x804
#define CHIP_MINOR_SHIFT		16
#define CHIP_MINOR_MASK			(0xF <<	CHIP_MINOR_SHIFT)

#define PROCESS_CORNERS_NUM		4

#define SPEEDO_ID_SELECT_0(rev)		((rev) <= 2)
#define SPEEDO_ID_SELECT_1(sku)		\
	(((sku) != 20) && ((sku) != 23) && ((sku) != 24) && \
	 ((sku) != 27) && ((sku) != 28))

/* Maximum speedo levels for each CPU process corner */
static const u32 cpu_process_speedos[][PROCESS_CORNERS_NUM] = {
/* proc_id 0	1    2	       3		  */
	{315, 366, 420, UINT_MAX}, /* speedo_id 0 */
	{303, 368, 419, UINT_MAX}, /* speedo_id 1 */
	{316, 331, 383, UINT_MAX}, /* speedo_id 2 */
};

/* Maximum speedo levels for each core process corner */
static const u32 core_process_speedos[][PROCESS_CORNERS_NUM] = {
/* proc_id 0	1    2	       3		  */
	{165, 195, 224, UINT_MAX}, /* speedo_id 0 */
	{165, 195, 224, UINT_MAX}, /* speedo_id 1 */
	{165, 195, 224, UINT_MAX}, /* speedo_id 2 */
};

static int cpu_process_id;
static int core_process_id;
static int soc_speedo_id;

void tegra_init_speedo_data(void)
{
	u32 reg, val;
	int i, bit, rev;
	int sku = tegra_sku_id;
	void __iomem *apb_misc = IO_ADDRESS(TEGRA_APB_MISC_BASE);

	reg = readl(apb_misc + CHIP_ID);
	rev = (reg & CHIP_MINOR_MASK) >> CHIP_MINOR_SHIFT;
	if (SPEEDO_ID_SELECT_0(rev))
		soc_speedo_id = 0;
	else if (SPEEDO_ID_SELECT_1(sku))
		soc_speedo_id = 1;
	else
		soc_speedo_id = 2;
	BUG_ON(soc_speedo_id >= ARRAY_SIZE(cpu_process_speedos));
	BUG_ON(soc_speedo_id >= ARRAY_SIZE(core_process_speedos));

	val = 0;
	for (bit = CPU_SPEEDO_MSBIT; bit >= CPU_SPEEDO_LSBIT; bit--) {
		reg = tegra_spare_fuse(bit) |
			tegra_spare_fuse(bit + CPU_SPEEDO_REDUND_OFFS);
		val = (val << 1) | (reg & 0x1);
	}
	val = val * SPEEDO_MULT;
	pr_debug("%s CPU speedo level %u\n", __func__, val);

	for (i = 0; i < (PROCESS_CORNERS_NUM - 1); i++) {
		if (val <= cpu_process_speedos[soc_speedo_id][i])
			break;
	}
	cpu_process_id = i;

	val = 0;
	for (bit = CORE_SPEEDO_MSBIT; bit >= CORE_SPEEDO_LSBIT; bit--) {
		reg = tegra_spare_fuse(bit) |
			tegra_spare_fuse(bit + CORE_SPEEDO_REDUND_OFFS);
		val = (val << 1) | (reg & 0x1);
	}
	val = val * SPEEDO_MULT;
	pr_debug("%s Core speedo level %u\n", __func__, val);

	for (i = 0; i < (PROCESS_CORNERS_NUM - 1); i++) {
		if (val <= core_process_speedos[soc_speedo_id][i])
			break;
	}
	core_process_id = i;

	pr_info("Tegra SKU: %d Rev: A%.2d CPU Process: %d Core Process: %d"
		" Speedo ID: %d\n", sku, rev, cpu_process_id, core_process_id,
		soc_speedo_id);
}

int tegra_cpu_process_id(void)
{
	return cpu_process_id;
}

int tegra_core_process_id(void)
{
	return core_process_id;
}

int tegra_soc_speedo_id(void)
{
	return soc_speedo_id;
}
