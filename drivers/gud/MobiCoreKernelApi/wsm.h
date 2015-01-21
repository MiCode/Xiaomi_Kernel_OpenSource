/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
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
/*
 * World shared memory definitions.
 */
#ifndef _MC_KAPI_WSM_H_
#define _MC_KAPI_WSM_H_

#include "common.h"
#include <linux/list.h>

struct wsm {
	void			*virt_addr;
	uint32_t		len;
	uint32_t		handle;
	struct list_head	list;
};

#endif /* _MC_KAPI_WSM_H_ */
