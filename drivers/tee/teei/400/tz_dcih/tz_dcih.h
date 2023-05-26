/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
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

#ifndef _TZ_DCIH_H_
#define _TZ_DCIH_H_

#include <linux/completion.h>

#include <ut_drv.h>

#define MAX_DCIH_BUF_SIZE SZ_4K

enum {
	DCIH_MODE_SLAVE,
	DCIH_MODE_MASTER,
	DCIH_MODE_INVALID,
};

struct dcih_reg_info {
	struct ut_drv_entry *drv_info;
	uint32_t mode;
	uint32_t buf_size;
	unsigned long virt_addr;
	unsigned long phy_addr;
	struct completion wait_notify; /* only for slave mode */
	struct completion wait_result; /* only for slave mode */
	struct list_head list;
};

void init_dcih_service(void);
int tz_create_share_buffer(unsigned int driver_id, unsigned int buff_size);
int tz_free_share_buffer(unsigned int driver_id);
int tz_notify_ree_handler(unsigned int driver_id);

#endif
