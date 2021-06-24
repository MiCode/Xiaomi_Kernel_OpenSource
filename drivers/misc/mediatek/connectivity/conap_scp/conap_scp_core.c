// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/workqueue.h>
#include <asm/atomic.h>
#include <linux/spinlock.h>
#include <linux/string.h>

#include "conap_scp.h"
#include "conap_scp_priv.h"
#include "msg_thread.h"
#include "conap_scp_shm.h"
#include "conap_scp_ipi.h"
#include "ring_buffer.h"

struct conap_scp_drv_user g_drv_user[CONAP_SCP_DRV_NUM];
struct conap_scp_core_ctx {
	unsigned int enable;
	unsigned int chip_info;
	phys_addr_t emi_phy_addr;
	unsigned int state; /* stop: 0, ready : 1 */
	struct mutex lock;
	struct task_struct *rx_thread;
	struct msg_thread_ctx tx_msg_thread;
	wait_queue_head_t waitQueue;
};

static struct conap_scp_core_ctx g_core_ctx;

static int opfunc_init_handshake(struct msg_op_data *op);
static int opfunc_send_msg(struct msg_op_data *op);
static int opfunc_is_drv_ready(struct msg_op_data *op);

static const msg_opid_func conap_scp_core_opfunc[] = {
	[CONAP_SCP_OPID_INIT_HANDSHAKE] = opfunc_init_handshake,
	[CONAP_SCP_OPID_SEND_MSG] = opfunc_send_msg,
	[CONAP_SCP_OPID_DRV_READY] = opfunc_is_drv_ready,
};

uint32_t g_msg_buf[1024];
uint32_t g_send_seq_num = 0;
uint32_t g_recv_seq_num = 0;

enum conap_core_event {
	CONAP_CORE_DRV_QRY = 0,
	CONAP_CORE_SCP_STATE_CHANGE = 1,
	CONAP_CORE_EVENT_NUM
};
struct conap_core_rb g_core_rb;
struct conap_core_rb g_drv_rdy_rb;


/*********************************************************************/
/* CORE OP FUNC */
/*********************************************************************/
static int opfunc_init_handshake(struct msg_op_data *op)
{
	int ret = 0;

	ret = conap_scp_ipi_handshake();
	if (ret) {
		pr_info("[%s] ======= =[%d]", __func__, ret);
		return ret;
	}

	return ret;
}

static int opfunc_send_msg(struct msg_op_data *op)
{
	int ret = 0;
	struct scif_msg_header msg_header;

	unsigned int drv_type = (unsigned int)op->op_data[0];
	unsigned int msg_id = (unsigned int)op->op_data[1];
	unsigned char* buf = (unsigned char*)op->op_data[2];
	unsigned int size = (unsigned int)op->op_data[3];

	msg_header.guard_pattern = SCIF_MSG_GUARD_PATTERN;
	msg_header.msg_len = size + sizeof(struct scif_msg_header);
	msg_header.src_mod_id = drv_type;
	msg_header.dst_mod_id = drv_type;
	msg_header.msg_id = msg_id;
	msg_header.seq_num = g_send_seq_num++;
	msg_header.timestamp = 0; /* TODO: TBD */

	ret = conap_scp_shm_write_rbf(&msg_header, buf, size);

	if (ret) {
		pr_err("[%s] write shm fail [%d]", __func__, ret);
		return -1;
	}
	ret = conap_scp_ipi_send((enum conap_scp_drv_type)drv_type, msg_id, 0, 0);
	if (ret) {
		pr_err("[%s] ipi fail [%d]", __func__, ret);
		return -2;
	}
	return 0;
}

static int opfunc_is_drv_ready(struct msg_op_data *op)
{
	int ret = 0;

	unsigned int drv_type = (unsigned int)op->op_data[0];
	ret = conap_scp_ipi_send(DRV_TYPE_CORE, CONAP_SCP_CORE_DRV_QRY, drv_type, 0);
	if (ret) {
		pr_err("[%s] ipi fail [%d]", __func__, ret);
		return -2;
	}
	return 0;
}

/*********************************************************************/
/* PRIVATE API */
/*********************************************************************/
static int _conap_scp_is_scp_ready(void)
{
	if (mutex_lock_killable(&g_core_ctx.lock))
		return -1;
	if (g_core_ctx.state == 0) {
		mutex_unlock(&g_core_ctx.lock);
		return 0;
	}
	mutex_unlock(&g_core_ctx.lock);
	return 1;
}

