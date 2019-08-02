/*
 * Copyright (c) 2013, 2018-2019 The Linux Foundation. All rights reserved.
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

#ifndef __MSM_IPC_BRIDGE_H__
#define __MSM_IPC_BRIDGE_H__

#include <linux/platform_device.h>

/*
 * The IPC bridge driver adds a IPC bridge platform device when the
 * underlying transport is ready. The IPC transport driver acts as a
 * platform driver for this device. The platform data is populated by
 * IPC bridge driver to facilitate I/O. The callback functions are
 * passed in platform data to avoid export functions. This would allow
 * different bridge drivers to exist in the kernel. The IPC bridge driver
 * removes the platform device when the underly transport is no longer
 * available. It typically happens during shutdown and remote processor's
 * subsystem restart.
 */

/**
 * struct ipc_bridge_platform_data - platform device data for IPC
 *              transport driver.
 * @max_read_size: The maximum possible read size.
 * @max_write_size: The maximum possible write size.
 * @open: The open must be called before starting I/O.  The IPC bridge
 *              driver use the platform device pointer to identify the
 *              underlying transport channel. The IPC bridge driver may
 *              notify that remote processor that it is ready to receive
 *              data. Returns 0 upon success and appropriate error code
 *              upon failure.
 * @read: The read is done synchronously and should be called from process
 *              context. Returns the number of bytes read from remote
 *              processor or error code upon failure. The IPC transport
 *              driver may pass the buffer of max_read_size length if the
 *              available data size is not known in advance.
 * @write: The write is done synchronously and should be called from process
 *              context. The IPC bridge driver uses the same buffer for DMA
 *              to avoid additional memcpy. So it must be physically contiguous.
 *              Returns the number of bytes written or error code upon failure.
 * @close: The close must be called when the IPC bridge platform device
 *              is removed. The IPC transport driver may call close when
 *              it is no longer required to communicate with remote processor.
 */
struct ipc_bridge_platform_data {
	unsigned int max_read_size;
	unsigned int max_write_size;
	int (*open)(struct platform_device *pdev);
	int (*read)(struct platform_device *pdev, char *buf,
			unsigned int count);
	int (*write)(struct platform_device *pdev, char *buf,
			unsigned int count);
	void (*close)(struct platform_device *pdev);
};

#endif
