/**
 * Synaptics Register Mapped Interface (RMI4) - RMI Function Module.
 * Copyright (C) 2007 - 2011, Synaptics Incorporated
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

static const char functionname[10] = "fn";

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/hrtimer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>

#include "rmi_drvr.h"
#include "rmi_function.h"
#include "rmi_bus.h"
#include "rmi_sensor.h"
#include "rmi_f01.h"
#include "rmi_f05.h"
#include "rmi_f11.h"
#include "rmi_f19.h"
#include "rmi_f34.h"

/* Each time a new RMI4 function support is added the developer needs to
bump the number of supported functions and add the info for
that RMI4 function to the array along with pointers to the report,
config, init and detect functions that they coded in rmi_fxx.c
and rmi_fxx.h - where xx is the RMI4 function number in hex for the new
RMI4 data source function. The information for the RMI4 functions is
obtained from the RMI4 specification document.
 */
#define rmi4_num_supported_data_src_fns 5

/* supported RMI4 functions list - controls what we
 * will provide support for - if it's not in the list then
 * the developer needs to add support functions for it.*/
static LIST_HEAD(fns_list);
static DEFINE_MUTEX(fns_mutex);

/* NOTE: Developer - add in any new RMI4 fn data info - function number
 * and ptrs to report, config, init and detect functions.  This data is
 * used to point to the functions that need to be called to config, init,
 * detect and report data for the new RMI4 function. Refer to the RMI4
 * specification for information on RMI4 functions.
 */
/* TODO: This will eventually go away, and each function will be an independent
 * module. */
static struct rmi_functions_data
	rmi4_supported_data_src_functions[rmi4_num_supported_data_src_fns] = {
	/* Fn $11 - 2D sensing */
	{.functionNumber = 0x11, .inthandlerFn = FN_11_inthandler, .configFn = FN_11_config, .initFn = FN_11_init, .detectFn = FN_11_detect, .attnFn = NULL},
	/* Fn $01 - device control */
	{.functionNumber = 0x01, .inthandlerFn = FN_01_inthandler, .configFn = FN_01_config, .initFn = FN_01_init, .detectFn = FN_01_detect, .attnFn = FN_01_attention},
	/* Fn $05 - analog report */
	{.functionNumber = 0x05, .inthandlerFn = FN_05_inthandler, .configFn = FN_05_config, .initFn = FN_05_init, .detectFn = FN_05_detect, .attnFn = NULL},
	/* Fn $19 - buttons */
	{.functionNumber = 0x19, .inthandlerFn = FN_19_inthandler, .configFn = FN_19_config, .initFn = FN_19_init, .detectFn = FN_19_detect, .attnFn = NULL},
	/* Fn $34 - firmware reflash */
	{.functionNumber = 0x34, .inthandlerFn = FN_34_inthandler, .configFn = FN_34_config, .initFn = FN_34_init, .detectFn = FN_34_detect, .attnFn = FN_34_attention},
};


/* This function is here to provide a way for external modules to access the
 * functions list.  It will try to find a matching function base on the passed
 * in RMI4 function number and return  the pointer to the struct rmi_functions
 * if a match is found or NULL if not found.
 */
struct rmi_functions *rmi_find_function(int functionNum)
{
	struct rmi_functions *fn;
	bool found = false;

	list_for_each_entry(fn, &fns_list, link) {
		if (functionNum == fn->functionNum) {
			found = true;
			break;
		}
	}

	if (!found)
		return NULL;
	else
		return fn;
}
EXPORT_SYMBOL(rmi_find_function);


static void rmi_function_config(struct rmi_function_device *function)
{
	printk(KERN_DEBUG "%s: rmi_function_config", __func__);

}

#if 0 /* This may not be needed anymore. */
/**
 * This is the probe function passed to the RMI4 subsystem that gives us a
 * chance to recognize an RMI4 function.
 */
static int rmi_function_probe(struct rmi_function_driver *function)
{
	struct rmi_phys_driver *rpd;

	rpd = function->rpd;

	if (!rpd) {
		printk(KERN_ERR "%s: Invalid rmi physical driver - null ptr.", __func__);
		return 0;
	}

	return 1;
}
#endif

/** Just a stub for now.
 */
static int rmi_function_suspend(struct device *dev, pm_message_t state)
{
	printk(KERN_INFO "%s: function suspend called.", __func__);
	return 0;
}

/** Just a stub for now.
 */
static int rmi_function_resume(struct device *dev)
{
	printk(KERN_INFO "%s: function resume called.", __func__);
	return 0;
}

