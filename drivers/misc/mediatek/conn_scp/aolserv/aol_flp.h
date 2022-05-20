/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2022 MediaTek Inc.
 */

#ifndef _AOL_FLP_H_
#define _AOL_FLP_H_

#include <linux/types.h>
#include <linux/compiler.h>

int aol_flp_init(void);
void aol_flp_deinit(void);

int aol_flp_bind_to_conap(void);
int aol_flp_test(void);

#endif // _AOL_FLP_H_
