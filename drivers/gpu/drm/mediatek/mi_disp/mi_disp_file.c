// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2020 XiaoMi, Inc.
 */
#define pr_fmt(fmt) "mi_disp_file:[%s:%d] " fmt, __func__, __LINE__

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/poll.h>
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
#include <linux/rtc.h>
#include <linux/kthread.h>
#include <linux/eventpoll.h>
#include "mi_dsi_display.h"

#include "mi_disp_feature.h"
#include "mi_dsi_panel.h"
#include "mi_disp_file.h"
#include "mi_disp_print.h"

int mi_disp_open(struct inode *inode, struct file *file)
{
	struct disp_feature_client *client;
	struct disp_feature *df = mi_get_disp_feature();

	client = kzalloc(sizeof(*client), GFP_KERNEL);
	if (!client)
		return -ENOMEM;

	client->df = df;
	INIT_LIST_HEAD(&client->link);
	init_waitqueue_head(&client->event_wait);
	memset(client->disp, 0, sizeof(client->disp));

	spin_lock(&df->client_spinlock);
	list_add_tail(&client->link, &df->client_list);
	spin_unlock(&df->client_spinlock);

	INIT_LIST_HEAD(&client->event_list);
	mutex_init(&client->event_lock);
	mutex_init(&client->client_lock);
	/* set aside 4k for event buffer */
	client->event_space = 4096;
	file->private_data = client;

	return 0;
}

int mi_disp_release(struct inode *inode, struct file *filp)
{
	struct disp_feature_client *client = filp->private_data;
	struct disp_feature *df = client->df;
	struct disp_pending_event *e, *et;
	unsigned long flags;

	spin_lock_irqsave(&df->client_spinlock, flags);

	list_del(&client->link);

	/* Remove pending events */
	list_for_each_entry_safe(e, et, &client->event_list, link) {
		list_del(&e->link);
		kfree(e);
	}

	spin_unlock_irqrestore(&df->client_spinlock, flags);

	kfree(client);

	return 0;
}

unsigned int mi_disp_poll(struct file *filp, struct poll_table_struct *wait)
{
	struct disp_feature_client *client = filp->private_data;
	unsigned int mask = 0;

	poll_wait(filp, &client->event_wait, wait);

	if (!list_empty(&client->event_list))
		mask |= EPOLLIN | EPOLLRDNORM;

	return mask;
}

ssize_t mi_disp_read(struct file *filp, char __user *buffer,
			size_t count, loff_t *offset)
{
	struct disp_feature_client *client = filp->private_data;
	struct disp_feature *df = client->df;
	int ret;

	ret = mutex_lock_interruptible(&client->event_lock);
	if (ret)
		return ret;

	for (;;) {
		struct disp_pending_event *e = NULL;

		spin_lock_irq(&df->client_spinlock);
		if (!list_empty(&client->event_list)) {
			e = list_first_entry(&client->event_list,
					struct disp_pending_event, link);
			client->event_space += e->event.base.length;
			list_del(&e->link);
		}
		spin_unlock_irq(&df->client_spinlock);

		if (e == NULL) {
			if (ret)
				break;

			if (filp->f_flags & O_NONBLOCK) {
				ret = -EAGAIN;
				break;
			}

			mutex_unlock(&client->event_lock);
			ret = wait_event_interruptible(client->event_wait,
						       !list_empty(&client->event_list));
			if (ret >= 0)
				ret = mutex_lock_interruptible(&client->event_lock);
			if (ret)
				return ret;
		} else {
			unsigned length = e->event.base.length;

			if (length > count - ret) {
put_back_event:
				spin_lock_irq(&df->client_spinlock);
				client->event_space -= length;
				list_add(&e->link, &client->event_list);
				spin_unlock_irq(&df->client_spinlock);
				wake_up_interruptible(&client->event_wait);
				break;
			}

			pr_debug("event type: %s\n", get_disp_event_type_name(e->event.base.type));
			pr_debug("event length: %d\n", length);

			if (copy_to_user(buffer + ret, &e->event, length)) {
				if (ret == 0)
					ret = -EFAULT;
				goto put_back_event;
			}

			ret += length;
			kfree(e);
		}
	}
	mutex_unlock(&client->event_lock);

	return ret;
}

