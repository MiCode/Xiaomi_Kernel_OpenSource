// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#define pr_fmt(fmt) "mi_disp_log:[%s:%d] " fmt, __func__, __LINE__

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
#include <linux/rtc.h>
#include <linux/kthread.h>
#include <uapi/linux/sched/types.h>
#include <uapi/drm/mi_disp.h>
//#include "dp_display.h"
//#include "dsi_display.h"
#include "mi_dsi_display.h"
//#include "dsi_panel.h"
//#include "mi_disp.h"
#include "mi_disp_core.h"
#include "mi_disp_procfs.h"
#include "mi_disp_feature.h"
#include "mi_dsi_panel.h"
#include "mi_disp_file.h"
#include "mi_disp_print.h"
#include "mi_disp_sysfs.h"

struct disp_feature *g_disp_feature = NULL;

extern int mi_cooler_brightness_clone_init(struct mtk_dsi *display);
extern void mi_cooler_brightness_clone_exit(void);

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
	struct disp_thread *dt_ptr = NULL;
	struct sched_param param = { 0 };

	if (!df) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	param.sched_priority = 16;
	if (is_support_disp_id(disp_id)) {
		dt_ptr = &df->d_display[disp_id].d_thread;
		dt_ptr->dd_ptr = &df->d_display[disp_id];
		kthread_init_worker(&dt_ptr->worker);
		dt_ptr->thread = kthread_run(kthread_worker_fn,
				&dt_ptr->worker, "disp_feature:%d", disp_id);

		ret = sched_setscheduler(dt_ptr->thread, SCHED_FIFO, &param);
		if (ret)
			DISP_WARN("display thread priority update failed: %d\n", ret);

		if (IS_ERR(dt_ptr->thread)) {
			DISP_ERROR("failed to create disp_feature kthread\n");
			dt_ptr->thread = NULL;
		}
		DISP_INFO("create disp_feature:%d kthread success\n", disp_id);
	} else {
		DISP_ERROR("Unknown display id, failed to create disp_feature kthread\n");
	}

	return ret;
}

static int mi_disp_feature_thread_destroy(struct disp_feature *df, int disp_id)
{
	int ret = 0;
	struct disp_thread *dt_ptr = NULL;

	if (!df) {
		DISP_ERROR("invalid params\n");
		return -EINVAL;
	}

	if (is_support_disp_id(disp_id)) {
		dt_ptr = &df->d_display[disp_id].d_thread;
		if (dt_ptr->thread) {
			kthread_flush_worker(&dt_ptr->worker);
			kthread_stop(dt_ptr->thread);
			dt_ptr->thread = NULL;
		}
		dt_ptr->dd_ptr = NULL;
		DISP_INFO("destroy disp_feature:%d kthread success\n", disp_id);
	} else {
		DISP_ERROR("Unknown display id, failed to destroy disp_feature kthread\n");
	}

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

	if (is_support_disp_id(disp_id) && is_support_disp_intf_type(intf_type)) {
		dd_ptr = &df->d_display[disp_id];
		dd_ptr->display = display;
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

		ret = mi_disp_procfs_init(dd_ptr, disp_id);
		if (ret) {
			DISP_ERROR("failed to create procfs entry\n");
			goto err_thread;
		}

		ret = mi_disp_debugfs_init(dd_ptr, disp_id);
		if (ret) {
			DISP_ERROR("failed to create debugfs entry\n");
			goto err_procfs;
		}

		mi_disp_cfg_init(display);

		ret = mi_cooler_brightness_clone_init(display);
		if (ret) {
			DISP_ERROR("failed to init cooler brightness clone\n");
			goto err_procfs;
		}

		DISP_INFO("attach %s display(%s intf) success\n", get_disp_id_name(disp_id),
				get_disp_intf_type_name(intf_type));

		atomic_set(&dd_ptr->pending_doze_cnt, 0);
	} else {
		DISP_ERROR("Unknown display or interface, failed to bind\n");
	}

	return 0;


err_procfs:
	mi_disp_procfs_deinit(dd_ptr, disp_id);
err_thread:
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

	if (is_support_disp_id(disp_id) && is_support_disp_intf_type(intf_type)) {
		dd_ptr = &df->d_display[disp_id];

		ret = mi_disp_debugfs_deinit(dd_ptr, disp_id);
		if (ret < 0)
			DISP_ERROR("failed to remove debugfs entry\n");

		ret = mi_disp_procfs_deinit(dd_ptr, disp_id);
		if (ret < 0)
			DISP_ERROR("failed to remove procfs entry\n");

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
	} else {
		DISP_ERROR("Unknown display or interface, failed to unbind\n");
	}

	return ret;
}

