/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
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
