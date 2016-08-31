/*
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __IIO_LS_DT_H__
#define __IIO_LS_DT_H__

#include <linux/iio/iio.h>

struct lightsensor_spec;

#ifdef CONFIG_LS_OF
extern struct lightsensor_spec *of_get_ls_spec(struct device *);
#else
static inline struct lightsensor_spec *of_get_ls_spec(struct device *dev)
{
	return NULL;
}
#endif


#endif