void mi_disp_feature_event_notify(struct disp_event *event, u8 *payload)
{
	struct disp_feature *df = mi_get_disp_feature();
	struct disp_feature_client *client;
	unsigned long flags;
	struct disp_pending_event *notify;
	int len = 0;

	if (!event || !event->length || !payload) {
		pr_err("event %pK len %d payload %pK\n",
			event, ((event) ? (event->length) : -1), payload);
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
			DISP_ERROR("Insufficient space %d for event %x len %d\n",
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


void mi_disp_feature_sysfs_notify(int sysfs_node)
{
	if (g_disp_feature) {
		switch (sysfs_node) {
		case MI_SYSFS_DISP_PARAM:
			sysfs_notify(&g_disp_feature->pdev->kobj, NULL, "disp_param");
			break;
		case MI_SYSFS_DYNAMIC_FPS:
			sysfs_notify(&g_disp_feature->pdev->kobj, NULL, "dynamic_fps");
			break;
		case MI_SYSFS_DOZE_BRIGHTNESS:
			sysfs_notify(&g_disp_feature->pdev->kobj, NULL, "doze_brightness");
			break;
		case MI_SYSFS_BRIGHTNESS_CLONE:
			sysfs_notify(&g_disp_feature->pdev->kobj, NULL, "brightness_clone");
			break;
		case MI_SYSFS_DC:
			sysfs_notify(&g_disp_feature->pdev->kobj, NULL, "dc_status");
			break;
		default:
			pr_err("Invalid params\n");
			break;
		}
	}
}

static ssize_t disp_param_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct disp_feature *dev = to_disp_feature(device);
	struct mtk_dsi *display = dev->d_display[DSI_PRIMARY].display;
	u32 param;
	int ret;

	sscanf(buf, "0x%x", &param);

	ret = dsi_display_set_disp_param(&display->conn, param);

	return ret ? ret : count;
}

static ssize_t disp_param_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_feature *dev = to_disp_feature(device);
	struct mtk_dsi *display = dev->d_display[DSI_PRIMARY].display;

	return dsi_display_get_disp_param(&display->conn, buf);
}

static ssize_t mipi_reg_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	int ret;
	struct disp_feature *dev = to_disp_feature(device);
	struct mtk_dsi *display = dev->d_display[DSI_PRIMARY].display;

	ret = dsi_display_write_mipi_reg(&display->conn, (char *)buf, count);

	return ret ? ret : count;
}

static ssize_t mipi_reg_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_feature *dev = to_disp_feature(device);
	struct mtk_dsi *display = dev->d_display[DSI_PRIMARY].display;

	return dsi_display_read_mipi_reg(&display->conn, buf);
}

static ssize_t oled_pmic_id_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	return 0;
	//return mi_dsi_display_read_oled_pmic_id(display, buf);
}

static ssize_t panel_info_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_feature *dev = to_disp_feature(device);
	struct mtk_dsi *display = dev->d_display[DSI_PRIMARY].display;
	return mi_dsi_display_read_panel_info(display, buf);
}

static ssize_t wp_info_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	return 0;
	//return mi_dsi_display_read_wp_info(display, buf);
}

static ssize_t dynamic_fps_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct disp_feature *dev = to_disp_feature(device);
	struct mtk_dsi *display = dev->d_display[DSI_PRIMARY].display;
	int fps = 0;
	mi_dsi_panel_get_fps(display, &fps);
	return snprintf(buf, PAGE_SIZE, "%d\n", fps);
}

static ssize_t doze_brightness_store(struct device *device,
			   struct device_attribute *attr,
			   const char *buf, size_t count)
{
	struct disp_feature *dev = to_disp_feature(device);
	struct mtk_dsi *display = dev->d_display[DSI_PRIMARY].display;
	int doze_brightness;
	int ret;

	ret = kstrtoint(buf, 0, &doze_brightness);
	if (ret)
		return ret;

	ret = mi_dsi_display_set_doze_brightness(display, doze_brightness);

	return ret ? ret : count;
}

static ssize_t doze_brightness_show(struct device *device,
			struct device_attribute *attr, char *buf)
{
	struct disp_feature *dev = to_disp_feature(device);
	struct mtk_dsi *display = dev->d_display[DSI_PRIMARY].display;
	int doze_brightness = 0;
	return mi_dsi_display_get_doze_brightness(display, &doze_brightness);
	snprintf(buf, PAGE_SIZE, "%d\n", doze_brightness);
}

static ssize_t gamma_test_show(struct device *device,
			struct device_attribute *attr, char *buf)
{

	int ret = 0;

	//ret = mi_dsi_display_read_gamma_param(display);
	if (ret) {
		pr_err("Failed to update panel id and gamma para!\n");
	}

	//ret = mi_dsi_display_print_gamma_param(display, buf);
	return ret;
}

static ssize_t brightness_clone_store(struct device *device,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct disp_feature *dev = to_disp_feature(device);
	struct mtk_dsi *display = dev->d_display[DSI_PRIMARY].display;
	int brightness;
	int ret;
	ret = kstrtoint(buf, 0, &brightness);
	if (ret)
		return ret;

	mi_dsi_display_set_brightness_clone(display, brightness);
	display->conn.brightness_clone = brightness;
	mi_disp_feature_sysfs_notify(MI_SYSFS_BRIGHTNESS_CLONE);
	return ret ? ret : count;
}

