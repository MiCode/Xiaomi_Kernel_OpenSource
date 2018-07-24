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

#include <soc/qcom/qmi_rmnet.h>
#include <soc/qcom/rmnet_qmi.h>
#include <linux/soc/qcom/qmi.h>
#include <linux/rtnetlink.h>
#include <uapi/linux/rtnetlink.h>
#include <net/pkt_sched.h>
#include "qmi_rmnet_i.h"
#include <trace/events/dfc.h>

#define NLMSG_FLOW_ACTIVATE 1
#define NLMSG_FLOW_DEACTIVATE 2
#define NLMSG_CLIENT_SETUP 4
#define NLMSG_CLIENT_DELETE 5

#define FLAG_DFC_MASK 0x0001
#define FLAG_POWERSAVE_MASK 0x0010

#define PS_INTERVAL (0x0004 * HZ)
#define NO_DELAY (0x0000 * HZ)

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

static struct qmi_info *qmi_rmnet_qmi_init(void)
{
	struct qmi_info *qmi_info;

	qmi_info = kzalloc(sizeof(*qmi_info), GFP_KERNEL);
	if (!qmi_info)
		return NULL;

	return qmi_info;
}

static inline int
qmi_rmnet_has_dfc_client(struct qmi_info *qmi)
{
	int i;

	if (!qmi || !(qmi->flag & FLAG_DFC_MASK))
		return 0;

	for (i = 0; i < MAX_CLIENT_NUM; i++) {
		if (qmi->fc_info[i].dfc_client)
			return 1;
	}

	return 0;
}

static inline int
qmi_rmnet_has_client(struct qmi_info *qmi)
{
	if (qmi->wda_client)
		return 1;

	return qmi_rmnet_has_dfc_client(qmi);
}

#ifdef CONFIG_QCOM_QMI_DFC
static void
qmi_rmnet_update_flow_link(struct qmi_info *qmi, struct net_device *dev,
			   struct rmnet_flow_map *itm, int add_flow)
{
	int i;

	if (add_flow) {
		if (qmi->flow_cnt == MAX_FLOW_NUM - 1) {
			pr_err("%s() No more space for new flow\n", __func__);
			return;
		}

		qmi->flow[qmi->flow_cnt].dev = dev;
		qmi->flow[qmi->flow_cnt].itm = itm;
		qmi->flow_cnt++;
	} else {
		for (i = 0; i < qmi->flow_cnt; i++) {
			if ((qmi->flow[i].dev == dev) &&
			    (qmi->flow[i].itm == itm)) {
				qmi->flow[i].dev =
					qmi->flow[qmi->flow_cnt-1].dev;
				qmi->flow[i].itm =
					qmi->flow[qmi->flow_cnt-1].itm;
				qmi->flow[qmi->flow_cnt-1].dev = NULL;
				qmi->flow[qmi->flow_cnt-1].itm = NULL;
				qmi->flow_cnt--;
				break;
			}
		}
	}
}

static void
qmi_rmnet_clean_flow_list(struct qmi_info *qmi, struct net_device *dev,
			  struct qos_info *qos)
{
	struct rmnet_bearer_map *bearer, *br_tmp;
	struct rmnet_flow_map *itm, *fl_tmp;

	ASSERT_RTNL();

	list_for_each_entry_safe(itm, fl_tmp, &qos->flow_head, list) {
		qmi_rmnet_update_flow_link(qmi, dev, itm, 0);
		list_del(&itm->list);
		kfree(itm);
	}

	list_for_each_entry_safe(bearer, br_tmp, &qos->bearer_head, list) {
		list_del(&bearer->list);
		kfree(bearer);
	}
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
	itm->tcm_handle = new_map->tcm_handle;
}

static int qmi_rmnet_add_flow(struct net_device *dev, struct tcmsg *tcm,
			      struct qmi_info *qmi)
{
	struct qos_info *qos_info = (struct qos_info *)rmnet_get_qos_pt(dev);
	struct rmnet_flow_map new_map, *itm;
	struct rmnet_bearer_map *bearer;

	if (!qos_info)
		return -EINVAL;

	ASSERT_RTNL();

	/* flow activate
	 * tcm->tcm__pad1 - bearer_id, tcm->tcm_parent - flow_id,
	 * tcm->tcm_ifindex - ip_type, tcm->tcm_handle - tcm_handle
	 */

	new_map.bearer_id = tcm->tcm__pad1;
	new_map.flow_id = tcm->tcm_parent;
	new_map.ip_type = tcm->tcm_ifindex;
	new_map.tcm_handle = tcm->tcm_handle;
	trace_dfc_flow_info(new_map.bearer_id, new_map.flow_id,
			    new_map.ip_type, new_map.tcm_handle, 1);

