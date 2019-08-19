/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include <soc/qcom/qmi_rmnet.h>
#include <soc/qcom/rmnet_qmi.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/rtnetlink.h>
#include <uapi/linux/rtnetlink.h>
#include <net/pkt_sched.h>
#include "qmi_rmnet_i.h"
#include <trace/events/dfc.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/ip.h>
#include <linux/ipv6.h>

#define NLMSG_FLOW_ACTIVATE 1
#define NLMSG_FLOW_DEACTIVATE 2
#define NLMSG_CLIENT_SETUP 4
#define NLMSG_CLIENT_DELETE 5

#define FLAG_DFC_MASK 0x000F
#define FLAG_POWERSAVE_MASK 0x0010
#define FLAG_QMAP_MASK 0x0020

#define FLAG_TO_MODE(f) ((f) & FLAG_DFC_MASK)
#define DFC_SUPPORTED_MODE(m) \
	((m) == DFC_MODE_FLOW_ID || (m) == DFC_MODE_MQ_NUM)
#define FLAG_TO_QMAP(f) ((f) & FLAG_QMAP_MASK)

int dfc_mode;
int dfc_qmap;
#define IS_ANCILLARY(type) ((type) != AF_INET && (type) != AF_INET6)

unsigned int rmnet_wq_frequency __read_mostly = 1000;
module_param(rmnet_wq_frequency, uint, 0644);
MODULE_PARM_DESC(rmnet_wq_frequency, "Frequency of PS check in ms");

#define PS_WORK_ACTIVE_BIT 0
#define PS_INTERVAL (((!rmnet_wq_frequency) ?                             \
					1 : rmnet_wq_frequency/10) * (HZ/100))
#define NO_DELAY (0x0000 * HZ)

#ifdef CONFIG_QCOM_QMI_DFC
static unsigned int qmi_rmnet_scale_factor = 5;
#endif

static int
qmi_rmnet_del_flow(struct net_device *dev, struct tcmsg *tcm,
		   struct qmi_info *qmi);

struct qmi_elem_info data_ep_id_type_v01_ei[] = {
	{
		.data_type	= QMI_SIGNED_4_BYTE_ENUM,
		.elem_len	= 1,
		.elem_size	= sizeof(enum data_ep_type_enum_v01),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct data_ep_id_type_v01,
					   ep_type),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_UNSIGNED_4_BYTE,
		.elem_len	= 1,
		.elem_size	= sizeof(u32),
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= offsetof(struct data_ep_id_type_v01,
					   iface_id),
		.ei_array	= NULL,
	},
	{
		.data_type	= QMI_EOTI,
		.elem_len	= 0,
		.elem_size	= 0,
		.is_array	= NO_ARRAY,
		.tlv_type	= QMI_COMMON_TLV_TYPE,
		.offset		= 0,
		.ei_array	= NULL,
	},
};
EXPORT_SYMBOL(data_ep_id_type_v01_ei);

void *qmi_rmnet_has_dfc_client(struct qmi_info *qmi)
{
	int i;

	if (!qmi)
		return NULL;

	for (i = 0; i < MAX_CLIENT_NUM; i++) {
		if (qmi->dfc_clients[i])
			return qmi->dfc_clients[i];
	}

	return NULL;
}

static inline int
qmi_rmnet_has_client(struct qmi_info *qmi)
{
	if (qmi->wda_client)
		return 1;

	return qmi_rmnet_has_dfc_client(qmi) ? 1 : 0;
}

static int
qmi_rmnet_has_pending(struct qmi_info *qmi)
{
	int i;

	if (qmi->wda_pending)
		return 1;

	for (i = 0; i < MAX_CLIENT_NUM; i++) {
		if (qmi->dfc_pending[i])
			return 1;
	}

	return 0;
}

#ifdef CONFIG_QCOM_QMI_DFC
static void
qmi_rmnet_clean_flow_list(struct qmi_info *qmi, struct net_device *dev,
			  struct qos_info *qos)
{
	struct rmnet_bearer_map *bearer, *br_tmp;
	struct rmnet_flow_map *itm, *fl_tmp;

	ASSERT_RTNL();

	list_for_each_entry_safe(itm, fl_tmp, &qos->flow_head, list) {
		list_del(&itm->list);
		kfree(itm);
	}

	list_for_each_entry_safe(bearer, br_tmp, &qos->bearer_head, list) {
		list_del(&bearer->list);
		kfree(bearer);
	}

