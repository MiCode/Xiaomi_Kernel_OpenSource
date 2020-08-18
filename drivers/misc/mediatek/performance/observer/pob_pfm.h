/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef POB_PFM_H
#define POB_PFM_H

#include <linux/debugfs.h>

void pob_qos_tracelog(unsigned long val, void *data);

int pob_qos_pfm_init(struct dentry *pob_debugfs_dir);
int pob_qos_pfm_enable(void);
int pob_qos_pfm_disable(void);

#endif

