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
#include <linux/bug.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <mtk_mcdi_governor.h>
#include <mtk_mcdi_plat.h>
#include <mtk_mcdi_reg.h>
#include <mtk_mcdi_mcupm.h>
#include <mtk_mcdi_api.h>
#include <mtk_mcdi_util.h>

static const char mcdi_node_name[] = "mediatek,mt6739-mcdi";

#ifdef MCDI_MCUPM_INTF
unsigned int MCUPM_CFGREG_MP_SLEEP_TH[NF_CLUSTER] = {
	MCUPM_CFGREG_MP0_SLEEP_TH
};

unsigned int MCUPM_CFGREG_MP_CPU0_RES[NF_CLUSTER] = {
	MCUPM_CFGREG_MP0_CPU0_RES
};
#endif

static int cluster_idx_map[NF_CPU] = {
	0,
	0,
	0,
	0
};

static int cpu_type_idx_map[NF_CPU] = {
	CPU_TYPE_LL,
	CPU_TYPE_LL,
	CPU_TYPE_LL,
	CPU_TYPE_LL
};

static unsigned int cpu_cluster_pwr_stat_map[NF_PWR_STAT_MAP_TYPE][NF_CPU] = {
	[ALL_CPU_IN_CLUSTER] = {
		0x000F		/* Cluster 0 */
	},
	[CPU_CLUSTER] = {
		0x2000E,	/* Only CPU 0 */
		0x2000D,	/* Only CPU 1 */
		0x2000B,
		0x20007
	},
	[CPU_IN_OTHER_CLUSTER] = {
		0x00000
	}
};

unsigned int get_pwr_stat_check_map(int type, int idx)
{
	if (!(type >= 0 && type < NF_PWR_STAT_MAP_TYPE))
		return 0;

	if (cpu_is_invalid(idx))
		return 0;

	return cpu_cluster_pwr_stat_map[type][idx];
}

int cluster_idx_get(int cpu)
{
	if (cpu_is_invalid(cpu)) {
		WARN_ON(cpu_is_invalid(cpu));

		return 0;
	}

	return cluster_idx_map[cpu];
}

int cpu_type_idx_get(int cpu)
{
	if (unlikely(cpu_is_invalid(cpu)))
		return 0;

	return cpu_type_idx_map[cpu];
}

void mcdi_status_init(void)
{
	if (mcdi_fw_is_ready())
		set_mcdi_enable_status(true);
	else
		set_mcdi_enable_status(false);
}

void mcdi_of_init(void **base)
{
	struct device_node *node = NULL;

	if (base == NULL)
		return;

	/* MCDI sysram base */
	node = of_find_compatible_node(NULL, NULL, mcdi_node_name);

	if (!node)
		pr_info("node '%s' not found!\n", mcdi_node_name);

	*base = of_iomap(node, 0);

	if (!*base)
		pr_info("mcdi sysram can not iomap!\n");

	mcupm_sram_is_ready = false;

	mcdi_mcupm_base = of_iomap(node, 1);

	if (!mcdi_mcupm_base)
		pr_info("mcdi mcupm base can not iomap!\n");

	mcdi_mcupm_sram_base = of_iomap(node, 2);

	if (!mcdi_mcupm_sram_base)
		pr_info("mcdi mcupm sram base can not iomap!\n");

	pr_info("mcdi_mcupm_sram = %p\n", mcdi_mcupm_sram_base);

	mcupm_sram_is_ready = true;
}
