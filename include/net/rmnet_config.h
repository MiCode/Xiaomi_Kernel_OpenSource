/*
 * Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * RMNET Data config definition
 *
 */

#ifndef _RMNET_CONFIG_H_
#define _RMNET_CONFIG_H_

#include <linux/skbuff.h>

struct rmnet_phys_ep_conf_s {
	void (*recycle)(struct sk_buff *); /* Destruct function */
	void *config;
};

#endif /* _RMNET_CONFIG_H_ */
