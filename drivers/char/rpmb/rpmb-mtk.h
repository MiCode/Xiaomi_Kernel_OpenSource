/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _MTK_RPMB_H_
#define _MTK_RPMB_H_

/**********************************************************
 * Function Declaration                                   *
 **********************************************************/
#if IS_ENABLED(CONFIG_MICROTRUST_TEE_SUPPORT)

#define RPMB_MULTI_BLOCK_ACCESS 1

#if RPMB_MULTI_BLOCK_ACCESS
/* 8KB(16blks) per requests */
#else
#define MAX_RPMB_TRANSFER_BLK 1
/* 512B(1blks) per requests */
#define MAX_RPMB_REQUEST_SIZE (512*MAX_RPMB_TRANSFER_BLK)
#endif

#define RPMB_IOCTL_SOTER_WRITE_DATA   5
#define RPMB_IOCTL_SOTER_READ_DATA    6
#define RPMB_IOCTL_SOTER_GET_CNT      7
#define RPMB_IOCTL_SOTER_GET_WR_SIZE      8
#define RPMB_IOCTL_SOTER_SET_KEY      9

struct rpmb_infor {
	unsigned int size;
	unsigned char *data_frame;
};
#endif /* CONFIG_MICROTRUST_TEE_SUPPORT */

#if IS_ENABLED(CONFIG_RPMB)
int mmc_rpmb_register(struct mmc_host *mmc);
#else
//int mmc_rpmb_register(...);
#endif

#endif
