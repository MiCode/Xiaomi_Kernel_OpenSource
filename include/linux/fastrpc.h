/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef __LINUX_fastrpc_H
#define __LINUX_fastrpc_H

#include <linux/device.h>
#include <linux/errno.h>
#include <linux/kernel.h>

#define FASTRPC_DRV_NAME_SIZE 32

enum fastrpc_driver_status {
	FASTRPC_PROC_DOWN = 0,
};
enum fastrpc_driver_invoke_nums {
	FASTRPC_DEV_MAP_DMA = 1,
	FASTRPC_DEV_UNMAP_DMA,
};

/**
 * struct fastrpc_dev_map_dma - fastrpc dma buffer map structure
 * @buf        : Shared DMA buf object
 * @attrs      : Attributes to map buffer on IOMMU
 * @size       : Size of DMA buffer
 * @v_dsp_addr : Virtual addr of DSP after mapping the buffer on DSP
 */
struct fastrpc_dev_map_dma {
	struct dma_buf *buf;
	uint32_t attrs;
	size_t size;
	uint64_t v_dsp_addr;
};

/**
 * struct fastrpc_dev_unmap_dma - fastrpc dma buffer unmap structure
 * @buf   : Shared DMA buf object
 * @size  : Size of DMA buffer
 */
struct fastrpc_dev_unmap_dma {
	struct dma_buf *buf;
	size_t size;
};

/**
 * fastrpc_device - device that belong to the fastrpc bus
 * @hn: Head node to add to fastrpc device list
 * @dev: the device struct
 * @handle: handle of the process
 * @fl: process file of fastrpc device
 * @dev_close: flag to determine if device is closed
 * @refs: reference count of drivers using the device
 */
struct fastrpc_device {
	struct hlist_node hn;
	struct device dev;
	int handle;
	struct fastrpc_file *fl;
	bool dev_close;
	unsigned int refs;
};

#define to_fastrpc_device(d) container_of(d, struct fastrpc_device, dev)

/**
 * struct fastrpc_driver - fastrpc driver struct
 * @hn: Node to add to fastrpc driver list
 * @driver: underlying device driver
 * @device: device that is matching to driver
 * @handle: handle of the process
 * @create: 0 to attach, 1 to create process
 * @probe: invoked when a matching fastrpc device (i.e. device) is found
 * @callback: invoked when there is a status change in the process
 */
struct fastrpc_driver {
	struct hlist_node hn;
	struct device_driver driver;
	struct device *device;
	int handle;
	int create;
	int (*probe)(struct fastrpc_device *dev);
	int (*callback)(struct fastrpc_device *dev,
					enum fastrpc_driver_status status);
};

#define to_fastrpc_driver(x) container_of((x), struct fastrpc_driver, driver)

/**
 * function fastrpc_driver_register - Register fastrpc driver
 * @drv: Initialized fastrpc driver structure pointer
 */
int fastrpc_driver_register(struct fastrpc_driver *drv);

/**
 * function fastrpc_driver_unregister - Un-register fastrpc driver
 * @drv: fastrpc driver structure pointer
 */
void fastrpc_driver_unregister(struct fastrpc_driver *drv);

/**
 * function fastrpc_driver_invoke - fastrpc driver invocation function
 * Invoke fastrpc driver using fastrpc_device received in probe of registration
 * @dev         : Device received in probe of registration.
 * @invoke_num  : Invocation number of operation,
 *                one of "fastrpc_driver_invoke_nums"
 * @invoke_param: Address of invocation structure corresponding to invoke_num
 *                (struct fastrpc_dev_map_dma *) for FASTRPC_DEV_MAP_DMA
 *                (struct fastrpc_dev_unmap_dma *) for FASTRPC_DEV_UNMAP_DMA.
 */
long fastrpc_driver_invoke(struct fastrpc_device *dev,
	enum fastrpc_driver_invoke_nums invoke_num, unsigned long invoke_param);

/**
 * module_fastrpc_driver() - Helper macro for registering a fastrpc driver
 * @__fastrpc_driver: fastrpc_driver struct
 *
 * Helper macro for fastrpc drivers which do not do anything special in module
 * init/exit. This eliminates a lot of boilerplate code. Each module may only
 * use this macro once, and calling it replaces module_init and module_exit.
 */
#define module_fastrpc_driver(__fastrpc_driver) \
static int __init __fastrpc_driver##_init(void) \
{ \
	return fastrpc_driver_register(&(__fastrpc_driver)); \
} \
module_init(__fastrpc_driver##_init); \
static void __exit __fastrpc_driver##_exit(void) \
{ \
	fastrpc_driver_unregister(&(__fastrpc_driver)); \
} \
module_exit(__fastrpc_driver##_exit)

#endif /* __LINUX_fastrpc_H */