int conap_scp_init_handshake(void)
{
	int ret;

	ret = msg_thread_send(&g_core_ctx.tx_msg_thread, CONAP_SCP_OPID_INIT_HANDSHAKE);
	if (ret)
		pr_info("[%s] handshake ret=[%d]", __func__, ret);

	return ret;
}

int msg_notify = 0;
static void conap_scp_msg_notify(uint16_t drv_type, uint16_t msg_id, uint32_t param0, uint32_t param1)
{
	struct conap_rb_data *drv_rdy_data = NULL;
	struct conap_rb_data *core_data;
	unsigned long flags;

	if (drv_type == DRV_TYPE_CORE) {
		if (msg_id == CONAP_SCP_CORE_DRV_QRY_ACK) {
			spin_lock_irqsave(&g_drv_rdy_rb.lock, flags);
			drv_rdy_data = conap_core_rb_pop_active(&g_drv_rdy_rb);
			spin_unlock_irqrestore(&g_drv_rdy_rb.lock, flags);
			if (drv_rdy_data) {
				drv_rdy_data->param0 = param0;
				complete(&drv_rdy_data->comp);

				if (atomic_dec_and_test(&drv_rdy_data->ref_count) == 0) {
					spin_lock_irqsave(&g_drv_rdy_rb.lock, flags);
					conap_core_rb_push_free(&g_drv_rdy_rb, drv_rdy_data);
					spin_unlock_irqrestore(&g_drv_rdy_rb.lock, flags);
				}
			} else {
				pr_warn("[%s] not found rdy_data", __func__);
			}
			return;
		} else if (msg_id == CONAP_SCP_CORE_DRV_QRY) {
			spin_lock_irqsave(&g_core_rb.lock, flags);
			core_data = conap_core_rb_pop_free(&g_core_rb);
			if (core_data) {
				core_data->param0 = CONAP_CORE_DRV_QRY;
				core_data->param1 = param0;
				conap_core_rb_push_active(&g_core_rb, core_data);
			} else {
				pr_err("[%s] core_rb is NULL", __func__);
			}
			spin_unlock_irqrestore(&g_core_rb.lock, flags);
		}
	}

	msg_notify = 1;
	wake_up_interruptible(&g_core_ctx.waitQueue);
}

static void conap_scp_ipi_ctrl_notify(unsigned int state)
{
	struct conap_rb_data *core_data;
	unsigned long flags;

	if ((g_core_ctx.state == 1 && state == 1) ||
		(g_core_ctx.state == 0 && state == 0))
		return;

	spin_lock_irqsave(&g_core_rb.lock, flags);
	core_data = conap_core_rb_pop_free(&g_core_rb);
	if (core_data) {
		core_data->param0 = CONAP_CORE_SCP_STATE_CHANGE;
		core_data->param1 = state;
		conap_core_rb_push_active(&g_core_rb, core_data);
	} else {
		pr_err("[%s] core_rb is NULL", __func__);
	}
	spin_unlock_irqrestore(&g_core_rb.lock, flags);

	wake_up_interruptible(&g_core_ctx.waitQueue);
}


/*********************************************************************/
/* OPEN API */
/*********************************************************************/
int conap_scp_send_message(enum conap_scp_drv_type type,
					unsigned int msg_id,
					unsigned char *buf, unsigned int size)
{
	int ret = 0;

	if (g_core_ctx.enable == 0)
		return CONN_CONAP_NOT_SUPPORT;

	if (type == DRV_TYPE_CORE || type >= CONAP_SCP_DRV_NUM)
		return CONN_INVALID_ARGUMENT;

	if (size > SCIF_MAX_MSG_SIZE || size % 4 > 0)
		return CONN_INVALID_ARGUMENT;

	if (_conap_scp_is_scp_ready() != 1)
		return CONN_NOT_READY;

	ret = msg_thread_send_wait_4(&g_core_ctx.tx_msg_thread, CONAP_SCP_OPID_SEND_MSG, MSG_OP_TIMEOUT,
					(size_t)type, msg_id, (size_t)buf, size);
	if (ret)
		pr_info("[%s] msg_thread_send_wait ret=[%d]", __func__, ret);

	return ret;
}
EXPORT_SYMBOL(conap_scp_send_message);

