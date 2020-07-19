// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2015-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>
#include <linux/workqueue.h>
#include <linux/socket.h>
#include <linux/pm_runtime.h>
#include <linux/delay.h>
#include <linux/diagchar.h>
#include <linux/of.h>
#include <linux/kmemleak.h>
#include <linux/net.h>
#include <linux/socket.h>
#include <asm/current.h>
#include <net/sock.h>
#include <linux/notifier.h>
#include <linux/qrtr.h>
#include <linux/termios.h>
#include "diagchar.h"
#include "diagfwd.h"
#include "diagfwd_peripheral.h"
#include "diagfwd_socket.h"
#include "diag_ipc_logging.h"

#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>

#define DIAG_SVC_ID		0x1001

#define MODEM_INST_BASE		0
#define LPASS_INST_BASE		64
#define WCNSS_INST_BASE		128
#define SENSORS_INST_BASE	192
#define CDSP_INST_BASE		256
#define WDSP_INST_BASE		320
#define NPU_INST_BASE		384

#define INST_ID_CNTL		0
#define INST_ID_CMD		1
#define INST_ID_DATA		2
#define INST_ID_DCI_CMD		3
#define INST_ID_DCI		4

#define MAX_BUF_SIZE		0x4400
#define MAX_NO_PACKETS		10
#define DIAG_SO_RCVBUF_SIZE	(MAX_BUF_SIZE * MAX_NO_PACKETS)

struct qmi_handle *cntl_qmi;
static uint64_t bootup_req[NUM_SOCKET_SUBSYSTEMS];

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
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_DATA,
		.name = "DIAG_DATA"
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_DATA,
		.name = "CDSP_DATA"
	},
	{
		.peripheral = PERIPHERAL_NPU,
		.type = TYPE_DATA,
		.name = "NPU_DATA"
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
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_CNTL,
		.name = "DIAG_CTRL"
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_CNTL,
		.name = "CDSP_CNTL"
	},
	{
		.peripheral = PERIPHERAL_NPU,
		.type = TYPE_CNTL,
		.name = "NPU_CNTL"
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
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_DCI,
		.name = "DIAG_DCI_DATA"
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_DCI,
		.name = "CDSP_DCI"
	},
	{
		.peripheral = PERIPHERAL_NPU,
		.type = TYPE_DCI,
		.name = "NPU_DCI"
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
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_CMD,
		.name = "DIAG_CMD"
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_CMD,
		.name = "CDSP_CMD"
	},
	{
		.peripheral = PERIPHERAL_NPU,
		.type = TYPE_CMD,
		.name = "NPU_CMD"
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
	},
	{
		.peripheral = PERIPHERAL_WDSP,
		.type = TYPE_DCI_CMD,
		.name = "DIAG_DCI_CMD"
	},
	{
		.peripheral = PERIPHERAL_CDSP,
		.type = TYPE_DCI_CMD,
		.name = "CDSP_DCI_CMD"
	},
	{
		.peripheral = PERIPHERAL_NPU,
		.type = TYPE_DCI_CMD,
		.name = "NPU_DCI_CMD"
	}
};

struct restart_notifier_block {
	unsigned int processor;
	char *name;
	struct notifier_block nb;
};

