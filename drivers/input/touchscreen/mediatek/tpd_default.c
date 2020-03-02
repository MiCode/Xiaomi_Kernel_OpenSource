/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include "tpd.h"

/* #ifndef TPD_CUSTOM_TREMBLE_TOLERANCE */
int tpd_trembling_tolerance(int t, int p)
{
	if (t > 5 || p > 120)
		return 200;
	if (p > 90)
		return 64;
	if (p > 80)
		return 36;
	return 26;
}

/* #endif */