	memset(qos->mq, 0, sizeof(qos->mq));
}

struct rmnet_flow_map *
qmi_rmnet_get_flow_map(struct qos_info *qos, u32 flow_id, int ip_type)
{
	struct rmnet_flow_map *itm;

	if (!qos)
		return NULL;

	list_for_each_entry(itm, &qos->flow_head, list) {
		if ((itm->flow_id == flow_id) && (itm->ip_type == ip_type))
			return itm;
	}
	return NULL;
}

struct rmnet_bearer_map *
qmi_rmnet_get_bearer_map(struct qos_info *qos, uint8_t bearer_id)
{
	struct rmnet_bearer_map *itm;

	if (!qos)
		return NULL;

	list_for_each_entry(itm, &qos->bearer_head, list) {
		if (itm->bearer_id == bearer_id)
			return itm;
	}
	return NULL;
}

static void qmi_rmnet_update_flow_map(struct rmnet_flow_map *itm,
				      struct rmnet_flow_map *new_map)
{
	itm->bearer_id = new_map->bearer_id;
	itm->flow_id = new_map->flow_id;
	itm->ip_type = new_map->ip_type;
	itm->mq_idx = new_map->mq_idx;
}

int qmi_rmnet_flow_control(struct net_device *dev, u32 mq_idx, int enable)
{
	struct netdev_queue *q;

	if (unlikely(mq_idx >= dev->num_tx_queues))
		return 0;

	q = netdev_get_tx_queue(dev, mq_idx);
	if (unlikely(!q))
		return 0;

	if (enable)
		netif_tx_wake_queue(q);
	else
		netif_tx_stop_queue(q);

	return 0;
}

static void qmi_rmnet_reset_txq(struct net_device *dev, unsigned int txq)
{
	struct Qdisc *qdisc;

	if (unlikely(txq >= dev->num_tx_queues))
		return;

	qdisc = rtnl_dereference(netdev_get_tx_queue(dev, txq)->qdisc);
	if (qdisc) {
		spin_lock_bh(qdisc_lock(qdisc));
		qdisc_reset(qdisc);
		spin_unlock_bh(qdisc_lock(qdisc));
	}
}

static int qmi_rmnet_add_flow(struct net_device *dev, struct tcmsg *tcm,
			      struct qmi_info *qmi)
{
	struct qos_info *qos_info = (struct qos_info *)rmnet_get_qos_pt(dev);
	struct rmnet_flow_map new_map, *itm;
	struct rmnet_bearer_map *bearer;
	struct tcmsg tmp_tcm;
	struct mq_map *mq;
	u32 mq_idx;

	if (!qos_info || !tcm || tcm->tcm_handle >= MAX_MQ_NUM)
		return -EINVAL;

	ASSERT_RTNL();

	/* flow activate
	 * tcm->tcm__pad1 - bearer_id, tcm->tcm_parent - flow_id,
	 * tcm->tcm_ifindex - ip_type, tcm->tcm_handle - mq_idx
	 */

	new_map.bearer_id = tcm->tcm__pad1;
	new_map.flow_id = tcm->tcm_parent;
	new_map.ip_type = tcm->tcm_ifindex;
	new_map.mq_idx = tcm->tcm_handle;
	trace_dfc_flow_info(dev->name, new_map.bearer_id, new_map.flow_id,
			    new_map.ip_type, new_map.mq_idx, 1);

again:
	spin_lock_bh(&qos_info->qos_lock);

	itm = qmi_rmnet_get_flow_map(qos_info, new_map.flow_id,
				     new_map.ip_type);
	if (itm) {
		pr_debug("%s: stale flow found\n", __func__);
		tmp_tcm.tcm__pad1 = itm->bearer_id;
		tmp_tcm.tcm_parent = itm->flow_id;
		tmp_tcm.tcm_ifindex = itm->ip_type;
		tmp_tcm.tcm_handle = itm->mq_idx;
		spin_unlock_bh(&qos_info->qos_lock);
		qmi_rmnet_del_flow(dev, &tmp_tcm, qmi);
		goto again;
	}

	/* Create flow map */
	itm = kzalloc(sizeof(*itm), GFP_ATOMIC);
	if (!itm) {
		spin_unlock_bh(&qos_info->qos_lock);
		return -ENOMEM;
	}

