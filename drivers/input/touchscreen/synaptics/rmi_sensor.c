/**
 * Synaptics Register Mapped Interface (RMI4) - RMI Sensor Module.
 * Copyright (C) 2007 - 2011, Synaptics Incorporated
 *
 */
/*
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

static const char sensorname[] = "sensor";

#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/list.h>
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
#include "rmi_bus.h"
#include "rmi_function.h"
#include "rmi_sensor.h"

long polltime = 25000000;   /* Shared with rmi_function.c. */
EXPORT_SYMBOL(polltime);
module_param(polltime, long, 0644);
MODULE_PARM_DESC(polltime, "How long to wait between polls (in nano seconds).");


#define PDT_START_SCAN_LOCATION 0x00E9
#define PDT_END_SCAN_LOCATION 0x0005
#define PDT_ENTRY_SIZE 0x0006

static DEFINE_MUTEX(rfi_mutex);

struct rmi_functions *rmi_find_function(int functionNum);

int rmi_read(struct rmi_sensor_driver *sensor, unsigned short address,
		char *dest)
{
	struct rmi_phys_driver *rpd = sensor->rpd;
	if (!rpd)
		return -ENODEV;
	return rpd->read(rpd, address, dest);
}
EXPORT_SYMBOL(rmi_read);

int rmi_write(struct rmi_sensor_driver *sensor, unsigned short address,
		unsigned char data)
{
	struct rmi_phys_driver *rpd = sensor->rpd;
	if (!rpd)
		return -ENODEV;
	return rpd->write(rpd, address, data);
}
EXPORT_SYMBOL(rmi_write);

int rmi_read_multiple(struct rmi_sensor_driver *sensor,
		unsigned short address,	char *dest, int length)
{
	struct rmi_phys_driver *rpd = sensor->rpd;
	if (!rpd)
		return -ENODEV;
	return rpd->read_multiple(rpd, address, dest, length);
}
EXPORT_SYMBOL(rmi_read_multiple);

int rmi_write_multiple(struct rmi_sensor_driver *sensor,
		unsigned short address,	unsigned char *data, int length)
{
	struct rmi_phys_driver *rpd = sensor->rpd;
	if (!rpd)
		return -ENODEV;
	return rpd->write_multiple(rpd, address, data, length);
}
EXPORT_SYMBOL(rmi_write_multiple);

/* Utility routine to set bits in a register. */
int rmi_set_bits(struct rmi_sensor_driver *sensor, unsigned short address,
		unsigned char bits)
{
	unsigned char reg_contents;
	int retval;

	retval = rmi_read(sensor, address, &reg_contents);
	if (retval)
		return retval;
	reg_contents = reg_contents | bits;
	retval = rmi_write(sensor, address, reg_contents);
	if (retval == 1)
		return 0;
	else if (retval == 0)
		return -EINVAL;	/* TODO: What should this be? */
	else
		return retval;
}
EXPORT_SYMBOL(rmi_set_bits);

/* Utility routine to clear bits in a register. */
int rmi_clear_bits(struct rmi_sensor_driver *sensor,
		unsigned short address, unsigned char bits)
{
	unsigned char reg_contents;
	int retval;

	retval = rmi_read(sensor, address, &reg_contents);
	if (retval)
		return retval;
	reg_contents = reg_contents & ~bits;
	retval = rmi_write(sensor, address, reg_contents);
	if (retval == 1)
		return 0;
	else if (retval == 0)
		return -EINVAL;	/* TODO: What should this be? */
	else
		return retval;
}
EXPORT_SYMBOL(rmi_clear_bits);

/* Utility routine to set the value of a bit field in a register. */
int rmi_set_bit_field(struct rmi_sensor_driver *sensor,
	unsigned short address, unsigned char field_mask, unsigned char bits)
{
	unsigned char reg_contents;
	int retval;

	retval = rmi_read(sensor, address, &reg_contents);
	if (retval)
		return retval;
	reg_contents = (reg_contents & ~field_mask) | bits;
	retval = rmi_write(sensor, address, reg_contents);
	if (retval == 1)
		return 0;
	else if (retval == 0)
		return -EINVAL;	/* TODO: What should this be? */
	else
		return retval;
}
EXPORT_SYMBOL(rmi_set_bit_field);

