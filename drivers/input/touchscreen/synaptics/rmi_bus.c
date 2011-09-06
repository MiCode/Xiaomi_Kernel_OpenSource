/**
 * Synaptics Register Mapped Interface (RMI4) - RMI Bus Module.
 * Copyright (C) 2007 - 2011, Synaptics Incorporated
 *
 * Impliments "rmi" bus per Documentation/driver-model/bus.txt
 *
 * This protocol is layered as follows.
 *
 *
 *
 *  +-------+ +-------+ +-------+ +--------+
 *  | Fn32  | |   Fn11| |  Fn19 | |  Fn11  |   Devices/Functions
 *  *---|---+ +--|----+ +----|--+ +----|---*   (2D, cap. btns, etc.)
 *      |        |           |         |
 *  +----------------+      +----------------+
 *  | Sensor0        |      |  Sensor1       | Sensors Dev/Drivers
 *  +----------------+      +----------------+ (a sensor has one or
 *          |                      |            more functions)
 *          |                      |
 *  +----------------------------------------+
 *  |                                        |
 *  |                RMI4 Bus                | RMI Bus Layer
 *  |                (this file)             |
 *  *--|-----|------|--------------|---------*
 *     |     |      |              |
 *     |     |      |              |
 *  +-----+-----+-------+--------------------+
 *  | I2C | SPI | SMBus |         etc.       | Physical Layer
 *  +-----+-----+-------+--------------------+
 *
 */
/*
 * This file is licensed under the GPL2 license.
 *
 *#############################################################################
 * GPL
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published
 * by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 *
 *#############################################################################
 */

static const char busname[] = "rmi";

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/input/rmi_platformdata.h>

#include "rmi_drvr.h"
#include "rmi.h"
#include "rmi_bus.h"
#include "rmi_sensor.h"
#include "rmi_function.h"

/* list of physical drivers - i2c, spi, etc. */
static LIST_HEAD(phys_drivers);
static DEFINE_MUTEX(phys_drivers_mutex);

/* list of sensors found on a physical bus (i2c, smi, etc.)*/
static LIST_HEAD(sensor_drivers);
static DEFINE_MUTEX(sensor_drivers_mutex);
static LIST_HEAD(sensor_devices);
static DEFINE_MUTEX(sensor_devices_mutex);

#define PDT_START_SCAN_LOCATION 0x00E9
#define PDT_END_SCAN_LOCATION 0x0005
#define PDT_ENTRY_SIZE 0x0006

/* definitions for rmi bus */
struct device rmi_bus_device;

struct bus_type rmi_bus_type;
EXPORT_SYMBOL(rmi_bus_type);


/*
 * This method is called, perhaps multiple times, whenever a new device or driver
 * is added for this bus. It should return a nonzero value if the given device can be
 * handled by the given driver. This function must be handled at the bus level,
 * because that is where the proper logic exists; the core kernel cannot know how
 * to match devices and drivers for every possible bus type
 * The match function does a comparison between the hardware ID provided by
 * the device itself and the IDs supported by the driver.
 *
 */
static int rmi_bus_match(struct device *dev, struct device_driver *driver)
{
	printk(KERN_DEBUG "%s: Matching %s for rmi bus.\n", __func__, dev->bus->name);
	return !strncmp(dev->bus->name, driver->name, strlen(driver->name));
}

/** Stub for now.
 */
static int rmi_bus_suspend(struct device *dev, pm_message_t state)
{
	printk(KERN_INFO "%s: RMI bus suspending.", __func__);
	return 0;
}

/** Stub for now.
 */
static int rmi_bus_resume(struct device *dev)
{
	printk(KERN_INFO "%s: RMI bus resuming.", __func__);
	return 0;
}

/*
 * This method is called, whenever a new device is added for this bus.
 * It will scan the devices PDT to get the function $01 query, control,
 * command and data regsiters so that it can create a function $01 (sensor)
 * device for the new physical device. It also caches the PDT for later use by
 * other functions that are created for the device. For example, if a function
 * $11 is found it will need the query, control, command and data register
 * addresses for that function. The new function could re-scan the PDT but
 * since it is being done here we can cache it and keep it around.
 *
 * TODO: If the device is reset or some action takes place that would invalidate
 * the PDT - such as a reflash of the firmware - then the device should be re-added
 * to the bus and the PDT re-scanned and cached.
 *
 */
