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

#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/memory.h>
#include "cmdq_record.h"
#include "cmdq_core.h"
#include "cmdq_reg.h"
#include "cmdq_platform.h"
/* #include "ddp_reg.h" */

int32_t cmdq_rec_realloc_addr_metadata_buffer(cmdqRecHandle handle, const uint32_t size)
{
	void *pNewBuf = NULL;
	void *pOriginalBuf = (void *)CMDQ_U32_PTR(handle->secData.addrMetadatas);
	const uint32_t originalSize =
	    sizeof(cmdqSecAddrMetadataStruct) * (handle->secData.addrMetadataMaxCount);

	if (size <= originalSize)
		return 0;


	pNewBuf = kzalloc(size, GFP_KERNEL);
	if (NULL == pNewBuf) {
		CMDQ_ERR("REC: secAddrMetadata, kzalloc %d bytes addr_metadata buffer failed\n",
			 size);
		return -ENOMEM;
	}

	if (pOriginalBuf && originalSize > 0)
		memcpy(pNewBuf, pOriginalBuf, originalSize);


	CMDQ_VERBOSE("REC: secAddrMetadata, realloc size from %d to %d bytes\n", originalSize,
		     size);
	kfree(pOriginalBuf);
	handle->secData.addrMetadatas = (cmdqU32Ptr_t) (unsigned long)(pNewBuf);
	handle->secData.addrMetadataMaxCount = size / sizeof(cmdqSecAddrMetadataStruct);

	return 0;
}

int cmdq_rec_realloc_cmd_buffer(cmdqRecHandle handle, uint32_t size)
{
	void *pNewBuf = NULL;

	if (size <= handle->bufferSize)
		return 0;


	pNewBuf = kzalloc(size, GFP_KERNEL);

	if (NULL == pNewBuf) {
		CMDQ_ERR("REC: kzalloc %d bytes cmd_buffer failed\n", size);
		return -ENOMEM;
	}

	memset(pNewBuf, 0, size);

	if (handle->pBuffer && handle->blockSize > 0)
		memcpy(pNewBuf, handle->pBuffer, handle->blockSize);


	CMDQ_VERBOSE("REC: realloc size from %d to %d bytes\n", handle->bufferSize, size);

	kfree(handle->pBuffer);
	handle->pBuffer = pNewBuf;
	handle->bufferSize = size;

	return 0;
}

int32_t cmdqRecCreate(enum CMDQ_SCENARIO_ENUM scenario, cmdqRecHandle *pHandle)
{
	cmdqRecHandle handle = NULL;

	if (scenario < 0 || scenario >= CMDQ_MAX_SCENARIO_COUNT) {
		CMDQ_ERR("Unknown scenario type %d\n", scenario);
		return -EINVAL;
	}

	handle = kzalloc(sizeof(uint8_t *) * sizeof(struct cmdqRecStruct), GFP_KERNEL);
	if (NULL == handle)
		return -ENOMEM;


	handle->scenario = scenario;
	handle->pBuffer = NULL;
	handle->bufferSize = 0;
	handle->blockSize = 0;
	handle->engineFlag = cmdq_rec_flag_from_scenario(scenario);
	handle->priority = CMDQ_THR_PRIO_NORMAL;
	handle->prefetchCount = 0;
	handle->finalized = false;
	handle->pRunningTask = NULL;

	/* secure path */
	handle->secData.isSecure = false;
	handle->secData.enginesNeedDAPC = 0LL;
	handle->secData.enginesNeedPortSecurity = 0LL;
	handle->secData.addrMetadatas = (cmdqU32Ptr_t) (unsigned long)NULL;
	handle->secData.addrMetadataMaxCount = 0;
	handle->secData.addrMetadataCount = 0;

	if (0 != cmdq_rec_realloc_cmd_buffer(handle, CMDQ_INITIAL_CMD_BLOCK_SIZE)) {
		kfree(handle);
		return -ENOMEM;
	}

	*pHandle = handle;

	return 0;
}

int32_t cmdq_append_addr_metadata(cmdqRecHandle handle, const cmdqSecAddrMetadataStruct *pMetadata)
{
	cmdqSecAddrMetadataStruct *pAddrs;
	int32_t status;
	/* element index of the New appended addr metadat */
	const uint32_t index = handle->secData.addrMetadataCount;

	pAddrs = NULL;
	status = 0;

	if (0 >= handle->secData.addrMetadataMaxCount) {
		/* not init yet, initialize to allow max 8 addr metadata */
		status = cmdq_rec_realloc_addr_metadata_buffer(handle,
							       sizeof(cmdqSecAddrMetadataStruct) *
							       8);
	} else if (handle->secData.addrMetadataCount >= (handle->secData.addrMetadataMaxCount)) {
		/* enlarge metadata buffer to twice as */
		status = cmdq_rec_realloc_addr_metadata_buffer(handle,
							       sizeof(cmdqSecAddrMetadataStruct) *
							       (handle->
								secData.addrMetadataMaxCount) * 2);
	}

	if (0 > status)
		return -ENOMEM;


	pAddrs = (cmdqSecAddrMetadataStruct *) (CMDQ_U32_PTR(handle->secData.addrMetadatas));
	/* append meatadata */
	pAddrs[index].instrIndex = pMetadata->instrIndex;
	pAddrs[index].baseHandle = pMetadata->baseHandle;
	pAddrs[index].offset = pMetadata->offset;
	pAddrs[index].size = pMetadata->size;
	pAddrs[index].port = pMetadata->port;
	pAddrs[index].type = pMetadata->type;

	/* meatadata count ++ */
	handle->secData.addrMetadataCount += 1;
	return 0;
}

