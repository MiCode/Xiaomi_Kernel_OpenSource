/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#ifndef _RMNET_QMI_H
#define _RMNET_QMI_H

#include <linux/netdevice.h>
#ifdef CONFIG_QCOM_QMI_DFC
void *rmnet_get_qmi_pt(void *port);
void *rmnet_get_qos_pt(struct net_device *dev);
void *rmnet_get_rmnet_port(struct net_device *dev);
struct net_device *rmnet_get_rmnet_dev(void *port, u8 mux_id);
void rmnet_reset_qmi_pt(void *port);
void rmnet_init_qmi_pt(void *port, void *qmi);
#else
static inline void *rmnet_get_qmi_pt(void *port)
{
	return NULL;
}

static inline void *rmnet_get_qos_pt(struct net_device *dev)
{
	return NULL;
}

static inline void *rmnet_get_rmnet_port(struct net_device *dev)
{
	return NULL;
}

static inline struct net_device *rmnet_get_rmnet_dev(void *port,
						     u8 mux_id)
{
	return NULL;
}

static inline void rmnet_reset_qmi_pt(void *port)
{
}

static inline void rmnet_init_qmi_pt(void *port, void *qmi)
{
}
#endif /* CONFIG_QCOM_QMI_DFC */
#endif /*_RMNET_QMI_H*/
