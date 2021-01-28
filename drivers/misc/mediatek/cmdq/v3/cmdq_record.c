// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015 MediaTek Inc.
 */

#include <linux/sched/clock.h>

#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_virtual.h"
#include "cmdq_helper_ext.h"
#include "cmdq_device.h"
#if IS_ENABLED(CONFIG_MMPROFILE)
#include "cmdq_mmp.h"
#endif
#include "cmdq_sec.h"

#ifdef CMDQ_SECURE_PATH_SUPPORT
#include "cmdq_sec_iwc_common.h"
#endif

#define CMDQ_DATA_VAR		(CMDQ_BIT_VAR<<CMDQ_DATA_BIT)
#define CMDQ_TASK_TPR_VAR	(CMDQ_DATA_VAR | CMDQ_TPR_ID)
#define CMDQ_TASK_TEMP_CPR_VAR	(CMDQ_DATA_VAR | CMDQ_SPR_FOR_TEMP)
#define CMDQ_TASK_LOOP_DEBUG_VAR (CMDQ_DATA_VAR | CMDQ_SPR_FOR_LOOP_DEBUG)
#define CMDQ_ARG_CPR_START	(CMDQ_DATA_VAR | CMDQ_CPR_STRAT_ID)
#define CMDQ_EVENT_ARGA(event_id)	(CMDQ_CODE_WFE << 24 | event_id)

#define CMDQ_TASK_CPR_POSITION_ARRAY_UNIT_SIZE	(32)

struct cmdq_async_data {
	CmdqAsyncFlushCB cb;
	u64 user_data;
	struct cmdqRecStruct *handle;
};

/* push a value into a stack */
s32 cmdq_op_condition_push(struct cmdq_stack_node **top_node, u32 position,
	enum CMDQ_STACK_TYPE_ENUM stack_type)
{
	/* allocate a new node for val */
	struct cmdq_stack_node *new_node = kmalloc(
		sizeof(struct cmdq_stack_node), GFP_KERNEL);

	if (!new_node) {
		CMDQ_ERR("failed to alloc cmdq_stack_node\n");
		return -ENOMEM;
	}

	new_node->position = position;
	new_node->stack_type = stack_type;
	new_node->next = *top_node;
	*top_node = new_node;

	return 0;
}

/* pop a value out from the stack */
s32 cmdq_op_condition_pop(struct cmdq_stack_node **top_node, u32 *position,
	enum CMDQ_STACK_TYPE_ENUM *stack_type)
{
	/* tmp for record the top node */
	struct cmdq_stack_node *temp_node;

	if (*top_node == NULL)
		return -1;

	/* get the value of the top */
	temp_node = *top_node;
	*position = temp_node->position;
	*stack_type = temp_node->stack_type;
	/* change top */
	*top_node = temp_node->next;
	kfree(temp_node);

	return 0;
}

/* query from the stack */
s32 cmdq_op_condition_query(const struct cmdq_stack_node *top_node,
	int *position, enum CMDQ_STACK_TYPE_ENUM *stack_type)
{
	*stack_type = CMDQ_STACK_NULL;

	if (!top_node)
		return -1;

	/* get the value of the top */
	*position = top_node->position;
	*stack_type = top_node->stack_type;

	return 0;
}

/* query op position from the stack by type bit */
s32 cmdq_op_condition_find_op_type(const struct cmdq_stack_node *top_node,
	const u32 position, u32 op_type_bit,
	const struct cmdq_stack_node **op_node)
{
	const struct cmdq_stack_node *temp_node = top_node;
	u32 got_position = position;

	/* get the value of the top */
	do {
		if (!temp_node)
			break;

		if ((1 << temp_node->stack_type) & op_type_bit) {
			got_position = temp_node->position;
			if (op_node)
				*op_node = temp_node;
			break;
		}

		temp_node = temp_node->next;
	} while (1);

	return (s32)(got_position - position);
}

static bool cmdq_is_cpr(u32 argument, u32 arg_type)
{
	if (arg_type == 1 &&
		argument >= CMDQ_THR_SPR_MAX &&
		argument < CMDQ_THR_VAR_MAX) {
		return true;
	}

	return false;
}

static void cmdq_save_op_variable_position(
	struct cmdqRecStruct *handle, u32 index)
{
	u32 *p_new_buffer = NULL;
	u32 *p_instr_position = NULL;
	u32 array_num = 0;
	u64 *inst, *logic_inst;
	u32 offset;

	if (!handle)
		return;

	/* Exceed max number of SPR, use CPR */
	if ((handle->replace_instr.number %
		CMDQ_TASK_CPR_POSITION_ARRAY_UNIT_SIZE) == 0) {
		array_num = (handle->replace_instr.number +
			CMDQ_TASK_CPR_POSITION_ARRAY_UNIT_SIZE) *
			sizeof(u32);

		p_new_buffer = kzalloc(array_num, GFP_KERNEL);

		/* copy and release old buffer */
		if (handle->replace_instr.position) {
			memcpy(p_new_buffer,
				CMDQ_U32_PTR(handle->replace_instr.position),
				handle->replace_instr.number * sizeof(u32));
			kfree(CMDQ_U32_PTR(handle->replace_instr.position));
		}
		handle->replace_instr.position = (cmdqU32Ptr_t)
			(unsigned long)p_new_buffer;
	}

	p_instr_position = CMDQ_U32_PTR(handle->replace_instr.position);
	p_instr_position[handle->replace_instr.number] = index;
	handle->replace_instr.number++;

	offset = index * CMDQ_INST_SIZE;
	if (offset >= handle->pkt->cmd_buf_size)
		offset = (u32)(handle->pkt->cmd_buf_size - CMDQ_INST_SIZE);

	inst = cmdq_pkt_get_va_by_offset(handle->pkt, offset);
	logic_inst = cmdq_pkt_get_va_by_offset(handle->pkt,
		offset - CMDQ_INST_SIZE);
	if (!inst || !logic_inst)
		CMDQ_MSG(
			"Add replace_instr: index:%u (real offset:%u) position:%u number:%u scenario:%d thread:%d\n",
			index, offset,
			p_instr_position[handle->replace_instr.number-1],
			handle->replace_instr.number,
			handle->scenario, handle->thread);
	else
		CMDQ_MSG(
			"Add replace_instr: index:%u (real offset:%u) position:%u number:%u inst:0x%016llx logic:0x%016llx scenario:%d thread:%d\n",
			index, offset,
			p_instr_position[handle->replace_instr.number-1],
			handle->replace_instr.number, *inst, *logic_inst,
			handle->scenario, handle->thread);
}

static s32 cmdq_var_data_type(CMDQ_VARIABLE arg_in, u32 *arg_out,
	u32 *arg_type)
{
	s32 status = 0;

	switch (arg_in >> CMDQ_DATA_BIT) {
	case CMDQ_BIT_VALUE:
		*arg_type = 0;
		*arg_out = (u32)(arg_in & 0xFFFFFFFF);
		break;
	case CMDQ_BIT_VAR:
		*arg_type = 1;
		*arg_out = (u32)(arg_in & 0xFFFFFFFF);
		break;
	default:
		CMDQ_ERR(
			"Incorrect CMDQ data type (0x%llx), can not append new command\n",
			arg_in);
		status = -EFAULT;
		*arg_out = 0;
		*arg_type = 0;
		break;
	}

	return status;
}

static s32 cmdq_create_variable_if_need(struct cmdqRecStruct *handle,
	CMDQ_VARIABLE *arg)
{
	s32 status = 0;
	u32 arg_value = 0, arg_type = 0;

	if (!handle)
		return -EINVAL;

	do {
		status = cmdq_var_data_type(*arg, &arg_value, &arg_type);
		if (status < 0)
			break;

		CMDQ_MSG("check CPR create: value:%d type:%d\n",
			arg_value, arg_type);
		if (arg_type == 1) {
			/* Already be variable */
			break;
		}

		if (handle->local_var_num >= CMDQ_THR_FREE_USR_VAR_MAX) {
			CMDQ_ERR(
				"Exceed max number of local variable in one task, please review your instructions.\n");
			status = -EFAULT;
			break;
		}

		*arg = ((CMDQ_BIT_VAR<<CMDQ_DATA_BIT) | handle->local_var_num);
		handle->local_var_num++;
	} while (0);

	return status;
}

s32 cmdq_reset_v3_struct(struct cmdqRecStruct *handle)
{
	u32 destroy_position;
	enum CMDQ_STACK_TYPE_ENUM destroy_stack_type;

	if (!handle)
		return -EFAULT;

	/* reset local variable setting */
	handle->local_var_num = CMDQ_THR_SPR_START;
	handle->arg_value = CMDQ_TASK_CPR_INITIAL_VALUE;
	handle->arg_source = CMDQ_TASK_CPR_INITIAL_VALUE;
	handle->arg_timeout = CMDQ_TASK_CPR_INITIAL_VALUE;

	do {
		/* check if-else stack */
		if (!handle->if_stack_node)
			break;

		/* pop all if-else stack out */
		cmdq_op_condition_pop(&handle->if_stack_node,
			&destroy_position, &destroy_stack_type);
	} while (1);

	do {
		/* check while stack */
		if (!handle->while_stack_node)
			break;

		/* pop all while stack out */
		cmdq_op_condition_pop(&handle->while_stack_node,
			&destroy_position, &destroy_stack_type);
	} while (1);
	return 0;
}

s32 cmdq_rec_realloc_addr_metadata_buffer(struct cmdqRecStruct *handle,
	const u32 size)
{
	void *pNewBuf = NULL;
	void *pOriginalBuf;
	u32 originalSize;

	if (!handle)
		return -EINVAL;

	pOriginalBuf = (void *)CMDQ_U32_PTR(
			handle->secData.addrMetadatas);

	originalSize = sizeof(struct cmdqSecAddrMetadataStruct) *
			handle->secData.addrMetadataMaxCount;

	if (size <= originalSize)
		return 0;

	pNewBuf = kzalloc(size, GFP_KERNEL);
	if (!pNewBuf) {
		CMDQ_ERR(
			"REC: secAddrMetadata, kzalloc %d bytes addr_metadata buffer failed\n",
			size);
		return -ENOMEM;
	}

	if (pOriginalBuf && originalSize > 0)
		memcpy(pNewBuf, pOriginalBuf, originalSize);

	CMDQ_VERBOSE(
		"REC: secAddrMetadata, realloc size from %d to %d bytes\n",
		originalSize, size);
	kfree(pOriginalBuf);
	handle->secData.addrMetadatas = (cmdqU32Ptr_t)(unsigned long)(pNewBuf);
	handle->secData.addrMetadataMaxCount = size /
		sizeof(struct cmdqSecAddrMetadataStruct);

	return 0;
}

static void cmdq_task_reset_thread(struct cmdqRecStruct *handle)
{
	if (!handle)
		return;

	/*
	 * for dynamic dispatch scenario (MDP) no need acquire at create
	 * static dispath in secure path
	 */
	if (cmdq_get_func()->isDynamic(handle->scenario) &&
		!handle->secData.is_secure) {
		/* mark as client dispatch */
		handle->thd_dispatch = CMDQ_THREAD_DYNAMIC;
		return;
	}

	if (handle->thread != CMDQ_INVALID_THREAD &&
		handle->thd_dispatch == CMDQ_THREAD_ACQUIRE)
		cmdq_core_release_thread(handle->scenario, handle->thread);

	/* try thread static assign first */
	handle->thread = handle->ctrl->get_thread_id(handle->scenario);

	/* acquire an empty thread */
	if (handle->thread == CMDQ_INVALID_THREAD) {
		handle->thread = cmdq_core_acquire_thread(handle->scenario,
			false);
		handle->thd_dispatch = CMDQ_THREAD_ACQUIRE;
	} else {
		/* thread ID define by scenario directly */
		handle->thd_dispatch = CMDQ_THREAD_STATIC;
	}

	if (handle->thread == CMDQ_INVALID_THREAD)
		CMDQ_LOG(
			"[warn]cannot acquire thread for scenario:%d handle:0x%p\n",
			handle->scenario, handle);
}

s32 cmdq_task_create(enum CMDQ_SCENARIO_ENUM scenario,
	struct cmdqRecStruct **handle_out)
{
	struct cmdqRecStruct *handle = NULL;
	s32 err;

	CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task, MMPROFILE_FLAG_START,
		current->pid, scenario);

	if (unlikely(!handle_out)) {
		CMDQ_ERR("Invalid empty handle\n");
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task,
			MMPROFILE_FLAG_END, current->pid, scenario);
		return -EINVAL;
	}

	*handle_out = NULL;

	if (scenario < 0 || scenario >= CMDQ_MAX_SCENARIO_COUNT) {
		CMDQ_ERR("Unknown scenario type %d\n", scenario);
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task,
			MMPROFILE_FLAG_END, current->pid, scenario);
		return -EINVAL;
	}

	handle = kzalloc(sizeof(struct cmdqRecStruct), GFP_KERNEL);
	if (!handle) {
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task,
			MMPROFILE_FLAG_END, current->pid, scenario);
		return -ENOMEM;
	}

	INIT_LIST_HEAD(&handle->list_entry);
	handle->engineFlag = cmdq_get_func()->flagFromScenario(scenario);
	handle->scenario = scenario;
	handle->ctrl = cmdq_core_get_controller();

	/* define thread type by scenario */
	handle->thread = CMDQ_INVALID_THREAD;
	cmdq_task_reset_thread(handle);

	err = cmdq_task_reset(handle);
	if (err < 0) {
		cmdq_task_destroy(handle);
		CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task,
			MMPROFILE_FLAG_END, current->pid, scenario);
		return err;
	}

	if (unlikely(handle->thread == CMDQ_INVALID_THREAD) &&
		cmdq_get_func()->isDispScenario(scenario)) {
		CMDQ_ERR("cannot dispatch thread for disp scenario:%d\n",
			scenario);
		cmdq_task_destroy(handle);
		return -EBUSY;
	}

	*handle_out = handle;

	/* record debug information */
	if (current) {
		handle->caller_pid = current->pid;
		memcpy(handle->caller_name, current->comm,
			sizeof(TASK_COMM_LEN));
	}

	handle->submit = sched_clock();
	CMDQ_PROF_MMP(cmdq_mmp_get_event()->alloc_task, MMPROFILE_FLAG_END,
		current->pid, scenario);
	return 0;
}

s32 cmdq_task_duplicate(struct cmdqRecStruct *handle,
	struct cmdqRecStruct **handle_out)
{
	s32 status;
	struct cmdqRecStruct *handle_new;
	struct cmdq_pkt_buffer *buf, *new_buf, *last_buf = NULL;
	u32 *va;
	u32 profile_size = 0, task_size, copy_size;

	if (!handle)
		return -EINVAL;

	*handle_out = NULL;

	status = cmdq_task_create(handle->scenario, &handle_new);
	if (status < 0)
		return status;

	handle_new->ctrl = handle->ctrl;

	task_size = handle->pkt->cmd_buf_size;
	if (handle_new->profile_exec) {
		profile_size = handle_new->pkt->cmd_buf_size;
		if (handle->finalized)
			task_size -= 4 * CMDQ_INST_SIZE;
	}

	CMDQ_MSG("duplicate handle:0x%p to 0x%p\n",
		handle, handle_new);