	qmi_rmnet_update_flow_map(itm, &new_map);
	list_add(&itm->list, &qos_info->flow_head);

	/* Create or update bearer map */
	bearer = qmi_rmnet_get_bearer_map(qos_info, new_map.bearer_id);
	if (bearer) {
		bearer->flow_ref++;
	} else {
		bearer = kzalloc(sizeof(*bearer), GFP_ATOMIC);
		if (!bearer) {
			spin_unlock_bh(&qos_info->qos_lock);
			return -ENOMEM;
		}

		bearer->bearer_id = new_map.bearer_id;
		bearer->flow_ref = 1;
		bearer->grant_size = qos_info->default_grant;
		bearer->grant_thresh = qmi_rmnet_grant_per(bearer->grant_size);
		qos_info->default_grant = DEFAULT_GRANT;
		list_add(&bearer->list, &qos_info->bearer_head);
	}
	itm->bearer = bearer;

	/* Update mq map */
	mq_idx = tcm->tcm_handle;
	mq = &qos_info->mq[mq_idx];
	if (!mq->bearer) {
		mq->bearer = bearer;
		mq->ancillary = IS_ANCILLARY(new_map.ip_type);

		qmi_rmnet_flow_control(dev, mq_idx,
				       bearer->grant_size > 0 ? 1 : 0);
		trace_dfc_qmi_tc(dev->name, itm->bearer_id,
				 bearer->grant_size, 0, mq_idx,
				 bearer->grant_size > 0 ? 1 : 0);

	} else if (mq->bearer->bearer_id != new_map.bearer_id) {
		pr_debug("%s: un-managered bearer %u\n",
				__func__, new_map.bearer_id);
	}

	spin_unlock_bh(&qos_info->qos_lock);
	return 0;
}

static int
qmi_rmnet_del_flow(struct net_device *dev, struct tcmsg *tcm,
		   struct qmi_info *qmi)
{
	struct qos_info *qos_info = (struct qos_info *)rmnet_get_qos_pt(dev);
	struct rmnet_flow_map new_map, *itm;
	struct rmnet_bearer_map *bearer;
	struct mq_map *mq;
	u32 mq_idx;

	if (!qos_info)
		return -EINVAL;

	ASSERT_RTNL();

	/* flow deactivate
	 * tcm->tcm__pad1 - bearer_id, tcm->tcm_parent - flow_id,
	 * tcm->tcm_ifindex - ip_type
	 */

	spin_lock_bh(&qos_info->qos_lock);

	new_map.bearer_id = tcm->tcm__pad1;
	new_map.flow_id = tcm->tcm_parent;
	new_map.ip_type = tcm->tcm_ifindex;
	itm = qmi_rmnet_get_flow_map(qos_info, new_map.flow_id,
				     new_map.ip_type);
	if (itm) {
		trace_dfc_flow_info(dev->name, new_map.bearer_id,
				    new_map.flow_id, new_map.ip_type,
				    itm->mq_idx, 0);

		bearer = itm->bearer;
		if (bearer && --bearer->flow_ref == 0) {
			/* Remove the bearer from mq map */
			for (mq_idx = 0; mq_idx < MAX_MQ_NUM; mq_idx++) {
				mq = &qos_info->mq[mq_idx];
				if (mq->bearer != bearer)
					continue;

				mq->bearer = NULL;
				mq->ancillary = false;
				qmi_rmnet_reset_txq(dev, mq_idx);
				qmi_rmnet_flow_control(dev, mq_idx, 1);
				trace_dfc_qmi_tc(dev->name,
					new_map.bearer_id, 0, 0, mq_idx, 1);
			}

			/* Remove from bearer map */
			list_del(&bearer->list);
			kfree(bearer);
		}

		/* Remove from flow map */
		list_del(&itm->list);
		kfree(itm);
	}

	if (list_empty(&qos_info->flow_head))
		netif_tx_wake_all_queues(dev);

	spin_unlock_bh(&qos_info->qos_lock);
	return 0;
}

static void qmi_rmnet_query_flows(struct qmi_info *qmi)
{
	int i;

	for (i = 0; i < MAX_CLIENT_NUM; i++) {
		if (qmi->dfc_clients[i] && !dfc_qmap)
			dfc_qmi_query_flow(qmi->dfc_clients[i]);
	}
}

