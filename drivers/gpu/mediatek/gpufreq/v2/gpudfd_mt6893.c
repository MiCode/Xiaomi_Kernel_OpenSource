// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

/**
 * @file    mtk_gpu_dfd.c
 * @brief   GPU DFD
 */

/**
 * ===============================================
 * SECTION : Include files
 * ===============================================
 */

#include <linux/seq_file.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <gpufreq_v2.h>
#include <gpu_misc.h>
#include <gpudfd_mt6893.h>
#include <gpufreq_debug.h>

/**
 * ===============================================
 * Local variables definition
 * ===============================================
 */
static void __iomem *g_mfg_top_base;
static struct gpufreq_platform_fp *gpufreq_fp;
static unsigned int g_dfd_force_dump_mode;
static struct gpudfd_platform_fp platform_fp = {
	.get_dfd_force_dump_mode = __gpudfd_get_dfd_force_dump_mode,
	.set_dfd_force_dump_mode = __gpudfd_set_dfd_force_dump_mode,
};

void __gpudfd_set_dfd_force_dump_mode(unsigned int mode)
{
	/*
	 * mode = 0, disable force dump
	 * mode = 1, enable force dump
	 */
	g_dfd_force_dump_mode = mode;
}

unsigned int __gpudfd_get_dfd_force_dump_mode(void)
{
	return g_dfd_force_dump_mode;
}

void __gpudfd_config_dfd(unsigned int enable)
{
#if GPUDFD_ENABLE
	if (enable) {
		if (__gpudfd_get_dfd_force_dump_mode() == 1)
			writel(MFG_DEBUGMON_CON_00_ENABLE, MFG_DEBUGMON_CON_00);

		writel(MFG_DFD_CON_0_ENABLE, MFG_DFD_CON_0);
		writel(MFG_DFD_CON_1_ENABLE, MFG_DFD_CON_1);
		writel(MFG_DFD_CON_2_ENABLE, MFG_DFD_CON_2);
		writel(MFG_DFD_CON_3_ENABLE, MFG_DFD_CON_3);
		writel(MFG_DFD_CON_4_ENABLE, MFG_DFD_CON_4);
		writel(MFG_DFD_CON_5_ENABLE, MFG_DFD_CON_5);
		writel(MFG_DFD_CON_6_ENABLE, MFG_DFD_CON_6);
		writel(MFG_DFD_CON_7_ENABLE, MFG_DFD_CON_7);
		writel(MFG_DFD_CON_8_ENABLE, MFG_DFD_CON_8);
		writel(MFG_DFD_CON_9_ENABLE, MFG_DFD_CON_9);
		writel(MFG_DFD_CON_10_ENABLE, MFG_DFD_CON_10);
		writel(MFG_DFD_CON_11_ENABLE, MFG_DFD_CON_11);
	} else {
		writel(MFG_DFD_CON_0_DISABLE, MFG_DFD_CON_0);
		writel(MFG_DFD_CON_1_DISABLE, MFG_DFD_CON_1);
		writel(MFG_DFD_CON_2_DISABLE, MFG_DFD_CON_2);
		writel(MFG_DFD_CON_3_DISABLE, MFG_DFD_CON_3);
		writel(MFG_DFD_CON_4_DISABLE, MFG_DFD_CON_4);
		writel(MFG_DFD_CON_5_DISABLE, MFG_DFD_CON_5);
		writel(MFG_DFD_CON_6_DISABLE, MFG_DFD_CON_6);
		writel(MFG_DFD_CON_7_DISABLE, MFG_DFD_CON_7);
		writel(MFG_DFD_CON_8_DISABLE, MFG_DFD_CON_8);
		writel(MFG_DFD_CON_9_DISABLE, MFG_DFD_CON_9);
		writel(MFG_DFD_CON_10_DISABLE, MFG_DFD_CON_10);
		writel(MFG_DFD_CON_11_DISABLE, MFG_DFD_CON_11);

		writel(MFG_DEBUGMON_CON_00_DISABLE, MFG_DEBUGMON_CON_00);
	}
#else
	(void)(enable);
#endif /* GPUDFD_ENABLE */
}

void gpudfd_register_gpufreq_fp(struct gpufreq_platform_fp *platform_fp)
{
	if (!platform_fp) {
		GPUFREQ_LOGE("null gpufreq platform function pointer (EINVAL)");
		return;
	}

	gpufreq_fp = platform_fp;
}

void __iomem *__gpudfd_of_ioremap(const char *node_name, int idx)
{
	struct device_node *node;
	void __iomem *base;

	node = of_find_compatible_node(NULL, NULL, node_name);

	if (node)
		base = of_iomap(node, idx);
	else
		base = NULL;

	return base;
}

unsigned int gpudfd_init(struct platform_device *pdev)
{
	struct device_node *of_gpufreq = pdev->dev.of_node;
	int ret = GPUFREQ_SUCCESS;

	/* 0x13FBF000 */
	g_mfg_top_base = __gpudfd_of_ioremap("mediatek,g3d_config", 0);
	if (!g_mfg_top_base) {
		GPUFREQ_LOGE("fail to ioremap g3d_config (ENOENT)");
		ret = GPUFREQ_ENOENT;
		goto done;
	}

	/* register gpudfd platform function to misc */
	gpu_misc_register_gpudfd_fp(&platform_fp);

done:
	return ret;
}
