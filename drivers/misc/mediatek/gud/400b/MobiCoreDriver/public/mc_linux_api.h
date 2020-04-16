/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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
#ifndef _MC_LINUX_API_H_
#define _MC_LINUX_API_H_

#include <linux/types.h>

/*
 * Switch TEE active core to core_num, defined as linux
 * core id
 */
int mc_switch_core(int core_num);

/*
 * Return TEE active core as Linux core id
 */
int mc_active_core(void);

#endif /* _MC_LINUX_API_H_ */
