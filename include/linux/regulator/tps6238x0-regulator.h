/*
 * tps6238x0-regulator.h -- TI TPS623850/TPS623860/TPS623870
 *
 * Interface for regulator driver for TI TPS623850/TPS623860/TPS623870
 * Processor core supply
 *
 * Copyright (C) 2012 NVIDIA Corporation

 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA	02110-1301, USA.
 *
 */

#ifndef __LINUX_REGULATOR_TPS6238X0_H
#define __LINUX_REGULATOR_TPS6238X0_H

/*
 * struct tps6238x0_regulator_platform_data - tps62360 regulator platform data.
 *
 * @init_data: The regulator init data.
 * @en_internal_pulldn: internal pull down enable or not.
 * @vsel_gpio: Gpio number for vsel. It should be -1 if this is tied with
 *              fixed logic.
 * @vsel_def_state: Default state of vsel. 1 if it is high else 0.
 */
struct tps6238x0_regulator_platform_data {
	struct regulator_init_data *init_data;
	bool en_internal_pulldn;
	int vsel_gpio;
	int vsel_def_state;
};

#endif /* __LINUX_REGULATOR_TPS6238X0_H */