int rmi_register_sensor(struct rmi_phys_driver *rpd, struct rmi_sensordata *sensordata)
{
	int i;
	int pdt_entry_count = 0;
	struct rmi_sensor_device *rmi_sensor_dev;
	struct rmi_function_info *rfi;
	struct rmi_function_descriptor rmi_fd;
	int retval;
	static int index;

	/* Make sure we have a read, write, read_multiple, write_multiple
	function pointers from whatever physical layer the sensor is on.
	*/
	if (!rpd->name) {
		printk(KERN_ERR "%s: Physical driver must specify a name",
			__func__);
		return -EINVAL;
	}
	if (!rpd->write) {
		printk(KERN_ERR
			"%s: Physical driver %s must specify a writer.",
			__func__, rpd->name);
		return -EINVAL;
	}
	if (!rpd->read) {
		printk(KERN_ERR
			"%s: Physical driver %s must specify a reader.",
			__func__, rpd->name);
		return -EINVAL;
	}
	if (!rpd->write_multiple) {
		printk(KERN_ERR "%s: Physical driver %s must specify a "
			"multiple writer.",
			__func__, rpd->name);
		return -EINVAL;
	}
	if (!rpd->read_multiple) {
		printk(KERN_ERR "%s: Physical driver %s must specify a "
			"multiple reader.",
			__func__, rpd->name);
		return -EINVAL;
	}

	/* Get some information from the device */
	printk(KERN_DEBUG "%s: Identifying sensors by presence of F01...", __func__);

	rmi_sensor_dev = NULL;

	/* Scan the page descriptor table until we find F01.  If we find that,
	 * we assume that we can reliably talk to this sensor.
	 */
	for (i = PDT_START_SCAN_LOCATION;	/* Register the rmi sensor driver */
			i >= PDT_END_SCAN_LOCATION;
			i -= PDT_ENTRY_SIZE) {
		retval = rpd->read_multiple(rpd, i, (char *)&rmi_fd,
				sizeof(rmi_fd));
		if (!retval) {
			rfi = NULL;

			if (rmi_fd.functionNum != 0x00 && rmi_fd.functionNum != 0xff) {
				pdt_entry_count++;
				if ((rmi_fd.functionNum & 0xff) == 0x01) {
					printk(KERN_DEBUG "%s: F01 Found - RMI Device Control", __func__);

					/* This appears to be a valid device, so create a sensor
					* device and sensor driver for it. */
					rmi_sensor_dev = kzalloc(sizeof(*rmi_sensor_dev), GFP_KERNEL);
					if (!rmi_sensor_dev) {
						printk(KERN_ERR "%s: Error allocating memory for rmi_sensor_device\n", __func__);
						retval = -ENOMEM;
						goto exit_fail;
					}
					rmi_sensor_dev->dev.bus = &rmi_bus_type;

					retval = rmi_sensor_register_device(rmi_sensor_dev, index++);
					if (retval < 0) {
						printk(KERN_ERR "%s: Error %d registering sensor device.", __func__, retval);
						goto exit_fail;
					}

					rmi_sensor_dev->driver = kzalloc(sizeof(struct rmi_sensor_driver), GFP_KERNEL);
					if (!rmi_sensor_dev->driver) {
						printk(KERN_ERR "%s: Error allocating memory for rmi_sensor_driver\n", __func__);
						retval = -ENOMEM;
						goto exit_fail;
					}
					rmi_sensor_dev->driver->sensor_device = rmi_sensor_dev;
					rmi_sensor_dev->driver->polling_required = rpd->polling_required;
					rmi_sensor_dev->driver->rpd = rpd;
					if (sensordata)
						rmi_sensor_dev->driver->perfunctiondata = sensordata->perfunctiondata;
					INIT_LIST_HEAD(&rmi_sensor_dev->driver->functions);

					retval = rmi_sensor_register_driver(rmi_sensor_dev->driver);
					if (retval < 0) {
						printk(KERN_ERR "%s: Error %d registering sensor driver.", __func__, retval);
						goto exit_fail;
					}

					/* link the attention fn in the rpd to the sensor attn fn */

					rpd->sensor = rmi_sensor_dev->driver;
					rpd->attention = rmi_sensor_dev->driver->attention;

					/* Add it into the list of sensors on the rmi bus */
					mutex_lock(&sensor_devices_mutex);
					list_add_tail(&rmi_sensor_dev->sensors, &sensor_devices);
					mutex_unlock(&sensor_devices_mutex);

					/* All done with this sensor, fall out of PDT scan loop. */
					break;
				} else {
					/* Just print out the function found for now */
					printk(KERN_DEBUG "%s: Found Function %02x - Ignored.\n", __func__, rmi_fd.functionNum & 0xff);
				}
			} else {
				/* A zero or 0xff in the function number
				signals the end of the PDT */
				pr_debug("%s:   Found End of PDT.",
					__func__);
				break;
			}
		} else {
			/* failed to read next PDT entry - end PDT
			scan - this may result in an incomplete set
			of recognized functions - should probably
			return an error but the driver may still be
			viable for diagnostics and debugging so let's
			let it continue. */
			printk(KERN_ERR "%s: Read Error %d when reading next PDT entry - "
				"ending PDT scan.",
				__func__, retval);
			break;
		}
	}

	/* If we actually found a sensor, keep it around. */
	if (rmi_sensor_dev) {
		/* Add physical driver struct to list */
		mutex_lock(&phys_drivers_mutex);
		list_add_tail(&rpd->drivers, &phys_drivers);
		mutex_unlock(&phys_drivers_mutex);
		printk(KERN_DEBUG "%s: Registered sensor drivers.", __func__);
		retval = 0;
	} else {
		printk(KERN_ERR "%s: Failed to find sensor. PDT contained %d entries.", __func__, pdt_entry_count);
		retval = -ENODEV;
	}

exit_fail:
	return retval;
}
EXPORT_SYMBOL(rmi_register_sensor);

