/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019-2020 TRUSTONIC LIMITED
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

#ifndef MC_VLX_FE_H
#define MC_VLX_FE_H

#include "protocol_common.h"

#ifdef CONFIG_VLX_HYP
struct tee_protocol_ops *vlx_fe_check(void);
#else /* MC_USE_VLX_PMEM */
static inline
struct tee_protocol_ops *vlx_fe_check(void)
{
	return NULL;
}
#endif /* MC_USE_VLX_PMEM */

#endif /* MC_VLX_FE_H */
