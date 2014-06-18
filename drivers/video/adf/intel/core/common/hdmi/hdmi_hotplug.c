/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/

#include "hdmi_hotplug.h"

static irqreturn_t hdmi_hotplug_top(int irq, void *data)
{
	if (data == NULL)
		return IRQ_HANDLED;
	return IRQ_WAKE_THREAD;
}

void adf_hdmi_hpd_init_work(struct work_struct *work)
{
	struct hdmi_pipe *pipe;
	int err = 0;
	pipe = container_of(work, struct hdmi_pipe, hotplug_register_work);
	err = pci_register_driver(&pipe->hdmi_hpd_driver);
	if (err)
		pr_err("%s: Error: Failed to register HDMI hotplug device\n",
			__func__);
}

int hdmi_hpd_probe(struct pci_dev *pdev, const struct pci_device_id *id)
{
	int err = 0;
	struct hdmi_pipe *pipe;

	pipe = (struct hdmi_pipe *)id->driver_data;
	if (pdev == NULL || pipe == NULL) {
		pr_err("%s: no device or context\n", __func__);
		err =  -EINVAL;
		return err;
	}

	err = pci_enable_device(pdev);
	if (err) {
		pr_err("%s: Failed to enable MSIC PCI device=0x%x\n",
			__func__, pipe->config.pci_device_id);
		err = -EIO;
		goto out_err0;
	}


	err = irq_set_irq_type(pipe->config.irq_number, IRQ_TYPE_EDGE_BOTH);
	if (err) {
		pr_err("%s: Failed to set HDMI HPD IRQ type for IRQ %d\n",
			 __func__, pipe->config.irq_number);
		goto out_err1;
	}

	err = request_threaded_irq(pipe->config.irq_number, hdmi_hotplug_top,
				   pipe->hotplug_irq_cb, IRQF_SHARED,
				   HDMI_HPD_DRIVER_NAME,
				   pipe->hotplug_data);
	if (err) {
		pr_err("%s: Register irq interrupt %d failed\n",
		      __func__, pipe->config.irq_number);
		goto out_err1;
	}
	return err;

out_err1:
	pci_disable_device(pdev);
out_err0:
	pci_dev_put(pdev);
	return err;
}
