/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * RMNET_CTL header
 *
 */

#ifndef _RMNET_CTL_H_
#define _RMNET_CTL_H_

#include <linux/skbuff.h>

struct rmnet_ctl_client_hooks {
	void (*ctl_dl_client_hook)(struct sk_buff *skb);
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
