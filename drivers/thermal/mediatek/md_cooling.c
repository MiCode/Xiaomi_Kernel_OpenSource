// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/thermal.h>
#include <linux/types.h>
#include <mtk_ccci_common.h>
#include "md_cooling.h"

static DEFINE_MUTEX(md_cdev_list_lock);
static DEFINE_MUTEX(md_cooling_lock);
static LIST_HEAD(md_cdev_list);
static enum md_status md_current_status;
static unsigned int pa_num;

enum md_status get_md_status(void)
{
	enum md_status cur_status = MD_OFF;
	int md_state;

	mutex_lock(&md_cooling_lock);
	md_state = exec_ccci_kern_func_by_md_id(0, ID_GET_MD_STATE, NULL, 0);
	if (md_state == MD_STATE_INVALID || md_state == MD_STATE_EXCEPTION) {
		pr_warn("Invalid MD state(%d)!\n", md_state);
		cur_status = MD_OFF;
	} else {
		cur_status = md_current_status;
	}
	mutex_unlock(&md_cooling_lock);

	return cur_status;
}
EXPORT_SYMBOL_GPL(get_md_status);

void set_md_status(enum md_status status)
{
	mutex_lock(&md_cooling_lock);
	md_current_status = status;
	mutex_unlock(&md_cooling_lock);
}
EXPORT_SYMBOL_GPL(set_md_status);

int send_throttle_msg(unsigned int msg)
{
	int ret = 0;

	mutex_lock(&md_cooling_lock);
	ret = exec_ccci_kern_func_by_md_id(MD_SYS1,
			ID_THROTTLING_CFG, (char *)&msg, 4);
	mutex_unlock(&md_cooling_lock);

	if (ret)
		pr_err("send tmc msg 0x%x failed, ret:%d\n", msg, ret);
	else
		pr_debug("send tmc msg 0x%x done\n", msg);

	return ret;
}
EXPORT_SYMBOL_GPL(send_throttle_msg);

void update_throttle_power(unsigned int pa_id, unsigned int *pwr)
{
	struct md_cooling_device *md_cdev;

	mutex_lock(&md_cdev_list_lock);
	list_for_each_entry(md_cdev, &md_cdev_list, node) {
		if (md_cdev->type == MD_COOLING_TYPE_TX_PWR &&
			md_cdev->pa_id == pa_id) {
			memcpy(md_cdev->throttle_tx_power, pwr,
				sizeof(md_cdev->throttle_tx_power));
			mutex_unlock(&md_cdev_list_lock);
			return;
		}
	}
	mutex_unlock(&md_cdev_list_lock);
}
EXPORT_SYMBOL_GPL(update_throttle_power);

struct md_cooling_device*
get_md_cdev(enum md_cooling_type type, unsigned int pa_id)
{
	struct md_cooling_device *md_cdev;

	mutex_lock(&md_cdev_list_lock);
	list_for_each_entry(md_cdev, &md_cdev_list, node) {
		if (md_cdev->type == type &&
			md_cdev->pa_id == pa_id) {
			mutex_unlock(&md_cdev_list_lock);
			return md_cdev;
		}
	}
	mutex_unlock(&md_cdev_list_lock);

	return NULL;
}
EXPORT_SYMBOL_GPL(get_md_cdev);

unsigned int get_pa_num(void)
{
	return pa_num;
}
EXPORT_SYMBOL_GPL(get_pa_num);

static void update_pa_num(unsigned int id)
{
	mutex_lock(&md_cooling_lock);
	if (!pa_num || id > pa_num - 1)
		pa_num = id + 1;
	mutex_unlock(&md_cooling_lock);
}

static int _md_cooling_register(struct device_node *np,
		enum md_cooling_type type, unsigned long max_level,
		unsigned int *throttle_pwr,
		struct thermal_cooling_device_ops *cooling_ops, void *data)
{
	struct md_cooling_device *md_cdev;
	struct thermal_cooling_device *cdev;
	unsigned int id;

	if (of_property_read_u32(np, "id", &id)) {
		pr_err("%s: Missing id property in DT\n", __func__);
		return -EINVAL;
	}

	md_cdev = kzalloc(sizeof(*md_cdev), GFP_KERNEL);
	if (!md_cdev)
		return -ENOMEM;

	strncpy(md_cdev->name, np->name, strlen(np->name));
	md_cdev->type = type;
	md_cdev->pa_id = id;
	md_cdev->target_level = MD_COOLING_UNLIMITED_LV;
	md_cdev->max_level = max_level;
	if (throttle_pwr)
		memcpy(md_cdev->throttle_tx_power, throttle_pwr,
			sizeof(md_cdev->throttle_tx_power));
	if (data)
		md_cdev->dev_data = data;

	cdev = thermal_of_cooling_device_register(np, md_cdev->name,
			md_cdev, cooling_ops);
	if (IS_ERR(cdev)) {
		kfree(md_cdev);
		return -EINVAL;
	}
	md_cdev->cdev = cdev;

	mutex_lock(&md_cdev_list_lock);
	list_add(&md_cdev->node, &md_cdev_list);
	mutex_unlock(&md_cdev_list_lock);

	update_pa_num(md_cdev->pa_id);

	pr_info("register %s done, id=%d\n", md_cdev->name, md_cdev->pa_id);

	return 0;
}

int md_cooling_register(struct device_node *np, enum md_cooling_type type,
		unsigned long max_level, unsigned int *throttle_pwr,
		struct thermal_cooling_device_ops *cooling_ops, void *data)
{
	struct device_node *child;
	int count, ret = -1;

	if (!np)
		return ret;

	count = of_get_child_count(np);
	if (!count)
		return -EINVAL;

	for_each_child_of_node(np, child) {
		ret = _md_cooling_register(child, type,
			max_level, throttle_pwr, cooling_ops, data);
		of_node_put(child);
		if (ret)
			return ret;
	}

	return ret;
}
EXPORT_SYMBOL_GPL(md_cooling_register);

void md_cooling_unregister(enum md_cooling_type type)
{
	struct md_cooling_device *md_cdev;

	mutex_lock(&md_cdev_list_lock);
	list_for_each_entry(md_cdev, &md_cdev_list, node) {
		if (md_cdev->type == type) {
			thermal_cooling_device_unregister(md_cdev->cdev);
			list_del(&md_cdev->node);
			kfree(md_cdev);
		}
	}
	mutex_unlock(&md_cdev_list_lock);
}
EXPORT_SYMBOL_GPL(md_cooling_unregister);

MODULE_AUTHOR("Shun-Yao Yang <brian-sy.yang@mediatek.com>");
MODULE_DESCRIPTION("Mediatek modem cooling common driver");
MODULE_LICENSE("GPL v2");
