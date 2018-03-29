/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _CMDQ_IWC_SEC_H_
#define  _CMDQ_IWC_SEC_H_
#include "cmdqSecTl_Api.h"
#include "tz_cross/trustzone.h"
#include "tz_cross/ta_mem.h"
#include "trustzone/kree/system.h"
#include "trustzone/kree/mem.h"


/**
 * inter-world communication state
 * this file should in cmdq_sec.h but cmdq_core.h will use following structure
 * so if following context in cmdq_sec.h, then cmdq_core.h will include cmdq_sec.h
 * but the question is cmdq_sec.h includes cmdq_core.h already.
 * so we extract this context out in a separate file which will not included outside the cmdq folder
 */
typedef enum {
	IWC_INIT = 0,
	IWC_MOBICORE_OPENED = 1,
	IWC_WSM_ALLOCATED = 2,
	IWC_SES_OPENED = 3,
	IWC_SES_MSG_PACKAGED = 4,
	IWC_SES_TRANSACTED = 5,
	IWC_SES_ON_TRANSACTED = 6,
	IWC_END_OF_ENUM = 7,
} CMDQ_IWC_STATE_ENUM;
/**
 * CMDQ secure context struct
 * note it is not global data, each process has its own CMDQ sec context
 */
typedef struct cmdqSecContextStruct {
	struct list_head listEntry;

	/* basic info */
	uint32_t tgid;		/* tgid of procexx context */
	uint32_t referCount;	/* reference count for open cmdq device node */

	/* iwc state */
	CMDQ_IWC_STATE_ENUM state;

	/* iwc information */
	void *iwcMessage;	/* message buffer */
#if defined(CMDQ_SECURE_PATH_SUPPORT)
	KREE_SESSION_HANDLE sessionHandle;
	KREE_SESSION_HANDLE memSessionHandle;
#endif
} cmdqSecContextStruct, *cmdqSecContextHandle;
/**
 * shared memory between normal and secure world
 */
typedef struct cmdqSecSharedMemoryStruct {
	void *pVABase;		/* virtual address of command buffer */
	dma_addr_t MVABase;	/* physical address of command buffer */
	uint32_t size;		/* buffer size */
	cmdqSecContextHandle handle;	/* for alloc path */
	KREE_SHAREDMEM_HANDLE cmdq_share_cookie_handle;
} cmdqSecSharedMemoryStruct, *cmdqSecSharedMemoryHandle;
#endif				/* end of  _CMDQ_IWC_SEC_H_ */
