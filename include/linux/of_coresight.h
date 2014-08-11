/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#ifndef __LINUX_OF_CORESIGHT_H
#define __LINUX_OF_CORESIGHT_H

#ifdef CONFIG_OF_CORESIGHT
extern struct coresight_platform_data *of_get_coresight_platform_data(
				struct device *dev, struct device_node *node);
extern struct coresight_cti_data *of_get_coresight_cti_data(
				struct device *dev, struct device_node *node);
#else
static inline struct coresight_platform_data *of_get_coresight_platform_data(
				struct device *dev, struct device_node *node)
{
	return NULL;
}
static inline struct coresight_cti_data *of_get_coresight_cti_data(
				struct device *dev, struct device_node *node)
{
	return NULL;
}
#endif

#endif
