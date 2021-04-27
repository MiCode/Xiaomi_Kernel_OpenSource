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
 */
#ifndef CPU_CTRL_CFP_H
#define CPU_CTRL_CFP_H

#include "cpu_ctrl.h"

extern void cpu_ctrl_cfp(struct ppm_limit_data *desired_freq);

extern int cpu_ctrl_cfp_init(struct proc_dir_entry *parent);
extern void cpu_ctrl_cfp_exit(void);
#endif

