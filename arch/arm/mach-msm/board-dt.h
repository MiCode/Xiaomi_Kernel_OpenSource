/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/of_platform.h>

extern struct sys_timer msm_dt_timer;
void __init msm_dt_init_irq(void);
void __init msm_dt_init_irq_nompm(void);
void __init msm_dt_init_irq_l2x0(void);
int __init msm_scan_dt_map_imem(unsigned long node, const char *uname,
				int depth, void *data);
void __init board_dt_populate(struct of_dev_auxdata *adata);
