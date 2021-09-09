// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/dma-mapping.h>

#include <utilities/mdla_debug.h>
#include <utilities/mdla_util.h>

#include "mdla_plat_internal.h"

static u32 nr_core_ids = 1;
static u32 default_polling_cmd_done;
static u32 mdla_ver;

/* SW configuration */
static bool pwr_rdy;
static bool mmu_en;
static bool nn_pmu_en;
static bool sw_preemption_en;
static bool hw_preemption_en;
static bool micro_p_en;
static int prof_ver;

u32 mdla_plat_get_core_num(void)
{
	return nr_core_ids;
}

u32 mdla_plat_get_polling_cmd_time(void)
{
	return default_polling_cmd_done;
}

u32 mdla_plat_get_version(void)
{
	return mdla_ver;
}

bool mdla_plat_pwr_drv_ready(void)
{
	return pwr_rdy;
}

bool mdla_plat_iommu_enable(void)
{
	return mmu_en;
}

bool mdla_plat_nn_pmu_support(void)
{
	return nn_pmu_en;
}

bool mdla_plat_sw_preemption_support(void)
{
	return sw_preemption_en;
}

bool mdla_plat_hw_preemption_support(void)
{
	return hw_preemption_en;
}

bool mdla_plat_micro_p_support(void)
{
	return micro_p_en;
}

int mdla_plat_get_prof_ver(void)
{
	return prof_ver;
}

int mdla_plat_init(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mdla_plat_drv *drv;
	u32 major_ver = 0;

	of_property_read_u32(dev->of_node, "core_num", &nr_core_ids);
	if (nr_core_ids > MAX_CORE_NUM) {
		dev_info(dev, "Invalid core number: %d\n", nr_core_ids);
		nr_core_ids = 1;
	}

	if (of_property_read_u32(pdev->dev.of_node, "version", &mdla_ver) == 0) {
		dev_info(&pdev->dev, "ver = %x\n", mdla_ver);
		mdla_dbg_set_version(mdla_ver);
	}

	drv = (struct mdla_plat_drv *)of_device_get_match_data(dev);

	if (!drv)
		return -1;

	pwr_rdy          = !(drv->sw_cfg & BIT(CFG_DUMMY_PWR));
	mmu_en           = !(drv->sw_cfg & BIT(CFG_DUMMY_MMU));
	nn_pmu_en        = !!(drv->sw_cfg & BIT(CFG_NN_PMU_SUPPORT));
	sw_preemption_en = !!(drv->sw_cfg & BIT(CFG_SW_PREEMPTION_SUPPORT));
	hw_preemption_en = !!(drv->sw_cfg & BIT(CFG_HW_PREEMPTION_SUPPORT));
	micro_p_en       = !!(drv->sw_cfg & BIT(CFG_MICRO_P_SUPPORT));

	if (sw_preemption_en && hw_preemption_en)
		sw_preemption_en = false;

	if (micro_p_en) {
		if (dma_set_mask_and_coherent(dev, DMA_BIT_MASK(34)))
			dev_info(dev, "MDLA: set DMA mask failed\n");
	}

	mdla_dbg_write_u64(FS_CFG_PMU_PERIOD, drv->pmu_period_us);
	mdla_dbg_write_u32(FS_KLOG, drv->klog);
	mdla_dbg_write_u32(FS_POWEROFF_TIME, drv->off_delay_ms);
	mdla_dbg_write_u32(FS_POLLING_CMD_DONE, drv->polling_cmd_ms);
	mdla_dbg_write_u32(FS_TIMEOUT, drv->timeout_ms);

	major_ver = mdla_util_get_ip_version();
	if (!micro_p_en && ((major_ver == 1) || (major_ver == 2))) {
		mdla_dbg_write_u32(FS_CFG_ENG0, 0x0);
		mdla_dbg_write_u32(FS_CFG_ENG1, 0x0);
		mdla_dbg_write_u32(FS_CFG_ENG2, 0x0);
		mdla_dbg_write_u32(FS_CFG_ENG11, 0x600);
	}

	prof_ver = drv->profile_ver;

	dev_info(dev, "core number = %d, sw_cfg = 0x%x\n",
			nr_core_ids, drv->sw_cfg);

	return drv->init(pdev);
}

void mdla_plat_deinit(struct platform_device *pdev)
{
	struct mdla_plat_drv *drv;

	drv = (struct mdla_plat_drv *)of_device_get_match_data(&pdev->dev);

	if (drv)
		drv->deinit(pdev);
}

