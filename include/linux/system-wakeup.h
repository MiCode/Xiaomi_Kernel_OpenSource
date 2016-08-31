/*
 * PMC interface for NVIDIA SoCs Tegra
 *
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
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

#ifndef __LINUX_SYSTEM_WAKEUP_H__
#define __LINUX_SYSTEM_WAKEUP_H__

#ifdef CONFIG_PM_SLEEP
extern int get_wakeup_reason_irq(void);
#else
static inline int get_wakeup_reason_irq(void)
{
	return -EINVAL;
}
#endif

#endif	/* __LINUX_SYSTEM_WAKEUP_H__ */
