/* Copyright (c) 2013-2015, 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/idr.h>
#include <linux/slab.h>
#include "esoc.h"

static DEFINE_IDA(esoc_ida);

/* SYSFS */
static ssize_t
esoc_name_show(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	return snprintf(buf, ESOC_NAME_LEN, "%s", to_esoc_clink(dev)->name);
}

static ssize_t
esoc_link_show(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	return snprintf(buf, ESOC_LINK_LEN, "%s",
				to_esoc_clink(dev)->link_name);
}

static ssize_t
esoc_link_info_show(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	return snprintf(buf, ESOC_LINK_LEN, "%s",
				to_esoc_clink(dev)->link_info);
}

static struct device_attribute esoc_clink_attrs[] = {

	__ATTR_RO(esoc_name),
	__ATTR_RO(esoc_link),
	__ATTR_RO(esoc_link_info),
	__ATTR_NULL,
};

static int esoc_bus_match(struct device *dev, struct device_driver *drv)
{
	int i = 0, match = 1;
	struct esoc_clink *esoc_clink = to_esoc_clink(dev);
	struct esoc_drv *esoc_drv = to_esoc_drv(drv);
	int entries = esoc_drv->compat_entries;
	struct esoc_compat *table = esoc_drv->compat_table;

	for (i = 0; i < entries; i++) {
		if (strcasecmp(esoc_clink->name, table[i].name) == 0)
			return match;
	}
	return 0;
}

static int esoc_bus_probe(struct device *dev)
{
	int ret;
	struct esoc_clink *esoc_clink = to_esoc_clink(dev);
	struct esoc_drv *esoc_drv = to_esoc_drv(dev->driver);

	ret = esoc_drv->probe(esoc_clink, esoc_drv);
	if (ret) {
		pr_err("failed to probe %s dev\n", esoc_clink->name);
		return ret;
	}
	return 0;
}

struct bus_type esoc_bus_type = {
	.name = "esoc",
	.match = esoc_bus_match,
	.dev_attrs = esoc_clink_attrs,
};
EXPORT_SYMBOL(esoc_bus_type);

struct device esoc_bus = {
	.init_name = "esoc-bus"
};
EXPORT_SYMBOL(esoc_bus);

/* bus accessor */
static void esoc_clink_release(struct device *dev)
{
	struct esoc_clink *esoc_clink = to_esoc_clink(dev);
	ida_simple_remove(&esoc_ida, esoc_clink->id);
	kfree(esoc_clink);
}

static int esoc_clink_match_id(struct device *dev, void *id)
{
	struct esoc_clink *esoc_clink = to_esoc_clink(dev);
	int *esoc_id = (int *)id;

	if (esoc_clink->id == *esoc_id) {
		if (!try_module_get(esoc_clink->owner))
			return 0;
		return 1;
	}
	return 0;
}

static int esoc_clink_match_node(struct device *dev, void *id)
{
	struct esoc_clink *esoc_clink = to_esoc_clink(dev);
	struct device_node *node = id;

	if (esoc_clink->np == node) {
		if (!try_module_get(esoc_clink->owner))
			return 0;
		return 1;
	}
	return 0;
}

void esoc_for_each_dev(void *data, int (*fn)(struct device *dev, void *))
{
	int ret;

	ret = bus_for_each_dev(&esoc_bus_type, NULL, data, fn);
	return;
}
EXPORT_SYMBOL(esoc_for_each_dev);

struct esoc_clink *get_esoc_clink(int id)
{
	struct esoc_clink *esoc_clink;
	struct device *dev;

	dev = bus_find_device(&esoc_bus_type, NULL, &id, esoc_clink_match_id);
	if (IS_ERR(dev))
		return NULL;
	esoc_clink = to_esoc_clink(dev);
	return esoc_clink;
}
EXPORT_SYMBOL(get_esoc_clink);

struct esoc_clink *get_esoc_clink_by_node(struct device_node *node)
{
	struct esoc_clink *esoc_clink;
	struct device *dev;

	dev = bus_find_device(&esoc_bus_type, NULL, node,
						esoc_clink_match_node);
	if (IS_ERR(dev))
		return NULL;
	esoc_clink = to_esoc_clink(dev);
	return esoc_clink;
}

