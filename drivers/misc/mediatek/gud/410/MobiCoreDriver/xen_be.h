/*
 * Copyright (c) 2017 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MC_XEN_BE_H_
#define _MC_XEN_BE_H_

#include <linux/version.h>

struct xen_be_map;

#ifdef CONFIG_XEN
int xen_be_init(void);
void xen_be_exit(void);
#else
static inline int xen_be_init(void)
{
	return 0;
}

static inline void xen_be_exit(void)
{
}
#endif

#endif /* _MC_XEN_BE_H_ */
