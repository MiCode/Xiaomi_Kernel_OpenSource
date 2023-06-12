/*
 * Copyright (C) 2020 Richtek Inc.
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

#ifndef PD_DPM_PDO_SELECT_H
#define PD_DPM_PDO_SELECT_H

#include "inc/tcpci.h"

struct dpm_pdo_info_t {
	uint8_t type;
	uint8_t apdo_type;
	uint8_t pwr_limit;
	int vmin;
	int vmax;
	int uw;
	int ma;
};

struct dpm_rdo_info_t {
	uint8_t pos;
	uint8_t type;
	bool mismatch;

	int vmin;
	int vmax;

	union {
		uint32_t max_uw;
		uint32_t max_ma;
	};

	union {
		uint32_t oper_uw;
		uint32_t oper_ma;
	};
};

#define DPM_PDO_TYPE_FIXED	TCPM_POWER_CAP_VAL_TYPE_FIXED
#define DPM_PDO_TYPE_VAR	TCPM_POWER_CAP_VAL_TYPE_VARIABLE
#define DPM_PDO_TYPE_BAT	TCPM_POWER_CAP_VAL_TYPE_BATTERY
#define DPM_PDO_TYPE_APDO	TCPM_POWER_CAP_VAL_TYPE_AUGMENT

#define DPM_APDO_TYPE_PPS	(TCPM_POWER_CAP_APDO_TYPE_PPS)
#define DPM_APDO_TYPE_PPS_CF	(TCPM_POWER_CAP_APDO_TYPE_PPS_CF)

extern void dpm_extract_pdo_info(
			uint32_t pdo, struct dpm_pdo_info_t *info);

extern bool dpm_find_match_req_info(struct dpm_rdo_info_t *req_info,
	struct dpm_pdo_info_t *sink, int cnt, uint32_t *src_pdos,
	int min_uw, uint32_t select_rule);

#endif	/* PD_DPM_PDO_SELECT_H */