int mi_disp_ioctl_get_version(struct disp_feature_client *client, void *data)
{
	struct disp_feature *df = client->df;
	struct disp_version *version = data;

	version->version = df->version;

	return 0;
}

static void mi_disp_set_feature_work_handler(struct kthread_work *work)
{
	struct disp_work *cur_work = container_of(work,
					struct disp_work, work);
	struct disp_display *dd_ptr = cur_work->dd_ptr;
	struct disp_feature_ctl *ctl = (struct disp_feature_ctl *)cur_work->data;

	mi_dsi_display_set_disp_param(dd_ptr->display, ctl);

	kfree(cur_work);
}

static int mi_disp_set_feature_queue_work(struct disp_display *dd_ptr,
			void *data)
{
	struct disp_work *cur_work;

	cur_work = kzalloc(sizeof(*cur_work), GFP_ATOMIC);
	if (!cur_work)
		return -ENOMEM;

	kthread_init_work(&cur_work->work, mi_disp_set_feature_work_handler);
	cur_work->dd_ptr = dd_ptr;
	cur_work->wq = &dd_ptr->pending_wq;
	cur_work->data = data;

	kthread_queue_work(&dd_ptr->d_thread.worker, &cur_work->work);

	return 0;
}

static int mi_disp_ioctl_set_feature(struct disp_feature_client *client, void *data)
{
	struct disp_feature *df = client->df;
	struct disp_feature_req *req = data;
	u32 disp_id = req->base.disp_id;
	struct disp_display *dd_ptr = NULL;
	static struct disp_feature_ctl ctl;
	int ret = 0;

	ret = mutex_lock_interruptible(&client->client_lock);
	if (ret)
		return ret;

	memset(&ctl, 0, sizeof(struct disp_feature_ctl));

	if (is_support_disp_id(disp_id)) {
		ctl.feature_id = req->feature_id;
		ctl.feature_val = req->feature_val;
		if (req->tx_len) {
			ctl.tx_len = req->tx_len;
			ctl.tx_ptr = kzalloc(ctl.tx_len, GFP_KERNEL);
			if (!ctl.tx_ptr) {
				ret = -ENOMEM;
				goto exit;
			}
			if (copy_from_user(ctl.tx_ptr, (void __user *)req->tx_ptr, ctl.tx_len) != 0) {
				ret = -EFAULT;
				goto err_free_tx;
			}
		}
		if (req->rx_len) {
			ctl.rx_len = req->rx_len;
			ctl.rx_ptr = kzalloc(ctl.rx_len, GFP_KERNEL);
			if (!ctl.rx_ptr) {
				ret = -ENOMEM;
				goto err_free_tx;
			}
		}

		dd_ptr = &df->d_display[disp_id];
		if (dd_ptr->intf_type == MI_INTF_DSI) {
			if (req->base.flag == MI_DISP_FLAG_NONBLOCK) {
				mi_disp_set_feature_queue_work(dd_ptr, &ctl);
			} else {
				ret = mi_dsi_display_set_disp_param(dd_ptr->display, &ctl);
			}
		} else {
			DISP_INFO("Unsupported display(%s intf)\n",
				get_disp_intf_type_name(dd_ptr->intf_type));
			ret = -EINVAL;
			goto err_free_rx;
		}
		if (req->rx_len && !ret) {
			if (copy_to_user((void __user *)req->rx_ptr, ctl.rx_ptr, ctl.rx_len) != 0) {
				ret = -EFAULT;
				goto err_free_rx;
			}
		}
	} else {
		DISP_INFO("Unsupported display id\n");
		ret = -EINVAL;
		goto exit;
	}

err_free_rx:
	if (ctl.rx_len && ctl.rx_ptr)
		kfree(ctl.rx_ptr);
err_free_tx:
	if (ctl.tx_len && ctl.tx_ptr)
		kfree(ctl.tx_ptr);
exit:
	mutex_unlock(&client->client_lock);
	return ret;
}

