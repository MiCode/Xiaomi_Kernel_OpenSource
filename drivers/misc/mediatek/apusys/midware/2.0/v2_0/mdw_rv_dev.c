// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_cmn.h"
#include "mdw_rv.h"
#include "mdw_msg.h"
#ifdef MDW_UP_POC_SUPPORT
#include "apu_ctrl_rpmsg.h"
#endif

#define MDW_CMD_IPI_TIMEOUT (2*1000) //ms

static struct mdw_rv_dev rdev;

#ifdef MDW_UP_POC_SUPPORT
static struct mdw_ipi_msg_sync *mdw_rv_dev_get_msg(uint64_t sync_id)
{
	struct mdw_ipi_msg_sync *s_msg = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	mdw_drv_debug("get msg(0x%llx)\n", sync_id);

	mutex_lock(&rdev.msg_mtx);
	list_for_each_safe(list_ptr, tmp, &rdev.s_list) {
		s_msg = list_entry(list_ptr, struct mdw_ipi_msg_sync, ud_item);
		if (s_msg->msg.sync_id == sync_id)
			break;
		s_msg = NULL;
	}
	mutex_unlock(&rdev.msg_mtx);

	return s_msg;
}
#endif

static int mdw_rv_dev_send_msg(struct mdw_ipi_msg_sync *s_msg)
{
	int ret = 0;

	s_msg->msg.sync_id = (uint64_t)s_msg;
	mdw_drv_debug("sync id(0x%llx)\n", s_msg->msg.sync_id);

	mutex_lock(&rdev.msg_mtx);
	list_add_tail(&s_msg->ud_item, &rdev.s_list);
	mutex_unlock(&rdev.msg_mtx);

	/* send */
#ifdef MDW_UP_POC_SUPPORT
	ret = apu_ctrl_send_msg(MDW_SEND_CMD_ID,
		&s_msg->msg, sizeof(struct mdw_ipi_msg), 0);
#endif
	if (ret) {
		mdw_drv_err("send msg(0x%llx) fail\n", s_msg->msg.sync_id);
		mutex_lock(&rdev.msg_mtx);
		list_del(&s_msg->ud_item);
		mutex_unlock(&rdev.msg_mtx);
	}

	return ret;
}

static int mdw_rv_dev_send_sync(struct mdw_ipi_msg *msg)
{
	int ret = 0;
	struct mdw_ipi_msg_sync *s_msg = NULL;
	unsigned long timeout = msecs_to_jiffies(MDW_CMD_IPI_TIMEOUT);

	s_msg = vzalloc(sizeof(*s_msg));
	if (!s_msg)
		return -ENOMEM;

	memcpy(&s_msg->msg, msg, sizeof(*s_msg));
	init_completion(&s_msg->cmplt);

	mutex_lock(&rdev.mtx);
	/* send */
	ret = mdw_rv_dev_send_msg(s_msg);
	if (ret) {
		mdw_drv_err("send msg fail\n");
		goto fail_send_sync;
	}
	mutex_unlock(&rdev.mtx);

	/* wait for response */
	if (!wait_for_completion_timeout(&s_msg->cmplt, timeout)) {
		mdw_drv_err("ipi no response\n");
		mutex_lock(&rdev.msg_mtx);
		list_del(&s_msg->ud_item);
		mutex_unlock(&rdev.msg_mtx);
		ret = -ETIME;
	} else {
		memcpy(msg, &s_msg->msg, sizeof(*msg));
		ret = msg->ret;
		if (ret)
			mdw_drv_warn("up return fail(%d)\n", ret);
	}

	goto out;

fail_send_sync:
	mutex_unlock(&rdev.mtx);
out:
	vfree(s_msg);

	return ret;
}

static int mdw_rv_dev_send_cmd(struct mdw_rv_cmd *rc)
{
	int ret = 0;

	mdw_drv_debug("pid(%d) run cmd(0x%llx) dva(0x%llx)\n",
		current->pid, rc->c->kid, rc->cb->device_va);

	rc->s_msg.msg.id = MDW_IPI_APU_CMD;
	rc->s_msg.msg.c.iova = rc->cb->device_va;
	rc->s_msg.msg.c.size = rc->cb->size;
	rc->s_msg.msg.c.start_ts_ns = rc->start_ts_ns;

	/* send */
	ret = mdw_rv_dev_send_msg(&rc->s_msg);
	if (ret)
		mdw_drv_err("pid(%d) send msg fail\n", current->pid);

	return ret;
}

