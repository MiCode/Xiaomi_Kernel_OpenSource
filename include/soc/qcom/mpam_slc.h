/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _QCOM_MPAM_SLC_H
#define _QCOM_MPAM_SLC_H

#define MAX_PART_ID		10
#define SLC_INVALID_PARTID      ((1 << 16) - 1)
#define SLC_NUM_PARTIDS		5

enum slc_clients_id {
	APPS,
	GPU,
	NSP,
	SLC_CLIENT_MAX,
};

enum gear_val {
	GEAR_VERY_HIGH,
	GEAR_HIGH,
	GEAR_MID,
	GEAR_LOW,
	GEAR_BYPASS,
	GEAR_MAX,
};

static char gear_index[][25] = {
	"SLC_GEAR_VERY_HIGH",
	"SLC_GEAR_HIGH",
	"SLC_GEAR_MID",
	"SLC_GEAR_LOW",
	"SLC_GEAR_BYPASS",
	"",
};

#define MAX_NUM_GEARS	GEAR_MAX

/* slc Monitor capability */
struct slc_mon_capability {
	uint32_t read_miss_config_available;
	uint32_t capacity_config_available;
};

struct slc_mon_configured {
	uint32_t read_miss_configured;
	uint32_t capacity_configured;
};

/* slc device capability */

struct slc_partid_capability {
	uint8_t part_id;
	uint8_t num_gears;
	uint8_t part_id_gears[MAX_NUM_GEARS];
} __packed;

struct slc_client_info {
	uint16_t client_id;
	uint16_t num_part_id;
} __packed;

struct slc_client_capability {
	struct slc_client_info client_info;
	struct slc_partid_capability *slc_partid_cap;
	uint8_t enabled;
	const char *client_name;
} __packed;

struct qcom_slc_capability {
	uint32_t num_clients;
	struct slc_client_capability *slc_client_cap;
	struct slc_mon_capability slc_mon_list;
	struct slc_mon_configured slc_mon_configured;
} __packed;

/* slc slice configuration */

struct qcom_slc_gear_val {
	uint32_t gear_val;
} __packed;

struct slc_partid_config {
	struct msc_query query;
	struct qcom_slc_gear_val gear_config;
} __packed;

/* slc mon configuration */

enum slc_mon_function {
	CACHE_CAPACITY_CONFIG,
	CACHE_READ_MISS_CONFIG,
};

struct slc_mon_config_val {
	uint32_t slc_mon_function;
	uint32_t enable;
} __packed;

struct slc_mon_config {
	struct msc_query query;
	struct slc_mon_config_val config;
} __packed;

/* PARAM_SET_CONFIG_SLC_MPAM_START_STOP */
struct mpam_enable {
	uint32_t value;
} __packed;

struct qcom_slc_mpam_enable_cfg {
	struct msc_query query;
	struct mpam_enable enable;
} __packed;

/* slc monitor shared memory */
struct slc_capacity {
	uint32_t num_cache_lines;
	uint32_t cap_enabled;
} __packed;

struct slc_read_miss_cntr {
	uint64_t rd_misses;
	uint32_t cntr_index;
	uint32_t miss_enabled;
} __packed;

struct slc_partid_info {
	uint32_t client_id;
	uint32_t part_id;
} __packed;

struct qcom_slc_mon_data {
	struct slc_partid_info part_info;
	struct slc_capacity cap_stats;
	struct slc_read_miss_cntr rd_miss_stats;
} __packed;

struct qcom_slc_mon_mem {
	uint32_t match_seq;
	uint16_t msc_id;
	uint16_t num_active_mon;
	struct qcom_slc_mon_data data[SLC_NUM_PARTIDS];
	uint64_t last_capture_time;
} __packed;

struct capacity_info  {
	uint32_t slc_mon_function;
	uint64_t last_capture_time;
	uint32_t num_cache_lines;
} __packed;

struct miss_info  {
	uint32_t slc_mon_function;
	uint64_t last_capture_time;
	uint64_t num_rd_misses;
} __packed;

struct mon_ref {
	uint32_t slc_mon_function;
	uint64_t last_capture_time;
	uint64_t mon_data;
} __packed;

union mon_values {
	struct capacity_info capacity;
	struct miss_info misses;
	struct mon_ref ref;
} __packed;

#endif /* _QCOM_MPAM_SLC_H */
