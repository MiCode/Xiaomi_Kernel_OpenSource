/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _WMT_PROC_DBG_H_
#define _WMT_PROC_DBG_H_

#include "osal.h"

#define CFG_WMT_PROC_FOR_AEE 1
#define CFG_WMT_PROC_FOR_DUMP_INFO 1

#if CFG_WMT_PROC_FOR_DUMP_INFO
INT32 wmt_dev_proc_for_dump_info_setup(VOID);
INT32 wmt_dev_proc_for_dump_info_remove(VOID);
#else
INT32 wmt_dev_proc_for_dump_info_setup(VOID) {}
INT32 wmt_dev_proc_for_dump_info_remove(VOID) {}
#endif

INT32 wmt_dev_proc_init(VOID);
INT32 wmt_dev_proc_deinit(VOID);


#endif /* _WMT_PROC_DBG_H_ */
