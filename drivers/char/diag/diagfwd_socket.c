/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/msm_ipc.h>
#include <linux/socket.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/diagchar.h>
#include <linux/of.h>
#include <linux/kmemleak.h>
#include <asm/current.h>
#include <net/sock.h>
#include <linux/ipc_router.h>
#include <linux/notifier.h>
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_peripheral.h"
#include "diagfwd_socket.h"
#include "diag_ipc_logging.h"

#define DIAG_SVC_ID		0x1001

#define MODEM_INST_BASE		0
#define LPASS_INST_BASE		64
#define WCNSS_INST_BASE		128
#define SENSORS_INST_BASE	192

#define INST_ID_CNTL		0
#define INST_ID_CMD		1
#define INST_ID_DATA		2
#define INST_ID_DCI_CMD		3
#define INST_ID_DCI		4

struct diag_cntl_socket_info *cntl_socket;

struct diag_socket_info socket_data[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_DATA,
		.name = "MODEM_DATA"
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_DATA,
		.name = "LPASS_DATA"
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_DATA,
		.name = "WCNSS_DATA"
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_DATA,
		.name = "SENSORS_DATA"
	}
};

struct diag_socket_info socket_cntl[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_CNTL,
		.name = "MODEM_CNTL"
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_CNTL,
		.name = "LPASS_CNTL"
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_CNTL,
		.name = "WCNSS_CNTL"
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_CNTL,
		.name = "SENSORS_CNTL"
	}
};

struct diag_socket_info socket_dci[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_DCI,
		.name = "MODEM_DCI"
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_DCI,
		.name = "LPASS_DCI"
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_DCI,
		.name = "WCNSS_DCI"
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_DCI,
		.name = "SENSORS_DCI"
	}
};

struct diag_socket_info socket_cmd[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_CMD,
		.name = "MODEM_CMD"
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_CMD,
		.name = "LPASS_CMD"
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_CMD,
		.name = "WCNSS_CMD"
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_CMD,
		.name = "SENSORS_CMD"
	}
};

struct diag_socket_info socket_dci_cmd[NUM_PERIPHERALS] = {
	{
		.peripheral = PERIPHERAL_MODEM,
		.type = TYPE_DCI_CMD,
		.name = "MODEM_DCI_CMD"
	},
	{
		.peripheral = PERIPHERAL_LPASS,
		.type = TYPE_DCI_CMD,
		.name = "LPASS_DCI_CMD"
	},
	{
		.peripheral = PERIPHERAL_WCNSS,
		.type = TYPE_DCI_CMD,
		.name = "WCNSS_DCI_CMD"
	},
	{
		.peripheral = PERIPHERAL_SENSORS,
		.type = TYPE_DCI_CMD,
		.name = "SENSORS_DCI_CMD"
	}
};

static void diag_state_open_socket(void *ctxt);
static void diag_state_close_socket(void *ctxt);
static int diag_socket_write(void *ctxt, unsigned char *buf, int len);
static int diag_socket_read(void *ctxt, unsigned char *buf, int buf_len);
static void diag_socket_queue_read(void *ctxt);
static void socket_init_work_fn(struct work_struct *work);
static int socket_ready_notify(struct notifier_block *nb,
			       unsigned long action, void *data);

static struct diag_peripheral_ops socket_ops = {
	.open = diag_state_open_socket,
	.close = diag_state_close_socket,
	.write = diag_socket_write,
	.read = diag_socket_read,
	.queue_read = diag_socket_queue_read
};

static struct notifier_block socket_notify = {
	.notifier_call = socket_ready_notify,
};

static void diag_state_open_socket(void *ctxt)
{
	struct diag_socket_info *info = NULL;

	if (!ctxt)
		return;

	info = (struct diag_socket_info *)(ctxt);
	atomic_set(&info->diag_state, 1);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		 "%s setting diag state to 1", info->name);
}

static void diag_state_close_socket(void *ctxt)
{
	struct diag_socket_info *info = NULL;

	if (!ctxt)
		return;

	info = (struct diag_socket_info *)(ctxt);
	atomic_set(&info->diag_state, 0);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		 "%s setting diag state to 0", info->name);
	wake_up_interruptible(&info->read_wait_q);
	flush_workqueue(info->wq);
}

