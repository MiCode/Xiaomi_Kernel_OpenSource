/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _VCP_IPI_PIN_H_
#define _VCP_IPI_PIN_H_

#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
#include "vcp.h"

/* vcp awake timeout count definition */
#define VCP_AWAKE_TIMEOUT 50000

/* this is only for ipi to distinguish core0 and core1 */
enum {
	VCP_CORE0_ID = 0,
	VCP_CORE1_ID = 1,
};

extern char *core_ids[VCP_CORE_TOTAL];

extern void vcp_reset_awake_counts(void);
extern int vcp_clr_spm_reg(void *unused);
extern int vcp_awake_counts[];
#if IS_ENABLED(CONFIG_MTK_EMI)
extern void mtk_emidbg_dump(void);
#endif
#endif
