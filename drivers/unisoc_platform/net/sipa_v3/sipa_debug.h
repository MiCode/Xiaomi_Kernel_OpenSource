/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SIPA_ROC1_LINUX_DEBUG_H__
#define __SIPA_ROC1_LINUX_DEBUG_H__

#include "sipa_priv.h"

void sipa_dbg(struct sipa_plat_drv_cfg *sipa, const char *fmt, ...);
#ifdef CONFIG_DEBUG_FS
int sipa_init_debugfs(struct sipa_plat_drv_cfg *sipa);
void sipa_exit_debugfs(struct sipa_plat_drv_cfg *sipa);
#else
static inline int sipa_init_debugfs(struct sipa_plat_drv_cfg *sipa)
{
	return 0;
}

static inline void sipa_exit_debugfs(struct sipa_plat_drv_cfg *sipa)
{
}
#endif

#endif /*  __SIPA_ROC1_LINUX_DEBUG_H__ */
