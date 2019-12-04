/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _RMNET_QMI_H
#define _RMNET_QMI_H

#include <linux/netdevice.h>
#include <linux/skbuff.h>

void rmnet_map_tx_qmap_cmd(struct sk_buff *qmap_skb);

#ifdef CONFIG_QCOM_QMI_RMNET
void *rmnet_get_qmi_pt(void *port);
void *rmnet_get_qos_pt(struct net_device *dev);
void *rmnet_get_rmnet_port(struct net_device *dev);
struct net_device *rmnet_get_rmnet_dev(void *port, u8 mux_id);
void rmnet_reset_qmi_pt(void *port);
void rmnet_init_qmi_pt(void *port, void *qmi);
void rmnet_enable_all_flows(void *port);
bool rmnet_all_flows_enabled(void *port);
void rmnet_set_powersave_format(void *port);
void rmnet_clear_powersave_format(void *port);
void rmnet_get_packets(void *port, u64 *rx, u64 *tx);
int rmnet_get_powersave_notif(void *port);
struct net_device *rmnet_get_real_dev(void *port);
int rmnet_get_dlmarker_info(void *port);
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

static inline void rmnet_enable_all_flows(void *port)
{
}

static inline bool rmnet_all_flows_enabled(void *port)
{
	return true;
}

static inline void rmnet_set_port_format(void *port)
{
}

static inline void rmnet_get_packets(void *port, u64 *rx, u64 *tx)
{
}

static inline int rmnet_get_powersave_notif(void *port)
{
	return 0;
}

static inline struct net_device *rmnet_get_real_dev(void *port)
{
	return NULL;
}

static inline int rmnet_get_dlmarker_info(void *port)
{
	return 0;
}
#endif /* CONFIG_QCOM_QMI_RMNET */
#endif /*_RMNET_QMI_H*/
