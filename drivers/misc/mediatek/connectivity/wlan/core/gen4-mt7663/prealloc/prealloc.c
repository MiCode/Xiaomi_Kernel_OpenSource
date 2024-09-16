/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2017 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*! \file   prealloc.c
*   \brief  memory preallocation module
*
*    This file contains all implementations of memory preallocation module
*/


/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include "precomp.h"

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
/*
 * -----------------             ----------------           ----------
 * | PRE_MEM_BLOCK |-pItemArray->| PRE_MEM_ITEM |-pvBuffer->| Memory |
 * |---------------|             |--------------|           ----------
 * | PRE_MEM_BLOCK |->...        | PRE_MEM_ITEM |-pvBuffer->----------
 * |---------------|             |--------------|           | Memory |
 *       .                              .                   ----------
 *       .                              .
 *       .                              .
 */
struct PRE_MEM_ITEM {
	void *pvBuffer;
};

struct PRE_MEM_BLOCK {
	uint8_t *pucName;
	struct PRE_MEM_ITEM *pItemArray;
	uint32_t u4Count;
	uint32_t u4Size;
	uint32_t u4KmallocFlags;
	uint32_t u4Curr;
};

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

MODULE_LICENSE("Dual BSD/GPL");

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
static int32_t blockCount;
static struct PRE_MEM_BLOCK arMemBlocks[MEM_ID_NUM];

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
* \brief export function for memory preallocation
*
* \param[in] memId memory id.
*
* \retval void pointer to the memory address
*/
/*----------------------------------------------------------------------------*/
void *preallocGetMem(enum ENUM_MEM_ID memId)
{
	struct PRE_MEM_BLOCK *block = NULL;
	uint32_t curr = 0, count = 0;
	uint8_t *name = NULL;

	/* check memId exist */
	if (memId < 0 || memId >= MEM_ID_NUM) {
		MP_Err("request wrong memId %d", memId);
		return NULL;
	}

	block = &arMemBlocks[memId];
	curr = block->u4Curr;
	count = block->u4Count;
	name = block->pucName;
	block->u4Curr = (curr + 1) % count; /* point to next */

	/* return request memory address */
	MP_Dbg("request [%s], return [%d]\n", name, curr);
	return block->pItemArray[curr].pvBuffer;
}
EXPORT_SYMBOL(preallocGetMem);

static void preallocFree(void)
{
	int32_t i = 0, j = 0;
	struct PRE_MEM_BLOCK *block = NULL;
	struct PRE_MEM_ITEM *items = NULL;
	void *memory = NULL;

	for (i = 0; i < MEM_ID_NUM; i++) {
		block = &arMemBlocks[i];
		MP_Info("free [%d], block name=\"%s\" count=%d size=%d\n",
				i, block->pucName, block->u4Count,
				block->u4Size);
		items = block->pItemArray;
		if (items == NULL)
			continue;
		/* free memory */
		for (j = 0; j < block->u4Count; j++) {
			memory = items[j].pvBuffer;
			MP_Dbg(" - [%d] memory 0x%p\n", j, memory);
			kfree(memory);
		}
		/* free items */
		MP_Dbg(" - items 0x%p\n", items);
		kfree(items);
		memset(block, 0, sizeof(*block));
	}
}

static int preallocAlloc(void)
{
	int32_t i = 0, j = 0;
	struct PRE_MEM_BLOCK *block = NULL;
	struct PRE_MEM_ITEM *items = NULL;
	void *memory = NULL;

	for (i = 0; i < MEM_ID_NUM; i++) {
		block = &arMemBlocks[i];
		MP_Info("allocate [%d] block name=\"%s\" count=%d size=%d\n",
				i, block->pucName, block->u4Count,
				block->u4Size);
		/* allocate u4Count items */
		items = kcalloc(block->u4Count, sizeof(*items), GFP_KERNEL);
		if (items == NULL) {
			MP_Err("allocate [%d] items failed\n", i);
			goto fail;
		}
		MP_Dbg(" + items 0x%p\n", items);
		block->pItemArray = items;
		for (j = 0; j < block->u4Count; j++) {
			/* allocate u4Size memory */
			memory = kmalloc(block->u4Size, block->u4KmallocFlags);
			if (memory == NULL) {
				MP_Err("allocate [%d][%d] memory failed\n",
						i, j);
				goto fail;
			}
			MP_Dbg(" + [%d] memory 0x%p\n", j, memory);
			items[j].pvBuffer = memory;
		}
	}

	return 0;

fail:
	preallocFree();
	return -ENOMEM;
}

