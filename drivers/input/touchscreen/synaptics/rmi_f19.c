/**
 *
 * Synaptics Register Mapped Interface (RMI4) Function $11 support for 2D.
 * Copyright (c) 2007 - 2011, Synaptics Incorporated
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

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/freezer.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/input/rmi_platformdata.h>
#include <linux/module.h>

#include "rmi.h"
#include "rmi_drvr.h"
#include "rmi_bus.h"
#include "rmi_sensor.h"
#include "rmi_function.h"
#include "rmi_f19.h"

struct f19_instance_data {
	struct rmi_F19_query *deviceInfo;
	struct rmi_F19_control *controlRegisters;
	bool *buttonDown;
	unsigned char buttonDataBufferSize;
	unsigned char *buttonDataBuffer;
	unsigned char *buttonMap;
	int fn19ControlRegisterSize;
	int fn19regCountForBitPerButton;
	int fn19btnUsageandfilterModeOffset;
	int fn19intEnableOffset;
	int fn19intEnableLen;
	int fn19singleBtnCtrlLen;
	int fn19singleBtnCtrlOffset;
	int fn19sensorMapCtrlOffset;
	int fn19sensorMapCtrlLen;
	int fn19singleBtnSensOffset;
	int fn19singleBtnSensLen;
	int fn19globalSensOffset;
	int fn19globalHystThreshOffset;
};

static ssize_t rmi_f19_buttonCount_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_f19_buttonCount_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(buttonCount, 0444, rmi_f19_buttonCount_show, rmi_f19_buttonCount_store);	/* RO attr */

static ssize_t rmi_f19_buttonMap_show(struct device *dev,
				struct device_attribute *attr, char *buf);

static ssize_t rmi_f19_buttonMap_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count);

DEVICE_ATTR(buttonMap, 0664, rmi_f19_buttonMap_show, rmi_f19_buttonMap_store);	/* RW attr */


/*
 * There is no attention function for F19 - it is left NULL
 * in the function table so it is not called.
 *
 */


/*
 * This reads in a sample and reports the F19 source data to the
 * input subsystem. It is used for both polling and interrupt driven
 * operation. This is called a lot so don't put in any informational
 * printks since they will slow things way down!
 */
void FN_19_inthandler(struct rmi_function_info *rmifninfo,
	unsigned int assertedIRQs)
{
	struct rmi_function_device *function_device;
	struct f19_instance_data *instanceData;
	int button;

	instanceData = (struct f19_instance_data *) rmifninfo->fndata;

	function_device = rmifninfo->function_device;

	/* Read the button data. */

	if (rmi_read_multiple(rmifninfo->sensor, rmifninfo->funcDescriptor.dataBaseAddr,
		instanceData->buttonDataBuffer, instanceData->buttonDataBufferSize)) {
		printk(KERN_ERR "%s: Failed to read button data registers.\n", __func__);
		return;
	}

	/* Generate events for buttons that change state. */
	for (button = 0; button < instanceData->deviceInfo->buttonCount; button++) {
		int buttonReg;
		int buttonShift;
		bool buttonStatus;

		/* determine which data byte the button status is in */
		buttonReg = button/4;
		/* bit shift to get button's status */
		buttonShift = button % 8;
		buttonStatus = ((instanceData->buttonDataBuffer[buttonReg] >> buttonShift) & 0x01) != 0;

		/* if the button state changed from the last time report it and store the new state */
		if (buttonStatus != instanceData->buttonDown[button]) {
			printk(KERN_DEBUG "%s: Button %d (code %d) -> %d.", __func__, button, instanceData->buttonMap[button], buttonStatus);
			/* Generate an event here. */
			input_report_key(function_device->input,
				instanceData->buttonMap[button], buttonStatus);
			instanceData->buttonDown[button] = buttonStatus;
		}
	}

	input_sync(function_device->input); /* sync after groups of events */
}
EXPORT_SYMBOL(FN_19_inthandler);

int FN_19_config(struct rmi_function_info *rmifninfo)
{
	int retval = 0;

	pr_debug("%s: RMI4 F19 config\n", __func__);

	/* TODO: Perform configuration.  In particular, write any cached control
	 * register values to the device. */

	return retval;
}
EXPORT_SYMBOL(FN_19_config);

