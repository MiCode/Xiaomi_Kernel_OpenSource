/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#define pr_fmt(fmt) "mi_disp_feature:[%s:%d] " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/crypto.h>
#include <linux/spinlock.h>
#include <linux/sched/clock.h>
#include <linux/types.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/wait.h>
#include <linux/freezer.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>

#include <drm/mi_disp.h>
#include "mi_disp_print.h"
#include "mi_disp_core.h"
#include "mi_disp_feature.h"
#include "mi_disp_file.h"
#include "mi_disp_sysfs.h"
#include "mi_disp_lhbm.h"

#include "mi_disp_procfs.h"
#include "mi_disp_debugfs.h"
#include "mi_disp_log.h"

struct disp_feature *g_disp_feature = NULL;

struct disp_feature *mi_get_disp_feature(void)
{
	int ret = 0;

	if (!g_disp_feature) {
		ret = mi_disp_feature_init();
		if (ret < 0)
			return NULL;
	}

	return g_disp_feature;
}

static int mi_disp_feature_thread_create(struct disp_feature *df, int disp_id)
{
	int ret = 0;
	struct disp_display *dd_ptr = NULL;

	if (!df || !is_support_disp_id(disp_id)) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	dd_ptr = &df->d_display[disp_id];

	dd_ptr->worker = kthread_create_worker(0, "disp_feature:%d", disp_id);
	if (IS_ERR(dd_ptr->worker)) {
		DISP_ERROR("failed to create disp_feature:%d kthread\n", disp_id);
		return PTR_ERR(dd_ptr->worker);

	}
	/* set realtime priority */
	sched_set_fifo(dd_ptr->worker->task);

	init_waitqueue_head(&dd_ptr->pending_wq);
	atomic_set(&dd_ptr->pending_doze_cnt, 0);

	DISP_INFO("create disp_feature:%d kthread success\n", disp_id);

	return ret;
}


static int mi_disp_feature_thread_destroy(struct disp_feature *df, int disp_id)
{
	int ret = 0;
	struct disp_display *dd_ptr = NULL;

	if (!df || !is_support_disp_id(disp_id)) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	dd_ptr = &df->d_display[disp_id];
	if (dd_ptr->worker) {
		kthread_destroy_worker(dd_ptr->worker);
	}

	DISP_INFO("destroy disp_feature:%d kthread success\n", disp_id);

	return ret;
}


int mi_disp_feature_attach_display(void *display, int disp_id, int intf_type)
{
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr = NULL;
	int ret = 0;

	if (!df) {
		return -ENODEV;
	}

	if (!display) {
		DISP_ERROR("invalid display param\n");
		return -EINVAL;
	}

	if (!is_support_disp_id(disp_id) || !is_support_disp_intf_type(intf_type)) {
		DISP_ERROR("unknown disp_id(%d) or intf_type(%d) params\n",
			disp_id, intf_type);
		return -EINVAL;
	}

	dd_ptr = &df->d_display[disp_id];

	dd_ptr->display = display;
	dd_ptr->disp_id = disp_id;
	dd_ptr->intf_type = intf_type;

	dd_ptr->dev = device_create(df->class, df->pdev, 0, dd_ptr,
			"disp-%s-%d", get_disp_intf_type_name(intf_type), disp_id);
	if (IS_ERR(dd_ptr->dev)) {
		DISP_ERROR("create device failed for disp-%s-%d\n",
			get_disp_intf_type_name(intf_type), disp_id);
		ret = -ENODEV;
		goto err_exit;
	}

	ret = mi_disp_create_device_attributes(dd_ptr->dev);
	if (ret < 0) {
		DISP_ERROR("failed to create sysfs attrs\n");
		goto err_dev;
	}

	ret = mi_disp_feature_thread_create(df, disp_id);
	if (ret) {
		DISP_ERROR("failed to create disp_feature kthread\n");
		goto err_sysfs;
	}

	ret = mi_disp_lhbm_fod_thread_create(df, disp_id);
	if (ret) {
		DISP_ERROR("failed to create fod_thread kthread\n");
		goto err_feature_thread;
	}

	ret = mi_disp_procfs_init(dd_ptr, disp_id);
	if (ret) {
		DISP_ERROR("failed to create procfs entry\n");
		goto err_fod_thread;
	}

	ret = mi_disp_debugfs_init(dd_ptr, disp_id);
	if (ret) {
		DISP_ERROR("failed to create debugfs entry\n");
		goto err_procfs;
	}

	DISP_INFO("attach %s display(%s intf) success\n", get_disp_id_name(disp_id),
			get_disp_intf_type_name(intf_type));

	return 0;

err_procfs:
	mi_disp_procfs_deinit(dd_ptr, disp_id);
err_fod_thread:
	mi_disp_lhbm_fod_thread_destroy(df, disp_id);
err_feature_thread:
	mi_disp_feature_thread_destroy(df, disp_id);
err_sysfs:
	mi_disp_remove_device_attributes(dd_ptr->dev);
err_dev:
	device_unregister(dd_ptr->dev);
	dd_ptr->dev = NULL;
err_exit:
	return ret;
}

