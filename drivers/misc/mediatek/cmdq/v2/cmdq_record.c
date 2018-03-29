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
#include <linux/vmalloc.h>
#include <linux/errno.h>
#include <linux/memory.h>
#include <mt-plat/mt_lpae.h>

#include "cmdq_record.h"
#include "cmdq_core.h"
#include "cmdq_virtual.h"
#include "cmdq_reg.h"
#include "cmdq_prof.h"

#if defined(CMDQ_SECURE_PATH_SUPPORT) || defined(CONFIG_MTK_CMDQ_TAB)
#include "cmdq_sec_iwc_common.h"
#endif

#ifdef _MTK_USER_
#define DISABLE_LOOP_IRQ
#endif

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

	pNewBuf = vzalloc(size);

	if (NULL == pNewBuf) {
		CMDQ_ERR("REC: kzalloc %d bytes cmd_buffer failed\n", size);
		return -ENOMEM;
	}

	memset(pNewBuf, 0, size);

	if (handle->pBuffer && handle->blockSize > 0)
		memcpy(pNewBuf, handle->pBuffer, handle->blockSize);

	CMDQ_VERBOSE("REC: realloc size from %d to %d bytes\n", handle->bufferSize, size);

	vfree(handle->pBuffer);
	handle->pBuffer = pNewBuf;
	handle->bufferSize = size;

	return 0;
}

static int32_t cmdq_reset_profile_maker_data(cmdqRecHandle handle)
{
#ifdef CMDQ_PROFILE_MARKER_SUPPORT
	int32_t i = 0;

	if (NULL == handle)
		return -EFAULT;

	handle->profileMarker.count = 0;
	handle->profileMarker.hSlot = 0LL;

	for (i = 0; i < CMDQ_MAX_PROFILE_MARKER_IN_TASK; i++)
		handle->profileMarker.tag[i] = (cmdqU32Ptr_t) (unsigned long)(NULL);

	return 0;
#endif
	return 0;
}

