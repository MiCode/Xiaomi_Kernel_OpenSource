/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2019, The Linux Foundation. All rights reserved.
 *
 * RMNET_CTL header
 *
 */

#ifndef _RMNET_CTL_H_
#define _RMNET_CTL_H_

#include <linux/skbuff.h>

enum rmnet_ctl_log_lvl {
	RMNET_CTL_LOG_CRIT,
	RMNET_CTL_LOG_ERR,
	RMNET_CTL_LOG_INFO,
	RMNET_CTL_LOG_DEBUG,
};

#define rmnet_ctl_log_err(msg, rc, data, len) \
		rmnet_ctl_log(RMNET_CTL_LOG_ERR, msg, rc, data, len)

#define rmnet_ctl_log_info(msg, data, len) \
		rmnet_ctl_log(RMNET_CTL_LOG_INFO, msg, 0, data, len)

#define rmnet_ctl_log_debug(msg, data, len) \
		rmnet_ctl_log(RMNET_CTL_LOG_DEBUG, msg, 0, data, len)

struct rmnet_ctl_client_hooks {
	void (*ctl_dl_client_hook)(struct sk_buff *skb);
};

#ifdef CONFIG_RMNET_CTL

void *rmnet_ctl_register_client(struct rmnet_ctl_client_hooks *hook);
int rmnet_ctl_unregister_client(void *handle);
int rmnet_ctl_send_client(void *handle, struct sk_buff *skb);
void rmnet_ctl_log(enum rmnet_ctl_log_lvl lvl, const char *msg,
		   int rc, const void *data, unsigned int len);

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

static inline void rmnet_ctl_log(enum rmnet_ctl_log_lvl lvl, const char *msg,
				 int rc, const void *data, unsigned int len)
{
}

#endif /* CONFIG_RMNET_CTL */

#endif /* _RMNET_CTL_H_ */
