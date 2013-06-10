/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _LINUX_REGULATOR_PROXY_CONSUMER_H_
#define _LINUX_REGULATOR_PROXY_CONSUMER_H_

#include <linux/device.h>
#include <linux/of.h>

struct proxy_consumer;

#ifdef CONFIG_REGULATOR_PROXY_CONSUMER

struct proxy_consumer *regulator_proxy_consumer_register(struct device *reg_dev,
			struct device_node *reg_node);

int regulator_proxy_consumer_unregister(struct proxy_consumer *consumer);

#else

static inline struct proxy_consumer *regulator_proxy_consumer_register(
			struct device *reg_dev, struct device_node *reg_node)
{ return NULL; }

static inline int regulator_proxy_consumer_unregister(
			struct proxy_consumer *consumer)
{ return 0; }

#endif

#endif
