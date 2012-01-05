/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/spmi.h>
#include <linux/of_irq.h>

#ifdef CONFIG_OF_SPMI
int of_spmi_register_devices(struct spmi_controller *ctrl);
#else
static int of_spmi_register_devices(struct spmi_controller *ctrl)
{
	return -ENXIO;
}
#endif /* CONFIG_OF_SPMI */
