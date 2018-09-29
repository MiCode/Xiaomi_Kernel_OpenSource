/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/of_device.h>
#include <linux/completion.h>
#include <linux/idr.h>
#include <linux/pm_runtime.h>
#include <soc/soundwire.h>

struct boardinfo {
	struct list_head	list;
	struct swr_boardinfo	board_info;
};

static LIST_HEAD(board_list);
static LIST_HEAD(swr_master_list);
static DEFINE_MUTEX(board_lock);
static DEFINE_IDR(master_idr);
static DEFINE_MUTEX(swr_lock);

static struct device_type swr_dev_type;

#define SOUNDWIRE_NAME_SIZE	32

static void swr_master_put(struct swr_master *master)
{
	if (master)
		put_device(&master->dev);
}

static struct swr_master *swr_master_get(struct swr_master *master)
{
	if (!master || !get_device(&master->dev))
		return NULL;
	return master;
}

static void swr_dev_release(struct device *dev)
{
	struct swr_device *swr_dev = to_swr_device(dev);
	struct swr_master *master;

	if (!swr_dev)
		return;
	master = swr_dev->master;
	if (!master)
		return;
	mutex_lock(&master->mlock);
	list_del_init(&swr_dev->dev_list);
	mutex_unlock(&master->mlock);
	swr_master_put(swr_dev->master);
	kfree(swr_dev);
}

/**
 * swr_remove_device - remove a soundwire device
 * @swr_dev: soundwire device to remove
 *
 * Remove a soundwire device. Go through the soundwire
 * device list that master has and remove swr_dev from
 * it.
 */
void swr_remove_device(struct swr_device *swr_dev)
{
	struct swr_device *swr_dev_loop, *safe;

	/*
	 * master still has reference to all nodes and deletes
	 * at platform_unregister, so need to init the deleted
	 * entry
	 */
	list_for_each_entry_safe(swr_dev_loop, safe,
				 &swr_dev->master->devices,
				 dev_list) {
		if (swr_dev == swr_dev_loop)
			list_del_init(&swr_dev_loop->dev_list);
	}
}
EXPORT_SYMBOL(swr_remove_device);

/**
 * swr_new_device - instantiate a new soundwire device
 * @master: Controller to which device is connected
 * @info: Describes the soundwire device
 * Context: can sleep
 *
 * Create a soundwire device. Binding is handled through driver model
 * probe/remove methods. A driver may be bound to this device when
 * the function gets returned.
 *
 * Returns a soundwire new device or NULL
 */
struct swr_device *swr_new_device(struct swr_master *master,
				 struct swr_boardinfo const *info)
{
	int result;
	struct swr_device *swr;

	if (!master || !swr_master_get(master)) {
		pr_err("%s: master is NULL\n", __func__);
		return NULL;
	}

	swr = kzalloc(sizeof(*swr), GFP_KERNEL);
	if (!swr) {
		put_device(&master->dev);
		return NULL;
	}
	swr->master = master;
	swr->addr = info->addr;
	strlcpy(swr->name, info->name, sizeof(swr->name));
	swr->dev.type = &swr_dev_type;
	swr->dev.parent = &master->dev;
	swr->dev.bus = &soundwire_type;
	swr->dev.release = swr_dev_release;
	swr->dev.of_node = info->of_node;
	mutex_lock(&master->mlock);
	list_add_tail(&swr->dev_list, &master->devices);
	mutex_unlock(&master->mlock);

	dev_set_name(&swr->dev, "%s.%lx", swr->name, swr->addr);
	result = device_register(&swr->dev);
	if (result) {
		dev_err(&master->dev, "device [%s] register failed err %d\n",
			swr->name, result);
		goto err_out;
	}
	dev_dbg(&master->dev, "Device [%s] registered with bus id %s\n",
		swr->name, dev_name(&swr->dev));
	return swr;

err_out:
	dev_dbg(&master->dev, "Failed to register swr device %s at 0x%lx %d\n",
		swr->name, swr->addr, result);
	swr_master_put(master);
	kfree(swr);
	return NULL;
}
EXPORT_SYMBOL(swr_new_device);

