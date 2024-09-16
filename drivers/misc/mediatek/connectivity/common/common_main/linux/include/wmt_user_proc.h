/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _WMT_USER_PROC_H_
#define _WMT_USER_PROC_H_
#include "osal.h"

typedef INT32(*WMT_DEV_USER_PROC_FUNC) (INT32 par1, INT32 par2, INT32 par3);
INT32 wmt_dev_user_proc_setup(VOID);
INT32 wmt_dev_user_proc_remove(VOID);

#endif /* _WMT_USER_PROC_H_ */