static void socket_data_ready(struct sock *sk_ptr)
{
	unsigned long flags;
	struct diag_socket_info *info = NULL;

	if (!sk_ptr) {
		pr_err_ratelimited("diag: In %s, invalid sk_ptr", __func__);
		return;
	}

	info = (struct diag_socket_info *)(sk_ptr->sk_user_data);
	if (!info) {
		pr_err_ratelimited("diag: In %s, invalid info\n", __func__);
		return;
	}

	spin_lock_irqsave(&info->lock, flags);
	info->data_ready++;
	spin_unlock_irqrestore(&info->lock, flags);
	diag_ws_on_notify();

	/*
	 * Initialize read buffers for the servers. The servers must read data
	 * first to get the address of its clients.
	 */
	if (!atomic_read(&info->opened) && info->port_type == PORT_TYPE_SERVER)
		diagfwd_buffers_init(info->fwd_ctxt);

	queue_work(info->wq, &(info->read_work));
	wake_up_interruptible(&info->read_wait_q);
	return;
}

static void cntl_socket_data_ready(struct sock *sk_ptr)
{
	if (!sk_ptr || !cntl_socket) {
		pr_err_ratelimited("diag: In %s, invalid ptrs. sk_ptr: %pK cntl_socket: %pK\n",
				   __func__, sk_ptr, cntl_socket);
		return;
	}

	atomic_inc(&cntl_socket->data_ready);
	wake_up_interruptible(&cntl_socket->read_wait_q);
	queue_work(cntl_socket->wq, &(cntl_socket->read_work));
}

static void socket_flow_cntl(struct sock *sk_ptr)
{
	struct diag_socket_info *info = NULL;

	if (!sk_ptr)
		return;

	info = (struct diag_socket_info *)(sk_ptr->sk_user_data);
	if (!info) {
		pr_err_ratelimited("diag: In %s, invalid info\n", __func__);
		return;
	}

	atomic_inc(&info->flow_cnt);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s flow controlled\n", info->name);
	pr_debug("diag: In %s, channel %s flow controlled\n",
		 __func__, info->name);
}

static int lookup_server(struct diag_socket_info *info)
{
	int ret = 0;
	struct server_lookup_args *args = NULL;
	struct sockaddr_msm_ipc *srv_addr = NULL;

	if (!info)
		return -EINVAL;

	args = kzalloc((sizeof(struct server_lookup_args) +
			sizeof(struct msm_ipc_server_info)), GFP_KERNEL);
	if (!args)
		return -ENOMEM;
	kmemleak_not_leak(args);

	args->lookup_mask = 0xFFFFFFFF;
	args->port_name.service = info->svc_id;
	args->port_name.instance = info->ins_id;
	args->num_entries_in_array = 1;
	args->num_entries_found = 0;

	ret = kernel_sock_ioctl(info->hdl, IPC_ROUTER_IOCTL_LOOKUP_SERVER,
				(unsigned long)args);
	if (ret < 0) {
		pr_err("diag: In %s, cannot find service for %s\n", __func__,
		       info->name);
		kfree(args);
		return -EFAULT;
	}

	srv_addr = &info->remote_addr;
	srv_addr->family = AF_MSM_IPC;
	srv_addr->address.addrtype = MSM_IPC_ADDR_ID;
	srv_addr->address.addr.port_addr.node_id = args->srv_info[0].node_id;
	srv_addr->address.addr.port_addr.port_id = args->srv_info[0].port_id;
	ret = args->num_entries_found;
	kfree(args);
	if (ret < 1)
		return -EIO;
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s found server node: %d port: %d",
		 info->name, srv_addr->address.addr.port_addr.node_id,
		 srv_addr->address.addr.port_addr.port_id);
	return 0;
}

static void __socket_open_channel(struct diag_socket_info *info)
{
	if (!info)
		return;

	if (!info->inited) {
		pr_debug("diag: In %s, socket %s is not initialized\n",
			 __func__, info->name);
		return;
	}

	if (atomic_read(&info->opened)) {
		pr_debug("diag: In %s, socket %s already opened\n",
			 __func__, info->name);
		return;
	}

	atomic_set(&info->opened, 1);
	diagfwd_channel_open(info->fwd_ctxt);
}