static ssize_t brightness_clone_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_feature *dev = to_disp_feature(device);
	struct mtk_dsi *display = dev->d_display[DSI_PRIMARY].display;
	int brightness_clone = 0;

	mi_dsi_display_get_brightness_clone(display, &brightness_clone);
	return snprintf(buf, PAGE_SIZE, "%d\n", brightness_clone);
}

static ssize_t dc_status_store(struct device *device,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct disp_feature *dev = to_disp_feature(device);
	struct mtk_dsi *display = dev->d_display[DSI_PRIMARY].display;
	int dc_status;
	int ret;
	ret = kstrtoint(buf, 0, &dc_status);
	if (ret)
		return ret;

	ret = display->conn.dc_status = dc_status;
	mi_disp_feature_sysfs_notify(MI_SYSFS_DC);
	return ret ? ret : count;
}

static ssize_t dc_status_show(struct device *device,
			   struct device_attribute *attr,
			   char *buf)
{
	struct disp_feature *dev = to_disp_feature(device);
	struct mtk_dsi *display = dev->d_display[DSI_PRIMARY].display;
	return snprintf(buf, PAGE_SIZE, "%d\n", display->conn.dc_status);
}
static DEVICE_ATTR_RW(disp_param);
static DEVICE_ATTR_RW(mipi_reg);
static DEVICE_ATTR_RO(oled_pmic_id);
static DEVICE_ATTR_RO(panel_info);
static DEVICE_ATTR_RO(wp_info);
static DEVICE_ATTR_RO(dynamic_fps);
static DEVICE_ATTR_RW(doze_brightness);
static DEVICE_ATTR_RO(gamma_test);
static DEVICE_ATTR_RW(brightness_clone);
static DEVICE_ATTR_RW(dc_status);

static struct attribute *disp_feature_attrs[] = {
	&dev_attr_disp_param.attr,
	&dev_attr_mipi_reg.attr,
	&dev_attr_oled_pmic_id.attr,
	&dev_attr_panel_info.attr,
	&dev_attr_wp_info.attr,
	&dev_attr_dynamic_fps.attr,
	&dev_attr_doze_brightness.attr,
	&dev_attr_gamma_test.attr,
	&dev_attr_brightness_clone.attr,
	&dev_attr_dc_status.attr,
	NULL
};

static const struct attribute_group disp_feature_group = {
	.attrs = disp_feature_attrs,
};

static const struct attribute_group *disp_feature_groups[] = {
	&disp_feature_group,
	NULL
};

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

	disp_core = mi_get_disp_core();
	if (!disp_core)
		return -ENODEV;

	if (g_disp_feature) {
		pr_info("mi disp_feature already initialized, return!\n");
		return 0;
	}

	df = kzalloc(sizeof(struct disp_feature), GFP_KERNEL);
	if (!df) {
		pr_err("can not allocate Buffer\n");
		ret = -ENOMEM;
		goto err_exit;
	}

	ret = mi_disp_cdev_register(DISP_FEATURE_DEVICE_NAME,
				&disp_feature_fops, &df->cdev);
	if (ret < 0) {
		pr_err("cdev register failed for %s\n", DISP_FEATURE_DEVICE_NAME);
		goto err_free_mem;
	}

	df->dev_id = df->cdev->dev;
	df->class = disp_core->class;
	df->pdev = device_create_with_groups(df->class, NULL,
			df->dev_id, df, disp_feature_groups,
			DISP_FEATURE_DEVICE_NAME);
	if (IS_ERR(df->pdev)) {
		DISP_ERROR("create device failed for %s\n", DISP_FEATURE_DEVICE_NAME);
		ret = -ENODEV;
		goto err_cdev_unreg;
	}

	df->version = MI_DISP_FEATURE_VERSION;
	for (i = MI_DISP_PRIMARY; i < MI_DISP_MAX; i++) {
		df->d_display[i].dev = NULL;
		df->d_display[i].display = NULL;
		df->d_display[i].intf_type = MI_INTF_MAX;
		mutex_init(&df->d_display[i].mutex_lock);
		init_waitqueue_head(&df->d_display[i].pending_wq);
	}

	INIT_LIST_HEAD(&df->client_list);
	spin_lock_init(&df->client_spinlock);
	df->initialized = true;

	g_disp_feature = df;

	pr_info("mi disp_feature driver initialized!\n");

	return 0;

err_cdev_unreg:
	mi_disp_cdev_unregister(df->cdev);
err_free_mem:
	kfree(df);
err_exit:
	return ret;
}

void mi_disp_feature_exit(void)
{
	if (!g_disp_feature)
		return;
	device_destroy(g_disp_feature->class, g_disp_feature->dev_id);
	mi_disp_cdev_unregister(g_disp_feature->cdev);
	kfree(g_disp_feature);
	g_disp_feature = NULL;
	mi_disp_core_deinit();
}