bool rmi_polling_required(struct rmi_sensor_driver *sensor)
{
	return sensor->polling_required;
}
EXPORT_SYMBOL(rmi_polling_required);

/** Functions can call this in order to dispatch IRQs. */
void dispatchIRQs(struct rmi_sensor_driver *sensor, unsigned int irqStatus)
{
	struct rmi_function_info *functionInfo;

	list_for_each_entry(functionInfo, &sensor->functions, link) {
		if ((functionInfo->interruptMask & irqStatus)) {
			if (functionInfo->function_device->
					rmi_funcs->inthandler) {
			/* Call the functions interrupt handler function. */
				functionInfo->function_device->rmi_funcs->
				inthandler(functionInfo,
				(functionInfo->interruptMask & irqStatus));
			}
		}
	}
}

/**
 * This is the function we pass to the RMI4 subsystem so we can be notified
 * when attention is required.  It may be called in interrupt context.
 */
static void attention(struct rmi_phys_driver *physdrvr, int instance)
{
	/* All we have to do is schedule work. */

	/* TODO: It's possible that workIsReady is not really needed anymore.
	 * Investigate this to see if the race condition between setting up
	 * the work and enabling the interrupt still exists.
	 */
	if (physdrvr->sensor->workIsReady) {
		schedule_work(&(physdrvr->sensor->work));
	} else {
		/* Got an interrupt but we're not ready so enable the irq
		 * so it doesn't get hung up
		 */
		printk(KERN_DEBUG "%s: Work not initialized yet -"
				"enabling irqs.\n", __func__);
		enable_irq(physdrvr->irq);
	}
}

/**
 * This notifies any interested functions that there
 * is an Attention interrupt.  The interested functions should take
 * appropriate
 * actions (such as reading the interrupt status register and dispatching any
 * appropriate RMI4 interrupts).
 */
void attn_notify(struct rmi_sensor_driver *sensor)
{
	struct rmi_function_info *functionInfo;

	/* check each function that has data sources and if the interrupt for
	 * that triggered then call that RMI4 functions report() function to
	 * gather data and report it to the input subsystem
	 */
	list_for_each_entry(functionInfo, &sensor->functions, link) {
		if (functionInfo->function_device &&
			functionInfo->function_device->rmi_funcs->attention)
			functionInfo->function_device->
				rmi_funcs->attention(functionInfo);
	}
}

/* This is the worker function - for now it simply has to call attn_notify.
 * This work should be scheduled whenever an ATTN interrupt is asserted by
 * the touch sensor.
 * We then call attn_notify to dispatch notification of the ATTN interrupt
 * to all
 * interested functions. After all the attention handling functions
 * have returned, it is presumed safe to re-enable the Attention interrupt.
 */
static void sensor_work_func(struct work_struct *work)
{
	struct rmi_sensor_driver *sensor = container_of(work,
			struct rmi_sensor_driver, work);

	attn_notify(sensor);

	/* we only need to enable the irq if doing interrupts */
	if (!rmi_polling_required(sensor))
		enable_irq(sensor->rpd->irq);
}

/* This is the timer function for polling - it simply has to schedule work
 * and restart the timer. */
static enum hrtimer_restart sensor_poll_timer_func(struct hrtimer *timer)
{
	struct rmi_sensor_driver *sensor = container_of(timer,
			struct rmi_sensor_driver, timer);

	schedule_work(&sensor->work);
	hrtimer_start(&sensor->timer, ktime_set(0, polltime),
			HRTIMER_MODE_REL);
	return HRTIMER_NORESTART;
}

/* This is the probe function passed to the RMI4 subsystem that gives us a
 * chance to recognize an RMI4 device.  In this case, we're looking for
 * Synaptics devices that have data sources - such as touch screens, buttons,
 * etc.
 *
 * TODO: Well, it used to do this.  I'm not sure it's required any more.
 */
static int probe(struct rmi_sensor_driver *sensor)
{
	struct rmi_phys_driver *rpd;

	rpd = sensor->rpd;

	if (!rpd) {
		printk(KERN_ERR "%s: Invalid rmi physical driver - null ptr:"
				"%p\n", __func__, rpd);
		return 0;
	}

	return 1;
}