	itm = qmi_rmnet_get_flow_map(qos_info, new_map.flow_id,
				     new_map.ip_type);
	if (itm) {
		qmi_rmnet_update_flow_map(itm, &new_map);
	} else {
		itm = kzalloc(sizeof(*itm), GFP_KERNEL);
		if (!itm)
			return -ENOMEM;

		qmi_rmnet_update_flow_link(qmi, dev, itm, 1);
		qmi_rmnet_update_flow_map(itm, &new_map);
		list_add(&itm->list, &qos_info->flow_head);
	}

	bearer = qmi_rmnet_get_bearer_map(qos_info, new_map.bearer_id);
	if (bearer) {
		bearer->flow_ref++;
	} else {
		bearer = kzalloc(sizeof(*bearer), GFP_KERNEL);
		if (!bearer)
			return -ENOMEM;

		bearer->bearer_id = new_map.bearer_id;
		bearer->flow_ref = 1;
		bearer->grant_size = qos_info->default_grant;
		list_add(&bearer->list, &qos_info->bearer_head);
	}

	return 0;
}

static int
qmi_rmnet_del_flow(struct net_device *dev, struct tcmsg *tcm,
		   struct qmi_info *qmi)
{
	struct qos_info *qos_info = (struct qos_info *)rmnet_get_qos_pt(dev);
	struct rmnet_flow_map new_map, *itm;
	struct rmnet_bearer_map *bearer;
	int bearer_removed = 0;

	if (!qos_info)
		return -EINVAL;

	ASSERT_RTNL();

	/* flow deactivate
	 * tcm->tcm__pad1 - bearer_id, tcm->tcm_parent - flow_id,
	 * tcm->tcm_ifindex - ip_type
	 */

	new_map.bearer_id = tcm->tcm__pad1;
	new_map.flow_id = tcm->tcm_parent;
	new_map.ip_type = tcm->tcm_ifindex;
	itm = qmi_rmnet_get_flow_map(qos_info, new_map.flow_id,
				     new_map.ip_type);
	if (itm) {
		trace_dfc_flow_info(new_map.bearer_id, new_map.flow_id,
				    new_map.ip_type, itm->tcm_handle, 0);
		qmi_rmnet_update_flow_link(qmi, dev, itm, 0);
		list_del(&itm->list);
	}

	/*clear bearer map*/
	bearer = qmi_rmnet_get_bearer_map(qos_info, new_map.bearer_id);
	if (bearer && --bearer->flow_ref == 0) {
		list_del(&bearer->list);
		bearer_removed = 1;
	}

	kfree(itm);
	if (bearer_removed)
		kfree(bearer);
	return 0;
}

static int qmi_rmnet_enable_all_flows(struct qmi_info *qmi)
{
	int i;
	struct qos_info *qos;
	struct rmnet_flow_map *m;
	struct rmnet_bearer_map *bearer;
	int qlen, need_enable = 0;

	if (!qmi_rmnet_has_dfc_client(qmi) || (qmi->flow_cnt == 0))
		return 0;

	ASSERT_RTNL();

	for (i = 0; i < qmi->flow_cnt; i++) {
		qos = (struct qos_info *)rmnet_get_qos_pt(qmi->flow[i].dev);
		m = qmi->flow[i].itm;
		bearer = qmi_rmnet_get_bearer_map(qos, m->bearer_id);
		if (bearer) {
			if (bearer->grant_size == 0)
				need_enable = 1;
			bearer->grant_size = qos->default_grant;
			if (need_enable) {
				qlen = tc_qdisc_flow_control(qmi->flow[i].dev,
							     m->tcm_handle, 1);
				trace_dfc_qmi_tc(m->bearer_id, m->flow_id,
						 bearer->grant_size, qlen,
						 m->tcm_handle, 1);
			}
		}
	}

	return 0;
}
#else
static inline void
qmi_rmnet_update_flow_link(struct qmi_info *qmi, struct net_device *dev,
			   struct rmnet_flow_map *itm, int add_flow)
{
}

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

static inline int qmi_rmnet_enable_all_flows(struct qmi_info *qmi)
{
	return 0;
}
#endif

static int
qmi_rmnet_setup_client(void *port, struct qmi_info *qmi, struct tcmsg *tcm)
{
	int idx, rc, err = 0;

	ASSERT_RTNL();

	/* client setup
	 * tcm->tcm_handle - instance, tcm->tcm_info - ep_type,
	 * tcm->tcm_parent - iface_id, tcm->tcm_ifindex - flags
	 */
	idx = (tcm->tcm_handle == 0) ? 0 : 1;

	if (!qmi) {
		qmi = qmi_rmnet_qmi_init();
		if (!qmi)
			return -ENOMEM;

		rmnet_init_qmi_pt(port, qmi);
	}

	qmi->flag = tcm->tcm_ifindex;
	qmi->fc_info[idx].svc.instance = tcm->tcm_handle;
	qmi->fc_info[idx].svc.ep_type = tcm->tcm_info;
	qmi->fc_info[idx].svc.iface_id = tcm->tcm_parent;

	if ((tcm->tcm_ifindex & FLAG_DFC_MASK) &&
	    (qmi->fc_info[idx].dfc_client == NULL)) {
		rc = dfc_qmi_client_init(port, idx, qmi);
		if (rc < 0)
			err = rc;
	}

	if ((tcm->tcm_ifindex & FLAG_POWERSAVE_MASK) &&
	    (idx == 0) && (qmi->wda_client == NULL)) {
		rc = wda_qmi_client_init(port, tcm->tcm_handle);
		if (rc < 0)
			err = rc;
	}

	return err;
}