/**
 * of_register_swr_devices - register child devices on to the soundwire bus
 * @master: pointer to soundwire master device
 *
 * Registers a soundwire device for each child node of master node which has
 * a "swr-devid" property
 *
 */
int of_register_swr_devices(struct swr_master *master)
{
	struct swr_device *swr;
	struct device_node *node;

	if (!master->dev.of_node)
		return -EINVAL;

	for_each_available_child_of_node(master->dev.of_node, node) {
		struct swr_boardinfo info = {};
		u64 addr;

		dev_dbg(&master->dev, "of_swr:register %s\n", node->full_name);

		if (of_modalias_node(node, info.name, sizeof(info.name)) < 0) {
			dev_err(&master->dev, "of_swr:modalias failure %s\n",
				node->full_name);
			continue;
		}
		if (of_property_read_u64(node, "reg", &addr)) {
			dev_err(&master->dev, "of_swr:invalid reg %s\n",
				node->full_name);
			continue;
		}
		info.addr = addr;
		info.of_node = of_node_get(node);
		master->num_dev++;
		swr = swr_new_device(master, &info);
		if (!swr) {
			dev_err(&master->dev, "of_swr: Register failed %s\n",
				node->full_name);
			of_node_put(node);
			master->num_dev--;
			continue;
		}
	}
	return 0;
}
EXPORT_SYMBOL(of_register_swr_devices);

/**
 * swr_port_response - response from master to free the completed transaction
 * @mstr: pointer to soundwire master device
 * @tid: transaction id that indicates transaction to be freed.
 *
 * Master calls this function to free the compeleted transaction memory
 */
void swr_port_response(struct swr_master *mstr, u8 tid)
{
	struct swr_params *txn;

	txn = mstr->port_txn[tid];

	if (txn == NULL) {
		dev_err(&mstr->dev, "%s: transaction is already NULL\n",
			__func__);
		return;
	}
	mstr->port_txn[tid] = NULL;
	kfree(txn);
}
EXPORT_SYMBOL(swr_port_response);

/**
 * swr_remove_from_group - remove soundwire slave devices from group
 * @dev: pointer to the soundwire slave device
 * dev_num: device number of the soundwire slave device
 *
 * Returns error code for failure and 0 for success
 */
int swr_remove_from_group(struct swr_device *dev, u8 dev_num)
{
	struct swr_master *master;

	if (!dev)
		return -ENODEV;

	master = dev->master;
	if (!master)
		return -EINVAL;

	if (!dev->group_id)
		return 0;

	if (master->gr_sid == dev_num)
		return 0;

	if (master->remove_from_group && master->remove_from_group(master))
		dev_dbg(&master->dev, "%s: falling back to GROUP_NONE\n",
			__func__);

	return 0;
}
EXPORT_SYMBOL(swr_remove_from_group);

/**
 * swr_slvdev_datapath_control - Enables/Disables soundwire slave device
 *                               data path
 * @dev: pointer to soundwire slave device
 * @dev_num: device number of the soundwire slave device
 *
 * Returns error code for failure and 0 for success
 */
int swr_slvdev_datapath_control(struct swr_device *dev, u8 dev_num,
				bool enable)
{
	struct swr_master *master;

	if (!dev)
		return -ENODEV;

	master = dev->master;
	if (!master)
		return -EINVAL;

	if (dev->group_id) {
		/* Broadcast */
		if (master->gr_sid != dev_num) {
			if (!master->gr_sid)
				master->gr_sid = dev_num;
			else
				return 0;
		}
	}

	if (master->slvdev_datapath_control)
		master->slvdev_datapath_control(master, enable);

	return 0;
}
EXPORT_SYMBOL(swr_slvdev_datapath_control);

/**
 * swr_connect_port - enable soundwire slave port(s)
 * @dev: pointer to soundwire slave device
 * @port_id: logical port id(s) of soundwire slave device
 * @num_port: number of slave device ports need to be enabled
 * @ch_mask: channels for each port that needs to be enabled
 * @ch_rate: rate at which each port/channels operate
 * @num_ch: number of channels for each port
 *
 * soundwire slave device call swr_connect_port API to enable all/some of
 * its ports and corresponding channels and channel rate. This API will
 * call master connect_port callback function to calculate frame structure
 * and enable master and slave ports
 */
