// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2022 Xiaomi, Inc.
 */

#include <linux/slab.h>
#include <linux/rpmsg.h>
#include <linux/debugfs.h>
#include <linux/atomic.h>
#include "../rproc/ispv4_rproc.h"
#include "ispv4_rpmsg.h"
#include <linux/mfd/ispv4_defs.h>
#include <linux/component.h>
#include <linux/thermal.h>
#include <linux/mfd/core.h>

#define CONTROL_THERMAL
#define CMD_COMPLETE_TIMEOUT 500
#define PRE_FRAME_MSG_TIMEOUT_MS 100
#define PRE_FRAME_MSG_TIMERCHECK 10

extern struct dentry *ispv4_debugfs;

static struct mfd_cell ispv4_thermal_cell = {
	.name = "ispv4_thermal",
	.ignore_resource_conflicts = true,
};

static int xm_ipc_notify_us(struct xm_ispv4_rproc *rp, enum xm_ispv4_etps ept,
			    void *data, int len, bool err);

static struct xm_ispv4_rproc *rpdev_to_xmrp(struct rpmsg_device *rpdev)
{
	struct device *rproc_dev;
	struct xm_ispv4_rproc *rp;
	// rpmsg-dev -> vdev -> rvdev -> rproc-dev
	rproc_dev = rpdev->dev.parent->parent->parent;
	rp = container_of(rproc_dev, struct rproc, dev)->priv;
	// BUG_ON(rp->magic_num != 0x1234abcd);
	return rp;
}

static ssize_t debugfs_rpmsg_send(struct file *file,
				  const char __user *user_buf, size_t count,
				  loff_t *ppos)
{
	struct rpmsg_device *rpdev = file->private_data;
	u32 data[64];
	u32 len = min_t(unsigned int, sizeof(data), count);

	(void)copy_from_user(data, user_buf, len);
	rpmsg_send(rpdev->ept, data, len);

	return count;
}

static struct dentry *rpmsg_debugfs[2];
static const struct file_operations rpmsg_debugfs_fops = {
	.open = simple_open,
	.write = debugfs_rpmsg_send,
};

