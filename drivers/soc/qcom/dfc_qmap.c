// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <net/pkt_sched.h>
#include <soc/qcom/rmnet_qmi.h>
#include <soc/qcom/qmi_rmnet.h>
#include <trace/events/dfc.h>
#include <soc/qcom/rmnet_ctl.h>
#include "dfc_defs.h"

#define QMAP_DFC_VER		1

#define QMAP_CMD_DONE		-1

#define QMAP_CMD_REQUEST	0
#define QMAP_CMD_ACK		1
#define QMAP_CMD_UNSUPPORTED	2
#define QMAP_CMD_INVALID	3

#define QMAP_DFC_CONFIG		10
#define QMAP_DFC_IND		11
#define QMAP_DFC_QUERY		12
#define QMAP_DFC_END_MARKER	13

struct qmap_hdr {
	u8	cd_pad;
	u8	mux_id;
	__be16	pkt_len;
} __aligned(1);

#define QMAP_HDR_LEN sizeof(struct qmap_hdr)

struct qmap_cmd_hdr {
	u8	pad_len:6;
	u8	reserved_bit:1;
	u8	cd_bit:1;
	u8	mux_id;
	__be16	pkt_len;
	u8	cmd_name;
	u8	cmd_type:2;
	u8	reserved:6;
	u16	reserved2;
	__be32	tx_id;
} __aligned(1);

struct qmap_dfc_config {
	struct qmap_cmd_hdr	hdr;
	u8			cmd_ver;
	u8			cmd_id;
	u8			reserved;
	u8			tx_info:1;
	u8			reserved2:7;
	__be32			ep_type;
	__be32			iface_id;
	u32			reserved3;
} __aligned(1);

struct qmap_dfc_ind {
	struct qmap_cmd_hdr	hdr;
	u8			cmd_ver;
	u8			reserved;
	__be16			seq_num;
	u8			reserved2;
	u8			tx_info_valid:1;
	u8			tx_info:1;
	u8			rx_bytes_valid:1;
	u8			reserved3:5;
	u8			bearer_id;
	u8			tcp_bidir:1;
	u8			bearer_status:3;
	u8			reserved4:4;
	__be32			grant;
	__be32			rx_bytes;
	u32			reserved6;
} __aligned(1);

struct qmap_dfc_query {
	struct qmap_cmd_hdr	hdr;
	u8			cmd_ver;
	u8			reserved;
	u8			bearer_id;
	u8			reserved2;
	u32			reserved3;
} __aligned(1);

struct qmap_dfc_query_resp {
	struct qmap_cmd_hdr	hdr;
	u8			cmd_ver;
	u8			bearer_id;
	u8			tcp_bidir:1;
	u8			rx_bytes_valid:1;
	u8			reserved:6;
	u8			invalid:1;
	u8			reserved2:7;
	__be32			grant;
	__be32			rx_bytes;
	u32			reserved4;
} __aligned(1);

struct qmap_dfc_end_marker_req {
	struct qmap_cmd_hdr	hdr;
	u8			cmd_ver;
	u8			reserved;
	u8			bearer_id;
	u8			reserved2;
	u16			reserved3;
	__be16			seq_num;
	u32			reserved4;
} __aligned(1);

struct qmap_dfc_end_marker_cnf {
	struct qmap_cmd_hdr	hdr;
	u8			cmd_ver;
	u8			reserved;
	u8			bearer_id;
	u8			reserved2;
	u16			reserved3;
	__be16			seq_num;
	u32			reserved4;
} __aligned(1);

static struct dfc_flow_status_ind_msg_v01 qmap_flow_ind;
static struct dfc_tx_link_status_ind_msg_v01 qmap_tx_ind;
static struct dfc_qmi_data __rcu *qmap_dfc_data;
static atomic_t qmap_txid;
static void *rmnet_ctl_handle;

static void dfc_qmap_send_end_marker_cnf(struct qos_info *qos,
					 u8 bearer_id, u16 seq, u32 tx_id);

static void dfc_qmap_send_cmd(struct sk_buff *skb)
{
	trace_dfc_qmap(skb->data, skb->len, false);

	if (rmnet_ctl_send_client(rmnet_ctl_handle, skb)) {
		pr_err("Failed to send to rmnet ctl\n");
		kfree_skb(skb);
	}
}

static void dfc_qmap_send_inband_ack(struct dfc_qmi_data *dfc,
				     struct sk_buff *skb)
{
	struct qmap_cmd_hdr *cmd;

	cmd = (struct qmap_cmd_hdr *)skb->data;

	skb->protocol = htons(ETH_P_MAP);
	skb->dev = rmnet_get_real_dev(dfc->rmnet_port);