int conap_scp_is_drv_ready(enum conap_scp_drv_type type)
{
	int ret = 0, wait_ret=1;
	struct conap_rb_data *drv_rdy_data = NULL;
	unsigned long flags;
	int is_ready = 0;
	int refcnt;

	if (g_core_ctx.enable == 0)
		return CONN_CONAP_NOT_SUPPORT;

	if (_conap_scp_is_scp_ready() != 1)
		return CONN_NOT_READY;

	/* acquire op cmd */
	spin_lock_irqsave(&g_drv_rdy_rb.lock, flags);

	drv_rdy_data = conap_core_rb_pop_free(&g_drv_rdy_rb);
	if (drv_rdy_data == NULL) {
		spin_unlock_irqrestore(&g_drv_rdy_rb.lock, flags);
		pr_err("[%s] core_rb is NULL", __func__);
		return -1;
	}

	atomic_set(&drv_rdy_data->ref_count, 2);
	drv_rdy_data->param0 = 0;

	reinit_completion(&drv_rdy_data->comp);
	conap_core_rb_push_active(&g_drv_rdy_rb, drv_rdy_data);

	spin_unlock_irqrestore(&g_drv_rdy_rb.lock, flags);

	/* send msg */
	ret = msg_thread_send_1(&g_core_ctx.tx_msg_thread, CONAP_SCP_OPID_DRV_READY, (size_t)type);
	if (ret)
		pr_info("[%s] ----- msg send ret=[%d]", __func__, ret);

	wait_ret = wait_for_completion_timeout(&drv_rdy_data->comp,
				msecs_to_jiffies(2000));

	if (wait_ret == 0)
		return CONN_TIMEOUT;

	is_ready = drv_rdy_data->param0;

	refcnt = atomic_dec_and_test(&drv_rdy_data->ref_count);
	if (refcnt == 0) {
		spin_lock_irqsave(&g_drv_rdy_rb.lock, flags);
		conap_core_rb_push_free(&g_drv_rdy_rb, drv_rdy_data);
		spin_unlock_irqrestore(&g_drv_rdy_rb.lock, flags);
	}

	return is_ready;
}
EXPORT_SYMBOL(conap_scp_is_drv_ready);

int conap_scp_register_drv(enum conap_scp_drv_type type, struct conap_scp_drv_cb *cb)
{
	if (g_core_ctx.enable == 0)
		return CONN_CONAP_NOT_SUPPORT;
	if (type >= CONAP_SCP_DRV_NUM)
		return -EINVAL;
	if (g_drv_user[type].enable)
		return -EEXIST;

	if (mutex_lock_killable(&g_core_ctx.lock))
		return -1;

	memcpy(&g_drv_user[type].drv_cb, cb, sizeof(struct conap_scp_drv_cb));
	g_drv_user[type].enable = 1;

	mutex_unlock(&g_core_ctx.lock);
	return 0;
}
EXPORT_SYMBOL(conap_scp_register_drv);

int conap_scp_unregister_drv(enum conap_scp_drv_type type)
{
	if (g_core_ctx.enable == 0)
		return CONN_CONAP_NOT_SUPPORT;
	if (type >= CONAP_SCP_DRV_NUM)
		return -EINVAL;
	if (!g_drv_user[type].enable)
		return -EEXIST;

	if (mutex_lock_killable(&g_core_ctx.lock))
		return -1;
	memset(&g_drv_user[type].drv_cb, 0, sizeof(struct conap_scp_drv_cb));
	g_drv_user[type].enable = 0;

	mutex_unlock(&g_core_ctx.lock);

	return 0;
}
EXPORT_SYMBOL(conap_scp_unregister_drv);