static int restart_notifier_cb(struct notifier_block *this, unsigned long code,
	void *_cmd)
{
	struct restart_notifier_block *notifier;

	notifier = container_of(this,
			struct restart_notifier_block, nb);
	if (!notifier) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"diag: %s: invalid notifier block\n", __func__);
		return NOTIFY_DONE;
	}

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"%s: ssr for processor %d ('%s')\n",
		__func__, notifier->processor, notifier->name);

	switch (code) {

	case SUBSYS_BEFORE_SHUTDOWN:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"diag: %s: SUBSYS_BEFORE_SHUTDOWN\n", __func__);
		mutex_lock(&driver->diag_notifier_mutex);
		bootup_req[notifier->processor] = PERIPHERAL_SSR_DOWN;
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"diag: bootup_req[%s] = %d\n",
		notifier->name, (int)bootup_req[notifier->processor]);
		mutex_unlock(&driver->diag_notifier_mutex);
		break;

	case SUBSYS_AFTER_SHUTDOWN:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"diag: %s: SUBSYS_AFTER_SHUTDOWN\n", __func__);
		break;

	case SUBSYS_BEFORE_POWERUP:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"diag: %s: SUBSYS_BEFORE_POWERUP\n", __func__);
		break;

	case SUBSYS_AFTER_POWERUP:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"diag: %s: SUBSYS_AFTER_POWERUP\n", __func__);
		mutex_lock(&driver->diag_notifier_mutex);
		if (!bootup_req[notifier->processor]) {
			bootup_req[notifier->processor] = PERIPHERAL_SSR_DOWN;
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"diag: bootup_req[%s] = %d\n",
			notifier->name, (int)bootup_req[notifier->processor]);
			mutex_unlock(&driver->diag_notifier_mutex);
			break;
		}
		bootup_req[notifier->processor] = PERIPHERAL_SSR_UP;
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
		"diag: bootup_req[%s] = %d\n",
		notifier->name, (int)bootup_req[notifier->processor]);
		mutex_unlock(&driver->diag_notifier_mutex);
		break;

	default:
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"diag: code: %lu\n", code);
		break;
	}
	return NOTIFY_DONE;
}

static struct restart_notifier_block restart_notifiers[] = {
	{SOCKET_MODEM, "modem", .nb.notifier_call = restart_notifier_cb},
	{SOCKET_ADSP, "adsp", .nb.notifier_call = restart_notifier_cb},
	{SOCKET_WCNSS, "wcnss", .nb.notifier_call = restart_notifier_cb},
	{SOCKET_SLPI, "slpi", .nb.notifier_call = restart_notifier_cb},
	{SOCKET_CDSP, "cdsp", .nb.notifier_call = restart_notifier_cb},
	{SOCKET_NPU, "npu", .nb.notifier_call = restart_notifier_cb},
};

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

static void diag_state_open_socket(void *ctxt);
static void diag_state_close_socket(void *ctxt);
static int diag_socket_write(void *ctxt, unsigned char *buf, int len);
static int diag_socket_read(void *ctxt, unsigned char *buf, int buf_len);
static void diag_socket_drop_data(struct diag_socket_info *info);
static void diag_socket_queue_read(void *ctxt);

