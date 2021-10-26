/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _VPU_PLATFORM_CONFIG_H_
#define _VPU_PLATFORM_CONFIG_H_

void vpu_opp_check(struct vpu_core *vpu_core, uint8_t vcore_index,
			uint8_t freq_index);

void vpu_init_opp(struct vpu_device *vpu_device);
void vpu_uninit_opp(struct vpu_device *vpu_device);

void vpu_get_power_set_opp(struct vpu_core *vpu_core);
int vpu_set_power_set_opp(struct vpu_core *vpu_core, struct vpu_power *power);

void vpu_set_opp_check(struct vpu_core *vpu_core, struct vpu_request *req);

void vpu_set_opp_all_index(struct vpu_device *vpu_device, uint8_t index);

int vpu_prepare_regulator_and_clock(struct device *pdev);
int vpu_enable_regulator_and_clock(struct vpu_core *vpu_core);
int vpu_disable_regulator_and_clock(struct vpu_core *vpu_core);
void vpu_unprepare_regulator_and_clock(void);

int vpu_thermal_en_throttle_cb_set(struct vpu_device *vpu_device,
					uint8_t vcore_opp, uint8_t vpu_opp);
int vpu_thermal_dis_throttle_cb_set(struct vpu_device *vpu_device);

void vpu_get_segment_from_efuse(struct vpu_device *vpu_device);

bool vpu_get_force_change_vvpu_opp(int core);
bool vpu_get_force_change_dsp_freq(int core);
bool vpu_get_change_freq_first(int core);
void vpu_get_opp_freq(struct vpu_device *vpu_device, int *vcore_opp,
			int *apu_freq, int *apu_if_freq);
void vpu_opp_keep_routine(struct work_struct *work);

#endif /* _VPU_PLATFORM_CONFIG_H_ */
