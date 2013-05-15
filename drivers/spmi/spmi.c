/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/idr.h>
#include <linux/slab.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/spmi.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>

#include "spmi-dbgfs.h"

struct spmii_boardinfo {
	struct list_head	list;
	struct spmi_boardinfo	board_info;
};

static DEFINE_MUTEX(board_lock);
static LIST_HEAD(board_list);
static DEFINE_IDR(ctrl_idr);
static struct device_type spmi_ctrl_type = { 0 };

#define to_spmi(dev)	platform_get_drvdata(to_platform_device(dev))

/* Forward declarations */
struct bus_type spmi_bus_type;
static int spmi_register_controller(struct spmi_controller *ctrl);

/**
 * spmi_busnum_to_ctrl: Map bus number to controller
 * @busnum: bus number
 * Returns controller representing this bus number
 */
struct spmi_controller *spmi_busnum_to_ctrl(u32 bus_num)
{
	struct spmi_controller *ctrl;

	mutex_lock(&board_lock);
	ctrl = idr_find(&ctrl_idr, bus_num);
	mutex_unlock(&board_lock);

	return ctrl;
}
EXPORT_SYMBOL_GPL(spmi_busnum_to_ctrl);

/**
 * spmi_add_controller: Controller bring-up.
 * @ctrl: controller to be registered.
 * A controller is registered with the framework using this API. ctrl->nr is the
 * desired number with which SPMI framework registers the controller.
 * Function will return -EBUSY if the number is in use.
 */
int spmi_add_controller(struct spmi_controller *ctrl)
{
	int	id;
	int	status;

	pr_debug("adding controller for bus %d (0x%p)\n", ctrl->nr, ctrl);

	if (ctrl->nr & ~MAX_ID_MASK) {
		pr_err("invalid bus identifier %d\n", ctrl->nr);
		return -EINVAL;
	}

retry:
	if (idr_pre_get(&ctrl_idr, GFP_KERNEL) == 0) {
		pr_err("no free memory for idr\n");
		return -ENOMEM;
	}

	mutex_lock(&board_lock);
	status = idr_get_new_above(&ctrl_idr, ctrl, ctrl->nr, &id);
	if (status == 0 && id != ctrl->nr) {
		status = -EBUSY;
		idr_remove(&ctrl_idr, id);
	}
	mutex_unlock(&board_lock);
	if (status == -EAGAIN)
		goto retry;

	if (status == 0)
		status = spmi_register_controller(ctrl);
	return status;
}
EXPORT_SYMBOL_GPL(spmi_add_controller);

/**
 * spmi_del_controller: Controller tear-down.
 * @ctrl: controller to which this device is to be added to.
 *
 * Controller added with the above API is torn down using this API.
 */
int spmi_del_controller(struct spmi_controller *ctrl)
{
	return -ENXIO;
}
EXPORT_SYMBOL_GPL(spmi_del_controller);

#define spmi_device_attr_gr NULL
#define spmi_device_uevent NULL
static void spmi_dev_release(struct device *dev)
{
	struct spmi_device *spmidev = to_spmi_device(dev);
	kfree(spmidev);
}

static struct device_type spmi_dev_type = {
	.groups		= spmi_device_attr_gr,
	.uevent		= spmi_device_uevent,
	.release	= spmi_dev_release,
};

/**
 * spmi_alloc_device: Allocate a new SPMI devices.
 * @ctrl: controller to which this device is to be added to.
 * Context: can sleep
 *
 * Allows a driver to allocate and initialize a SPMI device without
 * registering it immediately.  This allows a driver to directly fill
 * the spmi_device structure before calling spmi_add_device().
 *
 * Caller is responsible to call spmi_add_device() on the returned
 * spmi_device.  If the caller needs to discard the spmi_device without
 * adding it, then spmi_dev_put() should be called.
 */
struct spmi_device *spmi_alloc_device(struct spmi_controller *ctrl)
{
	struct spmi_device *spmidev;

	if (!ctrl) {
		pr_err("Missing SPMI controller\n");
		return NULL;
	}

	spmidev = kzalloc(sizeof(*spmidev), GFP_KERNEL);
	if (!spmidev) {
		dev_err(&ctrl->dev, "unable to allocate spmi_device\n");
		return NULL;
	}