static struct diag_peripheral_ops socket_ops = {
	.open = diag_state_open_socket,
	.close = diag_state_close_socket,
	.write = diag_socket_write,
	.read = diag_socket_read,
	.queue_read = diag_socket_queue_read
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

static void socket_data_ready(struct sock *sk_ptr)
{
	struct diag_socket_info *info;
	unsigned long flags;

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

	queue_work(info->wq, &(info->read_work));
	wake_up_interruptible(&info->read_wait_q);
}

static void socket_open_client(struct diag_socket_info *info)
{
	int ret;

	if (!info || info->port_type != PORT_TYPE_CLIENT)
		return;

	ret = sock_create(AF_QIPCRTR, SOCK_DGRAM, PF_QIPCRTR, &info->hdl);
	if (ret < 0 || !info->hdl) {
		pr_err("diag: In %s, socket not initialized for %s\n", __func__,
		       info->name);
		return;
	}

	write_lock_bh(&info->hdl->sk->sk_callback_lock);
	info->hdl->sk->sk_user_data = (void *)(info);
	info->hdl->sk->sk_data_ready = socket_data_ready;
	info->hdl->sk->sk_error_report = socket_data_ready;
	write_unlock_bh(&info->hdl->sk->sk_callback_lock);
	if (!info->remote_addr.sq_node && !info->remote_addr.sq_port) {
		pr_err("diag: In %s, failed to get remote_addr\n", __func__);
		return;
	}
	__socket_open_channel(info);
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s opened client\n", info->name);
}

static void socket_open_server(struct diag_socket_info *info)
{
	struct qrtr_ctrl_pkt pkt;
	struct sockaddr_qrtr sq;
	struct msghdr msg = {0};
	struct kvec iv = { &pkt, sizeof(pkt) };
	int ret;
	unsigned int size = DIAG_SO_RCVBUF_SIZE;

	if (!info || info->port_type != PORT_TYPE_SERVER)
		return;

	ret = sock_create(AF_QIPCRTR, SOCK_DGRAM, PF_QIPCRTR, &info->hdl);
	if (ret < 0 || !info->hdl) {
		pr_err("diag: In %s, socket not initialized for %s\n", __func__,
		       info->name);
		return;
	}
	ret = kernel_getsockname(info->hdl, (struct sockaddr *)&sq);
	if (ret < 0) {
		pr_err("diag: In %s, getsockname failed %d\n", __func__,
		       ret);
		sock_release(info->hdl);
		return;
	}

	kernel_setsockopt(info->hdl, SOL_SOCKET, SO_RCVBUF,
			  (char *)&size, sizeof(size));

	write_lock_bh(&info->hdl->sk->sk_callback_lock);
	info->hdl->sk->sk_user_data = (void *)(info);
	info->hdl->sk->sk_data_ready = socket_data_ready;
	info->hdl->sk->sk_error_report = socket_data_ready;
	write_unlock_bh(&info->hdl->sk->sk_callback_lock);

	memset(&pkt, 0, sizeof(pkt));
	pkt.cmd = cpu_to_le32(QRTR_TYPE_NEW_SERVER);
	pkt.server.service = cpu_to_le32(info->svc_id);
	pkt.server.instance = cpu_to_le32(info->ins_id);
	pkt.server.node = sq.sq_node;
	pkt.server.port = sq.sq_port;

	sq.sq_port = QRTR_PORT_CTRL;
	msg.msg_name = &sq;
	msg.msg_namelen = sizeof(sq);

	ret = kernel_sendmsg(info->hdl, &msg, &iv, 1, sizeof(pkt));
	if (ret < 0) {
		pr_err("%s: failed to send new_server: %d\n", __func__, ret);
		return;
	}
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s opened server svc: %d ins: %d\n",
		 info->name, info->svc_id, info->ins_id);
}

static void __socket_close_channel(struct diag_socket_info *info)
{
	unsigned long flags;

	if (!info)
		return;

	memset(&info->remote_addr, 0, sizeof(info->remote_addr));
	diagfwd_channel_close(info->fwd_ctxt);

	atomic_set(&info->opened, 0);
	/* Don't close the server. Server should always remain open */
	if (info->port_type == PORT_TYPE_SERVER)
		return;

	mutex_lock(&info->socket_info_mutex);
	if (!info->hdl) {
		mutex_unlock(&info->socket_info_mutex);
		return;
	}
	sock_release(info->hdl);
	info->hdl = NULL;
	mutex_unlock(&info->socket_info_mutex);
	wake_up_interruptible(&info->read_wait_q);
	cancel_work_sync(&info->read_work);

	spin_lock_irqsave(&info->lock, flags);
	info->data_ready = 0;
	spin_unlock_irqrestore(&info->lock, flags);

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s exiting\n", info->name);
}

static void socket_close_channel(struct diag_socket_info *info)
{
	if (!info)
		return;

	__socket_close_channel(info);
}


