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
#include "qmi_rmnet_i.h"
#include <trace/events/dfc.h>

#define NLMSG_FLOW_ACTIVATE 1
#define NLMSG_FLOW_DEACTIVATE 2
#define NLMSG_CLIENT_SETUP 4
#define NLMSG_CLIENT_DELETE 5

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
	int i;

	qmi_info = kzalloc(sizeof(*qmi_info), GFP_KERNEL);
	if (!qmi_info)
		return NULL;

	for (i = 0; i < MAX_CLIENT_NUM; i++)
		qmi_info->fc_info[i].dfc_client = NULL;

	return qmi_info;
}

static void qmi_rmnet_clean_flow_list(struct qos_info *qos)
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

static int qmi_rmnet_add_flow(struct net_device *dev, struct tcmsg *tcm)
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
qmi_rmnet_del_flow(struct net_device *dev, struct tcmsg *tcm)
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

#ifdef CONFIG_QCOM_QMI_POWER_COLLAPSE
int qmi_rmnet_reg_dereg_fc_ind(void *port, int reg)
{
	struct qmi_info *qmi = (struct qmi_info *)rmnet_get_qmi_pt(port);
	int rc = 0;

	if (!qmi) {
		pr_err("%s - qmi_info is NULL\n", __func__);
		return -EINVAL;
	}

	if (qmi->fc_info[0].dfc_client) {
		rc = dfc_reg_unreg_fc_ind(qmi->fc_info[0].dfc_client, reg);
		if (rc < 0) {
			pr_err("%s() failed dfc_reg_unreg_fc_ind[0] rc=%d\n",
				__func__, rc);
			goto out;
		}
	}
	if (qmi->fc_info[1].dfc_client) {
		rc = dfc_reg_unreg_fc_ind(qmi->fc_info[1].dfc_client, reg);
		if (rc < 0)
			pr_err("%s() failed dfc_reg_unreg_fc_ind[1] rc=%d\n",
				__func__, rc);
	}
out:
	return rc;
}
EXPORT_SYMBOL(qmi_rmnet_reg_dereg_fc_ind);
#endif

static int
qmi_rmnet_setup_client(void *port, struct qmi_info *qmi, struct tcmsg *tcm)
{
	int idx;

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

	if (!qmi->fc_info[idx].dfc_client) {
		qmi->client_count++;

		/* we may receive multiple client setup events if userspace
		 * creates a new dfc client.
		 */
		qmi->flag = tcm->tcm_ifindex;

		qmi->fc_info[idx].svc.instance = tcm->tcm_handle;
		qmi->fc_info[idx].svc.ep_type = tcm->tcm_info;
		qmi->fc_info[idx].svc.iface_id = tcm->tcm_parent;

		return dfc_qmi_client_init(port, idx, qmi);
	}

	return 0;
}

static int
__qmi_rmnet_delete_client(void *port, struct qmi_info *qmi, int idx)
{

	ASSERT_RTNL();

	/* dfc_client can be deleted by service request before
	 * client delete event arrival. Decrease client_count here always
	 */

	if (qmi->fc_info[idx].dfc_client) {
		qmi->client_count--;
		dfc_qmi_client_exit(qmi->fc_info[idx].dfc_client);
		qmi->fc_info[idx].dfc_client = NULL;

		if (qmi->client_count == 0) {
			rmnet_reset_qmi_pt(port);
			kfree(qmi);
			return 0;
		}
	}

	return 1;
}

static void
qmi_rmnet_delete_client(void *port, struct qmi_info *qmi, struct tcmsg *tcm)
{
	int idx;

	/* client delete: tcm->tcm_handle - instance*/
	idx = (tcm->tcm_handle == 0) ? 0 : 1;

	__qmi_rmnet_delete_client(port, qmi, idx);
}

void qmi_rmnet_change_link(struct net_device *dev, void *port, void *tcm_pt)
{
	struct qmi_info *qmi = (struct qmi_info *)rmnet_get_qmi_pt(port);
	struct tcmsg *tcm = (struct tcmsg *)tcm_pt;

	switch (tcm->tcm_family) {
	case NLMSG_FLOW_ACTIVATE:
		if (!qmi || !(qmi->flag & 0x01))
			return;

		qmi_rmnet_add_flow(dev, tcm);
		break;
	case NLMSG_FLOW_DEACTIVATE:
		if (!qmi || !(qmi->flag & 0x01))
			return;

		qmi_rmnet_del_flow(dev, tcm);
		break;
	case NLMSG_CLIENT_SETUP:
		if (!(tcm->tcm_ifindex & 0x01))
			return;

		qmi_rmnet_setup_client(port, qmi, tcm);
		break;
	case NLMSG_CLIENT_DELETE:
		if (!qmi)
			return;

		qmi_rmnet_delete_client(port, qmi, tcm);
		break;
	default:
		pr_debug("%s(): No handler\n", __func__);
		break;
	}
}
EXPORT_SYMBOL(qmi_rmnet_change_link);

void *qmi_rmnet_qos_init(struct net_device *real_dev, u8 mux_id)
{
	struct qos_info *qos;

	qos = kmalloc(sizeof(*qos), GFP_KERNEL);
	if (!qos)
		return NULL;

	qos->mux_id = mux_id;
	qos->real_dev = real_dev;
	qos->default_grant = 10240;
	qos->tran_num = 0;
	INIT_LIST_HEAD(&qos->flow_head);
	INIT_LIST_HEAD(&qos->bearer_head);

	return qos;
}
EXPORT_SYMBOL(qmi_rmnet_qos_init);

void qmi_rmnet_qos_exit(struct net_device *dev)
{
	struct qos_info *qos = (struct qos_info *)rmnet_get_qos_pt(dev);

	qmi_rmnet_clean_flow_list(qos);
	kfree(qos);
}
EXPORT_SYMBOL(qmi_rmnet_qos_exit);

void qmi_rmnet_qmi_exit(void *qmi_pt, void *port)
{
	struct qmi_info *qmi = (struct qmi_info *)qmi_pt;
	int i;

	if (!qmi)
		return;

	for (i = 0; i < MAX_CLIENT_NUM; i++) {
		if (!__qmi_rmnet_delete_client(port, qmi, i))
			return;
	}
}
EXPORT_SYMBOL(qmi_rmnet_qmi_exit);