int32_t cmdq_task_create(CMDQ_SCENARIO_ENUM scenario, cmdqRecHandle *pHandle)
{
	cmdqRecHandle handle = NULL;

	if (pHandle == NULL) {
		CMDQ_ERR("Invalid empty handle\n");
		return -EINVAL;
	}

	*pHandle = NULL;

	if (scenario < 0 || scenario >= CMDQ_MAX_SCENARIO_COUNT) {
		CMDQ_ERR("Unknown scenario type %d\n", scenario);
		return -EINVAL;
	}

	handle = kzalloc(sizeof(cmdqRecStruct), GFP_KERNEL);
	if (NULL == handle)
		return -ENOMEM;

	handle->scenario = scenario;
	handle->pBuffer = NULL;
	handle->bufferSize = 0;
	handle->blockSize = 0;
	handle->engineFlag = cmdq_get_func()->flagFromScenario(scenario);
	handle->priority = CMDQ_THR_PRIO_NORMAL;
	handle->prefetchCount = 0;
	handle->finalized = false;
	handle->pRunningTask = NULL;

	/* secure path */
	handle->secData.is_secure = false;
	handle->secData.enginesNeedDAPC = 0LL;
	handle->secData.enginesNeedPortSecurity = 0LL;
	handle->secData.addrMetadatas = (cmdqU32Ptr_t) (unsigned long)NULL;
	handle->secData.addrMetadataMaxCount = 0;
	handle->secData.addrMetadataCount = 0;

	/* profile marker */
	cmdq_reset_profile_maker_data(handle);

	/* CMD */
	if (0 != cmdq_rec_realloc_cmd_buffer(handle, CMDQ_INITIAL_CMD_BLOCK_SIZE)) {
		kfree(handle);
		return -ENOMEM;
	}

	*pHandle = handle;

	return 0;
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
int32_t cmdq_append_addr_metadata(cmdqRecHandle handle, const cmdqSecAddrMetadataStruct *pMetadata)
{
	cmdqSecAddrMetadataStruct *pAddrs;
	int32_t status;
	uint32_t size;
	/* element index of the New appended addr metadat */
	const uint32_t index = handle->secData.addrMetadataCount;

	pAddrs = NULL;
	status = 0;

	if (0 >= handle->secData.addrMetadataMaxCount) {
		/* not init yet, initialize to allow max 8 addr metadata */
		size = sizeof(cmdqSecAddrMetadataStruct) * 8;
		status = cmdq_rec_realloc_addr_metadata_buffer(handle, size);
	} else if (handle->secData.addrMetadataCount >= (handle->secData.addrMetadataMaxCount)) {
		/* enlarge metadata buffer to twice as */
		size =
		    sizeof(cmdqSecAddrMetadataStruct) * (handle->secData.addrMetadataMaxCount) * 2;
		status = cmdq_rec_realloc_addr_metadata_buffer(handle, size);
	}

	if (0 > status)
		return -ENOMEM;

	if (handle->secData.addrMetadataCount >= CMDQ_IWC_MAX_ADDR_LIST_LENGTH) {
		uint32_t maxMetaDataCount = CMDQ_IWC_MAX_ADDR_LIST_LENGTH;

		CMDQ_ERR("Metadata idx = %d reach the max allowed number = %d.\n",
			 handle->secData.addrMetadataCount, maxMetaDataCount);
		CMDQ_MSG("ADDR: type:%d, baseHandle:%x, offset:%d, size:%d, port:%d\n",
			 pMetadata->type, pMetadata->baseHandle, pMetadata->offset, pMetadata->size,
			 pMetadata->port);
		status = -EFAULT;
	} else {
		pAddrs =
		    (cmdqSecAddrMetadataStruct *) (CMDQ_U32_PTR(handle->secData.addrMetadatas));
		/* append meatadata */
		pAddrs[index].instrIndex = pMetadata->instrIndex;
		pAddrs[index].baseHandle = pMetadata->baseHandle;
		pAddrs[index].offset = pMetadata->offset;
		pAddrs[index].size = pMetadata->size;
		pAddrs[index].port = pMetadata->port;
		pAddrs[index].type = pMetadata->type;

		/* meatadata count ++ */
		handle->secData.addrMetadataCount += 1;
	}

	return status;
}
#endif

int32_t cmdq_check_before_append(cmdqRecHandle handle)
{
	if (NULL == handle)
		return -EFAULT;

	if (handle->finalized) {
		CMDQ_ERR("Finalized record 0x%p (scenario:%d)\n", handle, handle->scenario);
		return -EBUSY;
	}

	/* check if we have sufficient buffer size */
	/* we leave a 4 instruction (4 bytes each) margin. */
	if ((handle->blockSize + 32) >= handle->bufferSize) {
		if (0 != cmdq_rec_realloc_cmd_buffer(handle, handle->bufferSize * 2))
			return -ENOMEM;
	}

	return 0;
}

/**
 * centralize the write/polling/read command for APB and GPR handle
 * this function must be called inside cmdq_append_command
 * because we ignore buffer and pre-fetch check here.
 * Parameter:
 *     same as cmdq_append_command
 * Return:
 *     same as cmdq_append_command
 */
static int32_t cmdq_append_wpr_command(cmdqRecHandle handle, CMDQ_CODE_ENUM code,
				       uint32_t arg_a, uint32_t arg_b, uint32_t arg_a_type,
				       uint32_t arg_b_type)
{
	int32_t status = 0;
	int32_t subsys;
	uint32_t *p_command;
	bool bUseGPR = false;
	/* use new arg_a to present final inserted arg_a */
	uint32_t new_arg_a;
	uint32_t new_arg_a_type = arg_a_type;
	uint32_t arg_type = 0;

	/* be careful that subsys encoding position is different among platforms */
	const uint32_t subsys_bit = cmdq_get_func()->getSubsysLSBArgA();

	if (CMDQ_CODE_READ != code && CMDQ_CODE_WRITE != code && CMDQ_CODE_POLL != code) {
		CMDQ_ERR("Record 0x%p, flow error, should not append comment in wpr API", handle);
		return -EFAULT;
	}

	/* we must re-calculate current PC at first. */
	p_command = (uint32_t *) ((uint8_t *) handle->pBuffer + handle->blockSize);

	CMDQ_VERBOSE("REC: 0x%p CMD: 0x%p, op: 0x%02x\n", handle, p_command, code);
	CMDQ_VERBOSE("REC: 0x%p CMD: arg_a: 0x%08x, arg_b: 0x%08x, arg_a_type: %d, arg_b_type: %d\n",
		     handle, arg_a, arg_b, arg_a_type, arg_b_type);

	if (0 == arg_a_type) {
		/* arg_a is the HW register address to read from */
		subsys = cmdq_core_subsys_from_phys_addr(arg_a);
		if (CMDQ_SPECIAL_SUBSYS_ADDR == subsys) {
#ifdef CMDQ_GPR_SUPPORT
			bUseGPR = true;
			CMDQ_MSG("REC: Special handle memory base address 0x%08x\n", arg_a);
			/* Wait and clear for GPR mutex token to enter mutex */
			*p_command++ = ((1 << 31) | (1 << 15) | 1);
			*p_command++ = (CMDQ_CODE_WFE << 24) | CMDQ_SYNC_TOKEN_GPR_SET_4;
			handle->blockSize += CMDQ_INST_SIZE;
			/* Move extra handle APB address to GPR */
			*p_command++ = arg_a;
			*p_command++ = (CMDQ_CODE_MOVE << 24) |
			    ((CMDQ_DATA_REG_DEBUG & 0x1f) << 16) | (4 << 21);
			handle->blockSize += CMDQ_INST_SIZE;
			/* change final arg_a to GPR */
			new_arg_a = ((CMDQ_DATA_REG_DEBUG & 0x1f) << subsys_bit);
			if (arg_a & 0x1) {
				/* MASK case, set final bit to 1 */
				new_arg_a = new_arg_a | 0x1;
			}
			/* change arg_a type to 1 */
			new_arg_a_type = 1;
#else
			CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
			status = -EFAULT;
#endif
		} else if (0 == arg_a_type && 0 > subsys) {
			CMDQ_ERR("REC: Unsupported memory base address 0x%08x\n", arg_a);
			status = -EFAULT;
		} else {
			/* compose final arg_a according to subsys table */
			new_arg_a = (arg_a & 0xffff) | ((subsys & 0x1f) << subsys_bit);
		}
	} else {
		/* compose final arg_a according GPR value */
		new_arg_a = ((arg_a & 0x1f) << subsys_bit);
	}

	if (status < 0)
		return status;

	arg_type = (new_arg_a_type << 2) | (arg_b_type << 1);

	/* new_arg_a is the HW register address to access from or GPR value store the HW register address */
	/* arg_b is the value or register id  */
	/* bit 55: arg_a type, 1 for GPR */
	/* bit 54: arg_b type, 1 for GPR */
	/* argType: ('new_arg_a_type', 'arg_b_type', '0') */
	*p_command++ = arg_b;
	*p_command++ = (code << 24) | new_arg_a | (arg_type << 21);
	handle->blockSize += CMDQ_INST_SIZE;

	if (bUseGPR) {
		/* Set for GPR mutex token to leave mutex */
		*p_command++ = ((1 << 31) | (1 << 16));
		*p_command++ = (CMDQ_CODE_WFE << 24) | CMDQ_SYNC_TOKEN_GPR_SET_4;
		handle->blockSize += CMDQ_INST_SIZE;
	}
	return 0;
}

int32_t cmdq_append_command(cmdqRecHandle handle, CMDQ_CODE_ENUM code,
			    uint32_t arg_a, uint32_t arg_b, uint32_t arg_a_type, uint32_t arg_b_type)
{
	int32_t status;
	uint32_t *p_command;

	status = cmdq_check_before_append(handle);
	if (status < 0) {
		CMDQ_ERR("	  cannot add command (op: 0x%02x, arg_a: 0x%08x, arg_b: 0x%08x)\n",
			code, arg_a, arg_b);
		return status;
	}

	/* force insert MARKER if prefetch memory is full */
	/* GCE deadlocks if we don't do so */
	if (CMDQ_CODE_EOC != code && cmdq_get_func()->shouldEnablePrefetch(handle->scenario)) {
		uint32_t prefetchSize = 0;
		int32_t threadNo = cmdq_get_func()->getThreadID(handle->scenario, handle->secData.is_secure);

		prefetchSize = cmdq_core_thread_prefetch_size(threadNo);
		if (prefetchSize > 0 && handle->prefetchCount >= prefetchSize) {
			CMDQ_MSG
			    ("prefetchCount(%d) > %d, force insert disable prefetch marker\n",
			     handle->prefetchCount, prefetchSize);
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
	p_command = (uint32_t *) ((uint8_t *) handle->pBuffer + handle->blockSize);

	CMDQ_VERBOSE("REC: 0x%p CMD: 0x%p, op: 0x%02x, arg_a: 0x%08x, arg_b: 0x%08x\n", handle,
		     p_command, code, arg_a, arg_b);

	switch (code) {
	case CMDQ_CODE_READ:
	case CMDQ_CODE_WRITE:
	case CMDQ_CODE_POLL:
		/* Because read/write/poll have similar format, handle them together */
		return cmdq_append_wpr_command(handle, code, arg_a, arg_b, arg_a_type, arg_b_type);
	case CMDQ_CODE_MOVE:
		*p_command++ = arg_b;
		*p_command++ = CMDQ_CODE_MOVE << 24 | (arg_a & 0xffffff);
		break;
	case CMDQ_CODE_JUMP:
		*p_command++ = arg_b;
		*p_command++ = (CMDQ_CODE_JUMP << 24) | (arg_a & 0x0FFFFFF);
		break;
	case CMDQ_CODE_WFE:
		/* bit 0-11: wait_value, 1 */
		/* bit 15: to_wait, true */
		/* bit 31: to_update, true */
		/* bit 16-27: update_value, 0 */
		*p_command++ = ((1 << 31) | (1 << 15) | 1);
		*p_command++ = (CMDQ_CODE_WFE << 24) | arg_a;
		break;

	case CMDQ_CODE_SET_TOKEN:
		/* this is actually WFE(SYNC) but with different parameter */
		/* interpretation */
		/* bit 15: to_wait, false */
		/* bit 31: to_update, true */
		/* bit 16-27: update_value, 1 */
		*p_command++ = ((1 << 31) | (1 << 16));
		*p_command++ = (CMDQ_CODE_WFE << 24) | arg_a;
		break;

	case CMDQ_CODE_WAIT_NO_CLEAR:
		/* bit 0-11: wait_value, 1 */
		/* bit 15: to_wait, true */
		/* bit 31: to_update, false */
		*p_command++ = ((0 << 31) | (1 << 15) | 1);
		*p_command++ = (CMDQ_CODE_WFE << 24) | arg_a;
		break;

	case CMDQ_CODE_CLEAR_TOKEN:
		/* this is actually WFE(SYNC) but with different parameter */
		/* interpretation */
		/* bit 15: to_wait, false */
		/* bit 31: to_update, true */
		/* bit 16-27: update_value, 0 */
		*p_command++ = ((1 << 31) | (0 << 16));
		*p_command++ = (CMDQ_CODE_WFE << 24) | arg_a;
		break;

	case CMDQ_CODE_EOC:
		*p_command++ = arg_b;
		*p_command++ = (CMDQ_CODE_EOC << 24) | (arg_a & 0x0FFFFFF);
		break;

	case CMDQ_CODE_RAW:
		*p_command++ = arg_b;
		*p_command++ = arg_a;
		break;

	default:
		return -EFAULT;
	}

	handle->blockSize += CMDQ_INST_SIZE;
	return 0;
}

int32_t cmdq_task_set_engine(cmdqRecHandle handle, uint64_t engineFlag)
{
	if (NULL == handle)
		return -EFAULT;

	CMDQ_VERBOSE("REC: %p, engineFlag: 0x%llx\n", handle, engineFlag);
	handle->engineFlag = engineFlag;

	return 0;
}

int32_t cmdq_task_reset(cmdqRecHandle handle)
{
	if (NULL == handle)
		return -EFAULT;

	if (NULL != handle->pRunningTask)
		cmdq_task_stop_loop(handle);

	handle->blockSize = 0;
	handle->prefetchCount = 0;
	handle->finalized = false;

	/* reset secure path data */
	handle->secData.is_secure = false;
	handle->secData.enginesNeedDAPC = 0LL;
	handle->secData.enginesNeedPortSecurity = 0LL;
	if (handle->secData.addrMetadatas) {
		kfree(CMDQ_U32_PTR(handle->secData.addrMetadatas));
		handle->secData.addrMetadatas = (cmdqU32Ptr_t) (unsigned long)NULL;
		handle->secData.addrMetadataMaxCount = 0;
		handle->secData.addrMetadataCount = 0;
	}

	/* profile marker */
	cmdq_reset_profile_maker_data(handle);

	return 0;
}

int32_t cmdq_task_set_secure(cmdqRecHandle handle, const bool is_secure)
{
	if (NULL == handle)
		return -EFAULT;

	if (false == is_secure) {
		handle->secData.is_secure = is_secure;
		return 0;
	}
#ifdef CMDQ_SECURE_PATH_SUPPORT
	CMDQ_VERBOSE("REC: %p secure:%d\n", handle, is_secure);
	handle->secData.is_secure = is_secure;
	return 0;
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
	return -EFAULT;
#endif
}

int32_t cmdq_task_is_secure(cmdqRecHandle handle)
{
	if (NULL == handle)
		return -EFAULT;

	return handle->secData.is_secure;
}

#ifdef CONFIG_MTK_CMDQ_TAB
int32_t cmdq_task_set_secure_mode(cmdqRecHandle handle, enum CMDQ_DISP_MODE mode)
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
#endif

int32_t cmdq_task_secure_enable_dapc(cmdqRecHandle handle, const uint64_t engineFlag)
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

int32_t cmdq_task_secure_enable_port_security(cmdqRecHandle handle, const uint64_t engineFlag)
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

int32_t cmdq_op_write_reg(cmdqRecHandle handle, uint32_t addr,
				   CMDQ_VARIABLE argument, uint32_t mask)
{
	int32_t status = 0;
	CMDQ_CODE_ENUM op_code;
	uint32_t arg_b_i, arg_b_type;

	if (0xFFFFFFFF != mask) {
		status = cmdq_append_command(handle, CMDQ_CODE_MOVE, 0, ~mask, 0, 0);
		if (0 != status)
			return status;
	}

	if (0xFFFFFFFF != mask)
		addr = addr | 0x1;

	op_code = CMDQ_CODE_WRITE;
	arg_b_type = 0;
	arg_b_i = (uint32_t)(argument & 0xFFFFFFFF);

	return cmdq_append_command(handle, op_code, addr, arg_b_i, 0, arg_b_type);
}

int32_t cmdq_op_write_reg_secure(cmdqRecHandle handle, uint32_t addr,
			   CMDQ_SEC_ADDR_METADATA_TYPE type, uint32_t baseHandle,
			   uint32_t offset, uint32_t size, uint32_t port)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	int32_t status;
	int32_t writeInstrIndex;
	cmdqSecAddrMetadataStruct metadata;
	const uint32_t mask = 0xFFFFFFFF;

	/* append command */
	status = cmdq_op_write_reg(handle, addr, baseHandle, mask);
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

#ifdef CONFIG_MTK_CMDQ_TAB
int32_t cmdq_op_write_reg_secure_mask(cmdqRecHandle handle, uint32_t addr,
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
#endif

int32_t cmdq_op_poll(cmdqRecHandle handle, uint32_t addr, uint32_t value, uint32_t mask)
{
	int32_t status;

	status = cmdq_append_command(handle, CMDQ_CODE_MOVE, 0, ~mask, 0, 0);
	if (0 != status)
		return status;

	status = cmdq_append_command(handle, CMDQ_CODE_POLL, (addr | 0x1), value, 0, 0);
	if (0 != status)
		return status;

	return 0;
}

int32_t cmdq_op_wait(cmdqRecHandle handle, CMDQ_EVENT_ENUM event)
{
	int32_t arg_a;

	if (0 > event || CMDQ_SYNC_TOKEN_MAX <= event)
		return -EINVAL;

	arg_a = cmdq_core_get_event_value(event);
	if (arg_a < 0)
		return -EINVAL;

	return cmdq_append_command(handle, CMDQ_CODE_WFE, arg_a, 0, 0, 0);
}

int32_t cmdq_op_wait_no_clear(cmdqRecHandle handle, CMDQ_EVENT_ENUM event)
{
	int32_t arg_a;

	if (0 > event || CMDQ_SYNC_TOKEN_MAX <= event)
		return -EINVAL;

	arg_a = cmdq_core_get_event_value(event);
	if (arg_a < 0)
		return -EINVAL;

	return cmdq_append_command(handle, CMDQ_CODE_WAIT_NO_CLEAR, arg_a, 0, 0, 0);
}

int32_t cmdq_op_clear_event(cmdqRecHandle handle, CMDQ_EVENT_ENUM event)
{
	int32_t arg_a;

	if (0 > event || CMDQ_SYNC_TOKEN_MAX <= event)
		return -EINVAL;

	arg_a = cmdq_core_get_event_value(event);
	if (arg_a < 0)
		return -EINVAL;

	return cmdq_append_command(handle, CMDQ_CODE_CLEAR_TOKEN, arg_a, 1,	/* actually this param is ignored. */
				   0, 0);
}

int32_t cmdq_op_set_event(cmdqRecHandle handle, CMDQ_EVENT_ENUM event)
{
	int32_t arg_a;

	if (0 > event || CMDQ_SYNC_TOKEN_MAX <= event)
		return -EINVAL;

	arg_a = cmdq_core_get_event_value(event);
	if (arg_a < 0)
		return -EINVAL;

	return cmdq_append_command(handle, CMDQ_CODE_SET_TOKEN, arg_a, 1,	/* actually this param is ignored. */
				   0, 0);
}

int32_t cmdq_op_read_to_data_register(cmdqRecHandle handle, uint32_t hw_addr,
				  CMDQ_DATA_REGISTER_ENUM dst_data_reg)
{
#ifdef CMDQ_GPR_SUPPORT
	CMDQ_CODE_ENUM op_code;
	uint32_t arg_a_i, arg_b_i;
	uint32_t arg_a_type, arg_b_type;

	op_code = CMDQ_CODE_READ;
	arg_a_i = hw_addr;
	arg_a_type = 0;
	arg_b_i = dst_data_reg;
	arg_b_type = 1;

	/* read from hwRegAddr(arg_a) to dstDataReg(arg_b) */
	return cmdq_append_command(handle, op_code, arg_a_i, arg_b_i, arg_a_type, arg_b_type);
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif
}

int32_t cmdq_op_write_from_data_register(cmdqRecHandle handle,
				     CMDQ_DATA_REGISTER_ENUM src_data_reg, uint32_t hw_addr)
{
#ifdef CMDQ_GPR_SUPPORT
	CMDQ_CODE_ENUM op_code;
	uint32_t arg_b_i;

	op_code = CMDQ_CODE_WRITE;
	arg_b_i = src_data_reg;

	/* write HW register(arg_a) with data of GPR data register(arg_b) */
	return cmdq_append_command(handle, op_code, hw_addr, arg_b_i, 0, 1);
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

/**
 *  Allocate 32-bit register backup slot
 *
 */
int32_t cmdq_alloc_mem(cmdqBackupSlotHandle *p_h_backup_slot, uint32_t slotCount)
{
#ifdef CMDQ_GPR_SUPPORT

	dma_addr_t paStart = 0;
	int status = 0;

	if (NULL == p_h_backup_slot)
		return -EINVAL;

	status = cmdqCoreAllocWriteAddress(slotCount, &paStart);
	*p_h_backup_slot = paStart;

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
int32_t cmdq_cpu_read_mem(cmdqBackupSlotHandle h_backup_slot, uint32_t slot_index,
			   uint32_t *value)
{
#ifdef CMDQ_GPR_SUPPORT

	if (NULL == value)
		return -EINVAL;

	if (0 == h_backup_slot) {
		CMDQ_ERR("%s, h_backup_slot is NULL\n", __func__);
		return -EINVAL;
	}

	*value = cmdqCoreReadWriteAddress(h_backup_slot + slot_index * sizeof(uint32_t));

	return 0;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

int32_t cmdq_cpu_write_mem(cmdqBackupSlotHandle h_backup_slot, uint32_t slot_index,
			    uint32_t value)
{
#ifdef CMDQ_GPR_SUPPORT

	int status = 0;
	/* set the slot value directly */
	status = cmdqCoreWriteWriteAddress(h_backup_slot + slot_index * sizeof(uint32_t), value);
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
int32_t cmdq_free_mem(cmdqBackupSlotHandle h_backup_slot)
{
#ifdef CMDQ_GPR_SUPPORT
	return cmdqCoreFreeWriteAddress(h_backup_slot);
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

/**
 *  Insert instructions to backup given 32-bit HW register
 *  to a backup slot.
 *  You can use cmdq_cpu_read_mem() to retrieve the result
 *  AFTER cmdq_task_flush() returns, or INSIDE the callback of cmdq_task_flush_async_callback().
 *
 */
int32_t cmdq_op_read_reg_to_mem(cmdqRecHandle handle,
			    cmdqBackupSlotHandle h_backup_slot, uint32_t slot_index, uint32_t addr)
{
#ifdef CMDQ_GPR_SUPPORT
	const CMDQ_DATA_REGISTER_ENUM valueRegId = CMDQ_DATA_REG_DEBUG;
	const CMDQ_DATA_REGISTER_ENUM destRegId = CMDQ_DATA_REG_DEBUG_DST;
	const CMDQ_EVENT_ENUM regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;
	const dma_addr_t dramAddr = h_backup_slot + slot_index * sizeof(uint32_t);
	uint32_t highAddr = 0;

	/* lock GPR because we may access it in multiple CMDQ HW threads */
	cmdq_op_wait(handle, regAccessToken);

	if (cmdq_core_subsys_from_phys_addr(addr) != CMDQ_SPECIAL_SUBSYS_ADDR) {
		/* Load into 32-bit GPR (R0-R15) */
		cmdq_append_command(handle, CMDQ_CODE_READ, addr, valueRegId, 0, 1);
	} else {
		/*
		 * for special sw subsys addr,
		 * we don't read directly due to append command will acquire
		 * CMDQ_SYNC_TOKEN_GPR_SET_4 event again.
		 */

		/* set GPR to address */
		cmdq_append_command(handle, CMDQ_CODE_MOVE, valueRegId, addr, 0, 0);

		/* read data from address in GPR to GPR */
		cmdq_append_command(handle, CMDQ_CODE_READ, valueRegId, valueRegId, 1, 1);
	}

	/* Note that <MOVE> arg_b is 48-bit */
	/* so writeAddress is split into 2 parts */
	/* and we store address in 64-bit GPR (P0-P7) */
	CMDQ_GET_HIGH_ADDR(dramAddr, highAddr);
	cmdq_append_command(handle, CMDQ_CODE_MOVE,
			    highAddr |
			    ((destRegId & 0x1f) << 16) | (4 << 21), (uint32_t) dramAddr, 0, 0);

	/* write value in GPR to memory pointed by GPR */
	cmdq_append_command(handle, CMDQ_CODE_WRITE, destRegId, valueRegId, 1, 1);
	/* release the GPR lock */
	cmdq_op_set_event(handle, regAccessToken);

	return 0;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

int32_t cmdq_op_read_mem_to_reg(cmdqRecHandle handle,
			    cmdqBackupSlotHandle h_backup_slot, uint32_t slot_index, uint32_t addr)
{
#ifdef CMDQ_GPR_SUPPORT
	const CMDQ_DATA_REGISTER_ENUM valueRegId = CMDQ_DATA_REG_DEBUG;
	const CMDQ_DATA_REGISTER_ENUM addrRegId = CMDQ_DATA_REG_DEBUG_DST;
	const CMDQ_EVENT_ENUM regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;
	const dma_addr_t dramAddr = h_backup_slot + slot_index * sizeof(uint32_t);
	uint32_t highAddr = 0;

	/* lock GPR because we may access it in multiple CMDQ HW threads */
	cmdq_op_wait(handle, regAccessToken);

	/* 1. MOVE slot address to addr GPR */

	/* Note that <MOVE> arg_b is 48-bit */
	/* so writeAddress is split into 2 parts */
	/* and we store address in 64-bit GPR (P0-P7) */
	CMDQ_GET_HIGH_ADDR(dramAddr, highAddr);
	cmdq_append_command(handle, CMDQ_CODE_MOVE,
			    highAddr |
			    ((addrRegId & 0x1f) << 16) | (4 << 21), (uint32_t) dramAddr, 0, 0);	/* arg_a is GPR */

	/* 2. read value from src address, which is stroed in GPR, to valueRegId */
	cmdq_append_command(handle, CMDQ_CODE_READ, addrRegId, valueRegId, 1, 1);

	/* 3. write from data register */
	cmdq_op_write_from_data_register(handle, valueRegId, addr);

	/* release the GPR lock */
	cmdq_op_set_event(handle, regAccessToken);

	return 0;
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

int32_t cmdq_op_write_mem(cmdqRecHandle handle, cmdqBackupSlotHandle h_backup_slot,
			    uint32_t slot_index, uint32_t value)
{
#ifdef CMDQ_GPR_SUPPORT
	const CMDQ_DATA_REGISTER_ENUM valueRegId = CMDQ_DATA_REG_DEBUG;
	const CMDQ_DATA_REGISTER_ENUM destRegId = CMDQ_DATA_REG_DEBUG_DST;
	const CMDQ_EVENT_ENUM regAccessToken = CMDQ_SYNC_TOKEN_GPR_SET_4;
	const dma_addr_t dramAddr = h_backup_slot + slot_index * sizeof(uint32_t);
	uint32_t arg_a;
	uint32_t highAddr = 0;

	/* lock GPR because we may access it in multiple CMDQ HW threads */
	cmdq_op_wait(handle, regAccessToken);

	/* Assign 32-bit GRP with value */
	arg_a = (CMDQ_CODE_MOVE << 24) | (valueRegId << 16) | (4 << 21);	/* arg_a is GPR */
	cmdq_append_command(handle, CMDQ_CODE_RAW, arg_a, value, 0, 0);

	/* Note that <MOVE> arg_b is 48-bit */
	/* so writeAddress is split into 2 parts */
	/* and we store address in 64-bit GPR (P0-P7) */
	CMDQ_GET_HIGH_ADDR(dramAddr, highAddr);
	cmdq_append_command(handle, CMDQ_CODE_MOVE,
			    highAddr |
			    ((destRegId & 0x1f) << 16) | (4 << 21), (uint32_t) dramAddr, 0, 0);

	/* write value in GPR to memory pointed by GPR */
	cmdq_append_command(handle, CMDQ_CODE_WRITE, destRegId, valueRegId, 1, 1);

	/* release the GPR lock */
	cmdq_op_set_event(handle, regAccessToken);

	return 0;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

int32_t cmdq_op_finalize_command(cmdqRecHandle handle, bool loop)
{
	int32_t status = 0;
	uint32_t arg_b = 0;

	if (NULL == handle)
		return -EFAULT;

	if (!handle->finalized) {
		if ((handle->prefetchCount > 0)
		    && cmdq_get_func()->shouldEnablePrefetch(handle->scenario)) {
			CMDQ_ERR
			    ("not insert prefetch disble marker when prefetch enabled, prefetchCount:%d\n",
			     handle->prefetchCount);
			cmdq_task_dump_command(handle);

			status = -EFAULT;
			return status;
		}

		/* insert EOF instruction */
		arg_b = 0x1;	/* generate IRQ for each command iteration */
#ifdef DISABLE_LOOP_IRQ
		if (loop == true && cmdq_get_func()->force_loop_irq(handle->scenario) == false)
			arg_b = 0x0;	/* no generate IRQ for loop thread to save power */
#endif

		status = cmdq_append_command(handle, CMDQ_CODE_EOC, 0, arg_b, 0, 0);

		if (0 != status)
			return status;

		/* insert JUMP to loop to beginning or as a scheduling mark(8) */
		status = cmdq_append_command(handle, CMDQ_CODE_JUMP, 0,	/* not absolute */
					     loop ? -handle->blockSize : 8, 0, 0);
		if (0 != status)
			return status;

		handle->finalized = true;
	}

	return status;
}

int32_t cmdq_setup_sec_data_of_command_desc_by_rec_handle(cmdqCommandStruct *pDesc,
							      cmdqRecHandle handle)
{
	/* fill field from user's request */
	pDesc->secData.is_secure = handle->secData.is_secure;
	pDesc->secData.enginesNeedDAPC = handle->secData.enginesNeedDAPC;
	pDesc->secData.enginesNeedPortSecurity = handle->secData.enginesNeedPortSecurity;

	pDesc->secData.addrMetadataCount = handle->secData.addrMetadataCount;
	pDesc->secData.addrMetadatas = handle->secData.addrMetadatas;
	pDesc->secData.addrMetadataMaxCount = handle->secData.addrMetadataMaxCount;
#ifdef CMDQ_SECURE_PATH_SUPPORT
#ifdef CONFIG_MTK_CMDQ_TAB
	pDesc->secData.secMode = handle->secData.secMode;
#endif
#endif

	/* init reserved field */
	pDesc->secData.resetExecCnt = false;
	pDesc->secData.waitCookie = 0;

	return 0;
}

int32_t cmdq_rec_setup_profile_marker_data(cmdqCommandStruct *pDesc, cmdqRecHandle handle)
{
#ifdef CMDQ_PROFILE_MARKER_SUPPORT
	uint32_t i;

	pDesc->profileMarker.count = handle->profileMarker.count;
	pDesc->profileMarker.hSlot = handle->profileMarker.hSlot;

	for (i = 0; i < CMDQ_MAX_PROFILE_MARKER_IN_TASK; i++)
		pDesc->profileMarker.tag[i] = handle->profileMarker.tag[i];
#endif
	return 0;
}

int32_t cmdq_task_flush(cmdqRecHandle handle)
{
	int32_t status;
	cmdqCommandStruct desc = { 0 };

	status = cmdq_op_finalize_command(handle, false);
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
	/* secure path */
	cmdq_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);
	/* profile marker */
	cmdq_rec_setup_profile_marker_data(&desc, handle);

	return cmdqCoreSubmitTask(&desc);
}

int32_t cmdq_task_flush_and_read_register(cmdqRecHandle handle, uint32_t regCount,
			    uint32_t *addrArray, uint32_t *valueArray)
{
	int32_t status;
	cmdqCommandStruct desc = { 0 };

	status = cmdq_op_finalize_command(handle, false);
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
	/* secure path */
	cmdq_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);
	/* profile marker */
	cmdq_rec_setup_profile_marker_data(&desc, handle);

	return cmdqCoreSubmitTask(&desc);
}

int32_t cmdq_task_flush_async(cmdqRecHandle handle)
{
	int32_t status = 0;
	cmdqCommandStruct desc = { 0 };
	TaskStruct *pTask = NULL;

	status = cmdq_op_finalize_command(handle, false);
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
	/* secure path */
	cmdq_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);
	/* profile marker */
	cmdq_rec_setup_profile_marker_data(&desc, handle);

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

int32_t cmdq_task_flush_async_callback(cmdqRecHandle handle, CmdqAsyncFlushCB callback,
				  uint32_t userData)
{
	int32_t status = 0;
	cmdqCommandStruct desc = { 0 };
	TaskStruct *pTask = NULL;

	status = cmdq_op_finalize_command(handle, false);
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
	/* secure path */
	cmdq_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);
	/* profile marker */
	cmdq_rec_setup_profile_marker_data(&desc, handle);

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

static int32_t cmdq_dummy_irq_callback(unsigned long data)
{
	return 0;
}

int32_t cmdq_task_start_loop(cmdqRecHandle handle)
{
	return cmdq_task_start_loop_callback(handle, &cmdq_dummy_irq_callback, 0);
}

int32_t cmdq_task_start_loop_callback(cmdqRecHandle handle, CmdqInterruptCB loopCB, unsigned long loopData)
{
	int32_t status = 0;
	cmdqCommandStruct desc = { 0 };

	if (NULL == handle)
		return -EFAULT;

	if (NULL != handle->pRunningTask)
		return -EBUSY;

	status = cmdq_op_finalize_command(handle, true);
	if (status < 0)
		return status;

	CMDQ_MSG("Submit task loop: scenario: %d, priority: %d, engine: 0x%llx,",
			   handle->scenario, handle->priority, handle->engineFlag);
	CMDQ_MSG("Submit task loop: buffer: 0x%p, size: %d, callback: 0x%p, data: %ld\n",
			   handle->pBuffer, handle->blockSize, loopCB, loopData);

	desc.scenario = handle->scenario;
	desc.priority = handle->priority;
	desc.engineFlag = handle->engineFlag;
	desc.pVABase = (cmdqU32Ptr_t) (unsigned long)handle->pBuffer;
	desc.blockSize = handle->blockSize;
	/* secure path */
	cmdq_setup_sec_data_of_command_desc_by_rec_handle(&desc, handle);
	/* profile marker */
	cmdq_rec_setup_profile_marker_data(&desc, handle);

	status = cmdqCoreSubmitTaskAsync(&desc, loopCB, loopData, &handle->pRunningTask);
	return status;
}

int32_t cmdq_task_stop_loop(cmdqRecHandle handle)
{
	int32_t status = 0;
	struct TaskStruct *pTask;

	if (NULL == handle)
		return -EFAULT;

	pTask = handle->pRunningTask;
	if (NULL == pTask)
		return -EFAULT;

	status = cmdqCoreReleaseTask(pTask);
	handle->pRunningTask = NULL;
	return status;
}

int32_t cmdq_task_get_instruction_count(cmdqRecHandle handle)
{
	int32_t instruction_count;

	if (NULL == handle)
		return 0;

	instruction_count = handle->blockSize / CMDQ_INST_SIZE;

	return instruction_count;
}

int32_t cmdq_op_profile_marker(cmdqRecHandle handle, const char *tag)
{
#ifdef CMDQ_PROFILE_MARKER_SUPPORT
	int32_t status;
	int32_t index;
	cmdqBackupSlotHandle hSlot;
	dma_addr_t allocatedStartPA;

	do {
		allocatedStartPA = 0;
		status = 0;

		/* allocate temp slot for GCE to store timestamp info */
		/* those timestamp info will copy to record strute after task execute done */
		if ((0 == handle->profileMarker.count) && (0 == handle->profileMarker.hSlot)) {
			status =
			    cmdqCoreAllocWriteAddress(CMDQ_MAX_PROFILE_MARKER_IN_TASK,
						      &allocatedStartPA);
			if (0 > status) {
				CMDQ_ERR("[REC][PROF_MARKER]allocate failed, status:%d\n", status);
				break;
			}

			handle->profileMarker.hSlot = 0LL | (allocatedStartPA);

			CMDQ_VERBOSE
			    ("[REC][PROF_MARKER]update handle(%p) slot start PA:%pa(0x%llx)\n",
			     handle, &allocatedStartPA, handle->profileMarker.hSlot);
		}

		/* insert instruciton */
		index = handle->profileMarker.count;
		hSlot = (cmdqBackupSlotHandle) (handle->profileMarker.hSlot);

		if (index >= CMDQ_MAX_PROFILE_MARKER_IN_TASK) {
			CMDQ_ERR
			    ("[REC][PROF_MARKER]insert profile maker failed since already reach max count\n");
			status = -EFAULT;
			break;
		}

		CMDQ_VERBOSE
		    ("[REC][PROF_MARKER]inserting profile instr, handle:%p, slot:%pa(0x%llx), index:%d, tag:%s\n",
		     handle, &hSlot, handle->profileMarker.hSlot, index, tag);

		cmdq_op_read_reg_to_mem(handle, hSlot, index, CMDQ_APXGPT2_COUNT);

		handle->profileMarker.tag[index] = (cmdqU32Ptr_t) (unsigned long)tag;
		handle->profileMarker.count += 1;
	} while (0);

	return status;
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't enable profile marker\n", __func__);
	return -EFAULT;
#endif
}

int32_t cmdq_task_dump_command(cmdqRecHandle handle)
{
	int32_t status = 0;
	struct TaskStruct *pTask;

	if (NULL == handle)
		return -EFAULT;

	pTask = handle->pRunningTask;
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

int32_t cmdq_task_estimate_command_exec_time(const cmdqRecHandle handle)
{
	int32_t time = 0;

	if (NULL == handle)
		return -EFAULT;

	CMDQ_LOG("======REC 0x%p command execution time ESTIMATE:\n", handle);
	time = cmdq_prof_estimate_command_exe_time(handle->pBuffer, handle->blockSize);
	CMDQ_LOG("======REC 0x%p  END\n", handle);

	return time;
}

int32_t cmdq_task_destroy(cmdqRecHandle handle)
{
	if (NULL == handle)
		return -EFAULT;

	if (NULL != handle->pRunningTask)
		return cmdq_task_stop_loop(handle);

	/* Free command buffer */
	vfree(handle->pBuffer);
	handle->pBuffer = NULL;

	/* Free command handle */
	kfree(handle);

	return 0;
}

int32_t cmdq_op_set_nop(cmdqRecHandle handle, uint32_t index)
{
	uint32_t *p_command;
	uint32_t offsetIndex = index * CMDQ_INST_SIZE;

	if (NULL == handle || offsetIndex > (handle->blockSize - CMDQ_INST_SIZE))
		return -EFAULT;

	CMDQ_MSG("======REC 0x%p Set NOP to index: %d\n", handle, index);
	p_command = (uint32_t *) ((uint8_t *) handle->pBuffer + offsetIndex);
	*p_command++ = 8;
	*p_command++ = (CMDQ_CODE_JUMP << 24) | (0 & 0x0FFFFFF);
	CMDQ_MSG("======REC 0x%p  END\n", handle);

	return index;
}

int32_t cmdq_task_query_offset(cmdqRecHandle handle, uint32_t startIndex,
				  const CMDQ_CODE_ENUM opCode, CMDQ_EVENT_ENUM event)
{
	int32_t Offset = -1;
	uint32_t arg_a, arg_b;
	uint32_t *p_command;
	uint32_t QueryIndex, MaxIndex;

	if (NULL == handle || (startIndex * CMDQ_INST_SIZE) > (handle->blockSize - CMDQ_INST_SIZE))
		return -EFAULT;

	switch (opCode) {
	case CMDQ_CODE_WFE:
		/* bit 0-11: wait_value, 1 */
		/* bit 15: to_wait, true */
		/* bit 31: to_update, true */
		/* bit 16-27: update_value, 0 */
		arg_b = ((1 << 31) | (1 << 15) | 1);
		arg_a = (CMDQ_CODE_WFE << 24) | cmdq_core_get_event_value(event);
		break;
	case CMDQ_CODE_SET_TOKEN:
		/* this is actually WFE(SYNC) but with different parameter */
		/* interpretation */
		/* bit 15: to_wait, false */
		/* bit 31: to_update, true */
		/* bit 16-27: update_value, 1 */
		arg_b = ((1 << 31) | (1 << 16));
		arg_a = (CMDQ_CODE_WFE << 24) | cmdq_core_get_event_value(event);
		break;
	case CMDQ_CODE_WAIT_NO_CLEAR:
		/* bit 0-11: wait_value, 1 */
		/* bit 15: to_wait, true */
		/* bit 31: to_update, false */
		arg_b = ((0 << 31) | (1 << 15) | 1);
		arg_a = (CMDQ_CODE_WFE << 24) | cmdq_core_get_event_value(event);
		break;
	case CMDQ_CODE_CLEAR_TOKEN:
		/* this is actually WFE(SYNC) but with different parameter */
		/* interpretation */
		/* bit 15: to_wait, false */
		/* bit 31: to_update, true */
		/* bit 16-27: update_value, 0 */
		arg_b = ((1 << 31) | (0 << 16));
		arg_a = (CMDQ_CODE_WFE << 24) | cmdq_core_get_event_value(event);
		break;
	case CMDQ_CODE_PREFETCH_ENABLE:
		/* this is actually MARKER but with different parameter */
		/* interpretation */
		/* bit 53: non_suspendable, true */
		/* bit 48: no_inc_exec_cmds_cnt, true */
		/* bit 20: prefetch_marker, true */
		/* bit 17: prefetch_marker_en, true */
		/* bit 16: prefetch_en, true */
		arg_b = ((1 << 20) | (1 << 17) | (1 << 16));
		arg_a = (CMDQ_CODE_EOC << 24) | (0x1 << (53 - 32)) | (0x1 << (48 - 32));
		break;
	case CMDQ_CODE_PREFETCH_DISABLE:
		/* this is actually MARKER but with different parameter */
		/* interpretation */
		/* bit 48: no_inc_exec_cmds_cnt, true */
		/* bit 20: prefetch_marker, true */
		arg_b = (1 << 20);
		arg_a = (CMDQ_CODE_EOC << 24) | (0x1 << (48 - 32));
		break;
	default:
		CMDQ_MSG("This offset of instruction can not be queried.\n");
		return -EFAULT;
	}

	MaxIndex = handle->blockSize / CMDQ_INST_SIZE;
	for (QueryIndex = startIndex; QueryIndex < MaxIndex; QueryIndex++) {
		p_command = (uint32_t *) ((uint8_t *) handle->pBuffer + QueryIndex * CMDQ_INST_SIZE);
		if ((arg_b == *p_command++) && (arg_a == *p_command)) {
			Offset = (int32_t) QueryIndex;
			CMDQ_MSG("Get offset = %d\n", Offset);
			break;
		}
	}
	if (Offset < 0) {
		/* Can not find the offset of desired instruction */
		CMDQ_LOG("Can not find the offset of desired instruction\n");
	}

	return Offset;
}

int32_t cmdq_resource_acquire(cmdqRecHandle handle, CMDQ_EVENT_ENUM resourceEvent)
{
	bool acquireResult;

	acquireResult = cmdqCoreAcquireResource(resourceEvent);
	if (!acquireResult) {
		CMDQ_LOG("Acquire resource (event:%d) failed, handle:0x%p\n", resourceEvent, handle);
		return -EFAULT;
	}
	return 0;
}

int32_t cmdq_resource_acquire_and_write(cmdqRecHandle handle, CMDQ_EVENT_ENUM resourceEvent,
							uint32_t addr, uint32_t value, uint32_t mask)
{
	bool acquireResult;

	acquireResult = cmdqCoreAcquireResource(resourceEvent);
	if (!acquireResult) {
		CMDQ_LOG("Acquire resource (event:%d) failed, handle:0x%p\n", resourceEvent, handle);
		return -EFAULT;
	}

	return cmdq_op_write_reg(handle, addr, value, mask);
}

int32_t cmdq_resource_release(cmdqRecHandle handle, CMDQ_EVENT_ENUM resourceEvent)
{
	cmdqCoreReleaseResource(resourceEvent);
	return cmdq_op_set_event(handle, resourceEvent);
}

int32_t cmdq_resource_release_and_write(cmdqRecHandle handle, CMDQ_EVENT_ENUM resourceEvent,
							uint32_t addr, uint32_t value, uint32_t mask)
{
	int32_t result;

	cmdqCoreReleaseResource(resourceEvent);
	result = cmdq_op_write_reg(handle, addr, value, mask);
	if (result >= 0)
		return cmdq_op_set_event(handle, resourceEvent);

	CMDQ_ERR("Write instruction fail and not release resource!\n");
	return result;
}

int32_t cmdqRecCreate(CMDQ_SCENARIO_ENUM scenario, cmdqRecHandle *pHandle)
{
	return cmdq_task_create(scenario, pHandle);
}

int32_t cmdqRecSetEngine(cmdqRecHandle handle, uint64_t engineFlag)
{
	return cmdq_task_set_engine(handle, engineFlag);
}

int32_t cmdqRecReset(cmdqRecHandle handle)
{
	return cmdq_task_reset(handle);
}

int32_t cmdqRecSetSecure(cmdqRecHandle handle, const bool is_secure)
{
	return cmdq_task_set_secure(handle, is_secure);
}

int32_t cmdqRecIsSecure(cmdqRecHandle handle)
{
	return cmdq_task_is_secure(handle);
}

/* tablet use */
#ifdef CONFIG_MTK_CMDQ_TAB
int32_t cmdqRecSetSecureMode(cmdqRecHandle handle, enum CMDQ_DISP_MODE mode)
{
	return cmdq_task_set_secure_mode(handle, mode);
}
#endif

int32_t cmdqRecSecureEnableDAPC(cmdqRecHandle handle, const uint64_t engineFlag)
{
	return cmdq_task_secure_enable_dapc(handle, engineFlag);
}

int32_t cmdqRecSecureEnablePortSecurity(cmdqRecHandle handle, const uint64_t engineFlag)
{
	return cmdq_task_secure_enable_port_security(handle, engineFlag);
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
	status = cmdq_append_command(handle,
				     CMDQ_CODE_EOC,
				     (0x1 << (53 - 32)) | (0x1 << (48 - 32)), 0x00130000, 0, 0);

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
	return cmdq_op_write_reg(handle, addr, (CMDQ_VARIABLE)value, mask);
}

int32_t cmdqRecWriteSecure(cmdqRecHandle handle, uint32_t addr,
			   CMDQ_SEC_ADDR_METADATA_TYPE type,
			   uint32_t baseHandle, uint32_t offset, uint32_t size, uint32_t port)
{
	return cmdq_op_write_reg_secure(handle, addr, type, baseHandle, offset, size, port);
}

#ifdef CONFIG_MTK_CMDQ_TAB
int32_t cmdqRecWriteSecureMask(cmdqRecHandle handle, uint32_t addr,
				CMDQ_SEC_ADDR_METADATA_TYPE type, uint32_t value, uint32_t mask)
{
	return cmdq_op_write_reg_secure_mask(handle, addr, type, value, mask);
}
#endif

int32_t cmdqRecPoll(cmdqRecHandle handle, uint32_t addr, uint32_t value, uint32_t mask)
{
	return cmdq_op_poll(handle, addr, value, mask);
}

int32_t cmdqRecWait(cmdqRecHandle handle, CMDQ_EVENT_ENUM event)
{
	return cmdq_op_wait(handle, event);
}

int32_t cmdqRecWaitNoClear(cmdqRecHandle handle, CMDQ_EVENT_ENUM event)
{
	return cmdq_op_wait_no_clear(handle, event);
}

int32_t cmdqRecClearEventToken(cmdqRecHandle handle, CMDQ_EVENT_ENUM event)
{
	return cmdq_op_clear_event(handle, event);
}

int32_t cmdqRecSetEventToken(cmdqRecHandle handle, CMDQ_EVENT_ENUM event)
{
	return cmdq_op_set_event(handle, event);
}

int32_t cmdqRecReadToDataRegister(cmdqRecHandle handle, uint32_t hw_addr,
				  CMDQ_DATA_REGISTER_ENUM dst_data_reg)
{
	return cmdq_op_read_to_data_register(handle, hw_addr, dst_data_reg);
}

int32_t cmdqRecWriteFromDataRegister(cmdqRecHandle handle,
				     CMDQ_DATA_REGISTER_ENUM src_data_reg, uint32_t hw_addr)
{
	return cmdq_op_write_from_data_register(handle, src_data_reg, hw_addr);
}

int32_t cmdqBackupAllocateSlot(cmdqBackupSlotHandle *p_h_backup_slot, uint32_t slotCount)
{
	return cmdq_alloc_mem(p_h_backup_slot, slotCount);
}

int32_t cmdqBackupReadSlot(cmdqBackupSlotHandle h_backup_slot, uint32_t slot_index, uint32_t *value)
{
	return cmdq_cpu_read_mem(h_backup_slot, slot_index, value);
}

int32_t cmdqBackupWriteSlot(cmdqBackupSlotHandle h_backup_slot, uint32_t slot_index, uint32_t value)
{
	return cmdq_cpu_write_mem(h_backup_slot, slot_index, value);
}

int32_t cmdqBackupFreeSlot(cmdqBackupSlotHandle h_backup_slot)
{
	return cmdq_free_mem(h_backup_slot);
}

int32_t cmdqRecBackupRegisterToSlot(cmdqRecHandle handle,
				    cmdqBackupSlotHandle h_backup_slot,
				    uint32_t slot_index, uint32_t regAddr)
{
	return cmdq_op_read_reg_to_mem(handle, h_backup_slot, slot_index, regAddr);
}

int32_t cmdqRecBackupWriteRegisterFromSlot(cmdqRecHandle handle,
					   cmdqBackupSlotHandle h_backup_slot,
					   uint32_t slot_index, uint32_t addr)
{
	return cmdq_op_read_mem_to_reg(handle, h_backup_slot, slot_index, addr);
}

int32_t cmdqRecBackupUpdateSlot(cmdqRecHandle handle,
				cmdqBackupSlotHandle h_backup_slot,
				uint32_t slot_index, uint32_t value)
{
	return cmdq_op_write_mem(handle, h_backup_slot, slot_index, value);
}

int32_t cmdqRecEnablePrefetch(cmdqRecHandle handle)
{
#ifdef _CMDQ_DISABLE_MARKER_
	/* disable pre-fetch marker feature but use auto prefetch mechanism */
	CMDQ_MSG("not allow enable prefetch, scenario: %d\n", handle->scenario);
	return true;
#else
	if (NULL == handle)
		return -EFAULT;

	if (cmdq_get_func()->shouldEnablePrefetch(handle->scenario)) {
		/* enable prefetch */
		CMDQ_VERBOSE("REC: enable prefetch\n");
		cmdqRecMark(handle);
		return true;
	}
	CMDQ_ERR("not allow enable prefetch, scenario: %d\n", handle->scenario);
	return -EFAULT;
#endif
}

int32_t cmdqRecDisablePrefetch(cmdqRecHandle handle)
{
	uint32_t arg_b = 0;
	uint32_t arg_a = 0;
	int32_t status = 0;

	if (NULL == handle)
		return -EFAULT;

	if (!handle->finalized) {
		if (handle->prefetchCount > 0) {
			/* with prefetch threads we should end with */
			/* bit 48: no_inc_exec_cmds_cnt = 1 */
			/* bit 20: prefetch_mark = 1 */
			/* bit 17: prefetch_mark_en = 0 */
			/* bit 16: prefetch_en = 0 */
			arg_b = 0x00100000;
			arg_a = (0x1 << 16);	/* not increse execute counter */
			/* since we're finalized, no more prefetch */
			handle->prefetchCount = 0;
			status = cmdq_append_command(handle, CMDQ_CODE_EOC, arg_a, arg_b, 0, 0);
		}

		if (0 != status)
			return status;
	}

	CMDQ_MSG("cmdqRecDisablePrefetch, status:%d\n", status);
	return status;
}

int32_t cmdqRecFlush(cmdqRecHandle handle)
{
	return cmdq_task_flush(handle);
}

int32_t cmdqRecFlushAndReadRegister(cmdqRecHandle handle, uint32_t regCount, uint32_t *addrArray,
				    uint32_t *valueArray)
{
	return cmdq_task_flush_and_read_register(handle, regCount, addrArray, valueArray);
}

int32_t cmdqRecFlushAsync(cmdqRecHandle handle)
{
	return cmdq_task_flush_async(handle);
}

int32_t cmdqRecFlushAsyncCallback(cmdqRecHandle handle, CmdqAsyncFlushCB callback,
				  uint32_t userData)
{
	return cmdq_task_flush_async_callback(handle, callback, userData);
}

int32_t cmdqRecStartLoop(cmdqRecHandle handle)
{
	return cmdq_task_start_loop(handle);
}

int32_t cmdqRecStartLoopWithCallback(cmdqRecHandle handle, CmdqInterruptCB loopCB, unsigned long loopData)
{
	return cmdq_task_start_loop_callback(handle, loopCB, loopData);
}

int32_t cmdqRecStopLoop(cmdqRecHandle handle)
{
	return cmdq_task_stop_loop(handle);
}

int32_t cmdqRecGetInstructionCount(cmdqRecHandle handle)
{
	return cmdq_task_get_instruction_count(handle);
}

int32_t cmdqRecProfileMarker(cmdqRecHandle handle, const char *tag)
{
	return cmdq_op_profile_marker(handle, tag);
}

int32_t cmdqRecDumpCommand(cmdqRecHandle handle)
{
	return cmdq_task_dump_command(handle);
}

int32_t cmdqRecEstimateCommandExecTime(const cmdqRecHandle handle)
{
	return cmdq_task_estimate_command_exec_time(handle);
}

void cmdqRecDestroy(cmdqRecHandle handle)
{
	cmdq_task_destroy(handle);
}

int32_t cmdqRecSetNOP(cmdqRecHandle handle, uint32_t index)
{
	return cmdq_op_set_nop(handle, index);
}

int32_t cmdqRecQueryOffset(cmdqRecHandle handle, uint32_t startIndex, const CMDQ_CODE_ENUM opCode,
			   CMDQ_EVENT_ENUM event)
{
	return cmdq_task_query_offset(handle, startIndex, opCode, event);
}

int32_t cmdqRecAcquireResource(cmdqRecHandle handle, CMDQ_EVENT_ENUM resourceEvent)
{
	return cmdq_resource_acquire(handle, resourceEvent);
}

int32_t cmdqRecWriteForResource(cmdqRecHandle handle, CMDQ_EVENT_ENUM resourceEvent,
							uint32_t addr, uint32_t value, uint32_t mask)
{
	return cmdq_resource_acquire_and_write(handle, resourceEvent, addr, value, mask);
}

int32_t cmdqRecReleaseResource(cmdqRecHandle handle, CMDQ_EVENT_ENUM resourceEvent)
{
	return cmdq_resource_release(handle, resourceEvent);
}

int32_t cmdqRecWriteAndReleaseResource(cmdqRecHandle handle, CMDQ_EVENT_ENUM resourceEvent,
							uint32_t addr, uint32_t value, uint32_t mask)
{
	return cmdq_resource_release_and_write(handle, resourceEvent, addr, value, mask);
}
