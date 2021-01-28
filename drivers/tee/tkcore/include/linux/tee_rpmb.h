/* SPDX-License-Identifier: GPL-2.0
 *
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#ifndef TEE_RPMB_H
#define TEE_RPMB_H

#define TEE_RPMB_EMMC_CID_SIZE 16

#if defined(IN_KERNEL_RPMB_SUPPORT)

struct tee_rpmb_dev_info {
	uint8_t cid[TEE_RPMB_EMMC_CID_SIZE];
	uint8_t rpmb_size_mult;
	uint8_t rel_wr_sec_c;
	uint8_t ret_code;
};

struct tkcore_rpmb_request {
	uint16_t type;
	uint16_t blk_cnt;
	uint16_t addr;
	uint8_t *data_frame;
};

#define TEE_RPMB_GET_DEV_INFO		0x10
#define TEE_RPMB_PROGRAM_KEY		0x11
#define TEE_RPMB_GET_WRITE_COUNTER	0x12
#define TEE_RPMB_WRITE_DATA			0x13
#define TEE_RPMB_READ_DATA			0x14
#define TEE_RPMB_SWITCH_NORMAL		0x15

/*
 * the following function must be
 * implemented in kernel driver
 */
extern int tkcore_emmc_rpmb_execute(
		struct tkcore_rpmb_request *req);

#endif

#endif
