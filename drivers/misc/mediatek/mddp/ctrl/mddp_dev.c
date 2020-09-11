/*
 * mddp_dev.c - MDDP device node API.
 *
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/cdev.h>
#include <linux/types.h>
#include <linux/string.h>

#include "mddp_ctrl.h"
#include "mddp_dev.h"
#include "mddp_if.h"
#include "mddp_sm.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
#define MDDP_DEV_MINOR_BASE             0
#define MDDP_DEV_MINOR_CNT              16
#define MDDP_CLASS_NAME                 "md_direct"
#define MDDP_DEV_NAME                   "mddp"
#define MAX_GET_BUF_SZ                  256

struct mddp_dev_rb_t {
	struct mddp_dev_rb_t           *next;
	struct mddp_dev_rb_t           *prev;

	uint32_t                        len;
	struct mddp_dev_rsp_common_t   *dev_rsp;
};

struct mddp_dev_rb_head_t {
	struct mddp_dev_rb_t           *next;
	struct mddp_dev_rb_t           *prev;

	uint32_t                        cnt;
	spinlock_t                      locker;
	wait_queue_head_t               read_wq;
};

//------------------------------------------------------------------------------
// Private helper macro.
//------------------------------------------------------------------------------
#define MDDP_DEV_RB_LOCK_INIT(_locker) spin_lock_init(&_locker)
#define MDDP_DEV_RB_LOCK(_locker) spin_lock(&_locker)
#define MDDP_DEV_RB_UNLOCK(_locker) spin_unlock(&_locker)

#define MDDP_DEV_RB_LOCK_IRQ(_locker) spin_lock_irq(&_locker)
#define MDDP_DEV_RB_UNLOCK_IRQ(_locker) spin_unlock_irq(&_locker)

#define MDDP_DEV_CLONE_COMM_HDR(_rsp, _req, _status) \
	do { \
		(_rsp)->mcode = (_req)->mcode; \
		(_rsp)->status = _status; \
		(_rsp)->app_type = (_req)->app_type; \
		(_rsp)->msg = (_req)->msg; \
	} while (0)

//------------------------------------------------------------------------------
// Private variables.
//------------------------------------------------------------------------------
static const struct file_operations mddp_dev_fops = {
	.owner          = THIS_MODULE,
	.open           = &mddp_dev_open,
	.read           = &mddp_dev_read,
	.write          = &mddp_dev_write,
	.release        = &mddp_dev_close,
	.unlocked_ioctl = &mddp_dev_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl   = &mddp_dev_compat_ioctl,
#endif
	.poll           = &mddp_dev_poll,
};

static atomic_t mddp_dev_open_ref_cnt_s;
static struct mddp_dev_rb_head_t mddp_dev_rb_head_s;

#define MDDP_CMCMD_RSP_CNT (MDDP_CMCMD_RSP_END - MDDP_CMCMD_RSP_BEGIN)
static enum mddp_dev_evt_type_e
mddp_dev_rsp_status_mapping_s[MDDP_CMCMD_RSP_CNT][2] =  {
/* FAIL                             SUCCESS */
{MDDP_DEV_EVT_STOPPED_ERROR,        MDDP_DEV_EVT_SUPPORT_AVAILABLE},//ENABLE
{MDDP_DEV_EVT_NONE,                 MDDP_DEV_EVT_NONE},//DISABLE
{MDDP_DEV_EVT_STOPPED_ERROR,        MDDP_DEV_EVT_STARTED},//ACT
{MDDP_DEV_EVT_STOPPED_UNSUPPORTED,  MDDP_DEV_EVT_STOPPED_UNSUPPORTED},//DEACT
{MDDP_DEV_EVT_STOPPED_LIMIT_REACHED, MDDP_DEV_EVT_STOPPED_LIMIT_REACHED},//LIMIT
{MDDP_DEV_EVT_CONNECT_UPDATE,       MDDP_DEV_EVT_CONNECT_UPDATE},//CT_IND
};
#undef MDDP_CMCMD_RSP_CNT

static uint32_t mddp_dev_major_s;
static struct cdev mddp_cdev_s;
struct class *mddp_dev_class_s;


