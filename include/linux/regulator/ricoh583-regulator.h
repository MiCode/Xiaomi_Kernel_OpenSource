/*
 * linux/regulator/ricoh583-regulator.h
 *
 * Interface for regulator driver for RICOH583 power management chip.
 *
 * Copyright (C) 2011 NVIDIA Corporation
 *
 * Copyright (C) 2011 RICOH COMPANY,LTD
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
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

#ifndef __LINUX_REGULATOR_RICOH583_H
#define __LINUX_REGULATOR_RICOH583_H

#include <linux/regulator/machine.h>


#define ricoh583_rails(_name) "RICOH583_"#_name

/* RICHOH Regulator IDs */
enum regulator_id {
	RICOH583_ID_DC0,
	RICOH583_ID_DC1,
	RICOH583_ID_DC2,
	RICOH583_ID_DC3,
	RICOH583_ID_LDO0,
	RICOH583_ID_LDO1,
	RICOH583_ID_LDO2,
	RICOH583_ID_LDO3,
	RICOH583_ID_LDO4,
	RICOH583_ID_LDO5,
	RICOH583_ID_LDO6,
	RICOH583_ID_LDO7,
	RICOH583_ID_LDO8,
	RICOH583_ID_LDO9,
};

struct ricoh583_regulator_platform_data {
		struct regulator_init_data regulator;
		int init_uV;
		unsigned init_enable:1;
		unsigned init_apply:1;
		int deepsleep_uV;
		int deepsleep_slots;
		unsigned long ext_pwr_req;
		unsigned long flags;
};

#endif