static void socket_open_client(struct diag_socket_info *info)
{
	int ret = 0;

	if (!info || info->port_type != PORT_TYPE_CLIENT)
		return;

	ret = sock_create(AF_MSM_IPC, SOCK_DGRAM, 0, &info->hdl);
	if (ret < 0 || !info->hdl) {
		pr_err("diag: In %s, socket not initialized for %s\n", __func__,
		       info->name);
		return;
	}

	write_lock_bh(&info->hdl->sk->sk_callback_lock);
	info->hdl->sk->sk_user_data = (void *)(info);
	info->hdl->sk->sk_data_ready = socket_data_ready;
	info->hdl->sk->sk_write_space = socket_flow_cntl;
	write_unlock_bh(&info->hdl->sk->sk_callback_lock);
	ret = lookup_server(info);
	if (ret) {
		pr_err("diag: In %s, failed to lookup server, ret: %d\n",
		       __func__, ret);
		return;
	}
	__socket_open_channel(info);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s exiting\n", info->name);
}

static void socket_open_server(struct diag_socket_info *info)
{
	int ret = 0;
	struct sockaddr_msm_ipc srv_addr = { 0 };

	if (!info)
		return;

	ret = sock_create(AF_MSM_IPC, SOCK_DGRAM, 0, &info->hdl);
	if (ret < 0 || !info->hdl) {
		pr_err("diag: In %s, socket not initialized for %s\n", __func__,
		       info->name);
		return;
	}

	write_lock_bh(&info->hdl->sk->sk_callback_lock);
	info->hdl->sk->sk_user_data = (void *)(info);
	info->hdl->sk->sk_data_ready = socket_data_ready;
	info->hdl->sk->sk_write_space = socket_flow_cntl;
	write_unlock_bh(&info->hdl->sk->sk_callback_lock);

	srv_addr.family = AF_MSM_IPC;
	srv_addr.address.addrtype = MSM_IPC_ADDR_NAME;
	srv_addr.address.addr.port_name.service = info->svc_id;
	srv_addr.address.addr.port_name.instance = info->ins_id;

	ret = kernel_bind(info->hdl, (struct sockaddr *)&srv_addr,
			  sizeof(srv_addr));
	if (ret) {
		pr_err("diag: In %s, failed to bind, ch: %s, svc_id: %d ins_id: %d, err: %d\n",
		       __func__, info->name, info->svc_id, info->ins_id, ret);
		return;
	}
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s opened server svc: %d ins: %d",
		 info->name, info->svc_id, info->ins_id);
}

static void socket_init_work_fn(struct work_struct *work)
{
	struct diag_socket_info *info = container_of(work,
						     struct diag_socket_info,
						     init_work);
	if (!info)
		return;

	if (!info->inited) {
		pr_debug("diag: In %s, socket %s is not initialized\n",
			 __func__, info->name);
		return;
	}

	switch (info->port_type) {
	case PORT_TYPE_SERVER:
		socket_open_server(info);
		break;
	case PORT_TYPE_CLIENT:
		socket_open_client(info);
		break;
	default:
		pr_err("diag: In %s, unknown type %d\n", __func__,
		       info->port_type);
		break;
	}
}

static void __socket_close_channel(struct diag_socket_info *info)
{
	if (!info || !info->hdl)
		return;

	if (!atomic_read(&info->opened))
		return;

	memset(&info->remote_addr, 0, sizeof(struct sockaddr_msm_ipc));
	diagfwd_channel_close(info->fwd_ctxt);

	atomic_set(&info->opened, 0);

	/* Don't close the server. Server should always remain open */
	if (info->port_type != PORT_TYPE_SERVER) {
		write_lock_bh(&info->hdl->sk->sk_callback_lock);
		info->hdl->sk->sk_user_data = NULL;
		info->hdl->sk->sk_data_ready = NULL;
		write_unlock_bh(&info->hdl->sk->sk_callback_lock);
		sock_release(info->hdl);
		info->hdl = NULL;
		wake_up_interruptible(&info->read_wait_q);
	}
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s exiting\n", info->name);

	return;
}

static void socket_close_channel(struct diag_socket_info *info)
{
	if (!info)
		return;

	__socket_close_channel(info);

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s exiting\n", info->name);
}

