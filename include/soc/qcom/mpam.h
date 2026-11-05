/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2023-2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _QCOM_MPAM_H
#define _QCOM_MPAM_H

#include <linux/types.h>
#include <linux/platform_device.h>

#define SYS_MPAM0_EL1		sys_reg(3, 0, 10, 5, 1)
#define SYS_MPAM1_EL1		sys_reg(3, 0, 10, 5, 0)

#define PARTID_BITS		16
#define PARTID_I_SHIFT		0
#define PARTID_D_SHIFT		(PARTID_I_SHIFT + PARTID_BITS)

#define PARTID_DEFAULT		0
#define PARTID_RESERVED		16
#define PARTID_MAX		64
#define PARTID_AVAILABLE	(PARTID_MAX - PARTID_RESERVED)

#define MSMON_CFG_MON_SEL	0x0800
#define MSMON_CFG_CSU_FLT	0x0810
#define MSMON_CFG_CSU_CTL	0x0818
#define MSMON_CFG_MBWU_FLT	0x0820
#define MSMON_CFG_MBWU_CTL	0x0828
#define MSMON_CSU		0x0840
#define MSMON_MBWU		0x0860
#define MSMON_MBWU_L		0x0880

#define MAX_MONITOR_INSTANCES		16 /* Reserved 12:15 */
#define MAX_MONITOR_INSTANCES_SHARED	12
#define MONITOR_MAX		MAX_MONITOR_INSTANCES_SHARED

#define MPAM_MAX_RETRY			5000
#define MPAM_MAX_MATCH_SEQ_RETRY	10

enum msc_id {
	MSC_0 = 0,
	MSC_1,
	MSC_2,
	MSC_3,
	MSC_MAX,
};

/* Supported Mode */
enum mpam_config_mode {
	SET_CACHE_CAPACITY = 0,
	SET_CPBM,
	SET_DSPRI,
	SET_CACHE_CAPICITY_AND_CPBM,
	SET_CACHE_CAPACITY_AND_CPBM_AND_DSPRI,
	SET_SLC,
	SET_ALL_CPU_TUNABLE,
	MAX_MODE
};

/* Monitor type */
enum mpam_monitor_type {
	MPAM_INVALID_MONITOR = 0,
	MPAM_TYPE_MBW_MONITOR = 1,
	MPAM_TYPE_CSU_MONITOR = 2,
	MAX_MON_TYPE_SUPPORTED = 3
};

/* PARAM_SET_CACHE_PARTITION */
struct mpam_set_cache_partition {
	uint32_t part_id;
	uint32_t cache_capacity;
	uint32_t cpbm_mask;
	uint32_t dspri;
	/*
	 * [0:7] - mode
	 * --[0x00] : set cache_capicity
	 * --[0x01] : set cpbm
	 * --[0x02] : set dspri
	 * --[0x03] : set cache_capicity & cpbm
	 * --[0x04] : set cache_capicity & cpbm & dspri
	 * --[0x05] : set slc_gear
	 * --[0x06] : set all cpu tunable
	 * [8:63] - Reserved
	 */
	uint64_t mpam_config_ctrl;
	uint32_t msc_id;
	uint32_t slc_partition_id;
} __packed;

/* Part ID and monitor Parameters */
struct mpam_monitor_configuration {
	uint32_t msc_id;
	uint32_t part_id;
	uint32_t mon_instance;
	uint32_t mon_type;
	/* Filter and control bits */
	uint64_t mpam_config_ctrl;
} __packed;

/* PARAM_GET_VERSION */
struct mpam_ver_ret {
	uint32_t version;
};

/* PARAM_GET_CACHE_PARTITION */
struct mpam_read_cache_portion {
	uint32_t msc_id;
	uint32_t part_id;
} __packed;

struct mpam_config_val {
	uint32_t cpbm;
	uint32_t capacity;
	uint32_t dspri;
	uint32_t slc_partition_id;
} __packed;

