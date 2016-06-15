/* Copyright (c) 2012-2014, 2016 The Linux Foundation. All rights reserved.
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

#ifndef _LINUX_SPMI_H
#define _LINUX_SPMI_H

#include <linux/types.h>
#include <linux/device.h>
#include <linux/mod_devicetable.h>

/* Maximum slave identifier */
#define SPMI_MAX_SLAVE_ID		16

/* SPMI Commands */
enum spmi_commands {
	SPMI_CMD_EXT_WRITE = 0x00,
	SPMI_CMD_RESET = 0x10,
	SPMI_CMD_SLEEP = 0x11,
	SPMI_CMD_SHUTDOWN = 0x12,
	SPMI_CMD_WAKEUP = 0x13,
	SPMI_CMD_AUTHENTICATE = 0x14,
	SPMI_CMD_MSTR_READ = 0x15,
	SPMI_CMD_MSTR_WRITE = 0x16,
	SPMI_CMD_TRANSFER_BUS_OWNERSHIP = 0x1A,
	SPMI_CMD_DDB_MASTER_READ = 0x1B,
	SPMI_CMD_DDB_SLAVE_READ = 0x1C,
	SPMI_CMD_EXT_READ = 0x20,
	SPMI_CMD_EXT_WRITEL = 0x30,
	SPMI_CMD_EXT_READL = 0x38,
	SPMI_CMD_WRITE = 0x40,
	SPMI_CMD_READ = 0x60,
	SPMI_CMD_ZERO_WRITE = 0x80,
};

struct spmi_device;

/**
 * struct spmi_controller: interface to the SPMI master controller
 * @nr: board-specific number identifier for this controller/bus
 * @name: name for this controller
 * @cmd: sends a non-data command sequence on the SPMI bus.
 * @read_cmd: sends a register read command sequence on the SPMI bus.
 * @write_cmd: sends a register write command sequence on the SPMI bus.
 */
struct spmi_controller {
	struct device		dev;
	unsigned int		nr;
	struct completion	dev_released;
	int		(*cmd)(struct spmi_controller *, u8 opcode, u8 sid);
	int		(*read_cmd)(struct spmi_controller *,
				u8 opcode, u8 sid, u16 addr, u8 bc, u8 *buf);
	int		(*write_cmd)(struct spmi_controller *,
				u8 opcode, u8 sid, u16 addr, u8 bc, u8 *buf);
};
#define to_spmi_controller(d) container_of(d, struct spmi_controller, dev)

/**
 * struct spmi_driver: Manage SPMI generic/slave device driver
 * @probe: binds this driver to a SPMI device.
 * @remove: unbinds this driver from the SPMI device.
 * @shutdown: standard shutdown callback used during powerdown/halt.
 * @suspend: standard suspend callback used during system suspend
 * @resume: standard resume callback used during system resume
 * @driver: SPMI device drivers should initialize name and owner field of
 *	    this structure
 * @id_table: list of SPMI devices supported by this driver
 */
struct spmi_driver {
	int				(*probe)(struct spmi_device *dev);
	int				(*remove)(struct spmi_device *dev);
	void				(*shutdown)(struct spmi_device *dev);
	int				(*suspend)(struct spmi_device *dev,
					pm_message_t pmesg);
	int				(*resume)(struct spmi_device *dev);

	struct device_driver		driver;
	const struct spmi_device_id	*id_table;
};
#define to_spmi_driver(d) container_of(d, struct spmi_driver, driver)

/**
 * struct spmi_resource: spmi_resource for one device_node
 * @num_resources: number of resources for this device node
 * @resources: array of resources for this device_node
 * @of_node: device_node of the resource in question
 * @label: name used to reference the device from the driver
 *
 * Note that we explicitly add a 'label' pointer here since per
 * the ePAPR 2.2.2, the device_node->name should be generic and not
 * reflect precise programming model. Thus label enables a
 * platform specific name to be assigned with the 'label' binding to
 * allow for unique query names.
 */
struct spmi_resource {
	struct resource		*resource;
	u32			num_resources;
	struct device_node	*of_node;
	const char		*label;
};