long ispv4_eptdev_isp_send(struct xm_ispv4_rproc *rp, enum xm_ispv4_etps ept,
			   u32 cmd, int len, void *data, bool user_buf,
			   int *msgid)
{
	int ret;
	u32 rpmsg_id = 0;
	bool wack = false;
	bool wait_cmd = false;
	bool poped = false;
	struct rpept_wt_ack *wack_node = NULL;
	struct rpept_wt_ack *nd_waiting, *nw;
	bool nd_waiting_release = false;
	struct ispv4_rpept_dev *epdev;
	struct rpept_recv_msg *rmsg, *n;
	u8 buf[512];
	HEADER_FORMAT_PTR(msg_f, data);
	HEADER_FORMAT_PTR(msg_f_inner, buf);
	HEADER_FORMAT_PTR(msg_r, NULL);
	void *send_buf = NULL;

#define AVALID_MSG_LEN (512 - 16)

	if (ept == XM_ISPV4_IPC_EPT_MAX)
		return -EFAULT;

	if (len > AVALID_MSG_LEN)
		return -ENOPARAM;

	switch (cmd) {
	case MIPC_MSGHEADER_CMD:
		wait_cmd = true;
		break;
	case MIPC_MSGHEADER_CMD_NEED_ACK:
		pr_err("ispv4 mipc not support cmd need ack %ld\n", cmd);
		pr_err("ispv4 mipc cmd changed to sync call %ld\n", cmd);
		ret = -ENOPARAM;
		goto err;
		// wait_cmd = true;
		// wack = true;
		// break;
	case MIPC_MSGHEADER_COMMON:
		break;
	case MIPC_MSGHEADER_COMMON_NEED_ACK:
		wack = true;
		break;
	default:
		pr_warn("ispv4 send unknown command %ld\n", cmd);
		ret = -ENOPARAM;
		goto err;
	}

	if (user_buf) {
		ret = copy_from_user(buf, data, len);
		if (ret != 0)
			return -ENOPARAM;
	}

	pr_info("ispv4 into %s for cmd=%ld\n", __func__, cmd);
	mutex_lock(&rp->rpeptdev_lock[ept]);
	if (rp->ipc_stopsend[ept] || rp->rpeptdev[ept] == NULL) {
		mutex_unlock(&rp->rpeptdev_lock[ept]);
		return -ENODEV;
	}

	epdev = rp->rpeptdev[ept];
	rpmsg_id = atomic_fetch_inc(&epdev->msg_id);
	rpmsg_id &= MIPC_MSGID_MAX;
	rpmsg_id |= MIPC_MSGID_SEND2V4_CID << MIPC_MSGID_SHIFT;

	if (user_buf) {
		msg_f_inner->header.msg_header_id = rpmsg_id;
		msg_f_inner->header.msg_header_type = cmd;
		(void)copy_to_user(data, buf, MIPC_CMD_HEADER_SIZE);
		send_buf = msg_f_inner;
	} else {
		msg_f->header.msg_header_id = rpmsg_id;
		msg_f->header.msg_header_type = cmd;
		send_buf = msg_f;
	}

	if (msgid != NULL)
		*msgid = rpmsg_id;

	if (wack) {
		wack_node = kmalloc(sizeof(*wack_node), GFP_KERNEL);
		if (wack_node == NULL) {
			ret = -ENOMEM;
			goto err;
		}
		mutex_lock(&epdev->up_lock);
		wack_node->touttime = ktime_add(
			ktime_get(), ms_to_ktime(PRE_FRAME_MSG_TIMEOUT_MS));
		wack_node->msg_header_id = rpmsg_id;
		list_add_tail(&wack_node->node, &epdev->waiting_ack);
		mutex_unlock(&epdev->up_lock);
		pr_warn("ispv4 waiting ack for msg id %d", rpmsg_id);
	}

	if (wait_cmd)
		reinit_completion(&epdev->cmd_complete);

	pr_info("ispv4 rpmsg send %s for cmd=%ld msg id %d\n", __func__, cmd, rpmsg_id);
	ret = rpmsg_send(epdev->rpdev->ept, send_buf, len);
	if (ret != 0) {
		if (wack) {
			mutex_lock(&epdev->up_lock);
			list_for_each_entry_safe (nd_waiting, nw,
						  &epdev->waiting_ack, node) {
				if (nd_waiting->msg_header_id == rpmsg_id) {
					list_del(&nd_waiting->node);
					nd_waiting_release = true;
					break;
				}
			}
			mutex_unlock(&epdev->up_lock);
			if (nd_waiting_release) {
				kfree(nd_waiting);
				pr_warn("ispv4 ack for msg id %d drop(send failed)",
					rpmsg_id);
			} else {
				pr_err("ispv4 ack for msg id %d not found(send failed)",
				       rpmsg_id);
			}
		}
		goto err;
	}


	if (wait_cmd) {
		smp_rmb();
		if (unlikely(rp->ipc_stopsend[ept])) {
			ret = -ENOLINK;
			goto err;
		}
		ret = wait_for_completion_killable_timeout(
			&epdev->cmd_complete,
			msecs_to_jiffies(CMD_COMPLETE_TIMEOUT));
		if (ret == 0) {
			ret = -ETIME;
			goto err;
		} else if (ret < 0) {
			/* be killed */
			goto err;
		}

		mutex_lock(&epdev->up_lock);
		list_for_each_entry_safe (rmsg, n, &epdev->up_rets, node) {
			list_del(&rmsg->node);
			msg_r = (void *)rmsg->data;
			if (msg_r->header.msg_header_id == rpmsg_id) {
				poped = true;
				break;
			} else {
				pr_warn("ispv4 mipc drop msgid %ld in cmd %ld\n",
					msg_r->header.msg_header_id, cmd);
				dump_stack();
				kfree(rmsg);
			}
		}
		mutex_unlock(&epdev->up_lock);
		if (poped) {
			if (rmsg->len_of_data > len) {
				pr_err("ispv4 mipc too small buf for get return val cmd: %ld need ret buf %ld\n",
				       cmd, rmsg->len_of_data);
				ret = -ENOPARAM;
				kfree(rmsg);
				goto err;
			}

			if (user_buf)
				(void)copy_to_user(data, rmsg->data,
						   rmsg->len_of_data);
			else
				memcpy(data, rmsg->data, rmsg->len_of_data);

			kfree(rmsg);
		} else {
			/* Recv error id or Earlydown */
			pr_err("ispv4 mipc message id not match %ld \n", cmd);
			ret = -ENOLINK;
		}
	}

	mutex_unlock(&rp->rpeptdev_lock[ept]);
	return 0;

err:
	mutex_unlock(&rp->rpeptdev_lock[ept]);
	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_eptdev_isp_send);