static int qmi_rmnet_set_scale_factor(const char *val,
				      const struct kernel_param *kp)
{
	int ret;
	unsigned int num = 0;

	ret = kstrtouint(val, 10, &num);
	if (ret != 0 || num == 0)
		return -EINVAL;

	return param_set_uint(val, kp);
}

static const struct kernel_param_ops qmi_rmnet_scale_ops = {
	.set	= qmi_rmnet_set_scale_factor,
	.get	= param_get_uint,
};

module_param_cb(qmi_rmnet_scale_factor, &qmi_rmnet_scale_ops,
		&qmi_rmnet_scale_factor, 0664);
#else
static inline void qmi_rmnet_clean_flow_list(struct qos_info *qos)
{
}

static inline void
qmi_rmnet_update_flow_map(struct rmnet_flow_map *itm,
			  struct rmnet_flow_map *new_map)
{
}

static inline int
qmi_rmnet_add_flow(struct net_device *dev, struct tcmsg *tcm,
		   struct qmi_info *qmi)
{
	return -EINVAL;
}

static inline int
qmi_rmnet_del_flow(struct net_device *dev, struct tcmsg *tcm,
		   struct qmi_info *qmi)
{
	return -EINVAL;
}

static inline void qmi_rmnet_query_flows(struct qmi_info *qmi)
{
}
#endif

static int
qmi_rmnet_setup_client(void *port, struct qmi_info *qmi, struct tcmsg *tcm)
{
	int idx, err = 0;
	struct svc_info svc;

	ASSERT_RTNL();

	/* client setup
	 * tcm->tcm_handle - instance, tcm->tcm_info - ep_type,
	 * tcm->tcm_parent - iface_id, tcm->tcm_ifindex - flags
	 */
	idx = (tcm->tcm_handle == 0) ? 0 : 1;

	if (!qmi) {
		qmi = kzalloc(sizeof(struct qmi_info), GFP_ATOMIC);
		if (!qmi)
			return -ENOMEM;

		rmnet_init_qmi_pt(port, qmi);
	}

	qmi->flag = tcm->tcm_ifindex;
	svc.instance = tcm->tcm_handle;
	svc.ep_type = tcm->tcm_info;
	svc.iface_id = tcm->tcm_parent;

	if (DFC_SUPPORTED_MODE(dfc_mode) &&
	    !qmi->dfc_clients[idx] && !qmi->dfc_pending[idx]) {
		if (dfc_qmap)
			err = dfc_qmap_client_init(port, idx, &svc, qmi);
		else
			err = dfc_qmi_client_init(port, idx, &svc, qmi);
	}

	if ((tcm->tcm_ifindex & FLAG_POWERSAVE_MASK) &&
	    (idx == 0) && !qmi->wda_client && !qmi->wda_pending) {
		err = wda_qmi_client_init(port, &svc, qmi);
	}

	return err;
}

static int
__qmi_rmnet_delete_client(void *port, struct qmi_info *qmi, int idx)
{
	void *data = NULL;

	ASSERT_RTNL();

	if (qmi->dfc_clients[idx])
		data = qmi->dfc_clients[idx];
	else if (qmi->dfc_pending[idx])
		data = qmi->dfc_pending[idx];

	if (data) {
		if (dfc_qmap)
			dfc_qmap_client_exit(data);
		else
			dfc_qmi_client_exit(data);
		qmi->dfc_clients[idx] = NULL;
		qmi->dfc_pending[idx] = NULL;
	}

	if (!qmi_rmnet_has_client(qmi) && !qmi_rmnet_has_pending(qmi)) {
		rmnet_reset_qmi_pt(port);
		kfree(qmi);
		return 0;
	}

	return 1;
}

static void
qmi_rmnet_delete_client(void *port, struct qmi_info *qmi, struct tcmsg *tcm)
{
	int idx;
	void *data = NULL;

	/* client delete: tcm->tcm_handle - instance*/
	idx = (tcm->tcm_handle == 0) ? 0 : 1;

	ASSERT_RTNL();
	if (qmi->wda_client)
		data = qmi->wda_client;
	else if (qmi->wda_pending)
		data = qmi->wda_pending;

	if ((idx == 0) && data) {
		wda_qmi_client_exit(data);
		qmi->wda_client = NULL;
		qmi->wda_pending = NULL;
	} else {
		qmi_rmnet_flush_ps_wq();
	}

	__qmi_rmnet_delete_client(port, qmi, idx);
}