int swr_connect_port(struct swr_device *dev, u8 *port_id, u8 num_port,
			u8 *ch_mask, u32 *ch_rate, u8 *num_ch)
{
	u8 i = 0;
	int ret = 0;
	struct swr_params *txn = NULL;
	struct swr_params **temp_txn = NULL;
	struct swr_master *master = dev->master;

	if (!master) {
		pr_err("%s: Master is NULL\n", __func__);
		return -EINVAL;
	}
	if (num_port > SWR_MAX_DEV_PORT_NUM) {
		dev_err(&master->dev, "%s: num_port %d exceeds max port %d\n",
			__func__, num_port, SWR_MAX_DEV_PORT_NUM);
		return -EINVAL;
	}

	/*
	 * create "txn" to accommodate ports enablement of
	 * different slave devices calling swr_connect_port at the
	 * same time. Once master process the txn data, it calls
	 * swr_port_response() to free the transaction. Maximum
	 * of 256 transactions can be allocated.
	 */
	txn = kzalloc(sizeof(struct swr_params), GFP_KERNEL);
	if (!txn)
		return -ENOMEM;

	mutex_lock(&master->mlock);
	for (i = 0; i < master->last_tid; i++) {
		if (master->port_txn[i] == NULL)
			break;
	}
	if (i >= master->last_tid) {
		if (master->last_tid == 255) {
			mutex_unlock(&master->mlock);
			kfree(txn);
			dev_err(&master->dev, "%s Max tid reached\n",
				__func__);
			return -ENOMEM;
		}
		temp_txn = krealloc(master->port_txn,
				(i + 1) * sizeof(struct swr_params *),
				GFP_KERNEL);
		if (!temp_txn) {
			mutex_unlock(&master->mlock);
			kfree(txn);
			dev_err(&master->dev, "%s Not able to allocate\n"
				"master port transaction memory\n",
				__func__);
			return -ENOMEM;
		}
		master->port_txn = temp_txn;
		master->last_tid++;
	}
	master->port_txn[i] = txn;
	mutex_unlock(&master->mlock);
	txn->tid = i;

	txn->dev_id = dev->dev_num;
	txn->num_port = num_port;
	for (i = 0; i < num_port; i++) {
		txn->port_id[i] = port_id[i];
		txn->num_ch[i]  = num_ch[i];
		txn->ch_rate[i] = ch_rate[i];
		txn->ch_en[i]   = ch_mask[i];
	}
	ret = master->connect_port(master, txn);
	return ret;
}
EXPORT_SYMBOL(swr_connect_port);

/**
 * swr_disconnect_port - disable soundwire slave port(s)
 * @dev: pointer to soundwire slave device
 * @port_id: logical port id(s) of soundwire slave device
 * @num_port: number of slave device ports need to be disabled
 *
 * soundwire slave device call swr_disconnect_port API to disable all/some of
 * its ports. This API will call master disconnect_port callback function to
 * disable master and slave port and (re)configure frame structure
 */
