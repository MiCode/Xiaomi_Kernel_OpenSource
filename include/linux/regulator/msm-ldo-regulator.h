/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#ifndef __MSM_LDO_REGULATOR_H__
#define __MSM_LDO_REGULATOR_H__

/**
 * enum msm_ldo_supply_mode - supported operating modes by this regulator type.
 * Use negative logic to ensure BHS mode is treated as the safe default by the
 * the regulator framework. This is necessary since LDO mode can only be enabled
 * when several constraints are satisfied. Consumers of this regulator are
 * expected to request changes in operating modes through the use of
 * regulator_allow_bypass() passing in the desired LDO supply mode.
 * %BHS_MODE:	to select BHS as operating mode
 * %LDO_MODE:	to select LDO as operating mode
 */
enum msm_ldo_supply_mode {
	BHS_MODE = false,
	LDO_MODE = true,
};

#endif /* __MSM_LDO_REGULATOR_H__ */
