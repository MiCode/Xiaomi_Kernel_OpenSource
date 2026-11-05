/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024-2025, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _QCOM_LLCC_HEURISTICS_H
#define _QCOM_LLCC_HEURISTICS_H

#define SCID_HEURISTICS_SCMI_STR	0x4845555253434944 /* "HEURSCID" */
#define CD_MAX				2
#define CD_MAX_V2			8

enum llcc_set_attribute {
	HEURISTICS_INIT,
	SCID_ACTIVATION_CONTROL,
};

/* HEURISTICS_INIT */
struct scid_heuristics_params {
	uint32_t heuristics_scid;
	uint32_t freq_idx[CD_MAX];
	uint32_t freq_idx_residency[CD_MAX];
	uint32_t scid_heuristics_enabled;
} __packed;

struct scid_heuristics_params_v2 {
	uint32_t heuristics_scid;
	uint32_t freq_idx[CD_MAX_V2];
	uint32_t freq_idx_residency[CD_MAX_V2];
	uint32_t scid_heuristics_enabled;
} __packed;

struct scid_heuristics_data {
	struct scid_heuristics_params params;
} __packed;

struct scid_heuristics_data_v2 {
	struct scid_heuristics_params_v2 params;
} __packed;

#endif /* _QCOM_LLCC_HEURISTICS_H */
