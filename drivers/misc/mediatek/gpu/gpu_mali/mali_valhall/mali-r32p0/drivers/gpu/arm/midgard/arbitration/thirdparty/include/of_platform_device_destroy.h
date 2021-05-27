/*
 *    Copyright (C) 2006 Benjamin Herrenschmidt, IBM Corp.
 *			 <benh@kernel.crashing.org>
 *    and		 Arnd Bergmann, IBM Corp.
 *    Merged from powerpc/kernel/of_platform.c and
 *    sparc{,64}/kernel/of_device.c by Stephen Rothwell
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version
 *  2 of the License, or (at your option) any later version.
 *
 */

#ifndef _OF_PLATFORM_DEVICE_DESTROY_H_
#define _OF_PLATFORM_DEVICE_DESTROY_H_

/* The function of_platform_device_destroy is exported in of_platform.h
 * from kernel v4.12. For older kernel versions we define it below.
 */
#if (KERNEL_VERSION(4, 12, 0) > LINUX_VERSION_CODE)

#ifdef CONFIG_OF_ADDRESS

#include <linux/dma-mapping.h>
#include <linux/amba/bus.h>

static inline int of_platform_device_destroy(struct device *dev, void *data)
{
	/* Do not touch devices not populated from the device tree */
	if (!dev->of_node || !of_node_check_flag(dev->of_node, OF_POPULATED))
		return 0;

	/* Recurse for any nodes that were treated as busses */
	if (of_node_check_flag(dev->of_node, OF_POPULATED_BUS))
		device_for_each_child(dev, NULL, of_platform_device_destroy);

	if (dev->bus == &platform_bus_type)
		platform_device_unregister(to_platform_device(dev));
#ifdef CONFIG_ARM_AMBA
	else if (dev->bus == &amba_bustype)
		amba_device_unregister(to_amba_device(dev));
#endif

	arch_teardown_dma_ops(dev);
	of_node_clear_flag(dev->of_node, OF_POPULATED);
	of_node_clear_flag(dev->of_node, OF_POPULATED_BUS);
	return 0;
}
#endif /* CONFIG_OF_ADDRESS */

#endif /* (KERNEL_VERSION(4, 12, 0) > LINUX_VERSION_CODE) */

#endif /* _OF_PLATFORM_DEVICE_DESTROY_H_ */