int mi_disp_feature_detach_display(void *display, int disp_id, int intf_type)
{
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_display *dd_ptr = NULL;
	int ret = 0;

	if (!df) {
		return -ENODEV;
	}

	if (!display) {
		DISP_ERROR("invalid display param\n");
		return -EINVAL;
	}

	if (!is_support_disp_id(disp_id) || !is_support_disp_intf_type(intf_type)) {
		DISP_ERROR("unknown disp_id(%d) or intf_type(%d) params\n",
			disp_id, intf_type);
		return -EINVAL;
	}

	dd_ptr = &df->d_display[disp_id];

	if (dd_ptr->display != display) {
		DISP_ERROR("please check display param\n");
		return -EINVAL;
	}

	ret = mi_disp_debugfs_deinit(dd_ptr, disp_id);
	if (ret < 0)
		DISP_ERROR("failed to remove debugfs entry\n");

	ret = mi_disp_procfs_deinit(dd_ptr, disp_id);
	if (ret < 0)
		DISP_ERROR("failed to remove procfs entry\n");

	ret = mi_disp_lhbm_fod_thread_destroy(df, disp_id);
	if (ret)
		DISP_ERROR("failed to destroy disp_fod kthread\n");

	ret = mi_disp_feature_thread_destroy(df, disp_id);
	if (ret)
		DISP_ERROR("failed to destroy disp_feature kthread\n");

	mi_disp_remove_device_attributes(dd_ptr->dev);

	device_unregister(dd_ptr->dev);

	dd_ptr->dev = NULL;
	dd_ptr->display = NULL;
	dd_ptr->intf_type = MI_INTF_MAX;

	DISP_INFO("detach %s display(%s intf) success\n", get_disp_id_name(disp_id),
			get_disp_intf_type_name(intf_type));

	return ret;
}

void mi_disp_feature_event_notify(struct disp_event *event, u8 *payload)
{
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_feature_client *client;
	unsigned long flags;
	struct disp_pending_event *notify;
	int len = 0;

	if (!df || !event || !event->length || !payload) {
		DISP_ERROR("dev %pK, event %pK disp_id %d len %d payload %pK\n", df, event,
		((event) ? (event->disp_id) : -1),  ((event) ? (event->length) : -1), payload);
		return;
	}

	if (!is_support_disp_id(event->disp_id) ||
		!is_support_disp_event_type(event->type)) {
		DISP_ERROR("invalid display id(%d) or event type(%d)!\n",
			event->disp_id, event->type);
		return;
	}

	spin_lock_irqsave(&df->client_spinlock, flags);

	list_for_each_entry(client, &df->client_list, link) {
		if(!test_bit(event->type, client->disp[event->disp_id].evbit))
			continue;
		len = sizeof(struct disp_pending_event) + event->length;
		if (client->event_space < len) {
			DISP_WARN("Insufficient space %d for event %x len %d\n",
				client->event_space, event->type, len);
			continue;
		}
		notify = kzalloc(len, GFP_ATOMIC);
		if (!notify)
			continue;

		notify->event.base.disp_id = event->disp_id;
		notify->event.base.type = event->type;
		notify->event.base.length = sizeof(struct disp_event_resp) + event->length;
		memcpy(notify->event.data, payload, event->length);
		client->event_space -= notify->event.base.length;
		list_add_tail(&notify->link, &client->event_list);

		DISP_DEBUG("%s display event type: %s\n", get_disp_id_name(event->disp_id),
			get_disp_event_type_name(event->type));
		DISP_DEBUG("%s display event length: %d\n", get_disp_id_name(event->disp_id),
			notify->event.base.length);

		wake_up_interruptible(&client->event_wait);
	}

	spin_unlock_irqrestore(&df->client_spinlock, flags);
}

