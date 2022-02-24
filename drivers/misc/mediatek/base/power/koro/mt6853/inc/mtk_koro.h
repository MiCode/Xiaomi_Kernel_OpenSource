// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _MTK_KORO_
#define _MTK_KORO_

#ifdef __KERNEL__
#include <linux/kernel.h>
#include <mt-plat/sync_write.h>
#endif

#if defined(__MTK_KORO_C__)
#include<arm-smccc.h>
#include<mtk_sip_svc.h>
#endif

extern void mtk_koro_disable(void);

#endif
