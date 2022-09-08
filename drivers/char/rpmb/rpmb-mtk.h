/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_RPMB_H_
#define _MTK_RPMB_H_

/**********************************************************
 * Function Declaration                                   *
 **********************************************************/
#if IS_ENABLED(CONFIG_RPMB)
int mmc_rpmb_register(struct mmc_host *mmc);
#else
//int mmc_rpmb_register(...);
#endif

#endif