static void mi_disp_set_doze_brightness_work_handler(struct kthread_work *work)
{
	struct disp_work *cur_work = container_of(work,
					struct disp_work, work);
	struct disp_display *dd_ptr = cur_work->dd_ptr;
	u32 doze_brightness = *((u32 *)cur_work->data);
	struct mtk_dsi *display = (struct mtk_dsi *)dd_ptr->display;
	int ret = 0;

	if (is_support_doze_brightness(doze_brightness)) {
		atomic_inc(&dd_ptr->pending_doze_cnt);
		DISP_INFO("pending_doze_cnt = %d\n", atomic_read(&dd_ptr->pending_doze_cnt));
		if (doze_brightness == DOZE_TO_NORMAL) {
			ret = wait_event_freezable(*(cur_work->wq),
				dsi_panel_initialized(display));
			if (ret) {
				DISP_INFO("wait_event_freezable ret = %d\n", ret);
				/* Some event woke us up, so let's quit */
				goto exit;
			}
			mi_dsi_display_set_doze_brightness(dd_ptr->display, doze_brightness);
		} else {
			ret = wait_event_freezable(*(cur_work->wq),
				dsi_panel_initialized(display));
			if (ret) {
				DISP_INFO("wait_event_freezable ret = %d\n", ret);
				/* Some event woke us up, so let's quit */
				goto exit;
			}
			mi_dsi_display_set_doze_brightness(dd_ptr->display, doze_brightness);
		}
		atomic_add_unless(&dd_ptr->pending_doze_cnt, -1, 0);
	}

exit:
	kfree(cur_work);
}

static int mi_disp_set_doze_brightness_queue_work(struct disp_display *dd_ptr,
			void *data)
{
	struct disp_work *cur_work;

	cur_work = kzalloc(sizeof(*cur_work), GFP_KERNEL);
	if (!cur_work) {
		DISP_ERROR("%s kzalloc error\n",__func__);
		return -ENOMEM;
	}

	kthread_init_work(&cur_work->work, mi_disp_set_doze_brightness_work_handler);
	cur_work->dd_ptr = dd_ptr;
	cur_work->wq = &dd_ptr->pending_wq;
	cur_work->data = data;

	if (!kthread_queue_work(&dd_ptr->d_thread.worker, &cur_work->work)) {
		kfree(cur_work);
		DISP_ERROR("%s queuing_blocked\n",__func__);
	}

	return 0;
}

static int mi_disp_ioctl_set_doze_brightness(
			struct disp_feature_client *client, void *data)
{
	struct disp_feature *df = client->df;
	struct disp_doze_brightness_req *req = data;
	u32 disp_id = req->base.disp_id;
	struct disp_display *dd_ptr = NULL;
	static u32 doze_brightness;
	int ret = 0;

	ret = mutex_lock_interruptible(&client->client_lock);
	if (ret)
		return ret;

	doze_brightness = req->doze_brightness;

	if (is_support_disp_id(disp_id)) {
		dd_ptr = &df->d_display[disp_id];
		if (dd_ptr->intf_type == MI_INTF_DSI) {
			DISP_INFO("%s display doze_brightness = %d\n",
				get_disp_id_name(disp_id), doze_brightness);
			if (req->base.flag == MI_DISP_FLAG_NONBLOCK) {
				mi_disp_set_doze_brightness_queue_work(dd_ptr,
						&doze_brightness);
			} else {
				ret = mi_dsi_display_set_doze_brightness(dd_ptr->display,
						doze_brightness);
			}
		} else {
			DISP_INFO("Unsupported display(%s intf)\n",
				get_disp_intf_type_name(dd_ptr->intf_type));
			ret = -EINVAL;
		}
	} else {
		DISP_INFO("Unsupported display id\n");
		ret = -EINVAL;
	}

	mutex_unlock(&client->client_lock);
	return ret;
}

static int mi_disp_ioctl_get_doze_brightness(
			struct disp_feature_client *client, void *data)
{
	struct disp_feature *df = client->df;
	struct disp_doze_brightness_req *req = data;
	u32 disp_id = req->base.disp_id;
	struct disp_display *dd_ptr = NULL;
	int ret = 0;

	ret = mutex_lock_interruptible(&client->client_lock);
	if (ret)
		return ret;

	if (is_support_disp_id(disp_id)) {
		dd_ptr = &df->d_display[disp_id];
		if (dd_ptr->intf_type == MI_INTF_DSI) {
			ret = mi_dsi_display_get_doze_brightness(dd_ptr->display,
					&req->doze_brightness);
		} else {
			DISP_INFO("Unsupported display(%s intf)\n",
				get_disp_intf_type_name(dd_ptr->intf_type));
			ret = -EINVAL;
		}
	} else {
		DISP_INFO("Unsupported display id\n");
		ret = -EINVAL;
	}

