/* Copyright (c) 2010-2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LINUX_BQ27520_H
#define __LINUX_BQ27520_H
struct bq27520_platform_data {
	const char *name;
	unsigned int soc_int;
	unsigned int bi_tout;
	unsigned int chip_en; /* CE */
	const char *vreg_name; /* regulater used by bq27520 */
	int vreg_value; /* its value */
	int enable_dlog; /* if enable on-chip coulomb counter data logger */
};

#endif /* __LINUX_BQ27520_H */