int rmi_function_register_driver(struct rmi_function_driver *drv, int fnNumber)
{
	int retval;
	char *drvrname;

	printk(KERN_INFO "%s: Registering function driver for F%02x.\n", __func__, fnNumber);

	retval = 0;

	/* Create a function device and function driver for this Fn */
	drvrname = kzalloc(sizeof(functionname) + 4, GFP_KERNEL);
	if (!drvrname) {
		printk(KERN_ERR "%s: Error allocating memeory for rmi_function_driver name.\n", __func__);
		return -ENOMEM;
	}
	sprintf(drvrname, "fn%02x", fnNumber);

	drv->drv.name = drvrname;
	drv->module = drv->drv.owner;

	drv->drv.suspend = rmi_function_suspend;
	drv->drv.resume = rmi_function_resume;

	/* register the sensor driver */
	retval = driver_register(&drv->drv);
	if (retval) {
		printk(KERN_ERR "%s: Failed driver_register %d\n",
			__func__, retval);
	}

	return retval;
}
EXPORT_SYMBOL(rmi_function_register_driver);

void rmi_function_unregister_driver(struct rmi_function_driver *drv)
{
	printk(KERN_INFO "%s: Unregistering function driver.\n", __func__);

	driver_unregister(&drv->drv);
}
EXPORT_SYMBOL(rmi_function_unregister_driver);

int rmi_function_register_device(struct rmi_function_device *function_device, int fnNumber)
{
	struct input_dev *input;
	int retval;

	printk(KERN_INFO "%s: Registering function device for F%02x.\n", __func__, fnNumber);

	retval = 0;

	/* make name - fn11, fn19, etc. */
	dev_set_name(&function_device->dev, "%sfn%02x", function_device->sensor->drv.name, fnNumber);
	dev_set_drvdata(&function_device->dev, function_device);
	retval = device_register(&function_device->dev);
	if (retval) {
		printk(KERN_ERR "%s:  Failed device_register for function device.\n",
			__func__);
		return retval;
	}

	input = input_allocate_device();
	if (input == NULL) {
		printk(KERN_ERR "%s:  Failed to allocate memory for a "
			"new input device.\n",
			__func__);
		return -ENOMEM;
	}

	input->name = dev_name(&function_device->dev);
	input->phys = "rmi_function";
	function_device->input = input;


	/* init any input specific params for this function */
	function_device->rmi_funcs->init(function_device);

	retval = input_register_device(input);

	if (retval) {
		printk(KERN_ERR "%s:  Failed input_register_device.\n",
			__func__);
		return retval;
	}


	rmi_function_config(function_device);

	return retval;
}
EXPORT_SYMBOL(rmi_function_register_device);

void rmi_function_unregister_device(struct rmi_function_device *dev)
{
	printk(KERN_INFO "%s: Unregistering function device.n", __func__);

	input_unregister_device(dev->input);
	device_unregister(&dev->dev);
}
EXPORT_SYMBOL(rmi_function_unregister_device);

static int __init rmi_function_init(void)
{
	struct rmi_functions_data *rmi4_fn;
	int i;

	printk(KERN_DEBUG "%s: RMI Function Init\n", __func__);

	/* Initialize global list of RMI4 Functions.
	We need to add the supported RMI4 funcions so that we will have
	pointers to the associated functions for init, config, report and
	detect. See rmi.h for more details. The developer will add a new
	RMI4 function number in the array in rmi_drvr.h, then add a new file to
	the build (called rmi_fXX.c where XX is the hex number for
	the added RMI4 function). The rest should be automatic.
	*/

	/* for each function number defined in rmi.h creat a new rmi_function
	struct and initialize the pointers to the servicing functions and then
	add it into the global list for function support.
	*/
	for (i = 0; i < rmi4_num_supported_data_src_fns; i++) {
		/* Add new rmi4 function struct to list */
		struct rmi_functions *fn = kzalloc(sizeof(*fn), GFP_KERNEL);
		if (!fn) {
			printk(KERN_ERR "%s: could not allocate memory "
				"for rmi_function struct for function 0x%x\n",
				__func__,
				rmi4_supported_data_src_functions[i].functionNumber);
			return -ENOMEM;
		} else {

			rmi4_fn = &rmi4_supported_data_src_functions[i];
			fn->functionNum = rmi4_fn->functionNumber;
			/* Fill in ptrs to functions. The functions are
			linked in from a file called rmi_fxx.c
			where xx is the hex number of the RMI4 function
			from the RMI4 spec. Also, the function prototypes
			need to be added to rmi_fxx.h - also where
			xx is the hex number of the RMI4 function.  So
			that you don't get compile errors and that new
			header needs to be included in the rmi_function.h
			*/
			fn->inthandler = rmi4_fn->inthandlerFn;
			fn->config = rmi4_fn->configFn;
			fn->init =   rmi4_fn->initFn;
			fn->detect = rmi4_fn->detectFn;
			fn->attention = rmi4_fn->attnFn;

			/* Add the new fn to the global list */
			mutex_lock(&fns_mutex);
			list_add_tail(&fn->link, &fns_list);
			mutex_unlock(&fns_mutex);
		}
	}

	return 0;
}

static void __exit rmi_function_exit(void)
{
	printk(KERN_DEBUG "%s: RMI Function Exit\n", __func__);
}


module_init(rmi_function_init);
module_exit(rmi_function_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("RMI4 Function Driver");
MODULE_LICENSE("GPL");