	spmidev->ctrl = ctrl;
	spmidev->dev.parent = ctrl->dev.parent;
	spmidev->dev.bus = &spmi_bus_type;
	spmidev->dev.type = &spmi_dev_type;
	device_initialize(&spmidev->dev);

	return spmidev;
}
EXPORT_SYMBOL_GPL(spmi_alloc_device);

/* Validate the SPMI device structure */
static struct device *get_valid_device(struct spmi_device *spmidev)
{
	struct device *dev;

	if (!spmidev)
		return NULL;

	dev = &spmidev->dev;
	if (dev->bus != &spmi_bus_type || dev->type != &spmi_dev_type)
		return NULL;

	return dev;
}

/**
 * spmi_add_device: Add a new device without register board info.
 * @ctrl: controller to which this device is to be added to.
 *
 * Called when device doesn't have an explicit client-driver to be probed, or
 * the client-driver is a module installed dynamically.
 */
int spmi_add_device(struct spmi_device *spmidev)
{
	int rc;
	struct device *dev = get_valid_device(spmidev);

	if (!dev) {
		pr_err("%s: invalid SPMI device\n", __func__);
		return -EINVAL;
	}

	/* Set the device name */
	dev_set_name(dev, "%s-%p", spmidev->name, spmidev);

	/* Device may be bound to an active driver when this returns */
	rc = device_add(dev);

	if (rc < 0)
		dev_err(dev, "Can't add %s, status %d\n", dev_name(dev), rc);
	else
		dev_dbg(dev, "device %s registered\n", dev_name(dev));

	return rc;
}
EXPORT_SYMBOL_GPL(spmi_add_device);

/**
 * spmi_new_device: Instantiates a new SPMI device
 * @ctrl: controller to which this device is to be added to.
 * @info: board information for this device.
 *
 * Returns the new device or NULL.
 */
struct spmi_device *spmi_new_device(struct spmi_controller *ctrl,
					struct spmi_boardinfo const *info)
{
	struct spmi_device *spmidev;
	int rc;

	if (!ctrl || !info)
		return NULL;

	spmidev = spmi_alloc_device(ctrl);
	if (!spmidev)
		return NULL;

	spmidev->name = info->name;
	spmidev->sid  = info->slave_id;
	spmidev->dev.of_node = info->of_node;
	spmidev->dev.platform_data = (void *)info->platform_data;
	spmidev->num_dev_node = info->num_dev_node;
	spmidev->dev_node = info->dev_node;
	spmidev->res = info->res;

	rc = spmi_add_device(spmidev);
	if (rc < 0) {
		spmi_dev_put(spmidev);
		return NULL;
	}

	return spmidev;
}
EXPORT_SYMBOL_GPL(spmi_new_device);

/* spmi_remove_device: Remove the effect of spmi_add_device() */
void spmi_remove_device(struct spmi_device *spmi_dev)
{
	device_unregister(&spmi_dev->dev);
}
EXPORT_SYMBOL_GPL(spmi_remove_device);

static void spmi_match_ctrl_to_boardinfo(struct spmi_controller *ctrl,
				struct spmi_boardinfo *bi)
{
	struct spmi_device *spmidev;

	spmidev = spmi_new_device(ctrl, bi);
	if (!spmidev)
		dev_err(ctrl->dev.parent, "can't create new device for %s\n",
			bi->name);
}

/**
 * spmi_register_board_info: Board-initialization routine.
 * @bus_num: controller number (bus) on which this device will sit.
 * @info: list of all devices on all controllers present on the board.
 * @n: number of entries.
 * API enumerates respective devices on corresponding controller.
 * Called from board-init function.
 * If controller is not present, only add to boards list
 */
int spmi_register_board_info(int busnum,
			struct spmi_boardinfo const *info, unsigned n)
{
	int i;
	struct spmii_boardinfo *bi;
	struct spmi_controller *ctrl;

	bi = kzalloc(n * sizeof(*bi), GFP_KERNEL);
	if (!bi)
		return -ENOMEM;

	ctrl = spmi_busnum_to_ctrl(busnum);