static void config(struct rmi_sensor_driver *sensor)
{
	/* For each data source we had detected print info and set up interrupts
	or polling. */
	struct rmi_function_info *functionInfo;
	struct rmi_phys_driver *rpd;

	rpd = sensor->rpd; /* get ptr to rmi_physical_driver from app */

	list_for_each_entry(functionInfo, &sensor->functions, link) {
		/* Get and print some info about the data sources... */
		struct rmi_functions *fn;
		bool found = false;
		/* check if function number matches - if so call that
		config function */
		fn = rmi_find_function(functionInfo->functionNum);
		if (fn) {
			found = true;

			if (fn->config) {
				fn->config(functionInfo);
			} else {
				/* the developer did not add in the
				pointer to the config function into
				rmi4_supported_data_src_functions */
				printk(KERN_ERR
					"%s: no config function for "
					"function 0x%x\n",
					__func__, functionInfo->functionNum);
				break;
			}
		}

		if (!found) {
			/* if no support found for this RMI4 function
			it means the developer did not add the
			appropriate function pointer list into the
			rmi4_supported_data_src_functions array and/or
			did not bump up the number of supported RMI4
			functions in rmi.h as required */
			printk(KERN_ERR "%s: could not find support "
				"for function 0x%x\n",
				__func__, functionInfo->functionNum);
		}
	}

	/* This will handle interrupts on the ATTN line (interrupt driven)
	* or will be called every poll interval (when we're not interrupt
	* driven).
	*/
	INIT_WORK(&sensor->work, sensor_work_func);
	sensor->workIsReady = true;

	if (rmi_polling_required(sensor)) {
		/* We're polling driven, so set up the polling timer
		and timer function. */
		hrtimer_init(&sensor->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
		sensor->timer.function = sensor_poll_timer_func;
		hrtimer_start(&sensor->timer, ktime_set(1, 0), HRTIMER_MODE_REL);
	}
}

/** Just a stub for now.
 */
static int rmi_sensor_suspend(struct device *dev, pm_message_t state)
{
	printk(KERN_INFO "%s: sensor suspend called.", __func__);
	return 0;
}

/** Just a stub for now.
 */
static int rmi_sensor_resume(struct device *dev)
{
	printk(KERN_INFO "%s: sensor resume called.", __func__);
	return 0;
}

/*
 * This method is called, whenever a new sensor device is added for the rmi
 * bus.
 *
 * It will scan the devices PDT to determine the supported functions
 * and create a new function device for each of these. It will read
 * the query, control, command and data regsiters for the function
 * to be used for each newly created function device.
 *
 * The sensor device is then bound to every function it supports.
 *
 */
int rmi_sensor_register_functions(struct rmi_sensor_driver *sensor)
{
	struct rmi_function_device *function;
	unsigned int interruptRegisterCount;
	struct rmi_phys_driver *rpd;
	int i;
	unsigned char interruptCount;
	struct rmi_function_info *functionInfo;
	struct rmi_function_descriptor rmi_fd;
	struct rmi_functions *fn;
	int retval;

	pr_debug("%s: Registering sensor functions\n", __func__);

	retval = 0;

	/* Scan device for functions that may be supported */
	{
		pr_debug("%s: Scanning sensor for Functions:\n", __func__);

		interruptCount = 0;
		rpd = sensor->rpd;

		/* Read the Page Descriptor Table to determine what functions
		 * are present */

		printk(KERN_DEBUG "%s: Scanning page descriptors.", __func__);
		for (i = PDT_START_SCAN_LOCATION;
				i >= PDT_END_SCAN_LOCATION;
				i -= PDT_ENTRY_SIZE) {
			printk(KERN_DEBUG "%s: Reading page descriptor 0x%02x", __func__, i);
			retval = rpd->read_multiple(rpd, i, (char *)&rmi_fd,
					sizeof(rmi_fd));
			if (!retval) {
				functionInfo = NULL;

				if (rmi_fd.functionNum != 0x00 && rmi_fd.functionNum != 0xff) {
					printk(KERN_DEBUG "%s: F%02x - queries %02x commands %02x control %02x data %02x ints %02x", __func__, rmi_fd.functionNum, rmi_fd.queryBaseAddr, rmi_fd.commandBaseAddr, rmi_fd.controlBaseAddr, rmi_fd.dataBaseAddr, rmi_fd.interruptSrcCnt);

					if ((rmi_fd.functionNum & 0xff) == 0x01)
						printk(KERN_DEBUG "%s:   Fn $01 Found - RMI Device Control", __func__);

					/* determine if the function is supported and if so
					 * then bind this function device to the sensor */
					if (rmi_fd.interruptSrcCnt) {
						functionInfo = kzalloc(sizeof(*functionInfo), GFP_KERNEL);
						if (!functionInfo) {
							printk(KERN_ERR "%s: could not allocate memory for function 0x%x.",
								__func__, rmi_fd.functionNum);
							retval = -ENOMEM;
							goto exit_fail;
						}
						functionInfo->sensor = sensor;
						functionInfo->functionNum = (rmi_fd.functionNum & 0xff);
						INIT_LIST_HEAD(&functionInfo->link);
						/* Get the ptr to the detect function based on
						 * the function number */
						printk(KERN_DEBUG "%s: Checking for RMI function F%02x.", __func__, rmi_fd.functionNum);
						fn = rmi_find_function(rmi_fd.functionNum);
						if (fn) {
							retval = fn->detect(functionInfo, &rmi_fd,
								interruptCount);
							if (retval)
								printk(KERN_ERR "%s: Function detect for F%02x failed with %d.",
									   __func__, rmi_fd.functionNum, retval);

							/* Create a function device and function driver for this Fn */
							function = kzalloc(sizeof(*function), GFP_KERNEL);
							if (!function) {
								printk(KERN_ERR "%s: Error allocating memory for rmi_function_device.", __func__);
								return -ENOMEM;
							}

							function->dev.parent = &sensor->sensor_device->dev;
							function->dev.bus = sensor->sensor_device->dev.bus;
							function->rmi_funcs = fn;
							function->sensor = sensor;
							function->rfi = functionInfo;
							functionInfo->function_device = function;

							/* Check if we have an interrupt mask of 0 and a non-NULL interrupt
							handler function and print a debug message since we should never
							have this.
							*/
							if (functionInfo->interruptMask == 0 && fn->inthandler != NULL) {
								printk(KERN_DEBUG "%s: Can't have a zero interrupt mask for function F%02x (which requires an interrupt handler).\n",
									__func__, rmi_fd.functionNum);
							}


							/* Check if we have a non-zero interrupt mask and a NULL interrupt
							handler function and print a debug message since we should never
							have this.
							*/
							if (functionInfo->interruptMask != 0 && fn->inthandler == NULL) {
								printk(KERN_DEBUG "%s: Can't have a non-zero interrupt mask %d for function F%02x with a NULL inthandler fn.\n",
									__func__, functionInfo->interruptMask, rmi_fd.functionNum);
							}

							/* Register the rmi function device */
							retval = rmi_function_register_device(function, rmi_fd.functionNum);
							if (retval) {
								printk(KERN_ERR "%s:  Failed rmi_function_register_device.\n",
									__func__);
								return retval;
							}
						} else {
							printk(KERN_ERR "%s: could not find support for function 0x%02X.\n",
								__func__, rmi_fd.functionNum);
						}
					} else {
						printk(KERN_DEBUG "%s: Found function F%02x - Ignored.\n", __func__, rmi_fd.functionNum & 0xff);
					}

					/* bump interrupt count for next iteration */
					/* NOTE: The value 7 is reserved - for now, only bump up one for an interrupt count of 7 */
					if ((rmi_fd.interruptSrcCnt & 0x7) == 0x7) {
						interruptCount += 1;
					} else {
						interruptCount +=
							(rmi_fd.interruptSrcCnt & 0x7);
					}

					/* link this function info to the RMI module infos list
					of functions */
					if (functionInfo == NULL) {
						printk(KERN_DEBUG "%s: WTF? functionInfo is null here.", __func__);
					} else {
						printk(KERN_DEBUG "%s: Adding function F%02x with %d sources.\n",
							__func__, functionInfo->functionNum, functionInfo->numSources);

						mutex_lock(&rfi_mutex);
						list_add_tail(&functionInfo->link,
							&sensor->functions);
						mutex_unlock(&rfi_mutex);
					}

				} else {
					/* A zero or 0xff in the function number
					signals the end of the PDT */
					printk(KERN_DEBUG "%s:   Found End of PDT\n",
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
					"ending PDT scan.\n",
					__func__, retval);
				break;
			}
		}
		printk(KERN_DEBUG "%s: Done scanning.", __func__);

		/* calculate the interrupt register count - used in the
		ISR to read the correct number of interrupt registers */
		interruptRegisterCount = (interruptCount + 7) / 8;
		sensor->interruptRegisterCount = interruptRegisterCount;    /* TODO: Is this needed by the sensor anymore? */
	}

	return 0;

exit_fail:
	return retval;
}
EXPORT_SYMBOL(rmi_sensor_register_functions);

int rmi_sensor_register_device(struct rmi_sensor_device *dev, int index)
{
	int status;

	printk(KERN_INFO "%s: Registering sensor device.\n", __func__);

	/* make name - sensor00, sensor01, etc. */
	dev_set_name(&dev->dev, "sensor%02d", index);
	status = device_register(&dev->dev);

	return status;
}
EXPORT_SYMBOL(rmi_sensor_register_device);

static void rmi_sensor_unregister_device(struct rmi_sensor_device *rmisensordev)
{
	printk(KERN_INFO "%s: Unregistering sensor device.\n", __func__);

	device_unregister(&rmisensordev->dev);
}
EXPORT_SYMBOL(rmi_sensor_unregister_device);

int rmi_sensor_register_driver(struct rmi_sensor_driver *driver)
{
	static int index;
	int ret;
	char *drvrname;

	driver->workIsReady = false;

	printk(KERN_INFO "%s: Registering sensor driver.\n", __func__);
	driver->dispatchIRQs = dispatchIRQs;
	driver->attention = attention;
	driver->config = config;
	driver->probe = probe;

	/* assign the bus type for this driver to be rmi bus */
	driver->drv.bus = &rmi_bus_type;
	driver->drv.suspend = rmi_sensor_suspend;
	driver->drv.resume = rmi_sensor_resume;
	/* Create a function device and function driver for this Fn */
	drvrname = kzalloc(sizeof(sensorname) + 4, GFP_KERNEL);
	if (!drvrname) {
		printk(KERN_ERR "%s: Error allocating memeory for rmi_sensor_driver name.\n", __func__);
		return -ENOMEM;
	}
	sprintf(drvrname, "sensor%02d", index++);

	driver->drv.name = drvrname;
	driver->module = driver->drv.owner;

	/* register the sensor driver */
	ret = driver_register(&driver->drv);
	if (ret) {
		printk(KERN_ERR "%s: Failed driver_register %d\n",
			__func__, ret);
		goto exit_fail;
	}

	/* register the functions on the sensor */
	ret = rmi_sensor_register_functions(driver);
	if (ret) {
		printk(KERN_ERR "%s: Failed rmi_sensor_register_functions %d\n",
			__func__, ret);
	}

	/* configure the sensor - enable interrupts for each function, init work, set polling timer or adjust report rate, etc. */
	config(driver);

	printk(KERN_DEBUG "%s: sensor driver registration completed.", __func__);

exit_fail:
	return ret;
}
EXPORT_SYMBOL(rmi_sensor_register_driver);

static void rmi_sensor_unregister_driver(struct rmi_sensor_driver *driver)
{
	printk(KERN_DEBUG "%s: Unregistering sensor driver.\n", __func__);

	/* Stop the polling timer if doing polling */
	if (rmi_polling_required(driver))
		hrtimer_cancel(&driver->timer);

	flush_scheduled_work(); /* Make sure all scheduled work is stopped */

	driver_unregister(&driver->drv);
}
EXPORT_SYMBOL(rmi_sensor_unregister_driver);


static int __init rmi_sensor_init(void)
{
	printk(KERN_DEBUG "%s: RMI Sensor Init\n", __func__);
	return 0;
}

static void __exit rmi_sensor_exit(void)
{
	printk(KERN_DEBUG "%s: RMI Sensor Driver Exit\n", __func__);
	flush_scheduled_work(); /* Make sure all scheduled work is stopped */
}


module_init(rmi_sensor_init);
module_exit(rmi_sensor_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("RMI4 Sensor Driver");
MODULE_LICENSE("GPL");
