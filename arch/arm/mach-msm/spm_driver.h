/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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

enum msm_spm_pmic_port {
	MSM_SPM_PMIC_VCTL_PORT,
	MSM_SPM_PMIC_PHASE_PORT,
	MSM_SPM_PMIC_PFM_PORT,
};

struct msm_spm_driver_data {
	uint32_t major;
	uint32_t minor;
	uint32_t ver_reg;
	uint32_t vctl_port;
	uint32_t phase_port;
	uint32_t pfm_port;
	void __iomem *reg_base_addr;
	uint32_t vctl_timeout_us;
	uint32_t avs_timeout_us;
	uint32_t reg_shadow[MSM_SPM_REG_NR];
	uint32_t *reg_seq_entry_shadow;
	uint32_t *reg_offsets;
};

int msm_spm_drv_init(struct msm_spm_driver_data *dev,
		struct msm_spm_platform_data *data);
void msm_spm_drv_reinit(struct msm_spm_driver_data *dev);
int msm_spm_drv_set_low_power_mode(struct msm_spm_driver_data *dev,
		uint32_t addr);
int msm_spm_drv_set_vdd(struct msm_spm_driver_data *dev,
		unsigned int vlevel);
uint32_t msm_spm_drv_get_sts_curr_pmic_data(
		struct msm_spm_driver_data *dev);
int msm_spm_drv_write_seq_data(struct msm_spm_driver_data *dev,
		uint8_t *cmd, uint32_t *offset);
void msm_spm_drv_flush_seq_entry(struct msm_spm_driver_data *dev);
int msm_spm_drv_set_spm_enable(struct msm_spm_driver_data *dev,
		bool enable);
int msm_spm_drv_set_pmic_data(struct msm_spm_driver_data *dev,
		enum msm_spm_pmic_port port, unsigned int data);
#endif
