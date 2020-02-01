/* Copyright (c) 2016, 2018-2020, The Linux Foundation. All rights reserved.
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

#ifndef __MSM_ISP48_H__
#define __MSM_ISP48_H__

extern struct msm_vfe_hardware_info vfe48_hw_info;

enum msm_vfe_clk_rates {
	MSM_VFE_CLK_RATE_SVS = 0,
	MSM_VFE_CLK_RATE_NOMINAL = 1,
	MSM_VFE_CLK_RATE_TURBO = 2,
	MSM_VFE_MAX_CLK_RATES = 3,
};

#define MSM_VFE48_HW_VERSION 0x8
#define MSM_VFE48_HW_VERSION_TRINKET 0x9
#define MSM_VFE48_HW_VERSION_SHIFT 28
#define MSM_VFE48_HW_VERSION_MASK 0xF

static inline int msm_vfe_is_vfe48(struct vfe_device *vfe_dev)
{
	/* Check for Trinket specific as it uses h/w version 0x9 */
	if ((vfe_dev->vfe_hw_version >> MSM_VFE48_HW_VERSION_SHIFT) ==
		MSM_VFE48_HW_VERSION_TRINKET)
		return (((vfe_dev->vfe_hw_version >> MSM_VFE48_HW_VERSION_SHIFT)
			& MSM_VFE48_HW_VERSION_MASK)
			== MSM_VFE48_HW_VERSION_TRINKET);
	return (((vfe_dev->vfe_hw_version >> MSM_VFE48_HW_VERSION_SHIFT) &
		MSM_VFE48_HW_VERSION_MASK) == MSM_VFE48_HW_VERSION);
}

void msm_vfe48_stats_cfg_ub(struct vfe_device *vfe_dev);
uint32_t msm_vfe48_get_ub_size(struct vfe_device *vfe_dev);
void msm_vfe48_clear_dual_irq_status(struct vfe_device *vfe_dev,
	uint32_t *dual_irq_status);
void msm_vfe48_dual_config_irq(struct vfe_device *vfe_dev,
		uint32_t irq0_mask, uint32_t irq1_mask,
		enum msm_isp_irq_operation oper);
void msm_vfe48_set_dual_vfe_mode(struct vfe_device *vfe_dev);
void msm_vfe48_clear_dual_vfe_mode(struct vfe_device *vfe_dev);
int msm_vfe48_get_dual_sync_platform_data(struct vfe_device *vfe_dev);
void msm_vfe48_clear_dual_irq_status(struct vfe_device *vfe_dev,
	uint32_t *dual_irq_status);
void msm_vfe48_dual_config_irq(struct vfe_device *vfe_dev,
		uint32_t irq0_mask, uint32_t irq1_mask,
		enum msm_isp_irq_operation oper);
#endif /* __MSM_ISP48_H__ */