//------------------------------------------------------------------------------
// Sysfs APIs
//------------------------------------------------------------------------------
static ssize_t
version_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "MDDP version(%d)\n", __MDDP_VERSION__);
}
static DEVICE_ATTR_RO(version);

static ssize_t
state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mddp_app_t      *app;
	uint32_t                type;
	uint8_t                 idx;
	uint32_t                ret_num = 0;

	for (idx = 0; idx < MDDP_MOD_CNT; idx++) {
		type = mddp_sm_module_list_s[idx];
		app = mddp_get_app_inst(type);
		ret_num += sprintf(buf + ret_num, "type(%d), state(%d)\n",
				   app->type, app->state);

		// NG. Failed to fill-in data!
		if (ret_num <= 0)
			return sprintf(buf,
				"%s: Failed to fill-in data!\n", __func__);
	}

	// OK.
	return ret_num;
}
static DEVICE_ATTR_RO(state);

static ssize_t
wh_statistic_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mddp_app_t      *app;

	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);

	if (app->sysfs_callback)
		return app->sysfs_callback(app,
				MDDP_SYSFS_CMD_STATISTIC_READ, buf, 0);

	return sprintf(buf, "Cannot change WH mode, mddp-wh config(%d)\n",
					app->is_config);
}
static DEVICE_ATTR_RO(wh_statistic);

static ssize_t
wh_enable_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct mddp_app_t      *app;

	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);

	if (app->sysfs_callback)
		return app->sysfs_callback(app,
				MDDP_SYSFS_CMD_ENABLE_READ, buf, 0);

	return sprintf(buf, "Cannot change WH mode, mddp-wh config(%d)\n",
		       app->is_config);
}

static ssize_t
wh_enable_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf,
		size_t count)
{
	struct mddp_app_t      *app;

	app = mddp_get_app_inst(MDDP_APP_TYPE_WH);

	if (app->sysfs_callback) {
		// OK.
		app->sysfs_callback(app, MDDP_SYSFS_CMD_ENABLE_WRITE,
				    (char *)buf, count);
		return count;
	}

	// NG. Failed to configure!
	return count;
}
static DEVICE_ATTR_RW(wh_enable);

static struct attribute *mddp_attrs[] = {
	&dev_attr_version.attr,
	&dev_attr_state.attr,
	&dev_attr_wh_statistic.attr,

	&dev_attr_wh_enable.attr,
	NULL,
};
ATTRIBUTE_GROUPS(mddp);

//------------------------------------------------------------------------------
// Private functions.
//------------------------------------------------------------------------------
static inline void __mddp_dev_insert(struct mddp_dev_rb_t *new,
		struct mddp_dev_rb_t *prev,
		struct mddp_dev_rb_t *next,
		struct mddp_dev_rb_head_t *list)
{
	new->next = next;
	new->prev = prev;
	next->prev = prev->next = new;
	list->cnt++;
}

static inline void __mddp_dev_rb_unlink(struct mddp_dev_rb_t *entry,
		struct mddp_dev_rb_head_t *list)
{
	struct mddp_dev_rb_t           *next;
	struct mddp_dev_rb_t           *prev;

	list->cnt--;
	next = entry->next;
	prev = entry->prev;
	entry->next = entry->prev = NULL;
	next->prev = prev;
	prev->next = next;
}

static void mddp_dev_rb_queue_tail(struct mddp_dev_rb_head_t *list,
		struct mddp_dev_rb_t *new)
{
	MDDP_DEV_RB_LOCK(list->locker);

	__mddp_dev_insert(new, list->prev, (struct mddp_dev_rb_t *) list, list);

	MDDP_DEV_RB_UNLOCK(list->locker);
}

static struct mddp_dev_rb_t *mddp_dev_rb_peek(
		struct mddp_dev_rb_head_t *list)
{
	struct mddp_dev_rb_t *entry;

	entry = list->next;

	if (entry == (struct mddp_dev_rb_t *)list)
		entry = NULL;

	return entry;
}

static struct mddp_dev_rb_t *mddp_dev_rb_dequeue(
		struct mddp_dev_rb_head_t *list)
{
	struct mddp_dev_rb_t     *entry;

	MDDP_DEV_RB_LOCK(list->locker);

