// SPDX-License-Identifier: GPL-2.0
/*
 *  vow_scp.c  --  VoW SCP
 *
 *  Copyright (c) 2020 MediaTek Inc.
 *  Author: Michael HSiao <michael.hsiao@mediatek.com>
 */


/*****************************************************************************
 * Header Files
 *****************************************************************************/
#include "vow_scp.h"
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#include "scp_ipi.h"
#endif

/*****************************************************************************
 * Function
 ****************************************************************************/
unsigned int vow_check_scp_status(void)
{
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
	return is_scp_ready(SCP_A_ID);
#else
	return 0;
#endif
}
