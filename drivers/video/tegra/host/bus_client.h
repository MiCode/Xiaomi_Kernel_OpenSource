/*
 * drivers/video/tegra/host/bus_client.h
 *
 * Tegra Graphics Host client
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __NVHOST_BUS_CLIENT_H
#define __NVHOST_BUS_CLIENT_H

#include <linux/types.h>

struct firmware;
struct platform_device;

int nvhost_read_module_regs(struct platform_device *ndev,
			u32 offset, int count, u32 *values);

int nvhost_write_module_regs(struct platform_device *ndev,
			u32 offset, int count, const u32 *values);

int nvhost_client_user_init(struct platform_device *dev);

int nvhost_client_device_init(struct platform_device *dev);

int nvhost_client_device_release(struct platform_device *dev);

const struct firmware *
nvhost_client_request_firmware(struct platform_device *dev,
	const char *fw_name);

int nvhost_client_device_get_resources(struct platform_device *dev);

#endif
