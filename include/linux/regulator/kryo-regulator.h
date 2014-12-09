/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __KRYO_REGULATOR_H__
#define __KRYO_REGULATOR_H__

/**
 * enum kryo_supply_mode - supported operating modes by this regulator type.
 * Use negative logic to ensure BHS mode is treated as the safe default by the
 * the regulator framework. This is necessary since LDO mode can only be enabled
 * when several constraints are satisfied. Consumers of this regulator are
 * expected to request changes in operating modes through the use of
 * regulator_allow_bypass() passing in the desired Kryo supply mode.
 * %BHS_MODE:	to select BHS as operating mode
 * %LDO_MODE:	to select LDO as operating mode
 */
enum kryo_supply_mode {
	BHS_MODE = false,
	LDO_MODE = true,
};

#endif /* __KRYO_REGULATOR_H__ */