	entry = mddp_dev_rb_peek(list);
	if (entry)
		__mddp_dev_rb_unlink(entry, list);

	MDDP_DEV_RB_UNLOCK(list->locker);

	return entry;
}

static bool mddp_dev_rb_queue_empty(struct mddp_dev_rb_head_t *list)
{
	return list->next == (struct mddp_dev_rb_t *)list;
}

static char *__mddp_dev_devnode(struct device *dev, umode_t *mode)
{
	if (!mode)
		return NULL;

	pr_info("%s: Set permission of dev node(%s).\n",
			__func__, MDDP_DEV_NAME);
	*mode =	0666;

	return NULL;
}

void _mddp_dev_create_dev_node(void)
{
	dev_t                   dev;
	int32_t                 alloc_err = 0;
	int32_t                 cd_err = 0;

	mddp_dev_class_s = class_create(THIS_MODULE, MDDP_CLASS_NAME);
	if (IS_ERR(mddp_dev_class_s))
		goto create_class_error;

	mddp_dev_class_s->devnode = __mddp_dev_devnode;
	mddp_dev_class_s->dev_groups = mddp_groups;

	alloc_err = alloc_chrdev_region(&dev,
			MDDP_DEV_MINOR_BASE,
			MDDP_DEV_MINOR_CNT,
			MDDP_DEV_NAME);
	if (alloc_err)
		goto alloc_cd_error;

	mddp_dev_major_s = MAJOR(dev);

	cdev_init(&mddp_cdev_s, &mddp_dev_fops);
	mddp_cdev_s.owner = THIS_MODULE;
	cd_err = cdev_add(&mddp_cdev_s, dev, 1);
	if (cd_err < 0)
		goto cdev_add_error;

	device_create(mddp_dev_class_s,
			NULL,
			MKDEV(mddp_dev_major_s, 0),
			NULL,
			MDDP_DEV_NAME);

	return;

cdev_add_error:
	cdev_del(&mddp_cdev_s);
	unregister_chrdev_region(dev, MDDP_DEV_MINOR_CNT);
alloc_cd_error:
create_class_error:
	pr_notice("%s: Failed to create node, alloc_err(%d), cd_err(%d)!\n",
			__func__, alloc_err, cd_err);
}

void _mddp_dev_release_dev_node(void)
{
	dev_t                   dev;

	dev = MKDEV(mddp_dev_major_s, 0);

	device_destroy(mddp_dev_class_s, dev);
	class_destroy(mddp_dev_class_s);
	cdev_del(&mddp_cdev_s);
	unregister_chrdev_region(dev, MDDP_DEV_MINOR_CNT);
}

//------------------------------------------------------------------------------
// Public functions.
//------------------------------------------------------------------------------
int32_t mddp_dev_init(void)
{
	struct mddp_dev_rb_head_t      *list;

	atomic_set(&mddp_dev_open_ref_cnt_s, 0);

	list = &mddp_dev_rb_head_s;
	MDDP_DEV_RB_LOCK_INIT(list->locker);
	list->cnt = 0;
	list->prev = list->next = (struct mddp_dev_rb_t *)list;

	init_waitqueue_head(&list->read_wq);

	/*
	 * Create device node.
	 */
	_mddp_dev_create_dev_node();

	return 0;
}

void mddp_dev_uninit(void)
{
	/*
	 * Release CHAR device node.
	 */
	_mddp_dev_release_dev_node();
}

