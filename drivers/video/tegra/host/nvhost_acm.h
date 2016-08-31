/*
 * drivers/video/tegra/host/nvhost_acm.h
 *
 * Tegra Graphics Host Automatic Clock Management
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

#ifndef __NVHOST_ACM_H
#define __NVHOST_ACM_H

#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/pm_domain.h>
#include <linux/pm_runtime.h>
#include <linux/nvhost.h>

extern const struct dev_pm_ops nvhost_module_pm_ops;

/* Sets clocks and powergating state for a module */
int nvhost_module_init(struct platform_device *ndev);
void nvhost_module_deinit(struct platform_device *dev);
int nvhost_module_suspend(struct device *dev);
int nvhost_module_resume(struct device *dev);

void nvhost_module_busy_noresume(struct platform_device *dev);
void nvhost_module_reset(struct platform_device *dev);
void nvhost_module_busy(struct platform_device *dev);
void nvhost_module_disable_poweroff(struct platform_device *dev);
void nvhost_module_enable_poweroff(struct platform_device *dev);
void nvhost_module_idle_mult(struct platform_device *dev, int refs);
int nvhost_module_add_client(struct platform_device *dev,
		void *priv);
void nvhost_module_remove_client(struct platform_device *dev,
		void *priv);
int nvhost_module_get_rate(struct platform_device *dev,
		unsigned long *rate,
		int index);
int nvhost_module_set_rate(struct platform_device *dev, void *priv,
		unsigned long rate, int index, int bBW);
int nvhost_module_set_devfreq_rate(struct platform_device *dev, int index,
		unsigned long rate);

static inline bool nvhost_module_powered(struct platform_device *dev)
{
	return !pm_runtime_status_suspended(&dev->dev);
}

static inline void nvhost_module_idle(struct platform_device *dev)
{
	nvhost_module_idle_mult(dev, 1);
}

/* common runtime pm and power domain APIs */
int nvhost_module_add_domain(struct generic_pm_domain *domain,
	struct platform_device *pdev);
int nvhost_module_enable_clk(struct device *dev);
int nvhost_module_disable_clk(struct device *dev);
int nvhost_module_prepare_poweroff(struct device *dev);
int nvhost_module_finalize_poweron(struct device *dev);

#endif
