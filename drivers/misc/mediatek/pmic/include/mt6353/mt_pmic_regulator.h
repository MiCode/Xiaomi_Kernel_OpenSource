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


#define PMIC_BUCK_GEN(_name, en, vol, min, max, step)	\
	{	\
		.desc = {	\
			.name = #_name,	\
			.n_voltages = ((max) - (min)) / (step) + 1,	\
			.min_uV = (min),	\
			.uV_step = (step),	\
		},	\
		.en_att = __ATTR(BUCK_##_name##_STATUS, 0664, show_BUCK_STATUS, store_BUCK_STATUS),	\
		.voltage_att = __ATTR(BUCK_##_name##_VOLTAGE, 0664, show_BUCK_VOLTAGE, store_BUCK_VOLTAGE),	\
		.init_data = {	\
			.constraints = {	\
				.valid_ops_mask = 9,	\
			},	\
		},	\
		.qi_en_reg = (en),	\
		.qi_vol_reg = (vol),	\
		.isUsedable = 0,	\
		.type = "BUCK",	\
	}

#endif				/* _MT_PMIC_REGULATOR_H_ */