static int cntl_socket_process_msg_server(uint32_t cmd, uint32_t svc_id,
					  uint32_t ins_id)
{
	uint8_t peripheral;
	uint8_t found = 0;
	struct diag_socket_info *info = NULL;

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		info = &socket_cmd[peripheral];
		if ((svc_id == info->svc_id) &&
		    (ins_id == info->ins_id)) {
			found = 1;
			break;
		}

		info = &socket_dci_cmd[peripheral];
		if ((svc_id == info->svc_id) &&
		    (ins_id == info->ins_id)) {
			found = 1;
			break;
		}
	}

	if (!found)
		return -EIO;

	switch (cmd) {
	case CNTL_CMD_NEW_SERVER:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s received new server\n",
			 info->name);
		diagfwd_register(TRANSPORT_SOCKET, info->peripheral,
				 info->type, (void *)info, &socket_ops,
				 &info->fwd_ctxt);
		queue_work(info->wq, &(info->init_work));
		break;
	case CNTL_CMD_REMOVE_SERVER:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s received remove server\n",
			 info->name);
		socket_close_channel(info);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int cntl_socket_process_msg_client(uint32_t cmd, uint32_t node_id,
					  uint32_t port_id)
{
	uint8_t peripheral;
	uint8_t found = 0;
	struct diag_socket_info *info = NULL;
	struct msm_ipc_port_addr remote_port = {0};

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		info = &socket_data[peripheral];
		remote_port = info->remote_addr.address.addr.port_addr;
		if ((remote_port.node_id == node_id) &&
		    (remote_port.port_id == port_id)) {
			found = 1;
			break;
		}

		info = &socket_cntl[peripheral];
		remote_port = info->remote_addr.address.addr.port_addr;
		if ((remote_port.node_id == node_id) &&
		    (remote_port.port_id == port_id)) {
			found = 1;
			break;
		}

		info = &socket_dci[peripheral];
		remote_port = info->remote_addr.address.addr.port_addr;
		if ((remote_port.node_id == node_id) &&
		    (remote_port.port_id == port_id)) {
			found = 1;
			break;
		}
	}

	if (!found)
		return -EIO;

	switch (cmd) {
	case CNTL_CMD_REMOVE_CLIENT:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s received remove client\n",
			 info->name);
		socket_close_channel(info);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static void cntl_socket_read_work_fn(struct work_struct *work)
{
	union cntl_port_msg msg;
	int ret = 0;
	struct kvec iov = { 0 };
	struct msghdr read_msg = { 0 };


	if (!cntl_socket)
		return;

	ret = wait_event_interruptible(cntl_socket->read_wait_q,
				(atomic_read(&cntl_socket->data_ready) > 0));
	if (ret)
		return;

	do {
		iov.iov_base = &msg;
		iov.iov_len = sizeof(msg);
		read_msg.msg_name = NULL;
		read_msg.msg_namelen = 0;
		ret = kernel_recvmsg(cntl_socket->hdl, &read_msg, &iov, 1,
				     sizeof(msg), MSG_DONTWAIT);
		if (ret < 0) {
			pr_debug("diag: In %s, Error recving data %d\n",
				 __func__, ret);
			break;
		}

		atomic_dec(&cntl_socket->data_ready);

		switch (msg.srv.cmd) {
		case CNTL_CMD_NEW_SERVER:
		case CNTL_CMD_REMOVE_SERVER:
			cntl_socket_process_msg_server(msg.srv.cmd,
						       msg.srv.service,
						       msg.srv.instance);
			break;
		case CNTL_CMD_REMOVE_CLIENT:
			cntl_socket_process_msg_client(msg.cli.cmd,
						       msg.cli.node_id,
						       msg.cli.port_id);
			break;
		}
	} while (atomic_read(&cntl_socket->data_ready) > 0);
}

static void socket_read_work_fn(struct work_struct *work)
{
	struct diag_socket_info *info = container_of(work,
						     struct diag_socket_info,
						     read_work);

	if (!info)
		return;

	diagfwd_channel_read(info->fwd_ctxt);
}

static void diag_socket_queue_read(void *ctxt)
{
	struct diag_socket_info *info = NULL;

	if (!ctxt)
		return;

	info = (struct diag_socket_info *)ctxt;
	if (info->hdl && info->wq)
		queue_work(info->wq, &(info->read_work));
}

void diag_socket_invalidate(void *ctxt, struct diagfwd_info *fwd_ctxt)
{
	struct diag_socket_info *info = NULL;

	if (!ctxt || !fwd_ctxt)
		return;

	info = (struct diag_socket_info *)ctxt;
	info->fwd_ctxt = fwd_ctxt;
}