int32_t cmdq_append_command(cmdqRecHandle handle, enum CMDQ_CODE_ENUM code, uint32_t argA,
			    uint32_t argB)
{
	int32_t subsys;
	uint32_t *pCommand;

	/* be careful that subsys encoding position is different among platforms */
	const uint32_t subsysBit = cmdq_core_get_subsys_LSB_in_argA();

	if (NULL == handle)
		return -EFAULT;

	pCommand = (uint32_t *) ((uint8_t *) handle->pBuffer + handle->blockSize);

	if (handle->finalized) {
		CMDQ_ERR("Already finalized record 0x%p, cannot add more command", handle);
		return -EBUSY;
	}

	/* check if we have sufficient buffer size */
	/* we leave a 4 instruction (8 bytes each) margin. */
	if ((handle->blockSize + 32) >= handle->bufferSize) {
		if (0 != cmdq_rec_realloc_cmd_buffer(handle, handle->bufferSize * 2))
			return -ENOMEM;

	}
	/* force insert MARKER if prefetch memory is full */
	/* GCE deadlocks if we don't do so */
	if (CMDQ_CODE_EOC != code && cmdq_core_should_enable_prefetch(handle->scenario)) {
		if (handle->prefetchCount >= CMDQ_MAX_PREFETCH_INSTUCTION) {
			CMDQ_MSG("prefetchCount(%d) > MAX_PREFETCH_INSTUCTION, force insert disable prefetch marker\n",
						handle->prefetchCount);
			/* Mark END of prefetch section */
			cmdqRecDisablePrefetch(handle);
			/* BEGING of next prefetch section */
			cmdqRecMark(handle);
		} else {
			/* prefetch enabled marker exist */
			if (1 <= handle->prefetchCount) {
				++handle->prefetchCount;
				CMDQ_VERBOSE("handle->prefetchCount: %d, %s, %d\n",
					     handle->prefetchCount, __func__, __LINE__);
			}
		}
	}
	/* we must re-calculate current PC because we may already insert MARKER inst. */
	pCommand = (uint32_t *) ((uint8_t *) handle->pBuffer + handle->blockSize);

	CMDQ_VERBOSE("REC: 0x%p CMD: 0x%p, op: 0x%02x, argA: 0x%08x, argB: 0x%08x\n", handle,
		     pCommand, code, argA, argB);


	switch (code) {
	case CMDQ_CODE_READ:
		/* argA is the HW register address to read from */
		subsys = cmdq_subsys_from_phys_addr(argA);
		/* argB is the register id to read into */
		/* bit 54: argB type, 1 for GPR */
		*pCommand++ = argB;
		*pCommand++ =
		    (CMDQ_CODE_READ << 24) | (argA & 0xffff) | ((subsys & 0x1f) << subsysBit) | (2
												 <<
												 21);
		break;
	case CMDQ_CODE_MOVE:
		*pCommand++ = argB;
		*pCommand++ = CMDQ_CODE_MOVE << 24 | (argA & 0xffffff);
		break;
	case CMDQ_CODE_WRITE:
		subsys = cmdq_subsys_from_phys_addr(argA);
		if (-1 == subsys) {
			CMDQ_ERR("REC: Unsupported memory base address 0x%08x\n", argA);
			return -EFAULT;
		}

		*pCommand++ = argB;
		*pCommand++ =
		    (CMDQ_CODE_WRITE << 24) | (argA & 0x0FFFF) | ((subsys & 0x01F) << subsysBit);
		break;
	case CMDQ_CODE_POLL:
		subsys = cmdq_subsys_from_phys_addr(argA);
		if (-1 == subsys) {
			CMDQ_ERR("REC: Unsupported memory base address 0x%08x\n", argA);
			return -EFAULT;
		}
		*pCommand++ = argB;
		*pCommand++ =
		    (CMDQ_CODE_POLL << 24) | (argA & 0x0FFFF) | ((subsys & 0x01F) << subsysBit);
		break;
	case CMDQ_CODE_JUMP:
		*pCommand++ = argB;
		*pCommand++ = (CMDQ_CODE_JUMP << 24) | (argA & 0x0FFFFFF);
		break;
	case CMDQ_CODE_WFE:
		/* bit 0-11: wait_value, 1 */
		/* bit 15: to_wait, true */
		/* bit 31: to_update, true */
		/* bit 16-27: update_value, 0 */
		*pCommand++ = ((1 << 31) | (1 << 15) | 1);
		*pCommand++ = (CMDQ_CODE_WFE << 24) | argA;
		break;

	case CMDQ_CODE_SET_TOKEN:
		/* this is actually WFE(SYNC) but with different parameter */
		/* interpretation */
		/* bit 15: to_wait, false */
		/* bit 31: to_update, true */
		/* bit 16-27: update_value, 1 */
		*pCommand++ = ((1 << 31) | (1 << 16));
		*pCommand++ = (CMDQ_CODE_WFE << 24) | argA;
		break;

	case CMDQ_CODE_WAIT_NO_CLEAR:
		/* bit 0-11: wait_value, 1 */
		/* bit 15: to_wait, true */
		/* bit 31: to_update, false */
		*pCommand++ = ((0 << 31) | (1 << 15) | 1);
		*pCommand++ = (CMDQ_CODE_WFE << 24) | argA;
		break;

	case CMDQ_CODE_CLEAR_TOKEN:
		/* this is actually WFE(SYNC) but with different parameter */
		/* interpretation */
		/* bit 15: to_wait, false */
		/* bit 31: to_update, true */
		/* bit 16-27: update_value, 0 */
		*pCommand++ = ((1 << 31) | (0 << 16));
		*pCommand++ = (CMDQ_CODE_WFE << 24) | argA;
		break;

	case CMDQ_CODE_EOC:
		*pCommand++ = argB;
		*pCommand++ = (CMDQ_CODE_EOC << 24) | (argA & 0x0FFFFFF);
		break;

	case CMDQ_CODE_RAW:
		*pCommand++ = argB;
		*pCommand++ = argA;
		break;

	default:
		return -EFAULT;
	}

	handle->blockSize += CMDQ_INST_SIZE;

	return 0;
}

