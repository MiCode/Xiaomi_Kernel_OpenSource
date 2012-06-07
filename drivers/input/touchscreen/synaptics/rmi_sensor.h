/**
 *
 * Synaptics Register Mapped Interface (RMI4) - RMI Sensor Module Header.
 * Copyright (C) 2007 - 2011, Synaptics Incorporated
 *
 */
/*
 *
 * This file is licensed under the GPL2 license.
 *
 *############################################################################
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
 *############################################################################
 */

#include <linux/device.h>

#ifndef _RMI_SENSOR_H
#define _RMI_SENSOR_H

#include <linux/input/rmi_platformdata.h>

struct rmi_sensor_driver {
	struct module *module;
	struct device_driver drv;
	struct rmi_sensor_device *sensor_device;

	/* Attention Function
	 *  This function is called by the low level isr in the physical
	 * driver. It merely schedules work to be done.
	 */
	void (*attention)(struct rmi_phys_driver *physdrvr, int instance);
	/* Probe Function
	 *  This function is called to give the sensor driver layer an
	 *  opportunity to claim an RMI device.  The sensor layer cannot
	 *  read RMI registers at this point since the rmi physical driver
	 *  has not been bound to it yet.  Defer that to the config
	 *  function call which occurs immediately after a successful probe.
	 */
	int (*probe)(struct rmi_sensor_driver *sensor);
	/* Config Function
	 *  This function is called after a successful probe.  It gives the
	 *  sensor driver an opportunity to query and/or configure an RMI
	 *  device before data starts flowing.
	 */
	void (*config)(struct rmi_sensor_driver *sensor);

	/* Functions can call this in order to dispatch IRQs. */
	void (*dispatchIRQs)(struct rmi_sensor_driver *sensor,
		unsigned int irqStatus);

	/* Register Functions
	 *  This function is called in the rmi bus
	 * driver to have the sensor driver scan for any supported
	 * functions on the sensor and add devices for each one.
	 */
	void (*rmi_sensor_register_functions)(struct rmi_sensor_driver
							*sensor);

	unsigned int interruptRegisterCount;

	bool polling_required;

	/* pointer to the corresponding phys driver info for this sensor */
	/* The phys driver has the pointers to read, write, etc. */
	struct rmi_phys_driver *rpd;

	struct hrtimer timer;
	struct work_struct work;
	bool workIsReady;

	/* This list is for keeping around the list of sensors.
	 *  Every time that a physical device is detected by the
	 *  physical layer - be it i2c, spi, or some other - then
	 *  we need to bind the physical layer to the device. When
	 *  the Page Descriptor Table is scanned and when Function $01
	 *  is found then a new sensor device is created. The corresponding
	 *  rmi_phys_driver struct pointer needs to be bound to the new
	 *  sensor since Function $01 will be used to control and get
	 *  interrupt information about the particular data source that is
	 *  doing the interrupt. The rmi_phys_driver contains the pointers
	 *  to the particular read, write, read_multiple, write_multiple
	 *  functions for this device. This rmi_phys_driver struct will
	 *  have to be up-bound to any drivers upstream that need it.
	 */

	/* Standard kernel linked list implementation.
	 *  Documentation on how to use it can be found at
	 *  http://isis.poly.edu/kulesh/stuff/src/klist/.
	 */
	struct list_head sensor_drivers; /* link sensor drivers into list */

	struct list_head functions;     /* List of rmi_function_infos */
	/* Per function initialization data. */
	struct rmi_functiondata_list *perfunctiondata;
};

/* macro to get the pointer to the device_driver struct from the sensor */
#define to_rmi_sensor_driver(drv) container_of(drv, \
		struct rmi_sensor_driver, drv);

struct rmi_sensor_device {
	struct rmi_sensor_driver *driver;
	struct device dev;

	/* Standard kernel linked list implementation.
	 *  Documentation on how to use it can be found at
	 *  http://isis.poly.edu/kulesh/stuff/src/klist/.
	 */
	struct list_head sensors; /* link sensors into list */
};

int rmi_sensor_register_device(struct rmi_sensor_device *dev, int index);
int rmi_sensor_register_driver(struct rmi_sensor_driver *driver);
int rmi_sensor_register_functions(struct rmi_sensor_driver *sensor);
bool rmi_polling_required(struct rmi_sensor_driver *sensor);

static inline void *rmi_sensor_get_functiondata(struct rmi_sensor_driver
		*driver, unsigned char function_index)
{
	int i;
	if (driver->perfunctiondata) {
		for (i = 0; i < driver->perfunctiondata->count; i++) {
			if (driver->perfunctiondata->functiondata[i].
					function_index == function_index)
				return driver->perfunctiondata->
					functiondata[i].data;
		}
	}
	return NULL;
}

#endif