	/* copy command buffer */
	list_for_each_entry(buf, &handle->pkt->buf, list_entry) {
		if (task_size > CMDQ_CMD_BUFFER_SIZE)
			copy_size = CMDQ_CMD_BUFFER_SIZE;
		else
			copy_size = task_size;

		if (likely(!profile_size)) {
			new_buf = cmdq_pkt_alloc_buf(handle_new->pkt);
			if (IS_ERR(new_buf)) {
				status = PTR_ERR(new_buf);
				CMDQ_ERR("alloc buf in duplicate fail:%d\n",
					status);
				return status;
			}

			memcpy(new_buf->va_base, buf->va_base, copy_size);
		} else {
			void *copy_begin;

			new_buf = list_first_entry(&handle_new->pkt->buf,
				typeof(*new_buf), list_entry);
			copy_begin = buf->va_base;
			if (handle->profile_exec) {
				copy_begin += profile_size;
				copy_size -= profile_size;
				task_size -= profile_size;
			}
			memcpy(new_buf->va_base + profile_size, copy_begin,
				copy_size);

			profile_size = 0;
		}
		handle_new->pkt->avail_buf_size -= copy_size;
		handle_new->pkt->cmd_buf_size += copy_size;
		task_size -= copy_size;

		if (last_buf) {
			va = (u32 *)(last_buf->va_base + CMDQ_CMD_BUFFER_SIZE -
				CMDQ_INST_SIZE);
			va[0] = CMDQ_REG_SHIFT_ADDR(new_buf->pa_base);
		}
		last_buf = new_buf;
	}

	if (likely(last_buf))
		handle_new->cmd_end = (u32 *)(last_buf->va_base +
			CMDQ_CMD_BUFFER_SIZE -
			handle_new->pkt->avail_buf_size - CMDQ_INST_SIZE);
	else
		handle_new->cmd_end = NULL;

	handle_new->pkt->priority = handle->pkt->priority;

	/* copy metadata */
	handle_new->engineFlag = handle->engineFlag;
	handle_new->jump_replace = handle->jump_replace;
	handle_new->loop_cb = handle->loop_cb;
	handle_new->loop_user_data = handle->loop_user_data;
	handle_new->async_callback = handle->async_callback;
	handle_new->async_user_data = handle->async_user_data;
	handle_new->prefetchCount = handle->prefetchCount;
	handle_new->local_var_num = handle->local_var_num;
	handle_new->arg_source = handle->arg_source;
	handle_new->arg_value = handle->arg_value;
	handle_new->arg_timeout = handle->arg_timeout;
	handle_new->node_private = handle->node_private;
	handle_new->engine_clk = handle->engine_clk;
	handle_new->res_flag_acquire = handle->res_flag_acquire;
	handle_new->res_flag_release = handle->res_flag_release;

	if (handle->prop_addr)
		cmdq_task_update_property(handle_new, handle->prop_addr,
			handle->prop_size);

	if (handle->secData.is_secure) {
		handle_new->secData = handle->secData;
		cmdq_task_set_secure(handle_new,
			handle_new->secData.is_secure);

		if (handle_new->secData.addrMetadataCount) {
			u32 buf_size = handle->secData.addrMetadataCount *
				sizeof(struct cmdqSecAddrMetadataStruct);
			void *new_buf;

			/* copy metadata array */
			new_buf = kzalloc(buf_size, GFP_KERNEL);
			if (!new_buf) {
				CMDQ_ERR(
					"unable to allocate buffer for secure metadata\n");
				return -ENOMEM;
			}

			memcpy(new_buf,
				(void *)CMDQ_U32_PTR(
				handle->secData.addrMetadatas),
				buf_size);

			handle_new->secData.addrMetadatas =
				(cmdqU32Ptr_t)(unsigned long)new_buf;
		}
	}

	/* copy replace instr data */
	if (handle->replace_instr.number) {
		u32 array_size = handle->replace_instr.number * (sizeof(u32));
		u32 *p_new_buffer = NULL;

		handle_new->replace_instr.number =
			handle->replace_instr.number;

		/* alloc and copy buffer */
		p_new_buffer = kzalloc(array_size, GFP_KERNEL);
		memcpy(p_new_buffer,
			CMDQ_U32_PTR(handle->replace_instr.position),
			handle->replace_instr.number * sizeof(u32));
		handle_new->replace_instr.position = (cmdqU32Ptr_t)
			(unsigned long)p_new_buffer;
	}

	*handle_out = handle_new;

	return 0;
}

