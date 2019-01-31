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

#ifndef __CMDQ_RECORD_H__
#define __CMDQ_RECORD_H__

#include <linux/types.h>
#include <linux/soc/mediatek/mtk-cmdq.h>
#include <linux/mailbox/mtk-cmdq-mailbox.h>

#include "cmdq_def.h"
#include "cmdq_helper_ext.h"

enum CMDQ_STACK_TYPE_ENUM {
	CMDQ_STACK_NULL = -1,
	CMDQ_STACK_TYPE_IF = 0,
	CMDQ_STACK_TYPE_ELSE = 1,
	CMDQ_STACK_TYPE_WHILE = 2,
	CMDQ_STACK_TYPE_BREAK = 3,
	CMDQ_STACK_TYPE_CONTINUE = 4,
	CMDQ_STACK_TYPE_DO_WHILE = 5,
};

#define CMDQ_DATA_BIT			(62)
#define CMDQ_BIT_VALUE			(0LL)
#define CMDQ_BIT_VAR			(1LL)
#define CMDQ_TASK_CPR_INITIAL_VALUE	(0)
#define CMDQ_REC_DEFAULT_PRIORITY	(100)
#define CMDQ_REC_MAX_PRIORITY		(0x7FFFFFFF)

struct cmdq_stack_node {
	u32 position;
	enum CMDQ_STACK_TYPE_ENUM stack_type;
	struct cmdq_stack_node *next;
};

struct cmdq_user_req {
#if 0
	u32 *buffer;
	u32 buffer_size;
#endif
	u32 reg_count;
	u32 user_token;
};

#define cmdqBackupSlotHandle dma_addr_t

/* Create command queue recorder handle
 * Parameter:
 *     pHandle: pointer to retrieve the handle
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_task_create(enum CMDQ_SCENARIO_ENUM scenario,
	struct cmdqRecStruct **pHandle);
s32 cmdqRecCreate(enum CMDQ_SCENARIO_ENUM scenario,
	struct cmdqRecStruct **pHandle);
s32 cmdq_task_duplicate(struct cmdqRecStruct *handle,
	struct cmdqRecStruct **handle_out);

/* Set engine flag for command queue picking HW thread
 * Parameter:
 *     pHandle: pointer to retrieve the handle
 *     engineFlag: Flag use to identify which HW module can be accessed
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_task_set_engine(struct cmdqRecStruct *handle,
	u64 engineFlag);
s32 cmdqRecSetEngine(struct cmdqRecStruct *handle,
	u64 engineFlag);

/* Reset command queue recorder commands
 * Parameter:
 *    handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_task_reset(struct cmdqRecStruct *handle);
s32 cmdqRecReset(struct cmdqRecStruct *handle);

void cmdq_task_set_timeout(struct cmdqRecStruct *handle, u32 timeout);

/* Configure as secure task
 * Parameter:
 *     handle: the command queue recorder handle
 *     is_secure: true, execute the command in secure world
 * Return:
 *     0 for success; else the error code is returned
 *
 * Note:
 *     a. Secure CMDQ support when t-base enabled only
 *     b. By default struct cmdqRecStruct records a normal command,
 *	 please call cmdq_task_set_secure to set command as SECURE
 *        after cmdq_task_reset
 */
s32 cmdq_task_set_secure(struct cmdqRecStruct *handle, const bool is_secure);
s32 cmdqRecSetSecure(struct cmdqRecStruct *handle, const bool is_secure);

/* query handle is secure task or not
 * Parameter:
 *	  handle: the command queue recorder handle
 * Return:
 *	   0 for false (not secure) and 1 for true (is secure)
 */
s32 cmdq_task_is_secure(struct cmdqRecStruct *handle);
s32 cmdqRecIsSecure(struct cmdqRecStruct *handle);

/* Add DPAC protection flag
 * Parameter:
 * Note:
 *    a. Secure CMDQ support when t-base enabled only
 *     b. after reset handle, user have to specify protection flag again
 */
s32 cmdq_task_secure_enable_dapc(struct cmdqRecStruct *handle,
	const u64 engineFlag);
s32 cmdqRecSecureEnableDAPC(struct cmdqRecStruct *handle,
	const u64 engineFlag);