int rmi_unregister_sensors(struct rmi_phys_driver *rpd)
{
	if (rpd->sensor) {
		printk(KERN_WARNING "%s: WARNING: unregister of %s while %s still attached.",
			__func__, rpd->name, rpd->sensor->drv.name);
	}

	pr_debug("%s: Unregistering sensor drivers %s\n", __func__, rpd->name);

	/* TODO: We should call sensor_teardown() for each sensor before we get
	 * rid of this list.
	 */

	mutex_lock(&sensor_drivers_mutex);
	list_del(&rpd->sensor->sensor_drivers);
	mutex_unlock(&sensor_drivers_mutex);

	return 0;
}
EXPORT_SYMBOL(rmi_unregister_sensors);


static void rmi_bus_dev_release(struct device *dev)
{
	printk(KERN_DEBUG "rmi bus device release\n");
}


int rmi_register_bus_device(struct device *rmibusdev)
{
	printk(KERN_DEBUG "%s: Registering RMI4 bus device.\n", __func__);

	/* Here, we simply fill in some of the embedded device structure fields
	(which individual drivers should not need to know about), and register
	the device with the driver core. */

	rmibusdev->bus = &rmi_bus_type;
	rmibusdev->parent = &rmi_bus_device;
	rmibusdev->release = rmi_bus_dev_release;
	dev_set_name(rmibusdev, "rmi");

	/* If we wanted to add bus-specific attributes to the device, we could do so here.*/

	return device_register(rmibusdev);
}
EXPORT_SYMBOL(rmi_register_bus_device);

void rmi_unregister_bus_device(struct device *rmibusdev)
{
	printk(KERN_DEBUG "%s: Unregistering bus device.", __func__);

	device_unregister(rmibusdev);
}
EXPORT_SYMBOL(rmi_unregister_bus_device);

static int __init rmi_bus_init(void)
{
	int status;

	status = 0;

	printk(KERN_INFO "%s: RMI Bus Driver Init", __func__);

	/* Register the rmi bus */
	rmi_bus_type.name = busname;
	rmi_bus_type.match = rmi_bus_match;
	rmi_bus_type.suspend = rmi_bus_suspend;
	rmi_bus_type.resume = rmi_bus_resume;
	status = bus_register(&rmi_bus_type);
	if (status < 0) {
		printk(KERN_ERR "%s: Error %d registering the rmi bus.", __func__, status);
		goto err_exit;
	}
	printk(KERN_DEBUG "%s: registered bus.", __func__);

#if 0
	/** This doesn't seem to be required any more.  It worked OK in Froyo,
	 * but breaks in Gingerbread */
	/* Register the rmi bus device - "rmi". There is only one rmi bus device. */
	status = rmi_register_bus_device(&rmi_bus_device);
	if (status < 0) {
		printk(KERN_ERR "%s: Error %d registering rmi bus device.", __func__, status);
		bus_unregister(&rmi_bus_type);
		goto err_exit;
	}
	printk(KERN_DEBUG "%s: Registered bus device.", __func__);
#endif

	return 0;
err_exit:
	return status;
}

static void __exit rmi_bus_exit(void)
{
	printk(KERN_DEBUG "%s: RMI Bus Driver Exit.", __func__);

	/* Unregister the rmi bus device - "rmi". There is only one rmi bus device. */
	rmi_unregister_bus_device(&rmi_bus_device);

	/* Unregister the rmi bus */
	bus_unregister(&rmi_bus_type);
}


module_init(rmi_bus_init);
module_exit(rmi_bus_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("RMI4 Driver");
MODULE_LICENSE("GPL");