void put_esoc_clink(struct esoc_clink *esoc_clink)
{
	module_put(esoc_clink->owner);
}
EXPORT_SYMBOL(put_esoc_clink);

bool esoc_req_eng_enabled(struct esoc_clink *esoc_clink)
{
	return !esoc_clink->req_eng ? false : true;
}
EXPORT_SYMBOL(esoc_req_eng_enabled);

bool esoc_cmd_eng_enabled(struct esoc_clink *esoc_clink)
{
	return !esoc_clink->cmd_eng ? false : true;
}
EXPORT_SYMBOL(esoc_cmd_eng_enabled);
/* ssr operations */
int esoc_clink_register_ssr(struct esoc_clink *esoc_clink)
{
	int ret;
	int len;
	char *subsys_name;

	len = strlen("esoc") + sizeof(esoc_clink->id);
	subsys_name = kzalloc(len, GFP_KERNEL);
	if (IS_ERR(subsys_name))
		return PTR_ERR(subsys_name);
	snprintf(subsys_name, len, "esoc%d", esoc_clink->id);
	esoc_clink->subsys.name = subsys_name;
	esoc_clink->dev.of_node = esoc_clink->np;
	esoc_clink->subsys.dev = &esoc_clink->pdev->dev;
	esoc_clink->subsys_dev = subsys_register(&esoc_clink->subsys);
	if (IS_ERR(esoc_clink->subsys_dev)) {
		dev_err(&esoc_clink->dev, "failed to register ssr node\n");
		ret = PTR_ERR(esoc_clink->subsys_dev);
		goto subsys_err;
	}
	return 0;
subsys_err:
	kfree(subsys_name);
	return ret;
}
EXPORT_SYMBOL(esoc_clink_register_ssr);

void esoc_clink_unregister_ssr(struct esoc_clink *esoc_clink)
{
	subsys_unregister(esoc_clink->subsys_dev);
	kfree(esoc_clink->subsys.name);
}
EXPORT_SYMBOL(esoc_clink_unregister_ssr);

int esoc_clink_request_ssr(struct esoc_clink *esoc_clink)
{
	subsystem_restart_dev(esoc_clink->subsys_dev);
	return 0;
}
EXPORT_SYMBOL(esoc_clink_request_ssr);

/* bus operations */
void esoc_clink_evt_notify(enum esoc_evt evt, struct esoc_clink *esoc_clink)
{
	unsigned long flags;

	spin_lock_irqsave(&esoc_clink->notify_lock, flags);
	notify_esoc_clients(esoc_clink, evt);
	if (esoc_clink->req_eng && esoc_clink->req_eng->handle_clink_evt)
		esoc_clink->req_eng->handle_clink_evt(evt, esoc_clink->req_eng);
	if (esoc_clink->cmd_eng && esoc_clink->cmd_eng->handle_clink_evt)
		esoc_clink->cmd_eng->handle_clink_evt(evt, esoc_clink->cmd_eng);
	spin_unlock_irqrestore(&esoc_clink->notify_lock, flags);
}
EXPORT_SYMBOL(esoc_clink_evt_notify);

void *get_esoc_clink_data(struct esoc_clink *esoc)
{
	return esoc->clink_data;
}
EXPORT_SYMBOL(get_esoc_clink_data);

void set_esoc_clink_data(struct esoc_clink *esoc, void *data)
{
	esoc->clink_data = data;
}
EXPORT_SYMBOL(set_esoc_clink_data);

void esoc_clink_queue_request(enum esoc_req req, struct esoc_clink *esoc_clink)
{
	unsigned long flags;
	struct esoc_eng *req_eng;

	spin_lock_irqsave(&esoc_clink->notify_lock, flags);
	if (esoc_clink->req_eng != NULL) {
		req_eng = esoc_clink->req_eng;
		req_eng->handle_clink_req(req, req_eng);
	}
	spin_unlock_irqrestore(&esoc_clink->notify_lock, flags);
}
EXPORT_SYMBOL(esoc_clink_queue_request);

void esoc_set_drv_data(struct esoc_clink *esoc_clink, void *data)
{
	dev_set_drvdata(&esoc_clink->dev, data);
}
EXPORT_SYMBOL(esoc_set_drv_data);

void *esoc_get_drv_data(struct esoc_clink *esoc_clink)
{
	return dev_get_drvdata(&esoc_clink->dev);
}
EXPORT_SYMBOL(esoc_get_drv_data);

