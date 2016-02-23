/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

/*
 * struct esoc_desc: Describes an external soc
 * @name: external soc name
 * @priv: private data for external soc
 */
struct esoc_desc {
	const char *name;
	const char *link;
	void *priv;
};

#ifdef CONFIG_ESOC_CLIENT
/* Can return probe deferral */
struct esoc_desc *devm_register_esoc_client(struct device *dev,
							const char *name);
void devm_unregister_esoc_client(struct device *dev,
						struct esoc_desc *esoc_desc);
int esoc_register_client_notifier(struct notifier_block *nb);
#else
static inline struct esoc_desc *devm_register_esoc_client(struct device *dev,
							const char *name)
{
	return NULL;
}
static inline void devm_unregister_esoc_client(struct device *dev,
						struct esoc_desc *esoc_desc)
{
	return;
}
static inline int esoc_register_client_notifier(struct notifier_block *nb)
{
	return -EIO;
}
#endif
#endif