struct monitors_value {
	uint32_t capture_status;
	uint32_t msc_id;
	uint32_t csu_mon_enable_map[MAX_MONITOR_INSTANCES];
	uint32_t csu_mon_value[MAX_MONITOR_INSTANCES];
	uint32_t mbw_mon_enable_map[MAX_MONITOR_INSTANCES];
	uint64_t mbw_mon_value[MAX_MONITOR_INSTANCES];
	uint64_t last_capture_time;
} __packed;

/* PARAM_SET_PLATFORM_BW_CTRL */
struct platform_mpam_bw_ctrl_cfg {
	uint32_t msc_id;
	uint32_t client_id;
	uint32_t platform_mpam_gear;
	uint64_t config_ctrl;
} __packed;

/* PARAM_SET_PLATFORM_BW_MONITOR */
struct platform_mpam_bw_monitor_cfg {
	uint32_t msc_id;
	uint32_t client_id;
	uint32_t mon_instance;
	uint32_t mon_type;
	/*
	 * [0:1] - control
	 * --[0x0] : disable
	 * --[0x1] : enable
	 * [2:63] - Reserved
	 */
	uint64_t config_ctrl;
} __packed;

/* PARAM_GET_PLATFORM_BW_CTRL_CONFIG */
struct platform_mpam_read_bw_ctrl {
	uint32_t msc_id;
	uint32_t client_id;
} __packed;

struct platform_mpam_bw_ctrl_config {
	uint32_t platform_mpam_gear;
} __packed;

/* NOC Monitor structure in shared memory */
union platform_monitor_value {
	struct {
		uint32_t capture_status;
		uint32_t msc_id;
		uint32_t client_id;
		uint32_t bwmon_byte_count;
		uint64_t last_capture_time;
	} V1 __packed;

	struct {
		uint32_t capture_status;
		uint32_t msc_id;
		uint32_t client_id;
		uint64_t bwmon_byte_count __aligned(8);
		uint64_t last_capture_time;
	} V2 __packed;
};

#if IS_ENABLED(CONFIG_QTI_MPAM)
int qcom_mpam_set_cache_partition(struct mpam_set_cache_partition *param);
int qcom_mpam_get_version(struct mpam_ver_ret *ver);
int qcom_mpam_get_cache_partition(struct mpam_read_cache_portion *param,
						struct mpam_config_val *val);
int qcom_mpam_config_monitor(struct mpam_monitor_configuration *param);
int qcom_mpam_set_platform_bw_ctrl(struct platform_mpam_bw_ctrl_cfg *param);
int qcom_mpam_get_platform_bw_ctrl(struct platform_mpam_read_bw_ctrl *param,
						struct platform_mpam_bw_ctrl_config *val);
int qcom_mpam_set_platform_bw_monitor(struct platform_mpam_bw_monitor_cfg *param);
#else
static inline int qcom_mpam_set_cache_partition(struct mpam_set_cache_partition *param)
{
	return 0;
}

static inline int qcom_mpam_get_version(struct mpam_ver_ret *ver)
{
	return 0;
}

static inline int qcom_mpam_get_cache_partition(struct mpam_read_cache_portion *param,
						struct mpam_config_val *val)
{
	return 0;
}

static inline int qcom_mpam_config_monitor(struct mpam_monitor_configuration *param)
{
	return 0;
}

static inline int qcom_mpam_set_platform_bw_ctrl(struct platform_mpam_bw_ctrl_cfg *param)
{
	return 0;
}

static inline int qcom_mpam_get_platform_bw_ctrl(struct platform_mpam_read_bw_ctrl *param,
						struct platform_mpam_bw_ctrl_config *val)
{
	return 0;
}

static inline int qcom_mpam_set_platform_bw_monitor(struct platform_mpam_bw_monitor_cfg *param)
{
	return 0;
}
#endif
struct config_group *platform_mpam_get_root_group(void);

#endif /* _QCOM_MPAM_H */