int32_t cmdqRecReset(cmdqRecHandle handle)
{
	if (NULL == handle)
		return -EFAULT;


	if (NULL != handle->pRunningTask)
		cmdqRecStopLoop(handle);


	handle->blockSize = 0;
	handle->prefetchCount = 0;
	handle->finalized = false;

	/*reset secure path data */
	handle->secData.isSecure = false;
	handle->secData.enginesNeedDAPC = 0LL;
	handle->secData.enginesNeedPortSecurity = 0LL;
	if (handle->secData.addrMetadatas) {
		kfree(CMDQ_U32_PTR(handle->secData.addrMetadatas));
		handle->secData.addrMetadatas = (cmdqU32Ptr_t) (unsigned long)NULL;
		handle->secData.addrMetadataMaxCount = 0;
		handle->secData.addrMetadataCount = 0;
	}

	return 0;
}

int32_t cmdqRecSetSecure(cmdqRecHandle handle, const bool isSecure)
{
	if (NULL == handle)
		return -EFAULT;

	if (false == isSecure) {
		handle->secData.isSecure = isSecure;
		return 0;
	}
#ifdef CMDQ_SECURE_PATH_SUPPORT
	CMDQ_VERBOSE("REC: %p secure:%d\n", handle, isSecure);
	handle->secData.isSecure = isSecure;
	return 0;
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
	return -EFAULT;
#endif
}

int32_t cmdqRecSetSecureMode(cmdqRecHandle handle, enum CMDQ_DISP_MODE mode)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (NULL == handle)
		return -EFAULT;

	handle->secData.secMode = mode;
	return 0;
#else
	return -EFAULT;
#endif
}

int32_t cmdqRecSecureEnableDAPC(cmdqRecHandle handle, const uint64_t engineFlag)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (NULL == handle)
		return -EFAULT;

	handle->secData.enginesNeedDAPC |= engineFlag;
	return 0;
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
	return -EFAULT;
#endif
}

int32_t cmdqRecSecureEnablePortSecurity(cmdqRecHandle handle, const uint64_t engineFlag)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (NULL == handle)
		return -EFAULT;

	handle->secData.enginesNeedPortSecurity |= engineFlag;
	return 0;
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
	return -EFAULT;
#endif
}

int32_t cmdqRecMark(cmdqRecHandle handle)
{
	int32_t status;

	/* Do not check prefetch-ability here. */
	/* because cmdqRecMark may have other purposes. */

	/* bit 53: non-suspendable. set to 1 because we don't want */
	/* CPU suspend this thread during pre-fetching. */
	/* If CPU change PC, then there will be a mess, */
	/* because prefetch buffer is not properly cleared. */
	/* bit 48: do not increase CMD_COUNTER (because this is not the end of the task) */
	/* bit 20: prefetch_marker */
	/* bit 17: prefetch_marker_en */
	/* bit 16: prefetch_en */
	/* bit 0:  irq_en (set to 0 since we don't want EOC interrupt) */
	status =
	    cmdq_append_command(handle, CMDQ_CODE_EOC, (0x1 << (53 - 32)) | (0x1 << (48 - 32)),
				0x00130000);

	/* if we're in a prefetch region, */
	/* this ends the region so set count to 0. */
	/* otherwise we start the region by setting count to 1. */
	handle->prefetchCount = 1;

	if (0 != status)
		return -EFAULT;


	return 0;
}

int32_t cmdqRecWrite(cmdqRecHandle handle, uint32_t addr, uint32_t value, uint32_t mask)
{
	int32_t status;

	if (0xFFFFFFFF != mask) {
		status = cmdq_append_command(handle, CMDQ_CODE_MOVE, 0, ~mask);
		if (0 != status)
			return status;


		addr = addr | 0x1;
	}

	status = cmdq_append_command(handle, CMDQ_CODE_WRITE, addr, value);
	if (0 != status)
		return status;


	return 0;
}

int32_t cmdqRecWriteSecure(cmdqRecHandle handle, uint32_t addr,
			   CMDQ_SEC_ADDR_METADATA_TYPE type,
			   uint32_t baseHandle, uint32_t offset, uint32_t size, uint32_t port)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status;
	int32_t writeInstrIndex;
	cmdqSecAddrMetadataStruct metadata;
	const uint32_t mask = 0xFFFFFFFF;

	/* append command */
	status = cmdqRecWrite(handle, addr, baseHandle, mask);
	if (0 != status)
		return status;


	/* append to metadata list */
	writeInstrIndex = (handle->blockSize) / CMDQ_INST_SIZE - 1;	/* start from 0 */

	memset(&metadata, 0, sizeof(cmdqSecAddrMetadataStruct));
	metadata.instrIndex = writeInstrIndex;
	metadata.type = type;
	metadata.baseHandle = baseHandle;
	metadata.offset = offset;
	metadata.size = size;
	metadata.port = port;

	status = cmdq_append_addr_metadata(handle, &metadata);

	return 0;
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
	return -EFAULT;