	mutex_unlock(&client->client_lock);
	return ret;
}

static int mi_disp_ioctl_set_brightness(
			struct disp_feature_client *client, void *data)
{
	struct disp_feature *df = client->df;
	struct disp_brightness_req *req = data;
	u32 disp_id = req->base.disp_id;
	struct disp_display *dd_ptr = NULL;
	static u32 brightness;
	int ret = 0;

	ret = mutex_lock_interruptible(&client->client_lock);
	if (ret)
		return ret;

	brightness = req->brightness;

	if (is_support_disp_id(disp_id)) {
		dd_ptr = &df->d_display[disp_id];
		if (dd_ptr->intf_type == MI_INTF_DSI) {
			DISP_INFO("%s display brightness = %d\n",
				get_disp_id_name(disp_id), brightness);
			ret = mi_dsi_display_set_brightness(dd_ptr->display,
					brightness);
		} else {
			DISP_INFO("Unsupported display(%s intf)\n",
				get_disp_intf_type_name(dd_ptr->intf_type));
			ret = -EINVAL;
		}
	} else {
		DISP_INFO("Unsupported display id\n");
		ret = -EINVAL;
	}

	mutex_unlock(&client->client_lock);
	return ret;
}

static int mi_disp_ioctl_get_brightness(
			struct disp_feature_client *client, void *data)
{
	struct disp_feature *df = client->df;
	struct disp_brightness_req *req = data;
	u32 disp_id = req->base.disp_id;
	struct disp_display *dd_ptr = NULL;
	int ret = 0;

	ret = mutex_lock_interruptible(&client->client_lock);
	if (ret)
		return ret;

	if (is_support_disp_id(disp_id)) {
		dd_ptr = &df->d_display[disp_id];
		if (dd_ptr->intf_type == MI_INTF_DSI) {
			ret = mi_dsi_display_get_brightness(dd_ptr->display,
					&req->brightness);
		} else {
			DISP_INFO("Unsupported display(%s intf)\n",
				get_disp_intf_type_name(dd_ptr->intf_type));
			ret = -EINVAL;
		}
	} else {
		DISP_INFO("Unsupported display id\n");
		ret = -EINVAL;
	}

	mutex_unlock(&client->client_lock);
	return ret;
}

static int mi_disp_ioctl_get_panel_info(struct disp_feature_client *client, void *data)
{
	struct disp_feature *df = client->df;
	struct disp_panel_info *req = data;
	u32 disp_id = req->base.disp_id;
	char __user *buf = req->info;
	struct disp_display *dd_ptr = NULL;
	char panel_name[128] = {0};
	int len;
	int ret = 0;

	if (is_support_disp_id(disp_id)) {
		dd_ptr = &df->d_display[disp_id];
		if (dd_ptr->intf_type == MI_INTF_DSI) {
			len = mi_dsi_display_read_panel_info(dd_ptr->display,
					panel_name);
			if (len > req->info_len)
				len = req->info_len;
			req->info_len = strlen(panel_name);
			if (len && buf) {
				if (copy_to_user(buf, panel_name, len)) {
					ret = -EFAULT;
				}
			}
		} else {
			DISP_INFO("Unsupported display(%s intf)\n",
				get_disp_intf_type_name(dd_ptr->intf_type));
			ret = -EINVAL;
		}
	} else {
		DISP_INFO("Unsupported display id\n");
		ret = -EINVAL;
	}

	return ret;
}

static int mi_disp_ioctl_get_wp_info(struct disp_feature_client *client, void *data)
{
	struct disp_feature *df = client->df;
	struct disp_wp_info *req = data;
	u32 disp_id = req->base.disp_id;
	char __user *buf = req->info;
	struct disp_display *dd_ptr = NULL;
	char wp_info[64] = {0};
	int len;
	int ret = 0;

	if (is_support_disp_id(disp_id)) {
		dd_ptr = &df->d_display[disp_id];
		if (dd_ptr->intf_type == MI_INTF_DSI) {
			len = mi_dsi_display_read_wp_info(dd_ptr->display,
					wp_info, sizeof(wp_info));
			if (len > req->info_len)
				len = req->info_len;
			req->info_len = strlen(wp_info);
			if (len && buf) {
				if (copy_to_user(buf, wp_info, len)) {
					ret = -EFAULT;
				}
			}
		} else {
			DISP_INFO("Unsupported display(%s intf)\n",
				get_disp_intf_type_name(dd_ptr->intf_type));
			ret = -EINVAL;
		}
	} else {
		DISP_INFO("Unsupported display id\n");
		ret = -EINVAL;
	}

	return ret;
}