/* Initialize any F19 specific params and settings - input
 * settings, device settings, etc.
 */
int FN_19_init(struct rmi_function_device *function_device)
{
	int i, retval = 0;
	struct f19_instance_data *instance_data = function_device->rfi->fndata;
	struct rmi_f19_functiondata *functiondata = rmi_sensor_get_functiondata(function_device->sensor, RMI_F19_INDEX);

	printk(KERN_DEBUG "%s: RMI4 F19 init\n", __func__);

	if (functiondata) {
		if (functiondata->button_map) {
			if (functiondata->button_map->nbuttons != instance_data->deviceInfo->buttonCount) {
				printk(KERN_WARNING "%s: Platformdata button map size (%d) != number of buttons on device (%d) - ignored.", __func__, functiondata->button_map->nbuttons, instance_data->deviceInfo->buttonCount);
			} else if (!functiondata->button_map->map) {
				printk(KERN_WARNING "%s: Platformdata button map is missing!", __func__);
			} else {
				for (i = 0; i < functiondata->button_map->nbuttons; i++)
					instance_data->buttonMap[i] = functiondata->button_map->map[i];
			}
		}
	}

	/* Set up any input events. */
	set_bit(EV_SYN, function_device->input->evbit);
	set_bit(EV_KEY, function_device->input->evbit);
	/* set bits for each button...*/
	for (i = 0; i < instance_data->deviceInfo->buttonCount; i++) {
		set_bit(instance_data->buttonMap[i], function_device->input->keybit);
	}

	printk(KERN_DEBUG "%s: Creating sysfs files.", __func__);
	retval = device_create_file(&function_device->dev, &dev_attr_buttonCount);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create button count.", __func__);
		return retval;
	}

	retval = device_create_file(&function_device->dev, &dev_attr_buttonMap);
	if (retval) {
		printk(KERN_ERR "%s: Failed to create button map.", __func__);
		return retval;
	}

	return 0;
}
EXPORT_SYMBOL(FN_19_init);

static int getControlRegisters(struct rmi_function_info *rmifninfo,
	struct rmi_function_descriptor *fndescr)
{
	struct f19_instance_data *instanceData;
	unsigned char *fn19Control = NULL;
	int retval = 0;

	/* Get the instance data - it should have been allocated and stored in detect.*/
	instanceData = rmifninfo->fndata;

	/* Check to make sure instanceData is really there before using.*/
	if (!instanceData) {
		printk(KERN_ERR "%s: Error - instance data not initialized yet when getting fn19 control registers.\n", __func__);
		return -EINVAL;
	}

	/* Allocate memory for the control registers. */
	instanceData->controlRegisters = kzalloc(sizeof(struct rmi_F19_control), GFP_KERNEL);
	if (!instanceData->controlRegisters) {
		printk(KERN_ERR "%s: Error allocating F19 control registers.\n", __func__);
		return -ENOMEM;
	}

	instanceData->fn19regCountForBitPerButton = (instanceData->deviceInfo->buttonCount + 7)/8;

	/* Need to compute the amount of data to read since it varies with the
	 * number of buttons */
	instanceData->fn19ControlRegisterSize = 1  /* 1 for filter mode and button usage bits */
		+ 2*instanceData->fn19regCountForBitPerButton  /* interrupt enable bits and single button participation bits */
		+ 2*instanceData->deviceInfo->buttonCount  /* sensormap registers + single button sensitivity registers */
		+ 2; /* 1 for global sensitivity adjust + 1 for global hysteresis threshold */

	/* Allocate a temp memory buffer to read the control registers into */
	fn19Control = kzalloc(instanceData->fn19ControlRegisterSize, GFP_KERNEL);
	if (!fn19Control) {
		printk(KERN_ERR "%s: Error allocating temp storage to read fn19 control info.\n", __func__);
		return -ENOMEM;
	}

	/* Grab a copy of the control registers. */
	retval = rmi_read_multiple(rmifninfo->sensor, fndescr->controlBaseAddr,
		fn19Control, instanceData->fn19ControlRegisterSize);
	if (retval) {
		printk(KERN_ERR "%s: Failed to read F19 control registers.", __func__);
		return retval;
	}

