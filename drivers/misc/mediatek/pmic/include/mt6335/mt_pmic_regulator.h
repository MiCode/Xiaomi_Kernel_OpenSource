/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _MT_PMIC_REGULATOR_H_
#define _MT_PMIC_REGULATOR_H_

typedef enum {
	VCORE,
	VDRAM,
	VMODEM,
	VMD1,
	VS1,
	VS2,
	VPA1,
	VPA2,
	VSRAM_DVFS1,
	VSRAM_DVFS2,
	VSRAM_VGPU,
	VSRAM_VCORE,
	VSRAM_VMD,
} BUCK_TYPE;

struct mtk_bucks_t {
	const char *name;
	unsigned n_voltages;
	unsigned int min_uV;
	unsigned int max_uV;
	unsigned int uV_step;
	unsigned int stb;
	struct device_attribute en_att;
	struct device_attribute voltage_att;
	PMU_FLAGS_LIST_ENUM en;
	PMU_FLAGS_LIST_ENUM mode;
	PMU_FLAGS_LIST_ENUM vosel;
	PMU_FLAGS_LIST_ENUM da_qi_en;
	PMU_FLAGS_LIST_ENUM da_ni_vosel;
	bool isUsedable;
	const char *type;
};

#define PMIC_BUCK_GEN1(_name, _en, _mode, _vosel, _da_qi_en, _da_ni_vosel, min, max, step, _stb, _id)	\
	{	\
		.name = #_name,	\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.min_uV = (min),	\
		.max_uV = (max),	\
		.uV_step = (step),	\
		.en = (_en),	\
		.mode = (_mode),	\
		.en_att = __ATTR(buck_##_id##_status, 0664, show_BUCK_STATUS, store_BUCK_STATUS),	\
		.voltage_att = __ATTR(buck_##_id##_voltage, 0664, show_BUCK_VOLTAGE, store_BUCK_VOLTAGE),	\
		.vosel = (_vosel),	\
		.da_qi_en = (_da_qi_en),	\
		.da_ni_vosel = (_da_ni_vosel),	\
		.stb = (_stb),	\
		.isUsedable = 0,	\
		.type = "BUCK",	\
	}
#endif				/* _MT_PMIC_REGULATOR_H_ */
