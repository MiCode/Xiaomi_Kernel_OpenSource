// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 MediaTek Inc.
 */

/**
 * @file    gpudfd_mt6835.c
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
#include <linux/ioport.h>
#include <linux/delay.h>
#include <linux/io.h>

#include <gpufreq_v2.h>
#include <gpu_misc.h>
#include <gpudfd_mt6835.h>

/**
 * ===============================================
 * Local variables definition
 * ===============================================
 */
static void __iomem *g_mfg_top_base;
static void __iomem *g_drm_debug_base;
static struct gpufreq_platform_fp *gpufreq_fp;
static unsigned int g_dfd_force_dump_mode;
static struct gpudfd_platform_fp platform_fp = {
	.get_dfd_force_dump_mode = __gpudfd_get_dfd_force_dump_mode,
	.set_dfd_force_dump_mode = __gpudfd_set_dfd_force_dump_mode,
	.config_dfd = __gpudfd_config_dfd,
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
		writel(MFG_DFD_CON_12_ENABLE, MFG_DFD_CON_12);
		writel(MFG_DFD_CON_13_ENABLE, MFG_DFD_CON_13);
		writel(MFG_DFD_CON_17_ENABLE, MFG_DFD_CON_17);
		writel(MFG_DFD_CON_18_ENABLE, MFG_DFD_CON_18);
		writel(MFG_DFD_CON_19_ENABLE, MFG_DFD_CON_19);

		GPUFREQ_LOGI("[DFD] before setting MFG_DFD_CON_0 = 0x%08x", readl(MFG_DFD_CON_0));
		GPUFREQ_LOGI("[DFD] before setting DRM_MFG_REG = 0x%08x", readl(DRM_MFG_REG));
		if ((readl(DRM_MFG_REG) & 0x1) != 0x1) {
			writel(0x77000000, DRM_MFG_REG);
			GPUFREQ_LOGI("[DFD] after disable DRM_MFG_REG = 0x%08x",
				readl(DRM_MFG_REG));
			udelay(10);
			writel(0x77000001, DRM_MFG_REG);
		}
		GPUFREQ_LOGI("[DFD] after setting DRM_MFG_REG = 0x%08x", readl(DRM_MFG_REG));
	} else {
		writel(MFG_DFD_CON_0_DISABLE,  MFG_DFD_CON_0);
		writel(MFG_DFD_CON_1_DISABLE,  MFG_DFD_CON_1);
		writel(MFG_DFD_CON_2_DISABLE,  MFG_DFD_CON_2);
		writel(MFG_DFD_CON_3_DISABLE,  MFG_DFD_CON_3);
		writel(MFG_DFD_CON_4_DISABLE,  MFG_DFD_CON_4);
		writel(MFG_DFD_CON_5_DISABLE,  MFG_DFD_CON_5);
		writel(MFG_DFD_CON_6_DISABLE,  MFG_DFD_CON_6);
		writel(MFG_DFD_CON_7_DISABLE,  MFG_DFD_CON_7);
		writel(MFG_DFD_CON_8_DISABLE,  MFG_DFD_CON_8);
		writel(MFG_DFD_CON_9_DISABLE,  MFG_DFD_CON_9);
		writel(MFG_DFD_CON_12_DISABLE, MFG_DFD_CON_12);
		writel(MFG_DFD_CON_13_DISABLE, MFG_DFD_CON_13);
		writel(MFG_DFD_CON_17_DISABLE, MFG_DFD_CON_17);
		writel(MFG_DFD_CON_18_DISABLE, MFG_DFD_CON_18);
		writel(MFG_DFD_CON_19_DISABLE, MFG_DFD_CON_19);

		writel(MFG_DEBUGMON_CON_00_DISABLE, MFG_DEBUGMON_CON_00);
	}
#else
	(void)(enable);
#endif /* GPUDFD_ENABLE */
}

unsigned int gpudfd_init(struct platform_device *pdev)
{
	struct device *gpufreq_dev = &pdev->dev;
	struct resource *res;
	int ret = GPUFREQ_SUCCESS;

	/* 0x13FBF000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "mfg_top_config");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource MFG_TOP_CONFIG");
		goto done;
	}
	g_mfg_top_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_mfg_top_base)) {
		GPUFREQ_LOGE("fail to ioremap MFG_TOP_CONFIG: 0x%llx", res->start);
		goto done;
	}

	/* 0x1000D000 */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "drm_debug");
	if (unlikely(!res)) {
		GPUFREQ_LOGE("fail to get resource DRM_DEBUG");
		goto done;
	}
	g_drm_debug_base = devm_ioremap(gpufreq_dev, res->start, resource_size(res));
	if (unlikely(!g_drm_debug_base)) {
		GPUFREQ_LOGE("fail to ioremap DRM_DEBUG: 0x%llx", res->start);
		goto done;
	}

	/* register gpudfd platform function to misc */
	gpu_misc_register_gpudfd_fp(&platform_fp);

done:
	return ret;
}
