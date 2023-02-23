/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 MediaTek Inc.
 */

#ifndef _EMMC_RPMB_H
#define _EMMC_RPMB_H

#include <linux/mmc/ioctl.h>
#include <linux/mmc/card.h>

#if defined(CONFIG_MMC_MTK_PRO)
extern struct msdc_host *mtk_msdc_host[];
#endif

/************************************************************************
 *
 * RPMB IOCTL interface.
 *
 ***********************************************************************/
#define RPMB_IOCTL_PROGRAM_KEY  1
#define RPMB_IOCTL_WRITE_DATA   3
#define RPMB_IOCTL_READ_DATA    4

#if (defined(CONFIG_MICROTRUST_TEE_SUPPORT))

#define RPMB_MULTI_BLOCK_ACCESS 1

#if RPMB_MULTI_BLOCK_ACCESS
#define MAX_RPMB_TRANSFER_BLK 16
/* 8KB(16blks) per requests */
#define MAX_RPMB_REQUEST_SIZE (512*MAX_RPMB_TRANSFER_BLK)
#else
#define MAX_RPMB_TRANSFER_BLK 1
/* 512B(1blks) per requests */
#define MAX_RPMB_REQUEST_SIZE (512*MAX_RPMB_TRANSFER_BLK)
#endif

#define RPMB_IOCTL_SOTER_WRITE_DATA   5
#define RPMB_IOCTL_SOTER_READ_DATA    6
#define RPMB_IOCTL_SOTER_GET_CNT      7
#define RPMB_IOCTL_SOTER_GET_WR_SIZE  8
#define RPMB_IOCTL_SOTER_SET_KEY      9

struct rpmb_infor {
	unsigned int size;
	unsigned char *data_frame;
};
#endif /* CONFIG_MICROTRUST_TEE_SUPPORT */

struct rpmb_ioc_param {
	unsigned char *key;
	unsigned char *data;
	unsigned int  data_len;
	unsigned short addr;
	unsigned char *hmac;
	unsigned int hmac_len;
};
/***********************************************************************/


#define RPMB_SZ_STUFF 196
#define RPMB_SZ_MAC   32
#define RPMB_SZ_DATA  256
#define RPMB_SZ_NONCE 16
#define RPMB_IOC_MAX_BYTES (512L * 256) /* sync from linux/mmc/ioctl.h */

struct s_rpmb {
	unsigned char stuff[RPMB_SZ_STUFF];
	unsigned char mac[RPMB_SZ_MAC];
	unsigned char data[RPMB_SZ_DATA];
	unsigned char nonce[RPMB_SZ_NONCE];
	unsigned int write_counter;
	unsigned short address;
	unsigned short block_count;
	unsigned short result;
	unsigned short request;
};

enum {
	RPMB_SUCCESS = 0,
	RPMB_HMAC_ERROR,
	RPMB_RESULT_ERROR,
	RPMB_WC_ERROR,
	RPMB_NONCE_ERROR,
	RPMB_ALLOC_ERROR,
	RPMB_TRANSFER_NOT_COMPLETE,
};

#define RPMB_REQ               1       /* RPMB request mark */
#define RPMB_RESP              (1 << 1)/* RPMB response mark */
#define RPMB_AVAILABLE_SECTORS 8       /* 4K page size */

#define RPMB_TYPE_BEG          510
#define RPMB_RES_BEG           508
#define RPMB_BLKS_BEG          506
#define RPMB_ADDR_BEG          504
#define RPMB_WCOUNTER_BEG      500

#define RPMB_NONCE_BEG         484
#define RPMB_DATA_BEG          228
#define RPMB_MAC_BEG           196

struct emmc_rpmb_req {
	__u16 type;                     /* RPMB request type */
	__u16 *result;                  /* response or request result */
	__u16 blk_cnt;                  /* Number of blocks(half sector 256B) */
	__u16 addr;                     /* data address */
	__u32 *wc;                      /* write counter */
	__u8 *nonce;                    /* Ramdom number */
	__u8 *data;                     /* Buffer of the user data */
	__u8 *mac;                      /* Message Authentication Code */
	__u8 *data_frame;
};

int mmc_rpmb_register(struct mmc_host *mmc);
int mmc_rpmb_set_key(struct mmc_card *card, void *key);
int mmc_rpmb_read(struct mmc_card *card, u8 *buf, u16 blk, u16 cnt, void *key);
int mmc_rpmb_write(struct mmc_card *card, u8 *buf, u16 blk, u16 cnt, void *key);

extern void emmc_rpmb_set_host(void *mmc_host);


#endif
