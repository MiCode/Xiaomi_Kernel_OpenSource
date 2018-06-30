/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2014, 2017-2018, The Linux Foundation. All rights reserved.
 */
#ifndef __ESOC_CLIENT_H_
#define __ESOC_CLIENT_H_

#include <linux/device.h>
#include <linux/esoc_ctrl.h>
#include <linux/notifier.h>

struct esoc_client_hook {
	char *name;
	void *priv;
	enum esoc_client_hook_prio prio;
	int (*esoc_link_power_on)(void *priv, bool mdm_crashed);
	void (*esoc_link_power_off)(void *priv, bool mdm_crashed);
	u64 (*esoc_link_get_id)(void *priv);
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