	/* Copy over control registers data to the instance data */
	instanceData->fn19btnUsageandfilterModeOffset = 0;
	instanceData->controlRegisters->buttonUsage = fn19Control[instanceData->fn19btnUsageandfilterModeOffset] & 0x3;
	instanceData->controlRegisters->filterMode = fn19Control[instanceData->fn19btnUsageandfilterModeOffset] & 0xc;

	/* Fill in interrupt enable registers */
	instanceData->fn19intEnableOffset = 1;
	instanceData->fn19intEnableLen = instanceData->fn19regCountForBitPerButton;
	instanceData->controlRegisters->intEnableRegisters = kzalloc(instanceData->fn19intEnableLen, GFP_KERNEL);
	if (!instanceData->controlRegisters->intEnableRegisters) {
		printk(KERN_ERR "%s: Error allocating storage for interrupt enable control info.\n", __func__);
		return -ENOMEM;
	}
	memcpy(instanceData->controlRegisters->intEnableRegisters, &fn19Control[instanceData->fn19intEnableOffset],
		instanceData->fn19intEnableLen);

	/* Fill in single button control registers */
	instanceData->fn19singleBtnCtrlOffset = instanceData->fn19intEnableOffset + instanceData->fn19intEnableLen;
	instanceData->fn19singleBtnCtrlLen = instanceData->fn19regCountForBitPerButton;
	instanceData->controlRegisters->singleButtonControl = kzalloc(instanceData->fn19singleBtnCtrlLen, GFP_KERNEL);
	if (!instanceData->controlRegisters->singleButtonControl) {
		printk(KERN_ERR "%s: Error allocating storage for single button participation control info.\n", __func__);
		return -ENOMEM;
	}
	memcpy(instanceData->controlRegisters->singleButtonControl, &fn19Control[instanceData->fn19singleBtnCtrlOffset],
		instanceData->fn19singleBtnCtrlLen);

	/* Fill in sensor map registers */
	instanceData->fn19sensorMapCtrlOffset = instanceData->fn19singleBtnCtrlOffset + instanceData->fn19singleBtnCtrlLen;
	instanceData->fn19sensorMapCtrlLen = instanceData->deviceInfo->buttonCount;
	instanceData->controlRegisters->sensorMap = kzalloc(instanceData->fn19sensorMapCtrlLen, GFP_KERNEL);
	if (!instanceData->controlRegisters->sensorMap) {
		printk(KERN_ERR "%s: Error allocating storage for sensor map control info.\n", __func__);
		return -ENOMEM;
	}
	memcpy(instanceData->controlRegisters->sensorMap, &fn19Control[instanceData->fn19sensorMapCtrlOffset],
		instanceData->fn19sensorMapCtrlLen);

	/* Fill in single button sensitivity registers */
	instanceData->fn19singleBtnSensOffset = instanceData->fn19sensorMapCtrlOffset + instanceData->fn19sensorMapCtrlLen;
	instanceData->fn19singleBtnSensLen = instanceData->deviceInfo->buttonCount;
	instanceData->controlRegisters->singleButtonSensitivity = kzalloc(instanceData->fn19singleBtnSensLen, GFP_KERNEL);
	if (!instanceData->controlRegisters->intEnableRegisters) {
		printk(KERN_ERR "%s: Error allocating storage for single button sensitivity control info.\n", __func__);
		return -ENOMEM;
	}
	memcpy(instanceData->controlRegisters->singleButtonSensitivity, &fn19Control[instanceData->fn19singleBtnSensOffset],
		instanceData->fn19singleBtnSensLen);

	/* Fill in global sensitivity adjustment and global hysteresis threshold values */
	instanceData->fn19globalSensOffset = instanceData->fn19singleBtnSensOffset + instanceData->fn19singleBtnSensLen;
	instanceData->fn19globalHystThreshOffset = instanceData->fn19globalSensOffset + 1;
	instanceData->controlRegisters->globalSensitivityAdjustment = fn19Control[instanceData->fn19globalSensOffset] & 0x1f;
	instanceData->controlRegisters->globalHysteresisThreshold = fn19Control[instanceData->fn19globalHystThreshOffset] & 0x0f;

