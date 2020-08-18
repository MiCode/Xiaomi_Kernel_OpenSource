// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_address.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpuidle.h>
#include <linux/pm_qos.h>

#include "mtk_cpuidle_status.h"
#include "mtk_cpuidle_cpc.h"
#include "mtk_idle_procfs.h"

static void __iomem *cpupm_mcusys_base_addr;
static void __iomem *cpupm_syssram_base_addr;
static struct pm_qos_request cpuidle_dbg_qos_req;

int mtk_cpupm_mcusys_write(int ofs, unsigned int val)
{
	if (!cpupm_mcusys_base_addr)
		return -EADDRNOTAVAIL;

	if ((ofs % 4) != 0)
		return -EINVAL;

	__raw_writel(val, cpupm_mcusys_base_addr + ofs);
	mb(); /* make sure register access in order */

	return 0;
}

unsigned int mtk_cpupm_mcusys_read(int ofs)
{
	if (!cpupm_mcusys_base_addr)
		return 0;

	if ((ofs % 4) != 0)
		return 0;

	return __raw_readl(cpupm_mcusys_base_addr + ofs);
}

int mtk_cpupm_syssram_write(int ofs, unsigned int val)
{
	if (!cpupm_syssram_base_addr)
		return -EADDRNOTAVAIL;

	if ((ofs % 4) != 0)
		return -EINVAL;

	__raw_writel(val, cpupm_syssram_base_addr + ofs);
	mb(); /* make sure register access in order */

	return 0;
}

unsigned int mtk_cpupm_syssram_read(int ofs)
{
	if (!cpupm_syssram_base_addr)
		return 0;

	if ((ofs % 4) != 0)
		return 0;

	return __raw_readl(cpupm_syssram_base_addr + ofs);
}

void mtk_cpupm_block(void)
{
	pm_qos_update_request(&cpuidle_dbg_qos_req, 2);
}

void mtk_cpupm_allow(void)
{
	pm_qos_update_request(&cpuidle_dbg_qos_req, PM_QOS_DEFAULT_VALUE);
}

int mtk_cpupm_get_idle_state_count(int cpu)
{
	struct device_node *state_node, *cpu_node;
	int i, state_count, start_idx = 1;

	cpu_node = of_cpu_device_node_get(cpu);

	if (!cpu_node)
		return start_idx;

	for (i = 0; ; i++) {
		state_node = of_parse_phandle(cpu_node, "cpu-idle-states", i);
		if (!state_node)
			break;
		of_node_put(state_node);
	}
	of_node_put(cpu_node);

	state_count = i + start_idx;

	if (state_count > CPUIDLE_STATE_MAX)
		state_count = CPUIDLE_STATE_MAX;

	return state_count;
}

static void __init mtk_cpupm_node_init(void)
{
	struct device_node *node = NULL;

	cpupm_mcusys_base_addr = NULL;
	cpupm_syssram_base_addr = NULL;

	node = of_find_compatible_node(NULL, NULL,
						"mediatek,mcusys-ctrl");
	if (node) {
		cpupm_mcusys_base_addr = of_iomap(node, 0);
		of_node_put(node);
	}

	node = of_find_compatible_node(NULL, NULL,
						"mediatek,cpupm-sysram");
	if (node) {
		cpupm_syssram_base_addr = of_iomap(node, 0);
		of_node_put(node);
	}
}

static int __init mtk_cpupm_dbg_init(void)
{
	pm_qos_add_request(&cpuidle_dbg_qos_req,
		PM_QOS_CPU_DMA_LATENCY, PM_QOS_DEFAULT_VALUE);

	mtk_cpupm_node_init();

	mtk_cpuidle_status_init();
	mtk_cpc_init();
	mtk_idle_procfs_init();
	cpc_time_sync();

	return 0;
}

static void __exit mtk_cpupm_dbg_exit(void)
{
	mtk_idle_procfs_exit();

	mtk_cpuidle_status_exit();
	mtk_cpc_exit();

	pm_qos_remove_request(&cpuidle_dbg_qos_req);
}

module_init(mtk_cpupm_dbg_init);
module_exit(mtk_cpupm_dbg_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek CPU Idle Debug FileSystem");
MODULE_AUTHOR("MediaTek Inc.");