	for (i = 0; i < n; i++, bi++, info++) {

		memcpy(&bi->board_info, info, sizeof(*info));
		mutex_lock(&board_lock);
		list_add_tail(&bi->list, &board_list);

		if (ctrl)
			spmi_match_ctrl_to_boardinfo(ctrl, &bi->board_info);
		mutex_unlock(&board_lock);
	}
	return 0;
}
EXPORT_SYMBOL_GPL(spmi_register_board_info);

/* ------------------------------------------------------------------------- */

static inline int
spmi_cmd(struct spmi_controller *ctrl, u8 opcode, u8 sid)
{
	BUG_ON(!ctrl || !ctrl->cmd);
	return ctrl->cmd(ctrl, opcode, sid);
}

static inline int spmi_read_cmd(struct spmi_controller *ctrl,
				u8 opcode, u8 sid, u16 addr, u8 bc, u8 *buf)
{
	BUG_ON(!ctrl || !ctrl->read_cmd);
	return ctrl->read_cmd(ctrl, opcode, sid, addr, bc, buf);
}

static inline int spmi_write_cmd(struct spmi_controller *ctrl,
				u8 opcode, u8 sid, u16 addr, u8 bc, u8 *buf)
{
	BUG_ON(!ctrl || !ctrl->write_cmd);
	return ctrl->write_cmd(ctrl, opcode, sid, addr, bc, buf);
}

/*
 * register read/write: 5-bit address, 1 byte of data
 * extended register read/write: 8-bit address, up to 16 bytes of data
 * extended register read/write long: 16-bit address, up to 8 bytes of data
 */

/**
 * spmi_register_read() - register read
 * @dev: SPMI device.
 * @sid: slave identifier.
 * @ad: slave register address (5-bit address).
 * @buf: buffer to be populated with data from the Slave.
 *
 * Reads 1 byte of data from a Slave device register.
 */
int spmi_register_read(struct spmi_controller *ctrl, u8 sid, u8 addr, u8 *buf)
{
	/* 4-bit Slave Identifier, 5-bit register address */
	if (sid > SPMI_MAX_SLAVE_ID || addr > 0x1F)
		return -EINVAL;

	return spmi_read_cmd(ctrl, SPMI_CMD_READ, sid, addr, 0, buf);
}
EXPORT_SYMBOL_GPL(spmi_register_read);

/**
 * spmi_ext_register_read() - extended register read
 * @dev: SPMI device.
 * @sid: slave identifier.
 * @ad: slave register address (8-bit address).
 * @len: the request number of bytes to read (up to 16 bytes).
 * @buf: buffer to be populated with data from the Slave.
 *
 * Reads up to 16 bytes of data from the extended register space on a
 * Slave device.
 */
int spmi_ext_register_read(struct spmi_controller *ctrl,
				u8 sid, u8 addr, u8 *buf, int len)
{
	/* 4-bit Slave Identifier, 8-bit register address, up to 16 bytes */
	if (sid > SPMI_MAX_SLAVE_ID || len <= 0 || len > 16)
		return -EINVAL;

	return spmi_read_cmd(ctrl, SPMI_CMD_EXT_READ, sid, addr, len - 1, buf);
}
EXPORT_SYMBOL_GPL(spmi_ext_register_read);

/**
 * spmi_ext_register_readl() - extended register read long
 * @dev: SPMI device.
 * @sid: slave identifier.
 * @ad: slave register address (16-bit address).
 * @len: the request number of bytes to read (up to 8 bytes).
 * @buf: buffer to be populated with data from the Slave.
 *
 * Reads up to 8 bytes of data from the extended register space on a
 * Slave device using 16-bit address.
 */
int spmi_ext_register_readl(struct spmi_controller *ctrl,
				u8 sid, u16 addr, u8 *buf, int len)
{
	/* 4-bit Slave Identifier, 16-bit register address, up to 8 bytes */
	if (sid > SPMI_MAX_SLAVE_ID || len <= 0 || len > 8)
		return -EINVAL;

	return spmi_read_cmd(ctrl, SPMI_CMD_EXT_READL, sid, addr, len - 1, buf);
}
EXPORT_SYMBOL_GPL(spmi_ext_register_readl);