static void socket_init_work_fn(struct work_struct *work)
{
	struct diag_socket_info *info = container_of(work,
						     struct diag_socket_info,
						     init_work);

	if (!info)
		return;

	if (!info->inited) {
		pr_err("diag: In %s, socket %s is not initialized\n",
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

static void socket_read_work_fn(struct work_struct *work)
{
	int err;
	struct diag_socket_info *info = container_of(work,
						     struct diag_socket_info,
						     read_work);
	struct diagfwd_info *fwd_info;

	if (!info) {
		diag_ws_release();
		return;
	}
	mutex_lock(&info->socket_info_mutex);
	if (!info->hdl || !info->hdl->sk) {
		mutex_unlock(&info->socket_info_mutex);
		diag_ws_release();
		return;
	}
	err = sock_error(info->hdl->sk);
	mutex_unlock(&info->socket_info_mutex);
	if (unlikely(err == -ENETRESET)) {
		socket_close_channel(info);
		if (info->port_type == PORT_TYPE_SERVER)
			socket_init_work_fn(&info->init_work);
		diag_ws_release();
		return;
	}
	fwd_info = info->fwd_ctxt;
	if (info->port_type == PORT_TYPE_SERVER &&
		(!fwd_info || !atomic_read(&fwd_info->opened)))
		diag_socket_drop_data(info);

	if (!atomic_read(&info->opened) && info->port_type == PORT_TYPE_SERVER)
		diagfwd_buffers_init(info->fwd_ctxt);

	diagfwd_channel_read(info->fwd_ctxt);
}

static void diag_socket_queue_read(void *ctxt)
{
	struct diag_socket_info *info;

	if (!ctxt)
		return;

	info = (struct diag_socket_info *)ctxt;
	if (info->hdl && info->wq)
		queue_work(info->wq, &(info->read_work));
}

static void handle_ctrl_pkt(struct diag_socket_info *info, void *buf, int len)
{
	const struct qrtr_ctrl_pkt *pkt = buf;
	u32 node;
	u32 port;

	if (len < sizeof(struct qrtr_ctrl_pkt))
		return;

	switch (le32_to_cpu(pkt->cmd)) {
	case QRTR_TYPE_BYE:
		node = le32_to_cpu(pkt->client.node);
		if (info->remote_addr.sq_node == node) {
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s rcvd bye\n",
				 info->name);

			mutex_lock(&driver->diag_notifier_mutex);
			if (bootup_req[info->peripheral] == PERIPHERAL_SSR_UP)
				DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
				"diag: %s is up, bootup_req = %d\n",
				info->name, (int)bootup_req[info->peripheral]);
			mutex_unlock(&driver->diag_notifier_mutex);
			socket_close_channel(info);
		}
		break;
	case QRTR_TYPE_DEL_CLIENT:
		node = le32_to_cpu(pkt->client.node);
		port = le32_to_cpu(pkt->client.port);

		if (info->remote_addr.sq_node == node &&
		    info->remote_addr.sq_port == port) {
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s rcvd del client\n",
				 info->name);

			mutex_lock(&driver->diag_notifier_mutex);
			if (bootup_req[info->peripheral] == PERIPHERAL_SSR_UP) {
				DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
				"diag: %s is up, stopping cleanup: bootup_req = %d\n",
				info->name, (int)bootup_req[info->peripheral]);
				mutex_unlock(&driver->diag_notifier_mutex);
				break;
			}
			mutex_unlock(&driver->diag_notifier_mutex);
			socket_close_channel(info);
		}
		break;
	}
}

static void diag_socket_drop_data(struct diag_socket_info *info)
{
	int err = 0;
	int pkt_len = 0;
	int read_len = 0;
	unsigned char *temp = NULL;
	struct kvec iov;
	struct msghdr read_msg = {NULL, 0};
	struct sockaddr_qrtr src_addr = {0};
	unsigned long flags;

	temp = vzalloc(PERIPHERAL_BUF_SZ);
	if (!temp)
		return;

	while (info->data_ready > 0) {
		iov.iov_base = temp;
		iov.iov_len = PERIPHERAL_BUF_SZ;
		read_msg.msg_name = &src_addr;
		read_msg.msg_namelen = sizeof(src_addr);
		err = info->hdl->ops->ioctl(info->hdl, TIOCINQ,
					(unsigned long)&pkt_len);
		if (err || pkt_len < 0)
			break;
		spin_lock_irqsave(&info->lock, flags);
		if (info->data_ready > 0) {
			info->data_ready--;
		} else {
			spin_unlock_irqrestore(&info->lock, flags);
			break;
		}
		spin_unlock_irqrestore(&info->lock, flags);
		read_len = kernel_recvmsg(info->hdl, &read_msg, &iov, 1,
					  pkt_len, MSG_DONTWAIT);
		pr_debug("%s : %s drop total bytes: %d\n", __func__,
			info->name, read_len);
	}
	vfree(temp);
}

