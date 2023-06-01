/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef __SOC_QCOM_SOCINFO_H__
#define __SOC_QCOM_SOCINFO_H__

#include <linux/types.h>

enum subset_part_type {
	PART_UNKNOWN      = 0,
	PART_GPU          = 1,
	PART_VIDEO        = 2,
	PART_CAMERA       = 3,
	PART_DISPLAY      = 4,
	PART_AUDIO        = 5,
	PART_MODEM        = 6,
	PART_WLAN         = 7,
	PART_COMP         = 8,
	PART_SENSORS      = 9,
	PART_NPU          = 10,
	PART_SPSS         = 11,
	PART_NAV          = 12,
	PART_COMP1        = 13,
	PART_DISPLAY1     = 14,
	PART_NSP	  = 15,
	PART_EVA	  = 16,
	NUM_PARTS_MAX,
};

enum subset_cluster_type {
	CLUSTER_CPUSS      = 0,
	NUM_CLUSTERS_MAX,
};

#if IS_ENABLED(CONFIG_QCOM_SOCINFO)
uint32_t socinfo_get_id(void);
uint32_t socinfo_get_serial_number(void);
const char *socinfo_get_id_string(void);
uint32_t socinfo_get_cluster_info(enum subset_cluster_type cluster);
bool socinfo_get_part_info(enum subset_part_type part);
int socinfo_get_part_count(enum subset_part_type part);
int socinfo_get_subpart_info(enum subset_part_type part,
		u32 *part_info,
		u32 num_parts);
int socinfo_get_oem_variant_id(void);
#else
static inline uint32_t socinfo_get_id(void)
{
	return 0;
}

static inline uint32_t socinfo_get_serial_number(void)
{
	return 0;
}

static inline const char *socinfo_get_id_string(void)
{
	return "N/A";
}

uint32_t socinfo_get_cluster_info(enum subset_cluster_type cluster)
{
	return 0;
}
bool socinfo_get_part_info(enum subset_part_type part)
{
	return false;
}
int socinfo_get_part_count(enum subset_part_type part)
{
	return -EINVAL;
}
int socinfo_get_subpart_info(enum subset_part_type part,
		u32 *part_info,
		u32 num_parts)
{
	return -EINVAL;
}
int socinfo_get_oem_variant_id(void)
{
	return -EINVAL;
}
#endif /* CONFIG_QCOM_SOCINFO */

#endif /* __SOC_QCOM_SOCINFO_H__ */