#ifdef CMDQ_SECURE_PATH_SUPPORT
s32 cmdq_append_addr_metadata(struct cmdqRecStruct *handle,
	const struct cmdqSecAddrMetadataStruct *pMetadata)
{
	struct cmdqSecAddrMetadataStruct *pAddrs;
	s32 status;
	u32 size;
	/* element index of the New appended addr metadat */
	u32 index;

	pAddrs = NULL;
	status = 0;

	if (!handle)
		return -EINVAL;

	index = handle->secData.addrMetadataCount;

	if (handle->secData.addrMetadataMaxCount <= 0) {
		/* not init yet, initialize to allow max 8 addr metadata */
		size = sizeof(struct cmdqSecAddrMetadataStruct) * 8;
		status = cmdq_rec_realloc_addr_metadata_buffer(handle, size);
	} else if (handle->secData.addrMetadataCount >=
		handle->secData.addrMetadataMaxCount) {
		/* enlarge metadata buffer to twice as */
		size =
		    sizeof(struct cmdqSecAddrMetadataStruct) *
		    handle->secData.addrMetadataMaxCount * 2;
		status = cmdq_rec_realloc_addr_metadata_buffer(handle, size);
	}

	if (status < 0)
		return -ENOMEM;

	if (handle->secData.addrMetadataCount >=
		CMDQ_IWC_MAX_ADDR_LIST_LENGTH) {
		u32 maxMetaDataCount = CMDQ_IWC_MAX_ADDR_LIST_LENGTH;

		CMDQ_ERR(
			"Metadata idx = %d reach the max allowed number = %d.\n",
			handle->secData.addrMetadataCount, maxMetaDataCount);
		CMDQ_MSG(
			"ADDR: type:%d baseHandle:0x%llx offset:%d size:%d port:%d\n",
			pMetadata->type, pMetadata->baseHandle,
			pMetadata->offset, pMetadata->size, pMetadata->port);
		status = -EFAULT;
	} else {
		pAddrs = (struct cmdqSecAddrMetadataStruct *)(CMDQ_U32_PTR(
			handle->secData.addrMetadatas));
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

s32 cmdq_task_check_available(struct cmdqRecStruct *handle)
{
	if (!handle || !handle->pkt)
		return -EFAULT;

	if (unlikely(!handle->pkt->avail_buf_size))
		return cmdq_pkt_add_cmd_buffer(handle->pkt);
	return 0;
}

s32 cmdq_check_before_append(struct cmdqRecStruct *handle)
{
	if (!handle || !handle->pkt)
		return -EFAULT;

	if (handle->finalized) {
		CMDQ_ERR("Finalized record 0x%p (scenario:%d)\n",
			handle, handle->scenario);
		return -EBUSY;
	}
	return cmdq_task_check_available(handle);
}

static s32 cmdq_append_command_pkt(struct cmdq_pkt *pkt, enum cmdq_code code,
	u32 arg_a, u32 arg_b)
{
	struct cmdq_pkt_buffer *buf;
	u64 *va = NULL;

	if (unlikely(!pkt->avail_buf_size)) {
		if (cmdq_pkt_add_cmd_buffer(pkt) < 0)
			return -ENOMEM;
	}
	buf = list_last_entry(&pkt->buf, typeof(*buf), list_entry);
	va = (u64 *)(buf->va_base + CMDQ_CMD_BUFFER_SIZE -
		pkt->avail_buf_size);

	*va = (u64)((code << CMDQ_OP_CODE_SHIFT) | arg_a) << 32 | arg_b;
	pkt->cmd_buf_size += CMDQ_INST_SIZE;
	pkt->avail_buf_size -= CMDQ_INST_SIZE;
	return 0;
}

/*
 * centralize the write/polling/read command for APB and GPR handle
 * this function must be called inside cmdq_append_command
 * because we ignore buffer and pre-fetch check here.
 * Parameter:
 *     same as cmdq_append_command
 * Return:
 *     same as cmdq_append_command
 */
static s32 cmdq_append_wpr_command(
	struct cmdqRecStruct *handle, enum cmdq_code code,
	u32 arg_a, u32 arg_b, u32 arg_a_type, u32 arg_b_type)
{
	s32 status = 0;
	s32 subsys;
	bool bUseGPR = false;
	/* use new arg_a to present final inserted arg_a */
	u32 new_arg_a;
	u32 new_arg_a_type = arg_a_type;
	u32 arg_type = 0;

	/* be careful that subsys encoding position
	 * is different among platforms
	 */
	const u32 subsys_bit = cmdq_get_func()->getSubsysLSBArgA();

	if (!handle)
		return -EFAULT;

	if (code != CMDQ_CODE_READ && code != CMDQ_CODE_WRITE &&
		code != CMDQ_CODE_POLL) {
		CMDQ_ERR(
			"Record 0x%p, flow error, should not append comment in wpr API\n",
			handle);
		return -EFAULT;
	}

	CMDQ_VERBOSE(
		"REC:0x%p CMD:arg_a:0x%08x arg_b:0x%08x arg_a_type:%d arg_b_type:%d\n",
		handle, arg_a, arg_b, arg_a_type, arg_b_type);

	if (arg_a_type == 0) {
		/* arg_a is the HW register address to read from */
		subsys = cmdq_core_subsys_from_phys_addr(arg_a);
		if (subsys == CMDQ_SPECIAL_SUBSYS_ADDR) {
#ifdef CMDQ_GPR_SUPPORT
			bUseGPR = true;
			CMDQ_MSG(
				"REC:Special handle memory base address 0x%08x\n",
				arg_a);
			/* Wait and clear for GPR mutex token to enter mutex */
			cmdq_append_command_pkt(handle->pkt, CMDQ_CODE_WFE,
				CMDQ_SYNC_TOKEN_GPR_SET_4,
				((1 << 31) | (1 << 15) | 1));
			/* Move extra handle APB address to GPR */
			cmdq_append_command_pkt(handle->pkt, CMDQ_CODE_MOVE,
				((CMDQ_DATA_REG_DEBUG & 0x1f) << 16) |
				(4 << 21), arg_a);
			/* change final arg_a to GPR */
			new_arg_a = ((CMDQ_DATA_REG_DEBUG & 0x1f) <<
				subsys_bit);
			if (arg_a & 0x1) {
				/* MASK case, set final bit to 1 */
				new_arg_a = new_arg_a | 0x1;
			}
			/* change arg_a type to 1 */
			new_arg_a_type = 1;
#else
			CMDQ_ERR(
				"func:%s failed since CMDQ doesn't support GPR\n",
				__func__);
			status = -EFAULT;
#endif
		} else if (arg_a_type == 0 && subsys < 0) {
			CMDQ_ERR(
				"REC: Unsupported memory base address 0x%08x\n",
				arg_a);
			status = -EFAULT;
		} else {
			/* compose final arg_a according to subsys table */
			new_arg_a = (arg_a & 0xffff) |
				((subsys & 0x1f) << subsys_bit);
		}
	} else {
		/* compose final arg_a according GPR value */
		new_arg_a = ((arg_a & 0x1f) << subsys_bit);
	}

	if (status < 0)
		return status;

	arg_type = (new_arg_a_type << 2) | (arg_b_type << 1);

	/* new_arg_a is the HW register address to access from or
	 * GPR value store the HW register address
	 * arg_b is the value or register id
	 * bit 55: arg_a type, 1 for GPR
	 * bit 54: arg_b type, 1 for GPR
	 * argType: ('new_arg_a_type', 'arg_b_type', '0')
	 */
	cmdq_append_command_pkt(handle->pkt, code,
		new_arg_a | (arg_type << 21), arg_b);

	if (bUseGPR) {
		/* Set for GPR mutex token to leave mutex */
		cmdq_append_command_pkt(handle->pkt, CMDQ_CODE_WFE,
			CMDQ_SYNC_TOKEN_GPR_SET_4,
			((1 << 31) | (1 << 16)));
	}
	return 0;
}

/**
 * centralize the read_s & write_s command for APB and GPR handle
 * this function must be called inside cmdq_append_command
 * because we ignore buffer and pre-fetch check here.
 * Parameter:
 *     same as cmdq_append_command
 * Return:
 *     same as cmdq_append_command
 */
static s32 cmdq_append_rw_s_command(struct cmdqRecStruct *handle,
	enum cmdq_code code, u32 arg_a, u32 arg_b, u32 arg_a_type,
	u32 arg_b_type)
{
	s32 status = 0;
	u32 new_arg_a, new_arg_b;
	u32 arg_addr, arg_value;
	u32 arg_addr_type, arg_value_type;
	u32 arg_type = 0;
	s32 subsys = 0;
	bool save_op = false;

	/* be careful that subsys encoding position is different
	 * among platforms
	 */
	const u32 subsys_bit = cmdq_get_func()->getSubsysLSBArgA();

	if (!handle)
		return -EFAULT;

	if (code == CMDQ_CODE_WRITE_S || code == CMDQ_CODE_WRITE_S_W_MASK) {
		/* For write_s command */
		arg_addr = arg_a;
		arg_addr_type = arg_a_type;
		arg_value = arg_b;
		arg_value_type = arg_b_type;
	} else if (code == CMDQ_CODE_READ_S) {
		/* For read_s command */
		arg_addr = arg_b;
		arg_addr_type = arg_b_type;
		arg_value = arg_a;
		arg_value_type = arg_a_type;
	} else {
		CMDQ_ERR(
			"Record 0x%p, flow error, should not append comment in read_s & write_s API",
			handle);
		return -EFAULT;
	}

	CMDQ_VERBOSE(
		"REC:0x%p op:0x%02x CMD:arg_a:0x%08x arg_b:0x%08x arg_a_type:%d arg_b_type:%d\n",
		handle, code, arg_a, arg_b, arg_a_type, arg_b_type);

	if (arg_addr_type == 0) {
		/* arg_a is the HW register address to read from */
		subsys = cmdq_core_subsys_from_phys_addr(arg_addr);
		if (subsys == CMDQ_SPECIAL_SUBSYS_ADDR) {
			CMDQ_MSG(
				"REC: Special handle memory base address 0x%08x\n",
				arg_a);
			/* Assign extra handle APB address to SPR */
			cmdq_append_command_pkt(handle->pkt, CMDQ_CODE_LOGIC,
				(4 << 21) | (CMDQ_LOGIC_ASSIGN << 16) |
				CMDQ_SPR_FOR_TEMP, arg_addr);
			/* change final arg_addr to GPR */
			subsys = 0;
			arg_addr = CMDQ_SPR_FOR_TEMP;
			/* change arg_addr type to 1 */
			arg_addr_type = 1;
		} else if (arg_addr_type == 0 && subsys < 0) {
			CMDQ_ERR(
				"REC: Unsupported memory base address 0x%08x\n",
				arg_addr);
			status = -EFAULT;
		}
	}

	if (status < 0)
		return status;

	if (handle->thread != CMDQ_INVALID_THREAD) {
		u32 cpr_offset = CMDQ_CPR_STRAT_ID + CMDQ_THR_CPR_MAX *
			handle->thread;

		/* change cpr to thread cpr directly,
		 * if we already have exclusive thread.
		 */
		if (cmdq_is_cpr(arg_addr, arg_addr_type))
			arg_addr = cpr_offset + (arg_addr - CMDQ_THR_SPR_MAX);
		if (cmdq_is_cpr(arg_value, arg_value_type))
			arg_value = cpr_offset + (arg_value - CMDQ_THR_SPR_MAX);
	} else if (cmdq_is_cpr(arg_a, arg_a_type) ||
		cmdq_is_cpr(arg_b, arg_b_type)) {
		/* save local variable position */
		CMDQ_MSG(
			"save op:0x%02x CMD:arg_a:0x%08x arg_b:0x%08x arg_a_type:%d arg_b_type:%d\n",
		     code, arg_a, arg_b, arg_a_type, arg_b_type);
		save_op = true;
	}

	if (CMDQ_CODE_WRITE_S == code || CMDQ_CODE_WRITE_S_W_MASK == code) {
		/* For write_s command */
		new_arg_a = (arg_addr & 0xffff) |
			((subsys & 0x1f) << subsys_bit);
		if (arg_value_type == 0)
			new_arg_b = arg_value;
		else
			new_arg_b = arg_value << 16;
		arg_type = (arg_addr_type << 2) | (arg_value_type << 1);
	} else {
		/* For read_s command */
		new_arg_a = (arg_value & 0xffff) |
			((subsys & 0x1f) << subsys_bit);
		new_arg_b = (arg_addr & 0xffff) << 16;
		arg_type = (arg_value_type << 2) | (arg_addr_type << 1);
	}

	/* new_arg_a is the HW register address to access from or
	 * GPR value store the HW register address
	 * arg_b is the value or register id
	 * bit 55: arg_a type, 1 for HW register
	 * bit 54: arg_b type, 1 for HW register
	 * argType: ('new_arg_a_type', 'arg_b_type', '0')
	 */
	cmdq_append_command_pkt(handle->pkt, code,
		new_arg_a | (arg_type << 21), new_arg_b);
	if (save_op)
		cmdq_save_op_variable_position(handle,
			cmdq_task_get_inst_cnt(handle) - 1);

	return 0;
}

s32 cmdq_append_command(struct cmdqRecStruct *handle,
	enum cmdq_code code, u32 arg_a, u32 arg_b, u32 arg_a_type,
	u32 arg_b_type)
{
	s32 status;
	u32 new_arg_a, new_arg_b, new_code = code;

	status = cmdq_check_before_append(handle);
	if (status < 0) {
		CMDQ_ERR(
			"cannot add command (op:0x%02x arg_a:0x%08x arg_b:0x%08x)\n",
			code, arg_a, arg_b);
		return status;
	}

	/* force insert MARKER if prefetch memory is full
	 * GCE deadlocks if we don't do so
	 */
	if (code != CMDQ_CODE_EOC &&
		cmdq_get_func()->shouldEnablePrefetch(handle->scenario)) {
		u32 prefetchSize = 0;
		s32 threadNo = cmdq_get_func()->getThreadID(handle->scenario,
			handle->secData.is_secure);

		prefetchSize = cmdq_core_get_thread_prefetch_size(threadNo);
		if (prefetchSize > 0 &&
			handle->prefetchCount >= prefetchSize) {
			CMDQ_MSG(
				"prefetchCount(%d) > %d, force insert disable prefetch marker\n",
				handle->prefetchCount, prefetchSize);
			/* Mark END of prefetch section */
			cmdqRecDisablePrefetch(handle);
			/* BEGING of next prefetch section */
			cmdqRecMark(handle);
		} else {
			/* prefetch enabled marker exist */
			if (handle->prefetchCount >= 1) {
				++handle->prefetchCount;
				CMDQ_VERBOSE(
					"handle->prefetchCount:%d %s %d\n",
					handle->prefetchCount, __func__,
					__LINE__);
			}
		}
	}

	CMDQ_VERBOSE("REC:0x%p op:0x%02x arg_a:0x%08x arg_b:0x%08x\n",
		handle, code, arg_a, arg_b);

	switch (code) {
	case CMDQ_CODE_READ:
	case CMDQ_CODE_WRITE:
	case CMDQ_CODE_POLL:
		/* Because read/write/poll have similar format,
		 * handle them together
		 */
		return cmdq_append_wpr_command(handle, code, arg_a, arg_b,
			arg_a_type, arg_b_type);
	case CMDQ_CODE_READ_S:
	case CMDQ_CODE_WRITE_S:
	case CMDQ_CODE_WRITE_S_W_MASK:
		return cmdq_append_rw_s_command(handle, code, arg_a, arg_b,
			arg_a_type, arg_b_type);
	case CMDQ_CODE_MOVE:
		new_arg_b = arg_b;
		new_arg_a = arg_a & 0xffffff;
		break;
	case CMDQ_CODE_JUMP:
		/* note: if jump relative, adg_b maybe negative offset */
		if (arg_a & 0x1)
			new_arg_b = CMDQ_REG_SHIFT_ADDR(arg_b);
		else
			new_arg_b = (s32)CMDQ_REG_SHIFT_ADDR((s64)(s32)arg_b);
		new_arg_a = arg_a & 0x0FFFFFF;
		break;
	case CMDQ_CODE_WFE:
		/* bit 0-11: wait_value, 1
		 * bit 15: to_wait, true
		 * bit 31: to_update, true
		 * bit 16-27: update_value, 0
		 */
		new_arg_b = ((1 << 31) | (1 << 15) | 1);
		new_arg_a = arg_a;
		break;

	case CMDQ_CODE_SET_TOKEN:
		/* this is actually WFE(SYNC) but with different parameter
		 * interpretation
		 * bit 15: to_wait, false
		 * bit 31: to_update, true
		 * bit 16-27: update_value, 1
		 */
		new_arg_b = ((1 << 31) | (1 << 16));
		new_arg_a = arg_a;
		new_code = CMDQ_CODE_WFE;
		break;

	case CMDQ_CODE_WAIT_NO_CLEAR:
		/* bit 0-11: wait_value, 1 */
		/* bit 15: to_wait, true */
		/* bit 31: to_update, false */
		new_arg_b = ((0 << 31) | (1 << 15) | 1);
		new_arg_a = arg_a;
		new_code = CMDQ_CODE_WFE;
		break;

	case CMDQ_CODE_CLEAR_TOKEN:
		/* this is actually WFE(SYNC) but with different parameter
		 * interpretation
		 * bit 15: to_wait, false
		 * bit 31: to_update, true
		 * bit 16-27: update_value, 0
		 */
		new_arg_b = ((1 << 31) | (0 << 16));
		new_arg_a = arg_a;
		new_code = CMDQ_CODE_WFE;
		break;

	case CMDQ_CODE_EOC:
		new_arg_b = arg_b;
		new_arg_a = arg_a & 0x0FFFFFF;
		handle->jump_replace = false;
		break;

	case CMDQ_CODE_RAW:
		new_arg_b = arg_b;
		new_arg_a = arg_a;
		break;

	default:
		return -EFAULT;
	}

	status = cmdq_append_command_pkt(handle->pkt, new_code,
		new_arg_a, new_arg_b);
	if (status < 0) {
		CMDQ_ERR(
			"append cmd fail:%d handle:0x%p op:0x%02x arg a:0x%08x arg b:0x%08x size:%zu\n",
			status, handle, new_code, new_arg_a, new_arg_b,
			handle->pkt->cmd_buf_size);
		return status;
	}

	if (handle->jump_replace && code == CMDQ_CODE_JUMP &&
		(new_arg_a & 0x1) == 0 && handle->ctrl->change_jump) {
		u32 jump_idx;

		if (handle->pkt->cmd_buf_size % CMDQ_CMD_BUFFER_SIZE == 0) {
			jump_idx = cmdq_task_get_inst_cnt(handle);
			CMDQ_MSG(
				"%s handle:0x%p jump is last cmd in buffer, change idx to 0x%x\n",
				__func__, handle, jump_idx);
		} else {
			jump_idx = cmdq_task_get_inst_cnt(handle) - 1;
		}

		cmdq_save_op_variable_position(handle, jump_idx);
	}

	return 0;
}

s32 cmdq_task_set_engine(struct cmdqRecStruct *handle, u64 engineFlag)
{
	if (!handle)
		return -EFAULT;

	CMDQ_VERBOSE("REC:0x%p engineFlag:0x%llx\n", handle, engineFlag);
	handle->engineFlag = engineFlag;

	return 0;
}

static void cmdq_task_release_buffer(struct cmdqRecStruct *handle)
{
	u32 i;

	if (!handle)
		return;

	if (handle->pkt)
		cmdq_pkt_destroy(handle->pkt);
	handle->pkt = NULL;
	handle->cmd_end = NULL;

	/* secure path buffer */
	if (handle->secData.addrMetadatas) {
		kfree(CMDQ_U32_PTR(handle->secData.addrMetadatas));
		handle->secData.addrMetadatas = 0;
		handle->secData.addrMetadataMaxCount = 0;
		handle->secData.addrMetadataCount = 0;
	}
	for (i = 0; i < ARRAY_SIZE(handle->secData.ispMeta.ispBufs); i++)
		vfree(CMDQ_U32_PTR(handle->secData.ispMeta.ispBufs[i].va));

	/* reset local variable setting */
	handle->replace_instr.number = 0;
	if (handle->replace_instr.position) {
		kfree(CMDQ_U32_PTR(handle->replace_instr.position));
		handle->replace_instr.position = 0;
	}

	/* backup register buffer */
	if (handle->reg_values_pa) {
		cmdq_core_free_hw_buffer(cmdq_dev_get(),
			handle->reg_count * sizeof(handle->reg_values[0]),
			handle->reg_values,
			handle->reg_values_pa);
		handle->reg_count = 0;
		handle->reg_values = NULL;
		handle->reg_values_pa = 0;
	}

	/* user data */
	kfree(handle->user_debug_str);
	handle->user_debug_str = NULL;

	kfree(handle->secStatus);
	handle->secStatus = NULL;
}

s32 cmdq_task_reset(struct cmdqRecStruct *handle)
{
	if (!handle)
		return -EFAULT;

	if (handle->running_task)
		cmdq_task_stop_loop(handle);

	cmdq_reset_v3_struct(handle);

	handle->ctrl = cmdq_core_get_controller();
	handle->state = TASK_STATE_IDLE;
	handle->cmd_end = NULL;
	handle->jump_replace = true;
	handle->finalized = false;
	handle->prefetchCount = 0;
	handle->use_sram_buffer = false;
	handle->sram_owner_name = NULL;
	handle->sram_base = 0;
	handle->node_private = NULL;
	handle->engine_clk = 0;
	handle->res_flag_acquire = 0;
	handle->res_flag_release = 0;
	handle->dumpAllocTime = 0;
	handle->reorder = 0;
	handle->submit = 0;
	handle->trigger = 0;
	handle->beginWait = 0;
	handle->gotIRQ = 0;
	handle->wakedUp = 0;
	handle->durAlloc = 0;
	handle->durReclaim = 0;
	handle->durRelease = 0;
	handle->prepare = cmdq_task_prepare;
	handle->unprepare = cmdq_task_unprepare;

	/* store caller info for debug */
	if (current) {
		handle->caller_pid = current->pid;
		memcpy(handle->caller_name, current->comm,
			sizeof(current->comm));
	}

	/* we should have new buffers for new commands */
	cmdq_task_release_buffer(handle);

	if (handle->thread != CMDQ_INVALID_THREAD)
		handle->pkt = cmdq_pkt_create(
			cmdq_helper_mbox_client(handle->thread));
	else
		handle->pkt = cmdq_pkt_create(NULL);

	/* assign client or dev fail, assign cmdq dev directly */
	if (!handle->pkt->dev)
		handle->pkt->dev = cmdq_dev_get();

	/* assign handle to pkt */
	handle->pkt->user_data = (void *)handle;
	/* reset pkt data */
	handle->pkt->priority = CMDQ_REC_DEFAULT_PRIORITY;

	/* reset secure path data */
	if (handle->secData.is_secure) {
		cmdq_task_set_secure(handle, false);
		handle->secData.enginesNeedDAPC = 0LL;
		handle->secData.enginesNeedPortSecurity = 0LL;
	}

	/* performance debug begin */
	if (cmdq_core_profile_exec_enabled()) {
		cmdq_pkt_write(handle->pkt, NULL, CMDQ_TPR_MASK_PA,
			0xffffffff, 0x8000000);
		cmdq_pkt_perf_begin(handle->pkt);
		handle->profile_exec = true;
	}

	return 0;
}

s32 cmdq_task_set_secure(struct cmdqRecStruct *handle, const bool is_secure)
{
	if (handle == NULL)
		return -EFAULT;

	handle->secData.is_secure = is_secure;

	if (handle->finalized) {
		CMDQ_ERR("config secure after finalized\n");
		dump_stack();
	}

	if (handle->pkt->cmd_buf_size > 0)
		CMDQ_MSG("[warn]set secure after record size:%zu\n",
			handle->pkt->cmd_buf_size);

	if (!is_secure) {
		handle->ctrl = cmdq_core_get_controller();
		cmdq_task_reset_thread(handle);
		return 0;
	}
#ifdef CMDQ_SECURE_PATH_SUPPORT
	handle->ctrl = cmdq_sec_get_controller();
	cmdq_task_reset_thread(handle);
	CMDQ_VERBOSE("REC:0x%p secure:%d exclusive thread:%d\n",
		handle, is_secure, handle->thread);
	return 0;
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
	return -EFAULT;
#endif
}

s32 cmdq_task_is_secure(struct cmdqRecStruct *handle)
{
	if (!handle)
		return -EFAULT;

	return handle->secData.is_secure;
}

#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
s32 cmdq_task_set_secure_mode(struct cmdqRecStruct *handle,
	enum CMDQ_DISP_MODE mode)
{
	if (handle == NULL)
		return -EFAULT;

	handle->secData.secMode = mode;
	return 0;
}
#endif

s32 cmdq_task_secure_enable_dapc(struct cmdqRecStruct *handle,
	const u64 engineFlag)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (!handle)
		return -EFAULT;

	handle->secData.enginesNeedDAPC |= engineFlag;
	return 0;
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
	return -EFAULT;
#endif
}

s32 cmdq_task_secure_enable_port_security(
	struct cmdqRecStruct *handle, const u64 engineFlag)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (!handle)
		return -EFAULT;

	handle->secData.enginesNeedPortSecurity |= engineFlag;
	return 0;
#else
	CMDQ_ERR("%s failed since not support secure path\n", __func__);
	return -EFAULT;
#endif
}

s32 cmdq_task_set_secure_meta(struct cmdqRecStruct *handle,
	enum cmdq_sec_rec_meta_type type, void *meta, u32 size)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	if (!cmdq_task_is_secure(handle) || size > CMDQ_SEC_ISP_META_MAX)
		return -EINVAL;

	handle->sec_meta_type = type;
	handle->sec_meta_size = size;
	handle->sec_client_meta = meta;
#endif

	return 0;
}

s32 cmdq_op_write_reg(struct cmdqRecStruct *handle, u32 addr,
	CMDQ_VARIABLE argument, u32 mask)
{
	s32 status = 0;
	enum cmdq_code op_code;
	u32 arg_b_i, arg_b_type;

	if (mask != 0xFFFFFFFF) {
		status = cmdq_append_command(handle, CMDQ_CODE_MOVE, 0,
			~mask, 0, 0);
		if (status != 0)
			return status;
	}

	if (mask != 0xFFFFFFFF)
		op_code = CMDQ_CODE_WRITE_S_W_MASK;
	else
		op_code = CMDQ_CODE_WRITE_S;

	status = cmdq_var_data_type(argument, &arg_b_i, &arg_b_type);
	if (status < 0)
		return status;

	return cmdq_append_command(handle, op_code, addr, arg_b_i, 0,
		arg_b_type);
}