void qmi_rmnet_change_link(struct net_device *dev, void *port, void *tcm_pt)
{
	struct qmi_info *qmi = (struct qmi_info *)rmnet_get_qmi_pt(port);
	struct tcmsg *tcm = (struct tcmsg *)tcm_pt;

	switch (tcm->tcm_family) {
	case NLMSG_FLOW_ACTIVATE:
		if (!qmi || !DFC_SUPPORTED_MODE(dfc_mode) ||
		    !qmi_rmnet_has_dfc_client(qmi))
			return;

		qmi_rmnet_add_flow(dev, tcm, qmi);
		break;
	case NLMSG_FLOW_DEACTIVATE:
		if (!qmi || !DFC_SUPPORTED_MODE(dfc_mode))
			return;

		qmi_rmnet_del_flow(dev, tcm, qmi);
		break;
	case NLMSG_CLIENT_SETUP:
		dfc_mode = FLAG_TO_MODE(tcm->tcm_ifindex);
		dfc_qmap = FLAG_TO_QMAP(tcm->tcm_ifindex);

		if (!DFC_SUPPORTED_MODE(dfc_mode) &&
		    !(tcm->tcm_ifindex & FLAG_POWERSAVE_MASK))
			return;

		if (qmi_rmnet_setup_client(port, qmi, tcm) < 0) {
			/* retrieve qmi again as it could have been changed */
			qmi = (struct qmi_info *)rmnet_get_qmi_pt(port);
			if (qmi &&
			    !qmi_rmnet_has_client(qmi) &&
			    !qmi_rmnet_has_pending(qmi)) {
				rmnet_reset_qmi_pt(port);
				kfree(qmi);
			}
		} else if (tcm->tcm_ifindex & FLAG_POWERSAVE_MASK) {
			qmi_rmnet_work_init(port);
			rmnet_set_powersave_format(port);
		}
		break;
	case NLMSG_CLIENT_DELETE:
		if (!qmi)
			return;
		if (tcm->tcm_handle == 0) { /* instance 0 */
			rmnet_clear_powersave_format(port);
			qmi_rmnet_work_exit(port);
		}
		qmi_rmnet_delete_client(port, qmi, tcm);
		break;
	default:
		pr_debug("%s(): No handler\n", __func__);
		break;
	}
}
EXPORT_SYMBOL(qmi_rmnet_change_link);

void qmi_rmnet_qmi_exit(void *qmi_pt, void *port)
{
	struct qmi_info *qmi = (struct qmi_info *)qmi_pt;
	int i;
	void *data = NULL;

	if (!qmi)
		return;

	ASSERT_RTNL();

	qmi_rmnet_work_exit(port);

	if (qmi->wda_client)
		data = qmi->wda_client;
	else if (qmi->wda_pending)
		data = qmi->wda_pending;

	if (data) {
		wda_qmi_client_exit(data);
		qmi->wda_client = NULL;
		qmi->wda_pending = NULL;
	}

	for (i = 0; i < MAX_CLIENT_NUM; i++) {
		if (!__qmi_rmnet_delete_client(port, qmi, i))
			return;
	}
}
EXPORT_SYMBOL(qmi_rmnet_qmi_exit);

void qmi_rmnet_enable_all_flows(struct net_device *dev)
{
	struct qos_info *qos;
	struct rmnet_bearer_map *bearer;
	bool do_wake;

	qos = (struct qos_info *)rmnet_get_qos_pt(dev);
	if (!qos)
		return;

	spin_lock_bh(&qos->qos_lock);

	list_for_each_entry(bearer, &qos->bearer_head, list) {
		if (bearer->tx_off)
			continue;
		do_wake = !bearer->grant_size;
		bearer->grant_size = DEFAULT_GRANT;
		bearer->grant_thresh = qmi_rmnet_grant_per(DEFAULT_GRANT);
		bearer->seq = 0;
		bearer->ack_req = 0;
		bearer->tcp_bidir = false;
		bearer->rat_switch = false;

		if (do_wake)
			dfc_bearer_flow_ctl(dev, bearer, qos);
	}

	spin_unlock_bh(&qos->qos_lock);
}
EXPORT_SYMBOL(qmi_rmnet_enable_all_flows);