int diag_socket_check_state(void *ctxt)
{
	struct diag_socket_info *info = NULL;

	if (!ctxt)
		return 0;

	info = (struct diag_socket_info *)ctxt;
	return (int)(atomic_read(&info->diag_state));
}

static void __diag_socket_init(struct diag_socket_info *info)
{
	uint16_t ins_base = 0;
	uint16_t ins_offset = 0;

	char wq_name[DIAG_SOCKET_NAME_SZ + 10];
	if (!info)
		return;

	init_waitqueue_head(&info->wait_q);
	info->inited = 0;
	atomic_set(&info->opened, 0);
	atomic_set(&info->diag_state, 0);
	info->pkt_len = 0;
	info->pkt_read = 0;
	info->hdl = NULL;
	info->fwd_ctxt = NULL;
	info->data_ready = 0;
	atomic_set(&info->flow_cnt, 0);
	spin_lock_init(&info->lock);
	strlcpy(wq_name, "DIAG_SOCKET_", 10);
	strlcat(wq_name, info->name, sizeof(info->name));
	init_waitqueue_head(&info->read_wait_q);
	info->wq = create_singlethread_workqueue(wq_name);
	if (!info->wq) {
		pr_err("diag: In %s, unable to create workqueue for socket channel %s\n",
		       __func__, info->name);
		return;
	}
	INIT_WORK(&(info->init_work), socket_init_work_fn);
	INIT_WORK(&(info->read_work), socket_read_work_fn);

	switch (info->peripheral) {
	case PERIPHERAL_MODEM:
		ins_base = MODEM_INST_BASE;
		break;
	case PERIPHERAL_LPASS:
		ins_base = LPASS_INST_BASE;
		break;
	case PERIPHERAL_WCNSS:
		ins_base = WCNSS_INST_BASE;
		break;
	case PERIPHERAL_SENSORS:
		ins_base = SENSORS_INST_BASE;
		break;
	}

	switch (info->type) {
	case TYPE_DATA:
		ins_offset = INST_ID_DATA;
		info->port_type = PORT_TYPE_SERVER;
		break;
	case TYPE_CNTL:
		ins_offset = INST_ID_CNTL;
		info->port_type = PORT_TYPE_SERVER;
		break;
	case TYPE_DCI:
		ins_offset = INST_ID_DCI;
		info->port_type = PORT_TYPE_SERVER;
		break;
	case TYPE_CMD:
		ins_offset = INST_ID_CMD;
		info->port_type = PORT_TYPE_CLIENT;
		break;
	case TYPE_DCI_CMD:
		ins_offset = INST_ID_DCI_CMD;
		info->port_type = PORT_TYPE_CLIENT;
		break;
	}

	info->svc_id = DIAG_SVC_ID;
	info->ins_id = ins_base + ins_offset;
	info->inited = 1;
}

static void cntl_socket_init_work_fn(struct work_struct *work)
{
	int ret = 0;

	if (!cntl_socket)
		return;

	ret = sock_create(AF_MSM_IPC, SOCK_DGRAM, 0, &cntl_socket->hdl);
	if (ret < 0 || !cntl_socket->hdl) {
		pr_err("diag: In %s, cntl socket is not initialized, ret: %d\n",
		       __func__, ret);
		return;
	}

	write_lock_bh(&cntl_socket->hdl->sk->sk_callback_lock);
	cntl_socket->hdl->sk->sk_user_data = (void *)cntl_socket;
	cntl_socket->hdl->sk->sk_data_ready = cntl_socket_data_ready;
	write_unlock_bh(&cntl_socket->hdl->sk->sk_callback_lock);

	ret = kernel_sock_ioctl(cntl_socket->hdl,
				IPC_ROUTER_IOCTL_BIND_CONTROL_PORT, 0);
	if (ret < 0) {
		pr_err("diag: In %s Could not bind as control port, ret: %d\n",
		       __func__, ret);
	}

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "Initialized control sockets");
}

static int __diag_cntl_socket_init(void)
{
	cntl_socket = kzalloc(sizeof(struct diag_cntl_socket_info), GFP_KERNEL);
	if (!cntl_socket)
		return -ENOMEM;

	cntl_socket->svc_id = DIAG_SVC_ID;
	cntl_socket->ins_id = 1;
	atomic_set(&cntl_socket->data_ready, 0);
	init_waitqueue_head(&cntl_socket->read_wait_q);
	cntl_socket->wq = create_singlethread_workqueue("DIAG_CNTL_SOCKET");
	INIT_WORK(&(cntl_socket->read_work), cntl_socket_read_work_fn);
	INIT_WORK(&(cntl_socket->init_work), cntl_socket_init_work_fn);

	return 0;
}

