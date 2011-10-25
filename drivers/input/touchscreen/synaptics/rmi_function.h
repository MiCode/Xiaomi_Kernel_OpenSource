/**
 *
 * Synaptics Register Mapped Interface (RMI4) Function Device Header File.
 * Copyright (c) 2007 - 2011, Synaptics Incorporated
 *
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

#ifndef _RMI_FUNCTION_H
#define _RMI_FUNCTION_H

#include <linux/input.h>
#include <linux/device.h>


/* For each function present on the RMI device, there will be a corresponding
 * entry in the functions list of the rmi_sensor_driver structure.  This entry
 * gives information about the number of data sources and the number of data
 * registers associated with the function.
 */
struct rmi_function_info {
	/* The sensor this function belongs to.
	*/
	struct rmi_sensor_driver *sensor;

	/* A device associated with this function.
	*/
	struct rmi_function_device *function_device;

	unsigned char functionNum;

	/* This is the number of data sources associated with the function.*/
	unsigned char numSources;

	/* This is the number of data registers to read.*/
	unsigned char dataRegBlockSize;

	/* This is the interrupt register and mask - needed for enabling the
	*  interrupts and for checking what source had caused the attention line
	* interrupt.
	*/
	unsigned char interruptRegister;
	unsigned char interruptMask;

	/* This is the RMI function descriptor associated with this function.
	*  It contains the Base addresses for the functions query, command,
	*  control, and data registers.
	*/
	struct rmi_function_descriptor funcDescriptor;

	/* pointer to data specific to a functions implementation. */
	void *fndata;

	/* A list of the function information.
	*  This list uses the standard kernel linked list implementation.
	*  Documentation on on how to use it can be found at
	*  http://isis.poly.edu/kulesh/stuff/src/klist/.
	*/
	struct list_head link;
};


/* This struct is for creating a list of RMI4 functions that have data sources
associated with them. This is to facilitate adding new support for other
data sources besides 2D sensors.
To add a new data source support, the developer will create a new file
and add these 4 functions below with FN$## in front of the names - where
## is the hex number for the function taken from the RMI4 specification.

The function number will be associated with this and later will be used to
match the RMI4 function to the 4 functions for that RMI4 function number.
The user will also have to add code that adds the new rmi_functions item
to the global list of RMI4 functions and stores the pointers to the 4
functions in the function pointers.
 */
struct rmi_functions {
	unsigned char functionNum;

	/* Pointers to function specific functions for interruptHandler, config, init
	, detect and attention. */
	/* These ptrs. need to be filled in for every RMI4 function that has
	data source(s) associated with it - like fn $11 (2D sensors),
	fn $19 (buttons), etc. Each RMI4 function that has data sources
	will be added into a list that is used to match the function
	number against the number stored here.
	*/
	/* The sensor implementation will call this whenever and IRQ is
	* dispatched that this function is interested in.
	*/
	void (*inthandler)(struct rmi_function_info *rfi, unsigned int assertedIRQs);

	int (*config)(struct rmi_function_info *rmifninfo);
	int (*init)(struct rmi_function_device *function_device);
	int (*detect)(struct rmi_function_info *rmifninfo,
		struct rmi_function_descriptor *fndescr,
		unsigned int interruptCount);
	/** If this is non-null, the sensor implemenation will call this
	* whenever the ATTN line is asserted.
	*/
	void (*attention)(struct rmi_function_info *rmifninfo);


	/* Standard kernel linked list implementation.
	* Documentation on how to use it can be found at
	* http://isis.poly.edu/kulesh/stuff/src/klist/.
	*/
	struct list_head link;
};


typedef void(*inthandlerFuncPtr)(struct rmi_function_info *rfi, unsigned int assertedIRQs);
typedef int(*configFuncPtr)(struct rmi_function_info *rmifninfo);
typedef int(*initFuncPtr)(struct rmi_function_device *function_device);
typedef int(*detectFuncPtr)(struct rmi_function_info *rmifninfo,
		struct rmi_function_descriptor *fndescr,
		unsigned int interruptCount);
typedef	void (*attnFuncPtr)(struct rmi_function_info *rmifninfo);

struct rmi_functions_data {
	int functionNumber;
	inthandlerFuncPtr inthandlerFn;
	configFuncPtr configFn;
	initFuncPtr initFn;
	detectFuncPtr detectFn;
	attnFuncPtr attnFn;
};


struct rmi_functions *rmi_find_function(int functionNum);
int rmi_functions_init(struct input_dev *inputdev);

struct rmi_function_driver {
	struct module *module;
	struct device_driver drv;

	/* Probe Function
	*  This function is called to give the function driver layer an
	*  opportunity to claim an RMI function.
	*/
	int (*probe)(struct rmi_function_driver *function);
	/* Config Function
	*  This function is called after a successful probe.  It gives the
	*  function driver an opportunity to query and/or configure an RMI
	*  function before data starts flowing.
	*/
	void (*config)(struct rmi_function_driver *function);

	unsigned short functionQueryBaseAddr; /* RMI4 function control */
	unsigned short functionControlBaseAddr;
	unsigned short functionCommandBaseAddr;
	unsigned short functionDataBaseAddr;
	unsigned int interruptRegisterOffset; /* offset from start of interrupt registers */
	unsigned int interruptMask;

	/* pointer to the corresponding phys driver info for this sensor */
	/* The phys driver has the pointers to read, write, etc. */
	/* Probably don't need it here - used down in bus driver and sensor driver */
	struct rmi_phys_driver *rpd;

	/* Standard kernel linked list implementation.
	*  Documentation on how to use it can be found at
	*  http://isis.poly.edu/kulesh/stuff/src/klist/.
	*/
	struct list_head function_drivers; /* link function drivers into list */
};

struct rmi_function_device {
	struct rmi_function_driver *function;
	struct device dev;
	struct input_dev *input;
	struct rmi_sensor_driver *sensor; /* need this to be bound to phys driver layer */

	/* the function ptrs to the config, init, detect and
	report fns for this rmi function device. */
	struct rmi_functions *rmi_funcs;
	struct rmi_function_info *rfi;

	/** An RMI sensor might actually have several IRQ registers -
	* this tells us which IRQ register this function is interested in.
	*/
	unsigned int irqRegisterSet;

	/** This is a mask of the IRQs the function is interested in.
	*/
	unsigned int irqMask;

	/* Standard kernel linked list implementation.
	*  Documentation on how to use it can be found at
	*  http://isis.poly.edu/kulesh/stuff/src/klist/.
	*/
	struct list_head functions; /* link functions into list */
};

int rmi_function_register_device(struct rmi_function_device *dev, int fnNumber);

#endif
