/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __SMI_CONFIGURATION_H__
#define __SMI_CONFIGURATION_H__

#include <mt-plat/mtk_smi.h>

/* SMI common configuration */
#define SMI_PROFILE_CONFIG_NUM SMI_BWC_SCEN_CNT
#define SMI_MAX_PORT_NUM 32
#define SMI_ERROR_ADDR 0

#define LARB_BACKUP_REG_SIZE 128

#define SF_HWC_PIXEL_MAX_NORMAL  (1920 * 1080 * 7)
#define SF_HWC_PIXEL_MAX_VR   (1920 * 1080 * 4 + 1036800) /* 4.5 FHD size */
#define SF_HWC_PIXEL_MAX_VP   (1920 * 1080 * 7)
#define SF_HWC_PIXEL_MAX_ALWAYS_GPU  (1920 * 1080 * 1)

/* debug parameters */
#define SMI_COMMON_DEFAULT_DEBUG_OFFSET_NUM 17
#define SMI_LARB_DEFAULT_DEBUG_OFFSET_NUM 70

#if defined(SMI_VIN)
/* common configuration */
#define SMI_PARAM_BW_OPTIMIZATION (1)
#define SMI_PARAM_BUS_OPTIMIZATION (0xFF)
#define SMI_PARAM_ENABLE_IOCTL (1)
#define SMI_PARAM_DISABLE_FORCE_CAMERA_HPM (1)
#define SMI_PARAM_FORCE_MMSYS_CLKS_ALWAYS_ON (0)

/* larb configuration */
#define SMI_LARB_NUM 8

#define SMI_LARB0_PORT_NUM 7
#define SMI_LARB1_PORT_NUM 6
#define SMI_LARB2_PORT_NUM 3
#define SMI_LARB3_PORT_NUM 5
#define SMI_LARB4_PORT_NUM 10
#define SMI_LARB5_PORT_NUM 12
#define SMI_LARB6_PORT_NUM 21
#define SMI_LARB7_PORT_NUM 11

/* debug parameters */
#define SMI_COMMON_PORT_NUM SMI_LARB_NUM
#define SMI_COMMON_DEBUG_OFFSET_NUM 37
#define SMI_LARB_DEBUG_OFFSET_NUM 97

#define SMI_BASIC_LARB0_SETTING_NUM 6
#define SMI_BASIC_LARB1_SETTING_NUM 6
#define SMI_BASIC_LARB2_SETTING_NUM 2
#define SMI_BASIC_LARB3_SETTING_NUM 2
#define SMI_BASIC_LARB4_SETTING_NUM 2
#define SMI_BASIC_LARB5_SETTING_NUM 2
#define SMI_BASIC_LARB6_SETTING_NUM 3
#define SMI_BASIC_LARB7_SETTING_NUM 2
#define SMI_BASIC_COMMON_SETTING_NUM 17

#define SMI_MMU_LARB0_SETTING_NUM 3
#define SMI_MMU_LARB1_SETTING_NUM 3
#define SMI_MMU_LARB2_SETTING_NUM 0
#define SMI_MMU_LARB3_SETTING_NUM 0
#define SMI_MMU_LARB4_SETTING_NUM 0
#define SMI_MMU_LARB5_SETTING_NUM 0
#define SMI_MMU_LARB6_SETTING_NUM 0
#define SMI_MMU_LARB7_SETTING_NUM 0

#else
#define SMI_PARAM_BW_OPTIMIZATION (0)
#define SMI_PARAM_BUS_OPTIMIZATION (0x1FF)
#define SMI_PARAM_ENABLE_IOCTL (0)
#define SMI_PARAM_DISABLE_FORCE_CAMERA_HPM (1)
#define SMI_PARAM_FORCE_MMSYS_CLKS_ALWAYS_ON (0)

#define SMI_COMMON_DEBUG_OFFSET_NUM SMI_COMMON_DEFAULT_DEBUG_OFFSET_NUM
#define SMI_LARB_DEBUG_OFFSET_NUM SMI_LARB_DEFAULT_DEBUG_OFFSET_NUM
#define SMI_LARB_NUM 1
#endif

/* larb configuration */
#define SMI_LARB0_REG_INDX 0
#define SMI_LARB1_REG_INDX 1
#define SMI_LARB2_REG_INDX 2
#define SMI_LARB3_REG_INDX 3
#define SMI_LARB4_REG_INDX 4
#define SMI_LARB5_REG_INDX 5
#define SMI_LARB6_REG_INDX 6
#define SMI_LARB7_REG_INDX 7
#define SMI_LARB8_REG_INDX 8
#define SMI_COMMON_REG_INDX SMI_LARB_NUM
#define SMI_REG_REGION_MAX (SMI_LARB_NUM + 1)

struct SMI_SETTING_VALUE {
	unsigned int offset;
	int value;
};

struct SMI_SETTING {
	unsigned int smi_common_reg_num;
	struct SMI_SETTING_VALUE *smi_common_setting_vals;
	unsigned int smi_larb_reg_num[SMI_LARB_NUM];
	struct SMI_SETTING_VALUE *smi_larb_setting_vals[SMI_LARB_NUM];
};

struct SMI_PROFILE_CONFIG {
	int smi_profile;
	struct SMI_SETTING *setting;
};

extern struct SMI_SETTING smi_basic_setting_config;
extern struct SMI_SETTING smi_mmu_setting_config;
extern struct SMI_PROFILE_CONFIG smi_profile_config[SMI_PROFILE_CONFIG_NUM];

extern unsigned long smi_larb_debug_offset[SMI_LARB_DEBUG_OFFSET_NUM];
extern unsigned long smi_common_debug_offset[SMI_COMMON_DEBUG_OFFSET_NUM];
extern unsigned long smi_m4u_non_secure_offset[SMI_MAX_PORT_NUM];
extern unsigned long smi_m4u_secure_offset[SMI_MAX_PORT_NUM];
#endif /* __SMI_CONFIGURATION_H__ */