int mi_disp_feature_event_notify_by_type(int disp_id, u32 type, u32 len, u64 val)
{
	struct disp_event event;

	if (!is_support_disp_id(disp_id) ||
		!is_support_disp_event_type(type)) {
		DISP_ERROR("invalid display id(%d) or event type(%d)!\n",
			event.disp_id, event.type);
		return -EINVAL;
	}

	event.disp_id = disp_id;
	event.type = type;
	event.length = len;
	mi_disp_feature_event_notify(&event, (u8 *)&val);

	return 0;
}

void mi_disp_feature_sysfs_notify(int disp_id, int sysfs_node)
{
	struct disp_feature *df = mi_get_disp_feature();

	if (!df)
		return;

	if (is_support_disp_id(disp_id) && is_support_disp_sysfs_node(sysfs_node)) {
		sysfs_notify(&df->d_display[disp_id].dev->kobj, NULL,
				get_disp_sysfs_node_name(sysfs_node));
	} else {
		DISP_ERROR("invalid display id(%d) or sysfs_node(%d)!\n",
			disp_id, sysfs_node);
	}
}

static const struct file_operations disp_feature_fops = {
	.owner           = THIS_MODULE,
	.open            = mi_disp_open,
	.release         = mi_disp_release,
	.unlocked_ioctl  = mi_disp_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl    = mi_disp_ioctl_compat,
#endif
	.poll            = mi_disp_poll,
	.read            = mi_disp_read,
	.llseek          = no_llseek,
};

int mi_disp_feature_init(void)
{
	int ret = 0;
	struct disp_feature *df = NULL;
	struct disp_core *disp_core = NULL;
	int i;

	ret = mi_disp_core_init();
	if (ret < 0)
		return -ENODEV;

	mi_disp_log_init();

	disp_core = mi_get_disp_core();
	if (!disp_core)
		return -ENODEV;

	if (g_disp_feature) {
		DISP_INFO("mi disp_feature already initialized, return!\n");
		return 0;
	}

	df = kzalloc(sizeof(struct disp_feature), GFP_KERNEL);
	if (!df) {
		DISP_ERROR("can not allocate Buffer\n");
		ret = -ENOMEM;
		goto err_core_deinit;
	}

	ret = mi_disp_cdev_register(DISP_FEATURE_DEVICE_NAME,
				&disp_feature_fops, &df->cdev);
	if (ret < 0) {
		DISP_ERROR("cdev register failed for %s\n", DISP_FEATURE_DEVICE_NAME);
		goto err_alloc_mem;
	}

	df->dev_id = df->cdev->dev;
	df->class = disp_core->class;
	df->pdev = device_create(df->class, NULL, df->dev_id, df, DISP_FEATURE_DEVICE_NAME);
	if (IS_ERR(df->pdev)) {
		DISP_ERROR("create device failed for %s\n", DISP_FEATURE_DEVICE_NAME);
		ret = -ENODEV;
		goto err_cdev_register;
	}

	df->version = MI_DISP_FEATURE_VERSION;
	for (i = MI_DISP_PRIMARY; i < MI_DISP_MAX; i++) {
		df->d_display[i].dev = NULL;
		df->d_display[i].display = NULL;
		df->d_display[i].disp_id = MI_DISP_MAX;
		df->d_display[i].intf_type = MI_INTF_MAX;
		mutex_init(&df->d_display[i].mutex_lock);
	}
	INIT_LIST_HEAD(&df->client_list);
	spin_lock_init(&df->client_spinlock);

	g_disp_feature = df;

	DISP_INFO("mi disp_feature driver initialized!\n");

	return 0;

err_cdev_register:
	mi_disp_cdev_unregister(df->cdev);
err_alloc_mem:
	kfree(df);
err_core_deinit:
	mi_disp_core_deinit();
	return ret;
}

void mi_disp_feature_deinit(void)
{
	if (!g_disp_feature)
		return;
	device_destroy(g_disp_feature->class, g_disp_feature->dev_id);
	mi_disp_cdev_unregister(g_disp_feature->cdev);
	kfree(g_disp_feature);
	g_disp_feature = NULL;
	mi_disp_log_deinit();
	mi_disp_core_deinit();
}

