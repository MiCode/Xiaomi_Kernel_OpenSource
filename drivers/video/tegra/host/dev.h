/*
 * drivers/video/tegra/host/dev.h
 *
 * Copyright (c) 2012, NVIDIA Corporation.
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

#ifndef NVHOST_DEV_H
#define NVHOST_DEV_H

#include "host1x/host1x.h"

struct platform_device;

void nvhost_device_list_init(void);
int nvhost_device_list_add(struct platform_device *pdev);
void nvhost_device_list_for_all(void *data,
	int (*fptr)(struct platform_device *pdev, void *fdata));
struct platform_device *nvhost_device_list_match_by_id(u32 id);
void nvhost_device_list_remove(struct platform_device *pdev);

#endif