long ispv4_eptdev_isp_recv(struct xm_ispv4_rproc *rp, enum xm_ispv4_etps ept,
			   int cap, void *data, bool user_buf)
{
	struct ispv4_rpept_dev *epdev = NULL;
	struct rpept_recv_msg *msg, *n;
	bool poped = false;
	int ret = 0;

	mutex_lock(&rp->rpeptdev_lock[ept]);
	if (rp->rpeptdev[ept] == NULL) {
		mutex_unlock(&rp->rpeptdev_lock[ept]);
		return -ENODEV;
	}
	epdev = rp->rpeptdev[ept];

	mutex_lock(&epdev->up_lock);
	list_for_each_entry_safe (msg, n, &epdev->up_msgs, node) {
		list_del(&msg->node);
		poped = true;
		break;
	}
	mutex_unlock(&epdev->up_lock);

	if (poped) {
		if (msg->len_of_data > cap) {
			ret = -ENOPARAM;
			mutex_lock(&epdev->up_lock);
			list_add(&msg->node, &epdev->up_msgs);
			mutex_unlock(&epdev->up_lock);
			xm_ipc_notify_us(rp, ept, msg->data,
					 msg->len_of_data, false);
			goto err;
		}

		if (user_buf) {
			ret = copy_to_user(data, msg->data, msg->len_of_data);
			if (ret != 0) {
				mutex_lock(&epdev->up_lock);
				list_add(&msg->node, &epdev->up_msgs);
				mutex_unlock(&epdev->up_lock);
				xm_ipc_notify_us(rp, ept, msg->data,
						 msg->len_of_data, false);
				ret = -EFAULT;
				goto err;
			}
			kfree(msg);
		} else {
			memcpy(data, msg->data, msg->len_of_data);
			kfree(msg);
		}
	} else {
		ret = -ENOMSG;
		goto err;
	}

	mutex_unlock(&rp->rpeptdev_lock[ept]);
	return 0;

err:
	mutex_unlock(&rp->rpeptdev_lock[ept]);
	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_eptdev_isp_recv);

long ispv4_eptdev_isp_pop_err(struct xm_ispv4_rproc *rp, enum xm_ispv4_etps ept,
			      int cap, void *data, bool user_buf)
{
	struct ispv4_rpept_dev *epdev = NULL;
	struct rpept_wt_ack *msg_err, *n;
	bool poped = false;
	int ret = 0;

	mutex_lock(&rp->rpeptdev_lock[ept]);
	if (rp->rpeptdev[ept] == NULL) {
		mutex_unlock(&rp->rpeptdev_lock[ept]);
		return -ENODEV;
	}
	epdev = rp->rpeptdev[ept];

	mutex_lock(&epdev->up_lock);
	list_for_each_entry_safe (msg_err, n, &epdev->up_errs, node) {
		list_del(&msg_err->node);
		poped = true;
		break;
	}
	mutex_unlock(&epdev->up_lock);

	if (sizeof(msg_err->msg_header_id) > cap) {
		ret = -ENOPARAM;
		mutex_lock(&epdev->up_lock);
		list_add(&msg_err->node, &epdev->up_msgs);
		mutex_unlock(&epdev->up_lock);
		xm_ipc_notify_us(rp, ept, &msg_err->msg_header_id,
				 sizeof(msg_err->msg_header_id), true);
		goto err;
	}

	if (poped) {
		if (user_buf) {
			ret = copy_to_user(data, &msg_err->msg_header_id,
					   sizeof(msg_err->msg_header_id));
			if (ret != 0) {
				mutex_lock(&epdev->up_lock);
				list_add(&msg_err->node, &epdev->up_errs);
				mutex_unlock(&epdev->up_lock);
				xm_ipc_notify_us(rp, ept, &msg_err->msg_header_id,
						 sizeof(msg_err->msg_header_id),
						 true);
				ret = -EFAULT;
				goto err;
			}
			kfree(msg_err);
		} else {
			memcpy(data, &msg_err->msg_header_id,
			       sizeof(msg_err->msg_header_id));
			kfree(msg_err);
		}
	} else {
		ret = -ENOMSG;
		goto err;
	}

	mutex_unlock(&rp->rpeptdev_lock[ept]);
	return 0;
err:
	mutex_unlock(&rp->rpeptdev_lock[ept]);
	return ret;
}
EXPORT_SYMBOL_GPL(ispv4_eptdev_isp_pop_err);

