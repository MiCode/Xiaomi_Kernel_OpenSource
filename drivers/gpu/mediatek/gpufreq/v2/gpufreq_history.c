// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    gpufreq_history.c
 * @brief   History log for GPU-DVFS
 */

/**
 * ===============================================
 * Include
 * ===============================================
 */
#include <linux/random.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/of.h>
#include <linux/of_address.h>

#include <gpufreq_v2.h>
#include <gpufreq_history.h>

/**
 * ===============================================
 * Local Variable Definition
 * ===============================================
 */
static void __iomem *g_history_base;
static unsigned int g_history_log[GPUFREQ_HISTORY_SIZE];

/**
 * ===============================================
 * Function Definition
 * ===============================================
 */
static int gpufreq_history_get_reg_idx(struct device_node *node, const char *reg_name)
{
	int idx = -1;

	idx = of_property_match_string(node, "reg-names", reg_name);
	if (idx < 0)
		GPUFREQ_LOGE("fail to find gpufreq reg-names: %s", reg_name);

	return idx;
}

static void __iomem *gpufreq_history_of_ioremap(const char *node_name, const char *reg_name)
{
	struct device_node *node = NULL;
	void __iomem *base = NULL;
	int reg_idx = -1;

	node = of_find_compatible_node(NULL, NULL, node_name);
	if (node)
		reg_idx = gpufreq_history_get_reg_idx(node, reg_name);
	else
		return NULL;

	if (reg_idx >= 0)
		base = of_iomap(node, reg_idx);
	else
		base = NULL;

	return base;
}

static unsigned int gpufreq_history_bit_reverse(unsigned int x)
{
	x = ((x >> 8) & 0x00FF00FF) | ((x << 8) & 0xFF00FF00);
	x = ((x >> 16) & 0x0000FFFF) | ((x << 16) & 0xFFFF0000);

	return x;
}

const unsigned int *gpufreq_history_get_log(void)
{
	int i = 0;
	u32 val = 0;

	for (i = 0; i < GPUFREQ_HISTORY_SIZE; i++) {
		val = readl(g_history_base + (i << 2));
		g_history_log[i] = gpufreq_history_bit_reverse(val);
	}

	return g_history_log;
}

int gpufreq_history_init(void)
{
	g_history_base = gpufreq_history_of_ioremap("mediatek,gpufreq", "sysram_mfg_history");
	if (unlikely(!g_history_base)) {
		GPUFREQ_LOGE("fail to ioremap sysram_mfg_history");
		return GPUFREQ_ENOMEM;
	}

	GPUFREQ_LOGI("MFGSYS history base: 0x%08x", g_history_base);

	return GPUFREQ_SUCCESS;
}