#endif
}

int32_t cmdqRecWriteSecureMask(cmdqRecHandle handle, uint32_t addr,
			       CMDQ_SEC_ADDR_METADATA_TYPE type, uint32_t value, uint32_t mask)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status;
	int32_t writeInstrIndex;
	cmdqSecAddrMetadataStruct metadata;
	/* const uint32_t mask = 0xFFFFFFFF; */

	/* append command */
	status = cmdqRecWrite(handle, addr, value, mask);
	if (0 != status)
		return status;


	/* append to metadata list */
	writeInstrIndex = (handle->blockSize) / CMDQ_INST_SIZE - 1;	/* start from 0 */

	memset(&metadata, 0, sizeof(cmdqSecAddrMetadataStruct));
	metadata.instrIndex = writeInstrIndex;
	metadata.type = type;
	metadata.baseHandle = value;
	metadata.offset = mask;

	status = cmdq_append_addr_metadata(handle, &metadata);

	return 0;
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
	return -EFAULT;
#endif
}


int32_t cmdqRecPoll(cmdqRecHandle handle, uint32_t addr, uint32_t value, uint32_t mask)
{
	int32_t status;

	status = cmdq_append_command(handle, CMDQ_CODE_MOVE, 0, ~mask);
	if (0 != status)
		return status;


	status = cmdq_append_command(handle, CMDQ_CODE_POLL, (addr | 0x1), value);
	if (0 != status)
		return status;


	return 0;
}


int32_t cmdqRecWait(cmdqRecHandle handle, enum CMDQ_EVENT_ENUM event)
{
	if (CMDQ_SYNC_TOKEN_INVALID == event || CMDQ_SYNC_TOKEN_MAX <= event || 0 > event)
		return -EINVAL;


	return cmdq_append_command(handle, CMDQ_CODE_WFE, event, 0);
}


int32_t cmdqRecWaitNoClear(cmdqRecHandle handle, enum CMDQ_EVENT_ENUM event)
{
	if (CMDQ_SYNC_TOKEN_INVALID == event || CMDQ_SYNC_TOKEN_MAX <= event || 0 > event)
		return -EINVAL;


	return cmdq_append_command(handle, CMDQ_CODE_WAIT_NO_CLEAR, event, 0);
}

int32_t cmdqRecClearEventToken(cmdqRecHandle handle, enum CMDQ_EVENT_ENUM event)
{
	if (CMDQ_SYNC_TOKEN_INVALID == event || CMDQ_SYNC_TOKEN_MAX <= event || 0 > event)
		return -EINVAL;


	return cmdq_append_command(handle, CMDQ_CODE_CLEAR_TOKEN, event, 1	/* actually this param is ignored. */
	    );
}


int32_t cmdqRecSetEventToken(cmdqRecHandle handle, enum CMDQ_EVENT_ENUM event)
{
	if (CMDQ_SYNC_TOKEN_INVALID == event || CMDQ_SYNC_TOKEN_MAX <= event || 0 > event)
		return -EINVAL;


	return cmdq_append_command(handle, CMDQ_CODE_SET_TOKEN, event, 1	/* actually this param is ignored. */
	    );
}

int32_t cmdqRecReadToDataRegister(cmdqRecHandle handle, uint32_t hwRegAddr,
				  enum CMDQ_DATA_REGISTER_ENUM dstDataReg)
{
#ifdef CMDQ_GPR_SUPPORT
	/* read from hwRegAddr(argA) to dstDataReg(argB) */
	return cmdq_append_command(handle, CMDQ_CODE_READ, hwRegAddr, dstDataReg);

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif
}

int32_t cmdqRecWriteFromDataRegister(cmdqRecHandle handle,
				     enum CMDQ_DATA_REGISTER_ENUM srcDataReg, uint32_t hwRegAddr)
{
#ifdef CMDQ_GPR_SUPPORT
	const uint32_t subsys = cmdq_subsys_from_phys_addr(hwRegAddr);
	const uint32_t subsysBit = cmdq_core_get_subsys_LSB_in_argA();