static int mi_disp_ioctl_get_fps(struct disp_feature_client *client, void *data)
{
	struct disp_feature *df = client->df;
	struct disp_fps_info *req = data;
	u32 disp_id = req->base.disp_id;
	struct disp_display *dd_ptr = NULL;
	struct mtk_dsi *display = NULL;
	int ret = 0;

	if (is_support_disp_id(disp_id)) {
		dd_ptr = &df->d_display[disp_id];
		if (dd_ptr->intf_type == MI_INTF_DSI) {
			display = (struct mtk_dsi *)dd_ptr->display;
			mi_dsi_display_get_fps(display, &req->fps);
		} else {
			DISP_INFO("Unsupported display(%s intf)\n",
				get_disp_intf_type_name(dd_ptr->intf_type));
			ret = -EINVAL;
		}
	} else {
		DISP_INFO("Unsupported display id\n");
		ret = -EINVAL;
	}

	return ret;
}

static int mi_disp_ioctl_register_event(struct disp_feature_client *client,
		void *data)
{
	struct disp_event_req *req = data;
	u32 disp_id = req->base.disp_id;
	u32 type = req->type;
	int ret = 0;

	ret = mutex_lock_interruptible(&client->event_lock);
	if (ret)
		return ret;

	if (is_support_disp_id(disp_id)) {
		if (is_support_disp_event_type(type)) {
			if (test_bit(type, client->disp[disp_id].evbit)) {
				DISP_INFO("%s display %s event already register!\n",
					get_disp_id_name(disp_id), get_disp_event_type_name(type));
			} else {
				set_bit(type, client->disp[disp_id].evbit);
				DISP_INFO("%s display %s event register\n",
					get_disp_id_name(disp_id), get_disp_event_type_name(type));
			}
		} else {
			DISP_INFO("invalid event type!\n");
			ret = -EINVAL;
		}
	} else {
		DISP_INFO("Unsupported display id\n");
		ret = -EINVAL;
	}

	mutex_unlock(&client->event_lock);
	return ret;
}

static int mi_disp_ioctl_deregister_event(struct disp_feature_client *client,
		void *data)

{
	struct disp_event_req *req = data;
	u32 disp_id = req->base.disp_id;
	u32 type = req->type;
	int ret = 0;

	ret = mutex_lock_interruptible(&client->event_lock);
	if (ret)
		return ret;

	if (is_support_disp_id(disp_id)) {
		if (is_support_disp_event_type(type)) {
			if (test_bit(type, client->disp[disp_id].evbit)) {
				clear_bit(type, client->disp[disp_id].evbit);
				DISP_INFO("%s display %s event deregister\n",
					get_disp_id_name(disp_id), get_disp_event_type_name(type));
			} else {
				DISP_INFO("%s display %s event already deregister!\n",
					get_disp_id_name(disp_id), get_disp_event_type_name(type));
			}
		} else {
			DISP_INFO("invalid event type!\n");
			ret = -EINVAL;
		}
	} else {
		DISP_INFO("Unsupported display id\n");
		ret = -EINVAL;
	}

	mutex_unlock(&client->event_lock);
	return ret;
}

static int mi_disp_ioctl_write_dsi_cmd(
			struct disp_feature_client *client, void *data)
{
	struct disp_feature *df = client->df;
	struct disp_dsi_cmd_req *req = data;
	u32 disp_id = req->base.disp_id;
	struct disp_display *dd_ptr = NULL;
	static struct dsi_cmd_rw_ctl ctl;
	int ret = 0;

	ret = mutex_lock_interruptible(&client->client_lock);
	if (ret)
		return ret;

	memset(&ctl, 0, sizeof(struct dsi_cmd_rw_ctl));

