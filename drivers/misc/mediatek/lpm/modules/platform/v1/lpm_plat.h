/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LPM_PLAT_H__
#define __LPM_PLAT_H__

#include <linux/delay.h>
#include <lpm_type.h>
#include <lpm_plat_comm.h>

int lpm_do_mcusys_prepare_pdn(unsigned int status,
					   unsigned int *resource_req);
int lpm_do_mcusys_prepare_on(void);

#endif
