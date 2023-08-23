/* Copyright (c) 2014, 2017-2019, The Linux Foundation. All rights reserved.
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
#ifndef __ESOC_CLIENT_H_
#define __ESOC_CLIENT_H_

#include <linux/device.h>
#include <linux/esoc_ctrl.h>
#include <linux/notifier.h>

enum esoc_client_hook_prio {
	ESOC_MHI_HOOK,
	ESOC_CNSS_HOOK,
	ESOC_MAX_HOOKS
};

struct esoc_link_data {
	enum esoc_client_hook_prio prio;
	__u64 link_id;
};

/* Flag values used with the power_on and power_off hooks */
#define ESOC_HOOK_MDM_CRASH	0x0001 /* In crash handling path */
#define ESOC_HOOK_MDM_DOWN	0x0002 /* MDM about to go down */

struct esoc_client_hook {
	char *name;
	void *priv;
	enum esoc_client_hook_prio prio;
	int (*esoc_link_power_on)(void *priv, unsigned int flags);
	void (*esoc_link_power_off)(void *priv, unsigned int flags);
	u64 (*esoc_link_get_id)(void *priv);
	void (*esoc_link_mdm_crash)(void *priv);
};

/*
 * struct esoc_desc: Describes an external soc
 * @name: external soc name
 * @priv: private data for external soc
 */
struct esoc_desc {
	const char *name;
	const char *link;
	const char *link_info;
	void *priv;
};

#ifdef CONFIG_ESOC_CLIENT
/* Can return probe deferral */
struct esoc_desc *devm_register_esoc_client(struct device *dev,
							const char *name);
void devm_unregister_esoc_client(struct device *dev,
						struct esoc_desc *esoc_desc);
int esoc_register_client_notifier(struct notifier_block *nb);
int esoc_register_client_hook(struct esoc_desc *desc,
				struct esoc_client_hook *client_hook);
int esoc_unregister_client_hook(struct esoc_desc *desc,
				struct esoc_client_hook *client_hook);
#else
static inline struct esoc_desc *devm_register_esoc_client(struct device *dev,
							const char *name)
{
	return NULL;
}
static inline void devm_unregister_esoc_client(struct device *dev,
						struct esoc_desc *esoc_desc)
{
}
static inline int esoc_register_client_notifier(struct notifier_block *nb)
{
	return -EIO;
}
static inline int esoc_register_client_hook(struct esoc_desc *desc,
				struct esoc_client_hook *client_hook)
{
	return -EIO;
}
static inline int esoc_unregister_client_hook(struct esoc_desc *desc,
				struct esoc_client_hook *client_hook)
{
	return -EIO;
}
#endif
#endif