s32 cmdq_op_write_reg_secure(struct cmdqRecStruct *handle, u32 addr,
	enum CMDQ_SEC_ADDR_METADATA_TYPE type, u64 baseHandle,
	u32 offset, u32 size, u32 port)
{
#ifdef CMDQ_SECURE_PATH_SUPPORT
	s32 status;
	s32 writeInstrIndex;
	struct cmdqSecAddrMetadataStruct metadata;
	const u32 mask = 0xFFFFFFFF;

	/* append command */
	status = cmdq_op_write_reg(handle, addr, baseHandle, mask);
	if (status != 0)
		return status;

	/* append to metadata list start from 0 */
	writeInstrIndex = handle->pkt->cmd_buf_size / CMDQ_INST_SIZE - 1;

	memset(&metadata, 0, sizeof(struct cmdqSecAddrMetadataStruct));
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

s32 cmdq_op_poll(struct cmdqRecStruct *handle, u32 addr, u32 value, u32 mask)
{
	s32 status;

	status = cmdq_append_command(handle, CMDQ_CODE_MOVE, 0, ~mask, 0, 0);
	if (status)
		return status;

	status = cmdq_append_command(handle, CMDQ_CODE_POLL, (addr | 0x1),
		value, 0, 0);
	if (status)
		return status;

	return 0;
}

/* Efficient Polling */
s32 cmdq_op_poll_v3(struct cmdqRecStruct *handle, u32 addr, u32 value,
	u32 mask)
{
	/*
	 * Simulate Code
	 * do {
	 *   arg_temp = [addr] & mask
	 *   wait_and_clear (100us);
	 * } while (arg_temp != value);
	 */

	CMDQ_VARIABLE arg_loop_debug = CMDQ_TASK_LOOP_DEBUG_VAR;
	u32 condition_value = value & mask;

	if (!handle)
		return -EINVAL;

	cmdq_op_assign(handle, &handle->arg_value, condition_value);
	cmdq_op_assign(handle, &arg_loop_debug, 0);
	cmdq_op_do_while(handle);
		cmdq_op_read_reg(handle, addr, &handle->arg_source, mask);
		cmdq_op_add(handle, &arg_loop_debug, arg_loop_debug, 1);
		cmdq_op_wait(handle, CMDQ_EVENT_TIMER_00 +
			CMDQ_POLLING_TPR_MASK_BIT);
	cmdq_op_end_do_while(handle, handle->arg_value, CMDQ_NOT_EQUAL,
		handle->arg_source);
	return 0;
}

static s32 cmdq_get_event_op_id(enum cmdq_event event)
{
	s32 event_id = 0;

	if (event < 0 || CMDQ_SYNC_TOKEN_MAX <= event) {
		CMDQ_ERR("Invalid input event:%d\n", (s32)event);
		return -EINVAL;
	}

	event_id = cmdq_core_get_event_value(event);
	if (event_id < 0) {
		CMDQ_ERR("Invalid event:%d ID:%d\n", (s32)event,
			(s32)event_id);
		return -EINVAL;
	}

	return event_id;
}

s32 cmdq_op_wait(struct cmdqRecStruct *handle, enum cmdq_event event)
{
	s32 arg_a = cmdq_get_event_op_id(event);

	if (arg_a < 0 || !handle)
		return -EINVAL;

	return cmdq_pkt_wfe(handle->pkt, arg_a);
}

s32 cmdq_op_wait_no_clear(struct cmdqRecStruct *handle,
	enum cmdq_event event)
{
	s32 arg_a = cmdq_get_event_op_id(event);

	if (arg_a < 0 || !handle)
		return -EINVAL;

	return cmdq_pkt_wait_no_clear(handle->pkt, arg_a);
}

s32 cmdq_op_clear_event(struct cmdqRecStruct *handle,
	enum cmdq_event event)
{
	s32 arg_a = cmdq_get_event_op_id(event);

	if (arg_a < 0 || !handle)
		return -EINVAL;

	return cmdq_pkt_clear_event(handle->pkt, arg_a);
}

s32 cmdq_op_set_event(struct cmdqRecStruct *handle, enum cmdq_event event)
{
	s32 arg_a = cmdq_get_event_op_id(event);

	if (arg_a < 0 || !handle)
		return -EINVAL;

	return cmdq_pkt_set_event(handle->pkt, arg_a);
}

s32 cmdq_op_replace_overwrite_cpr(struct cmdqRecStruct *handle, u32 index,
	s32 new_arg_a, s32 new_arg_b, s32 new_arg_c)
{
	/* check instruction is wait or not */
	u32 *va;
	u32 offset = index * CMDQ_INST_SIZE;

	if (!handle)
		return -EFAULT;

	if (offset > (handle->pkt->cmd_buf_size - CMDQ_INST_SIZE)) {
		CMDQ_ERR("======REC overwrite offset (%d) invalid:%zu\n",
			offset, handle->pkt->cmd_buf_size);
		return -EFAULT;
	}

	va = (u32 *)cmdq_pkt_get_va_by_offset(handle->pkt, offset);
	if (!va) {
		CMDQ_LOG("Cannot find va, handle:%p pkt:%p offset:%u\n",
			handle, handle->pkt, offset);
		return -EINVAL;
	}
	if (new_arg_a >= 0)
		va[1] = (va[1] & 0xffff0000) | (new_arg_a & 0xffff);
	if (new_arg_b >= 0)
		va[0] = (va[0] & 0x0000ffff) | ((new_arg_b & 0xffff) << 16);
	if (new_arg_c >= 0)
		va[0] = (va[0] & 0xffff0000) | (new_arg_c & 0xffff);
	CMDQ_MSG("======REC 0x%p replace cpr cmd(%d):0x%08x 0x%08x\n",
		handle, index, va[0], va[1]);

	return 0;
}

s32 cmdq_op_read_to_data_register(struct cmdqRecStruct *handle, u32 hw_addr,
	enum cmdq_gpr_reg dst_data_reg)
{
#ifdef CMDQ_GPR_SUPPORT
	enum cmdq_code op_code;
	u32 arg_a_i, arg_b_i;
	u32 arg_a_type, arg_b_type;

	if (dst_data_reg < CMDQ_DATA_REG_JPEG_DST) {
		op_code = CMDQ_CODE_READ_S;
		arg_a_i = dst_data_reg + CMDQ_GPR_V3_OFFSET;
		arg_a_type = 1;
		arg_b_i = hw_addr;
		arg_b_type = 0;
	} else {
		op_code = CMDQ_CODE_READ;
		arg_a_i = hw_addr;
		arg_a_type = 0;
		arg_b_i = dst_data_reg;
		arg_b_type = 1;
	}

	/* read from hwRegAddr(arg_a) to dstDataReg(arg_b) */
	return cmdq_append_command(handle, op_code, arg_a_i, arg_b_i,
		arg_a_type, arg_b_type);
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif
}

s32 cmdq_op_write_from_data_register(struct cmdqRecStruct *handle,
	enum cmdq_gpr_reg src_data_reg, u32 hw_addr)
{
#ifdef CMDQ_GPR_SUPPORT
	enum cmdq_code op_code;
	u32 arg_b_i;

	if (src_data_reg < CMDQ_DATA_REG_JPEG_DST) {
		op_code = CMDQ_CODE_WRITE_S;
		arg_b_i = src_data_reg + CMDQ_GPR_V3_OFFSET;
	} else {
		op_code = CMDQ_CODE_WRITE;
		arg_b_i = src_data_reg;
	}

	/* write HW register(arg_a) with data of GPR data register(arg_b) */
	return cmdq_append_command(handle, op_code, hw_addr, arg_b_i, 0, 1);
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

s32 cmdq_op_write_reg_ex(struct cmdqRecStruct *handle, u32 addr,
	CMDQ_VARIABLE argument, u32 mask)
{
	return cmdq_pkt_write_value_addr(handle->pkt, addr, argument, mask);
}

#define CMDQ_GET_ARG_B(arg)		(((arg) & GENMASK(31, 16)) >> 16)
#define CMDQ_GET_ARG_C(arg)		((arg) & GENMASK(15, 0))
s32 cmdq_op_acquire(struct cmdqRecStruct *handle, enum cmdq_event event)
{
	s32 arg_a = cmdq_get_event_op_id(event);
	u32 arg_b;

	if (arg_a < 0 || arg_a >= CMDQ_EVENT_MAX || !handle)
		return -EINVAL;

	/*
	 * WFE arg_b
	 * bit 0-11: wait value
	 * bit 15: 1 - wait, 0 - no wait
	 * bit 16-27: update value
	 * bit 31: 1 - update, 0 - no update
	 */
	arg_b = CMDQ_WFE_UPDATE | CMDQ_WFE_UPDATE_VALUE | CMDQ_WFE_WAIT;
	return cmdq_pkt_append_command(handle->pkt, CMDQ_GET_ARG_C(arg_b),
		CMDQ_GET_ARG_B(arg_b), arg_a,
		0, 0, 0, 0, CMDQ_CODE_WFE);
}

s32 cmdq_op_write_from_reg(struct cmdqRecStruct *handle,
	u32 write_reg, u32 from_reg)
{
	s32 status;

	if (!handle)
		return -EINVAL;

	do {
		status = cmdq_op_read_reg(handle, from_reg,
			&handle->arg_value, ~0);
		CMDQ_CHECK_AND_BREAK_STATUS(status);

		status = cmdq_op_write_reg(handle, write_reg,
			handle->arg_value, ~0);
	} while (0);

	return status;
}

s32 cmdq_alloc_write_addr(u32 count, dma_addr_t *paStart, u32 clt, void *fp)
{
	return cmdqCoreAllocWriteAddress(count, paStart, clt);
}

s32 cmdq_free_write_addr(dma_addr_t paStart, u32 clt)
{
	return cmdqCoreFreeWriteAddress(paStart, clt);
}

s32 cmdq_free_write_addr_by_node(u32 clt, void *fp)
{
	return 0;
}

/* Allocate 32-bit register backup slot */
s32 cmdq_alloc_mem(cmdqBackupSlotHandle *p_h_backup_slot, u32 slotCount)
{
#ifdef CMDQ_GPR_SUPPORT

	dma_addr_t paStart = 0;
	int status = 0;

	if (p_h_backup_slot == NULL)
		return -EINVAL;

	status = cmdqCoreAllocWriteAddress(slotCount, &paStart, CMDQ_CLT_DISP);
	*p_h_backup_slot = paStart;

	return status;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

/* Read 32-bit register backup slot by index */
s32 cmdq_cpu_read_mem(cmdqBackupSlotHandle h_backup_slot, u32 slot_index,
	u32 *value)
{
#ifdef CMDQ_GPR_SUPPORT

	if (value == NULL)
		return -EINVAL;

	if (!h_backup_slot) {
		CMDQ_ERR("%s, h_backup_slot is NULL\n", __func__);
		return -EINVAL;
	}

	*value = cmdqCoreReadWriteAddress(h_backup_slot + slot_index *
		sizeof(u32));

	return 0;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

s32 cmdq_cpu_write_mem(cmdqBackupSlotHandle h_backup_slot, u32 slot_index,
	u32 value)
{
#ifdef CMDQ_GPR_SUPPORT

	int status = 0;
	/* set the slot value directly */
	status = cmdqCoreWriteWriteAddress(h_backup_slot + slot_index *
		sizeof(u32), value);
	return status;

#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

/* Free allocated backup slot. DO NOT free them before corresponding
 * task finishes. Becareful on AsyncFlush use cases.
 */
s32 cmdq_free_mem(cmdqBackupSlotHandle h_backup_slot)
{
#ifdef CMDQ_GPR_SUPPORT
	return cmdqCoreFreeWriteAddress(h_backup_slot, CMDQ_CLT_DISP);
#else
	CMDQ_ERR("func:%s failed since CMDQ doesn't support GPR\n", __func__);
	return -EFAULT;
#endif				/* CMDQ_GPR_SUPPORT */
}

/* Insert instructions to backup given 32-bit HW register
 * to a backup slot.
 * You can use cmdq_cpu_read_mem() to retrieve the result
 * AFTER cmdq_task_flush() returns, or INSIDE the callback of
 * cmdq_task_flush_async_callback().
 */
s32 cmdq_op_read_reg_to_mem(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index, u32 addr)
{
	const dma_addr_t dram_addr = h_backup_slot + slot_index * sizeof(u32);
	CMDQ_VARIABLE var_mem_addr = CMDQ_TASK_TEMP_CPR_VAR;
	s32 status;

	if (!handle)
		return -EINVAL;

	do {
		status = cmdq_op_read_reg(handle, addr,
			&handle->arg_value, ~0);
		CMDQ_CHECK_AND_BREAK_STATUS(status);

		status = cmdq_op_assign(handle, &var_mem_addr, (u32)dram_addr);
		CMDQ_CHECK_AND_BREAK_STATUS(status);

		status = cmdq_append_command(handle, CMDQ_CODE_WRITE_S,
			(u32)(var_mem_addr & 0xFFFF),
			(u32)(handle->arg_value & 0xFFFF),
			1, 1);
	} while (0);

	return status;
}

s32 cmdq_op_read_mem_to_reg(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index, u32 addr)
{
	const dma_addr_t dram_addr = h_backup_slot + slot_index * sizeof(u32);
	CMDQ_VARIABLE var_mem_addr = CMDQ_TASK_TEMP_CPR_VAR;
	s32 status;

	if (!handle)
		return -EINVAL;

	do {
		status = cmdq_create_variable_if_need(handle,
			&handle->arg_value);
		CMDQ_CHECK_AND_BREAK_STATUS(status);

		status = cmdq_op_assign(handle, &var_mem_addr, (u32)dram_addr);
		CMDQ_CHECK_AND_BREAK_STATUS(status);

		/* read dram to temp var */
		status = cmdq_append_command(handle, CMDQ_CODE_READ_S,
			(u32)(handle->arg_value & 0xFFFF),
			(u32)(var_mem_addr & 0xFFFF), 1, 1);
		CMDQ_CHECK_AND_BREAK_STATUS(status);

		status = cmdq_op_write_reg(handle, addr,
			handle->arg_value, ~0);
	} while (0);

	return status;
}

s32 cmdq_op_write_mem(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index, u32 value)
{
	const dma_addr_t dram_addr = h_backup_slot + slot_index * sizeof(u32);
	CMDQ_VARIABLE var_mem_addr = CMDQ_TASK_TEMP_CPR_VAR;
	s32 status;

	if (!handle)
		return -EINVAL;

	do {
		status = cmdq_op_assign(handle, &handle->arg_value, value);
		CMDQ_CHECK_AND_BREAK_STATUS(status);

		status = cmdq_op_assign(handle, &var_mem_addr, (u32)dram_addr);
		CMDQ_CHECK_AND_BREAK_STATUS(status);

		status = cmdq_append_command(handle, CMDQ_CODE_WRITE_S,
			var_mem_addr, handle->arg_value, 1, 1);
	} while (0);

	return status;
}

s32 cmdq_op_finalize_command(struct cmdqRecStruct *handle, bool loop)
{
	s32 status = 0;
	u32 arg_b = 0;

	if (!handle)
		return -EFAULT;

	if (handle->if_stack_node) {
		CMDQ_ERR(
			"Incorrect if-else statement, please review your if-else instructions.");
		return -EFAULT;
	}

	if (handle->while_stack_node) {
		CMDQ_ERR(
			"Incorrect while statement, please review your while instructions.");
		return -EFAULT;
	}

	if (!handle->finalized) {
		if ((handle->prefetchCount > 0) &&
			cmdq_get_func()->shouldEnablePrefetch(
			handle->scenario)) {
			CMDQ_ERR(
				"not insert prefetch disable marker when prefetch enabled, prefetchCount:%d\n",
				handle->prefetchCount);
			cmdq_pkt_dump_command(handle);

			status = -EFAULT;
			return status;
		}

		/* performance debug end before finalize */
		if (loop)
			handle->profile_exec = false;
		else if (handle->profile_exec)
			cmdq_pkt_perf_end(handle->pkt);

		/* insert EOF instruction */
		arg_b = 0x1;	/* generate IRQ for each command iteration */
#ifndef CMDQ_DEBUG_LOOP_IRQ
		/* no generate IRQ for loop thread to save power */
		if (loop && !cmdq_get_func()->force_loop_irq(handle->scenario))
			arg_b = 0x0;
#endif
		/* no generate IRQ for delay loop thread */
		if (handle->scenario == CMDQ_SCENARIO_TIMER_LOOP)
			arg_b = 0x0;

#if defined(CONFIG_MTK_SEC_VIDEO_PATH_SUPPORT) || \
	defined(CONFIG_MTK_CAM_SECURITY_SUPPORT)
		if (handle->secData.is_secure) {
			status = cmdq_sec_insert_backup_cookie_instr(handle,
				handle->thread);
			if (status < 0) {
				CMDQ_ERR(
					"insert backup cookie fail task:%p status:%d size:%zu thread:%d\n",
					handle, status,
					handle->pkt->cmd_buf_size,
					handle->thread);
				return status;
			}
		}
#endif
		status = cmdq_append_command(handle, CMDQ_CODE_EOC,
			0, arg_b, 0, 0);

		if (status != 0)
			return status;

		/* insert JUMP to loop beginning or as a scheduling mark(8) */
		status = cmdq_append_command(handle, CMDQ_CODE_JUMP,
			0, /* not absolute */
			loop ? -handle->pkt->cmd_buf_size : 8,
			0, 0);
		if (status)
			return status;

		handle->finalized = true;
		handle->pkt->loop = loop;

		CMDQ_MSG(
			"finalized handle:0x%p buf size:%zu size:%zu avail:%zu\n",
			handle, handle->pkt->cmd_buf_size,
			handle->pkt->buf_size, handle->pkt->avail_buf_size);
	}

	return status;
}

s32 cmdq_setup_sec_data_of_command_desc_by_rec_handle(
	struct cmdqCommandStruct *pDesc, struct cmdqRecStruct *handle)
{
	if (!handle || !pDesc)
		return -EINVAL;

	/* fill field from user's request */
	pDesc->secData.is_secure = handle->secData.is_secure;
	pDesc->secData.enginesNeedDAPC = handle->secData.enginesNeedDAPC;
	pDesc->secData.enginesNeedPortSecurity =
		handle->secData.enginesNeedPortSecurity;

	pDesc->secData.addrMetadataCount = handle->secData.addrMetadataCount;
	pDesc->secData.addrMetadatas = handle->secData.addrMetadatas;
	pDesc->secData.addrMetadataMaxCount =
		handle->secData.addrMetadataMaxCount;

	/* init reserved field */
	pDesc->secData.resetExecCnt = false;
	pDesc->secData.waitCookie = 0;

	return 0;
}

s32 cmdq_setup_replace_of_command_desc_by_rec_handle(
	struct cmdqCommandStruct *pDesc, struct cmdqRecStruct *handle)
{
	if (!handle || !pDesc)
		return -EINVAL;

	/* fill field from user's request */
	pDesc->replace_instr.number = handle->replace_instr.number;
	pDesc->replace_instr.position = handle->replace_instr.position;

	return 0;
}

s32 cmdq_rec_setup_profile_marker_data(
	struct cmdqCommandStruct *pDesc, struct cmdqRecStruct *handle)
{
	u32 i;

	if (!handle || !pDesc)
		return -EINVAL;

	pDesc->profileMarker.count = handle->profileMarker.count;
	pDesc->profileMarker.hSlot = handle->profileMarker.hSlot;

	for (i = 0; i < CMDQ_MAX_PROFILE_MARKER_IN_TASK; i++)
		pDesc->profileMarker.tag[i] = handle->profileMarker.tag[i];

	return 0;
}

void cmdq_task_prepare(struct cmdqRecStruct *handle)
{
	if (!handle)
		return;

	/* enable resource clock for display task */
	if (handle->res_flag_acquire)
		cmdq_mdp_enable_res(handle->res_flag_acquire, true);
}

void cmdq_task_unprepare(struct cmdqRecStruct *handle)
{
	if (!handle)
		return;

	/* disable resource clock for display task */
	if (handle->res_flag_release)
		cmdq_mdp_enable_res(handle->res_flag_release, false);
}

void cmdq_task_release_property(struct cmdqRecStruct *handle)
{
	if (!handle)
		return;

	kfree(handle->prop_addr);
	handle->prop_addr = NULL;
	handle->prop_size = 0;
}

s32 cmdq_task_update_property(struct cmdqRecStruct *handle,
	void *prop_addr, u32 prop_size)
{
	void *pprop_addr;

	if (!handle || !prop_addr || !prop_size)
		return -EINVAL;

	CMDQ_MSG("%s handle:0x%p prop_addr:0x%p prop_size:%u",
		__func__, handle, prop_addr, prop_size);

	pprop_addr = kzalloc(prop_size, GFP_KERNEL);
	if (!pprop_addr)
		return -ENOMEM;

	cmdq_task_release_property(handle);

	memcpy(pprop_addr, prop_addr, prop_size);
	handle->prop_addr = pprop_addr;
	handle->prop_size = prop_size;

	return 0;
}

s32 cmdq_task_flush(struct cmdqRecStruct *handle)
{
	s32 status;

	status = cmdq_op_finalize_command(handle, false);
	if (status < 0)
		return status;

	return cmdq_pkt_flush_ex(handle);
}

s32 cmdq_task_append_backup_reg(struct cmdqRecStruct *handle,
	u32 reg_count, u32 *addrs)
{
	u32 i;

	if (!handle)
		return -EINVAL;

	if (handle->reg_count) {
		CMDQ_ERR("task already has backup regs count:%u\n",
			handle->reg_count);
		return -EINVAL;
	}

	handle->reg_count = reg_count;
	handle->reg_values = cmdq_core_alloc_hw_buffer(cmdq_dev_get(),
		reg_count * sizeof(handle->reg_values[0]),
		&handle->reg_values_pa,
		GFP_KERNEL);
	if (!handle->reg_values)
		return -ENOMEM;

	/* insert commands to read back regs into slot */
	for (i = 0; i < reg_count; i++)
		cmdq_op_read_reg_to_mem(handle, handle->reg_values_pa,
			i, addrs[i]);

	return 0;
}

s32 cmdq_task_flush_and_read_register(struct cmdqRecStruct *handle,
	u32 reg_count, u32 *addrs, u32 *values_out)
{
	s32 status;

	status = cmdq_task_append_backup_reg(handle, reg_count, addrs);
	if (status < 0)
		return status;

	status = cmdq_op_finalize_command(handle, false);
	if (status < 0)
		return status;

	status = cmdq_pkt_flush_ex(handle);
	if (status < 0)
		return status;

	memcpy(values_out, handle->reg_values,
		reg_count * sizeof(handle->reg_values[0]));

	return 0;
}

static s32 cmdq_task_async_callback_auto_release(unsigned long data)
{
	struct cmdq_async_data *async = (struct cmdq_async_data *)data;

	if (async->cb)
		async->cb(async->user_data);
	cmdq_task_destroy(async->handle);
	kfree(async);

	return 0;
}

s32 cmdq_task_flush_async(struct cmdqRecStruct *handle)
{
	return cmdq_task_flush_async_callback(handle, NULL, 0);
}

s32 cmdq_task_flush_async_callback(struct cmdqRecStruct *handle,
	CmdqAsyncFlushCB callback, u64 user_data)
{
	s32 status;
	struct cmdqRecStruct *flush_handle;
	struct cmdq_async_data *async;

	async = kzalloc(sizeof(*async), GFP_KERNEL);
	if (!async) {
		CMDQ_ERR("cannot allocate async data\n");
		return -ENOMEM;
	}

	status = cmdq_task_duplicate(handle, &flush_handle);
	if (status < 0) {
		kfree(async);
		return status;
	}

	status = cmdq_op_finalize_command(flush_handle, handle->pkt->loop);
	if (status < 0) {
		kfree(async);
		return status;
	}

	async->cb = callback;
	async->user_data = user_data;
	async->handle = flush_handle;

	return cmdq_pkt_flush_async_ex(flush_handle,
		cmdq_task_async_callback_auto_release,
			(unsigned long)async, true);
}

s32 cmdq_task_flush_async_destroy(struct cmdqRecStruct *handle)
{
	s32 status;
	struct cmdq_async_data *async;

	async = kzalloc(sizeof(*async), GFP_KERNEL);
	if (!async) {
		CMDQ_ERR("cannot allocate async data\n");
		return -ENOMEM;
	}

	status = cmdq_op_finalize_command(handle, false);
	if (status < 0) {
		kfree(async);
		return status;
	}

	async->cb = NULL;
	async->user_data = 0;
	async->handle = handle;

	return cmdq_pkt_flush_async_ex(handle,
		cmdq_task_async_callback_auto_release,
			(unsigned long)async, true);
}

static s32 cmdq_dummy_irq_callback(unsigned long data)
{
	return 0;
}

s32 _cmdq_task_start_loop_callback(struct cmdqRecStruct *handle,
	CmdqInterruptCB cb, unsigned long user_data,
	const char *sram_owner_name)
{
	s32 status;

	status = cmdq_op_finalize_command(handle, true);
	if (status < 0)
		return status;

	CMDQ_MSG("%s handle:0x%p state:%d\n",
		__func__, handle, handle->state);

	handle->loop_cb = cb;
	handle->loop_user_data = user_data;

	if (strlen(sram_owner_name) > 0) {
		CMDQ_MSG("Submit task loop in SRAM:%s\n", sram_owner_name);
		handle->use_sram_buffer = true;
		handle->sram_owner_name = sram_owner_name;
	}

	return cmdq_pkt_flush_async_ex(handle, NULL, 0, false);
}

s32 cmdq_task_start_loop(struct cmdqRecStruct *handle)
{
	return _cmdq_task_start_loop_callback(handle, NULL, 0, "");
}

s32 cmdq_task_start_loop_callback(struct cmdqRecStruct *handle,
	CmdqInterruptCB loopCB, unsigned long loopData)
{
	return _cmdq_task_start_loop_callback(handle, loopCB, loopData, "");
}

s32 cmdq_task_start_loop_sram(struct cmdqRecStruct *handle,
	const char *sram_owner_name)
{
	return _cmdq_task_start_loop_callback(handle, &cmdq_dummy_irq_callback,
		0, sram_owner_name);
}

s32 cmdq_task_stop_loop(struct cmdqRecStruct *handle)
{
	return cmdq_pkt_stop(handle);
}

s32 cmdq_task_copy_to_sram(dma_addr_t pa_src, u32 sram_dest, size_t size)
{
	struct cmdqRecStruct *handle;
	u32 i;
	s32 status;
	unsigned long long duration;
	CMDQ_VARIABLE pa_cpr, sram_cpr;

	CMDQ_MSG("%s DRAM addr:0x%pa SRAM addr:%d\n",
		__func__, &pa_src, sram_dest);

	cmdq_op_init_variable(&pa_cpr);
	cmdq_task_create(CMDQ_SCENARIO_MOVE, &handle);
	status = cmdq_task_reset(handle);
	if (status < 0) {
		cmdq_task_destroy(handle);
		return status;
	}
	handle->pkt->priority = CMDQ_REC_MAX_PRIORITY;

	for (i = 0; i < size / sizeof(u32); i++) {
		cmdq_op_assign(handle, &pa_cpr, (u32)pa_src + i * sizeof(u32));
		cmdq_op_init_global_cpr_variable(&sram_cpr, sram_dest + i);
		cmdq_append_command(handle, CMDQ_CODE_READ_S,
			(u32)sram_cpr, (u32)pa_cpr, 1, 1);
	}

	duration = sched_clock();
	status = cmdq_task_flush(handle);

	duration = sched_clock() - duration;
	CMDQ_MSG("%s result:%d cost time:%u us\n",
		__func__, status, (u32)duration);

	cmdq_task_destroy(handle);
	return status;
}

s32 cmdq_task_copy_from_sram(dma_addr_t pa_dest, u32 sram_src, size_t size)
{
	struct cmdqRecStruct *handle;
	u32 i;
	unsigned long long duration;
	CMDQ_VARIABLE pa_cpr, sram_cpr;
	s32 status;

	CMDQ_MSG("%s DRAM addr:0x%pa SRAM addr:%d\n",
		__func__, &pa_dest, sram_src);
	cmdq_op_init_variable(&pa_cpr);
	cmdq_task_create(CMDQ_SCENARIO_MOVE, &handle);
	status = cmdq_task_reset(handle);
	if (status < 0) {
		cmdq_task_destroy(handle);
		return status;
	}

	for (i = 0; i < size / sizeof(u32); i++) {
		cmdq_op_assign(handle, &pa_cpr, (u32)pa_dest + i * sizeof(u32));
		sram_cpr = CMDQ_ARG_CPR_START + sram_src + i;
		cmdq_append_command(handle, CMDQ_CODE_WRITE_S,
			(u32)pa_cpr, (u32)sram_cpr, 1, 1);
	}
	/*cmdq_pkt_dump_command(handle);*/

	duration = sched_clock();
	cmdq_task_flush(handle);

	duration = sched_clock() - duration;
	CMDQ_MSG("%s cost time:%u us\n", __func__, (u32)duration);

	cmdq_task_destroy(handle);
	return 0;
}

s32 cmdq_task_get_inst_cnt(struct cmdqRecStruct *handle)
{
	if (!handle)
		return 0;
	return handle->pkt->cmd_buf_size / CMDQ_INST_SIZE;
}

s32 cmdq_op_profile_marker(struct cmdqRecStruct *handle, const char *tag)
{
	s32 status;
	s32 index;
	cmdqBackupSlotHandle hSlot;
	dma_addr_t allocatedStartPA;

	if (!handle)
		return -EINVAL;

	do {
		allocatedStartPA = 0;
		status = 0;

		/* allocate temp slot for GCE to store timestamp info
		 * those timestamp info will copy to record strute
		 * after task execute done
		 */
		if (!handle->profileMarker.count &&
			!handle->profileMarker.hSlot) {
			status = cmdqCoreAllocWriteAddress(
				CMDQ_MAX_PROFILE_MARKER_IN_TASK,
				&allocatedStartPA, CMDQ_CLT_DISP);
			if (status < 0) {
				CMDQ_ERR(
					"[REC][PROF_MARKER]allocate failed, status:%d\n",
					status);
				break;
			}

			handle->profileMarker.hSlot = 0LL | (allocatedStartPA);

			CMDQ_VERBOSE(
				"[REC][PROF_MARKER]update handle(%p) slot start PA:0x%pa(0x%llx)\n",
				handle, &allocatedStartPA,
				handle->profileMarker.hSlot);
		}

		/* insert instruciton */
		index = handle->profileMarker.count;
		hSlot = (cmdqBackupSlotHandle)(handle->profileMarker.hSlot);

		if (index >= CMDQ_MAX_PROFILE_MARKER_IN_TASK) {
			CMDQ_ERR(
				"[REC][PROF_MARKER]insert profile maker failed since already reach max count\n");
			status = -EFAULT;
			break;
		}

		CMDQ_VERBOSE(
			"[REC][PROF_MARKER]inserting profile instr, handle:0x%p slot:0x%pa(0x%llx) index:%d tag:%s\n",
			handle, &hSlot, handle->profileMarker.hSlot, index,
			tag);

		cmdq_op_backup_TPR(handle, hSlot, index);

		handle->profileMarker.tag[index] =
			(cmdqU32Ptr_t)(unsigned long)tag;
		handle->profileMarker.count += 1;
	} while (0);

	return status;
}

s32 cmdq_task_destroy(struct cmdqRecStruct *handle)
{
	if (!handle) {
		CMDQ_ERR("try to release null handle\n");
		dump_stack();
		return -EINVAL;
	}

	CMDQ_SYSTRACE_BEGIN("%s\n", __func__);

	CMDQ_MSG("release handle:0x%p pkt:0x%p state:%d exec:%d irq:%llu\n",
		handle, handle->pkt, handle->state,
		(s32)atomic_read(&handle->exec), handle->gotIRQ);

	if (handle->running_task)
		cmdq_task_stop_loop(handle);

	/* free internal buffers */
	cmdq_task_release_buffer(handle);

	/* must release for dynamic thread */
	if (handle->thd_dispatch == CMDQ_THREAD_ACQUIRE &&
		handle->thread != CMDQ_INVALID_THREAD) {
		cmdq_core_release_thread(handle->scenario, handle->thread);
		handle->thread = CMDQ_INVALID_THREAD;
		handle->thd_dispatch = CMDQ_THREAD_NOTSET;
	}

	cmdq_task_release_property(handle);

	kfree(handle);

	CMDQ_SYSTRACE_END();

	return 0;
}

s32 cmdq_op_set_nop(struct cmdqRecStruct *handle, u32 index)
{
	if (!handle)
		return -EFAULT;

	CMDQ_MSG("======REC 0x%p Set NOP size:%zu\n",
		handle, handle->pkt->cmd_buf_size);
	cmdq_append_command_pkt(handle->pkt, CMDQ_CODE_JUMP, 0, 8);
	CMDQ_MSG("======REC 0x%p END\n", handle);

	return index;
}

s32 cmdq_task_query_offset(struct cmdqRecStruct *handle, u32 startIndex,
	const enum cmdq_code opCode, enum cmdq_event event)
{
	s32 offset = -1;
	u32 arg_a, arg_b;
	u64 inst;
	u32 buf_cnt = 0, size;
	struct cmdq_pkt_buffer *buf;
	void *va;

	if (handle == NULL || (startIndex * CMDQ_INST_SIZE) >
		(handle->pkt->cmd_buf_size - CMDQ_INST_SIZE))
		return -EFAULT;

	switch (opCode) {
	case CMDQ_CODE_WFE:
		/* bit 0-11: wait_value, 1
		 * bit 15: to_wait, true
		 * bit 31: to_update, true
		 * bit 16-27: update_value, 0
		 */
		arg_b = ((1 << 31) | (1 << 15) | 1);
		arg_a = (CMDQ_CODE_WFE << 24) |
			cmdq_core_get_event_value(event);
		break;
	case CMDQ_CODE_SET_TOKEN:
		/* this is actually WFE(SYNC) but with different parameter
		 * interpretation
		 * bit 15: to_wait, false
		 * bit 31: to_update, true
		 * bit 16-27: update_value, 1
		 */
		arg_b = ((1 << 31) | (1 << 16));
		arg_a = (CMDQ_CODE_WFE << 24) |
			cmdq_core_get_event_value(event);
		break;
	case CMDQ_CODE_WAIT_NO_CLEAR:
		/* bit 0-11: wait_value, 1
		 * bit 15: to_wait, true
		 * bit 31: to_update, false
		 */
		arg_b = ((0 << 31) | (1 << 15) | 1);
		arg_a = (CMDQ_CODE_WFE << 24) |
			cmdq_core_get_event_value(event);
		break;
	case CMDQ_CODE_CLEAR_TOKEN:
		/* this is actually WFE(SYNC) but with different parameter */
		/* interpretation */
		/* bit 15: to_wait, false */
		/* bit 31: to_update, true */
		/* bit 16-27: update_value, 0 */
		arg_b = ((1 << 31) | (0 << 16));
		arg_a = (CMDQ_CODE_WFE << 24) |
			cmdq_core_get_event_value(event);
		break;
	case CMDQ_CODE_PREFETCH_ENABLE:
		/* this is actually MARKER but with different parameter
		 * interpretation
		 * bit 53: non_suspendable, true
		 * bit 48: no_inc_exec_cmds_cnt, true
		 * bit 20: prefetch_marker, true
		 * bit 17: prefetch_marker_en, true
		 * bit 16: prefetch_en, true
		 */
		arg_b = ((1 << 20) | (1 << 17) | (1 << 16));
		arg_a = (CMDQ_CODE_EOC << 24) | (0x1 << (53 - 32)) |
			(0x1 << (48 - 32));
		break;
	case CMDQ_CODE_PREFETCH_DISABLE:
		/* this is actually MARKER but with different parameter
		 * interpretation
		 * bit 48: no_inc_exec_cmds_cnt, true
		 * bit 20: prefetch_marker, true
		 */
		arg_b = (1 << 20);
		arg_a = (CMDQ_CODE_EOC << 24) | (0x1 << (48 - 32));
		break;
	default:
		CMDQ_MSG("This offset of instruction can not be queried.\n");
		return -EFAULT;
	}

	inst = (u64)arg_a << 32 | arg_b;
	list_for_each_entry(buf, &handle->pkt->buf, list_entry) {
		if (list_is_last(&buf->list_entry, &handle->pkt->buf))
			size = CMDQ_CMD_BUFFER_SIZE -
				handle->pkt->avail_buf_size;
		else
			size = CMDQ_CMD_BUFFER_SIZE;
		for (va = buf->va_base; va < buf->va_base + size;
			va += CMDQ_INST_SIZE) {
			if (*((u64 *)va) == inst) {
				offset = buf_cnt * CMDQ_CMD_BUFFER_SIZE +
					va - buf->va_base;
				break;
			}
		}
		if (offset >= 0)
			break;
		buf_cnt++;
	}

	if (offset < 0) {
		/* Can not find the offset of desired instruction */
		CMDQ_LOG("Can not find the offset of desired instruction\n");
	}

	return offset;
}

s32 cmdq_resource_acquire(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent)
{
	bool result = false;

	if (!handle)
		return -EINVAL;

	result = cmdq_mdp_acquire_resource(resourceEvent,
		&handle->res_flag_acquire);
	if (!result) {
		CMDQ_MSG(
			"[Res]Acquire resource (event:%d) failed, handle:0x%p\n",
			resourceEvent, handle);
		return -EFAULT;
	}
	return 0;
}

s32 cmdq_resource_acquire_and_write(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent, u32 addr, u32 value, u32 mask)
{
	s32 status;

	status = cmdq_resource_acquire(handle, resourceEvent);
	if (status < 0)
		return status;

	return cmdq_op_write_reg(handle, addr, value, mask);
}

s32 cmdq_resource_release(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent)
{
	if (!handle)
		return -EINVAL;

	cmdq_mdp_release_resource(resourceEvent,
		&handle->res_flag_release);
	return cmdq_op_set_event(handle, resourceEvent);
}

s32 cmdq_resource_release_and_write(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent, u32 addr, u32 value, u32 mask)
{
	s32 result;

	if (!handle)
		return -EINVAL;

	cmdq_mdp_release_resource(resourceEvent,
		&handle->res_flag_release);
	result = cmdq_op_write_reg(handle, addr, value, mask);
	if (result >= 0)
		return cmdq_op_set_event(handle, resourceEvent);

	CMDQ_ERR(
		"[Res]Write instruction fail and not release resource result:%d\n",
		result);
	return result;
}

static s32 cmdq_append_logic_command(struct cmdqRecStruct *handle,
	CMDQ_VARIABLE *arg_a, CMDQ_VARIABLE arg_b, enum CMDQ_LOGIC_ENUM s_op,
	CMDQ_VARIABLE arg_c)
{
	s32 status = 0;
	u32 arg_a_i, arg_b_i, arg_c_i;
	u32 arg_a_type, arg_b_type, arg_c_type, arg_abc_type;

	status = cmdq_check_before_append(handle);
	if (status < 0) {
		CMDQ_ERR(
			"cannot add logic command (s_op:%d arg_b:0x%08x arg_c:0x%08x)\n",
			s_op, (u32)(arg_b & 0xFFFFFFFF),
			(u32)(arg_c & 0xFFFFFFFF));
		return status;
	}

	do {
		u32 cpr_offset = CMDQ_CPR_STRAT_ID + CMDQ_THR_CPR_MAX *
			handle->thread;

		/* get actual arg_b_i & arg_b_type */
		status = cmdq_var_data_type(arg_b, &arg_b_i, &arg_b_type);
		if (status < 0)
			break;

		/* get actual arg_c_i & arg_c_type */
		status = cmdq_var_data_type(arg_c, &arg_c_i, &arg_c_type);
		if (status < 0)
			break;

		/* get arg_a register by using module storage manager */
		status = cmdq_create_variable_if_need(handle, arg_a);
		if (status < 0)
			break;

		/* get actual arg_a_i & arg_a_type */
		status = cmdq_var_data_type(*arg_a, &arg_a_i, &arg_a_type);
		if (status < 0)
			break;

		/* arg_a always be SW register */
		arg_abc_type = (1 << 2) | (arg_b_type << 1) | (arg_c_type);

		/* change cpr to thread cpr directly,
		 * if we already have exclusive thread.
		 */
		if (handle->thread != CMDQ_INVALID_THREAD) {
			if (cmdq_is_cpr(arg_a_i, arg_a_type))
				arg_a_i = cpr_offset + (arg_a_i -
					CMDQ_THR_SPR_MAX);
			if (cmdq_is_cpr(arg_b_i, arg_b_type))
				arg_b_i = cpr_offset + (arg_b_i -
					CMDQ_THR_SPR_MAX);
			if (cmdq_is_cpr(arg_c_i, arg_c_type))
				arg_c_i = cpr_offset + (arg_c_i -
					CMDQ_THR_SPR_MAX);
		}

		cmdq_append_command_pkt(handle->pkt, CMDQ_CODE_LOGIC,
			(arg_abc_type << 21) | (s_op << 16) | arg_a_i,
			(arg_b_i << 16) | (arg_c_i & 0xFFFF));

		if (handle->thread == CMDQ_INVALID_THREAD) {
			if (cmdq_is_cpr(arg_a_i, arg_a_type) ||
				cmdq_is_cpr(arg_b_i, arg_b_type) ||
				cmdq_is_cpr(arg_c_i, arg_c_type)) {
				CMDQ_MSG(
					"save logic: sop:%d arg_a:0x%08x arg_b:0x%08x arg_c:0x%08x arg_abc_type:%d\n",
					 s_op, arg_a_i, arg_b_i, arg_c_i,
					 arg_abc_type);
				cmdq_save_op_variable_position(handle,
					cmdq_task_get_inst_cnt(handle) - 1);
			}
		}
	} while (0);

	return status;
}

void cmdq_op_init_variable(CMDQ_VARIABLE *arg)
{
	*arg = CMDQ_TASK_CPR_INITIAL_VALUE;
}

void cmdq_op_init_global_cpr_variable(CMDQ_VARIABLE *arg,
	u32 cpr_offset)
{
	*arg = CMDQ_ARG_CPR_START + cpr_offset;
}

s32 cmdq_op_assign(struct cmdqRecStruct *handle,
	CMDQ_VARIABLE *arg_out, CMDQ_VARIABLE arg_in)
{
	CMDQ_VARIABLE arg_b, arg_c;

	if (CMDQ_BIT_VALUE == (arg_in >> CMDQ_DATA_BIT)) {
		arg_c = (arg_in & 0x0000FFFF);
		arg_b = ((arg_in>>16) & 0x0000FFFF);
	} else {
		CMDQ_ERR("Assign only use value, can not append new command");
		return -EFAULT;
	}

	return cmdq_append_logic_command(handle, arg_out, arg_b,
		CMDQ_LOGIC_ASSIGN, arg_c);
}

s32 cmdq_op_add(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c)
{
	return cmdq_append_logic_command(handle, arg_out, arg_b,
		CMDQ_LOGIC_ADD, arg_c);
}

s32 cmdq_op_subtract(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c)
{
	return cmdq_append_logic_command(handle, arg_out, arg_b,
		CMDQ_LOGIC_SUBTRACT, arg_c);
}

s32 cmdq_op_multiply(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c)
{
	return cmdq_append_logic_command(handle, arg_out, arg_b,
		CMDQ_LOGIC_MULTIPLY, arg_c);
}

s32 cmdq_op_xor(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c)
{
	return cmdq_append_logic_command(handle, arg_out, arg_b,
		CMDQ_LOGIC_XOR, arg_c);
}

s32 cmdq_op_not(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b)
{
	return cmdq_append_logic_command(handle, arg_out, arg_b,
		CMDQ_LOGIC_NOT, 0);
}

s32 cmdq_op_or(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c)
{
	return cmdq_append_logic_command(handle, arg_out, arg_b,
		CMDQ_LOGIC_OR, arg_c);
}

s32 cmdq_op_and(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c)
{
	return cmdq_append_logic_command(handle, arg_out, arg_b,
		CMDQ_LOGIC_AND, arg_c);
}

s32 cmdq_op_left_shift(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c)
{
	return cmdq_append_logic_command(handle, arg_out, arg_b,
		CMDQ_LOGIC_LEFT_SHIFT, arg_c);
}

s32 cmdq_op_right_shift(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c)
{
	return cmdq_append_logic_command(handle, arg_out, arg_b,
		CMDQ_LOGIC_RIGHT_SHIFT, arg_c);
}

s32 cmdq_op_backup_CPR(struct cmdqRecStruct *handle, CMDQ_VARIABLE cpr,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index)
{
	s32 status = 0;
	CMDQ_VARIABLE pa_cpr = CMDQ_TASK_TEMP_CPR_VAR;
	const dma_addr_t dramAddr = h_backup_slot + slot_index * sizeof(u32);

	cmdq_op_assign(handle, &pa_cpr, (u32)dramAddr);
	cmdq_append_command(handle, CMDQ_CODE_WRITE_S,
		(u32)pa_cpr, (u32)cpr, 1, 1);

	return status;
}

s32 cmdq_op_backup_TPR(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index)
{
	s32 status = 0;
	CMDQ_VARIABLE pa_cpr = CMDQ_TASK_TEMP_CPR_VAR;
	const CMDQ_VARIABLE arg_tpr = (CMDQ_BIT_VAR<<CMDQ_DATA_BIT) |
		CMDQ_TPR_ID;
	const dma_addr_t dramAddr = h_backup_slot + slot_index * sizeof(u32);

	cmdq_op_assign(handle, &pa_cpr, (u32)dramAddr);
	cmdq_append_command(handle, CMDQ_CODE_WRITE_S,
		(u32)pa_cpr, (u32)arg_tpr, 1, 1);

	return status;
}

enum CMDQ_CONDITION_ENUM cmdq_reverse_op_condition(
	enum CMDQ_CONDITION_ENUM arg_condition)
{
	enum CMDQ_CONDITION_ENUM instr_condition = CMDQ_CONDITION_ERROR;

	switch (arg_condition) {
	case CMDQ_EQUAL:
		instr_condition = CMDQ_NOT_EQUAL;
		break;
	case CMDQ_NOT_EQUAL:
		instr_condition = CMDQ_EQUAL;
		break;
	case CMDQ_GREATER_THAN_AND_EQUAL:
		instr_condition = CMDQ_LESS_THAN;
		break;
	case CMDQ_LESS_THAN_AND_EQUAL:
		instr_condition = CMDQ_GREATER_THAN;
		break;
	case CMDQ_GREATER_THAN:
		instr_condition = CMDQ_LESS_THAN_AND_EQUAL;
		break;
	case CMDQ_LESS_THAN:
		instr_condition = CMDQ_GREATER_THAN_AND_EQUAL;
		break;
	default:
		CMDQ_ERR(
			"Incorrect CMDQ condition statement (%d), can not append new command\n",
			arg_condition);
		break;
	}

	return instr_condition;
}

s32 cmdq_append_jump_c_command(struct cmdqRecStruct *handle,
	CMDQ_VARIABLE arg_b, enum CMDQ_CONDITION_ENUM arg_condition,
	CMDQ_VARIABLE arg_c)
{
	s32 status = 0;
	const s32 dummy_address = 8;
	enum CMDQ_CONDITION_ENUM instr_condition;
	u32 arg_a_i, arg_b_i, arg_c_i;
	u32 arg_a_type, arg_b_type, arg_c_type, arg_abc_type;
	CMDQ_VARIABLE arg_temp_cpr = CMDQ_TASK_TEMP_CPR_VAR;
	bool always_jump_abs;
	u32 jump_c_idx;

	if (!handle)
		return -EINVAL;

	always_jump_abs = handle->scenario != CMDQ_SCENARIO_TIMER_LOOP;

	if (likely(always_jump_abs)) {
		/* Insert write_s to write address to SPR1,
		 * since we may change relative to absolute jump later,
		 * and use this write_s instruction
		 * to set destination PA address.
		 */
		status = cmdq_op_assign(handle, &arg_temp_cpr, dummy_address);
		if (status < 0)
			return status;
	}

	status = cmdq_check_before_append(handle);
	if (status < 0) {
		CMDQ_ERR(
			"cannot add jump_c command (condition:%d arg_b:0x%08x arg_c:0x%08x)\n",
			arg_condition, (u32)(arg_b & 0xFFFFFFFF),
			(u32)(arg_c & 0xFFFFFFFF));
		return status;
	}

	do {
		/* reverse condition statement */
		instr_condition = cmdq_reverse_op_condition(arg_condition);
		if (instr_condition < 0) {
			status = -EFAULT;
			break;
		}

		if (likely(always_jump_abs)) {
			/* arg_a always be register SPR1 */
			status = cmdq_var_data_type(arg_temp_cpr, &arg_a_i,
				&arg_a_type);
			if (status < 0)
				break;
		} else {
			arg_a_type = 0;
			arg_a_i = dummy_address;
		}

		/* get actual arg_b_i & arg_b_type */
		status = cmdq_var_data_type(arg_b, &arg_b_i, &arg_b_type);
		if (status < 0)
			break;

		/* get actual arg_c_i & arg_c_type */
		status = cmdq_var_data_type(arg_c, &arg_c_i, &arg_c_type);
		if (status < 0)
			break;

		arg_abc_type = (arg_a_type << 2) | (arg_b_type << 1) |
			(arg_c_type);
		if (cmdq_is_cpr(arg_b_i, arg_b_type) ||
			cmdq_is_cpr(arg_c_i, arg_c_type)) {
			/* save local variable position */
			CMDQ_MSG(
				"save jump_c: condition:%d arg_b:0x%08x arg_c:0x%08x arg_abc_type:%d\n",
				arg_condition, arg_b_i, arg_c_i, arg_abc_type);
		}
		if ((arg_c_i & 0xFFFF0000) != 0)
			CMDQ_ERR("jump_c arg_c value is over 16 bit:0x%08x\n",
			arg_c_i);

		cmdq_append_command_pkt(handle->pkt, CMDQ_CODE_JUMP_C_RELATIVE,
			(arg_abc_type << 21) | (instr_condition << 16) |
			(arg_a_i),
			(arg_b_i << 16) | (arg_c_i));

		/* save position to replace write value later
		 * and cpu use in jump_c
		 */
		if (handle->pkt->cmd_buf_size % CMDQ_CMD_BUFFER_SIZE == 0)
			jump_c_idx = cmdq_task_get_inst_cnt(handle);
		else
			jump_c_idx =
				cmdq_task_get_inst_cnt(handle) - 1;
		cmdq_save_op_variable_position(handle, jump_c_idx);
	} while (0);

	return status;
}

s32 cmdq_op_rewrite_jump_c(struct cmdqRecStruct *handle,
	u32 logic_pos, u32 exit_while_pos)
{
	u32 op, op_arg_type, op_jump;
	u32 *va_jump, *va_logic;
	u32 jump_pos;

	if (!handle)
		return -EFAULT;

	if (likely(handle->scenario != CMDQ_SCENARIO_TIMER_LOOP)) {
		va_logic = (u32 *)cmdq_pkt_get_va_by_offset(handle->pkt,
			logic_pos);
		if (!va_logic) {
			CMDQ_LOG("Cannot find va, handle:%p pkt:%p pos:%u\n",
				handle, handle->pkt, logic_pos);
			return -EINVAL;
		}
		if (((logic_pos + CMDQ_INST_SIZE * 2) %
			CMDQ_CMD_BUFFER_SIZE) == 0)
			jump_pos = logic_pos + CMDQ_INST_SIZE * 2;
		else
			jump_pos = logic_pos + CMDQ_INST_SIZE;

		va_jump = (u32 *)cmdq_pkt_get_va_by_offset(handle->pkt,
			jump_pos);
		if (!va_jump) {
			CMDQ_LOG("Cannot find va, handle:%p pkt:%p pos:%u\n",
				handle, handle->pkt, jump_pos);
			return -EINVAL;
		}

		/* reserve condition statement */
		op = (va_logic[1] & 0xFF000000) >> 24;
		op_jump = (va_jump[1] & 0xFF000000) >> 24;
		if (op != CMDQ_CODE_LOGIC || op_jump !=
			CMDQ_CODE_JUMP_C_RELATIVE) {
			CMDQ_ERR("rewrite wrong op:0x%08x jump:0x%08x\n",
				op, op_jump);
			return -EFAULT;
		}

		/* rewrite actual jump value */
		op_arg_type = (va_logic[0] & 0xFFFF0000) >> 16;
		va_logic[0] = (op_arg_type << 16) | CMDQ_REG_SHIFT_ADDR(
			exit_while_pos  - logic_pos - CMDQ_INST_SIZE);

		CMDQ_VERBOSE(
			"%s pos logic:%u exit:%u logic:0x%p 0x%016llx jump:0x%p 0x%016llx\n",
			__func__, logic_pos, exit_while_pos,
			va_logic, *(u64 *)va_logic,
			va_jump, *(u64 *)va_jump);
	} else {
		va_jump = (u32 *)cmdq_pkt_get_va_by_offset(handle->pkt,
			logic_pos);
		if (!va_jump) {
			CMDQ_LOG("Cannot find va, handle:%p pkt:%p pos:%u\n",
				handle, handle->pkt, logic_pos);
			return -EINVAL;
		}
		op = (va_jump[1] & 0xFF000000) >> 24;
		if (op != CMDQ_CODE_JUMP_C_RELATIVE) {
			CMDQ_ERR("fail to rewrite jump c handle:0x%p\n",
				handle);
			return -EFAULT;
		}

		/* rewrite actual jump value */
		op_arg_type = (va_jump[1] & 0xFFFF0000) >> 16;
		va_jump[1] = (op_arg_type << 16) | CMDQ_REG_SHIFT_ADDR(
			((s32)exit_while_pos  - (s32)logic_pos) & 0xFFFF);

		CMDQ_VERBOSE(
			"%s pos logic:%u exit:%u jump:0x%p 0x%016llx\n",
			__func__, logic_pos, exit_while_pos,
			va_jump, *(u64 *)va_jump);
	}

	return 0;
}

static void cmdq_op_check_logic_pos(u32 *logic_pos)
{
	if (((*logic_pos + CMDQ_INST_SIZE) % CMDQ_CMD_BUFFER_SIZE == 0) ||
		*logic_pos % CMDQ_CMD_BUFFER_SIZE == 0) {
		*logic_pos += CMDQ_INST_SIZE;
	}
}

s32 cmdq_op_if(struct cmdqRecStruct *handle, CMDQ_VARIABLE arg_b,
	enum CMDQ_CONDITION_ENUM arg_condition, CMDQ_VARIABLE arg_c)
{
	s32 status = 0;
	u32 logic_pos;

	if (!handle)
		return -EFAULT;

	do {
		u32 old_logic_pos;

		logic_pos  = handle->pkt->cmd_buf_size;
		old_logic_pos = logic_pos;

		/* append conditional jump instruction */
		status = cmdq_append_jump_c_command(handle, arg_b,
			arg_condition, arg_c);
		if (status < 0)
			break;

		cmdq_op_check_logic_pos(&logic_pos);

		/* handle if-else stack */
		status = cmdq_op_condition_push(&handle->if_stack_node,
			logic_pos, CMDQ_STACK_TYPE_IF);
	} while (0);

	return status;
}

s32 cmdq_op_end_if(struct cmdqRecStruct *handle)
{
	s32 status = 0, ifCount = 1;
	u32 rewritten_position = 0;
	u32 exit_if_pos;
	enum CMDQ_STACK_TYPE_ENUM rewritten_stack_type;

	if (!handle)
		return -EFAULT;

	do {
		/* check if-else stack */
		status = cmdq_op_condition_query(handle->if_stack_node,
			&rewritten_position, &rewritten_stack_type);
		if (status < 0)
			break;

		if (rewritten_stack_type == CMDQ_STACK_TYPE_IF) {
			if (ifCount <= 0)
				break;
		} else if (rewritten_stack_type != CMDQ_STACK_TYPE_ELSE)
			break;
		ifCount--;

		/* handle if-else stack */
		status = cmdq_op_condition_pop(&handle->if_stack_node,
			&rewritten_position, &rewritten_stack_type);
		if (status < 0) {
			CMDQ_ERR("failed to pop cmdq_stack_node\n");
			break;
		}

		if (handle->pkt->cmd_buf_size % CMDQ_CMD_BUFFER_SIZE == 0)
			exit_if_pos = handle->pkt->cmd_buf_size +
				CMDQ_INST_SIZE;
		else
			exit_if_pos = handle->pkt->cmd_buf_size;

		cmdq_op_rewrite_jump_c(handle, rewritten_position,
			exit_if_pos);
	} while (1);

	return status;
}

s32 cmdq_op_else(struct cmdqRecStruct *handle)
{
	s32 status = 0;
	u32 logic_pos, if_logic_pos, else_next_pos;
	enum CMDQ_STACK_TYPE_ENUM rewritten_stack_type = CMDQ_STACK_NULL;

	if (!handle)
		return -EFAULT;

	do {
		logic_pos  = handle->pkt->cmd_buf_size;

		/* check if-else stack */
		status = cmdq_op_condition_query(handle->if_stack_node,
			&if_logic_pos, &rewritten_stack_type);
		if (status < 0) {
			CMDQ_ERR("failed to query cmdq_stack_node\n");
			break;
		}

		if (rewritten_stack_type != CMDQ_STACK_TYPE_IF) {
			CMDQ_ERR(
				"Incorrect command, please review your if-else instructions.");
			status = -EFAULT;
			break;
		}

		/* append conditional jump instruction */
		status = cmdq_append_jump_c_command(handle, 1, CMDQ_NOT_EQUAL,
			1);
		if (status < 0)
			break;

		/* handle if-else stack */
		status = cmdq_op_condition_pop(&handle->if_stack_node,
			&if_logic_pos, &rewritten_stack_type);
		if (status < 0) {
			CMDQ_ERR("failed to pop cmdq_stack_node\n");
			break;
		}

		else_next_pos = handle->pkt->cmd_buf_size;
		if (else_next_pos % CMDQ_CMD_BUFFER_SIZE == 0)
			else_next_pos += CMDQ_INST_SIZE;

		cmdq_op_rewrite_jump_c(handle, if_logic_pos, else_next_pos);
		cmdq_op_check_logic_pos(&logic_pos);

		status = cmdq_op_condition_push(&handle->if_stack_node,
			logic_pos, CMDQ_STACK_TYPE_ELSE);
	} while (0);

	return status;
}

s32 cmdq_op_else_if(struct cmdqRecStruct *handle, CMDQ_VARIABLE arg_b,
	enum CMDQ_CONDITION_ENUM arg_condition, CMDQ_VARIABLE arg_c)
{
	s32 status = 0;

	if (!handle)
		return -EFAULT;

	do {
		/* handle else statement */
		status = cmdq_op_else(handle);
		if (status < 0)
			break;

		/* handle if statement */
		status = cmdq_op_if(handle, arg_b, arg_condition, arg_c);
	} while (0);

	return status;
}

s32 cmdq_op_while(struct cmdqRecStruct *handle, CMDQ_VARIABLE arg_b,
	enum CMDQ_CONDITION_ENUM arg_condition, CMDQ_VARIABLE arg_c)
{
	s32 status = 0;
	u32 logic_pos;

	if (!handle)
		return -EFAULT;

	do {
		u32 old_logic_pos;

		/* keep index of logic cmd */
		logic_pos = handle->pkt->cmd_buf_size;
		old_logic_pos = logic_pos;

		/* append conditional jump instruction */
		status = cmdq_append_jump_c_command(handle, arg_b,
			arg_condition, arg_c);
		if (status < 0)
			break;

		cmdq_op_check_logic_pos(&logic_pos);

		/* handle while stack */
		status = cmdq_op_condition_push(&handle->while_stack_node,
			logic_pos, CMDQ_STACK_TYPE_WHILE);
	} while (0);

	return status;
}

s32 cmdq_op_continue(struct cmdqRecStruct *handle)
{
	const u32 op_while_bit = 1 << CMDQ_STACK_TYPE_WHILE;
	const u32 op_do_while_bit = 1 << CMDQ_STACK_TYPE_DO_WHILE;
	s32 status = 0;
	u32 current_position;
	s32 rewritten_position;
	const struct cmdq_stack_node *op_node = NULL;

	if (!handle)
		return -EFAULT;

	do {
		current_position = handle->pkt->cmd_buf_size;

		/* query while/do while position from the stack */
		rewritten_position = cmdq_op_condition_find_op_type(
			handle->while_stack_node, current_position,
			op_while_bit | op_do_while_bit, &op_node);

		if (!op_node) {
			status = -EFAULT;
			break;
		}

		if (op_node->stack_type == CMDQ_STACK_TYPE_WHILE) {
			/* use jump command to start of while statement,
			 * since jump_c cannot process negative number
			 */

			/* minus CMDQ_INST_SIZE since rewritten_position is
			 * negative
			 */
			if ((current_position + CMDQ_INST_SIZE) %
				CMDQ_CMD_BUFFER_SIZE == 0 ||
				current_position % CMDQ_CMD_BUFFER_SIZE == 0)
				rewritten_position -= CMDQ_INST_SIZE;

			status = cmdq_append_command(handle, CMDQ_CODE_JUMP, 0,
				rewritten_position, 0, 0);
		} else if (op_node->stack_type == CMDQ_STACK_TYPE_DO_WHILE) {
			/* append conditional jump instruction
			 * to jump end op while
			 */
			status = cmdq_append_jump_c_command(handle, 1,
				CMDQ_NOT_EQUAL, 1);
			if (status < 0)
				break;
			status = cmdq_op_condition_push(
				&handle->while_stack_node, current_position,
				CMDQ_STACK_TYPE_CONTINUE);
		}
	} while (0);

	return status;
}

s32 cmdq_op_break(struct cmdqRecStruct *handle)
{
	const u32 op_while_bit = 1 << CMDQ_STACK_TYPE_WHILE;
	const u32 op_do_while_bit = 1 << CMDQ_STACK_TYPE_DO_WHILE;
	const struct cmdq_stack_node *op_node = NULL;
	s32 status = 0;
	u32 logic_pos;
	s32 while_position;

	if (!handle)
		return -EFAULT;

	do {
		logic_pos = handle->pkt->cmd_buf_size;

		/* query while position from the stack */
		while_position = cmdq_op_condition_find_op_type(
			handle->while_stack_node, logic_pos,
			op_while_bit | op_do_while_bit, &op_node);
		if (while_position >= 0) {
			CMDQ_ERR(
				"Incorrect break command, please review your while statement.");
			status = -EFAULT;
			break;
		}

		/* append conditional jump instruction */
		status = cmdq_append_jump_c_command(handle, 1, CMDQ_NOT_EQUAL,
			1);
		if (status < 0)
			break;

		cmdq_op_check_logic_pos(&logic_pos);

		/* handle while stack */
		status = cmdq_op_condition_push(&handle->while_stack_node,
			logic_pos, CMDQ_STACK_TYPE_BREAK);

	} while (0);

	return status;
}

s32 cmdq_op_end_while(struct cmdqRecStruct *handle)
{
	s32 status = 0, whileCount = 1;
	u32 logic_pos, exit_while_pos;
	enum CMDQ_STACK_TYPE_ENUM rewritten_stack_type = CMDQ_STACK_NULL;

	if (!handle)
		return -EFAULT;

	/* append command to loop start position */
	status = cmdq_op_continue(handle);
	if (status < 0) {
		CMDQ_ERR(
			"Cannot append end while, please review your while statement.");
		return status;
	}

	exit_while_pos =
		handle->pkt->cmd_buf_size % CMDQ_CMD_BUFFER_SIZE == 0 ?
		handle->pkt->cmd_buf_size + CMDQ_INST_SIZE :
		handle->pkt->cmd_buf_size;

	do {
		/* check while stack */
		status = cmdq_op_condition_query(handle->while_stack_node,
			&logic_pos, &rewritten_stack_type);
		if (status < 0)
			break;

		if (rewritten_stack_type == CMDQ_STACK_TYPE_DO_WHILE) {
			CMDQ_ERR("Mix with while and do while in while loop\n");
			status = -EFAULT;
			break;
		} else if (rewritten_stack_type == CMDQ_STACK_TYPE_WHILE) {
			if (whileCount <= 0)
				break;
			whileCount--;
		} else if (rewritten_stack_type != CMDQ_STACK_TYPE_BREAK)
			break;

		/* handle while stack */
		status = cmdq_op_condition_pop(&handle->while_stack_node,
			&logic_pos, &rewritten_stack_type);
		if (status < 0) {
			CMDQ_ERR("failed to pop cmdq_stack_node\n");
			break;
		}

		cmdq_op_rewrite_jump_c(handle, logic_pos, exit_while_pos);
	} while (1);

	return status;
}

s32 cmdq_op_do_while(struct cmdqRecStruct *handle)
{
	s32 status = 0;
	u32 current_position;

	if (!handle)
		return -EFAULT;

	current_position = handle->pkt->cmd_buf_size;
	/* handle while stack */
	status = cmdq_op_condition_push(&handle->while_stack_node,
		current_position, CMDQ_STACK_TYPE_DO_WHILE);
	return status;
}

s32 cmdq_op_end_do_while(struct cmdqRecStruct *handle, CMDQ_VARIABLE arg_b,
	enum CMDQ_CONDITION_ENUM arg_condition, CMDQ_VARIABLE arg_c)
{
	s32 status = 0;
	u32 stack_op_position, condition_position;
	enum CMDQ_STACK_TYPE_ENUM stack_op_type = CMDQ_STACK_NULL;

	if (!handle)
		return -EFAULT;

	/* mark position of end do while for continue */
	condition_position = handle->pkt->cmd_buf_size;

	/* Append conditional jump instruction and rewrite later.
	 * Reverse op since cmdq_append_jump_c_command
	 * always do reverse and jump.
	 */
	status = cmdq_append_jump_c_command(handle, arg_b,
		cmdq_reverse_op_condition(arg_condition), arg_c);

	do {
		u32 destination_position = handle->pkt->cmd_buf_size;

		/* check while stack */
		status = cmdq_op_condition_query(handle->while_stack_node,
			&stack_op_position, &stack_op_type);
		if (status < 0)
			break;

		if (stack_op_type == CMDQ_STACK_TYPE_WHILE) {
			CMDQ_ERR(
				"Mix with while and do while in do-while loop\n");
			status = -EFAULT;
			break;
		} else if (stack_op_type == CMDQ_STACK_TYPE_DO_WHILE) {
			/* close do-while loop by jump to begin of do-while */
			status = cmdq_op_condition_pop(
				&handle->while_stack_node, &stack_op_position,
				&stack_op_type);
			cmdq_op_rewrite_jump_c(handle,
				condition_position, stack_op_position);
			break;
		} else if (stack_op_type == CMDQ_STACK_TYPE_CONTINUE) {
			/* jump to while condition to do check again */
			destination_position = condition_position;
		} else if (stack_op_type == CMDQ_STACK_TYPE_BREAK) {
			/* jump after check to skip current loop */
			destination_position = handle->pkt->cmd_buf_size;
		} else {
			/* unknown error type */
			CMDQ_ERR("Unknown stack type in do-while loop:%d\n",
				stack_op_type);
			break;
		}

		/* handle continue/break case in stack */
		status = cmdq_op_condition_pop(&handle->while_stack_node,
			&stack_op_position, &stack_op_type);
		if (status < 0) {
			CMDQ_ERR(
				"failed to pop cmdq_stack_node in do-while loop\n");
			break;
		}
		cmdq_op_rewrite_jump_c(handle, stack_op_position,
			destination_position);
	} while (1);

	return status;
}

s32 cmdq_op_read_reg(struct cmdqRecStruct *handle, u32 addr,
	CMDQ_VARIABLE *arg_out, u32 mask)
{
	s32 status = 0;
	CMDQ_VARIABLE mask_var = CMDQ_TASK_TEMP_CPR_VAR;
	u32 arg_a_i, arg_a_type;

	/* get arg_a register by using module storage manager */
	do {
		status = cmdq_create_variable_if_need(handle, arg_out);
		CMDQ_CHECK_AND_BREAK_STATUS(status);

		/* get actual arg_a_i & arg_a_type */
		status = cmdq_var_data_type(*arg_out, &arg_a_i, &arg_a_type);
		CMDQ_CHECK_AND_BREAK_STATUS(status);

		status = cmdq_append_command(handle, CMDQ_CODE_READ_S, arg_a_i,
			addr, arg_a_type, 0);
		CMDQ_CHECK_AND_BREAK_STATUS(status);

		if (mask != 0xFFFFFFFF) {
			if ((mask >> 16) > 0) {
				status = cmdq_op_assign(handle, &mask_var,
					mask);
				CMDQ_CHECK_AND_BREAK_STATUS(status);
				status = cmdq_op_and(handle, arg_out, *arg_out,
					mask_var);
			} else {
				status = cmdq_op_and(handle, arg_out, *arg_out,
					mask);
			}
		}
	} while (0);

	return status;
}

s32 cmdq_op_read_mem(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index,
	CMDQ_VARIABLE *arg_out)
{
	return 0;
}

s32 cmdqRecCreate(enum CMDQ_SCENARIO_ENUM scenario,
	struct cmdqRecStruct **pHandle)
{
	return cmdq_task_create(scenario, pHandle);
}

s32 cmdqRecSetEngine(struct cmdqRecStruct *handle, u64 engineFlag)
{
	return cmdq_task_set_engine(handle, engineFlag);
}

s32 cmdqRecReset(struct cmdqRecStruct *handle)
{
	return cmdq_task_reset(handle);
}

s32 cmdqRecSetSecure(struct cmdqRecStruct *handle, const bool is_secure)
{
	return cmdq_task_set_secure(handle, is_secure);
}

s32 cmdqRecIsSecure(struct cmdqRecStruct *handle)
{
	return cmdq_task_is_secure(handle);
}

/* tablet use */
#ifdef CONFIG_MTK_IN_HOUSE_TEE_SUPPORT
s32 cmdqRecSetSecureMode(struct cmdqRecStruct *handle, enum CMDQ_DISP_MODE mode)
{
	return cmdq_task_set_secure_mode(handle, mode);
}
#endif

s32 cmdqRecSecureEnableDAPC(struct cmdqRecStruct *handle, const u64 engineFlag)
{
	return cmdq_task_secure_enable_dapc(handle, engineFlag);
}

s32 cmdqRecSecureEnablePortSecurity(struct cmdqRecStruct *handle,
	const u64 engineFlag)
{
	return cmdq_task_secure_enable_port_security(handle, engineFlag);
}

s32 cmdqRecMark(struct cmdqRecStruct *handle)
{
	s32 status;

	/* Do not check prefetch-ability here.
	 * because cmdqRecMark may have other purposes.
	 *
	 * bit 53: non-suspendable. set to 1 because we don't want
	 * CPU suspend this thread during pre-fetching.
	 * If CPU change PC, then there will be a mess,
	 * because prefetch buffer is not properly cleared.
	 * bit 48: do not increase CMD_COUNTER
	 * (because this is not the end of the task)
	 * bit 20: prefetch_marker
	 * bit 17: prefetch_marker_en
	 * bit 16: prefetch_en
	 * bit 0:  irq_en (set to 0 since we don't want EOC interrupt)
	 */
	status = cmdq_append_command(handle,
		CMDQ_CODE_EOC, (0x1 << (53 - 32)) | (0x1 << (48 - 32)),
		0x00130000, 0, 0);

	/* if we're in a prefetch region,
	 * this ends the region so set count to 0.
	 * otherwise we start the region by setting count to 1.
	 */
	handle->prefetchCount = 1;

	if (status != 0)
		return -EFAULT;

	return 0;
}

s32 cmdqRecWrite(struct cmdqRecStruct *handle, u32 addr, u32 value, u32 mask)
{
	return cmdq_op_write_reg(handle, addr, (CMDQ_VARIABLE)value, mask);
}

s32 cmdqRecWriteSecure(struct cmdqRecStruct *handle, u32 addr,
	enum CMDQ_SEC_ADDR_METADATA_TYPE type,
	u64 baseHandle, u32 offset, u32 size, u32 port)
{
	return cmdq_op_write_reg_secure(handle, addr, type, baseHandle, offset,
		size, port);
}

s32 cmdqRecPoll(struct cmdqRecStruct *handle, u32 addr, u32 value, u32 mask)
{
	return cmdq_op_poll(handle, addr, value, mask);
}

s32 cmdqRecWait(struct cmdqRecStruct *handle, enum cmdq_event event)
{
	return cmdq_op_wait(handle, event);
}

s32 cmdqRecWaitNoClear(struct cmdqRecStruct *handle,
	enum cmdq_event event)
{
	return cmdq_op_wait_no_clear(handle, event);
}

s32 cmdqRecClearEventToken(struct cmdqRecStruct *handle,
	enum cmdq_event event)
{
	return cmdq_op_clear_event(handle, event);
}

s32 cmdqRecSetEventToken(struct cmdqRecStruct *handle,
	enum cmdq_event event)
{
	return cmdq_op_set_event(handle, event);
}

s32 cmdqRecReadToDataRegister(struct cmdqRecStruct *handle, u32 hw_addr,
	enum cmdq_gpr_reg dst_data_reg)
{
	return cmdq_op_read_to_data_register(handle, hw_addr, dst_data_reg);
}

s32 cmdqRecWriteFromDataRegister(struct cmdqRecStruct *handle,
	enum cmdq_gpr_reg src_data_reg, u32 hw_addr)
{
	return cmdq_op_write_from_data_register(handle, src_data_reg, hw_addr);
}

s32 cmdqBackupAllocateSlot(cmdqBackupSlotHandle *p_h_backup_slot, u32 slotCount)
{
	return cmdq_alloc_mem(p_h_backup_slot, slotCount);
}

s32 cmdqBackupReadSlot(cmdqBackupSlotHandle h_backup_slot, u32 slot_index,
	u32 *value)
{
	return cmdq_cpu_read_mem(h_backup_slot, slot_index, value);
}

s32 cmdqBackupWriteSlot(cmdqBackupSlotHandle h_backup_slot,
	u32 slot_index, u32 value)
{
	return cmdq_cpu_write_mem(h_backup_slot, slot_index, value);
}

s32 cmdqBackupFreeSlot(cmdqBackupSlotHandle h_backup_slot)
{
	return cmdq_free_mem(h_backup_slot);
}

s32 cmdqRecBackupRegisterToSlot(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index, u32 regAddr)
{
	return cmdq_op_read_reg_to_mem(handle, h_backup_slot, slot_index,
		regAddr);
}

s32 cmdqRecBackupWriteRegisterFromSlot(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index, u32 addr)
{
	return cmdq_op_read_mem_to_reg(handle, h_backup_slot, slot_index, addr);
}

s32 cmdqRecBackupUpdateSlot(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index, u32 value)
{
	return cmdq_op_write_mem(handle, h_backup_slot, slot_index, value);
}

s32 cmdqRecEnablePrefetch(struct cmdqRecStruct *handle)
{
#ifdef _CMDQ_DISABLE_MARKER_
	/* disable pre-fetch marker feature but use auto prefetch mechanism */
	CMDQ_MSG("not allow enable prefetch, scenario:%d\n", handle->scenario);
	return true;
#else
	if (!handle)
		return -EFAULT;

	if (cmdq_get_func()->shouldEnablePrefetch(handle->scenario)) {
		/* enable prefetch */
		CMDQ_VERBOSE("REC: enable prefetch\n");
		cmdqRecMark(handle);
		return true;
	}
	CMDQ_ERR("not allow enable prefetch, scenario:%d\n", handle->scenario);
	return -EFAULT;
#endif
}

s32 cmdqRecDisablePrefetch(struct cmdqRecStruct *handle)
{
	u32 arg_b = 0;
	u32 arg_a = 0;
	s32 status = 0;

	if (!handle)
		return -EFAULT;

	if (!handle->finalized) {
		if (handle->prefetchCount > 0) {
			/* with prefetch threads we should end with
			 * bit 48: no_inc_exec_cmds_cnt = 1
			 * bit 20: prefetch_mark = 1
			 * bit 17: prefetch_mark_en = 0
			 * bit 16: prefetch_en = 0
			 */
			arg_b = 0x00100000;
			/* not increse execute counter */
			arg_a = (0x1 << 16);
			/* since we're finalized, no more prefetch */
			handle->prefetchCount = 0;
			status = cmdq_append_command(handle, CMDQ_CODE_EOC,
				arg_a, arg_b, 0, 0);
		}

		if (status != 0)
			return status;
	}

	CMDQ_MSG("%s status:%d\n", __func__, status);
	return status;
}

s32 cmdqRecFlush(struct cmdqRecStruct *handle)
{
	return cmdq_task_flush(handle);
}

s32 cmdqRecFlushAndReadRegister(struct cmdqRecStruct *handle, u32 regCount,
	u32 *addrArray, u32 *valueArray)
{
	return cmdq_task_flush_and_read_register(handle, regCount, addrArray,
		valueArray);
}

s32 cmdqRecFlushAsync(struct cmdqRecStruct *handle)
{
	return cmdq_task_flush_async(handle);
}

s32 cmdqRecFlushAsyncCallback(struct cmdqRecStruct *handle,
	CmdqAsyncFlushCB callback, u64 user_data)
{
	return cmdq_task_flush_async_callback(handle, callback, user_data);
}

s32 cmdqRecStartLoop(struct cmdqRecStruct *handle)
{
	return cmdq_task_start_loop(handle);
}

s32 cmdqRecStartLoopWithCallback(struct cmdqRecStruct *handle,
	CmdqInterruptCB loopCB, unsigned long loopData)
{
	return cmdq_task_start_loop_callback(handle, loopCB, loopData);
}

s32 cmdqRecStopLoop(struct cmdqRecStruct *handle)
{
	return cmdq_task_stop_loop(handle);
}

s32 cmdqRecGetInstructionCount(struct cmdqRecStruct *handle)
{
	return cmdq_task_get_inst_cnt(handle);
}

s32 cmdqRecProfileMarker(struct cmdqRecStruct *handle, const char *tag)
{
	return cmdq_op_profile_marker(handle, tag);
}

s32 cmdqRecDumpCommand(struct cmdqRecStruct *handle)
{
	return cmdq_pkt_dump_command(handle);
}

void cmdqRecDestroy(struct cmdqRecStruct *handle)
{
	cmdq_task_destroy(handle);
}

s32 cmdqRecSetNOP(struct cmdqRecStruct *handle, u32 index)
{
	return cmdq_op_set_nop(handle, index);
}

s32 cmdqRecQueryOffset(struct cmdqRecStruct *handle, u32 startIndex,
	const enum cmdq_code opCode, enum cmdq_event event)
{
	return cmdq_task_query_offset(handle, startIndex, opCode, event);
}

s32 cmdqRecAcquireResource(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent)
{
	return cmdq_resource_acquire(handle, resourceEvent);
}

s32 cmdqRecWriteForResource(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent, u32 addr, u32 value, u32 mask)
{
	return cmdq_resource_acquire_and_write(handle, resourceEvent, addr,
		value, mask);
}

s32 cmdqRecReleaseResource(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent)
{
	return cmdq_resource_release(handle, resourceEvent);
}

s32 cmdqRecWriteAndReleaseResource(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent, u32 addr, u32 value, u32 mask)
{
	return cmdq_resource_release_and_write(handle, resourceEvent, addr,
		value, mask);
}