static int
__qmi_rmnet_delete_client(void *port, struct qmi_info *qmi, int idx)
{

	ASSERT_RTNL();

	if (qmi->fc_info[idx].dfc_client) {
		dfc_qmi_client_exit(qmi->fc_info[idx].dfc_client);
		qmi->fc_info[idx].dfc_client = NULL;
	}

	if (!qmi_rmnet_has_client(qmi)) {
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

	/* client delete: tcm->tcm_handle - instance*/
	idx = (tcm->tcm_handle == 0) ? 0 : 1;

	ASSERT_RTNL();

	if ((idx == 0) && qmi->wda_client) {
		wda_qmi_client_exit(qmi->wda_client);
		qmi->wda_client = NULL;
	}

	__qmi_rmnet_delete_client(port, qmi, idx);
}

void qmi_rmnet_change_link(struct net_device *dev, void *port, void *tcm_pt)
{
	struct qmi_info *qmi = (struct qmi_info *)rmnet_get_qmi_pt(port);
	struct tcmsg *tcm = (struct tcmsg *)tcm_pt;

	switch (tcm->tcm_family) {
	case NLMSG_FLOW_ACTIVATE:
		if (!qmi || !(qmi->flag & FLAG_DFC_MASK) ||
		    !qmi_rmnet_has_dfc_client(qmi))
			return;

		qmi_rmnet_add_flow(dev, tcm, qmi);
		break;
	case NLMSG_FLOW_DEACTIVATE:
		if (!qmi || !(qmi->flag & FLAG_DFC_MASK))
			return;

		qmi_rmnet_del_flow(dev, tcm, qmi);
		break;
	case NLMSG_CLIENT_SETUP:
		if (!(tcm->tcm_ifindex & FLAG_DFC_MASK) &&
		    !(tcm->tcm_ifindex & FLAG_POWERSAVE_MASK))
			return;

		if (qmi_rmnet_setup_client(port, qmi, tcm) < 0) {
			if (!qmi_rmnet_has_client(qmi)) {
				kfree(qmi);
				rmnet_reset_qmi_pt(port);
			}
		}
		if (tcm->tcm_ifindex & FLAG_POWERSAVE_MASK) {
			qmi_rmnet_work_init(port);
			rmnet_set_powersave_format(port);
		}
		break;
	case NLMSG_CLIENT_DELETE:
		if (!qmi)
			return;
		if (tcm->tcm_ifindex & FLAG_POWERSAVE_MASK) {
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

	if (!qmi)
		return;

	ASSERT_RTNL();

	if (qmi->wda_client) {
		wda_qmi_client_exit(qmi->wda_client);
		qmi->wda_client = NULL;
	}

	qmi_rmnet_work_exit(port);

	for (i = 0; i < MAX_CLIENT_NUM; i++) {
		if (!__qmi_rmnet_delete_client(port, qmi, i))
			return;
	}
}
EXPORT_SYMBOL(qmi_rmnet_qmi_exit);

#ifdef CONFIG_QCOM_QMI_DFC
void qmi_rmnet_burst_fc_check(struct net_device *dev, struct sk_buff *skb)
{
	void *port = rmnet_get_rmnet_port(dev);
	struct qmi_info *qmi = rmnet_get_qmi_pt(port);
	struct qos_info *qos = rmnet_get_qos_pt(dev);

	if (!qmi || !qos)
		return;

	dfc_qmi_burst_check(dev, qos, skb);
}
EXPORT_SYMBOL(qmi_rmnet_burst_fc_check);

void *qmi_rmnet_qos_init(struct net_device *real_dev, u8 mux_id)
{
	struct qos_info *qos;

	qos = kmalloc(sizeof(*qos), GFP_KERNEL);
	if (!qos)
		return NULL;

	qos->mux_id = mux_id;
	qos->real_dev = real_dev;
	qos->default_grant = DEFAULT_GRANT;
	qos->tran_num = 0;
	INIT_LIST_HEAD(&qos->flow_head);
	INIT_LIST_HEAD(&qos->bearer_head);

	return qos;
}
EXPORT_SYMBOL(qmi_rmnet_qos_init);

void qmi_rmnet_qos_exit(struct net_device *dev)
{
	void *port = rmnet_get_rmnet_port(dev);
	struct qmi_info *qmi = rmnet_get_qmi_pt(port);
	struct qos_info *qos = (struct qos_info *)rmnet_get_qos_pt(dev);

	if (!qmi || !qos)
		return;

	qmi_rmnet_clean_flow_list(qmi, dev, qos);
	kfree(qos);
}
EXPORT_SYMBOL(qmi_rmnet_qos_exit);
#endif

#ifdef CONFIG_QCOM_QMI_POWER_COLLAPSE
static struct workqueue_struct  *rmnet_ps_wq;
static struct rmnet_powersave_work *rmnet_work;

struct rmnet_powersave_work {
	struct delayed_work work;
	void *port;
	u64 old_rx_pkts;
	u64 old_tx_pkts;
};

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
	if (enable)
		qmi_rmnet_enable_all_flows(qmi);

	return 0;
}
EXPORT_SYMBOL(qmi_rmnet_set_powersave_mode);

void qmi_rmnet_work_restart(void *port)
{
	if (!rmnet_ps_wq || !rmnet_work)
		return;
	queue_delayed_work(rmnet_ps_wq, &rmnet_work->work, NO_DELAY);
}
EXPORT_SYMBOL(qmi_rmnet_work_restart);

static void qmi_rmnet_check_stats(struct work_struct *work)
{
	struct rmnet_powersave_work *real_work;
	u64 rxd, txd;
	u64 rx, tx;

	real_work = container_of(to_delayed_work(work),
				 struct rmnet_powersave_work, work);

	if (unlikely(!real_work || !real_work->port))
		return;

	if (!rtnl_trylock()) {
		queue_delayed_work(rmnet_ps_wq, &real_work->work, PS_INTERVAL);
		return;
	}
	if (!qmi_rmnet_work_get_active(real_work->port)) {
		qmi_rmnet_work_set_active(real_work->port, 1);
		qmi_rmnet_set_powersave_mode(real_work->port, 0);
		goto end;
	}

	rmnet_get_packets(real_work->port, &rx, &tx);
	rxd = rx - real_work->old_rx_pkts;
	txd = tx - real_work->old_tx_pkts;
	real_work->old_rx_pkts = rx;
	real_work->old_tx_pkts = tx;

	if (!rxd && !txd) {
		qmi_rmnet_work_set_active(real_work->port, 0);
		qmi_rmnet_set_powersave_mode(real_work->port, 1);
		rtnl_unlock();
		return;
	}
end:
	rtnl_unlock();
	queue_delayed_work(rmnet_ps_wq, &real_work->work, PS_INTERVAL);
}

void qmi_rmnet_work_init(void *port)
{
	if (rmnet_ps_wq)
		return;

	rmnet_ps_wq = alloc_workqueue("rmnet_powersave_work",
					WQ_MEM_RECLAIM | WQ_CPU_INTENSIVE, 1);

	rmnet_work = kmalloc(sizeof(*rmnet_work), GFP_ATOMIC);
	if (!rmnet_work) {
		destroy_workqueue(rmnet_ps_wq);
		rmnet_ps_wq = NULL;
		return;
	}

	INIT_DEFERRABLE_WORK(&rmnet_work->work, qmi_rmnet_check_stats);
	rmnet_work->port = port;
	rmnet_get_packets(rmnet_work->port, &rmnet_work->old_rx_pkts,
			  &rmnet_work->old_tx_pkts);

	qmi_rmnet_work_set_active(rmnet_work->port, 1);
	queue_delayed_work(rmnet_ps_wq, &rmnet_work->work, PS_INTERVAL);
}
EXPORT_SYMBOL(qmi_rmnet_work_init);

void qmi_rmnet_work_set_active(void *port, int status)
{
	if (!port)
		return;
	((struct qmi_info *)rmnet_get_qmi_pt(port))->active = status;
}
EXPORT_SYMBOL(qmi_rmnet_work_set_active);

int qmi_rmnet_work_get_active(void *port)
{
	if (!port)
		return 0;
	return ((struct qmi_info *)rmnet_get_qmi_pt(port))->active;
}
EXPORT_SYMBOL(qmi_rmnet_work_get_active);

void qmi_rmnet_work_exit(void *port)
{
	qmi_rmnet_work_set_active(port, 0);
	if (!rmnet_ps_wq || !rmnet_work)
		return;
	cancel_delayed_work_sync(&rmnet_work->work);
	destroy_workqueue(rmnet_ps_wq);
	rmnet_ps_wq = NULL;
	kfree(rmnet_work);
	rmnet_work = NULL;
}
EXPORT_SYMBOL(qmi_rmnet_work_exit);
#endif
