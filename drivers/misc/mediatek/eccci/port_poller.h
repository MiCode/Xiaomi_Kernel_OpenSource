/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __PORT_POLLER_H__
#define __PORT_POLLER_H__

#include "ccci_core.h"

struct port_md_status_poller {
	unsigned long long latest_poll_start_time;
	unsigned int md_status_poller_flag;
	struct timer_list md_status_poller;
	struct timer_list md_status_timeout;
};
#endif	/* __PORT_POLLER_H__ */