	if (is_support_disp_id(disp_id)) {
		dd_ptr = &df->d_display[disp_id];
		if (dd_ptr->intf_type == MI_INTF_DSI) {
			ctl.tx_state = req->tx_state;
			if (!req->tx_len) {
				DISP_ERROR("invalid params\n");
				ret = -EINVAL;
				goto exit;
			}

			ctl.tx_len = req->tx_len;
			ctl.tx_ptr = kzalloc(ctl.tx_len, GFP_KERNEL);
			if (!ctl.tx_ptr) {
				ret = -ENOMEM;
				goto exit;
			}
			if (copy_from_user(ctl.tx_ptr, (void __user *)req->tx_ptr, ctl.tx_len) != 0) {
				ret = -EFAULT;
				goto err_free_tx;
			}
			ret = mi_dsi_display_write_dsi_cmd(dd_ptr->display, &ctl);
		} else {
			DISP_INFO("Unsupported display(%s intf)\n",
				get_disp_intf_type_name(dd_ptr->intf_type));
			ret = -EINVAL;
		}
	} else {
		DISP_INFO("Unsupported display id\n");
		ret = -EINVAL;
	}

err_free_tx:
	if (ctl.tx_len && ctl.tx_ptr)
		kfree(ctl.tx_ptr);
exit:
	mutex_unlock(&client->client_lock);
	return ret;
}

static int mi_disp_ioctl_read_dsi_cmd(
			struct disp_feature_client *client, void *data)
{
	struct disp_feature *df = client->df;
	struct disp_dsi_cmd_req *req = data;
	u32 disp_id = req->base.disp_id;
	struct disp_display *dd_ptr = NULL;
	struct dsi_cmd_rw_ctl ctl;
	u32 recv_buf_len;
	u32 recv_len;
	int ret = 0;
	int i = 0;

	ret = mutex_lock_interruptible(&client->client_lock);
	if (ret)
		return ret;

	memset(&ctl, 0, sizeof(struct dsi_cmd_rw_ctl));

	if (is_support_disp_id(disp_id)) {
		dd_ptr = &df->d_display[disp_id];
		if (dd_ptr->intf_type == MI_INTF_DSI) {
			ctl.tx_state = req->tx_state;
			if (!req->tx_len || !req->rx_len) {
				DISP_ERROR("invalid params\n");
				ret = -EINVAL;
				goto exit;
			}

			ctl.tx_len = req->tx_len;
			ctl.tx_ptr = kzalloc(ctl.tx_len, GFP_KERNEL);
			if (!ctl.tx_ptr) {
				ret = -ENOMEM;
				goto exit;
			}
			if (copy_from_user(ctl.tx_ptr, (void __user *)req->tx_ptr, ctl.tx_len) != 0) {
				ret = -EFAULT;
				goto err_free_tx;
			}

			ctl.rx_len = req->rx_len;
			recv_buf_len = ((ctl.rx_len + 3) >> 2) << 2;
			ctl.rx_ptr = kzalloc(recv_buf_len, GFP_KERNEL);
			if (!ctl.rx_ptr) {
				ret = -ENOMEM;
				goto err_free_tx;
			}

			recv_len = mi_dsi_display_read_dsi_cmd(dd_ptr->display, &ctl);
			if (recv_len <= 0 || recv_len != ctl.rx_len) {
				DISP_ERROR("read dsi cmd transfer failed rc = %d\n", ret);
				ret = -EAGAIN;
				goto err_free_rx;
			} else {
				if (copy_to_user((void __user *)req->rx_ptr, ctl.rx_ptr, ctl.rx_len) != 0) {
					ret = -EFAULT;
					goto err_free_rx;
				}
			}
			ret = 0;
			DISP_DEBUG("rx_len = %d\n", ctl.rx_len);
			for (i = 0; i < ctl.rx_len; i++) {
				DISP_DEBUG("rx_ptr[%d] = 0x%02X\n", i, ctl.rx_ptr[i]);
			}
		} else {
			DISP_INFO("Unsupported display(%s intf)\n",
				get_disp_intf_type_name(dd_ptr->intf_type));
			ret = -EINVAL;
		}
	} else {
		DISP_INFO("Unsupported display id\n");
		ret = -EINVAL;
	}

err_free_rx:
	if (ctl.rx_len && ctl.rx_ptr)
		kfree(ctl.rx_ptr);
err_free_tx:
	if (ctl.tx_len && ctl.tx_ptr)
		kfree(ctl.tx_ptr);
exit:
	mutex_unlock(&client->client_lock);
	return ret;
}