/* Add flag for M4U security ports
 * Parameter:
 * Note:
 *	   a. Secure CMDQ support when t-base enabled only
 *	   b. after reset handle, user have to specify protection flag again
 */
s32 cmdq_task_secure_enable_port_security(struct cmdqRecStruct *handle,
	const u64 engineFlag);
s32 cmdqRecSecureEnablePortSecurity(struct cmdqRecStruct *handle,
	const u64 engineFlag);

/* Append mark command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdqRecMark(struct cmdqRecStruct *handle);

/* Append mark command to enable prefetch
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdqRecEnablePrefetch(struct cmdqRecStruct *handle);

/* Append mark command to disable prefetch
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdqRecDisablePrefetch(struct cmdqRecStruct *handle);

/* Append write command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 *     addr: the specified target register physical address
 *     argument / value: the specified target register value
 *     mask: the specified target register mask
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_write_reg(struct cmdqRecStruct *handle, u32 addr,
	CMDQ_VARIABLE argument, u32 mask);
s32 cmdqRecWrite(struct cmdqRecStruct *handle, u32 addr, u32 value, u32 mask);

/* Append write command to the update secure buffer address in secure path
 * Parameter:
 *	handle: the command queue recorder handle
 *	addr: the specified register physical address about module src/dst
 *      buffer address
 *	type: base handle type
 *	base handle: secure handle of a secure mememory
 *	offset: offset related to base handle
 *	    (secure buffer = addr(base_handle) + offset)
 *	size: secure buffer size
 *	mask: 0xFFFF_FFFF
 * Return:
 *	0 for success; else the error code is returned
 * Note:
 *	support only when secure OS enabled
 */
s32 cmdq_op_write_reg_secure(struct cmdqRecStruct *handle, u32 addr,
	enum CMDQ_SEC_ADDR_METADATA_TYPE type, u64 baseHandle,
	u32 offset, u32 size, u32 port);
s32 cmdqRecWriteSecure(struct cmdqRecStruct *handle,
	u32 addr, enum CMDQ_SEC_ADDR_METADATA_TYPE type,
	u64 baseHandle, u32 offset, u32 size, u32 port);

/* Append poll command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 *     addr: the specified register physical address
 *     value: the required register value
 *     mask: the required register mask
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_poll(struct cmdqRecStruct *handle, u32 addr, u32 value, u32 mask);
s32 cmdqRecPoll(struct cmdqRecStruct *handle, u32 addr, u32 value, u32 mask);

/* Append wait command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type to "wait and CLEAR"
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_wait(struct cmdqRecStruct *handle, enum cmdq_event event);
s32 cmdqRecWait(struct cmdqRecStruct *handle, enum cmdq_event event);

/* like cmdq_op_wait, but won't clear the event after
 * leaving wait state.
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type wait for
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_wait_no_clear(struct cmdqRecStruct *handle,
	enum cmdq_event event);
s32 cmdqRecWaitNoClear(struct cmdqRecStruct *handle,
	enum cmdq_event event);

/* Unconditionally set to given event to 0.
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type to set
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_clear_event(struct cmdqRecStruct *handle,
	enum cmdq_event event);
s32 cmdqRecClearEventToken(struct cmdqRecStruct *handle,
	enum cmdq_event event);

/* Unconditionally set to given event to 1.
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type to set
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_set_event(struct cmdqRecStruct *handle,
	enum cmdq_event event);
s32 cmdqRecSetEventToken(struct cmdqRecStruct *handle,
	enum cmdq_event event);

/* Replace overwite CPR parameters of arg_a.
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   index: the index of instruction to replace
 *	   new_arg_a: the desired cpr value to overwrite arg_a
 *	   new_arg_b: the desired cpr value to overwrite arg_b
 *	   new_arg_c: the desired cpr value to overwrite arg_c
 * Return:
 *	   0 for success; else the error code is returned
 */
s32 cmdq_op_replace_overwrite_cpr(struct cmdqRecStruct *handle, u32 index,
	s32 new_arg_a, s32 new_arg_b, s32 new_arg_c);

