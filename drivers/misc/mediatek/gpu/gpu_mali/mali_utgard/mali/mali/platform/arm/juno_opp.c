/*
 * This confidential and proprietary software may be used only as
 * authorised by a licensing agreement from ARM Limited
 * (C) COPYRIGHT 2009-2010, 2012-2016 ARM Limited
 * ALL RIGHTS RESERVED
 * The entire notice above must be reproduced on all authorised
 * copies and copies may only be made to the extent permitted
 * by a licensing agreement from ARM Limited.
 */

/**
 * @file juno_opp.c
 * Example: Set up opp table
 * Using ARM64 juno specific SCPI_PROTOCOL get frequence inform
 * Customer need implement your own platform releated logic
 */
#ifdef CONFIG_ARCH_VEXPRESS
#ifdef CONFIG_MALI_DEVFREQ
#ifdef CONFIG_ARM64
#ifdef CONFIG_ARM_SCPI_PROTOCOL
#include <linux/module.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/scpi_protocol.h>
#include <linux/version.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0)
#include <linux/pm_opp.h>
#else /* Linux >= 3.13 */
/* In 3.13 the OPP include header file, types, and functions were all
 * renamed. Use the old filename for the include, and define the new names to
 * the old, when an old kernel is detected.
 */
#include <linux/opp.h>
#define dev_pm_opp_add opp_add
#define dev_pm_opp_remove opp_remove
#endif /* Linux >= 3.13 */

#include "mali_kernel_common.h"

static int init_juno_opps_from_scpi(struct device *dev)
{
	struct scpi_dvfs_info *sinfo;
	struct scpi_ops *sops;

	int i;

	sops = get_scpi_ops();
	if (NULL == sops) {
		MALI_DEBUG_PRINT(2, ("Mali didn't get any scpi ops \n"));
		return -1;
	}

	/* Hard coded for Juno. 2 is GPU domain */
	sinfo = sops->dvfs_get_info(2);
	if (IS_ERR_OR_NULL(sinfo))
		return PTR_ERR(sinfo);

	for (i = 0; i < sinfo->count; i++) {
		struct scpi_opp *e = &sinfo->opps[i];

		MALI_DEBUG_PRINT(2, ("Mali OPP from SCPI: %u Hz @ %u mV\n", e->freq, e->m_volt));

		dev_pm_opp_add(dev, e->freq, e->m_volt * 1000);
	}

	return 0;
}

int setup_opps(void)
{
	struct device_node *np;
	struct platform_device *pdev;
	int err;

	np = of_find_node_by_name(NULL, "gpu");
	if (!np) {
		pr_err("Failed to find DT entry for Mali\n");
		return -EFAULT;
	}

	pdev = of_find_device_by_node(np);
	if (!pdev) {
		pr_err("Failed to find device for Mali\n");
		of_node_put(np);
		return -EFAULT;
	}

	err = init_juno_opps_from_scpi(&pdev->dev);

	of_node_put(np);

	return err;
}

int term_opps(struct device *dev)
{
	struct scpi_dvfs_info *sinfo;
	struct scpi_ops *sops;

	int i;

	sops = get_scpi_ops();
	if (NULL == sops) {
		MALI_DEBUG_PRINT(2, ("Mali didn't get any scpi ops \n"));
		return -1;
	}

	/* Hard coded for Juno. 2 is GPU domain */
	sinfo = sops->dvfs_get_info(2);
	if (IS_ERR_OR_NULL(sinfo))
		return PTR_ERR(sinfo);

	for (i = 0; i < sinfo->count; i++) {
		struct scpi_opp *e = &sinfo->opps[i];

		MALI_DEBUG_PRINT(2, ("Mali Remove OPP: %u Hz \n", e->freq));

		dev_pm_opp_remove(dev, e->freq);
	}

	return 0;

}
#endif
#endif
#endif
#endif