int mdw_rv_dev_run_cmd(struct mdw_rv_cmd *rc)
{
	int ret = 0;

	mdw_drv_debug("run rc(0%llx)\n", (uint64_t)rc);
	mutex_lock(&rdev.mtx);
	if (atomic_read(&rdev.clock_flag))
		list_add_tail(&rc->d_item, &rdev.c_list);
	else
		ret = mdw_rv_dev_send_cmd(rc);
	mutex_unlock(&rdev.mtx);

	return ret;
}

int mdw_rv_dev_lock(void)
{
	int ret = 0;

	mutex_lock(&rdev.mtx);
	/* check flag */
	if (atomic_read(&rdev.clock_flag)) {
		mdw_drv_err("md_msg lock twice\n");
		ret = -EINVAL;
		goto out;
	}

	/* check msg list empty */
	mutex_lock(&rdev.msg_mtx);
	if (!list_empty_careful(&rdev.s_list))
		ret = -EBUSY;
	else
		atomic_set(&rdev.clock_flag, 1);
	mutex_unlock(&rdev.msg_mtx);

out:
	mutex_unlock(&rdev.mtx);
	return ret;
}

int mdw_rv_dev_unlock(void)
{
	int ret = 0;

	mutex_lock(&rdev.mtx);
	/* check flag */
	if (!atomic_read(&rdev.clock_flag)) {
		mdw_drv_err("md msg unlock twice\n");
		ret = -EINVAL;
		goto out;
	}

	atomic_set(&rdev.clock_flag, 0);
	schedule_work(&(rdev.c_wk));

out:
	mutex_unlock(&rdev.mtx);
	return ret;
}

static void mdw_rv_dev_unlock_func(struct work_struct *wk)
{
	struct mdw_rv_cmd *rc = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	mutex_lock(&rdev.mtx);

	if (atomic_read(&rdev.clock_flag))
		goto out;

	list_for_each_safe(list_ptr, tmp, &rdev.c_list) {
		rc = list_entry(list_ptr, struct mdw_rv_cmd, d_item);

		mdw_flw_debug("re-trigger cmd(0x%llx)\n", rc->c->kid);
		if (mdw_rv_dev_send_cmd(rc))
			mdw_drv_err("send cmd(0x%llx) fail\n", rc->c->kid);
		else
			list_del(&rc->d_item);
	}

out:
	mutex_unlock(&rdev.mtx);
}

#ifdef MDW_UP_POC_SUPPORT
static void mdw_ipi_callback(u32 id, void *priv, void *data, u32 len)
{
	struct mdw_ipi_msg *msg = (struct mdw_ipi_msg *)data;
	struct mdw_ipi_msg_sync *s_msg = NULL;
	struct mdw_cmd *c = NULL;

	mdw_drv_debug("callback msg(%d/0x%llx)\n", msg->id, msg->sync_id);

	s_msg = mdw_rv_dev_get_msg(msg->sync_id);
	if (!s_msg)
		mdw_drv_err("get msg fail(0x%llx)\n", msg->sync_id);
	else {
		memcpy(&s_msg->msg, msg, sizeof(*msg));
		mutex_lock(&rdev.msg_mtx);
		list_del(&s_msg->ud_item);
		mutex_unlock(&rdev.msg_mtx);

		complete(&s_msg->cmplt);
	}
}
#endif

int mdw_rv_dev_set_param(uint32_t idx, uint32_t val)
{
	struct mdw_ipi_msg msg;
	int ret = 0;

	memset(&msg, 0, sizeof(msg));
	msg.id = MDW_IPI_PARAM;
	memcpy(&msg.p, &rdev.param, sizeof(msg.p));
	switch (idx) {
	case MDW_PARAM_UPLOG:
		msg.p.uplog = val;
		break;
	case MDW_PARAM_PREEMPT_POLICY:
		msg.p.preempt_policy = val;
		break;
	case MDW_PARAM_SCHED_POLICY:
		msg.p.sched_policy = val;
		break;
	default:
		return -EINVAL;
	}

	ret = mdw_rv_dev_send_sync(&msg);
	if (!ret)
		memcpy(&rdev.param, &msg.p, sizeof(msg.p));

	return ret;
}

