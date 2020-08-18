// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/of_device.h>

#include "mnoc_plat_internal.h"


static struct mnoc_plat_drv mt6853_drv = {
	.init             = mnoc_hw_v1_52_init,
	.exit             = mnoc_hw_v1_52_exit,
	.dev_2_core_id	  = apusys_dev_to_core_id_v1_52,
	.int_endis	  = mnoc_int_endis_v1_52,
	.print_int_sta	  = print_int_sta_v1_52,
	.chk_int_status   = mnoc_check_int_status_v1_52,
	.hw_reinit	  = mnoc_hw_reinit_v1_52,
	.get_pmu_counter  = mnoc_get_pmu_counter_v1_52,
	.clr_pmu_counter  = mnoc_clear_pmu_counter_v1_52,
	.pmu_reg_in_range = mnoc_pmu_reg_in_range_v1_52,

	.get_apu_iommu_tfrp        = get_apu_iommu_tfrp_v1_52,
	.set_mni_pre_ultra         = mnoc_set_mni_pre_ultra_v1_52,
	.set_lt_guardian_pre_ultra = mnoc_set_lt_guardian_pre_ultra_v1_52,

	.apu_qos_engine_count = 3,

//	.met_pmu_reg_init = mnoc_met_pmu_reg_init_v1_52,
//	.met_pmu_reg_uninit = mnoc_met_pmu_reg_uninit_v1_52,
};


static struct mnoc_plat_drv mt6873_drv = {
	.init             = mnoc_hw_v1_51_init,
	.exit             = mnoc_hw_v1_51_exit,
	.dev_2_core_id    = apusys_dev_to_core_id_v1_51,
	.int_endis        = mnoc_int_endis_v1_51,
	.print_int_sta    = print_int_sta_v1_51,
	.chk_int_status   = mnoc_check_int_status_v1_51,
	.hw_reinit        = mnoc_hw_reinit_v1_51,
	.get_pmu_counter  = mnoc_get_pmu_counter_v1_51,
	.clr_pmu_counter  = mnoc_clear_pmu_counter_v1_51,
	.pmu_reg_in_range = mnoc_pmu_reg_in_range_v1_51,

	.get_apu_iommu_tfrp        = get_apu_iommu_tfrp_v1_51,
	.set_mni_pre_ultra         = mnoc_set_mni_pre_ultra_v1_51,
	.set_lt_guardian_pre_ultra = mnoc_set_lt_guardian_pre_ultra_v1_51,

	.apu_qos_engine_count = 5,
//	.met_pmu_reg_init = NULL,
//	.met_pmu_reg_uninit = NULL,
};

static struct mnoc_plat_drv mt6885_drv = {
	.init             = mnoc_hw_v1_50_init,
	.exit             = mnoc_hw_v1_50_exit,
	.dev_2_core_id    = apusys_dev_to_core_id_v1_50,
	.int_endis        = mnoc_int_endis_v1_50,
	.print_int_sta    = print_int_sta_v1_50,
	.chk_int_status   = mnoc_check_int_status_v1_50,
	.hw_reinit        = mnoc_hw_reinit_v1_50,
	.get_pmu_counter  = mnoc_get_pmu_counter_v1_50,
	.clr_pmu_counter  = mnoc_clear_pmu_counter_v1_50,
	.pmu_reg_in_range = mnoc_pmu_reg_in_range_v1_50,

	.get_apu_iommu_tfrp        = get_apu_iommu_tfrp_v1_50,
	.set_mni_pre_ultra         = mnoc_set_mni_pre_ultra_v1_50,
	.set_lt_guardian_pre_ultra = mnoc_set_lt_guardian_pre_ultra_v1_50,

	.apu_qos_engine_count = 8,
//	.met_pmu_reg_init = NULL,
//	.met_pmu_reg_uninit = NULL,
};

static const struct of_device_id mnoc_of_match[] = {
	{ .compatible = "mediatek,mt6853-mnoc", .data = &mt6853_drv},
	{ .compatible = "mediatek,mt6873-mnoc", .data = &mt6873_drv},
	{ .compatible = "mediatek,mt6885-mnoc", .data = &mt6885_drv},
	{ /* end of list */},
};
MODULE_DEVICE_TABLE(of, mnoc_of_match);

const struct of_device_id *mnoc_plat_get_device(void)
{
	return mnoc_of_match;
}