/**
 * Client/device handle (struct spmi_device):
 * ------------------------------------------
 *  This is the client/device handle returned when a SPMI device
 *  is registered with a controller.
 *  Pointer to this structure is used by client-driver as a handle.
 *  @dev: Driver model representation of the device.
 *  @name: Name of driver to use with this device.
 *  @ctrl: SPMI controller managing the bus hosting this device.
 *  @res: SPMI resource for the primary node
 *  @dev_node: array of SPMI resources when used with spmi-dev-container.
 *  @num_dev_node: number of device_node structures.
 *  @sid: Slave Identifier.
 *  @id: Unique identifier to differentiate from other spmi devices with
 *       possibly same name.
 *
 */
struct spmi_device {
	struct device		dev;
	const char		*name;
	struct spmi_controller	*ctrl;
	struct spmi_resource	res;
	struct spmi_resource	*dev_node;
	u32			num_dev_node;
	u8			sid;
	int			id;
};
#define to_spmi_device(d) container_of(d, struct spmi_device, dev)

/**
 * struct spmi_boardinfo: Declare board info for SPMI device bringup.
 * @name: Name of driver to use with this device.
 * @slave_id: slave identifier.
 * @spmi_device: device to be registered with the SPMI framework.
 * @of_node: pointer to the OpenFirmware device node.
 * @res: SPMI resource for the primary node
 * @dev_node: array of SPMI resources when used with spmi-dev-container.
 * @num_dev_node: number of device_node structures.
 * @platform_data: goes to spmi_device.dev.platform_data
 */
struct spmi_boardinfo {
	char			name[SPMI_NAME_SIZE];
	uint8_t			slave_id;
	struct device_node	*of_node;
	struct spmi_resource	res;
	struct spmi_resource	*dev_node;
	u32			num_dev_node;
	const void		*platform_data;
};

/**
 * spmi_driver_register: Client driver registration with SPMI framework.
 * @drv: client driver to be associated with client-device.
 *
 * This API will register the client driver with the SPMI framework.
 * It is called from the driver's module-init function.
 */
extern int spmi_driver_register(struct spmi_driver *drv);

/**
 * spmi_driver_unregister - reverse effect of spmi_driver_register
 * @sdrv: the driver to unregister
 * Context: can sleep
 */
static inline void spmi_driver_unregister(struct spmi_driver *sdrv)
{
	if (sdrv)
		driver_unregister(&sdrv->driver);
}

/**
 * spmi_add_controller: Controller bring-up.
 * @ctrl: controller to be registered.
 *
 * A controller is registered with the framework using this API. ctrl->nr is the
 * desired number with which SPMI framework registers the controller.
 * Function will return -EBUSY if the number is in use.
 */
extern int spmi_add_controller(struct spmi_controller *ctrl);

/**
 * spmi_del_controller: Controller tear-down.
 * Controller added with the above API is teared down using this API.
 */
extern int spmi_del_controller(struct spmi_controller *ctrl);

/**
 * spmi_busnum_to_ctrl: Map bus number to controller
 * @busnum: bus number
 *
 * Returns controller device representing this bus number
 */
extern struct spmi_controller *spmi_busnum_to_ctrl(u32 bus_num);

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
extern struct spmi_device *spmi_alloc_device(struct spmi_controller *ctrl);

/**
 * spmi_add_device: Add spmi_device allocated with spmi_alloc_device().
 * @spmi_dev: spmi_device to be added (registered).
 */
extern int spmi_add_device(struct spmi_device *spmi_dev);

/**
 * spmi_new_device: Instantiates a new SPMI device
 * @ctrl: controller to which this device is to be added to.
 * @info: board information for this device.
 *
 * Returns the new device or NULL.
 */
extern struct spmi_device *spmi_new_device(struct spmi_controller *ctrl,
					struct spmi_boardinfo const *info);

/* spmi_remove_device: Remove the effect of spmi_add_device() */
extern void spmi_remove_device(struct spmi_device *spmi_dev);

#ifdef CONFIG_SPMI
/**
 * spmi_register_board_info: Board-initialization routine.
 * @bus_num: controller number (bus) on which this device will sit.
 * @info: list of all devices on all controllers present on the board.
 * @n: number of entries.
 *
 * API enumerates respective devices on corresponding controller.
 * Called from board-init function.
 */
extern int spmi_register_board_info(int busnum,
			struct spmi_boardinfo const *info, unsigned n);