static void rpept_device_release(struct device *dev)
{
	struct rpept_wt_ack *msg_err, *ne;
	struct rpept_recv_msg *msg, *nm;
	struct rpept_recv_msg *mret, *nr;
	struct rpept_wt_ack *nd_waiting, *nw;
	struct ispv4_rpept_dev *epdev =
		container_of(dev, struct ispv4_rpept_dev, dev);
	dev_info(dev, "%s entry\n", __FUNCTION__);
	atomic_set(&epdev->msg_id, 0);

	del_timer_sync(&epdev->cacktimer);
	cancel_work_sync(&epdev->cackwork);

	list_for_each_entry_safe (msg_err, ne, &epdev->up_errs, node) {
		list_del(&msg_err->node);
		kfree(msg_err);
	}
	list_for_each_entry_safe (msg, nm, &epdev->up_msgs, node) {
		list_del(&msg->node);
		kfree(msg);
	}
	list_for_each_entry_safe (mret, nr, &epdev->up_rets, node) {
		list_del(&mret->node);
		kfree(mret);
	}
	list_for_each_entry_safe (nd_waiting, nw, &epdev->waiting_ack, node) {
		list_del(&nd_waiting->node);
		kfree(nd_waiting);
	}
	kfree(epdev);
}

static void rpept_check_ack_work(struct work_struct *work)
{
	struct ispv4_rpept_dev *epdev =
		container_of(work, struct ispv4_rpept_dev, cackwork);
	ktime_t nt = ktime_get();
	struct rpept_wt_ack *wack, *tmp;
	bool meet_tout = false;
	u32 mid;

	mutex_lock(&epdev->up_lock);
	list_for_each_entry_safe (wack, tmp, &epdev->waiting_ack, node) {
		if (ktime_compare(nt, wack->touttime) >= 0) {
			meet_tout = true;
			mid = wack->msg_header_id;
			list_del(&wack->node);
			// TODO
			list_add_tail(&wack->node, &epdev->up_errs);
		} else
			break;
	}
	mutex_unlock(&epdev->up_lock);

	if (meet_tout) {
		// TODO
		//xm_ipc_notify_us(epdev->rp, epdev->idx, NULL, 0, true);
		dev_info(&epdev->dev, "mailbox ack timeout id = %d!\n", mid);
	}
}

static void rpept_check_ack_timer(struct timer_list *t)
{
	struct ispv4_rpept_dev *epdev =
		container_of(t, struct ispv4_rpept_dev, cacktimer);
	schedule_work(&epdev->cackwork);

	mod_timer(t, jiffies + msecs_to_jiffies(PRE_FRAME_MSG_TIMERCHECK));
}

static int ispv4_rpisp_regiser_cb(struct xm_ispv4_rproc *rp,
				  enum xm_ispv4_etps ept,
				  void (*cb)(void *, void *, void *, int),
				  void *priv)
{
	rp->rpmsgnotify[ept] = cb;
	rp->rpmsgnotify_priv[ept] = priv;
	return 0;
}

static int ispv4_comp_bind(struct device *comp, struct device *master,
			   void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;

	if (atomic_fetch_inc(&priv->v4l2_rpmsg.avalid_num) == 0) {
		// priv->v4l2_rpmsg.ctrl = NULL;
		priv->v4l2_rpmsg.register_cb = ispv4_rpisp_regiser_cb;
		priv->v4l2_rpmsg.send = ispv4_eptdev_isp_send;
		priv->v4l2_rpmsg.recv = ispv4_eptdev_isp_recv;
		priv->v4l2_rpmsg.get_err = ispv4_eptdev_isp_pop_err;
		priv->v4l2_rpmsg.epdev =
			container_of(comp, struct ispv4_rpept_dev, dev);
		priv->v4l2_rpmsg.rp = priv->v4l2_rpmsg.epdev->rp;
		priv->v4l2_rpmsg.avalid = true;
	}

	return 0;
}