bool qmi_rmnet_all_flows_enabled(struct net_device *dev)
{
	struct qos_info *qos;
	struct rmnet_bearer_map *bearer;
	bool ret = true;

	qos = (struct qos_info *)rmnet_get_qos_pt(dev);
	if (!qos)
		return true;

	spin_lock_bh(&qos->qos_lock);

	list_for_each_entry(bearer, &qos->bearer_head, list) {
		if (!bearer->grant_size) {
			ret = false;
			break;
		}
	}

	spin_unlock_bh(&qos->qos_lock);

	return ret;
}
EXPORT_SYMBOL(qmi_rmnet_all_flows_enabled);

#ifdef CONFIG_QCOM_QMI_DFC
void qmi_rmnet_burst_fc_check(struct net_device *dev,
			      int ip_type, u32 mark, unsigned int len)
{
	struct qos_info *qos = rmnet_get_qos_pt(dev);

	if (!qos)
		return;

	dfc_qmi_burst_check(dev, qos, ip_type, mark, len);
}
EXPORT_SYMBOL(qmi_rmnet_burst_fc_check);

int qmi_rmnet_get_queue(struct net_device *dev, struct sk_buff *skb)
{
	struct qos_info *qos = rmnet_get_qos_pt(dev);
	int txq = 0, ip_type = AF_INET;
	unsigned int len = skb->len;
	struct rmnet_flow_map *itm;
	u32 mark = skb->mark;

	if (!qos)
		return 0;

	/* If mark is mq num return it */
	if (dfc_mode == DFC_MODE_MQ_NUM)
		return mark;

	switch (skb->protocol) {
	/* TCPv4 ACKs */
	case htons(ETH_P_IP):
		ip_type = AF_INET;
		if ((!mark) &&
		    (ip_hdr(skb)->protocol == IPPROTO_TCP) &&
		    (len == 40 || len == 52) &&
		    (ip_hdr(skb)->ihl == 5) &&
		    ((tcp_flag_word(tcp_hdr(skb)) & 0xFF00) == TCP_FLAG_ACK))
			return 1;
		break;

	/* TCPv6 ACKs */
	case htons(ETH_P_IPV6):
		ip_type = AF_INET6;
		if ((!mark) &&
		    (ipv6_hdr(skb)->nexthdr == IPPROTO_TCP) &&
		    (len == 60 || len == 72) &&
		    ((tcp_flag_word(tcp_hdr(skb)) & 0xFF00) == TCP_FLAG_ACK))
			return 1;
		/* Fall through */
	}

	/* Default flows */
	if (!mark)
		return 0;

	/* Dedicated flows */
	spin_lock_bh(&qos->qos_lock);

	itm = qmi_rmnet_get_flow_map(qos, mark, ip_type);
	if (unlikely(!itm))
		goto done;

	txq = itm->mq_idx;

done:
	spin_unlock_bh(&qos->qos_lock);
	return txq;
}
EXPORT_SYMBOL(qmi_rmnet_get_queue);

inline unsigned int qmi_rmnet_grant_per(unsigned int grant)
{
	return grant / qmi_rmnet_scale_factor;
}
EXPORT_SYMBOL(qmi_rmnet_grant_per);

void *qmi_rmnet_qos_init(struct net_device *real_dev, u8 mux_id)
{
	struct qos_info *qos;

	qos = kzalloc(sizeof(*qos), GFP_KERNEL);
	if (!qos)
		return NULL;

	qos->mux_id = mux_id;
	qos->real_dev = real_dev;
	qos->default_grant = DEFAULT_GRANT;
	qos->tran_num = 0;
	INIT_LIST_HEAD(&qos->flow_head);
	INIT_LIST_HEAD(&qos->bearer_head);
	spin_lock_init(&qos->qos_lock);

	return qos;
}
EXPORT_SYMBOL(qmi_rmnet_qos_init);

void qmi_rmnet_qos_exit(struct net_device *dev, void *qos)
{
	void *port = rmnet_get_rmnet_port(dev);
	struct qmi_info *qmi = rmnet_get_qmi_pt(port);
	struct qos_info *qos_info = (struct qos_info *)qos;

	if (!qmi || !qos)
		return;

	qmi_rmnet_clean_flow_list(qmi, dev, qos_info);
	kfree(qos);
}
EXPORT_SYMBOL(qmi_rmnet_qos_exit);
#endif

#ifdef CONFIG_QCOM_QMI_POWER_COLLAPSE
static struct workqueue_struct  *rmnet_ps_wq;
static struct rmnet_powersave_work *rmnet_work;
static bool rmnet_work_quit;
static LIST_HEAD(ps_list);