int diag_socket_init(void)
{
	int err = 0;
	int peripheral = 0;
	struct diag_socket_info *info = NULL;

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		info = &socket_cntl[peripheral];
		__diag_socket_init(&socket_cntl[peripheral]);

		diagfwd_cntl_register(TRANSPORT_SOCKET, peripheral,
			(void *)info, &socket_ops, &(info->fwd_ctxt));

		__diag_socket_init(&socket_data[peripheral]);
		__diag_socket_init(&socket_cmd[peripheral]);
		__diag_socket_init(&socket_dci[peripheral]);
		__diag_socket_init(&socket_dci_cmd[peripheral]);
	}

	err = __diag_cntl_socket_init();
	if (err) {
		pr_err("diag: Unable to open control sockets, err: %d\n", err);
		goto fail;
	}

	register_ipcrtr_af_init_notifier(&socket_notify);
fail:
	return err;
}

static int socket_ready_notify(struct notifier_block *nb,
			       unsigned long action, void *data)
{
	uint8_t peripheral;
	struct diag_socket_info *info = NULL;

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "received notification from IPCR");

	if (action != IPCRTR_AF_INIT) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			 "action not recognized by diag %lu\n", action);
		return 0;
	}

	/* Initialize only the servers */
	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		info = &socket_cntl[peripheral];
		queue_work(info->wq, &(info->init_work));
		info = &socket_data[peripheral];
		queue_work(info->wq, &(info->init_work));
		info = &socket_dci[peripheral];
		queue_work(info->wq, &(info->init_work));
	}
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "Initialized all servers");

	queue_work(cntl_socket->wq, &(cntl_socket->init_work));

	return 0;
}

int diag_socket_init_peripheral(uint8_t peripheral)
{
	struct diag_socket_info *info = NULL;

	if (peripheral >= NUM_PERIPHERALS)
		return -EINVAL;

	info = &socket_data[peripheral];
	diagfwd_register(TRANSPORT_SOCKET, info->peripheral,
			 info->type, (void *)info, &socket_ops,
			 &info->fwd_ctxt);

	info = &socket_dci[peripheral];
	diagfwd_register(TRANSPORT_SOCKET, info->peripheral,
			 info->type, (void *)info, &socket_ops,
			 &info->fwd_ctxt);
	return 0;
}

static void __diag_socket_exit(struct diag_socket_info *info)
{
	if (!info)
		return;

	diagfwd_deregister(info->peripheral, info->type, (void *)info);
	info->fwd_ctxt = NULL;
	info->hdl = NULL;
	if (info->wq)
		destroy_workqueue(info->wq);

}

void diag_socket_early_exit(void)
{
	int i = 0;

	for (i = 0; i < NUM_PERIPHERALS; i++)
		__diag_socket_exit(&socket_cntl[i]);
}

void diag_socket_exit(void)
{
	int i = 0;

	for (i = 0; i < NUM_PERIPHERALS; i++) {
		__diag_socket_exit(&socket_data[i]);
		__diag_socket_exit(&socket_cmd[i]);
		__diag_socket_exit(&socket_dci[i]);
		__diag_socket_exit(&socket_dci_cmd[i]);
	}
}