int swr_disconnect_port(struct swr_device *dev, u8 *port_id, u8 num_port)
{
	u8 i = 0;
	int ret;
	struct swr_params *txn = NULL;
	struct swr_params **temp_txn = NULL;
	struct swr_master *master = dev->master;

	if (!master) {
		pr_err("%s: Master is NULL\n", __func__);
		return -EINVAL;
	}

	if (num_port > SWR_MAX_DEV_PORT_NUM) {
		dev_err(&master->dev, "%s: num_port %d exceeds max port %d\n",
			__func__, num_port, SWR_MAX_DEV_PORT_NUM);
		return -EINVAL;
	}

	txn = kzalloc(sizeof(struct swr_params), GFP_KERNEL);
	if (!txn)
		return -ENOMEM;

	mutex_lock(&master->mlock);
	for (i = 0; i < master->last_tid; i++) {
		if (master->port_txn[i] == NULL)
			break;
	}
	if (i >= master->last_tid) {
		if (master->last_tid == 255) {
			mutex_unlock(&master->mlock);
			kfree(txn);
			dev_err(&master->dev, "%s Max tid reached\n",
				__func__);
			return -ENOMEM;
		}
		temp_txn = krealloc(master->port_txn,
				(i + 1) * sizeof(struct swr_params *),
				GFP_KERNEL);
		if (!temp_txn) {
			mutex_unlock(&master->mlock);
			kfree(txn);
			dev_err(&master->dev, "%s Not able to allocate\n"
				"master port transaction memory\n",
				__func__);
			return -ENOMEM;
		}
		master->port_txn = temp_txn;
		master->last_tid++;
	}
	master->port_txn[i] = txn;
	mutex_unlock(&master->mlock);
	txn->tid = i;

	txn->dev_id = dev->dev_num;
	txn->num_port = num_port;
	for (i = 0; i < num_port; i++)
		txn->port_id[i] = port_id[i];
	ret = master->disconnect_port(master, txn);
	return ret;
}
EXPORT_SYMBOL(swr_disconnect_port);

/**
 * swr_get_logical_dev_num - Get soundwire slave logical device number
 * @dev: pointer to soundwire slave device
 * @dev_id: physical device id of soundwire slave device
 * @dev_num: pointer to logical device num of soundwire slave device
 *
 * This API will get the logical device number of soundwire slave device
 */