/* Read a register value to a CMDQ general purpose register(GPR)
 * Parameter:
 *     handle: the command queue recorder handle
 *     hw_addr: register address to read from
 *     dst_data_reg: CMDQ GPR to write to
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_read_to_data_register(struct cmdqRecStruct *handle,
	u32 hw_addr, enum cmdq_gpr_reg dst_data_reg);
s32 cmdqRecReadToDataRegister(struct cmdqRecStruct *handle, u32 hw_addr,
	enum cmdq_gpr_reg dst_data_reg);

/* Write a register value from a CMDQ general purpose register(GPR)
 * Parameter:
 *     handle: the command queue recorder handle
 *     src_data_reg: CMDQ GPR to read from
 *     hw_addr: register address to write to
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_write_from_data_register(struct cmdqRecStruct *handle,
	enum cmdq_gpr_reg src_data_reg, u32 hw_addr);
s32 cmdqRecWriteFromDataRegister(struct cmdqRecStruct *handle,
	enum cmdq_gpr_reg src_data_reg, u32 hw_addr);


/* Allocate 32-bit register backup slot
 */
s32 cmdq_alloc_mem(cmdqBackupSlotHandle *p_h_backup_slot, u32 slotCount);
s32 cmdqBackupAllocateSlot(cmdqBackupSlotHandle *p_h_backup_slot,
	u32 slotCount);

/* Read 32-bit register backup slot by index
 */
s32 cmdq_cpu_read_mem(cmdqBackupSlotHandle h_backup_slot, u32 slot_index,
	u32 *value);
s32 cmdqBackupReadSlot(cmdqBackupSlotHandle h_backup_slot, u32 slot_index,
	u32 *value);

/* Use CPU to write value into 32-bit register backup slot by index directly.
 */
s32 cmdq_cpu_write_mem(cmdqBackupSlotHandle h_backup_slot,
	u32 slot_index, u32 value);
s32 cmdqBackupWriteSlot(cmdqBackupSlotHandle h_backup_slot, u32 slot_index,
	u32 value);


/* Free allocated backup slot. DO NOT free them before corresponding
 * task finishes. Becareful on AsyncFlush use cases.
 */
s32 cmdq_free_mem(cmdqBackupSlotHandle h_backup_slot);
s32 cmdqBackupFreeSlot(cmdqBackupSlotHandle h_backup_slot);


/* Insert instructions to backup given 32-bit HW register
 * to a backup slot.
 * You can use cmdq_cpu_read_mem() to retrieve the result
 * AFTER cmdq_task_flush() returns, or INSIDE the callback of
 * cmdq_task_flush_async_callback().
 */
s32 cmdq_op_read_reg_to_mem(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index, u32 addr);
s32 cmdqRecBackupRegisterToSlot(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index, u32 addr);

/* Insert instructions to write 32-bit HW register
 * from a backup slot.
 * You can use cmdq_cpu_read_mem() to retrieve the result
 * AFTER cmdq_task_flush() returns, or INSIDE the callback of
 * cmdq_task_flush_async_callback().
 */
s32 cmdq_op_read_mem_to_reg(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index, u32 addr);
s32 cmdqRecBackupWriteRegisterFromSlot(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index, u32 addr);

/* Insert instructions to update slot with given 32-bit value
 * You can use cmdq_cpu_read_mem() to retrieve the result
 * AFTER cmdq_task_flush() returns, or INSIDE the callback of
 * cmdq_task_flush_async_callback().
 */
s32 cmdq_op_write_mem(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index, u32 value);
s32 cmdqRecBackupUpdateSlot(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index, u32 value);

void cmdq_task_prepare(struct cmdqRecStruct *handle);
void cmdq_task_unprepare(struct cmdqRecStruct *handle);

/* Trigger CMDQ to execute the recorded commands
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 * Note:
 *     This is a synchronous function. When the function
 *     returned, the recorded commands have been done.
 */
s32 cmdq_task_flush(struct cmdqRecStruct *handle);
s32 cmdqRecFlush(struct cmdqRecStruct *handle);

/* Flush the command; Also at the end of the command, backup registers
 * appointed by addrArray.
 */
s32 cmdq_task_append_backup_reg(struct cmdqRecStruct *handle,
	u32 reg_count, u32 *addrs);
s32 cmdq_task_flush_and_read_register(struct cmdqRecStruct *handle,
	u32 reg_count, u32 *addrs, u32 *values_out);
s32 cmdqRecFlushAndReadRegister(struct cmdqRecStruct *handle,
	u32 reg_count, u32 *addrs, u32 *values_out);

