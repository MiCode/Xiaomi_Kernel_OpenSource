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

#ifndef _MT_REGULATOR_CODEGEN_H_
#define _MT_REGULATOR_CODEGEN_H_

/* -------Code Gen Start-------*/
/* Non regular voltage regulator */
#define NON_REGULAR_VOLTAGE_REGULATOR_GEN(_name, _type, array, mode, use)	\
{	\
	.desc = {	\
		.name = #_name,	\
		.n_voltages = (sizeof(array)/sizeof(array[0])),	\
		.ops = &pmic_##_type##_##_name##_ops,	\
		.type = REGULATOR_VOLTAGE,	\
	},	\
	.init_data = {	\
		.constraints = {	\
			.valid_ops_mask = (mode),	\
		},	\
	},	\
	.pvoltages = (void *)(array),	\
	.en_att = __ATTR(ldo_##_name##_status, 0664, show_LDO_STATUS, store_LDO_STATUS),	\
	.voltage_att = __ATTR(ldo_##_name##_voltage, 0664, show_LDO_VOLTAGE, store_LDO_VOLTAGE),	\
	.vol_cb = mt6335_upmu_set_rg_##_name##_vosel,	\
	.da_en_cb = mt6335_upmu_get_da_qi_##_name##_en,	\
	.da_vol_cb = mt6335_upmu_get_rg_##_name##_vosel,	\
	.isUsedable = (use),	\
}

/* Regular voltage regulator */
#define REGULAR_VOLTAGE_REGULATOR_GEN(_name, _type, min, max, step, min_sel, mode, use)	\
{	\
	.desc = {	\
		.name = #_name,	\
		.n_voltages = ((max) - (min)) / (step) + 1,	\
		.ops = &pmic_##_type##_##_name##_ops,	\
		.type = REGULATOR_VOLTAGE,	\
		.min_uV = (min),	\
		.uV_step = (step),	\
		.linear_min_sel = min_sel,	\
	},	\
	.init_data = {	\
		.constraints = {	\
			.valid_ops_mask = (mode),	\
		},	\
	},	\
	.en_att = __ATTR(ldo_##_name##_status, 0664, show_LDO_STATUS, store_LDO_STATUS),	\
	.voltage_att = __ATTR(ldo_##_name##_voltage, 0664, show_LDO_VOLTAGE, store_LDO_VOLTAGE),	\
	.vol_cb = mt6335_upmu_set_rg_##_name##_vosel,	\
	.da_en_cb = mt6335_upmu_get_da_qi_##_name##_en,	\
	.da_vol_cb = mt6335_upmu_get_da_ni_##_name##_vosel,	\
	.isUsedable = (use),	\
}

#define FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(_name, _type, array, mode, use)	\
{	\
	.desc = {	\
		.name = #_name,	\
		.n_voltages = 1,	\
		.ops = &pmic_##_type##_##_name##_ops,	\
		.type = REGULATOR_VOLTAGE,	\
	},	\
	.init_data = {	\
		.constraints = {	\
			.valid_ops_mask = (mode),	\
		},	\
	},	\
	.pvoltages = (void *)(array),	\
	.en_att = __ATTR(ldo_##_name##_status, 0664, show_LDO_STATUS, store_LDO_STATUS),	\
	.voltage_att = __ATTR(ldo_##_name##_voltage, 0664, show_LDO_VOLTAGE, store_LDO_VOLTAGE),	\
	.da_en_cb = mt6335_upmu_get_da_qi_##_name##_en,	\
	.da_vol_cb = NULL,  \
	.isUsedable = (use),	\
}

#endif				/* _MT_REGULATOR_CODEGEN_H_ */
