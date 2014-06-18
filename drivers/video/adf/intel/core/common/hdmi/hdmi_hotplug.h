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
#ifndef HDMI_HOTPLUG_H_
#define HDMI_HOTPLUG_H_

#include <linux/kernel.h>
#include <linux/pci.h>
#include "hdmi_pipe.h"

#define HDMI_HPD_DRIVER_NAME "HDMI HPD DRIVER"

void adf_hdmi_hpd_init_work(struct work_struct *work);
int hdmi_hpd_probe(struct pci_dev *pdev, const struct pci_device_id *id);

#endif /* HDMI_HOTPLUG_H_ */
