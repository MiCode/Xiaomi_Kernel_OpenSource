// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include <linux/rpmsg.h>
#include "mdw_cmn.h"
#include "mdw_rv.h"
#include "mdw_msg.h"

#define MDW_CMD_IPI_TIMEOUT (2*1000) //ms

static struct mdw_rv_dev mrdev;

static struct mdw_ipi_msg_sync *mdw_rv_dev_get_msg(uint64_t sync_id)
{
	struct mdw_ipi_msg_sync *s_msg = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	mdw_drv_debug("get msg(0x%llx)\n", sync_id);

	mutex_lock(&mrdev.msg_mtx);
	list_for_each_safe(list_ptr, tmp, &mrdev.s_list) {
		s_msg = list_entry(list_ptr, struct mdw_ipi_msg_sync, ud_item);
		if (s_msg->msg.sync_id == sync_id)
			break;
		s_msg = NULL;
	}
	mutex_unlock(&mrdev.msg_mtx);

	return s_msg;
}

static int mdw_rv_dev_send_msg(struct mdw_ipi_msg_sync *s_msg)
{
	int ret = 0;

	s_msg->msg.sync_id = (uint64_t)s_msg;
	mdw_drv_debug("sync id(0x%llx)\n", s_msg->msg.sync_id);

	mutex_lock(&mrdev.msg_mtx);
	list_add_tail(&s_msg->ud_item, &mrdev.s_list);
	mutex_unlock(&mrdev.msg_mtx);

	/* send */
	ret = rpmsg_send(mrdev.ept, &s_msg->msg, sizeof(s_msg->msg));
	if (ret) {
		mdw_drv_err("send msg(0x%llx) fail\n", s_msg->msg.sync_id);
		mutex_lock(&mrdev.msg_mtx);
		list_del(&s_msg->ud_item);
		mutex_unlock(&mrdev.msg_mtx);
	}

	return ret;
}

static void mdw_rv_ipi_cmplt_sync(struct mdw_ipi_msg_sync *s_msg)
{
	mdw_flw_debug("\n");
	complete(&s_msg->cmplt);
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
	s_msg->complete = mdw_rv_ipi_cmplt_sync;

	mutex_lock(&mrdev.mtx);
	/* send */
	ret = mdw_rv_dev_send_msg(s_msg);
	if (ret) {
		mdw_drv_err("send msg fail\n");
		goto fail_send_sync;
	}
	mutex_unlock(&mrdev.mtx);

	/* wait for response */
	if (!wait_for_completion_timeout(&s_msg->cmplt, timeout)) {
		mdw_drv_err("ipi no response\n");
		mutex_lock(&mrdev.msg_mtx);
		list_del(&s_msg->ud_item);
		mutex_unlock(&mrdev.msg_mtx);
		ret = -ETIME;
	} else {
		memcpy(msg, &s_msg->msg, sizeof(*msg));
		ret = msg->ret;
		if (ret)
			mdw_drv_warn("up return fail(%d)\n", ret);
	}

	goto out;

fail_send_sync:
	mutex_unlock(&mrdev.mtx);
out:
	vfree(s_msg);

	return ret;
}

static void mdw_rv_ipi_cmplt_cmd(struct mdw_ipi_msg_sync *s_msg)
{
	struct mdw_rv_cmd *rc =
		container_of(s_msg, struct mdw_rv_cmd, s_msg);

	mdw_flw_debug("\n");
	mdw_rv_cmd_done(rc, 0);
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
	rc->s_msg.complete = mdw_rv_ipi_cmplt_cmd;

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
	mutex_lock(&mrdev.mtx);
	if (atomic_read(&mrdev.clock_flag))
		list_add_tail(&rc->d_item, &mrdev.c_list);
	else
		ret = mdw_rv_dev_send_cmd(rc);
	mutex_unlock(&mrdev.mtx);

	return ret;
}

int mdw_rv_dev_lock(void)
{
	int ret = 0;

	mutex_lock(&mrdev.mtx);
	/* check flag */
	if (atomic_read(&mrdev.clock_flag)) {
		mdw_drv_err("md_msg lock twice\n");
		ret = -EINVAL;
		goto out;
	}

	/* check msg list empty */
	mutex_lock(&mrdev.msg_mtx);
	if (!list_empty_careful(&mrdev.s_list))
		ret = -EBUSY;
	else
		atomic_set(&mrdev.clock_flag, 1);
	mutex_unlock(&mrdev.msg_mtx);

out:
	mutex_unlock(&mrdev.mtx);
	return ret;
}

int mdw_rv_dev_unlock(void)
{
	int ret = 0;

	mutex_lock(&mrdev.mtx);
	/* check flag */
	if (!atomic_read(&mrdev.clock_flag)) {
		mdw_drv_err("md msg unlock twice\n");
		ret = -EINVAL;
		goto out;
	}

	atomic_set(&mrdev.clock_flag, 0);
	schedule_work(&(mrdev.c_wk));

out:
	mutex_unlock(&mrdev.mtx);
	return ret;
}