/**
 * spmi_register_write() - register write
 * @dev: SPMI device.
 * @sid: slave identifier.
 * @ad: slave register address (5-bit address).
 * @buf: buffer containing the data to be transferred to the Slave.
 *
 * Writes 1 byte of data to a Slave device register.
 */
int spmi_register_write(struct spmi_controller *ctrl, u8 sid, u8 addr, u8 *buf)
{
	u8 op = SPMI_CMD_WRITE;

	/* 4-bit Slave Identifier, 5-bit register address */
	if (sid > SPMI_MAX_SLAVE_ID || addr > 0x1F)
		return -EINVAL;

	return spmi_write_cmd(ctrl, op, sid, addr, 0, buf);
}
EXPORT_SYMBOL_GPL(spmi_register_write);

/**
 * spmi_register_zero_write() - register zero write
 * @dev: SPMI device.
 * @sid: slave identifier.
 * @data: the data to be written to register 0 (7-bits).
 *
 * Writes data to register 0 of the Slave device.
 */
int spmi_register_zero_write(struct spmi_controller *ctrl, u8 sid, u8 data)
{
	u8 op = SPMI_CMD_ZERO_WRITE;

	/* 4-bit Slave Identifier, 5-bit register address */
	if (sid > SPMI_MAX_SLAVE_ID)
		return -EINVAL;

	return spmi_write_cmd(ctrl, op, sid, 0, 0, &data);
}
EXPORT_SYMBOL_GPL(spmi_register_zero_write);

/**
 * spmi_ext_register_write() - extended register write
 * @dev: SPMI device.
 * @sid: slave identifier.
 * @ad: slave register address (8-bit address).
 * @buf: buffer containing the data to be transferred to the Slave.
 * @len: the request number of bytes to read (up to 16 bytes).
 *
 * Writes up to 16 bytes of data to the extended register space of a
 * Slave device.
 */
int spmi_ext_register_write(struct spmi_controller *ctrl,
				u8 sid, u8 addr, u8 *buf, int len)
{
	u8 op = SPMI_CMD_EXT_WRITE;

	/* 4-bit Slave Identifier, 8-bit register address, up to 16 bytes */
	if (sid > SPMI_MAX_SLAVE_ID || len <= 0 || len > 16)
		return -EINVAL;

	return spmi_write_cmd(ctrl, op, sid, addr, len - 1, buf);
}
EXPORT_SYMBOL_GPL(spmi_ext_register_write);

/**
 * spmi_ext_register_writel() - extended register write long
 * @dev: SPMI device.
 * @sid: slave identifier.
 * @ad: slave register address (16-bit address).
 * @buf: buffer containing the data to be transferred to the Slave.
 * @len: the request number of bytes to read (up to 8 bytes).
 *
 * Writes up to 8 bytes of data to the extended register space of a
 * Slave device using 16-bit address.
 */
int spmi_ext_register_writel(struct spmi_controller *ctrl,
				u8 sid, u16 addr, u8 *buf, int len)
{
	u8 op = SPMI_CMD_EXT_WRITEL;

	/* 4-bit Slave Identifier, 16-bit register address, up to 8 bytes */
	if (sid > SPMI_MAX_SLAVE_ID || len <= 0 || len > 8)
		return -EINVAL;

	return spmi_write_cmd(ctrl, op, sid, addr, len - 1, buf);
}
EXPORT_SYMBOL_GPL(spmi_ext_register_writel);

/**
 * spmi_command_reset() - sends RESET command to the specified slave
 * @dev: SPMI device.
 * @sid: slave identifier.
 *
 * The Reset command initializes the Slave and forces all registers to
 * their reset values. The Slave shall enter the STARTUP state after
 * receiving a Reset command.
 *
 * Returns
 * -EINVAL for invalid Slave Identifier.
 * -EPERM if the SPMI transaction is denied due to permission issues.
 * -EIO if the SPMI transaction fails (parity errors, etc).
 * -ETIMEDOUT if the SPMI transaction times out.
 */
int spmi_command_reset(struct spmi_controller *ctrl, u8 sid)
{
	if (sid > SPMI_MAX_SLAVE_ID)
		return -EINVAL;
	return spmi_cmd(ctrl, SPMI_CMD_RESET, sid);
}
EXPORT_SYMBOL_GPL(spmi_command_reset);