/* Trigger CMDQ to asynchronously execute the recorded commands
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for successfully start execution; else the error code is returned
 * Note:
 *     This is an ASYNC function. When the function
 *     returned, it may or may not be finished. There is no way to retrieve
 *     the result.
 */
s32 cmdq_task_flush_async(struct cmdqRecStruct *handle);
s32 cmdqRecFlushAsync(struct cmdqRecStruct *handle);

s32 cmdq_task_flush_async_callback(struct cmdqRecStruct *handle,
	CmdqAsyncFlushCB callback, u64 user_data);
s32 cmdqRecFlushAsyncCallback(struct cmdqRecStruct *handle,
	CmdqAsyncFlushCB callback, u64 user_data);

/* Trigger CMDQ to execute the recorded commands in loop.
 * each loop completion generates callback in interrupt context.
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     irqCallback: this CmdqInterruptCB callback is called after each loop
 *         completion.
 *     data:   user data, this will pass back to irqCallback
 *     hLoop:  output, a handle used to stop this loop.
 *
 * Return:
 *     0 for success; else the error code is returned
 *
 * Note:
 *     This is an asynchronous function. When the function
 *     returned, the thread has started. Return -1 in irqCallback to stop it.
 */
s32 cmdq_task_start_loop(struct cmdqRecStruct *handle);
s32 cmdqRecStartLoop(struct cmdqRecStruct *handle);

s32 cmdq_task_start_loop_callback(struct cmdqRecStruct *handle,
	CmdqInterruptCB loopCB, unsigned long loopData);
s32 cmdqRecStartLoopWithCallback(struct cmdqRecStruct *handle,
	CmdqInterruptCB loopCB, unsigned long loopData);

s32 cmdq_task_start_loop_sram(struct cmdqRecStruct *handle,
	const char *SRAM_owner_name);

/* Unconditionally stops the loop thread.
 * Must call after cmdq_task_start_loop().
 */
s32 cmdq_task_stop_loop(struct cmdqRecStruct *handle);
s32 cmdqRecStopLoop(struct cmdqRecStruct *handle);

/* Trigger CMDQ to copy data between DRAM and SRAM.
 *
 * Parameter:
 *     pa_src: the copy to source of DRAM PA address
 *     pa_dest: the copy from destination of DRAM PA address
 *     sram_src: the copy to destination of SRAM address
 *     sram_dest: the copy from source of SRAM address
 *     size: the copy size
 *
 * Return:
 *     0 for success; else the error code is returned
 *
 * Note:
 *     This is an BLOCKING function. When the function is returned,
 *     the SRAM move is done.
 */
s32 cmdq_task_copy_to_sram(dma_addr_t pa_src, u32 sram_dest, size_t size);
s32 cmdq_task_copy_from_sram(dma_addr_t pa_dest, u32 sram_src, size_t size);

/* returns current count of instructions in given handle
 */
s32 cmdq_task_get_instruction_count(struct cmdqRecStruct *handle);
s32 cmdqRecGetInstructionCount(struct cmdqRecStruct *handle);

/* Record timestamp while CMDQ HW executes here
 * This is for prfiling  purpose.
 *
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_profile_marker(struct cmdqRecStruct *handle, const char *tag);
s32 cmdqRecProfileMarker(struct cmdqRecStruct *handle, const char *tag);

/* Dump command buffer to kernel log
 * This is for debugging purpose.
 */
s32 cmdqRecDumpCommand(struct cmdqRecStruct *handle);

/* Destroy command queue recorder handle
 * Parameter:
 *     handle: the command queue recorder handle
 */
s32 cmdq_task_destroy(struct cmdqRecStruct *handle);
void cmdqRecDestroy(struct cmdqRecStruct *handle);

/* Change instruction of index to NOP instruction
 * Current NOP is [JUMP + 8]
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     index: the index of replaced instruction (start from 0)
 * Return:
 *     > 0 (index) for success; else the error code is returned
 */
s32 cmdq_op_set_nop(struct cmdqRecStruct *handle, u32 index);
s32 cmdqRecSetNOP(struct cmdqRecStruct *handle, u32 index);

