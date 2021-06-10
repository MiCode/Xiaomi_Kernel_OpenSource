/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017-2019 TRUSTONIC LIMITED
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

#ifndef _MC_XEN_FE_H_
#define _MC_XEN_FE_H_

#include <linux/version.h>

#include "main.h"
#include "client.h"
#include "protocol_common.h"

#ifdef CONFIG_XEN
struct tee_protocol_ops *xen_fe_check(void);
#else
static inline
struct tee_protocol_ops *xen_fe_check(void)
{
	return NULL;
}
#endif

#endif /* _MC_XEN_FE_H_ */
