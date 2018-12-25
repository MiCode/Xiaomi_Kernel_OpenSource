/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef TLSECMEM_H_
#define TLSECMEM_H_

#include "tci.h"

#include "secmem_plat.h"

/*
 * Command ID's for communication Trustlet Connector -> Trustlet.
 */
#define CMD_SEC_MEM_ALLOC         1
#define CMD_SEC_MEM_REF           2
#define CMD_SEC_MEM_UNREF         3
#define CMD_SEC_MEM_ALLOC_TBL     4
#define CMD_SEC_MEM_UNREF_TBL     5
#define CMD_SEC_MEM_USAGE_DUMP    6
#define CMD_SEC_MEM_ENABLE        7
#define CMD_SEC_MEM_DISABLE       8
#define CMD_SEC_MEM_ALLOCATED     9
#define CMD_SEC_MEM_ALLOC_PA      10
#define CMD_SEC_MEM_REF_PA        11
#define CMD_SEC_MEM_UNREF_PA      12
#define CMD_SEC_MEM_ALLOC_ZERO    13

#define CMD_SEC_MEM_DUMP_INFO     255

#define MAX_NAME_SZ              (32)

/*
 * Termination codes
 */
#define EXIT_ERROR                  ((uint32_t)(-1))

/*
 * command message.
 *
 * @param len Length of the data to process.
 * @param data Data to processed (cleartext or ciphertext).
 */
struct tl_cmd_t {
	/* Command header */
	struct tciCommandHeader_t header;
	/* Length of data to process or buffer */
	uint32_t                  len;
	/* Length of response buffer */
	uint32_t                  respLen;
};

/*
 * Response structure Trustlet -> Trustlet Connector.
 */
struct tl_rsp_t {
	/* Response header */
	struct tciResponseHeader_t header;
	uint32_t                   len;
};

struct tl_sender_info_t {
	uint8_t  name[MAX_NAME_SZ];
	uint32_t id;
};

/*
 * TCI message data.
 */
struct tciMessage_t {
	union {
		struct tl_cmd_t     cmd_secmem;
		struct tl_rsp_t     rsp_secmem;
	};

#ifdef SECMEM_64BIT_PHYS_SUPPORT
	uint64_t    alignment;  /* IN */
	uint64_t    size;       /* IN */
	uint32_t    refcount;   /* INOUT */
	uint64_t    sec_handle; /* OUT */
#else
	uint32_t    alignment;  /* IN */
	uint32_t    size;       /* IN */
	uint32_t    refcount;   /* INOUT */
	uint32_t    sec_handle; /* OUT */
#endif /* SECMEM_64BIT_PHYS_SUPPORT */
	uint32_t    ResultData;
	/* Debugging purpose */
	struct tl_sender_info_t sender;
};

/*
 * Trustlet UUID.
 */
#define TL_SECMEM_UUID {0x08, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, \
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}

#endif /* TLSECMEM_H_ */
