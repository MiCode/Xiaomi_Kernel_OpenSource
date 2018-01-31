/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef __QTI_VIRT_SENS_H__
#define __QTI_VIRT_SENS_H__

#ifdef CONFIG_QTI_VIRTUAL_SENSOR

int qti_virtual_sensor_register(struct device *dev);

#else

static inline int qti_virtual_sensor_register(struct device *dev)
{
	return -ENODEV;
}

#endif /* CONFIG_QTI_VIRTUAL_SENSOR */

#endif /* __QTI_VIRT_SENS_H__ */