/**
 * spmi_command_sleep() - sends SLEEP command to the specified slave
 * @dev: SPMI device.
 * @sid: slave identifier.
 *
 * The Sleep command causes the Slave to enter the user defined SLEEP state.
 *
 * Returns
 * -EINVAL for invalid Slave Identifier.
 * -EPERM if the SPMI transaction is denied due to permission issues.
 * -EIO if the SPMI transaction fails (parity errors, etc).
 * -ETIMEDOUT if the SPMI transaction times out.
 */
int spmi_command_sleep(struct spmi_controller *ctrl, u8 sid)
{
	if (sid > SPMI_MAX_SLAVE_ID)
		return -EINVAL;
	return spmi_cmd(ctrl, SPMI_CMD_SLEEP, sid);
}
EXPORT_SYMBOL_GPL(spmi_command_sleep);

/**
 * spmi_command_wakeup() - sends WAKEUP command to the specified slave
 * @dev: SPMI device.
 * @sid: slave identifier.
 *
 * The Wakeup command causes the Slave to move from the SLEEP state to
 * the ACTIVE state.
 *
 * Returns
 * -EINVAL for invalid Slave Identifier.
 * -EPERM if the SPMI transaction is denied due to permission issues.
 * -EIO if the SPMI transaction fails (parity errors, etc).
 * -ETIMEDOUT if the SPMI transaction times out.
 */
int spmi_command_wakeup(struct spmi_controller *ctrl, u8 sid)
{
	if (sid > SPMI_MAX_SLAVE_ID)
		return -EINVAL;
	return spmi_cmd(ctrl, SPMI_CMD_WAKEUP, sid);
}
EXPORT_SYMBOL_GPL(spmi_command_wakeup);

/**
 * spmi_command_shutdown() - sends SHUTDOWN command to the specified slave
 * @dev: SPMI device.
 * @sid: slave identifier.
 *
 * The Shutdown command causes the Slave to enter the SHUTDOWN state.
 *
 * Returns
 * -EINVAL for invalid Slave Identifier.
 * -EPERM if the SPMI transaction is denied due to permission issues.
 * -EIO if the SPMI transaction fails (parity errors, etc).
 * -ETIMEDOUT if the SPMI transaction times out.
 */
int spmi_command_shutdown(struct spmi_controller *ctrl, u8 sid)
{
	if (sid > SPMI_MAX_SLAVE_ID)
		return -EINVAL;
	return spmi_cmd(ctrl, SPMI_CMD_SHUTDOWN, sid);
}
EXPORT_SYMBOL_GPL(spmi_command_shutdown);

/* ------------------------------------------------------------------------- */

static const struct spmi_device_id *spmi_match(const struct spmi_device_id *id,
		const struct spmi_device *spmi_dev)
{
	while (id->name[0]) {
		if (strncmp(spmi_dev->name, id->name, SPMI_NAME_SIZE) == 0)
			return id;
		id++;
	}
	return NULL;
}