#else
static inline int spmi_register_board_info(int busnum,
			struct spmi_boardinfo const *info, unsigned n)
{
	return 0;
}
#endif

static inline void *spmi_get_ctrldata(const struct spmi_controller *ctrl)
{
	return dev_get_drvdata(&ctrl->dev);
}

static inline void spmi_set_ctrldata(struct spmi_controller *ctrl, void *data)
{
	dev_set_drvdata(&ctrl->dev, data);
}

static inline void *spmi_get_devicedata(const struct spmi_device *dev)
{
	return dev_get_drvdata(&dev->dev);
}

static inline void spmi_set_devicedata(struct spmi_device *dev, void *data)
{
	dev_set_drvdata(&dev->dev, data);
}

static inline void spmi_dev_put(struct spmi_device *spmidev)
{
	if (spmidev)
		put_device(&spmidev->dev);
}

/**
 * spmi_register_read() - register read
 * @ctrl: SPMI controller.
 * @sid: slave identifier.
 * @ad: slave register address (5-bit address).
 * @buf: buffer to be populated with data from the Slave.
 *
 * Reads 1 byte of data from a Slave device register.
 */
extern int spmi_register_read(struct spmi_controller *ctrl,
					u8 sid, u8 ad, u8 *buf);

/**
 * spmi_ext_register_read() - extended register read
 * @ctrl: SPMI controller.
 * @sid: slave identifier.
 * @ad: slave register address (8-bit address).
 * @len: the request number of bytes to read (up to 16 bytes).
 * @buf: buffer to be populated with data from the Slave.
 *
 * Reads up to 16 bytes of data from the extended register space on a
 * Slave device.
 */
extern int spmi_ext_register_read(struct spmi_controller *ctrl,
					u8 sid, u8 ad, u8 *buf, int len);

/**
 * spmi_ext_register_readl() - extended register read long
 * @ctrl: SPMI controller.
 * @sid: slave identifier.
 * @ad: slave register address (16-bit address).
 * @len: the request number of bytes to read (up to 8 bytes).
 * @buf: buffer to be populated with data from the Slave.
 *
 * Reads up to 8 bytes of data from the extended register space on a
 * Slave device using 16-bit address.
 */
extern int spmi_ext_register_readl(struct spmi_controller *ctrl,
					u8 sid, u16 ad, u8 *buf, int len);

/**
 * spmi_register_write() - register write
 * @ctrl: SPMI controller.
 * @sid: slave identifier.
 * @ad: slave register address (5-bit address).
 * @buf: buffer containing the data to be transferred to the Slave.
 *
 * Writes 1 byte of data to a Slave device register.
 */
extern int spmi_register_write(struct spmi_controller *ctrl,
					u8 sid, u8 ad, u8 *buf);

/**
 * spmi_register_zero_write() - register zero write
 * @ctrl: SPMI controller.
 * @sid: slave identifier.
 * @data: the data to be written to register 0 (7-bits).
 *
 * Writes data to register 0 of the Slave device.
 */
extern int spmi_register_zero_write(struct spmi_controller *ctrl,
					u8 sid, u8 data);

/**
 * spmi_ext_register_write() - extended register write
 * @ctrl: SPMI controller.
 * @sid: slave identifier.
 * @ad: slave register address (8-bit address).
 * @buf: buffer containing the data to be transferred to the Slave.
 * @len: the request number of bytes to read (up to 16 bytes).
 *
 * Writes up to 16 bytes of data to the extended register space of a
 * Slave device.
 */
extern int spmi_ext_register_write(struct spmi_controller *ctrl,
					u8 sid, u8 ad, u8 *buf, int len);

/**
 * spmi_ext_register_writel() - extended register write long
 * @ctrl: SPMI controller.
 * @sid: slave identifier.
 * @ad: slave register address (16-bit address).
 * @buf: buffer containing the data to be transferred to the Slave.
 * @len: the request number of bytes to read (up to 8 bytes).
 *
 * Writes up to 8 bytes of data to the extended register space of a
 * Slave device using 16-bit address.
 */
extern int spmi_ext_register_writel(struct spmi_controller *ctrl,
					u8 sid, u16 ad, u8 *buf, int len);

