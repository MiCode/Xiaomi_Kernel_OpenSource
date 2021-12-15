/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MT_REGULATOR_CODEGEN_H_
#define _MT_REGULATOR_CODEGEN_H_

extern struct mtk_regulator mt_ldos[], mt_bucks[];
extern struct of_regulator_match pmic_regulator_buck_matches[];
extern struct of_regulator_match pmic_regulator_ldo_matches[];
extern int mt_ldos_size, mt_bucks_size;
extern int pmic_regulator_ldo_matches_size, pmic_regulator_buck_matches_size;

/* -------Code Gen Start-------*/
/* Non regular voltage regulator */
#define NON_REGULAR_VOLTAGE_REGULATOR_GEN(_name, _type, array,	\
					array_idx, mode, use)	\
{	\
	.desc = {	\
		.name = #_name,	\
		.n_voltages = ARRAY_SIZE(array),	\
		.ops = &pmic_##_type##_##_name##_ops,	\
		.type = REGULATOR_VOLTAGE,	\
	},	\
	.init_data = {	\
		.constraints = {	\
			.valid_ops_mask = (mode),	\
		},	\
	},	\
	.pvoltages = (void *)(array),	\
	.idxs = (void *)(array_idx),	\
	.en_att = __ATTR(_type##_##_name##_status, 0664,		\
			show_regulator_status, store_regulator_status),	\
	.voltage_att = __ATTR(_type##_##_name##_voltage, 0664,  \
		show_regulator_voltage, store_regulator_voltage),	\
	.en_cb = mt6357_upmu_set_rg_##_type##_##_name##_en,	\
	.vol_cb = mt6357_upmu_set_rg_##_name##_vosel,	\
	.da_en_cb = mt6357_upmu_get_da_##_name##_en,   \
	.da_vol_cb = mt6357_upmu_get_rg_##_name##_vosel,  \
	.isUsedable = (use),	\
}

/* Regular voltage regulator */
#define REGULAR_VOLTAGE_REGULATOR_LDO_GEN(_name, _type, min, max,	\
					step, min_sel, mode, use)	\
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
	.en_att = __ATTR(_type##_##_name##_status, 0664,	\
		show_regulator_status, store_regulator_status), \
	.voltage_att = __ATTR(_type##_##_name##_voltage, 0664,  \
		show_regulator_voltage, store_regulator_voltage), \
	.en_cb = mt6357_upmu_set_rg_##_type##_##_name##_en,	\
	.vol_cb = mt6357_upmu_set_rg_##_type##_##_name##_vosel,	\
	.da_en_cb = mt6357_upmu_get_da_##_name##_en,   \
	.da_vol_cb = mt6357_upmu_get_da_##_name##_vosel,  \
	.isUsedable = (use),	\
}

#define REGULAR_VOLTAGE_REGULATOR_BUCK_GEN(_name, _type, min, max,  \
				step, min_sel, mode, use)	\
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
	.en_att = __ATTR(_type##_##_name##_status, 0664,	\
		show_regulator_status, store_regulator_status), \
	.voltage_att = __ATTR(_type##_##_name##_voltage, 0664,  \
		show_regulator_voltage, store_regulator_voltage), \
	.en_cb = mt6357_upmu_set_rg_##_type##_##_name##_en,	\
	.vol_cb = mt6357_upmu_set_rg_##_type##_##_name##_vosel,	\
	.da_en_cb = mt6357_upmu_get_da_##_name##_en,   \
	.da_vol_cb = mt6357_upmu_get_da_##_name##_vosel,  \
	.isUsedable = (use),	\
}
/* Fixed voltage regulator */
#define FIXED_REGULAR_VOLTAGE_REGULATOR_GEN(_name, _type, fixed, mode, use)\
{	\
	.desc = {	\
		.name = #_name,	\
		.n_voltages = 1,	\
		.ops = &pmic_##_type##_##_name##_ops,	\
		.type = REGULATOR_VOLTAGE,	\
		.fixed_uV = (fixed),	\
	},	\
	.init_data = {	\
		.constraints = {	\
			.valid_ops_mask = (mode),	\
		},	\
	},	\
	.en_att = __ATTR(_type##_##_name##_status, 0664,	\
			show_regulator_status, store_regulator_status),	\
	.voltage_att = __ATTR(_type##_##_name##_voltage, 0664,  \
			show_regulator_voltage, store_regulator_voltage), \
	.en_cb = mt6357_upmu_set_rg_##_type##_##_name##_en,	\
	.da_en_cb = mt6357_upmu_get_da_##_name##_en,   \
	.isUsedable = (use),	\
}

enum MT6357_POWER_BUCK {
	MT6357_POWER_BUCK_VS1,
	MT6357_POWER_BUCK_VMODEM,
	MT6357_POWER_BUCK_VCORE,
	MT6357_POWER_BUCK_VPROC,
	MT6357_POWER_BUCK_VPA,
	MT6357_BUCK_COUNT_END
};

enum MT6357_POWER_LDO {
	MT6357_POWER_LDO_VFE28,
	MT6357_POWER_LDO_VXO22,
	MT6357_POWER_LDO_VRF18,
	MT6357_POWER_LDO_VRF12,
	MT6357_POWER_LDO_VEFUSE,
	MT6357_POWER_LDO_VCN33_BT,
	MT6357_POWER_LDO_VCN33_WIFI,
	MT6357_POWER_LDO_VCN28,
	MT6357_POWER_LDO_VCN18,
	MT6357_POWER_LDO_VCAMA,
	MT6357_POWER_LDO_VCAMD,
	MT6357_POWER_LDO_VCAMIO,
	MT6357_POWER_LDO_VLDO28,
	MT6357_POWER_LDO_VSRAM_OTHERS,
	MT6357_POWER_LDO_VSRAM_PROC,
	MT6357_POWER_LDO_VAUX18,
	MT6357_POWER_LDO_VAUD28,
	MT6357_POWER_LDO_VIO28,
	MT6357_POWER_LDO_VIO18,
	MT6357_POWER_LDO_VDRAM,
	MT6357_POWER_LDO_VMC,
	MT6357_POWER_LDO_VMCH,
	MT6357_POWER_LDO_VEMC,
	MT6357_POWER_LDO_VSIM1,
	MT6357_POWER_LDO_VSIM2,
	MT6357_POWER_LDO_VIBR,
	MT6357_POWER_LDO_VUSB33,
	MT6357_LDO_COUNT_END
};

#endif				/* _MT_REGULATOR_CODEGEN_H_ */