void mddp_dev_response(enum mddp_app_type_e type,
		enum mddp_ctrl_msg_e msg, bool is_success,
		uint8_t *data, uint32_t data_len)
{
	struct mddp_dev_rb_head_t      *list = &mddp_dev_rb_head_s;
	struct mddp_dev_rb_t           *entry;
	struct mddp_dev_rsp_common_t   *dev_rsp;
	uint16_t                        status;
	uint32_t                        rsp_idx;

	if (msg < MDDP_CMCMD_RSP_BEGIN || msg >= MDDP_CMCMD_RSP_END) {
		pr_notice("%s: invalid rsp msg(%d) in type(%d)!\n",
				__func__, msg, type);
		return;
	}

	rsp_idx = (msg - MDDP_CMCMD_RSP_BEGIN);
	status = mddp_dev_rsp_status_mapping_s[rsp_idx][is_success];

	if (status == MDDP_DEV_EVT_NONE) {
		// No response to upper module.
		pr_notice("%s: No RSP, type(%d), msg(%d), is_success(%d).\n",
				__func__, type, msg, is_success);
		return;
	}

	dev_rsp = kmalloc(sizeof(struct mddp_dev_rsp_common_t) + data_len,
				GFP_KERNEL);
	if (unlikely(!dev_rsp)) {
		return;
	}

	dev_rsp->mcode = MDDP_CTRL_MSG_MCODE;
	dev_rsp->status = status;
	dev_rsp->app_type = type;
	dev_rsp->msg = msg;
	dev_rsp->data_len = data_len;
	if (data_len > 0)
		memcpy(dev_rsp->data, data, data_len);

	entry = kmalloc(sizeof(struct mddp_dev_rb_t), GFP_KERNEL);
	if (unlikely(!entry)) {
		kfree(dev_rsp);
		return;
	}

	entry->len = sizeof(dev_rsp) + data_len;
	entry->dev_rsp = dev_rsp;
	mddp_dev_rb_queue_tail(list, entry);
}

//------------------------------------------------------------------------------
// Device node functins.
//------------------------------------------------------------------------------
int32_t mddp_dev_open(struct inode *inode, struct file *file)
{
	pr_info("%s: IOCTL dev open.\n", __func__);

	if (atomic_read(&mddp_dev_open_ref_cnt_s))
		return -EBUSY;

	atomic_inc(&mddp_dev_open_ref_cnt_s);

	return 0;
}

int32_t mddp_dev_close(struct inode *inode, struct file *file)
{
	pr_info("%s: IOCTL dev close.\n", __func__);

	atomic_dec(&mddp_dev_open_ref_cnt_s);

	return 0;
}

ssize_t mddp_dev_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
	int32_t                         ret = 0;
	uint32_t                        len = 0;
	struct mddp_dev_rb_head_t      *list = &mddp_dev_rb_head_s;
	struct mddp_dev_rb_t           *entry;

	/*
	 * READ: MDDP send data to upper module.
	 */
	if (mddp_dev_rb_queue_empty(list)) {
		if (!(file->f_flags & O_NONBLOCK)) {
			MDDP_DEV_RB_LOCK_IRQ(list->read_wq.lock);
			ret = wait_event_interruptible_locked_irq(
				list->read_wq,
				!mddp_dev_rb_queue_empty(list));
			MDDP_DEV_RB_UNLOCK_IRQ(list->read_wq.lock);

			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				goto exit;
			}
		} else {
			ret = -EAGAIN;
			goto exit;
		}
	}

	pr_info("%s: IOCTL dev read, count(%zu).\n", __func__, count);
	entry = mddp_dev_rb_peek(list);

	if (!entry) {
		len = 0;
		goto exit;
	}
	len = entry->len;

	if (count >= entry->len) {
		if (copy_to_user(buf, entry->dev_rsp, entry->len)) {
			pr_notice("%s: copy_to_user fail!\n", __func__);
			ret = -EFAULT;
		}

		entry = mddp_dev_rb_dequeue(list);
		if (entry == NULL) {
			pr_notice("%s: unexpected dequeue fail!\n", __func__);
			ret = -EFAULT;
			goto exit;
		}
		kfree(entry->dev_rsp);
		kfree(entry);
	} else {
		ret = -ENOBUFS;
		goto exit;
	}

exit:
	return ret ? ret : len;
}

ssize_t mddp_dev_write(struct file *file,
		const char __user *buf,
		size_t count,
		loff_t *ppos)
{
	/*
	 * Not support WRITE.
	 */
	pr_notice("%s: Receive\n", __func__);

	return 0;
}