/**
 * spmi_command_reset() - sends RESET command to the specified slave
 * @ctrl: SPMI controller.
 * @sid: slave identifier.
 *
 * The Reset command initializes the Slave and forces all registers to
 * their reset values. The Slave shall enter the STARTUP state after
 * receiving a Reset command.
 *
 * Returns
 * -EINVAL for invalid slave identifier.
 * -EPERM if the SPMI transaction is denied due to permission issues.
 * -EIO if the SPMI transaction fails (parity errors, etc).
 * -ETIMEDOUT if the SPMI transaction times out.
 * -EAGAIN if the SPMI transaction is temporarily unavailable
 */
extern int spmi_command_reset(struct spmi_controller *ctrl, u8 sid);

/**
 * spmi_command_sleep() - sends SLEEP command to the specified slave
 * @ctrl: SPMI controller.
 * @sid: slave identifier.
 *
 * The Sleep command causes the Slave to enter the user defined SLEEP state.
 *
 * Returns
 * -EINVAL for invalid slave identifier.
 * -EPERM if the SPMI transaction is denied due to permission issues.
 * -EIO if the SPMI transaction fails (parity errors, etc).
 * -ETIMEDOUT if the SPMI transaction times out.
 * -EAGAIN if the SPMI transaction is temporarily unavailable
 */
extern int spmi_command_sleep(struct spmi_controller *ctrl, u8 sid);

/**
 * spmi_command_wakeup() - sends WAKEUP command to the specified slave
 * @ctrl: SPMI controller.
 * @sid: slave identifier.
 *
 * The Wakeup command causes the Slave to move from the SLEEP state to
 * the ACTIVE state.
 *
 * Returns
 * -EINVAL for invalid slave identifier.
 * -EPERM if the SPMI transaction is denied due to permission issues.
 * -EIO if the SPMI transaction fails (parity errors, etc).
 * -ETIMEDOUT if the SPMI transaction times out.
 * -EAGAIN if the SPMI transaction is temporarily unavailable
 */
extern int spmi_command_wakeup(struct spmi_controller *ctrl, u8 sid);

/**
 * spmi_command_shutdown() - sends SHUTDOWN command to the specified slave
 * @ctrl: SPMI controller.
 * @sid: slave identifier.
 *
 * The Shutdown command causes the Slave to enter the SHUTDOWN state.
 *
 * Returns
 * -EINVAL for invalid slave identifier.
 * -EPERM if the SPMI transaction is denied due to permission issues.
 * -EIO if the SPMI transaction fails (parity errors, etc).
 * -ETIMEDOUT if the SPMI transaction times out.
 * -EAGAIN if the SPMI transaction is temporarily unavailable
 */
extern int spmi_command_shutdown(struct spmi_controller *ctrl, u8 sid);

/**
 * spmi_for_each_container_dev - iterate over the array of devnode resources.
 * @res: spmi_resource pointer used as the array cursor
 * @spmi_dev: spmi_device to iterate
 *
 * Only useable in spmi-dev-container configurations.
 */
#define spmi_for_each_container_dev(res, spmi_dev)			      \
	for (res = ((spmi_dev)->dev_node ? &(spmi_dev)->dev_node[0] : NULL);  \
	     (res - (spmi_dev)->dev_node) < (spmi_dev)->num_dev_node; res++)

extern struct resource *spmi_get_resource(struct spmi_device *dev,
				      struct spmi_resource *node,
				      unsigned int type, unsigned int res_num);

struct resource *spmi_get_resource_byname(struct spmi_device *dev,
					  struct spmi_resource *node,
					  unsigned int type,
					  const char *name);

extern int spmi_get_irq(struct spmi_device *dev, struct spmi_resource *node,
						 unsigned int res_num);

extern int spmi_get_irq_byname(struct spmi_device *dev,
			       struct spmi_resource *node, const char *name);

/**
 * spmi_get_node_name - return device name for spmi node
 * @dev: spmi device handle
 *
 * Get the primary node name of a spmi_device coresponding with
 * with the 'label' binding.
 *
 * Returns NULL if no primary dev name has been assigned to this spmi_device.
 */
static inline const char *spmi_get_primary_dev_name(struct spmi_device *dev)
{
	if (dev->res.label)
		return dev->res.label;
	return NULL;
}

struct spmi_resource *spmi_get_dev_container_byname(struct spmi_device *dev,
						    const char *label);
#endif