	/* write HW register(argA) with data of GPR data register(argB) */
	return cmdq_append_command(handle,
				   CMDQ_CODE_RAW,
				   ((CMDQ_CODE_WRITE << 24) | (hwRegAddr & 0x0FFFF) |
				    ((subsys & 0x01F) << subsysBit) | (2 << 21)), srcDataReg);
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

/**
 *  Allocate 32-bit register backup slot
 *
 */
int32_t cmdqBackupAllocateSlot(cmdqBackupSlotHandle *phBackupSlot, uint32_t slotCount)
{
#ifdef CMDQ_GPR_SUPPORT

	dma_addr_t paStart = 0;
	int status = 0;

	if (NULL == phBackupSlot)
		return -EINVAL;


	status = cmdqCoreAllocWriteAddress(slotCount, &paStart);
	*phBackupSlot = paStart;

	return status;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

/**
 *  Read 32-bit register backup slot by index
 *
 */
int32_t cmdqBackupReadSlot(cmdqBackupSlotHandle hBackupSlot, uint32_t slotIndex, uint32_t *value)
{
#ifdef CMDQ_GPR_SUPPORT

	if (NULL == value)
		return -EINVAL;


	if (0 == hBackupSlot) {
		CMDQ_ERR("%s, hBackupSlot is NULL\n", __func__);
		return -EINVAL;
	}

	*value = cmdqCoreReadWriteAddress(hBackupSlot + slotIndex * sizeof(uint32_t));

	return 0;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

int32_t cmdqBackupWriteSlot(cmdqBackupSlotHandle hBackupSlot, uint32_t slotIndex, uint32_t value)
{
#ifdef CMDQ_GPR_SUPPORT

	int status = 0;
	/* set the slot value directly */
	status = cmdqCoreWriteWriteAddress(hBackupSlot + slotIndex * sizeof(uint32_t), value);
	return status;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}


/**
 *  Free allocated backup slot. DO NOT free them before corresponding
 *  task finishes. Becareful on AsyncFlush use cases.
 *
 */
int32_t cmdqBackupFreeSlot(cmdqBackupSlotHandle hBackupSlot)
{
#ifdef CMDQ_GPR_SUPPORT
	return cmdqCoreFreeWriteAddress(hBackupSlot);
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

/**
 *  Insert instructions to backup given 32-bit HW register
 *  to a backup slot.
 *  You can use cmdqBackupReadSlot() to retrieve the result
 *  AFTER cmdqRecFlush() returns, or INSIDE the callback of cmdqRecFlushAsyncCallback().
 *
 */
int32_t cmdqRecBackupRegisterToSlot(cmdqRecHandle handle,
				    cmdqBackupSlotHandle hBackupSlot,
				    uint32_t slotIndex, uint32_t regAddr)
{
#ifdef CMDQ_GPR_SUPPORT
	const enum CMDQ_DATA_REGISTER_ENUM valueRegId = CMDQ_DATA_REG_DEBUG;
	const enum CMDQ_DATA_REGISTER_ENUM destRegId = CMDQ_DATA_REG_DEBUG_DST;
	const enum CMDQ_EVENT_ENUM regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;
	const dma_addr_t dramAddr = hBackupSlot + slotIndex * sizeof(uint32_t);

	/* lock GPR because we may access it in multiple CMDQ HW threads */
	cmdqRecWait(handle, regAccessToken);

	/* Load into 32-bit GPR (R0-R15) */
	cmdq_append_command(handle, CMDQ_CODE_READ, regAddr, valueRegId);

	/* Note that <MOVE> argB is 48-bit */
	/* so writeAddress is split into 2 parts */
	/* and we store address in 64-bit GPR (P0-P7) */
	cmdq_append_command(handle, CMDQ_CODE_MOVE,
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
			    ((dramAddr >> 32) & 0xffff) |
#endif
			    ((destRegId & 0x1f) << 16) | (4 << 21), (uint32_t) dramAddr);

	/* write value in GPR to memory pointed by GPR */
	cmdq_append_command(handle,
			    CMDQ_CODE_RAW,
			    (CMDQ_CODE_WRITE << 24) | (0 & 0xffff) | ((destRegId & 0x1f) << 16) | (6
												   <<
												   21),
			    valueRegId);

	/* release the GPR lock */
	cmdqRecSetEventToken(handle, regAccessToken);

	return 0;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

int32_t cmdqRecBackupWriteRegisterFromSlot(cmdqRecHandle handle,
					   cmdqBackupSlotHandle hBackupSlot,
					   uint32_t slotIndex, uint32_t addr)
{
#ifdef CMDQ_GPR_SUPPORT
	const enum CMDQ_DATA_REGISTER_ENUM valueRegId = CMDQ_DATA_REG_DEBUG;
	const enum CMDQ_DATA_REGISTER_ENUM addrRegId = CMDQ_DATA_REG_DEBUG_DST;
	const enum CMDQ_EVENT_ENUM regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;
	const dma_addr_t dramAddr = hBackupSlot + slotIndex * sizeof(uint32_t);
	/* const uint32_t subsysBit = cmdq_core_get_subsys_LSB_in_argA(); */

	/* lock GPR because we may access it in multiple CMDQ HW threads */
	cmdqRecWait(handle, regAccessToken);

	/* 1. MOVE slot address to addr GPR */

	/* Note that <MOVE> argB is 48-bit */
	/* so writeAddress is split into 2 parts */
	/* and we store address in 64-bit GPR (P0-P7) */
	cmdq_append_command(handle, CMDQ_CODE_MOVE,
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
			    ((dramAddr >> 32) & 0xffff) |
#endif
			    ((addrRegId & 0x1f) << 16) | (4 << 21), (uint32_t) dramAddr);	/* argA is GPR */

	/* 2. read value from src address, which is stroed in GPR, to valueRegId */
	cmdq_append_command(handle, CMDQ_CODE_RAW,
			    (CMDQ_CODE_READ << 24) | (0 & 0xffff) | ((addrRegId & 0x1f) << 16) | (6
												  <<
												  21),
			    valueRegId);

	/* 3. write from data register */
	cmdqRecWriteFromDataRegister(handle, valueRegId, addr);

	/* release the GPR lock */
	cmdqRecSetEventToken(handle, regAccessToken);

	return 0;
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

/*copy value to dma memory pointed by hBackupSlot:slotIndex*/
int32_t cmdqRecBackupUpdateSlot(cmdqRecHandle handle,
				cmdqBackupSlotHandle hBackupSlot,
				uint32_t slotIndex, uint32_t value)
{
#ifdef CMDQ_GPR_SUPPORT
	const enum CMDQ_DATA_REGISTER_ENUM valueRegId = CMDQ_DATA_REG_DEBUG;
	const enum CMDQ_DATA_REGISTER_ENUM destRegId = CMDQ_DATA_REG_DEBUG_DST;
	const enum CMDQ_EVENT_ENUM regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;
	const dma_addr_t dramAddr = hBackupSlot + slotIndex * sizeof(uint32_t);

	/* lock GPR because we may access it in multiple CMDQ HW threads */
	cmdqRecWait(handle, regAccessToken);

	/* Assign 32-bit GRP with value */
	/* argA is GPR */
	cmdq_append_command(handle, CMDQ_CODE_RAW,
			    (CMDQ_CODE_MOVE << 24) | (valueRegId << 16) | (4 << 21), value);
	/* Note that <MOVE> argB is 48-bit */
	/* so writeAddress is split into 2 parts */
	/* and we store address in 64-bit GPR (P0-P7) */
	cmdq_append_command(handle, CMDQ_CODE_MOVE,
#ifdef CONFIG_ARCH_DMA_ADDR_T_64BIT
			    ((dramAddr >> 32) & 0xffff) |
#endif
			    ((destRegId & 0x1f) << 16) | (4 << 21), (uint32_t) dramAddr);

	/* write value in GPR to memory pointed by GPR */
	cmdq_append_command(handle,
			    CMDQ_CODE_RAW,
			    (CMDQ_CODE_WRITE << 24) | (0 & 0xffff) | ((destRegId & 0x1f) << 16) | (6
												   <<
												   21),
			    valueRegId);

	/* release the GPR lock */
	cmdqRecSetEventToken(handle, regAccessToken);

	return 0;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */

}

int32_t cmdqRecEnablePrefetch(cmdqRecHandle handle)
{
#if 1/* #ifdef _CMDQ_DISABLE_MARKER_ */
		/* disable pre-fetch marker feature but use auto prefetch mechanism */
		CMDQ_MSG("not allow enable prefetch, scenario: %d\n", handle->scenario);
		return true;
#else
	if (cmdq_core_should_enable_prefetch(handle->scenario)) {
		/* enable prefetch */
		CMDQ_VERBOSE("REC: enable prefetch\n");
		cmdqRecMark(handle);
		return true;
	}
	CMDQ_MSG("not allow enable prefetch, scenario: %d\n", handle->scenario);
	return -EFAULT;
#endif
}

int32_t cmdqRecDisablePrefetch(cmdqRecHandle handle)
{
	uint32_t argB = 0;
	uint32_t argA = 0;
	int32_t status = 0;

	if (!handle->finalized) {
		if (handle->prefetchCount > 0) {
			/* with prefetch threads we should end with */
			/* bit 48: no_inc_exec_cmds_cnt = 1 */
			/* bit 20: prefetch_mark = 1 */
			/* bit 17: prefetch_mark_en = 0 */
			/* bit 16: prefetch_en = 0 */
			argB = 0x00100000;
			argA = (0x1 << 16); /* not increse execute counter */
			/* since we're finalized, no more prefetch */
			handle->prefetchCount = 0;
			status = cmdq_append_command(handle, CMDQ_CODE_EOC, argA, argB);
		}

		if (0 != status)
			return status;

	}

	CMDQ_MSG("cmdqRecDisablePrefetch, status:%d\n", status);
	return status;
}

int32_t cmdq_rec_finalize_command(cmdqRecHandle handle, bool loop)
{
	int32_t status = 0;
	uint32_t argB = 0;

	if (NULL == handle)
		return -EFAULT;

	if (!handle->finalized) {
		if ((handle->prefetchCount > 0) && cmdq_core_should_enable_prefetch(handle->scenario)) {
			CMDQ_ERR
			    ("not insert prefetch disble marker when prefetch enabled, prefetchCount:%d\n",
			     handle->prefetchCount);
			cmdqRecDumpCommand(handle);

			status = -EFAULT;
			return status;
		}

		/* insert EOF instruction */
		argB = 0x1;	/* generate IRQ for each command iteration */
		status = cmdq_append_command(handle, CMDQ_CODE_EOC, 0, argB);

		if (0 != status)
			return status;



		/* insert JUMP to loop to beginning or as a scheduling mark(8) */
		status = cmdq_append_command(handle, CMDQ_CODE_JUMP, 0,	/* not absolute */
					     loop ? -handle->blockSize : 8);
		if (0 != status)
			return status;


		handle->finalized = true;
	}

	return status;
}

int32_t cmdq_rec_setup_sec_data_of_command_desc_by_rec_handle(struct cmdqCommandStruct *pDesc,
							      cmdqRecHandle handle)
{
	/* fill field from user's request */
	pDesc->secData.isSecure = handle->secData.isSecure;
	pDesc->secData.enginesNeedDAPC = handle->secData.enginesNeedDAPC;
	pDesc->secData.enginesNeedPortSecurity = handle->secData.enginesNeedPortSecurity;

	pDesc->secData.addrMetadataCount = handle->secData.addrMetadataCount;
	pDesc->secData.addrMetadatas = handle->secData.addrMetadatas;
	pDesc->secData.addrMetadataMaxCount = handle->secData.addrMetadataMaxCount;
#ifdef CMDQ_SECURE_PATH_SUPPORT
	pDesc->secData.secMode = handle->secData.secMode;
#endif

	/* init reserved field */
	pDesc->secData.resetExecCnt = false;
	pDesc->secData.waitCookie = 0;
	return 0;
}

int32_t cmdqRecFlush(cmdqRecHandle handle)
{
	int32_t status;
	struct cmdqCommandStruct desc = { 0 };

	status = cmdq_rec_finalize_command(handle, false);
	if (status < 0)
		return status;

	CMDQ_MSG("Submit task scenario: %d, priority: %d, engine: 0x%llx, buffer: 0x%p, size: %d\n",
		 handle->scenario, handle->priority, handle->engineFlag, handle->pBuffer,
		 handle->blockSize);

	desc.scenario = handle->scenario;
	desc.priority = handle->priority;
	desc.engineFlag = handle->engineFlag;
	desc.pVABase = (cmdqU32Ptr_t) (unsigned long)handle->pBuffer;
	desc.blockSize = handle->blockSize;
	cmdq_rec_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);	/* secure path */
	return cmdqCoreSubmitTask(&desc);
}

int32_t cmdqRecFlushAndReadRegister(cmdqRecHandle handle, uint32_t regCount, uint32_t *addrArray,
				    uint32_t *valueArray)
{
	int32_t status;
	struct cmdqCommandStruct desc = { 0 };

	status = cmdq_rec_finalize_command(handle, false);
	if (status < 0)
		return status;


	CMDQ_MSG("Submit task scenario: %d, priority: %d, engine: 0x%llx, buffer: 0x%p, size: %d\n",
		 handle->scenario, handle->priority, handle->engineFlag, handle->pBuffer,
		 handle->blockSize);

	desc.scenario = handle->scenario;
	desc.priority = handle->priority;
	desc.engineFlag = handle->engineFlag;
	desc.pVABase = (cmdqU32Ptr_t) (unsigned long)handle->pBuffer;
	desc.blockSize = handle->blockSize;
	desc.regRequest.count = regCount;
	desc.regRequest.regAddresses = (cmdqU32Ptr_t) (unsigned long)addrArray;
	desc.regValue.count = regCount;
	desc.regValue.regValues = (cmdqU32Ptr_t) (unsigned long)valueArray;
	cmdq_rec_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);	/* secure path */

	return cmdqCoreSubmitTask(&desc);
}


int32_t cmdqRecFlushAsync(cmdqRecHandle handle)
{
	int32_t status = 0;
	struct cmdqCommandStruct desc = { 0 };
	struct TaskStruct *pTask = NULL;

	status = cmdq_rec_finalize_command(handle, false);
	if (status < 0)
		return status;


	desc.scenario = handle->scenario;
	desc.priority = handle->priority;
	desc.engineFlag = handle->engineFlag;
	desc.pVABase = (cmdqU32Ptr_t) (unsigned long)handle->pBuffer;
	desc.blockSize = handle->blockSize;
	desc.regRequest.count = 0;
	desc.regRequest.regAddresses = (cmdqU32Ptr_t) (unsigned long)NULL;
	desc.regValue.count = 0;
	desc.regValue.regValues = (cmdqU32Ptr_t) (unsigned long)NULL;
	cmdq_rec_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);	/* secure path */

	status = cmdqCoreSubmitTaskAsync(&desc, NULL, 0, &pTask);

	CMDQ_MSG
	    ("[Auto Release] Submit ASYNC task scenario: %d, priority: %d, engine: 0x%llx, buffer: 0x%p, size: %d\n",
	     handle->scenario, handle->priority, handle->engineFlag, handle->pBuffer,
	     handle->blockSize);

	if (pTask) {
		pTask->flushCallback = NULL;
		pTask->flushData = 0;
	}
	/* insert the task into auto-release queue */
	if (pTask)
		status = cmdqCoreAutoReleaseTask(pTask);
	else
		status = -ENOMEM;

	return status;
}

int32_t cmdqRecFlushAsyncCallback(cmdqRecHandle handle, CmdqAsyncFlushCB callback,
				  uint32_t userData)
{
	int32_t status = 0;
	struct cmdqCommandStruct desc = { 0 };
	struct TaskStruct *pTask = NULL;

	status = cmdq_rec_finalize_command(handle, false);
	if (status < 0)
		return status;


	desc.scenario = handle->scenario;
	desc.priority = handle->priority;
	desc.engineFlag = handle->engineFlag;
	desc.pVABase = (cmdqU32Ptr_t) (unsigned long)handle->pBuffer;
	desc.blockSize = handle->blockSize;
	desc.regRequest.count = 0;
	desc.regRequest.regAddresses = (cmdqU32Ptr_t) (unsigned long)NULL;
	desc.regValue.count = 0;
	desc.regValue.regValues = (cmdqU32Ptr_t) (unsigned long)NULL;
	cmdq_rec_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);	/* secure path */

	status = cmdqCoreSubmitTaskAsync(&desc, NULL, 0, &pTask);

	/* insert the callback here. */
	/* note that, the task may be already completed at this point. */
	if (pTask) {
		pTask->flushCallback = callback;
		pTask->flushData = userData;
	}

	CMDQ_MSG
	    ("[Auto Release] Submit ASYNC task scenario: %d, priority: %d, engine: 0x%llx, buffer: 0x%p, size: %d\n",
	     handle->scenario, handle->priority, handle->engineFlag, handle->pBuffer,
	     handle->blockSize);

	/* insert the task into auto-release queue */
	if (pTask)
		status = cmdqCoreAutoReleaseTask(pTask);
	else
		status = -ENOMEM;


	return status;

}


static int32_t cmdqRecIRQCallback(unsigned long data)
{
	return 0;
}

int32_t cmdqRecStartLoop(cmdqRecHandle handle)
{
	int32_t status = 0;
	struct cmdqCommandStruct desc = { 0 };

	if (NULL == handle)
		return -EFAULT;

	if (NULL != handle->pRunningTask)
		return -EBUSY;


	status = cmdq_rec_finalize_command(handle, true);
	if (status < 0)
		return status;

	/*line over 120 characters,so devide to two statments */
	CMDQ_MSG
	    ("Submit task loop: scenario: %d, priority: %d, engine: 0x%llx, buffer: 0x%p,",
	     handle->scenario, handle->priority, handle->engineFlag, handle->pBuffer);
	CMDQ_MSG
	    (" size: %d, callback: 0x%p, data: %d\n", handle->blockSize, &cmdqRecIRQCallback, 0);

	desc.scenario = handle->scenario;
	desc.priority = handle->priority;
	desc.engineFlag = handle->engineFlag;
	desc.pVABase = (cmdqU32Ptr_t) (unsigned long)handle->pBuffer;
	desc.blockSize = handle->blockSize;
	cmdq_rec_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);	/* secure path */

	status = cmdqCoreSubmitTaskAsync(&desc, &cmdqRecIRQCallback, 0, &handle->pRunningTask);
	return status;
}

int32_t cmdqRecStopLoop(cmdqRecHandle handle)
{
	int32_t status = 0;
	struct TaskStruct *pTask = handle->pRunningTask;

	if (NULL == handle)
		return -EFAULT;

	if (NULL == pTask)
		return -EFAULT;


	status = cmdqCoreReleaseTask(pTask);
	handle->pRunningTask = NULL;
	return status;
}

int32_t cmdqRecGetInstructionCount(cmdqRecHandle handle)
{
	if (NULL == handle)
		return 0;


	return handle->blockSize / CMDQ_INST_SIZE;
}

int32_t cmdqRecDumpCommand(cmdqRecHandle handle)
{
	int32_t status = 0;
	struct TaskStruct *pTask = handle->pRunningTask;

	if (NULL == handle)
		return -EFAULT;

	if (pTask) {
		/* running, so dump from core task direct */
		status = cmdqCoreDebugDumpCommand(pTask);
	} else {
		/* not running, dump from rec->pBuffer */
		const uint32_t *pCmd = NULL;
		static char textBuf[128] = { 0 };
		int i = 0;

		CMDQ_LOG("======REC 0x%p command buffer:\n", handle->pBuffer);
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 16, 4,
			       handle->pBuffer, handle->blockSize, false);

		CMDQ_LOG("======REC 0x%p command buffer END\n", handle->pBuffer);
		CMDQ_LOG("REC 0x%p command buffer TRANSLATED:\n", handle->pBuffer);
		for (i = 0, pCmd = handle->pBuffer; i < handle->blockSize; i += 8, pCmd += 2) {
			cmdq_core_parse_instruction(pCmd, textBuf, 128);
			CMDQ_LOG("%s", textBuf);
		}
		CMDQ_LOG("======REC 0x%p command END\n", handle->pBuffer);

		return 0;
	}

