/*
 * intel_whiskey_cove_plus_pmic.h - Support for WhiskeyCove pmic VR
 *
 * Copyright (c) 2014, Intel Corporation.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __INTEL_WHISKEY_COVE_PMIC_H_
#define __INTEL_WHISKEY_COVE_PMIC_H_

#include <linux/regulator/driver.h>

enum WCOVE_REGULATOR_ID {
	WCOVE_ID_V1P8A = 1,
	WCOVE_ID_V1P05A,
	WCOVE_ID_V1P15,
	WCOVE_ID_VDDQ,
	WCOVE_ID_V3P3A,
	WCOVE_ID_VPROG1A,
	WCOVE_ID_VPROG1B,
	WCOVE_ID_VPROG1F,
	WCOVE_ID_V1P8SX,
	WCOVE_ID_V1P2SX,
	WCOVE_ID_V1P2A,
	WCOVE_ID_VSDIO,
	WCOVE_ID_V2P8SX,
	WCOVE_ID_V3P3SD,
	WCOVE_ID_VPROG2D,
	WCOVE_ID_VPROG3A,
	WCOVE_ID_VPROG3B,
	WCOVE_ID_VPROG4A,
	WCOVE_ID_VPROG4B,
	WCOVE_ID_VPROG4C,
	WCOVE_ID_VPROG4D,
	WCOVE_ID_VPROG5A,
	WCOVE_ID_VPROG5B,
	WCOVE_ID_VPROG6A,
	WCOVE_ID_VPROG6B,
};

struct wcove_regulator_info {
	struct regulator_desc	desc;
	struct regulator_dev	*regulator;
	struct regulator_init_data *init_data;
	unsigned int vctl_reg;
	unsigned int vsel_reg;
	unsigned int min_mV;
	unsigned int max_mV;
	unsigned int start;
	unsigned int vsel_mask;
	unsigned int scale;
	unsigned int nvolts;
	unsigned int vctl_mask;
	unsigned int reg_enbl_mask;
	unsigned int reg_dsbl_mask;
	unsigned int *vtable;
	bool runtime_table;
};

#endif /* __INTEL_WHISKEY_COVE_PMIC_H_ */
