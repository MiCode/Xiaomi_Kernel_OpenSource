/*
 * drivers/video/tegra/host/vi/vi.h
 *
 * Tegra Graphics Host ISP
 *
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.
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

#ifndef __NVHOST_ISP_H__
#define __NVHOST_ISP_H__

#include "camera_priv_defs.h"

typedef void (*callback)(void *);

struct tegra_isp_mfi {
	struct work_struct my_isp_work;
};

struct isp {
	struct platform_device *ndev;
#if defined(CONFIG_TEGRA_ISOMGR)
	tegra_isomgr_handle isomgr_handle;
#endif
	int dev_id;
	void __iomem    *base;
	spinlock_t lock;
	int irq;

	struct workqueue_struct *isp_workqueue;
	struct tegra_isp_mfi *my_isr_work;
};

extern const struct file_operations tegra_isp_ctrl_ops;
int nvhost_isp_t124_finalize_poweron(struct platform_device *);
int tegra_isp_register_mfi_cb(callback cb, void *cb_arg);
int tegra_isp_unregister_mfi_cb(void);

#endif