struct rmnet_powersave_work {
	struct delayed_work work;
	void *port;
	u64 old_rx_pkts;
	u64 old_tx_pkts;
};

void qmi_rmnet_ps_on_notify(void *port)
{
	struct qmi_rmnet_ps_ind *tmp;

	list_for_each_entry_rcu(tmp, &ps_list, list)
		tmp->ps_on_handler(port);
}
EXPORT_SYMBOL(qmi_rmnet_ps_on_notify);

void qmi_rmnet_ps_off_notify(void *port)
{
	struct qmi_rmnet_ps_ind *tmp;

	list_for_each_entry_rcu(tmp, &ps_list, list)
		tmp->ps_off_handler(port);

}
EXPORT_SYMBOL(qmi_rmnet_ps_off_notify);

int qmi_rmnet_ps_ind_register(void *port,
			      struct qmi_rmnet_ps_ind *ps_ind)
{

	if (!port || !ps_ind || !ps_ind->ps_on_handler ||
	    !ps_ind->ps_off_handler)
		return -EINVAL;

	list_add_rcu(&ps_ind->list, &ps_list);

	return 0;
}
EXPORT_SYMBOL(qmi_rmnet_ps_ind_register);

int qmi_rmnet_ps_ind_deregister(void *port,
				struct qmi_rmnet_ps_ind *ps_ind)
{
	struct qmi_rmnet_ps_ind *tmp;

	if (!port || !ps_ind)
		return -EINVAL;

	list_for_each_entry_rcu(tmp, &ps_list, list) {
		if (tmp == ps_ind) {
			list_del_rcu(&ps_ind->list);
			goto done;
		}
	}
done:
	return 0;
}
EXPORT_SYMBOL(qmi_rmnet_ps_ind_deregister);

int qmi_rmnet_set_powersave_mode(void *port, uint8_t enable)
{
	int rc = -EINVAL;
	struct qmi_info *qmi = (struct qmi_info *)rmnet_get_qmi_pt(port);

	if (!qmi || !qmi->wda_client)
		return rc;

	rc = wda_set_powersave_mode(qmi->wda_client, enable);
	if (rc < 0) {
		pr_err("%s() failed set powersave mode[%u], err=%d\n",
			__func__, enable, rc);
		return rc;
	}

	return 0;
}
EXPORT_SYMBOL(qmi_rmnet_set_powersave_mode);

static void qmi_rmnet_work_restart(void *port)
{
	rcu_read_lock();
	if (!rmnet_work_quit)
		queue_delayed_work(rmnet_ps_wq, &rmnet_work->work, NO_DELAY);
	rcu_read_unlock();
}

static void qmi_rmnet_check_stats(struct work_struct *work)
{
	struct rmnet_powersave_work *real_work;
	struct qmi_info *qmi;
	u64 rxd, txd;
	u64 rx, tx;
	bool dl_msg_active;

	real_work = container_of(to_delayed_work(work),
				 struct rmnet_powersave_work, work);

	if (unlikely(!real_work || !real_work->port))
		return;

	qmi = (struct qmi_info *)rmnet_get_qmi_pt(real_work->port);
	if (unlikely(!qmi))
		return;

	if (qmi->ps_enabled) {

		/* Ready to accept grant */
		qmi->ps_ignore_grant = false;

		/* Register to get QMI DFC and DL marker */
		if (qmi_rmnet_set_powersave_mode(real_work->port, 0) < 0)
			goto end;

		qmi->ps_enabled = false;

		/* Do a query when coming out of powersave */
		qmi_rmnet_query_flows(qmi);

		if (rmnet_get_powersave_notif(real_work->port))
			qmi_rmnet_ps_off_notify(real_work->port);

		goto end;
	}

	rmnet_get_packets(real_work->port, &rx, &tx);
	rxd = rx - real_work->old_rx_pkts;
	txd = tx - real_work->old_tx_pkts;
	real_work->old_rx_pkts = rx;
	real_work->old_tx_pkts = tx;

	dl_msg_active = qmi->dl_msg_active;
	qmi->dl_msg_active = false;

	if (!rxd && !txd) {
		/* If no DL msg received and there is a flow disabled,
		 * (likely in RLF), no need to enter powersave
		 */
		if (!dl_msg_active &&
		    !rmnet_all_flows_enabled(real_work->port))
			goto end;

		/* Deregister to suppress QMI DFC and DL marker */
		if (qmi_rmnet_set_powersave_mode(real_work->port, 1) < 0)
			goto end;

		qmi->ps_enabled = true;

		/* Ignore grant after going into powersave */
		qmi->ps_ignore_grant = true;

		/* Clear the bit before enabling flow so pending packets
		 * can trigger the work again
		 */
		clear_bit(PS_WORK_ACTIVE_BIT, &qmi->ps_work_active);
		rmnet_enable_all_flows(real_work->port);

		if (rmnet_get_powersave_notif(real_work->port))
			qmi_rmnet_ps_on_notify(real_work->port);

		return;
	}
end:
	rcu_read_lock();
	if (!rmnet_work_quit)
		queue_delayed_work(rmnet_ps_wq, &real_work->work, PS_INTERVAL);
	rcu_read_unlock();
}