	rmnet_ctl_log_debug("TXI", skb->data, skb->len);
	trace_dfc_qmap(skb->data, skb->len, false);
	dev_queue_xmit(skb);
}

static int dfc_qmap_handle_ind(struct dfc_qmi_data *dfc,
			       struct sk_buff *skb)
{
	struct qmap_dfc_ind *cmd;

	if (skb->len < sizeof(struct qmap_dfc_ind))
		return QMAP_CMD_INVALID;

	cmd = (struct qmap_dfc_ind *)skb->data;

	if (cmd->tx_info_valid) {
		memset(&qmap_tx_ind, 0, sizeof(qmap_tx_ind));
		qmap_tx_ind.tx_status = cmd->tx_info;
		qmap_tx_ind.bearer_info_valid = 1;
		qmap_tx_ind.bearer_info_len = 1;
		qmap_tx_ind.bearer_info[0].mux_id = cmd->hdr.mux_id;
		qmap_tx_ind.bearer_info[0].bearer_id = cmd->bearer_id;

		dfc_handle_tx_link_status_ind(dfc, &qmap_tx_ind);

		/* Ignore grant since it is always 0 */
		goto done;
	}

	memset(&qmap_flow_ind, 0, sizeof(qmap_flow_ind));
	qmap_flow_ind.flow_status_valid = 1;
	qmap_flow_ind.flow_status_len = 1;
	qmap_flow_ind.flow_status[0].mux_id = cmd->hdr.mux_id;
	qmap_flow_ind.flow_status[0].bearer_id = cmd->bearer_id;
	qmap_flow_ind.flow_status[0].num_bytes = ntohl(cmd->grant);
	qmap_flow_ind.flow_status[0].seq_num = ntohs(cmd->seq_num);

	if (cmd->rx_bytes_valid) {
		qmap_flow_ind.flow_status[0].rx_bytes_valid = 1;
		qmap_flow_ind.flow_status[0].rx_bytes = ntohl(cmd->rx_bytes);
	}

	if (cmd->tcp_bidir) {
		qmap_flow_ind.ancillary_info_valid = 1;
		qmap_flow_ind.ancillary_info_len = 1;
		qmap_flow_ind.ancillary_info[0].mux_id = cmd->hdr.mux_id;
		qmap_flow_ind.ancillary_info[0].bearer_id = cmd->bearer_id;
		qmap_flow_ind.ancillary_info[0].reserved = DFC_MASK_TCP_BIDIR;
	}

	dfc_do_burst_flow_control(dfc, &qmap_flow_ind, false);

done:
	return QMAP_CMD_ACK;
}

static int dfc_qmap_handle_query_resp(struct dfc_qmi_data *dfc,
				      struct sk_buff *skb)
{
	struct qmap_dfc_query_resp *cmd;

	if (skb->len < sizeof(struct qmap_dfc_query_resp))
		return QMAP_CMD_DONE;

	cmd = (struct qmap_dfc_query_resp *)skb->data;

	if (cmd->invalid)
		return QMAP_CMD_DONE;

	memset(&qmap_flow_ind, 0, sizeof(qmap_flow_ind));
	qmap_flow_ind.flow_status_valid = 1;
	qmap_flow_ind.flow_status_len = 1;

	qmap_flow_ind.flow_status[0].mux_id = cmd->hdr.mux_id;
	qmap_flow_ind.flow_status[0].bearer_id = cmd->bearer_id;
	qmap_flow_ind.flow_status[0].num_bytes = ntohl(cmd->grant);
	qmap_flow_ind.flow_status[0].seq_num = 0xFFFF;

	if (cmd->rx_bytes_valid) {
		qmap_flow_ind.flow_status[0].rx_bytes_valid = 1;
		qmap_flow_ind.flow_status[0].rx_bytes = ntohl(cmd->rx_bytes);
	}

	if (cmd->tcp_bidir) {
		qmap_flow_ind.ancillary_info_valid = 1;
		qmap_flow_ind.ancillary_info_len = 1;
		qmap_flow_ind.ancillary_info[0].mux_id = cmd->hdr.mux_id;
		qmap_flow_ind.ancillary_info[0].bearer_id = cmd->bearer_id;
		qmap_flow_ind.ancillary_info[0].reserved = DFC_MASK_TCP_BIDIR;
	}

	dfc_do_burst_flow_control(dfc, &qmap_flow_ind, true);

	return QMAP_CMD_DONE;
}

