/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef _LINUX_EDPDEV_H
#define _LINUX_EDPDEV_H

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/edp.h>

/*
 * Temperature -> IBAT LUT
 * Should be descending wrt temp
 * { ..., .ibat = 0 } must be the last entry
 */
struct psy_depletion_ibat_lut {
	int temp;
	unsigned int ibat;
};

/*
 * Capacity -> RBAT LUT
 * Should be descending wrt capacity
 * { .capacity = 0, ... } must be the last entry
 */
struct psy_depletion_rbat_lut {
	unsigned int capacity;
	unsigned int rbat;
};

/*
 * Capacity -> OCV LUT
 * Should be descending wrt capacity
 * { .capacity = 0, ... } must be the last entry
 * @capacity: battery capacity in percents
 * @ocv: OCV in uV
 */
struct psy_depletion_ocv_lut {
	unsigned int capacity;
	unsigned int ocv;
};

/* Power supply depletion EDP client */
struct psy_depletion_platform_data {
	char *power_supply;
	unsigned int *states;
	unsigned int num_states;
	unsigned int e0_index;
	unsigned int r_const;
	unsigned int vsys_min;
	unsigned int vcharge;
	unsigned int ibat_nom;
	struct psy_depletion_ibat_lut *ibat_lut;
	struct psy_depletion_rbat_lut *rbat_lut;
	struct psy_depletion_ocv_lut *ocv_lut;
};

#endif
