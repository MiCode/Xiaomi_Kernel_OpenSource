/*
 * Copyright (C) 2017 MediaTek Inc.
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
#include <linux/of.h>
#include <linux/of_address.h>

#include <mtk_idle_mcdi.h>

#include <mtk_mcdi.h>
#include <mtk_mcdi_state.h>
#include <mtk_mcdi_governor.h>
#include <mtk_mcdi_mbox.h>

unsigned int cpu_cluster_pwr_stat_map[NF_PWR_STAT_MAP_TYPE][NF_CPU] = {
	[ALL_CPU_IN_CLUSTER] = {
		0x000F,		/* Cluster 0 */
#if 0
		0x00F0,		/* Cluster 1 */
		0x0000,		/* N/A */
		0x0000,
		0x0000,
		0x0000,
		0x0000,
		0x0000
#endif
	},
	[CPU_CLUSTER] = {
		0x200FE,     /* Only CPU 0 on, all the other cores and cluster off */
		0x200FD,     /* Only CPU 1 */
		0x200FB,
		0x200F7,
#if 0
		0x100EF,
		0x100DF,
		0x100BF,
		0x1007F      /* Only CPU 7 */
#endif
	},
	[CPU_IN_OTHER_CLUSTER] = {
		0x000F0, /* for cpu 0, cluster 1 all cores off*/
		0x000F0,
		0x000F0,
		0x000F0,
#if 0
		0x0000F,
		0x0000F,
		0x0000F,
		0x0000F,
#endif
	},
	/* dummy definition, since MT6739 contains only 1 cluster */
	[OTHER_CLUSTER_IDX] = {
		0,
		0,
		0,
		0,
	}
};

static int mcdi_idle_state_mapping[NR_TYPES] = {
	MCDI_STATE_DPIDLE,		/* IDLE_TYPE_DP */
	MCDI_STATE_SODI3,		/* IDLE_TYPE_SO3 */
	MCDI_STATE_SODI,		/* IDLE_TYPE_SO */
	MCDI_STATE_CLUSTER_OFF	/* IDLE_TYPE_RG */
};

static const char mcdi_node_name[] = "mediatek,mt6739-mcdi";

int mcdi_get_mcdi_idle_state(int idx)
{
	return mcdi_idle_state_mapping[idx];
}

/* can't control buck */
unsigned int mcdi_get_buck_ctrl_mask(void)
{
	return 0;
}

void mcdi_status_init(void)
{
	if (mcdi_fw_is_ready())
		set_mcdi_enable_status(true);
	else
		set_mcdi_enable_status(false);
}

void mcdi_of_init(void)
{
	struct device_node *node = NULL;

	/* MCDI sysram base */
	node = of_find_compatible_node(NULL, NULL, mcdi_node_name);

	if (!node)
		pr_info("node '%s' not found!\n", mcdi_node_name);

	mcdi_sysram_base = of_iomap(node, 0);

	mcdi_mcupm_base = of_iomap(node, 1);

	mcdi_mcupm_sram_base = of_iomap(node, 2);
}
