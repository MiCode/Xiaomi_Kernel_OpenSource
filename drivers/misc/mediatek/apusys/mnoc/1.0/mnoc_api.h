/*
 * Copyright (C) 2019 MediaTek Inc.
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
#ifndef __APUSYS_MNOC_API_H__
#define __APUSYS_MNOC_API_H__

int apu_cmd_qos_start(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core, uint32_t boost_val);
int apu_cmd_qos_suspend(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core);
int apu_cmd_qos_end(uint64_t cmd_id, uint64_t sub_cmd_id,
	int dev_type, int dev_core);
void mnoc_set_mni_pre_ultra(int dev_type, int dev_core, bool endis);
void mnoc_set_lt_guardian_pre_ultra(int dev_type, int dev_core, bool endis);

phys_addr_t get_apu_iommu_tfrp(unsigned int id);

#endif
