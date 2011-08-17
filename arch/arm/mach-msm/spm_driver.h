/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __ARCH_ARM_MACH_MSM_SPM_DEVICES_H
#define __ARCH_ARM_MACH_MSM_SPM_DEVICES_H

#include "spm.h"

struct msm_spm_driver_data {
	void __iomem *reg_base_addr;
	uint32_t vctl_timeout_us;
	uint32_t reg_shadow[MSM_SPM_REG_NR_INITIALIZE];
	uint32_t *reg_seq_entry_shadow;
};

int msm_spm_drv_init(struct msm_spm_driver_data *dev,
		struct msm_spm_platform_data *data);
int msm_spm_drv_set_low_power_mode(struct msm_spm_driver_data *dev,
		uint32_t addr);
int msm_spm_drv_set_vdd(struct msm_spm_driver_data *dev,
		unsigned int vlevel);
int msm_spm_drv_write_seq_data(struct msm_spm_driver_data *dev,
		uint8_t *cmd, uint32_t offset);
void msm_spm_drv_flush_seq_entry(struct msm_spm_driver_data *dev);
int msm_spm_drv_set_spm_enable(struct msm_spm_driver_data *dev,
		bool enable);

#endif
