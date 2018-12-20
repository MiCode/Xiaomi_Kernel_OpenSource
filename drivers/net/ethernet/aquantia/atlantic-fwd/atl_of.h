/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _ATL_OF_H_
#define _ATL_OF_H_

#include <linux/device.h>

#ifdef CONFIG_OF

int atl_parse_dt(struct device *dev);

#else

static inline int atl_parse_dt(struct device *dev)
{
	return 0;
}

#endif // CONFIG_OF

#endif // _ATL_OF_H_
