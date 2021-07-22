/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

/**
 * @file   drrpmb_Api.h
 * @brief  Contains DCI command definitions and data structures
 *
 */

#ifndef __DRRPMBAPI_H__
#define __DRRPMBAPI_H__

#include "dci.h"


/*
 * Command ID's
 */
#define DCI_RPMB_CMD_READ_DATA      1
#define DCI_RPMB_CMD_GET_WCNT       2
#define DCI_RPMB_CMD_WRITE_DATA     3
#define DCI_RPMB_CMD_PROGRAM_KEY    4


/*... add more command ids when needed */

/**
 * command message.
 *
 * @param len Length of the data to process.
 * @param data Data to be processed
 */
struct cmd_t {
	struct dciCommandHeader_t header;     /**< Command header */
	uint32_t            len;        /**< Length of data to process */
};

/**
 * Response structure
 */
struct rsp_t {
	struct dciResponseHeader_t header;     /**< Response header */
	uint32_t            len;
};


/*
 * Alternative access flow to improve performance. (this is customization)
 */
#define RPMB_MULTI_BLOCK_ACCESS 1

#if RPMB_MULTI_BLOCK_ACCESS
/* 16 blocks per request. */
#define MAX_RPMB_TRANSFER_BLK (16U)
#define MAX_RPMB_REQUEST_SIZE (512U*MAX_RPMB_TRANSFER_BLK) /* 8KB */
#else
/* 1 block per request. */
#define MAX_RPMB_TRANSFER_BLK (1U)
#define MAX_RPMB_REQUEST_SIZE (512U*MAX_RPMB_TRANSFER_BLK) /* 512byte */
#endif

struct rpmb_req_t {
	uint8_t frame[MAX_RPMB_REQUEST_SIZE];
	uint32_t frameLen;
	uint16_t type;
	uint16_t addr;
	uint16_t blks;
	uint16_t result;
};

/*
 * DCI message data.
 */
struct dciMessage_t {
	union {
		struct cmd_t  command;
		struct rsp_t  response;
	};

	struct rpmb_req_t request;

};

/*
 * Driver UUID. Update accordingly after reserving UUID
 */
#define RPMB_UUID { { 7, 5, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 } }

#endif