static int diag_socket_read(void *ctxt, unsigned char *buf, int buf_len)
{
	int err = 0;
	int pkt_len = 0;
	int read_len = 0;
	int bytes_remaining = 0;
	int total_recd = 0;
	int qrtr_ctrl_recd = 0;
	uint8_t buf_full = 0;
	unsigned char *temp = NULL;
	struct kvec iov;
	struct msghdr read_msg = {NULL, 0};
	struct sockaddr_qrtr src_addr = {0};
	struct diag_socket_info *info;
	struct mutex *channel_mutex;
	unsigned long flags;

	info = (struct diag_socket_info *)(ctxt);
	if (!info)
		return -ENODEV;

	if (!buf || !ctxt || buf_len <= 0)
		return -EINVAL;

	temp = buf;
	bytes_remaining = buf_len;
	channel_mutex = &driver->diagfwd_channel_mutex[info->peripheral];

	err = wait_event_interruptible(info->read_wait_q,
				      (info->data_ready > 0) || (!info->hdl) ||
				      (atomic_read(&info->diag_state) == 0));
	if (err) {
		mutex_lock(channel_mutex);
		diagfwd_channel_read_done(info->fwd_ctxt, buf, 0);
		mutex_unlock(channel_mutex);
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
		mutex_lock(channel_mutex);
		diagfwd_channel_read_done(info->fwd_ctxt, buf, 0);
		mutex_unlock(channel_mutex);
		return 0;
	}

	if (!info->hdl) {
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s closing read thread\n",
			 info->name);
		goto fail;
	}

	do {
		iov.iov_base = temp;
		iov.iov_len = bytes_remaining;
		read_msg.msg_name = &src_addr;
		read_msg.msg_namelen = sizeof(src_addr);

		mutex_lock(&info->socket_info_mutex);
		if (!info->hdl) {
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			"%s closing read thread\n",
				info->name);
			mutex_unlock(&info->socket_info_mutex);
			goto fail;
		}
		err =  info->hdl->ops->ioctl(info->hdl, TIOCINQ,
					(unsigned long)&pkt_len);
		if (err || pkt_len < 0) {
			mutex_unlock(&info->socket_info_mutex);
			break;
		}

		if (pkt_len > bytes_remaining) {
			buf_full = 1;
			mutex_unlock(&info->socket_info_mutex);
			break;
		}

		spin_lock_irqsave(&info->lock, flags);
		if (info->data_ready > 0) {
			info->data_ready--;
		} else {
			spin_unlock_irqrestore(&info->lock, flags);
			mutex_unlock(&info->socket_info_mutex);
			break;
		}
		spin_unlock_irqrestore(&info->lock, flags);

		read_len = kernel_recvmsg(info->hdl, &read_msg, &iov, 1,
					  pkt_len, MSG_DONTWAIT);
		mutex_unlock(&info->socket_info_mutex);
		if (unlikely(read_len == -ENETRESET)) {
			mutex_lock(channel_mutex);
			diagfwd_channel_read_done(info->fwd_ctxt, buf, 0);
			mutex_unlock(channel_mutex);
			socket_close_channel(info);
			if (info->port_type == PORT_TYPE_SERVER)
				socket_init_work_fn(&info->init_work);
			return read_len;
		} else if (read_len <= 0) {
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
				"Invalid read_len: %d\n", read_len);
			continue;
		}

		if (src_addr.sq_port == QRTR_PORT_CTRL) {
			handle_ctrl_pkt(info, temp, read_len);
			qrtr_ctrl_recd += read_len;
			continue;
		}
		if (info->type == TYPE_CNTL) {
			memcpy(&info->remote_addr, &src_addr, sizeof(src_addr));
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
				"%s client node:port::[0x%x]:[0x%x]\n",
				info->name, src_addr.sq_node, src_addr.sq_port);

			if (!atomic_read(&info->opened))
				__socket_open_channel(info);
		} else {
			if (!atomic_read(&info->opened) &&
			    info->port_type == PORT_TYPE_SERVER) {
				/*
				 * This is the first packet from the client.
				 * Copy its address to the connection object.
				 * Consider this channel open for communication.
				 */
				memcpy(&info->remote_addr, &src_addr,
					sizeof(src_addr));
				DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
					 "%s client node:port::[0x%x]:[0x%x]\n",
					 info->name, src_addr.sq_node,
					 src_addr.sq_port);

				if (info->ins_id == INST_ID_DCI)
					atomic_set(&info->opened, 1);
				else
					__socket_open_channel(info);
			}
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
		mutex_lock(channel_mutex);
		err = diagfwd_channel_read_done(info->fwd_ctxt, buf,
						total_recd);
		mutex_unlock(channel_mutex);
		if (err)
			goto fail;
	} else {
		if (qrtr_ctrl_recd > 0)
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
				"%s read qrtr ctrl bytes: %d\n",
				info->name, qrtr_ctrl_recd);
		else
			DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
				"%s error in read, err: %d\n",
				info->name, total_recd);
		goto fail;
	}

	diag_socket_queue_read(info);
	return 0;

