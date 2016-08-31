/*
 * include/linux/tegra_gmi_access.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
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
 *
 */

int request_gmi_access(u32 gdevHandle);
void release_gmi_access(void);
u32 register_gmi_device(const char *dev_name, u32 priority);

typedef void(*gasync_callbackp)(void *priv_data);
int enqueue_gmi_async_request(u32 gdev_handle,
		gasync_callbackp cb, void *pdata);