/* Ioctl table */
static const struct disp_ioctl_desc disp_ioctls[] = {
	DISP_IOCTL_DEF(MI_DISP_IOCTL_VERSION, mi_disp_ioctl_get_version),
	DISP_IOCTL_DEF(MI_DISP_IOCTL_SET_FEATURE, mi_disp_ioctl_set_feature),
	DISP_IOCTL_DEF(MI_DISP_IOCTL_SET_DOZE_BRIGHTNESS, mi_disp_ioctl_set_doze_brightness),
	DISP_IOCTL_DEF(MI_DISP_IOCTL_GET_DOZE_BRIGHTNESS, mi_disp_ioctl_get_doze_brightness),
	DISP_IOCTL_DEF(MI_DISP_IOCTL_GET_PANEL_INFO, mi_disp_ioctl_get_panel_info),
	DISP_IOCTL_DEF(MI_DISP_IOCTL_GET_WP_INFO, mi_disp_ioctl_get_wp_info),
	DISP_IOCTL_DEF(MI_DISP_IOCTL_GET_FPS, mi_disp_ioctl_get_fps),
	DISP_IOCTL_DEF(MI_DISP_IOCTL_REGISTER_EVENT, mi_disp_ioctl_register_event),
	DISP_IOCTL_DEF(MI_DISP_IOCTL_DEREGISTER_EVENT, mi_disp_ioctl_deregister_event),
	DISP_IOCTL_DEF(MI_DISP_IOCTL_WRITE_DSI_CMD, mi_disp_ioctl_write_dsi_cmd),
	DISP_IOCTL_DEF(MI_DISP_IOCTL_READ_DSI_CMD, mi_disp_ioctl_read_dsi_cmd),
	DISP_IOCTL_DEF(MI_DISP_IOCTL_SET_BRIGHTNESS, mi_disp_ioctl_set_brightness),
	DISP_IOCTL_DEF(MI_DISP_IOCTL_GET_BRIGHTNESS, mi_disp_ioctl_get_brightness),
};

#define DISP_IOCTL_COUNT	ARRAY_SIZE(disp_ioctls)

long mi_disp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct disp_feature_client *client = filp->private_data;
	const struct disp_ioctl_desc *ioctl = NULL;
	disp_ioctl_func_t *func;
	unsigned int nr = _IOC_NR(cmd);
	int ret = -EINVAL;
	char stack_kdata[128];
	char *kdata = NULL;
	unsigned int in_size, out_size, drv_size, ksize;


	if (nr >= DISP_IOCTL_COUNT)
		goto err_i1;

	ioctl = &disp_ioctls[nr];

	drv_size = _IOC_SIZE(ioctl->cmd);
	out_size = in_size = _IOC_SIZE(cmd);
	if ((cmd & ioctl->cmd & IOC_IN) == 0)
		in_size = 0;
	if ((cmd & ioctl->cmd & IOC_OUT) == 0)
		out_size = 0;
	ksize = max(max(in_size, out_size), drv_size);

	DISP_TIME_DEBUG("pid=%d, %s\n", task_pid_nr(current), ioctl->name);

	func = ioctl->func;

	if (unlikely(!func)) {
		pr_info("no function\n");
		ret = -EINVAL;
		goto err_i1;
	}

	if (ksize <= sizeof(stack_kdata)) {
		kdata = stack_kdata;
	} else {
		kdata = kmalloc(ksize, GFP_KERNEL);
		if (!kdata) {
			ret = -ENOMEM;
			goto err_i1;
		}
	}

	if (copy_from_user(kdata, (void __user *)arg, in_size) != 0) {
		ret = -EFAULT;
		goto err_i1;
	}

	if (ksize > in_size)
		memset(kdata + in_size, 0, ksize - in_size);

	ret  = func(client, kdata);
	if (copy_to_user((void __user *)arg, kdata, out_size) != 0)
		ret = -EFAULT;

err_i1:
	if (!ioctl)
		pr_debug("invalid ioctl: pid=%d, cmd=0x%02x, nr=0x%02x\n",
			  task_pid_nr(current), cmd, nr);

	if (kdata != stack_kdata)
		kfree(kdata);
	if (ret)
		pr_debug("pid=%d, ret = %d\n", task_pid_nr(current), ret);
	return ret;
}


#ifdef CONFIG_COMPAT
long mi_disp_ioctl_compat(struct file *file,
		unsigned int cmd, unsigned long arg)
{
	return mi_disp_ioctl(file, cmd, arg);
}
#endif