static void dfc_qmap_set_end_marker(struct dfc_qmi_data *dfc, u8 mux_id,
				    u8 bearer_id, u16 seq_num, u32 tx_id)
{
	struct net_device *dev;
	struct qos_info *qos;
	struct rmnet_bearer_map *bearer;

	dev = rmnet_get_rmnet_dev(dfc->rmnet_port, mux_id);
	if (!dev)
		return;

	qos = (struct qos_info *)rmnet_get_qos_pt(dev);
	if (!qos)
		return;

	spin_lock_bh(&qos->qos_lock);

	bearer = qmi_rmnet_get_bearer_map(qos, bearer_id);

	if (bearer && bearer->last_seq == seq_num && bearer->grant_size) {
		bearer->ack_req = 1;
		bearer->ack_txid = tx_id;
	} else {
		dfc_qmap_send_end_marker_cnf(qos, bearer_id, seq_num, tx_id);
	}

	spin_unlock_bh(&qos->qos_lock);
}

static int dfc_qmap_handle_end_marker_req(struct dfc_qmi_data *dfc,
					  struct sk_buff *skb)
{
	struct qmap_dfc_end_marker_req *cmd;

	if (skb->len < sizeof(struct qmap_dfc_end_marker_req))
		return QMAP_CMD_INVALID;

	cmd = (struct qmap_dfc_end_marker_req *)skb->data;

	dfc_qmap_set_end_marker(dfc, cmd->hdr.mux_id, cmd->bearer_id,
				ntohs(cmd->seq_num), ntohl(cmd->hdr.tx_id));

	return QMAP_CMD_DONE;
}

static void dfc_qmap_cmd_handler(struct sk_buff *skb)
{
	struct qmap_cmd_hdr *cmd;
	struct dfc_qmi_data *dfc;
	int rc = QMAP_CMD_DONE;

	if (!skb)
		return;

	trace_dfc_qmap(skb->data, skb->len, true);

	if (skb->len < sizeof(struct qmap_cmd_hdr))
		goto free_skb;

	cmd = (struct qmap_cmd_hdr *)skb->data;
	if (!cmd->cd_bit || skb->len != ntohs(cmd->pkt_len) + QMAP_HDR_LEN)
		goto free_skb;

	if (cmd->cmd_name == QMAP_DFC_QUERY) {
		if (cmd->cmd_type != QMAP_CMD_ACK)
			goto free_skb;
	} else if (cmd->cmd_type != QMAP_CMD_REQUEST) {
		goto free_skb;
	}

	rcu_read_lock();

	dfc = rcu_dereference(qmap_dfc_data);
	if (!dfc || READ_ONCE(dfc->restart_state)) {
		rcu_read_unlock();
		goto free_skb;
	}

	switch (cmd->cmd_name) {
	case QMAP_DFC_IND:
		rc = dfc_qmap_handle_ind(dfc, skb);
		qmi_rmnet_set_dl_msg_active(dfc->rmnet_port);
		break;

	case QMAP_DFC_QUERY:
		rc = dfc_qmap_handle_query_resp(dfc, skb);
		break;

	case QMAP_DFC_END_MARKER:
		rc = dfc_qmap_handle_end_marker_req(dfc, skb);
		break;

	default:
		rc = QMAP_CMD_UNSUPPORTED;
	}

	/* Send ack */
	if (rc != QMAP_CMD_DONE) {
		cmd->cmd_type = rc;
		if (cmd->cmd_name == QMAP_DFC_IND)
			dfc_qmap_send_inband_ack(dfc, skb);
		else
			dfc_qmap_send_cmd(skb);

		rcu_read_unlock();
		return;
	}

	rcu_read_unlock();

free_skb:
	kfree_skb(skb);
}

static void dfc_qmap_send_config(struct dfc_qmi_data *data)
{
	struct sk_buff *skb;
	struct qmap_dfc_config *dfc_config;
	unsigned int len = sizeof(struct qmap_dfc_config);

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;

	skb->protocol = htons(ETH_P_MAP);
	dfc_config = (struct qmap_dfc_config *)skb_put(skb, len);
	memset(dfc_config, 0, len);

	dfc_config->hdr.cd_bit = 1;
	dfc_config->hdr.mux_id = 0;
	dfc_config->hdr.pkt_len = htons(len - QMAP_HDR_LEN);
	dfc_config->hdr.cmd_name = QMAP_DFC_CONFIG;
	dfc_config->hdr.cmd_type = QMAP_CMD_REQUEST;
	dfc_config->hdr.tx_id = htonl(atomic_inc_return(&qmap_txid));

	dfc_config->cmd_ver = QMAP_DFC_VER;
	dfc_config->cmd_id = QMAP_DFC_IND;
	dfc_config->tx_info = 1;
	dfc_config->ep_type = htonl(data->svc.ep_type);
	dfc_config->iface_id = htonl(data->svc.iface_id);

	dfc_qmap_send_cmd(skb);
}