static void ispv4_comp_unbind(struct device *comp, struct device *master,
			      void *master_data)
{
	struct ispv4_v4l2_dev *priv = master_data;
	if (atomic_fetch_dec(&priv->v4l2_rpmsg.avalid_num) == 1)
		priv->v4l2_rpmsg.avalid = false;
}

__maybe_unused static const struct component_ops comp_ops = {
	.bind = ispv4_comp_bind,
	.unbind = ispv4_comp_unbind
};

static int xm_ipc_rpmsg_probe(struct rpmsg_device *rpdev)
{
	struct xm_ispv4_rproc *rp;
	int idx, ret;
	char debugfs_name[32];
	struct ispv4_rpept_dev *epdev;

	rp = rpdev_to_xmrp(rpdev);
	if (!strcmp(rpdev->id.name, XM_ISPV4_IPC_EPT_RPMSG_ISP_NAME)) {
		idx = XM_ISPV4_IPC_EPT_RPMSG_ISP;
	} else if (!strcmp(rpdev->id.name, XM_ISPV4_IPC_EPT_RPMSG_ASST_NAME)) {
		idx = XM_ISPV4_IPC_EPT_RPMSG_ASST;
	} else {
		dev_err(rp->dev, "probe unknown epts %s\n", rpdev->id.name);
		return -1;
	}

	dev_info(rp->dev, "find epts %s\n", rpdev->id.name);

	rp->ipc_epts[idx] = rpdev->ept;
	rpdev->ept->priv = rp;

	sprintf(debugfs_name, "ispv4-%s", rpdev->id.name);
	rpmsg_debugfs[idx] = debugfs_create_file(
		debugfs_name, 0444, ispv4_debugfs, rpdev, &rpmsg_debugfs_fops);
	if (IS_ERR_OR_NULL(rpmsg_debugfs[idx])) {
		dev_err(rp->dev, "debugfs create failed %s\n", debugfs_name);
	}

	ret = rpmsg_send(rpdev->ept, &idx, sizeof(idx));
	if (ret != 0) {
		dev_err(rp->dev, "rpmsg ept %d first msg send failed %d\n", idx,
			ret);
		goto init_send_err;
	}

	epdev = kzalloc(sizeof(*epdev), GFP_KERNEL);
	if (epdev == NULL) {
		ret = -ENOMEM;
		goto err_alloc;
	}
	// mutex_init(&epdev->remove_mutex);
	rp->rpeptdev[idx] = epdev;
	epdev->rp = rp;
	epdev->idx = idx;
	epdev->rpdev = rpdev;

	INIT_LIST_HEAD(&epdev->up_msgs);
	INIT_LIST_HEAD(&epdev->up_errs);
	INIT_LIST_HEAD(&epdev->up_rets);
	INIT_LIST_HEAD(&epdev->waiting_ack);
	init_completion(&epdev->cmd_complete);
	INIT_WORK(&epdev->cackwork, rpept_check_ack_work);
	timer_setup(&epdev->cacktimer, rpept_check_ack_timer, 0);
	add_timer(&epdev->cacktimer);

	mutex_init(&epdev->up_lock);
	device_initialize(&epdev->dev);
	dev_set_name(&epdev->dev, "ispv4-%s", rpdev->id.name);
	epdev->dev.release = rpept_device_release;
	ret = component_add(&epdev->dev, &comp_ops);
	if (ret != 0)
		goto comp_add_err;

	if (rp->rpmsg_ready_cb != NULL)
		rp->rpmsg_ready_cb(rp->rpmsg_ready_cb_thiz, idx, true);

#ifdef CONTROL_THERMAL
	if (idx == XM_ISPV4_IPC_EPT_RPMSG_ASST) {
		ret = mfd_add_devices(&rpdev->dev, PLATFORM_DEVID_NONE,
				      &ispv4_thermal_cell, 1, NULL, 0, NULL);
		if (ret) {
			dev_err(&epdev->dev, "mfd add thermal failed!\n");
		} else {
			dev_info(&epdev->dev, "mfd add thermal success!\n");
		}
	}
#endif

	dev_info(rp->dev, "epts %s probe success\n", rpdev->id.name);
	return 0;

comp_add_err:
	kfree(epdev);
err_alloc:
init_send_err:
	dev_info(rp->dev, "epts %s remove debugfs\n", rpdev->id.name);
	debugfs_remove(rpmsg_debugfs[idx]);
	return ret;
}

