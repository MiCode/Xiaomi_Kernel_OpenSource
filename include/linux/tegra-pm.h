/*
 * include/linux/tegra-pm.h
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _LINUX_TEGRA_PM_H_
#define _LINUX_TEGRA_PM_H_

#define TEGRA_PM_SUSPEND	0x0001
#define TEGRA_PM_RESUME		0x0002
 
int tegra_register_pm_notifier(struct notifier_block *nb);
int tegra_unregister_pm_notifier(struct notifier_block *nb);

#endif /* _LINUX_TEGRA_PM_H_ */