/* bus registration functions */
void esoc_clink_unregister(struct esoc_clink *esoc_clink)
{
	if (get_device(&esoc_clink->dev) != NULL) {
		device_unregister(&esoc_clink->dev);
		put_device(&esoc_clink->dev);
	}
}
EXPORT_SYMBOL(esoc_clink_unregister);

int esoc_clink_register(struct esoc_clink *esoc_clink)
{
	int id, err;
	struct device *dev;

	if (!esoc_clink->name || !esoc_clink->link_name ||
					!esoc_clink->clink_ops) {
		dev_err(esoc_clink->parent, "invalid esoc arguments\n");
		return -EINVAL;
	}
	id = ida_simple_get(&esoc_ida, 0, ESOC_DEV_MAX, GFP_KERNEL);
	if (id < 0) {
		err = id;
		goto exit_ida;
	}
	esoc_clink->id = id;
	dev = &esoc_clink->dev;
	dev->bus = &esoc_bus_type;
	dev->release = esoc_clink_release;
	if (!esoc_clink->parent)
		dev->parent = &esoc_bus;
	else
		dev->parent = esoc_clink->parent;
	dev_set_name(dev, "esoc%d", id);
	err = device_register(dev);
	if (err) {
		dev_err(esoc_clink->parent, "esoc device register failed\n");
		goto exit_ida;
	}
	spin_lock_init(&esoc_clink->notify_lock);
	return 0;
exit_ida:
	ida_simple_remove(&esoc_ida, id);
	pr_err("unable to register %s, err = %d\n", esoc_clink->name, err);
	return err;
}
EXPORT_SYMBOL(esoc_clink_register);

int esoc_clink_register_req_eng(struct esoc_clink *esoc_clink,
						struct esoc_eng *eng)
{
	if (esoc_clink->req_eng)
		return -EBUSY;
	if (!eng->handle_clink_req)
		return -EINVAL;
	esoc_clink->req_eng = eng;
	eng->esoc_clink = esoc_clink;
	esoc_clink_evt_notify(ESOC_REQ_ENG_ON, esoc_clink);
	return 0;
}
EXPORT_SYMBOL(esoc_clink_register_req_eng);

int esoc_clink_register_cmd_eng(struct esoc_clink *esoc_clink,
						struct esoc_eng *eng)
{
	if (esoc_clink->cmd_eng)
		return -EBUSY;
	esoc_clink->cmd_eng = eng;
	eng->esoc_clink = esoc_clink;
	esoc_clink_evt_notify(ESOC_CMD_ENG_ON, esoc_clink);
	return 0;
}
EXPORT_SYMBOL(esoc_clink_register_cmd_eng);

void esoc_clink_unregister_req_eng(struct esoc_clink *esoc_clink,
						struct esoc_eng *eng)
{
	esoc_clink->req_eng = NULL;
	esoc_clink_evt_notify(ESOC_REQ_ENG_OFF, esoc_clink);
}
EXPORT_SYMBOL(esoc_clink_unregister_req_eng);

void esoc_clink_unregister_cmd_eng(struct esoc_clink *esoc_clink,
						struct esoc_eng *eng)
{
	esoc_clink->cmd_eng = NULL;
	esoc_clink_evt_notify(ESOC_CMD_ENG_OFF, esoc_clink);
}
EXPORT_SYMBOL(esoc_clink_unregister_cmd_eng);

int esoc_drv_register(struct esoc_drv *driver)
{
	int ret;

	driver->driver.bus = &esoc_bus_type;
	driver->driver.probe = esoc_bus_probe;
	ret = driver_register(&driver->driver);
	if (ret)
		return ret;
	return 0;
}
EXPORT_SYMBOL(esoc_drv_register);

static int __init esoc_init(void)
{
	int ret;

	ret = device_register(&esoc_bus);
	if (ret) {
		pr_err("esoc bus device register fail\n");
		return ret;
	}
	ret = bus_register(&esoc_bus_type);
	if (ret) {
		pr_err("esoc bus register fail\n");
		return ret;
	}
	pr_debug("esoc bus registration done\n");
	return 0;
}

subsys_initcall(esoc_init);
MODULE_LICENSE("GPL v2");