void xm_ipc_rpmsg_remove(struct rpmsg_device *rpdev)
{
	struct xm_ispv4_rproc *rp;
	int idx;

	rp = rpdev_to_xmrp(rpdev);
	if (!strcmp(rpdev->id.name, XM_ISPV4_IPC_EPT_RPMSG_ISP_NAME)) {
		idx = XM_ISPV4_IPC_EPT_RPMSG_ISP;
	} else if (!strcmp(rpdev->id.name, XM_ISPV4_IPC_EPT_RPMSG_ASST_NAME)) {
		idx = XM_ISPV4_IPC_EPT_RPMSG_ASST;
	} else {
		dev_err(rp->dev, "remove unknown epts %s\n", rpdev->id.name);
		return;
	}

	if (rp->rpmsg_ready_cb != NULL)
		rp->rpmsg_ready_cb(rp->rpmsg_ready_cb_thiz, idx, false);

#ifdef CONTROL_THERMAL
	if (idx == XM_ISPV4_IPC_EPT_RPMSG_ASST) {
		mfd_remove_devices(&rpdev->dev);
	}
#endif

	if (!IS_ERR_OR_NULL(rpmsg_debugfs[idx])) {
		debugfs_remove(rpmsg_debugfs[idx]);
		rpmsg_debugfs[idx] = NULL;
	}

	component_del(&rp->rpeptdev[idx]->dev, &comp_ops);
	mutex_lock(&rp->rpeptdev_lock[idx]);
	put_device(&rp->rpeptdev[idx]->dev);
	rp->rpeptdev[idx] = NULL;
	mutex_unlock(&rp->rpeptdev_lock[idx]);

	dev_info(rp->dev, "remove epts %s\n", rpdev->id.name);
	rp->ipc_epts[idx] = NULL;
}

static int xm_ipc_notify_us(struct xm_ispv4_rproc *rp, enum xm_ispv4_etps ept,
			    void *data, int len, bool err)
{
	u64 id = err ? MIISP_V4L_EVENT_RPMSG_ISP_ERR :
		       MIISP_V4L_EVENT_RPMSG_ISP_RECV;
	if (rp->rpmsgnotify[ept] != NULL)
		rp->rpmsgnotify[ept](rp->rpmsgnotify_priv[ept], (void*)id,
				     data, len);
	return 0;
}

void xm_ipc_knotify_register(struct xm_ispv4_rproc *rp,
			     void (*fn)(void *priv, void *data, int len),
			     void *p)
{
	rp->krpmsg_notify_priv = p;
	rp->krpmsg_notify = fn;
}
EXPORT_SYMBOL_GPL(xm_ipc_knotify_register);