	/* Free up temp storage that held copy of control registers */
	kfree(fn19Control);

	return 0;
}

int FN_19_detect(struct rmi_function_info *rmifninfo,
	struct rmi_function_descriptor *fndescr, unsigned int interruptCount)
{
	unsigned char fn19queries[2];
	int retval = 0;
	int i;
	struct f19_instance_data *instanceData;
	int fn19InterruptOffset;

	printk(KERN_DEBUG "%s: RMI4 F19 detect\n", __func__);

	instanceData = kzalloc(sizeof(struct f19_instance_data), GFP_KERNEL);
	if (!instanceData) {
		printk(KERN_ERR "%s: Error allocating F19 instance data.\n", __func__);
		return -ENOMEM;
	}
	instanceData->deviceInfo = kzalloc(sizeof(struct rmi_F19_query), GFP_KERNEL);
	if (!instanceData->deviceInfo) {
		printk(KERN_ERR "%s: Error allocating F19 device query.\n", __func__);
		return -ENOMEM;
	}
	rmifninfo->fndata = instanceData;

	/* Store addresses - used elsewhere to read data,
	* control, query, etc. */
	rmifninfo->funcDescriptor.queryBaseAddr = fndescr->queryBaseAddr;
	rmifninfo->funcDescriptor.commandBaseAddr = fndescr->commandBaseAddr;
	rmifninfo->funcDescriptor.controlBaseAddr = fndescr->controlBaseAddr;
	rmifninfo->funcDescriptor.dataBaseAddr = fndescr->dataBaseAddr;
	rmifninfo->funcDescriptor.interruptSrcCnt = fndescr->interruptSrcCnt;
	rmifninfo->funcDescriptor.functionNum = fndescr->functionNum;

	rmifninfo->numSources = fndescr->interruptSrcCnt;

	/* need to get number of fingers supported, data size, etc. -
	to be used when getting data since the number of registers to
	read depends on the number of fingers supported and data size. */
	retval = rmi_read_multiple(rmifninfo->sensor, fndescr->queryBaseAddr, fn19queries,
			sizeof(fn19queries));
	if (retval) {
		printk(KERN_ERR "%s: RMI4 F19 detect: "
			"Could not read function query registers 0x%x\n",
			__func__,  rmifninfo->funcDescriptor.queryBaseAddr);
		return retval;
	}

	/* Extract device data. */
	instanceData->deviceInfo->configurable = fn19queries[0] & 0x01;
	instanceData->deviceInfo->hasSensitivityAdjust = fn19queries[0] & 0x02;
	instanceData->deviceInfo->hasHysteresisThreshold = fn19queries[0] & 0x04;
	instanceData->deviceInfo->buttonCount = fn19queries[1] & 0x01F;
	printk(KERN_DEBUG "%s: F19 device - %d buttons...", __func__, instanceData->deviceInfo->buttonCount);

	/* Need to get interrupt info to be used later when handling
	interrupts. */
	rmifninfo->interruptRegister = interruptCount/8;

	/* loop through interrupts for each source in fn $11 and or in a bit
	to the interrupt mask for each. */
	fn19InterruptOffset = interruptCount % 8;

	for (i = fn19InterruptOffset;
			i < ((fndescr->interruptSrcCnt & 0x7) + fn19InterruptOffset);
			i++)
		rmifninfo->interruptMask |= 1 << i;

	/* Figure out just how much data we'll need to read. */
	instanceData->buttonDown = kcalloc(instanceData->deviceInfo->buttonCount, sizeof(bool), GFP_KERNEL);
	if (!instanceData->buttonDown) {
		printk(KERN_ERR "%s: Error allocating F19 button state buffer.\n", __func__);
		return -ENOMEM;
	}

	instanceData->buttonDataBufferSize = (instanceData->deviceInfo->buttonCount + 7) / 8;
	instanceData->buttonDataBuffer = kcalloc(instanceData->buttonDataBufferSize, sizeof(unsigned char), GFP_KERNEL);
	if (!instanceData->buttonDataBuffer) {
		printk(KERN_ERR "%s: Failed to allocate button data buffer.", __func__);
		return -ENOMEM;
	}