static int spmi_device_match(struct device *dev, struct device_driver *drv)
{
	struct spmi_device *spmi_dev;
	struct spmi_driver *sdrv = to_spmi_driver(drv);

	if (dev->type == &spmi_dev_type)
		spmi_dev = to_spmi_device(dev);
	else
		return 0;

	/* Attempt an OF style match */
	if (of_driver_match_device(dev, drv))
		return 1;

	if (sdrv->id_table)
		return spmi_match(sdrv->id_table, spmi_dev) != NULL;

	if (drv->name)
		return strncmp(spmi_dev->name, drv->name, SPMI_NAME_SIZE) == 0;
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int spmi_legacy_suspend(struct device *dev, pm_message_t mesg)
{
	struct spmi_device *spmi_dev = NULL;
	struct spmi_driver *driver;
	if (dev->type == &spmi_dev_type)
		spmi_dev = to_spmi_device(dev);

	if (!spmi_dev || !dev->driver)
		return 0;

	driver = to_spmi_driver(dev->driver);
	if (!driver->suspend)
		return 0;

	return driver->suspend(spmi_dev, mesg);
}

static int spmi_legacy_resume(struct device *dev)
{
	struct spmi_device *spmi_dev = NULL;
	struct spmi_driver *driver;
	if (dev->type == &spmi_dev_type)
		spmi_dev = to_spmi_device(dev);

	if (!spmi_dev || !dev->driver)
		return 0;

	driver = to_spmi_driver(dev->driver);
	if (!driver->resume)
		return 0;

	return driver->resume(spmi_dev);
}

static int spmi_pm_suspend(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_suspend(dev);
	else
		return spmi_legacy_suspend(dev, PMSG_SUSPEND);
}

static int spmi_pm_resume(struct device *dev)
{
	const struct dev_pm_ops *pm = dev->driver ? dev->driver->pm : NULL;

	if (pm)
		return pm_generic_resume(dev);
	else
		return spmi_legacy_resume(dev);
}

#else
#define spmi_pm_suspend		NULL
#define spmi_pm_resume		NULL
#endif

static const struct dev_pm_ops spmi_pm_ops = {
	.suspend = spmi_pm_suspend,
	.resume = spmi_pm_resume,
	SET_RUNTIME_PM_OPS(
		pm_generic_suspend,
		pm_generic_resume,
		pm_generic_runtime_idle
		)
};
struct bus_type spmi_bus_type = {
	.name		= "spmi",
	.match		= spmi_device_match,
	.pm		= &spmi_pm_ops,
};
EXPORT_SYMBOL_GPL(spmi_bus_type);

struct device spmi_dev = {
	.init_name = "spmi",
};

static int spmi_drv_probe(struct device *dev)
{
	const struct spmi_driver *sdrv = to_spmi_driver(dev->driver);

	return sdrv->probe(to_spmi_device(dev));
}

static int spmi_drv_remove(struct device *dev)
{
	const struct spmi_driver *sdrv = to_spmi_driver(dev->driver);

	return sdrv->remove(to_spmi_device(dev));
}

static void spmi_drv_shutdown(struct device *dev)
{
	const struct spmi_driver *sdrv = to_spmi_driver(dev->driver);

	sdrv->shutdown(to_spmi_device(dev));
}

/**
 * spmi_driver_register: Client driver registration with SPMI framework.
 * @drv: client driver to be associated with client-device.
 *
 * This API will register the client driver with the SPMI framework.
 * It is called from the driver's module-init function.
 */
int spmi_driver_register(struct spmi_driver *drv)
{
	drv->driver.bus = &spmi_bus_type;

	if (drv->probe)
		drv->driver.probe = spmi_drv_probe;

	if (drv->remove)
		drv->driver.remove = spmi_drv_remove;

	if (drv->shutdown)
		drv->driver.shutdown = spmi_drv_shutdown;

	return driver_register(&drv->driver);
}
EXPORT_SYMBOL_GPL(spmi_driver_register);

static int spmi_register_controller(struct spmi_controller *ctrl)
{
	int ret = 0;

	/* Can't register until after driver model init */
	if (WARN_ON(!spmi_bus_type.p)) {
		ret = -EAGAIN;
		goto exit;
	}

	dev_set_name(&ctrl->dev, "spmi-%d", ctrl->nr);
	ctrl->dev.bus = &spmi_bus_type;
	ctrl->dev.type = &spmi_ctrl_type;
	ret = device_register(&ctrl->dev);
	if (ret)
		goto exit;

	dev_dbg(&ctrl->dev, "Bus spmi-%d registered: dev:%x\n",
					ctrl->nr, (u32)&ctrl->dev);

	spmi_dfs_add_controller(ctrl);
	return 0;

exit:
	mutex_lock(&board_lock);
	idr_remove(&ctrl_idr, ctrl->nr);
	mutex_unlock(&board_lock);
	return ret;
}

static void __exit spmi_exit(void)
{
	device_unregister(&spmi_dev);
	bus_unregister(&spmi_bus_type);
}

static int __init spmi_init(void)
{
	int retval;

	retval = bus_register(&spmi_bus_type);
	if (!retval)
		retval = device_register(&spmi_dev);

	if (retval)
		bus_unregister(&spmi_bus_type);

	return retval;
}
postcore_initcall(spmi_init);
module_exit(spmi_exit);

MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("SPMI module");
MODULE_ALIAS("platform:spmi");
