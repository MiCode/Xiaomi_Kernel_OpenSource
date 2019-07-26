/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
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

#ifndef _RMNET_CTL_H_
#define _RMNET_CTL_H_

#include <linux/skbuff.h>

struct rmnet_ctl_client_hooks {
	void (*ctl_dl_client_hook)(struct sk_buff *);
};

#ifdef CONFIG_RMNET_CTL

void *rmnet_ctl_register_client(struct rmnet_ctl_client_hooks *hook);
int rmnet_ctl_unregister_client(void *handle);
int rmnet_ctl_send_client(void *handle, struct sk_buff *skb);

#else

static inline void *rmnet_ctl_register_client(
			struct rmnet_ctl_client_hooks *hook)
{
	return NULL;
}

static inline int rmnet_ctl_unregister_client(void *handle)
{
	return -EINVAL;
}

static inline int rmnet_ctl_send_client(void *handle, struct sk_buff *skb)
{
	return -EINVAL;
}

#endif /* CONFIG_RMNET_CTL */

#endif /* _RMNET_CTL_H_ */
