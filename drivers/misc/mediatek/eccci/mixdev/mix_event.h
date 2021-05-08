/*
 * Copyright (C) 2017 MediaTek Inc.
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

#ifndef __MIX_EVENT_H__
#define __MIX_EVENT_H__

#include <linux/ioctl.h>

#define MIXDEV_IOC_MAGIC 'M'

extern void inject_mix_event(struct sk_buff *skb,
			   struct net_device *dev,
			   struct iphdr *iph);



#endif /* __MIX_EVENT_H__ */