	return status;
}

void cmdqRecDestroy(cmdqRecHandle handle)
{
	if (NULL == handle)
		return;


	if (NULL != handle->pRunningTask)
		cmdqRecStopLoop(handle);

	/* Free command buffer */
	kfree(handle->pBuffer);
	handle->pBuffer = NULL;

	/* Free command handle */
	kfree(handle);
}

#include <linux/delay.h>
int32_t cmdqRecWaitThreadIdleWithTimeout(int threadID, unsigned int retryCount)
{
	int count = 0;
	int idle = 0;
	int threadIsRunning = 0;
	uint32_t currentThreadData = 0;
	struct ThreadStruct *pThread = NULL;

	CMDQ_LOG("func|cmdqRecWaitThreadIdleWithTimeout\n");
	/*
	 ** disp thread 0 ~ 4
	 ** for secure thread 12/13
	 */
	if ((0 > threadID || 4 < threadID) && (12 != threadID) && (13 != threadID)) {
		CMDQ_ERR("invalid theadID[%d]\n", threadID);
		return -1;
	}

	pThread = cmdq_core_getThreadStruct(threadID);
	if (12 != threadID && 13 != threadID) {
		/*for normal thread*/
		while (count < retryCount) {
			currentThreadData = CMDQ_REG_GET32(CMDQ_CURR_LOADED_THR);

			if (currentThreadData & 0x8000)
				threadIsRunning = ((currentThreadData & 0x00FF) == (1 << threadID));
			else
				threadIsRunning = 0;

			idle = (!threadIsRunning &&
					(CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(threadID)) ==
					 CMDQ_REG_GET32(CMDQ_THR_END_ADDR(threadID)))) &&
				pThread->taskCount == 0;
			if (idle)
				break;

			udelay(1000);
			count++;
		}
	} else {
		/*for secure thread*/
		while (count < retryCount) {
			idle = (pThread->taskCount == 0);
			if (idle)
				break;

			udelay(1000);
			count++;
		}
	}

	if (count > 0 && count < retryCount)
		CMDQ_LOG("check hw thread[%d] idle count[%d]\n", threadID, count);
	else if (count >= retryCount) {
		CMDQ_LOG("check hw thread[%d] idle timeout[%d]\n", threadID, count);
		CMDQ_LOG("current load thread = %x status = %x CURR_ADDR = 0x%08x END_ADDR = 0x%08x task count[%d]\n",
				CMDQ_REG_GET32(CMDQ_CURR_LOADED_THR),
				(CMDQ_REG_GET32(CMDQ_THR_CURR_STATUS(threadID))),
				CMDQ_REG_GET32(CMDQ_THR_CURR_ADDR(threadID)),
				CMDQ_REG_GET32(CMDQ_THR_END_ADDR(threadID)),
				pThread->taskCount);
	}
	CMDQ_LOG("func|cmdqRecWaitThreadIdleWithTimeout done\n");

	return 0;
}