static void qmi_rmnet_work_set_active(void *port, int status)
{
	struct qmi_info *qmi;

	qmi = (struct qmi_info *)rmnet_get_qmi_pt(port);
	if (unlikely(!qmi))
		return;

	if (status)
		set_bit(PS_WORK_ACTIVE_BIT, &qmi->ps_work_active);
	else
		clear_bit(PS_WORK_ACTIVE_BIT, &qmi->ps_work_active);
}

void qmi_rmnet_work_init(void *port)
{
	if (rmnet_ps_wq)
		return;

	rmnet_ps_wq = alloc_workqueue("rmnet_powersave_work",
					WQ_MEM_RECLAIM | WQ_CPU_INTENSIVE, 1);

	rmnet_work = kzalloc(sizeof(*rmnet_work), GFP_ATOMIC);
	if (!rmnet_work) {
		destroy_workqueue(rmnet_ps_wq);
		rmnet_ps_wq = NULL;
		return;
	}
	INIT_DEFERRABLE_WORK(&rmnet_work->work, qmi_rmnet_check_stats);
	rmnet_work->port = port;
	rmnet_get_packets(rmnet_work->port, &rmnet_work->old_rx_pkts,
			  &rmnet_work->old_tx_pkts);

	rmnet_work_quit = false;
	qmi_rmnet_work_set_active(rmnet_work->port, 1);
	queue_delayed_work(rmnet_ps_wq, &rmnet_work->work, PS_INTERVAL);
}
EXPORT_SYMBOL(qmi_rmnet_work_init);

void qmi_rmnet_work_maybe_restart(void *port)
{
	struct qmi_info *qmi;

	qmi = (struct qmi_info *)rmnet_get_qmi_pt(port);
	if (unlikely(!qmi))
		return;

	if (!test_and_set_bit(PS_WORK_ACTIVE_BIT, &qmi->ps_work_active))
		qmi_rmnet_work_restart(port);
}
EXPORT_SYMBOL(qmi_rmnet_work_maybe_restart);

void qmi_rmnet_work_exit(void *port)
{
	if (!rmnet_ps_wq || !rmnet_work)
		return;

	rmnet_work_quit = true;
	synchronize_rcu();

	cancel_delayed_work_sync(&rmnet_work->work);
	destroy_workqueue(rmnet_ps_wq);
	qmi_rmnet_work_set_active(port, 0);
	rmnet_ps_wq = NULL;
	kfree(rmnet_work);
	rmnet_work = NULL;
}
EXPORT_SYMBOL(qmi_rmnet_work_exit);

void qmi_rmnet_set_dl_msg_active(void *port)
{
	struct qmi_info *qmi;

	qmi = (struct qmi_info *)rmnet_get_qmi_pt(port);
	if (unlikely(!qmi))
		return;

	qmi->dl_msg_active = true;
}
EXPORT_SYMBOL(qmi_rmnet_set_dl_msg_active);

void qmi_rmnet_flush_ps_wq(void)
{
	if (rmnet_ps_wq)
		flush_workqueue(rmnet_ps_wq);
}

bool qmi_rmnet_ignore_grant(void *port)
{
	struct qmi_info *qmi;

	qmi = (struct qmi_info *)rmnet_get_qmi_pt(port);
	if (unlikely(!qmi))
		return false;

	return qmi->ps_ignore_grant;
}
EXPORT_SYMBOL(qmi_rmnet_ignore_grant);

#endif
