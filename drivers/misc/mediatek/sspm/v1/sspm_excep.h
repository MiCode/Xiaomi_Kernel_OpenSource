/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __SSPM_EXCEP_H__
#define __SSPM_EXCEP_H__

#include <linux/types.h>

enum sspm_excep_id {
	SSPM_NR_EXCEP,
};

extern void sspm_log_coredump_recv(unsigned int exists);
extern void sspm_aed(enum sspm_excep_id type);
extern unsigned int __init sspm_coredump_init(phys_addr_t start,
	phys_addr_t limit);
extern int __init sspm_coredump_init_done(void);
extern int sspm_excep_init(void);

#endif
