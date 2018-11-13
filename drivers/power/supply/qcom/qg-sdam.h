/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 */

#ifndef __QG_SDAM_H__
#define __QG_SDAM_H__

#define SDAM_TYPE			0x2E

enum qg_sdam_param {
	SDAM_VALID,
	SDAM_SOC,
	SDAM_TEMP,
	SDAM_RBAT_MOHM,
	SDAM_OCV_UV,
	SDAM_IBAT_UA,
	SDAM_TIME_SEC,
	SDAM_PON_OCV_UV,
	SDAM_ESR_CHARGE_DELTA,
	SDAM_ESR_DISCHARGE_DELTA,
	SDAM_ESR_CHARGE_SF,
	SDAM_ESR_DISCHARGE_SF,
	SDAM_MAX,
};

struct qg_sdam {
	struct regmap		*regmap;
	u16			sdam_base;
};

int qg_sdam_init(struct device *dev);
int qg_sdam_write(u8 param, u32 data);
int qg_sdam_read(u8 param, u32 *data);
int qg_sdam_write_all(u32 *sdam_data);
int qg_sdam_read_all(u32 *sdam_data);
int qg_sdam_multibyte_write(u32 offset, u8 *sdam_data, u32 length);
int qg_sdam_multibyte_read(u32 offset, u8 *sdam_data, u32 length);

#endif
