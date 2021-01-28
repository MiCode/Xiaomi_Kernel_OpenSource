/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __AAL_CONTROL_H__
#define __AAL_CONTROL_H__

#define AAL_TAG                  "[ALS/AAL]"

extern int aal_use;
int __init AAL_init(void);
void __exit AAL_exit(void);

#endif

