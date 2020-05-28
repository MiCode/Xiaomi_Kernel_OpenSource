/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __MCUPM_PLT_H__
#define __MCUPM_PLT_H__
#include <linux/device.h>

/* import from mcupm_logger */
extern unsigned int mcupm_logger_init(phys_addr_t start, phys_addr_t limit);
extern int mcupm_logger_init_done(void);
extern int mcupm_sysfs_create_file(struct device_attribute *attr);
extern int mcupm_sysfs_init(void);
extern int mcupm_plt_module_init(void);
extern void mcupm_plt_module_exit(void);
#endif