	instanceData->buttonMap = kcalloc(instanceData->deviceInfo->buttonCount, sizeof(unsigned char),  GFP_KERNEL);
	if (!instanceData->buttonMap) {
		printk(KERN_ERR "%s: Error allocating F19 button map.\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < instanceData->deviceInfo->buttonCount; i++)
		instanceData->buttonMap[i] = BTN_0 + i; /* default values */

	/* Grab the control register info. */
	retval = getControlRegisters(rmifninfo, fndescr);
	if (retval) {
		printk(KERN_ERR "%s: Error %d getting fn19 control register info.\n", __func__, retval);
		return retval;
	}

	return 0;
}
EXPORT_SYMBOL(FN_19_detect);

static ssize_t rmi_f19_buttonCount_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f19_instance_data *instance_data = (struct f19_instance_data *)fn->rfi->fndata;

	return sprintf(buf, "%u\n", instance_data->deviceInfo->buttonCount);
}

static ssize_t rmi_f19_buttonCount_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	/* Not allowed. */
	return -EPERM;
}

static ssize_t rmi_f19_buttonMap_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f19_instance_data *instance_data = (struct f19_instance_data *)fn->rfi->fndata;
	int i, len, totalLen = 0;

	/* loop through each button map value and copy it's string representation into buf */
	for (i = 0; i < instance_data->deviceInfo->buttonCount; i++) {
		/* get next button mapping value and write it to buf */
		len = sprintf(buf, "%u ", instance_data->buttonMap[i]);
		/* bump up ptr to next location in buf if the sprintf was valid */
		if (len > 0) {
			buf += len;
			totalLen += len;
		}
	}

	return totalLen;
}

static ssize_t rmi_f19_buttonMap_store(struct device *dev,
				struct device_attribute *attr,
				const char *buf, size_t count)
{
	struct rmi_function_device *fn = dev_get_drvdata(dev);
	struct f19_instance_data *instance_data = (struct f19_instance_data *)fn->rfi->fndata;
	unsigned int button;
	int i;
	int retval = count;
	int buttonCount = 0;
	unsigned char *tmpButtonMap;

	/* Do validation on the button map data passed in. */
	/* Store button mappings into a temp buffer and then verify button count
	and data prior to clearing out old button mappings and storing the new ones. */
	tmpButtonMap = kzalloc(instance_data->deviceInfo->buttonCount, GFP_KERNEL);
	if (!tmpButtonMap) {
		printk(KERN_ERR "%s: Error allocating temp button map.\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < instance_data->deviceInfo->buttonCount && *buf != 0; i++) {
		/* get next button mapping value and store and bump up to point to next item in buf */
		sscanf(buf, "%u", &button);

		/* Make sure the key is a valid key */
		if (button > KEY_MAX) {
			printk(KERN_ERR "%s: Error - button map for button %d is not a valid value 0x%x.\n",
				__func__, i, button);
			retval = -EINVAL;
			goto err_ret;
		}

		tmpButtonMap[i] = button;
		buttonCount++;

		/* bump up buf to point to next item to read */
		while (*buf != 0) {
			buf++;
			if (*(buf-1) == ' ')
				break;
		}
	}

	/* Make sure the button count matches */
	if (buttonCount != instance_data->deviceInfo->buttonCount) {
		printk(KERN_ERR "%s: Error - button map count of %d doesn't match device button count of %d.\n"
			 , __func__, buttonCount, instance_data->deviceInfo->buttonCount);
		retval = -EINVAL;
		goto err_ret;
	}

	/* Clear out old buttonMap data */
	memset(instance_data->buttonMap, 0, buttonCount);

	/* Loop through the temp buffer and copy the button event and set the key bit for the new mapping. */
	for (i = 0; i < buttonCount; i++) {
		instance_data->buttonMap[i] = tmpButtonMap[1];
		set_bit(instance_data->buttonMap[i], fn->input->keybit);
	}

err_ret:
	kfree(tmpButtonMap);

	return retval;
}
