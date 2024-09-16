/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _CONNINFRA_DBG_H_
#define _CONNINFRA_DBG_H_
#include "osal.h"

typedef int(*CONNINFRA_DEV_DBG_FUNC) (int par1, int par2, int par3);
int conninfra_dev_dbg_init(void);
int conninfra_dev_dbg_deinit(void);

#endif /* _CONNINFRA_DBG_H_ */