static void mdw_rv_dev_unlock_func(struct work_struct *wk)
{
	struct mdw_rv_cmd *rc = NULL;
	struct list_head *tmp = NULL, *list_ptr = NULL;

	mutex_lock(&mrdev.mtx);

	if (atomic_read(&mrdev.clock_flag))
		goto out;

	list_for_each_safe(list_ptr, tmp, &mrdev.c_list) {
		rc = list_entry(list_ptr, struct mdw_rv_cmd, d_item);

		mdw_flw_debug("re-trigger cmd(0x%llx)\n", rc->c->kid);
		if (mdw_rv_dev_send_cmd(rc))
			mdw_drv_err("send cmd(0x%llx) fail\n", rc->c->kid);
		else
			list_del(&rc->d_item);
	}

out:
	mutex_unlock(&mrdev.mtx);
}

static int mdw_rv_callback(struct rpmsg_device *rpdev, void *data,
	int len, void *priv, u32 src)
{
	struct mdw_ipi_msg *msg = (struct mdw_ipi_msg *)data;
	struct mdw_ipi_msg_sync *s_msg = NULL;

	mdw_drv_debug("callback msg(%d/0x%llx)\n", msg->id, msg->sync_id);

	s_msg = mdw_rv_dev_get_msg(msg->sync_id);
	if (!s_msg) {
		mdw_drv_err("get msg fail(0x%llx)\n", msg->sync_id);
	} else {
		memcpy(&s_msg->msg, msg, sizeof(*msg));
		mutex_lock(&mrdev.msg_mtx);
		list_del(&s_msg->ud_item);
		mutex_unlock(&mrdev.msg_mtx);
		/* complete callback */
		s_msg->complete(s_msg);
	}

	return 0;
}

int mdw_rv_dev_set_param(uint32_t idx, uint32_t val)
{
	struct mdw_ipi_msg msg;
	int ret = 0;

	memset(&msg, 0, sizeof(msg));
	msg.id = MDW_IPI_PARAM;
	memcpy(&msg.p, &mrdev.param, sizeof(msg.p));
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
		memcpy(&mrdev.param, &msg.p, sizeof(msg.p));

	return ret;
}

uint32_t mdw_rv_dev_get_param(uint32_t idx)
{
	uint32_t ret = 0;

	switch (idx) {
	case MDW_PARAM_UPLOG:
		ret = (int)mrdev.param.uplog;
		break;
	case MDW_PARAM_PREEMPT_POLICY:
		ret = (int)mrdev.param.preempt_policy;
		break;
	case MDW_PARAM_SCHED_POLICY:
		ret = (int)mrdev.param.sched_policy;
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

	mrdev.dev_bitmask = msg.h.basic.dev_bmp;
	/* TODO, rv version */
	mrdev.rv_version = msg.h.basic.version;

	mdw_drv_debug("rv info(%u/0x%llx/%u)\n",
		mrdev.rv_version, mrdev.dev_bitmask);

	do {
		type = find_next_bit((unsigned long *)&mrdev.dev_bitmask,
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

		mrdev.dev_num[msg.h.dev.type] = msg.h.dev.num;
		memcpy(&mrdev.meta_data[msg.h.dev.type][0],
			msg.h.dev.meta, sizeof(msg.h.dev.meta));
		mdw_drv_debug("type(%d) num(%u)\n", type, msg.h.dev.num);
		type++;
	} while (type < APUSYS_DEVICE_MAX);

out:
	return ret;
}

struct mdw_rv_dev *mdw_rv_dev_get(void)
{
	return &mrdev;
}

int mdw_rv_dev_init(struct mdw_device *mdev)
{
	struct rpmsg_channel_info chinfo = {};
	int ret = 0;

	if (!mdev->rpdev || mdev->driver_type != MDW_DRIVER_TYPE_RPMSG) {
		mdw_drv_err("no rpdev(%d)\n", mdev->driver_type);
		ret = -EINVAL;
		goto out;
	}

	memset(&mrdev, 0, sizeof(mrdev));
	mrdev.mdev = mdev;
	mrdev.rpdev = mdev->rpdev;

	strscpy(chinfo.name, mrdev.rpdev->id.name, RPMSG_NAME_SIZE);
	chinfo.src = mrdev.rpdev->src;
	chinfo.dst = RPMSG_ADDR_ANY;
	mrdev.ept = rpmsg_create_ept(mrdev.rpdev,
		mdw_rv_callback, &mrdev, chinfo);
	if (!mrdev.ept) {
		mdw_drv_err("create ept fail\n");
		ret = -ENODEV;
		goto out;
	}

	/* init up dev */
	mutex_init(&mrdev.msg_mtx);
	mutex_init(&mrdev.mtx);
	atomic_set(&mrdev.clock_flag, 0);
	INIT_LIST_HEAD(&mrdev.s_list);
	INIT_LIST_HEAD(&mrdev.c_list);
	INIT_WORK(&(mrdev.c_wk), &mdw_rv_dev_unlock_func);

out:
	return ret;
}

void mdw_rv_dev_deinit(struct mdw_device *mdev)
{
	rpmsg_destroy_ept(mrdev.ept);
}