long mddp_dev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct mddp_dev_req_common_t    dev_req;
	struct mddp_dev_rsp_common_t   *dev_rsp;
	long                            ret = 0;
	uint32_t                        data_len;
	uint8_t                         buf[MAX_GET_BUF_SZ];
	uint32_t                        buf_len = MAX_GET_BUF_SZ;

	/*
	 * NG. copy_from_user fail!
	 */
	if (copy_from_user(&dev_req, (void __user *)arg,
			sizeof(struct mddp_dev_req_common_t))) {
		pr_notice("%s: copy_from_user failed!\n", __func__);
		ret = -EFAULT;
		goto ioctl_error;
	}

	/*
	 * NG. MCODE check fail!
	 */
	if (dev_req.mcode != MDDP_CTRL_MSG_MCODE) {
		pr_notice("%s: MCODE(%d) wrong!\n", __func__, dev_req.mcode);
		ret = -EINVAL;
		goto ioctl_error;
	}

	/*
	 * OK. IOCTL command dispatch.
	 */
	switch (dev_req.msg) {
	case MDDP_CMCMD_ENABLE_REQ:
		ret = mddp_on_enable(dev_req.app_type);
		break;

	case MDDP_CMCMD_DISABLE_REQ:
		ret = mddp_on_disable(dev_req.app_type);
		break;

	case MDDP_CMCMD_ACT_REQ:
		data_len = dev_req.data_len;
		if (data_len == sizeof(struct mddp_dev_req_act_t)) {
			ret = copy_from_user(&buf,
				((struct mddp_dev_req_common_t *)arg)->data,
				data_len);

			if (!ret) {
				/* OK */
				ret = mddp_on_activate(dev_req.app_type,
				((struct mddp_dev_req_act_t *)buf)->ul_dev_name,
				((struct mddp_dev_req_act_t *)buf)->dl_dev_name
				);
				break;
			}
		}
		/* NG */
		pr_notice("%s: ACT fail, data_len(%d), ret(%ld)!\n",
					__func__, data_len, ret);

		break;

	case MDDP_CMCMD_DEACT_REQ:
		ret = mddp_on_deactivate(dev_req.app_type);
		break;

	case MDDP_CMCMD_GET_OFFLOAD_STATS_REQ:
		ret = mddp_on_get_offload_stats(dev_req.app_type,
				buf, &buf_len);
		pr_info("%s: ret(%ld), type(%d), buf(%p), len(%d)\n",
			__func__, ret, dev_req.app_type, buf, buf_len);
		pr_info("%s: get_offload_stats, rx(%llu), tx(%llu).\n",
			__func__,
			((struct mddp_u_data_stats_t *)buf)->total_rx_bytes,
			((struct mddp_u_data_stats_t *)buf)->total_tx_bytes);

		if (!ret) {
			dev_rsp = kmalloc(
				sizeof(struct mddp_dev_rsp_common_t) + buf_len,
				GFP_KERNEL);

			if (dev_rsp == NULL) {
				ret = -ENOMEM;
				goto ioctl_error;
			}

			MDDP_DEV_CLONE_COMM_HDR(dev_rsp, &dev_req, 0);
			dev_rsp->data_len = buf_len;
			memcpy(dev_rsp->data, &buf, buf_len);
			ret = (copy_to_user((void *)arg, dev_rsp,
				sizeof(struct mddp_dev_rsp_common_t) +
				buf_len))
				? -EFAULT : 0;
			kfree(dev_rsp);
		}
		break;

	case MDDP_CMCMD_SET_DATA_LIMIT_REQ:
		buf_len = sizeof(struct mddp_dev_req_set_data_limit_t);
		ret = copy_from_user(&buf,
				((struct mddp_dev_req_common_t *)arg)->data,
				buf_len);
		if (!ret)
			ret = mddp_on_set_data_limit(dev_req.app_type,
					buf, buf_len);
		break;

	default:
		pr_notice("%s: Invalid command(%d)!\n",
				__func__, dev_req.msg);
		ret = -EINVAL;
		break;
	}

ioctl_error:
	pr_notice("%s: cmd(%d) app_type(%d), ret (%ld).\n",
				__func__, dev_req.msg, dev_req.app_type, ret);
	return ret;
}

#ifdef CONFIG_COMPAT
long mddp_dev_compat_ioctl(struct file *filp,
		unsigned int cmd,
		unsigned long arg)
{
	return 0;
}
#endif

unsigned int mddp_dev_poll(struct file *fp, struct poll_table_struct *poll)
{
	return 0;
}