static void read_pkt_from_shm(void)
{
	int sz;
	unsigned drv_type;
	struct scif_msg_header header;

	while (1) {
		sz = conap_scp_shm_has_pending_data(&header);

		if (sz == 0) {
			break;
		}
		/* sz < 0, shm corrupted */
		if (sz < 0) {
			pr_err("pending data fail [%d]\n", sz);
			/* TODO: trigger remote assert */
			//trigger_remote_assert(ch, SCIF_MSG_SIZE_ERROR);
			break;
		}

		/* check gauard pattern & seq num */
		if (header.guard_pattern != SCIF_MSG_GUARD_PATTERN) {
			pr_err("[conn_recv] sz=[%d] header invalid [%x][%x][%x][%x] [%x][%x][%x][%x]\n", sz,
					header.guard_pattern, header.msg_len, header.src_mod_id,
					header.dst_mod_id, header.msg_id, header.seq_num,
					header.timestamp, header.checksum32);
			/* TODO: trigger remote assert */
			//trigger_remote_assert(ch, SCIF_GUARD_ERROR);
			break;
		}

		if (header.seq_num != g_recv_seq_num) {
			//atomicbit_clearbit(&g_task_recv_state, type);
			pr_info("[conn_recv] header seq=[%d][%d]\n",
					header.seq_num, g_recv_seq_num);
			//trigger_remote_assert(ch, SCIF_SEQUENCE_ERROR);
			break;
		}
		g_recv_seq_num++;

		pr_info("read_data sz=[%d] len=[%d] [%d][%d] msg=[%d] time=[%x]\n",
				sz, header.msg_len, header.src_mod_id, header.dst_mod_id, header.msg_id,
				header.timestamp);

		if (header.msg_len > conap_scp_shm_get_slave_rbf_len() ||
			(header.msg_len % 4) || header.msg_len > SCIF_MAX_MSG_SIZE) {
			pr_info("[conn_recv] header invalid msglen=[%d] buflen=[%d]\n",
						header.msg_len, conap_scp_shm_get_slave_rbf_len());
			/* TODO: trigger remote assert */
			//trigger_remote_assert(ch, SCIF_MSG_SIZE_ERROR);
			break;
		}

		conap_scp_shm_collect_msg_body(&header, (uint32_t*)&g_msg_buf[0], SCIF_MAX_MSG_SIZE);

		drv_type  = header.dst_mod_id;

		if (drv_type < CONAP_SCP_DRV_NUM && g_drv_user[drv_type].enable) {
			(*g_drv_user[drv_type].drv_cb.conap_scp_msg_notify_cb)(header.msg_id,
						(unsigned int*)&g_msg_buf[0], header.msg_len - sizeof(struct scif_msg_header));
		}

	}
}

static int _conap_scp_state_change_handler(int cur_state)
{
	int ret = 0, i;
	struct conap_scp_shm_config *shm_config;

	if (mutex_lock_killable(&g_core_ctx.lock))
		return -1;

	/* SCP ready */
	if (cur_state == 1) {
		g_send_seq_num = 0;
		g_recv_seq_num = 0;

		shm_config = conap_scp_get_shm_info();
		if (shm_config == NULL)
			return -1;

		/* reset shm */
		ret = conap_scp_shm_reset(shm_config);
		if (ret) {
			pr_info("[SCP_STATE_CHG] shm reset ret=[%d]", ret);
			return -1;
		}
		/* ipi handshake */
		ret = conap_scp_ipi_handshake();
		if (ret) {
			pr_info("[SCP_STATE_CHG] ipi handshake ret=[%d]", ret);
			return -1;
		}
	}

	for (i = 0; i < CONAP_SCP_DRV_NUM; i++) {
		if (g_drv_user[i].enable && g_drv_user[i].drv_cb.conap_scp_state_notify_cb)
		(*g_drv_user[i].drv_cb.conap_scp_state_notify_cb)(cur_state);
	}
	g_core_ctx.state = cur_state;

	mutex_unlock(&g_core_ctx.lock);
	return 0;
}