/* Query offset of instruction by instruction name
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     startIndex: Query offset from "startIndex" of instruction (start from 0)
 *     opCode: instruction name you can use the following 6 instruction names:
 *	CMDQ_CODE_WFE: create via cmdq_op_wait()
 *	CMDQ_CODE_SET_TOKEN: create via cmdq_op_set_event()
 *	CMDQ_CODE_WAIT_NO_CLEAR: create via cmdq_op_wait_no_clear()
 *	CMDQ_CODE_CLEAR_TOKEN: create via cmdq_op_clear_event()
 *	CMDQ_CODE_PREFETCH_ENABLE: create via cmdqRecEnablePrefetch()
 *	CMDQ_CODE_PREFETCH_DISABLE: create via cmdqRecDisablePrefetch()
 *     event: the desired event type to set, clear, or wait
 * Return:
 *     > 0 (index) for offset of instruction; else the error code is returned
 */
s32 cmdq_task_query_offset(struct cmdqRecStruct *handle, u32 startIndex,
	const enum cmdq_code opCode, enum cmdq_event event);
s32 cmdqRecQueryOffset(struct cmdqRecStruct *handle, u32 startIndex,
	const enum cmdq_code opCode, enum cmdq_event event);

/* acquire resource by resourceEvent
 * Parameter:
 *     handle: the command queue recorder handle
 *     resourceEvent: the event of resource to control in GCE thread
 * Return:
 *     0 for success; else the error code is returned
 * Note:
 *       mutex protected, be careful
 */
s32 cmdq_resource_acquire(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent);
s32 cmdqRecAcquireResource(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent);

/* acquire resource by resourceEvent and ALSO ADD write instruction to use
 * resource
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   resourceEvent: the event of resource to control in GCE thread
 *       addr, value, mask: same as cmdq_op_write_reg
 * Return:
 *	   0 for success; else the error code is returned
 * Note:
 *       mutex protected, be careful
 *	   Order: CPU clear resourceEvent at first, then add write instruction
 */
s32 cmdq_resource_acquire_and_write(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent, u32 addr, u32 value, u32 mask);
s32 cmdqRecWriteForResource(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent, u32 addr, u32 value, u32 mask);

/* Release resource by ADD INSTRUCTION to set event
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   resourceEvent: the event of resource to control in GCE thread
 * Return:
 *	   0 for success; else the error code is returned
 * Note:
 *       mutex protected, be careful
 *       Remember to flush handle after this API to release resource via GCE
 */
s32 cmdq_resource_release(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent);
s32 cmdqRecReleaseResource(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent);

/* Release resource by ADD INSTRUCTION to set event
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   resourceEvent: the event of resource to control in GCE thread
 *	   addr, value, mask: same as cmdq_op_write_reg
 * Return:
 *	   0 for success; else the error code is returned
 * Note:
 *       mutex protected, be careful
 *	   Order: Add add write instruction at first, then set resourceEvent
 *	   instruction
 *       Remember to flush handle after this API to release resource via GCE
 */
s32 cmdq_resource_release_and_write(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent, u32 addr, u32 value, u32 mask);
s32 cmdqRecWriteAndReleaseResource(struct cmdqRecStruct *handle,
	enum cmdq_event resourceEvent, u32 addr, u32 value, u32 mask);

s32 cmdq_task_create_delay_thread_dram(void **pp_delay_thread_buffer,
	u32 *buffer_size);
s32 cmdq_task_create_delay_thread_sram(void **pp_delay_thread_buffer,
	u32 *buffer_size, u32 *cpr_offset);


/* Initialize the logical variable
 * Parameter:
 *	   arg: the variable you want to Initialize
 */
void cmdq_op_init_variable(CMDQ_VARIABLE *arg);

/* Initialize the logical variable
 * Parameter:
 *	   arg: the variable you want to Initialize
 *      cpr_offset: the cpr offset you want to use
 */
void cmdq_op_init_global_cpr_variable(CMDQ_VARIABLE *arg, u32 cpr_offset);