static void preallocAddBlock(enum ENUM_MEM_ID memId,
		uint8_t *name, uint32_t count, uint32_t size,
		uint32_t kmallocFlags)
{
	if (memId != blockCount) {
		MP_Err("memId %d != index %d\n", memId, blockCount);
		return;
	}
	arMemBlocks[blockCount].pucName = name;
	arMemBlocks[blockCount].pItemArray = NULL;
	arMemBlocks[blockCount].u4Count = count;
	arMemBlocks[blockCount].u4Size = size;
	arMemBlocks[blockCount].u4KmallocFlags = kmallocFlags;
	arMemBlocks[blockCount].u4Curr = 0;
	blockCount++;
}

static int __init preallocInit(void)
{
	uint32_t u4Size;

	blockCount = 0;

	/* ADD BLOCK START, follow the sequence of ENUM_MEM_ID */
	preallocAddBlock(MEM_ID_NIC_ADAPTER, "NIC ADAPTER MEMORY",
			1, MGT_BUFFER_SIZE,
			GFP_KERNEL);

	u4Size = HIF_TX_COALESCING_BUFFER_SIZE;
	u4Size = u4Size > HIF_RX_COALESCING_BUFFER_SIZE ?
		u4Size : HIF_RX_COALESCING_BUFFER_SIZE;

	preallocAddBlock(MEM_ID_IO_BUFFER, "IO BUFFER",
			 1, u4Size,
			 GFP_KERNEL);

#if defined(_HIF_SDIO)
	preallocAddBlock(MEM_ID_IO_CTRL, "IO CTRL",
			1, sizeof(struct ENHANCE_MODE_DATA_STRUCT),
			GFP_KERNEL);
	preallocAddBlock(MEM_ID_RX_DATA, "RX DATA",
			HIF_RX_COALESCING_BUF_COUNT,
			HIF_RX_COALESCING_BUFFER_SIZE,
			GFP_KERNEL);
#endif
#if defined(_HIF_USB)
	preallocAddBlock(MEM_ID_TX_CMD, "TX CMD",
			USB_REQ_TX_CMD_CNT, USB_TX_CMD_BUF_SIZE,
			GFP_KERNEL);
	preallocAddBlock(MEM_ID_TX_DATA_FFA, "TX DATA FFA",
			USB_REQ_TX_DATA_FFA_CNT, USB_TX_DATA_BUFF_SIZE,
			GFP_KERNEL);
#if CFG_USB_TX_AGG
	preallocAddBlock(MEM_ID_TX_DATA, "TX AGG DATA",
			(USB_TC_NUM * USB_REQ_TX_DATA_CNT),
			USB_TX_DATA_BUFF_SIZE,
			GFP_KERNEL);
#else
	preallocAddBlock(MEM_ID_TX_DATA, "TX DATA",
			USB_REQ_TX_DATA_CNT, USB_TX_DATA_BUFF_SIZE,
			GFP_KERNEL);
#endif
	preallocAddBlock(MEM_ID_RX_EVENT, "RX EVENT",
			USB_REQ_RX_EVENT_CNT, USB_RX_EVENT_BUF_SIZE,
			GFP_KERNEL);
	preallocAddBlock(MEM_ID_RX_DATA, "RX DATA",
			USB_REQ_RX_DATA_CNT, USB_RX_DATA_BUF_SIZE,
			GFP_KERNEL);
#endif
	/* ADD BLOCK END */

	return preallocAlloc();
}

static void __exit preallocExit(void)
{
	preallocFree();
}

module_init(preallocInit);
module_exit(preallocExit);
