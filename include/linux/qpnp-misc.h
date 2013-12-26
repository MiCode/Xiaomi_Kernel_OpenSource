/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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

#ifndef __QPNP_MISC_H
#define __QPNP_MISC_H

#include <linux/errno.h>

#ifdef CONFIG_QPNP_MISC
/**
 * qpnp_misc_irqs_available - check if IRQs are available
 *
 * @consumer_dev: device struct
 *
 * This function returns true if the MISC interrupts are available
 * based on a check in the MISC peripheral revision registers.
 *
 * Any consumer of this function needs to reference a MISC device phandle
 * using the "qcom,misc-ref" property in their device tree node.
 */

int qpnp_misc_irqs_available(struct device *consumer_dev);
#else
static int qpnp_misc_irqs_available(struct device *consumer_dev)
{
	return 0;
}
#endif
#endif