/* Append logic command for assign
 * arg_out = arg_b
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_in: the specified GCE CPR or value
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_assign(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_in);

/* Append logic command for addition
 * arg_out = arg_b + arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_add(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/* Append logic command for subtraction
 * arg_out = arg_b - arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_subtract(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/* Append logic command for multiplication
 * arg_out = arg_b * arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_multiply(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/* Append logic command for exclusive or operation
 * arg_out = arg_b ^ arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_xor(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/* Append logic command for not operation
 * arg_out = ~arg_b
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_not(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b);

/* Append logic command for or operation
 * arg_out = arg_b | arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_or(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/* Append logic command for or operation
 * arg_out = arg_b & arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_and(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/* Append logic command for left shift operation
 * arg_out = arg_b << arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_left_shift(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/* Append logic command for left right operation
 * arg_out = arg_b >> arg_c
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out: the output value save in GCE CPR
 *     arg_b: the value who use to do logical operation
 *     arg_c: the value who use to do logical operation
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_right_shift(struct cmdqRecStruct *handle, CMDQ_VARIABLE *arg_out,
	CMDQ_VARIABLE arg_b, CMDQ_VARIABLE arg_c);

/* Append commands for delay (micro second)
 * Parameter:
 *     handle: the command queue recorder handle
 *     delay_time: delay time in us
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_delay_us(struct cmdqRecStruct *handle, u32 delay_time);
s32 cmdq_op_backup_CPR(struct cmdqRecStruct *handle, CMDQ_VARIABLE cpr,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index);
s32 cmdq_op_backup_TPR(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index);

/* Append if statement command
 * if (arg_b condition arg_c)
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_b: the value who use to do conditional operation
 *     arg_condition: conditional operator
 *     arg_c: the value who use to do conditional operation
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_if(struct cmdqRecStruct *handle, CMDQ_VARIABLE arg_b,
	enum CMDQ_CONDITION_ENUM arg_condition, CMDQ_VARIABLE arg_c);

/* Append end if statement command
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_end_if(struct cmdqRecStruct *handle);

/* Append else statement command
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_else(struct cmdqRecStruct *handle);

/* Append if statement command
 * else if (arg_b condition arg_c)
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_b: the value who use to do conditional operation
 *     arg_condition: conditional operator
 *     arg_c: the value who use to do conditional operation
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_else_if(struct cmdqRecStruct *handle, CMDQ_VARIABLE arg_b,
	enum CMDQ_CONDITION_ENUM arg_condition, CMDQ_VARIABLE arg_c);

/* Append while statement command
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_b: the value who use to do conditional operation
 *     arg_condition: conditional operator
 *     arg_c: the value who use to do conditional operation
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_while(struct cmdqRecStruct *handle, CMDQ_VARIABLE arg_b,
	enum CMDQ_CONDITION_ENUM arg_condition, CMDQ_VARIABLE arg_c);

/* Append continue statement command into while loop
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_continue(struct cmdqRecStruct *handle);

/* Append break statement command into while loop
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_break(struct cmdqRecStruct *handle);

/* Append end while statement command
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_end_while(struct cmdqRecStruct *handle);

/* Append do-while statement command
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_do_while(struct cmdqRecStruct *handle);

/* Append end do while statement command
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_b: the value who use to do conditional operation
 *     arg_condition: conditional operator
 *     arg_c: the value who use to do conditional operation
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_end_do_while(struct cmdqRecStruct *handle, CMDQ_VARIABLE arg_b,
	enum CMDQ_CONDITION_ENUM arg_condition, CMDQ_VARIABLE arg_c);

/* Linux like wait_event_timeout
 * Parameter:
 *     handle: the command queue recorder handle
 *     arg_out, wait result (=0: timeout, >0: wait time when got event)
 *     wait_event: GCE event
 *     timeout_time: timeout time in us
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_wait_event_timeout(struct cmdqRecStruct *handle,
	CMDQ_VARIABLE *arg_out, enum cmdq_event wait_event,
	u32 timeout_time);

/* Append write command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 *     addr: the specified source register physical address
 *     arg_out: the value will be save in GCE CPR
 *     mask: the specified target register mask
 * Return:
 *     0 for success; else the error code is returned
 */
s32 cmdq_op_read_reg(struct cmdqRecStruct *handle, u32 addr,
	CMDQ_VARIABLE *arg_out, u32 mask);

/* Insert instructions to write to CMDQ variable
 * from a backup memory.
 */
s32 cmdq_op_read_mem(struct cmdqRecStruct *handle,
	cmdqBackupSlotHandle h_backup_slot, u32 slot_index,
	CMDQ_VARIABLE *arg_out);

#endif	/* __CMDQ_RECORD_H__ */
