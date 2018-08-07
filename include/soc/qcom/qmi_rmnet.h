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

#ifndef _QMI_RMNET_H
#define _QMI_RMNET_H

#include <linux/netdevice.h>
#include <linux/skbuff.h>

#ifdef CONFIG_QCOM_QMI_RMNET
void qmi_rmnet_qmi_exit(void *qmi_pt, void *port);
void qmi_rmnet_change_link(struct net_device *dev, void *port, void *tcm_pt);
#else
static inline void qmi_rmnet_qmi_exit(void *qmi_pt, void *port)
{
}

static inline void
qmi_rmnet_change_link(struct net_device *dev, void *port, void *tcm_pt)
{
}
#endif

#ifdef CONFIG_QCOM_QMI_DFC
void *qmi_rmnet_qos_init(struct net_device *real_dev, u8 mux_id);
void qmi_rmnet_qos_exit(struct net_device *dev);
void qmi_rmnet_burst_fc_check(struct net_device *dev, struct sk_buff *skb);
#else
static inline void *
qmi_rmnet_qos_init(struct net_device *real_dev, u8 mux_id)
{
	return NULL;
}

static inline void qmi_rmnet_qos_exit(struct net_device *dev)
{
}

static inline void
qmi_rmnet_burst_fc_check(struct net_device *dev, struct sk_buff *skb)
{
}
#endif

#ifdef CONFIG_QCOM_QMI_POWER_COLLAPSE
int qmi_rmnet_set_powersave_mode(void *port, uint8_t enable);
void qmi_rmnet_work_init(void *port);
void qmi_rmnet_work_exit(void *port);
int qmi_rmnet_work_get_active(void *port);
void qmi_rmnet_work_set_active(void *port, int status);
void qmi_rmnet_work_restart(void *port);
#else
static inline int qmi_rmnet_set_powersave_mode(void *port, uint8_t enable)
{
	return 0;
}
static inline void qmi_rmnet_work_init(void *port)
{
}
static inline void qmi_rmnet_work_restart(void *port)
{

}
static inline void qmi_rmnet_work_exit(void *port)
{
}

static inline int  qmi_rmnet_work_get_active(void *port)
{
	return 0;

}
static inline void qmi_rmnet_work_set_active(void *port, int status)
{

}
#endif
#endif /*_QMI_RMNET_H*/