fail:
	mutex_lock(channel_mutex);
	diagfwd_channel_read_done(info->fwd_ctxt, buf, 0);
	mutex_unlock(channel_mutex);
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
	mutex_lock(&info->socket_info_mutex);
	if (!info->hdl) {
		mutex_unlock(&info->socket_info_mutex);
		return -ENODEV;
	}
	write_len = kernel_sendmsg(info->hdl, &write_msg, &iov, 1, len);
	mutex_unlock(&info->socket_info_mutex);
	if (write_len < 0) {
		err = write_len;
		/*
		 * -EAGAIN means that the number of packets in flight is at
		 * max capactity and the peripheral hasn't read the data.
		 */
		if (err != -EAGAIN && err != -ECONNRESET) {
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

static void __diag_socket_init(struct diag_socket_info *info)
{
	uint16_t ins_base = 0;
	uint16_t ins_offset = 0;
	char wq_name[DIAG_SOCKET_NAME_SZ + 10];

	if (!info)
		return;

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
	strlcpy(wq_name, info->name, sizeof(wq_name));
	init_waitqueue_head(&info->read_wait_q);
	info->wq = create_singlethread_workqueue(wq_name);
	if (!info->wq) {
		pr_err("diag: In %s, unable to create workqueue for socket channel %s\n",
		       __func__, info->name);
		return;
	}
	INIT_WORK(&(info->init_work), socket_init_work_fn);
	INIT_WORK(&(info->read_work), socket_read_work_fn);
	memset(&info->remote_addr, 0, sizeof(info->remote_addr));

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
	case PERIPHERAL_WDSP:
		ins_base = WDSP_INST_BASE;
		break;
	case PERIPHERAL_CDSP:
		ins_base = CDSP_INST_BASE;
		break;
	case PERIPHERAL_NPU:
		ins_base = NPU_INST_BASE;
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

	mutex_init(&info->socket_info_mutex);
	info->svc_id = DIAG_SVC_ID;
	info->ins_id = ins_base + ins_offset;
	info->inited = 1;
}

static struct diag_socket_info *diag_get_svc_sock_info(struct qmi_service *svc)
{
	struct diag_socket_info *info = NULL;
	u32 inst;
	int i;

	inst = svc->version | (svc->instance << 8);
	for (i = 0; i < NUM_PERIPHERALS; i++) {
		if ((svc->service == socket_cmd[i].svc_id) &&
		    (inst == socket_cmd[i].ins_id)) {
			info = &socket_cmd[i];
			break;
		}

		if ((svc->service == socket_dci_cmd[i].svc_id) &&
		    (inst == socket_dci_cmd[i].ins_id)) {
			info = &socket_dci_cmd[i];
			break;
		}
	}
	return info;
}

static int diag_new_server(struct qmi_handle *qmi, struct qmi_service *svc)
{
	struct diag_socket_info *info;
	int ret;

	info = diag_get_svc_sock_info(svc);
	if (!info)
		return -EINVAL;

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s rcvd new server\n", info->name);
	ret = diagfwd_register(TRANSPORT_SOCKET, info->peripheral, info->type,
			       (void *)info, &socket_ops, &info->fwd_ctxt);
	info->remote_addr.sq_family = AF_QIPCRTR;
	info->remote_addr.sq_node = svc->node;
	info->remote_addr.sq_port = svc->port;
	socket_init_work_fn(&info->init_work);

	return 0;
}

static void diag_del_server(struct qmi_handle *qmi, struct qmi_service *svc)
{
	struct diag_socket_info *info;

	info = diag_get_svc_sock_info(svc);
	if (!info)
		return;

	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s rcvd del server\n", info->name);
	socket_close_channel(info);
}

static struct qmi_ops diag_qmi_cntl_ops = {
	.new_server = diag_new_server,
	.del_server = diag_del_server,
};

int diag_socket_init(void)
{
	struct diag_socket_info *info = NULL;
	struct restart_notifier_block *nb;
	int peripheral;
	void *handle;
	int rc;
	int i;

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

	for (i = 0; i < ARRAY_SIZE(restart_notifiers); i++) {
		nb = &restart_notifiers[i];
		handle = subsys_notif_register_notifier(nb->name, &nb->nb);
		DIAG_LOG(DIAG_DEBUG_PERIPHERALS,
			 "%s: registering notifier for '%s', handle=%pK\n",
			 __func__, nb->name, handle);
	}

	cntl_qmi = kzalloc(sizeof(*cntl_qmi), GFP_KERNEL);
	if (!cntl_qmi) {
		rc = -ENOMEM;
		goto fail;
	}
	rc = qmi_handle_init(cntl_qmi, 0, &diag_qmi_cntl_ops, NULL);
	if (rc < 0)
		goto fail;

	for (peripheral = 0; peripheral < NUM_PERIPHERALS; peripheral++) {
		info = &socket_cmd[peripheral];
		qmi_add_lookup(cntl_qmi, info->svc_id,
			       info->ins_id & 0xFF, info->ins_id >> 8);

		info = &socket_dci_cmd[peripheral];
		qmi_add_lookup(cntl_qmi, info->svc_id,
			       info->ins_id & 0xFF, info->ins_id >> 8);

		info = &socket_cntl[peripheral];
		socket_init_work_fn(&info->init_work);

		info = &socket_data[peripheral];
		socket_init_work_fn(&info->init_work);

		info = &socket_dci[peripheral];
		socket_init_work_fn(&info->init_work);
	}
	DIAG_LOG(DIAG_DEBUG_PERIPHERALS, "%s: init done\n", __func__);

fail:
	return rc;
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
	if (info->hdl)
		sock_release(info->hdl);
	info->hdl = NULL;
	mutex_destroy(&info->socket_info_mutex);
	if (info->wq)
		destroy_workqueue(info->wq);
}

void diag_socket_exit(void)
{
	int i;

	if (cntl_qmi) {
		qmi_handle_release(cntl_qmi);
		kfree(cntl_qmi);
	}
	for (i = 0; i < NUM_PERIPHERALS; i++) {
		__diag_socket_exit(&socket_cntl[i]);
		__diag_socket_exit(&socket_data[i]);
		__diag_socket_exit(&socket_cmd[i]);
		__diag_socket_exit(&socket_dci[i]);
		__diag_socket_exit(&socket_dci_cmd[i]);
	}
}