static int conap_scp_rx_thread(void *pvData)
{
	struct conap_rb_data *core_data;
	int ret;
	unsigned long flags;

	for (;;) {

		wait_event_interruptible(g_core_ctx.waitQueue, (msg_notify == 1 ||
								conap_core_rb_has_pending_data(&g_core_rb) ||
								kthread_should_stop()));

		if (kthread_should_stop())
			break;

		while (conap_core_rb_has_pending_data(&g_core_rb)) {
			spin_lock_irqsave(&g_core_rb.lock, flags);
			core_data = conap_core_rb_pop_active(&g_core_rb);
			spin_unlock_irqrestore(&g_core_rb.lock, flags);
			if (core_data) {
				if (core_data->param0 == CONAP_CORE_DRV_QRY) {
					ret = -1;
					if (core_data->param1 < CONAP_SCP_DRV_NUM) {
						ret = g_drv_user[core_data->param1].enable;
						pr_info("[%s] DRV_QUERY_ACK type=[%d] ret=[%d]", __func__, core_data->param1, ret);
					}
					ret = conap_scp_ipi_send(DRV_TYPE_CORE, CONAP_SCP_CORE_DRV_QRY_ACK, ret, 0);
					if (ret)
						pr_info("[%s] DRV_QUERY_ACK fail ret=[%d]", __func__, ret);
				} else if (core_data->param0 == CONAP_CORE_SCP_STATE_CHANGE) {

					/* Notify users that state was changed */
					_conap_scp_state_change_handler(core_data->param1);
				}
				spin_lock_irqsave(&g_core_rb.lock, flags);
				conap_core_rb_push_free(&g_core_rb, core_data);
				spin_unlock_irqrestore(&g_core_rb.lock, flags);
			}

		}

#if 0
		/* SCP state Event */
		while (atomic_read(&g_scp_state_ridx) != atomic_read(&g_scp_state_widx)) {

			cur_idx = (atomic_read(&g_scp_state_ridx) + 1) % SCP_STATE_OP_SZ;
			cur_state = g_scp_state_op_cmd[cur_idx];
			atomic_set(&g_scp_state_ridx, cur_idx);
			for (i = 0; i < CONAP_SCP_DRV_NUM; i++) {
				if (g_drv_user[i].enable && g_drv_user[i].drv_cb.conap_scp_state_notify_cb)
					(*g_drv_user[i].drv_cb.conap_scp_state_notify_cb)(cur_state);
			}
			g_core_ctx.state = cur_state;
			pr_info("[SCP_STATE] change to=[%d]", g_core_ctx.state);
		}
#endif

		/* scp was stop */
		if (g_core_ctx.state == 0) {
			continue;
		}
		// need a lock to protect user register/unregister
		read_pkt_from_shm();
		msg_notify = 0;
	}

	return 0;
}

/*********************************************************************/
/* init/deinit */
/*********************************************************************/
int conap_scp_init(unsigned int chip_info, phys_addr_t emi_phy_addr)
{
	int ret, i;
	struct task_struct *p_thread;
	struct conap_scp_ipi_cb ipi_cb;

	memset(&g_core_ctx, 0, sizeof(struct conap_scp_core_ctx));
	g_core_ctx.chip_info = chip_info;
	g_core_ctx.emi_phy_addr = emi_phy_addr;

	ret = connsys_scp_platform_data_init(chip_info, emi_phy_addr);

	/* check if platform support */
	pr_info("[%s] chip_info=[%d] addr[%x] ret=[%d]", __func__, chip_info, emi_phy_addr, ret);

	if (ret) {
		pr_info("[%s] conap not support", __func__);
		return 0;
	}
	g_core_ctx.enable = 1;

	/* init drv_user */
	for (i = 0; i < CONAP_SCP_DRV_NUM; i++)
		memset(&g_drv_user[i], 0, sizeof(struct conap_scp_drv_user));

	/* init shm */
	ret = conap_scp_shm_init(emi_phy_addr);
	if (ret) {
		pr_err("shm init fail [%d]", ret);
		return -1;
	}

	/* tx msg thread */
	ret = msg_thread_init(&g_core_ctx.tx_msg_thread, "conap_scp_core",
					conap_scp_core_opfunc, CONAP_SCP_OPID_MAX);
	if (ret) {
		pr_err("msg thread init fail [%d]", ret);
		return -1;
	}

	mutex_init(&g_core_ctx.lock);

	/* rx thread */
	p_thread = kthread_create(conap_scp_rx_thread,
				&g_core_ctx, "conap_rxd");
	if (IS_ERR(p_thread)) {
		pr_err("[%s] create thread fail", __func__);
		return -1;
	}
	g_core_ctx.rx_thread = p_thread;
	init_waitqueue_head(&g_core_ctx.waitQueue);

	/* core ringbuffer */
	conap_core_rb_init(&g_drv_rdy_rb);
	conap_core_rb_init(&g_core_rb);

	wake_up_process(p_thread);

	/* init ipi */
	ipi_cb.conap_scp_ipi_msg_notify = conap_scp_msg_notify;
	ipi_cb.conap_scp_ipi_ctrl_notify = conap_scp_ipi_ctrl_notify;
	ret = conap_scp_ipi_init(&ipi_cb);
	if (ret) {
		pr_err("msg thread init fail [%d]", ret);
		return -1;
	}

	pr_info("[%s] DONE", __func__);
	return 0;
}

int conap_scp_deinit(void)
{
	conap_core_rb_deinit(&g_drv_rdy_rb);
	conap_core_rb_deinit(&g_core_rb);
	msg_thread_deinit(&g_core_ctx.tx_msg_thread);

	conap_scp_shm_deinit();
	mutex_destroy(&g_core_ctx.lock);
	return 0;
}