int swr_get_logical_dev_num(struct swr_device *dev, u64 dev_id,
			u8 *dev_num)
{
	int ret = 0;
	struct swr_master *master = dev->master;

	if (!master) {
		pr_err("%s: Master is NULL\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&master->mlock);
	ret = master->get_logical_dev_num(master, dev_id, dev_num);
	if (ret) {
		pr_err("%s: Error %d to get logical addr for device %llx\n",
			__func__, ret, dev_id);
	}
	mutex_unlock(&master->mlock);
	return ret;
}
EXPORT_SYMBOL(swr_get_logical_dev_num);

/**
 * swr_read - read soundwire slave device registers
 * @dev: pointer to soundwire slave device
 * @dev_num: logical device num of soundwire slave device
 * @reg_addr: base register address that needs to be read
 * @buf: pointer to store the values of registers from base address
 * @len: length of the buffer
 *
 * This API will read the value of the register address from
 * soundwire slave device
 */
int swr_read(struct swr_device *dev, u8 dev_num, u16 reg_addr,
	     void *buf, u32 len)
{
	struct swr_master *master = dev->master;

	if (!master)
		return -EINVAL;
	return master->read(master, dev_num, reg_addr, buf, len);
}
EXPORT_SYMBOL(swr_read);

/**
 * swr_bulk_write - write soundwire slave device registers
 * @dev: pointer to soundwire slave device
 * @dev_num: logical device num of soundwire slave device
 * @reg_addr: register address of soundwire slave device
 * @buf: contains value of register address
 * @len: indicates number of registers
 *
 * This API will write the value of the register address to
 * soundwire slave device
 */
int swr_bulk_write(struct swr_device *dev, u8 dev_num, void *reg,
		   const void *buf, size_t len)
{
	struct swr_master *master;

	if (!dev || !dev->master)
		return -EINVAL;

	master = dev->master;
	if (dev->group_id) {
		if (master->gr_sid != dev_num) {
			if (!master->gr_sid)
				master->gr_sid = dev_num;
			else
				return 0;
		}
		dev_num = dev->group_id;
	}
	if (master->bulk_write)
		return master->bulk_write(master, dev_num, reg, buf, len);

	return -EOPNOTSUPP;
}
EXPORT_SYMBOL(swr_bulk_write);

/**
 * swr_write - write soundwire slave device registers
 * @dev: pointer to soundwire slave device
 * @dev_num: logical device num of soundwire slave device
 * @reg_addr: register address of soundwire slave device
 * @buf: contains value of register address
 *
 * This API will write the value of the register address to
 * soundwire slave device
 */
int swr_write(struct swr_device *dev, u8 dev_num, u16 reg_addr,
	      const void *buf)
{
	struct swr_master *master = dev->master;

	if (!master)
		return -EINVAL;

	if (dev->group_id) {
		if (master->gr_sid != dev_num) {
			if (!master->gr_sid)
				master->gr_sid = dev_num;
			else
				return 0;
		}
		dev_num = dev->group_id;
	}
	return master->write(master, dev_num, reg_addr, buf);
}
EXPORT_SYMBOL(swr_write);

/**
 * swr_device_up - Function to bringup the soundwire slave device
 * @swr_dev: pointer to soundwire slave device
 * Context: can sleep
 *
 * This API will be called by soundwire master to bringup the slave
 * device.
 */
int swr_device_up(struct swr_device *swr_dev)
{
	struct device *dev;
	const struct swr_driver *sdrv;

	if (!swr_dev)
		return -EINVAL;

	dev = &swr_dev->dev;
	sdrv = to_swr_driver(dev->driver);
	if (!sdrv)
		return 0;

	if (sdrv->device_up)
		return sdrv->device_up(to_swr_device(dev));

	return -ENODEV;
}
EXPORT_SYMBOL(swr_device_up);

/**
 * swr_device_down - Function to call soundwire slave device down
 * @swr_dev: pointer to soundwire slave device
 * Context: can sleep
 *
 * This API will be called by soundwire master to put slave device in
 * shutdown state.
 */
int swr_device_down(struct swr_device *swr_dev)
{
	struct device *dev;
	const struct swr_driver *sdrv;

	if (!swr_dev)
		return -EINVAL;

	dev = &swr_dev->dev;
	sdrv = to_swr_driver(dev->driver);
	if (!sdrv)
		return 0;

	if (sdrv->device_down)
		return sdrv->device_down(to_swr_device(dev));

	return -ENODEV;
}
EXPORT_SYMBOL(swr_device_down);

/**
 * swr_reset_device - reset soundwire slave device
 * @swr_dev: pointer to soundwire slave device
 * Context: can sleep
 *
 * This API will be called by soundwire master to reset the slave
 * device when the slave device is not responding or in undefined
 * state
 */
int swr_reset_device(struct swr_device *swr_dev)
{
	struct device *dev;
	const struct swr_driver *sdrv;

	if (!swr_dev)
		return -EINVAL;

	dev = &swr_dev->dev;
	sdrv = to_swr_driver(dev->driver);
	if (!sdrv)
		return -EINVAL;

	if (sdrv->reset_device)
		return sdrv->reset_device(to_swr_device(dev));

	return -ENODEV;
}
EXPORT_SYMBOL(swr_reset_device);

/**
 * swr_set_device_group - Assign group id to the slave devices
 * @swr_dev: pointer to soundwire slave device
 * @id: group id to be assigned to slave device
 * Context: can sleep
 *
 * This API will be called either from soundwire master or slave
 * device to assign group id.
 */
int swr_set_device_group(struct swr_device *swr_dev, u8 id)
{
	struct swr_master *master;

	if (!swr_dev)
		return -EINVAL;

	swr_dev->group_id = id;
	master = swr_dev->master;
	if (!id && master)
		master->gr_sid = 0;

	return 0;
}
EXPORT_SYMBOL(swr_set_device_group);

static int swr_drv_probe(struct device *dev)
{
	const struct swr_driver *sdrv = to_swr_driver(dev->driver);

	if (!sdrv)
		return -EINVAL;

	if (sdrv->probe)
		return sdrv->probe(to_swr_device(dev));
	return -ENODEV;
}

static int swr_drv_remove(struct device *dev)
{
	const struct swr_driver *sdrv = to_swr_driver(dev->driver);

	if (!sdrv)
		return -EINVAL;

	if (sdrv->remove)
		return sdrv->remove(to_swr_device(dev));
	return -ENODEV;
}

static void swr_drv_shutdown(struct device *dev)
{
	const struct swr_driver *sdrv = to_swr_driver(dev->driver);

	if (!sdrv)
		return;

	if (sdrv->shutdown)
		sdrv->shutdown(to_swr_device(dev));
}

/**
 * swr_driver_register - register a soundwire driver
 * @drv: the driver to register
 * Context: can sleep
 */
int swr_driver_register(struct swr_driver *drv)
{
	drv->driver.bus = &soundwire_type;
	if (drv->probe)
		drv->driver.probe = swr_drv_probe;
	if (drv->remove)
		drv->driver.remove = swr_drv_remove;

	if (drv->shutdown)
		drv->driver.shutdown = swr_drv_shutdown;

	return driver_register(&drv->driver);
}
EXPORT_SYMBOL(swr_driver_register);

/**
 * swr_driver_unregister - unregister a soundwire driver
 * @drv: the driver to unregister
 */
void swr_driver_unregister(struct swr_driver *drv)
{
	if (drv)
		driver_unregister(&drv->driver);
}
EXPORT_SYMBOL(swr_driver_unregister);

static void swr_match_ctrl_to_boardinfo(struct swr_master *master,
				struct swr_boardinfo *bi)
{
	struct swr_device *swr;

	if (master->bus_num != bi->bus_num) {
		dev_dbg(&master->dev,
			"%s: master# %d and bi# %d does not match\n",
			__func__, master->bus_num, bi->bus_num);
		return;
	}

	swr = swr_new_device(master, bi);
	if (!swr)
		dev_err(&master->dev, "can't create new device for %s\n",
			bi->swr_slave->name);
}

/**
 * swr_master_add_boarddevices - Add devices registered by board info
 * @master: master to which these devices are to be added to.
 *
 * This API is called by master when it is up and running. If devices
 * on a master were registered before master, this will make sure that
 * they get probed when master is up.
 */
void swr_master_add_boarddevices(struct swr_master *master)
{
	struct boardinfo *bi;

	mutex_lock(&board_lock);
	list_add_tail(&master->list, &swr_master_list);
	list_for_each_entry(bi, &board_list, list)
		swr_match_ctrl_to_boardinfo(master, &bi->board_info);
	mutex_unlock(&board_lock);
}
EXPORT_SYMBOL(swr_master_add_boarddevices);

static void swr_unregister_device(struct swr_device *swr)
{
	if (swr)
		device_unregister(&swr->dev);
}

static void swr_master_release(struct device *dev)
{
	/* kfree of master done at swrm_remove of device */
}

#define swr_master_attr_gr NULL
static struct device_type swr_master_type = {
	.groups     = swr_master_attr_gr,
	.release    = swr_master_release,
};

static int __unregister(struct device *dev, void *null)
{
	swr_unregister_device(to_swr_device(dev));
	return 0;
}

/**
 * swr_unregister_master - unregister soundwire master controller
 * @master: the master being unregistered
 *
 * This API is called by master controller driver to unregister
 *  master controller that was registered by swr_register_master API.
 */
void swr_unregister_master(struct swr_master *master)
{
	int dummy;
	struct swr_master *m_ctrl;

	mutex_lock(&swr_lock);
	m_ctrl = idr_find(&master_idr, master->bus_num);
	mutex_unlock(&swr_lock);
	if (m_ctrl != master)
		return;

	mutex_lock(&board_lock);
	list_del(&master->list);
	mutex_unlock(&board_lock);

	/* free bus id */
	mutex_lock(&swr_lock);
	idr_remove(&master_idr, master->bus_num);
	mutex_unlock(&swr_lock);

	dummy = device_for_each_child(&master->dev, NULL, __unregister);
	device_unregister(&master->dev);
}
EXPORT_SYMBOL(swr_unregister_master);

/**
 * swr_register_master - register soundwire master controller
 * @master: master to be registered
 *
 * This API will register master with the framework. master->bus_num
 * is the desired number with which soundwire framework registers the
 * master.
 */
int swr_register_master(struct swr_master *master)
{
	int id;
	int status = 0;

	mutex_lock(&swr_lock);
	id = idr_alloc(&master_idr, master, master->bus_num,
			master->bus_num+1, GFP_KERNEL);
	mutex_unlock(&swr_lock);
	if (id < 0)
		return id;
	master->bus_num = id;

	/* Can't register until driver model init */
	if (WARN_ON(!soundwire_type.p)) {
		status = -EAGAIN;
		goto done;
	}

	dev_set_name(&master->dev, "swr%u", master->bus_num);
	master->dev.bus = &soundwire_type;
	master->dev.type = &swr_master_type;
	mutex_init(&master->mlock);
	status = device_register(&master->dev);
	if (status < 0)
		goto done;

	INIT_LIST_HEAD(&master->devices);
	pr_debug("%s: SWR master registered successfully %s\n",
		__func__, dev_name(&master->dev));
	return 0;

done:
	idr_remove(&master_idr, master->bus_num);
	return status;
}
EXPORT_SYMBOL(swr_register_master);

#define swr_device_attr_gr NULL
#define swr_device_uevent NULL
static struct device_type swr_dev_type = {
	.groups    = swr_device_attr_gr,
	.uevent    = swr_device_uevent,
	.release   = swr_dev_release,
};

static const struct swr_device_id *swr_match(const struct swr_device_id *id,
					     const struct swr_device *swr_dev)
{
	while (id->name[0]) {
		if (strcmp(swr_dev->name, id->name) == 0)
			return id;
		id++;
	}
	return NULL;
}

static int swr_device_match(struct device *dev, struct device_driver *driver)
{
	struct swr_device *swr_dev;
	struct swr_driver *drv = to_swr_driver(driver);

	if (!drv)
		return -EINVAL;

	if (dev->type == &swr_dev_type)
		swr_dev = to_swr_device(dev);
	else
		return 0;
	if (drv->id_table)
		return swr_match(drv->id_table, swr_dev) != NULL;

	if (driver->name)
		return strcmp(swr_dev->name, driver->name) == 0;
	return 0;
}
#ifdef CONFIG_PM_SLEEP
static int swr_legacy_suspend(struct device *dev, pm_message_t mesg)
{
	struct swr_device *swr_dev = NULL;
	struct swr_driver *driver;

	if (dev->type == &swr_dev_type)
		swr_dev = to_swr_device(dev);

	if (!swr_dev || !dev->driver)
		return 0;

	driver = to_swr_driver(dev->driver);
	if (!driver->suspend)
		return 0;

	return driver->suspend(swr_dev, mesg);
}

static int swr_legacy_resume(struct device *dev)
{
	struct swr_device *swr_dev = NULL;
	struct swr_driver *driver;

	if (dev->type == &swr_dev_type)
		swr_dev = to_swr_device(dev);

	if (!swr_dev || !dev->driver)
		return 0;

	driver = to_swr_driver(dev->driver);
	if (!driver->resume)
		return 0;

	return driver->resume(swr_dev);
}

static int swr_pm_suspend(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_suspend(dev);
	else
		return swr_legacy_suspend(dev, PMSG_SUSPEND);
}

static int swr_pm_resume(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_resume(dev);
	else
		return swr_legacy_resume(dev);
}
#else
#define swr_pm_suspend	NULL
#define swr_pm_resume	NULL
#endif /*CONFIG_PM_SLEEP*/

static const struct dev_pm_ops soundwire_pm = {
	.suspend = swr_pm_suspend,
	.resume = swr_pm_resume,
	SET_RUNTIME_PM_OPS(
		pm_generic_suspend,
		pm_generic_resume,
		NULL
		)
};

struct device soundwire_dev = {
	.init_name = "soundwire",
};

struct bus_type soundwire_type = {
	.name		= "soundwire",
	.match		= swr_device_match,
	.pm		= &soundwire_pm,
};
EXPORT_SYMBOL(soundwire_type);

static void __exit soundwire_exit(void)
{
	device_unregister(&soundwire_dev);
	bus_unregister(&soundwire_type);
}

static int __init soundwire_init(void)
{
	int retval;

	retval = bus_register(&soundwire_type);
	if (!retval)
		retval = device_register(&soundwire_dev);

	if (retval)
		bus_unregister(&soundwire_type);

	return retval;
}
postcore_initcall(soundwire_init);
module_exit(soundwire_exit);


MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Soundwire module");
MODULE_ALIAS("platform:soundwire");
