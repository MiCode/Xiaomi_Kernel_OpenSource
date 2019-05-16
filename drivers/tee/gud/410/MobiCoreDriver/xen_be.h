/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
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
