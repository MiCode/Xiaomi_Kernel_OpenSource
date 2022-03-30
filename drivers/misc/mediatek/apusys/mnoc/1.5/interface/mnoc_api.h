/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_MNOC_API_H__
#define __APUSYS_MNOC_API_H__

#if IS_ENABLED(CONFIG_MTK_QOS_FRAMEWORK)
int apu_cmd_qos_start(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core, uint32_t boost_val);
int apu_cmd_qos_suspend(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core);
int apu_cmd_qos_end(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core);
#else
static inline int apu_cmd_qos_start(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core, uint32_t boost_val)
{
	return 0;
}
static inline int apu_cmd_qos_suspend(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core)
{
	return 0;
}
static inline int apu_cmd_qos_end(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core)
{
	return 0;
}
#endif

void mnoc_set_mni_pre_ultra(int dev_type, int dev_core, bool endis);
void mnoc_set_lt_guardian_pre_ultra(int dev_type, int dev_core, bool endis);

phys_addr_t get_apu_iommu_tfrp(unsigned int id);

#endif
