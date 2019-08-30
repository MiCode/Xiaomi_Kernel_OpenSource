/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef CPU_CTRL_CFP_H
#define CPU_CTRL_CFP_H

#include <mt-plat/cpu_ctrl.h>

extern void cpu_ctrl_cfp(struct ppm_limit_data *desired_freq);

extern int cpu_ctrl_cfp_init(struct proc_dir_entry *parent);
extern void cpu_ctrl_cfp_exit(void);
#endif

