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
#include <uapi/linux/rtnetlink.h>
#include "qmi_rmnet_i.h"

#define MODEM_0_INSTANCE 0
#define MODEM_0 0
#define MODEM_1 1

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
		.elem_size	= sizeof(uint32_t),
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

static void *qmi_rmnet_qmi_init(void)
{
	struct qmi_info *qmi_info;

	qmi_info = kzalloc(sizeof(*qmi_info), GFP_KERNEL);
	if (!qmi_info)
		return NULL;

	return (void *)qmi_info;
}

struct rmnet_flow_map *
qmi_rmnet_get_flow_map(struct qos_info *qos, uint32_t flow_id, int ip_type)
{
	struct rmnet_flow_map *itm;

	if (!qos)
		return NULL;

	list_for_each_entry(itm, &qos->flow_head, list) {
		if (unlikely(!itm))
			return NULL;

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
		if (unlikely(!itm))
			return NULL;

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

static int qmi_rmnet_add_flow(struct net_device *dev, struct qmi_info *qmi,
			      struct rmnet_flow_map *new_map)
{
	struct qos_info *qos_info = (struct qos_info *)rmnet_get_qos_pt(dev);
	struct rmnet_flow_map *itm;
	struct rmnet_bearer_map *bearer;
	unsigned long flags;

	if (!qos_info)
		return -EINVAL;

	pr_debug("%s() bearer[%u], flow[%u], ip[%u]\n", __func__,
		new_map->bearer_id, new_map->flow_id, new_map->ip_type);

	write_lock_irqsave(&qos_info->flow_map_lock, flags);
	itm = qmi_rmnet_get_flow_map(qos_info, new_map->flow_id,
				     new_map->ip_type);
	if (itm) {
		qmi_rmnet_update_flow_map(itm, new_map);
	} else {
		write_unlock_irqrestore(&qos_info->flow_map_lock, flags);
		itm = kzalloc(sizeof(*itm), GFP_KERNEL);
		if (!itm)
			return -ENOMEM;

		qmi_rmnet_update_flow_map(itm, new_map);
		write_lock_irqsave(&qos_info->flow_map_lock, flags);
		list_add(&itm->list, &qos_info->flow_head);
	}

	bearer = qmi_rmnet_get_bearer_map(qos_info, new_map->bearer_id);
	if (bearer) {
		bearer->flow_ref++;
	} else {
		write_unlock_irqrestore(&qos_info->flow_map_lock, flags);
		bearer = kzalloc(sizeof(*bearer), GFP_KERNEL);
		if (!bearer)
			return -ENOMEM;

		bearer->bearer_id = new_map->bearer_id;
		bearer->flow_ref = 1;
		bearer->grant_size = qos_info->default_grant;
		write_lock_irqsave(&qos_info->flow_map_lock, flags);
		list_add(&bearer->list, &qos_info->bearer_head);
	}
	write_unlock_irqrestore(&qos_info->flow_map_lock, flags);
	return 0;
}

static int
qmi_rmnet_del_flow(struct net_device *dev, struct rmnet_flow_map *new_map)
{
	struct qos_info *qos_info = (struct qos_info *)rmnet_get_qos_pt(dev);
	struct rmnet_flow_map *itm;
	struct rmnet_bearer_map *bearer;
	unsigned long flags;
	int bearer_removed = 0;

	if (!qos_info) {
		pr_err("%s() NULL qos info\n", __func__);
		return -EINVAL;
	}
	pr_debug("%s() bearer[%u], flow[%u], ip[%u]\n", __func__,
		new_map->bearer_id, new_map->flow_id, new_map->ip_type);

	write_lock_irqsave(&qos_info->flow_map_lock, flags);
	itm = qmi_rmnet_get_flow_map(qos_info, new_map->flow_id,
				     new_map->ip_type);
	if (itm)
		list_del(&itm->list);

	/*clear bearer map*/
	bearer = qmi_rmnet_get_bearer_map(qos_info, new_map->bearer_id);
	if (bearer && --bearer->flow_ref == 0) {
		list_del(&bearer->list);
		bearer_removed = 1;
	}
	write_unlock_irqrestore(&qos_info->flow_map_lock, flags);

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

void qmi_rmnet_change_link(struct net_device *dev, void *port, void *tcm_pt)
{
	struct tcmsg *tcm = (struct tcmsg *)tcm_pt;
	struct qmi_info *qmi = (struct qmi_info *)rmnet_get_qmi_pt(port);
	struct rmnet_flow_map new_map;
	int idx;

	if (!dev || !port || !tcm_pt)
		return;

	switch (tcm->tcm_family) {
	case 1:
		/*
		 * flow activate
		 * tcm->tcm__pad1 - bearer_id, tcm->tcm_parent - flow_id,
		 * tcm->tcm_ifindex - ip_type, tcm->tcm_handle - tcm_handle
		 */
		if (!qmi)
			return;

		new_map.bearer_id = tcm->tcm__pad1;
		new_map.flow_id = tcm->tcm_parent;
		new_map.ip_type = tcm->tcm_ifindex;
		new_map.tcm_handle = tcm->tcm_handle;
		qmi_rmnet_add_flow(dev, qmi, &new_map);
		break;
	case 2:
		/*
		 * flow deactivate
		 * tcm->tcm__pad1 - bearer_id, tcm->tcm_parent - flow_id,
		 * tcm->tcm_ifindex - ip_type
		 */
		if (!qmi)
			return;

		new_map.bearer_id = tcm->tcm__pad1;
		new_map.flow_id = tcm->tcm_parent;
		new_map.ip_type = tcm->tcm_ifindex;
		qmi_rmnet_del_flow(dev, &new_map);
		break;
	case 4:
		/*
		 * modem up
		 * tcm->tcm_handle - instance, tcm->tcm_info - ep_type,
		 * tcm->tcm_parent - iface_id, tcm->tcm_ifindex - flags
		 */
		pr_debug("%s() instance[%u], ep_type[%u], iface[%u]\n",
			__func__, tcm->tcm_handle, tcm->tcm_info,
			tcm->tcm_parent);

		if (tcm->tcm_ifindex != 1)
			return;

		if (tcm->tcm_handle == MODEM_0_INSTANCE)
			idx = MODEM_0;
		else
			idx = MODEM_1;

		if (!qmi) {
			qmi = (struct qmi_info *)qmi_rmnet_qmi_init();
			if (!qmi)
				return;
			qmi->modem_count = 1;
			rmnet_init_qmi_pt(port, qmi);
		} else if (!qmi->fc_info[idx].dfc_client) {
			/*
			 * dfc_client is per modem, we may receive multiple
			 * modem up events due to netmagrd restarts so only
			 * increase modem_count when we need to create a new
			 * dfc client.
			 */
			qmi->modem_count++;
		}
		if (qmi->fc_info[idx].dfc_client == NULL) {
			qmi->fc_info[idx].svc.instance = tcm->tcm_handle;
			qmi->fc_info[idx].svc.ep_type = tcm->tcm_info;
			qmi->fc_info[idx].svc.iface_id = tcm->tcm_parent;
			if (dfc_qmi_client_init(port, idx) < 0)
				pr_err("%s failed[%d]\n", __func__, idx);
		}
		break;
	case 5:
		/* modem down: tcm->tcm_handle - instance*/
		pr_debug("%s() instance[%u]\n", __func__, tcm->tcm_handle);
		if (!qmi)
			return;

		if (tcm->tcm_handle == MODEM_0_INSTANCE)
			idx = MODEM_0;
		else
			idx = MODEM_1;

		/*
		 * dfc_client can be deleted by service request before
		 * modem down event arrival. Decrease modem_count here always
		 */
		qmi->modem_count--;
		if (qmi->fc_info[idx].dfc_client) {
			dfc_qmi_client_exit(qmi->fc_info[idx].dfc_client);
			qmi->fc_info[idx].dfc_client = NULL;
		}
		if (qmi->modem_count == 0) {
			kfree(qmi);
			rmnet_reset_qmi_pt(port);
		}
		break;
	default:
		pr_debug("%s(): No handler\n", __func__);
		break;
	}
}
EXPORT_SYMBOL(qmi_rmnet_change_link);

void *qmi_rmnet_qos_init(struct net_device *real_dev, uint8_t mux_id)
{
	struct qos_info *qos_info;

	qos_info = kmalloc(sizeof(struct qos_info), GFP_KERNEL);
	if (!qos_info)
		return NULL;

	qos_info->mux_id = mux_id;
	qos_info->real_dev = real_dev;
	qos_info->default_grant = 10240;
	qos_info->tran_num = 0;
	rwlock_init(&qos_info->flow_map_lock);
	INIT_LIST_HEAD(&qos_info->flow_head);
	INIT_LIST_HEAD(&qos_info->bearer_head);

	return (void *)qos_info;
}
EXPORT_SYMBOL(qmi_rmnet_qos_init);

void qmi_rmnet_qos_exit(struct net_device *dev)
{
	struct qos_info *qos = (struct qos_info *)rmnet_get_qos_pt(dev);

	kfree(qos);
}
EXPORT_SYMBOL(qmi_rmnet_qos_exit);