uint32_t mdw_rv_dev_get_param(uint32_t idx)
{
	uint32_t ret = 0;

	switch (idx) {
	case MDW_PARAM_UPLOG:
		ret = (int)rdev.param.uplog;
		break;
	case MDW_PARAM_PREEMPT_POLICY:
		ret = (int)rdev.param.preempt_policy;
		break;
	case MDW_PARAM_SCHED_POLICY:
		ret = (int)rdev.param.sched_policy;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

int mdw_rv_dev_handshake(void)
{
	struct mdw_ipi_msg msg;
	int ret = 0, type = 0;

	memset(&msg, 0, sizeof(msg));
	msg.id = MDW_IPI_HANDSHAKE;
	msg.h.h_id = MDW_IPI_HANDSHAKE_BASIC_INFO;
	ret = mdw_rv_dev_send_sync(&msg);
	if (ret)
		goto out;

	if (msg.id != MDW_IPI_HANDSHAKE ||
		msg.h.h_id != MDW_IPI_HANDSHAKE_BASIC_INFO) {
		ret = -EINVAL;
		goto out;
	}

	rdev.dev_bitmask = msg.h.basic.dev_bmp;
	/* TODO, rv version */
	rdev.rv_version = msg.h.basic.version;

	mdw_drv_debug("rv info(%u/0x%llx/%u)\n",
		rdev.rv_version, rdev.dev_bitmask);

	do {
		type = find_next_bit((unsigned long *)&rdev.dev_bitmask,
			APUSYS_DEVICE_MAX, type);
		if (type >= APUSYS_DEVICE_MAX)
			break;

		memset(&msg, 0, sizeof(msg));
		msg.id = MDW_IPI_HANDSHAKE;
		msg.h.h_id = MDW_IPI_HANDSHAKE_DEV_NUM;
		msg.h.dev.type = type;
		ret = mdw_rv_dev_send_sync(&msg);
		if (ret)
			break;

		if (msg.id != MDW_IPI_HANDSHAKE ||
			msg.h.h_id != MDW_IPI_HANDSHAKE_DEV_NUM) {
			ret = -EINVAL;
			break;
		}

		rdev.dev_num[msg.h.dev.type] = msg.h.dev.num;
		mdw_drv_debug("type(%d) num(%u)\n", type, msg.h.dev.num);
		type++;
	} while (type < APUSYS_DEVICE_MAX);

out:
	return ret;
}

struct mdw_rv_dev *mdw_rv_dev_get(void)
{
	return &rdev;
}

int mdw_rv_dev_init(void)
{
	int ret = 0;

	memset(&rdev, 0, sizeof(rdev));

	/* register ipi for cmd send */
#ifdef MDW_UP_POC_SUPPORT
	ret = apu_ctrl_register_channel(MDW_SEND_CMD_ID, NULL, &rdev,
		&rdev.tmp_msg, sizeof(rdev.tmp_msg));
#endif
	if (ret) {
		mdw_drv_err("register send ipi fail\n");
		goto out;
	}

	/* register ipi for cmd send */
#ifdef MDW_UP_POC_SUPPORT
	ret = apu_ctrl_register_channel(MDW_RECV_CMD_ID, mdw_ipi_callback,
		&rdev, &rdev.msg, sizeof(rdev.msg));
#endif
	if (ret) {
		mdw_drv_err("register recv ipi fail\n");
		goto out;
	}

	/* init up dev */
	mutex_init(&rdev.msg_mtx);
	mutex_init(&rdev.mtx);
	atomic_set(&rdev.clock_flag, 0);
	INIT_LIST_HEAD(&rdev.s_list);
	INIT_LIST_HEAD(&rdev.c_list);
	INIT_WORK(&(rdev.c_wk), &mdw_rv_dev_unlock_func);

out:
	return ret;
}

void mdw_rv_dev_deinit(void)
{
}
