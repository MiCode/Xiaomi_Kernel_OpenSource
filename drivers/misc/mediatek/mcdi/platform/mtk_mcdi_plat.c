// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
 */
#include <linux/bug.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <mtk_mcdi_governor.h>
#include <mtk_mcdi_plat.h>

static const char mcdi_node_name[] = "mediatek,mt6761-mcdi";

static unsigned int cpu_cluster_pwr_stat_map[NF_PWR_STAT_MAP_TYPE][NF_CPU] = {
	[ALL_CPU_IN_CLUSTER] = {
		0x000F,     /* Cluster 0 */
		0x0000,     /* Cluster 1 */
		0x0000,     /* N/A */
		0x0000
	},
	[CPU_CLUSTER] = {
		0x200FE,     /* Only CPU 0 */
		0x200FD,     /* Only CPU 1 */
		0x200FB,
		0x200F7
	},
	[CPU_IN_OTHER_CLUSTER] = {
		0x00000,
		0x00000,
		0x00000,
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
	return 0;
}

int cpu_type_idx_get(int cpu)
{
	return CPU_TYPE_L;
}

void mcdi_status_init(void)
{
	set_mcdi_enable_status(true);
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

	if (*base)
		pr_info("node '%s' can not iomap!\n", mcdi_node_name);

	pr_info("mcdi_sysram_base = %p\n", *base);
}
