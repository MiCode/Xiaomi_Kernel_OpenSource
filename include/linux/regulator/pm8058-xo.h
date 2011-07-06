/* Copyright (c) 2011, The Linux Foundation. All rights reserved.
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

#ifndef __PM8058_XO_H__
#define __PM8058_XO_H__

#include <linux/regulator/machine.h>

#define PM8058_XO_BUFFER_DEV_NAME	"pm8058-xo-buffer"

/* XO buffer control ids */
#define PM8058_XO_ID_A0		0
#define PM8058_XO_ID_A1		1

#define PM8058_XO_ID_MAX	(PM8058_XO_ID_A1 + 1)

struct pm8058_xo_pdata {
	struct regulator_init_data	init_data;
	int id;
};

#endif