static void dfc_qmap_send_query(u8 mux_id, u8 bearer_id)
{
	struct sk_buff *skb;
	struct qmap_dfc_query *dfc_query;
	unsigned int len = sizeof(struct qmap_dfc_query);

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;

	skb->protocol = htons(ETH_P_MAP);
	dfc_query = (struct qmap_dfc_query *)skb_put(skb, len);
	memset(dfc_query, 0, len);

	dfc_query->hdr.cd_bit = 1;
	dfc_query->hdr.mux_id = mux_id;
	dfc_query->hdr.pkt_len = htons(len - QMAP_HDR_LEN);
	dfc_query->hdr.cmd_name = QMAP_DFC_QUERY;
	dfc_query->hdr.cmd_type = QMAP_CMD_REQUEST;
	dfc_query->hdr.tx_id = htonl(atomic_inc_return(&qmap_txid));

	dfc_query->cmd_ver = QMAP_DFC_VER;
	dfc_query->bearer_id = bearer_id;

	dfc_qmap_send_cmd(skb);
}

static void dfc_qmap_send_end_marker_cnf(struct qos_info *qos,
					 u8 bearer_id, u16 seq, u32 tx_id)
{
	struct sk_buff *skb;
	struct qmap_dfc_end_marker_cnf *em_cnf;
	unsigned int len = sizeof(struct qmap_dfc_end_marker_cnf);

	skb = alloc_skb(len, GFP_ATOMIC);
	if (!skb)
		return;

	em_cnf = (struct qmap_dfc_end_marker_cnf *)skb_put(skb, len);
	memset(em_cnf, 0, len);

	em_cnf->hdr.cd_bit = 1;
	em_cnf->hdr.mux_id = qos->mux_id;
	em_cnf->hdr.pkt_len = htons(len - QMAP_HDR_LEN);
	em_cnf->hdr.cmd_name = QMAP_DFC_END_MARKER;
	em_cnf->hdr.cmd_type = QMAP_CMD_ACK;
	em_cnf->hdr.tx_id = htonl(tx_id);

	em_cnf->cmd_ver = QMAP_DFC_VER;
	em_cnf->bearer_id = bearer_id;
	em_cnf->seq_num = htons(seq);

	skb->protocol = htons(ETH_P_MAP);
	skb->dev = qos->real_dev;

	/* This cmd needs to be sent in-band */
	rmnet_ctl_log_info("TXI", skb->data, skb->len);
	trace_dfc_qmap(skb->data, skb->len, false);
	rmnet_map_tx_qmap_cmd(skb);
}

void dfc_qmap_send_ack(struct qos_info *qos, u8 bearer_id, u16 seq, u8 type)
{
	struct rmnet_bearer_map *bearer;

	if (type == DFC_ACK_TYPE_DISABLE) {
		bearer = qmi_rmnet_get_bearer_map(qos, bearer_id);
		if (bearer)
			dfc_qmap_send_end_marker_cnf(qos, bearer_id,
						     seq, bearer->ack_txid);
	} else if (type == DFC_ACK_TYPE_THRESHOLD) {
		dfc_qmap_send_query(qos->mux_id, bearer_id);
	}
}

static struct rmnet_ctl_client_hooks cb = {
	.ctl_dl_client_hook = dfc_qmap_cmd_handler,
};

int dfc_qmap_client_init(void *port, int index, struct svc_info *psvc,
			 struct qmi_info *qmi)
{
	struct dfc_qmi_data *data;

	if (!port || !qmi)
		return -EINVAL;

	data = kzalloc(sizeof(struct dfc_qmi_data), GFP_KERNEL);
	if (!data)
		return -ENOMEM;

	data->rmnet_port = port;
	data->index = index;
	memcpy(&data->svc, psvc, sizeof(data->svc));

	qmi->dfc_clients[index] = (void *)data;
	rcu_assign_pointer(qmap_dfc_data, data);

	atomic_set(&qmap_txid, 0);

	rmnet_ctl_handle = rmnet_ctl_register_client(&cb);
	if (!rmnet_ctl_handle)
		pr_err("Failed to register with rmnet ctl\n");

	trace_dfc_client_state_up(data->index, data->svc.instance,
				  data->svc.ep_type, data->svc.iface_id);

	pr_info("DFC QMAP init\n");

	dfc_qmap_send_config(data);

	return 0;
}

void dfc_qmap_client_exit(void *dfc_data)
{
	struct dfc_qmi_data *data = (struct dfc_qmi_data *)dfc_data;

	if (!data) {
		pr_err("%s() data is null\n", __func__);
		return;
	}

	trace_dfc_client_state_down(data->index, 0);

	rmnet_ctl_unregister_client(rmnet_ctl_handle);

	WRITE_ONCE(data->restart_state, 1);
	RCU_INIT_POINTER(qmap_dfc_data, NULL);
	synchronize_rcu();

	kfree(data);

	pr_info("DFC QMAP exit\n");
}
