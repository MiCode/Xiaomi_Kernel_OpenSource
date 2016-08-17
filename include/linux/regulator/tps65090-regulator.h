
/*
 * Regulator driver interface for TI TPS65090 PMIC family
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __REGULATOR_TPS65090_H
#define __REGULATOR_TPS65090_H

#include <linux/regulator/machine.h>

#define tps65090_rails(_name) "tps65090_"#_name

enum {
	TPS65090_REGULATOR_DCDC1,
	TPS65090_REGULATOR_DCDC2,
	TPS65090_REGULATOR_DCDC3,
	TPS65090_REGULATOR_LDO1,
	TPS65090_REGULATOR_LDO2,
	TPS65090_REGULATOR_FET1,
	TPS65090_REGULATOR_FET2,
	TPS65090_REGULATOR_FET3,
	TPS65090_REGULATOR_FET4,
	TPS65090_REGULATOR_FET5,
	TPS65090_REGULATOR_FET6,
	TPS65090_REGULATOR_FET7,
};

/*
 * struct tps65090_regulator_platform_data
 *
 * @reg_init_data: The regulator init data.
 * @id: Regulator ID.
 * @enable_ext_control: Enable extrenal control or not. Only available for
 *	DCDC1, DCDC2 and DCDC3.
 * @gpio: Gpio number if external control is enabled and controlled through
 *	gpio.
 * @wait_timeout_us: wait timeout in microseconds;
 *	>0 : specify minimum wait timeout in us for FETx, will update WTFET[1:0]
 *	     in FETx_CTRL reg;
 *	 0 : not to update WTFET[1:0] in FETx_CTRL reg for FETx;
 *	-1 : for non-FETx.
 */

struct tps65090_regulator_platform_data {
	int id;
	bool enable_ext_control;
	int gpio;
	struct regulator_init_data *reg_init_data;
	int wait_timeout_us;
};

#endif	/* __REGULATOR_TPS65090_H */