static int __xm_ipc_rpmsg_cb(struct rpmsg_device *rpdev, void *data, int len,
			     void *priv, u32 addr, int idx)
{
	struct xm_ispv4_rproc *rp;
	struct ispv4_rpept_dev *epdev;
	struct rpept_wt_ack *nd_waiting, *nw;
	bool waiting_ack_ok = false;
	mipc_cmd_format_t ack_buf;
	struct rpept_recv_msg *recv_msg;
	u8 header[16];
	mipc_cmd_format_t *msg_f;
	int cid;

	memcpy_fromio(header, data, sizeof(mipc_cmd_format_t));
	msg_f = FORMAT_RAW_CMD(header);

	rp = rpdev_to_xmrp(rpdev);
	if (len < MIPC_CMD_HEADER_SIZE) {
		dev_err(rp->dev, "epts %s recv error msg (illegal len).\n",
			rpdev->id.name);
		return 0;
	}

	epdev = rp->rpeptdev[idx];
	cid = (msg_f->header.msg_header_id & MIPC_MSGID_MASK) >>
	      MIPC_MSGID_SHIFT;
	if (cid == MIPC_MSGID_KSEND2V4_CID) {
		if (rp->krpmsg_notify != NULL)
			rp->krpmsg_notify(rp->krpmsg_notify_priv, data, len);
		return 0;
	}

	switch (msg_f->header.msg_header_type) {
	case MIPC_MSGHEADER_CMD:
	case MIPC_MSGHEADER_CMD_NEED_ACK:
		dev_err(rp->dev, "epts %s recv error msg (illegal type %d).\n",
			rpdev->id.name, msg_f->header.msg_header_type);
		break;
	case MIPC_MSGHEADER_ACK:
		/* Remove node from ack-waiting list. */
		mutex_lock(&epdev->up_lock);
		list_for_each_entry_safe (nd_waiting, nw, &epdev->waiting_ack,
					  node) {
			if (nd_waiting->msg_header_id ==
			    msg_f->header.msg_header_id) {
				list_del(&nd_waiting->node);
				waiting_ack_ok = true;
				break;
			}
		}
		mutex_unlock(&epdev->up_lock);
		if (waiting_ack_ok) {
			kfree(nd_waiting);
			dev_info(rp->dev, "recv ack id = %d",
				 msg_f->header.msg_header_id);
		} else
			dev_err(rp->dev,
				"epts %s recv error msg (unexpected ack: id %d).\n",
				rpdev->id.name, msg_f->header.msg_header_id);

		break;
	case MIPC_MSGHEADER_COMMON_NEED_ACK:
		ack_buf.header.msg_header_id = msg_f->header.msg_header_id;
		ack_buf.header.msg_header_type = MIPC_MSGHEADER_ACK;
		dev_err(rp->dev,"cannot request master ack");
		// rpmsg_send(rpdev->ept, &ack_buf, sizeof(ack_buf));
		xm_ipc_notify_us(rp, idx, data, len, false);
		break;
	case MIPC_MSGHEADER_COMMON:
		xm_ipc_notify_us(rp, idx, data, len, false);
		break;
	case MIPC_MSGHEADER_RET_V:
	case MIPC_MSGHEADER_RET_D:
		/* Push message to use space. */
		recv_msg = kmalloc(sizeof(*recv_msg) + len, GFP_KERNEL);
		if (recv_msg == NULL) {
			dev_err(rp->dev, "alloc msg buf failed!!\n");
			break;
		}
		recv_msg->len_of_data = len;
		memcpy_fromio(recv_msg->data, data, len);
		mutex_lock(&epdev->up_lock);
		list_add_tail(&recv_msg->node, &epdev->up_rets);
		mutex_unlock(&epdev->up_lock);
		complete(&epdev->cmd_complete);
		// xm_ipc_notify_us(rp, idx, false);
		break;
	default:
		dev_err(rp->dev, "epts %s recv error msg (unknown type %d).\n",
			rpdev->id.name, msg_f->header.msg_header_type);
	}

	return 0;
}

static int xm_ipc_rpmsg_cb(struct rpmsg_device *rpdev, void *data, int len,
			   void *priv, u32 addr)
{
	struct xm_ispv4_rproc *rp;
	int idx;

	rp = rpdev_to_xmrp(rpdev);
	for (idx = 0; idx < XM_ISPV4_IPC_EPT_MAX; idx++) {
		if (rp->ipc_epts[idx] == rpdev->ept)
			break;
	}
	if (idx == XM_ISPV4_IPC_EPT_MAX) {
		dev_err(rp->dev, "rpmsg-cb: unknown epts %s\n", rpdev->id.name);
		return 0;
	}

	return __xm_ipc_rpmsg_cb(rpdev, data, len, priv, addr, idx);
}

static const struct rpmsg_device_id xm_ipc_rpmsg_match[] = {
	{ XM_ISPV4_IPC_EPT_RPMSG_ISP_NAME },
	{ XM_ISPV4_IPC_EPT_RPMSG_ASST_NAME },
	{},
};

struct rpmsg_driver xm_ipc_rpmsg_driver = {
	.probe = xm_ipc_rpmsg_probe,
	.remove = xm_ipc_rpmsg_remove,
	.callback = xm_ipc_rpmsg_cb,
	.id_table = xm_ipc_rpmsg_match,
	.drv = {
		.name = "xm-ipc-rpmsg",
	},
};