static int diag_socket_read(void *ctxt, unsigned char *buf, int buf_len)
{
	int err = 0;
	int pkt_len = 0;
	int read_len = 0;
	int bytes_remaining = 0;
	int total_recd = 0;
	int loop_count = 0;
	uint8_t buf_full = 0;
	unsigned char *temp = NULL;
	struct kvec iov = {0};
	struct msghdr read_msg = {0};
	struct sockaddr_msm_ipc src_addr = {0};
	struct diag_socket_info *info = NULL;
	unsigned long flags;

	info = (struct diag_socket_info *)(ctxt);
	if (!info)
		return -ENODEV;

	if (!buf || !ctxt || buf_len <= 0)
		return -EINVAL;

	temp = buf;
	bytes_remaining = buf_len;

	err = wait_event_interruptible(info->read_wait_q,
				      (info->data_ready > 0) || (!info->hdl) ||
				      (atomic_read(&info->diag_state) == 0));
	if (err) {
		diagfwd_channel_read_done(info->fwd_ctxt, buf, 0);
		return -ERESTARTSYS;
	}

	/*
	 * There is no need to continue reading over peripheral in this case.
	 * Release the wake source hold earlier.
	 */
	if (atomic_read(&info->diag_state) == 0) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			 "%s closing read thread. diag state is closed\n",
			 info->name);
		diagfwd_channel_read_done(info->fwd_ctxt, buf, 0);
		return 0;
	}

	if (!info->hdl) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s closing read thread\n",
			 info->name);
		goto fail;
	}

	do {
		loop_count++;
		iov.iov_base = temp;
		iov.iov_len = bytes_remaining;
		read_msg.msg_name = &src_addr;
		read_msg.msg_namelen = sizeof(src_addr);

		pkt_len = kernel_recvmsg(info->hdl, &read_msg, &iov, 1, 0,
					 MSG_PEEK);
		if (pkt_len <= 0)
			break;

		if (pkt_len > bytes_remaining) {
			buf_full = 1;
			break;
		}

		spin_lock_irqsave(&info->lock, flags);
		info->data_ready--;
		spin_unlock_irqrestore(&info->lock, flags);

		read_len = kernel_recvmsg(info->hdl, &read_msg, &iov, 1,
					  pkt_len, 0);
		if (read_len <= 0)
			goto fail;

		if (!atomic_read(&info->opened) &&
		    info->port_type == PORT_TYPE_SERVER) {
			/*
			 * This is the first packet from the client. Copy its
			 * address to the connection object. Consider this
			 * channel open for communication.
			 */
			memcpy(&info->remote_addr, &src_addr, sizeof(src_addr));
			if (info->ins_id == INST_ID_DCI)
				atomic_set(&info->opened, 1);
			else
				__socket_open_channel(info);
		}

		if (read_len < 0) {
			pr_err_ratelimited("diag: In %s, error receiving data, err: %d\n",
					   __func__, pkt_len);
			err = read_len;
			goto fail;
		}
		temp += read_len;
		total_recd += read_len;
		bytes_remaining -= read_len;
	} while (info->data_ready > 0);

	if (buf_full || (info->type == TYPE_DATA && pkt_len))
		err = queue_work(info->wq, &(info->read_work));

	if (total_recd > 0) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s read total bytes: %d\n",
			 info->name, total_recd);
		err = diagfwd_channel_read_done(info->fwd_ctxt,
						buf, total_recd);
		if (err)
			goto fail;
	} else {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s error in read, err: %d\n",
			 info->name, total_recd);
		goto fail;
	}

	diag_socket_queue_read(info);
	return 0;

fail:
	diagfwd_channel_read_done(info->fwd_ctxt, buf, 0);
	return -EIO;
}

static int diag_socket_write(void *ctxt, unsigned char *buf, int len)
{
	int err = 0;
	int write_len = 0;
	struct kvec iov = {0};
	struct msghdr write_msg = {0};
	struct diag_socket_info *info = NULL;

	if (!ctxt || !buf || len <= 0)
		return -EIO;

	info = (struct diag_socket_info *)(ctxt);
	if (!atomic_read(&info->opened) || !info->hdl)
		return -ENODEV;

	iov.iov_base = buf;
	iov.iov_len = len;
	write_msg.msg_name = &info->remote_addr;
	write_msg.msg_namelen = sizeof(info->remote_addr);
	write_msg.msg_flags |= MSG_DONTWAIT;
	write_len = kernel_sendmsg(info->hdl, &write_msg, &iov, 1, len);
	if (write_len < 0) {
		err = write_len;
		/*
		 * -EAGAIN means that the number of packets in flight is at
		 * max capactity and the peripheral hasn't read the data.
		 */
		if (err != -EAGAIN) {
			pr_err_ratelimited("diag: In %s, error sending data, err: %d, ch: %s\n",
					   __func__, err, info->name);
		}
	} else if (write_len != len) {
		err = write_len;
		pr_err_ratelimited("diag: In %s, wrote partial packet to %s, len: %d, wrote: %d\n",
				   __func__, info->name, len, write_len);
	}

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s wrote to socket, len: %d\n",
		 info->name, write_len);

	return err;
}

