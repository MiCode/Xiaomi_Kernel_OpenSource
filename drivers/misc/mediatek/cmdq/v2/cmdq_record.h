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
#include <linux/uaccess.h>
#include "cmdq_def.h"
#include "cmdq_core.h"

struct TaskStruct;
typedef uint64_t CMDQ_VARIABLE;

struct task_private {
	void *node_private_data;
	bool internal;		/* internal used only task */
	bool ignore_timeout;	/* timeout is expected */
};

struct cmdqRecStruct {
	uint64_t engineFlag;
	int32_t scenario;
	uint32_t blockSize;	/* command size */
	void *pBuffer;
	uint32_t bufferSize;	/* allocated buffer size */
	/* running task after flush() or startLoop() */
	struct TaskStruct *pRunningTask;
	/* setting high priority. This implies Prefetch ENABLE. */
	enum CMDQ_HW_THREAD_PRIORITY_ENUM priority;
	bool finalized;		/* set to true after flush() or startLoop() */
	uint32_t prefetchCount;	/* maintenance prefetch instruction */

	/* register backup at end of task */
	u32 reg_count;
	u32 *reg_values;
	dma_addr_t reg_values_pa;
	/* user space data */
	u32 user_reg_count;
	u32 user_token;
	struct TaskStruct *mdp_meta_task;
	bool get_meta_task;

	struct cmdqSecDataStruct secData;	/* secure execution data */

	/* Readback slot protection */
	s32 slot_ids[8];

	/* profile marker */
#ifdef CMDQ_PROFILE_MARKER_SUPPORT
	struct cmdqProfileMarkerStruct profileMarker;
#endif
};

/* typedef dma_addr_t cmdqBackupSlotHandle; */
#define cmdqBackupSlotHandle dma_addr_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Create command queue recorder handle
 * Parameter:
 *     pHandle: pointer to retrieve the handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_task_create(enum CMDQ_SCENARIO_ENUM scenario,
		struct cmdqRecStruct **pHandle);
	int32_t cmdqRecCreate(enum CMDQ_SCENARIO_ENUM scenario,
		struct cmdqRecStruct **pHandle);

/**
 * Set engine flag for command queue picking HW thread
 * Parameter:
 *     pHandle: pointer to retrieve the handle
 *     engineFlag: Flag use to identify which HW module can be accessed
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_task_set_engine(struct cmdqRecStruct *handle,
		uint64_t engineFlag);
	int32_t cmdqRecSetEngine(struct cmdqRecStruct *handle,
		uint64_t engineFlag);

/**
 * Reset command queue recorder commands
 * Parameter:
 *    handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_task_reset(struct cmdqRecStruct *handle);
	int32_t cmdqRecReset(struct cmdqRecStruct *handle);

/**
 * Configure as secure task
 * Parameter:
 *     handle: the command queue recorder handle
 *     is_secure: true, execute the command in secure world
 * Return:
 *     0 for success; else the error code is returned
 *
 * Note:
 *     a. Secure CMDQ support when t-base enabled only
 *     b. By default struct cmdqRecStruct records a normal command,
 *	please call cmdq_task_set_secure to set
 *	command as SECURE after cmdq_task_reset
 */
	int32_t cmdq_task_set_secure(struct cmdqRecStruct *handle,
		const bool is_secure);
	int32_t cmdqRecSetSecure(struct cmdqRecStruct *handle,
		const bool is_secure);

/**
 * query handle is secure task or not
 * Parameter:
 *	  handle: the command queue recorder handle
 * Return:
 *	   0 for false (not secure) and 1 for true (is secure)
 */
	int32_t cmdq_task_is_secure(struct cmdqRecStruct *handle);
	int32_t cmdqRecIsSecure(struct cmdqRecStruct *handle);

/**
 * Add DPAC protection flag
 * Parameter:
 * Note:
 *     a. Secure CMDQ support when t-base enabled only
 *     b. after reset handle, user have to specify protection flag again
 */
	int32_t cmdq_task_secure_enable_dapc(
		struct cmdqRecStruct *handle,
		const uint64_t engineFlag);
	int32_t cmdqRecSecureEnableDAPC(
		struct cmdqRecStruct *handle,
		const uint64_t engineFlag);

/**
 * Add flag for M4U security ports
 * Parameter:
 * Note:
 *	   a. Secure CMDQ support when t-base enabled only
 *	   b. after reset handle, user have to specify protection flag again
 */
	int32_t cmdq_task_secure_enable_port_security(
		struct cmdqRecStruct *handle,
		const uint64_t engineFlag);
	int32_t cmdqRecSecureEnablePortSecurity(
		struct cmdqRecStruct *handle,
		const uint64_t engineFlag);

/**
 * Append mark command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecMark(struct cmdqRecStruct *handle);

/**
 * Append mark command to enable prefetch
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecEnablePrefetch(struct cmdqRecStruct *handle);

/**
 * Append mark command to disable prefetch
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecDisablePrefetch(struct cmdqRecStruct *handle);

/**
 * Append write command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 *     addr: the specified target register physical address
 *     argument / value: the specified target register value
 *     mask: the specified target register mask
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_write_reg(struct cmdqRecStruct *handle,
		uint32_t addr,
		CMDQ_VARIABLE argument, uint32_t mask);
	int32_t cmdqRecWrite(struct cmdqRecStruct *handle,
		uint32_t addr, uint32_t value, uint32_t mask);

/**
 * Append write command to the update secure buffer address in secure path
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   addr: the specified register physical address
 *		about module src/dst buffer address
 *	   type: base handle type
 *     base handle: secure handle of a secure mememory
 *     offset: offset related to base handle
 *	(secure buffer = addr(base_handle) + offset)
 *     size: secure buffer size
 *	   mask: 0xFFFF_FFFF
 * Return:
 *	   0 for success; else the error code is returned
 * Note:
 *     support only when secure OS enabled
 */
	int32_t cmdq_op_write_reg_secure(struct cmdqRecStruct *handle,
		uint32_t addr,
		enum CMDQ_SEC_ADDR_METADATA_TYPE type, uint64_t baseHandle,
		uint32_t offset, uint32_t size, uint32_t port);
	int32_t cmdqRecWriteSecure(struct cmdqRecStruct *handle,
		uint32_t addr,
		enum CMDQ_SEC_ADDR_METADATA_TYPE type,
		uint64_t baseHandle,
		uint32_t offset, uint32_t size, uint32_t port);

/* tablet use */
/*
 * Record and memorize the index of write command to trust zone in secure path
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   addr: the specified target register physical address
 *	   type: base handle type
 *	   value: the specified target register value
 *       mask: the specified target register mask
 * Return:
 *	   0 for success; else the error code is returned
 * Note:
 *	   support only when secure OS enabled
 */
#ifdef CONFIG_MTK_CMDQ_TAB
	int32_t cmdq_op_write_reg_secure_mask(
		struct cmdqRecStruct *handle, uint32_t addr,
		enum CMDQ_SEC_ADDR_METADATA_TYPE type,
		uint32_t value, uint32_t mask);
	int32_t cmdqRecWriteSecureMask(struct cmdqRecStruct *handle,
		uint32_t addr,
		enum CMDQ_SEC_ADDR_METADATA_TYPE type,
		uint32_t value, uint32_t mask);
#endif

/**
 * Append poll command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 *     addr: the specified register physical address
 *     value: the required register value
 *     mask: the required register mask
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_poll(struct cmdqRecStruct *handle,
		uint32_t addr, uint32_t value, uint32_t mask);
	int32_t cmdqRecPoll(struct cmdqRecStruct *handle,
		uint32_t addr, uint32_t value, uint32_t mask);

/**
 * Append wait command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type to "wait and CLEAR"
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_wait(struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM event);
	int32_t cmdqRecWait(struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM event);

/**
 * like cmdq_op_wait, but won't clear the event after
 * leaving wait state.
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type wait for
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_wait_no_clear(struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM event);
	int32_t cmdqRecWaitNoClear(struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM event);

/**
 * Unconditionally set to given event to 0.
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type to set
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_clear_event(struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM event);
	int32_t cmdqRecClearEventToken(struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM event);

/**
 * Unconditionally set to given event to 1.
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type to set
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_set_event(struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM event);
	int32_t cmdqRecSetEventToken(struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM event);

/**
 * Read a register value to a CMDQ general purpose register(GPR)
 * Parameter:
 *     handle: the command queue recorder handle
 *     hw_addr: register address to read from
 *     dst_data_reg: CMDQ GPR to write to
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_read_to_data_register(
		struct cmdqRecStruct *handle, uint32_t hw_addr,
		enum CMDQ_DATA_REGISTER_ENUM dst_data_reg);
	int32_t cmdqRecReadToDataRegister(
		struct cmdqRecStruct *handle, uint32_t hw_addr,
		enum CMDQ_DATA_REGISTER_ENUM dst_data_reg);

/**
 * Write a register value from a CMDQ general purpose register(GPR)
 * Parameter:
 *     handle: the command queue recorder handle
 *     src_data_reg: CMDQ GPR to read from
 *     hw_addr: register address to write to
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdq_op_write_from_data_register(
		struct cmdqRecStruct *handle,
		enum CMDQ_DATA_REGISTER_ENUM src_data_reg,
		uint32_t hw_addr);
	int32_t cmdqRecWriteFromDataRegister(
		struct cmdqRecStruct *handle,
		enum CMDQ_DATA_REGISTER_ENUM src_data_reg,
		uint32_t hw_addr);


/**
 *  Allocate 32-bit register backup slot
 *
 */
	int32_t cmdq_alloc_mem(cmdqBackupSlotHandle *p_h_backup_slot,
		uint32_t slotCount);
	int32_t cmdqBackupAllocateSlot(
		cmdqBackupSlotHandle *p_h_backup_slot,
		uint32_t slotCount);

/**
 *  Read 32-bit register backup slot by index
 *
 */
	int32_t cmdq_cpu_read_mem(cmdqBackupSlotHandle h_backup_slot,
		uint32_t slot_index,
		uint32_t *value);
	int32_t cmdqBackupReadSlot(cmdqBackupSlotHandle h_backup_slot,
		uint32_t slot_index,
		uint32_t *value);

/**
 *  Use CPU to write value into 32-bit register backup slot by index directly.
 *
 */
	int32_t cmdq_cpu_write_mem(cmdqBackupSlotHandle h_backup_slot,
		uint32_t slot_index,
		uint32_t value);
	int32_t cmdqBackupWriteSlot(cmdqBackupSlotHandle h_backup_slot,
		uint32_t slot_index,
		uint32_t value);


/**
 *  Free allocated backup slot. DO NOT free them before corresponding
 *  task finishes. Becareful on AsyncFlush use cases.
 *
 */
	int32_t cmdq_free_mem(cmdqBackupSlotHandle h_backup_slot);
	int32_t cmdqBackupFreeSlot(cmdqBackupSlotHandle h_backup_slot);


/**
 *  Insert instructions to backup given 32-bit HW register
 *  to a backup slot.
 *  You can use cmdq_cpu_read_mem() to retrieve the result
 *  AFTER cmdq_task_flush() returns, or INSIDE
 * the callback of cmdq_task_flush_async_callback().
 *
 */
	int32_t cmdq_op_read_reg_to_mem(struct cmdqRecStruct *handle,
		cmdqBackupSlotHandle h_backup_slot,
		uint32_t slot_index, uint32_t addr);
	int32_t cmdqRecBackupRegisterToSlot(
		struct cmdqRecStruct *handle,
		cmdqBackupSlotHandle h_backup_slot,
		uint32_t slot_index, uint32_t addr);

/**
 *  Insert instructions to write 32-bit HW register
 *  from a backup slot.
 *  You can use cmdq_cpu_read_mem() to retrieve the result
 *  AFTER cmdq_task_flush() returns, or INSIDE the callback
 * of cmdq_task_flush_async_callback().
 *
 */
	int32_t cmdq_op_read_mem_to_reg(
		struct cmdqRecStruct *handle,
		cmdqBackupSlotHandle h_backup_slot,
		uint32_t slot_index, uint32_t addr);
	int32_t cmdqRecBackupWriteRegisterFromSlot(
		struct cmdqRecStruct *handle,
		cmdqBackupSlotHandle h_backup_slot,
		uint32_t slot_index, uint32_t addr);

/**
 *  Insert instructions to update slot with given 32-bit value
 *  You can use cmdq_cpu_read_mem() to retrieve the result
 *  AFTER cmdq_task_flush() returns, or INSIDE
 * the callback of cmdq_task_flush_async_callback().
 *
 */
	int32_t cmdq_op_write_mem(struct cmdqRecStruct *handle,
		cmdqBackupSlotHandle h_backup_slot,
		uint32_t slot_index, uint32_t value);
	int32_t cmdqRecBackupUpdateSlot(struct cmdqRecStruct *handle,
		cmdqBackupSlotHandle h_backup_slot,
		uint32_t slot_index, uint32_t value);

/**
 * Trigger CMDQ to execute the recorded commands
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 * Note:
 *     This is a synchronous function. When the function
 *     returned, the recorded commands have been done.
 */
	int32_t cmdq_task_flush(struct cmdqRecStruct *handle);
	int32_t cmdqRecFlush(struct cmdqRecStruct *handle);

/**
 *  Flush the command; Also at the end of the command, backup registers
 *  appointed by addrArray.
 *
 */
	int32_t cmdq_task_flush_and_read_register(
		struct cmdqRecStruct *handle, uint32_t regCount,
		uint32_t *addrArray, uint32_t *valueArray);
	int32_t cmdqRecFlushAndReadRegister(
		struct cmdqRecStruct *handle, uint32_t regCount,
		uint32_t *addrArray, uint32_t *valueArray);

/**
 * Trigger CMDQ to asynchronously execute the recorded commands
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for successfully start execution; else the error code is returned
 * Note:
 *     This is an ASYNC function. When the function
 *     returned, it may or may not be finished.
 *	There is no way to retrieve the result.
 */
	int32_t cmdq_task_flush_async(struct cmdqRecStruct *handle);
	int32_t cmdqRecFlushAsync(struct cmdqRecStruct *handle);

	int32_t cmdq_task_flush_async_callback(
		struct cmdqRecStruct *handle, CmdqAsyncFlushCB callback,
		uint32_t userData);
	int32_t cmdqRecFlushAsyncCallback(
		struct cmdqRecStruct *handle, CmdqAsyncFlushCB callback,
		uint32_t userData);

/**
 * Trigger CMDQ to execute the recorded commands in loop.
 * each loop completion generates callback in interrupt context.
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     irqCallback: this CmdqInterruptCB callback
 *	is called after each loop completion.
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
	int32_t cmdq_task_start_loop(struct cmdqRecStruct *handle);
	int32_t cmdqRecStartLoop(struct cmdqRecStruct *handle);

	int32_t cmdq_task_start_loop_callback(
		struct cmdqRecStruct *handle, CmdqInterruptCB loopCB,
		unsigned long loopData);
	int32_t cmdqRecStartLoopWithCallback(
		struct cmdqRecStruct *handle, CmdqInterruptCB loopCB,
		unsigned long loopData);

/**
 * Unconditionally stops the loop thread.
 * Must call after cmdq_task_start_loop().
 */
	int32_t cmdq_task_stop_loop(struct cmdqRecStruct *handle);
	int32_t cmdqRecStopLoop(struct cmdqRecStruct *handle);

/**
 * returns current count of instructions in given handle
 */
	int32_t cmdq_task_get_instruction_count(
		struct cmdqRecStruct *handle);
	int32_t cmdqRecGetInstructionCount(
		struct cmdqRecStruct *handle);

/**
 * Record timestamp while CMDQ HW executes here
 * This is for prfiling  purpose.
 *
 * Return:
 *     0 for success; else the error code is returned
 *
 * Note:
 *     Please define CMDQ_PROFILE_MARKER_SUPPORT in cmdq_def.h
 *     to enable profile marker.
 */
	int32_t cmdq_op_profile_marker(struct cmdqRecStruct *handle,
		const char *tag);
	int32_t cmdqRecProfileMarker(struct cmdqRecStruct *handle,
		const char *tag);

/**
 * Dump command buffer to kernel log
 * This is for debugging purpose.
 */
	int32_t cmdq_task_dump_command(struct cmdqRecStruct *handle);
	int32_t cmdqRecDumpCommand(struct cmdqRecStruct *handle);

/**
 * Estimate command execu time.
 * This is for debugging purpose.
 *
 * Note this estimation supposes all POLL/WAIT condition pass immediately
 */
	int32_t cmdq_task_estimate_command_exec_time(
		const struct cmdqRecStruct *handle);
	int32_t cmdqRecEstimateCommandExecTime(
		const struct cmdqRecStruct *handle);

/**
 * Destroy command queue recorder handle
 * Parameter:
 *     handle: the command queue recorder handle
 */
	int32_t cmdq_task_destroy(struct cmdqRecStruct *handle);
	void cmdqRecDestroy(struct cmdqRecStruct *handle);

/**
 * Change instruction of index to NOP instruction
 * Current NOP is [JUMP + 8]
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     index: the index of replaced instruction (start from 0)
 * Return:
 *     > 0 (index) for success; else the error code is returned
 */
	int32_t cmdq_op_set_nop(struct cmdqRecStruct *handle,
		uint32_t index);
	int32_t cmdqRecSetNOP(struct cmdqRecStruct *handle,
		uint32_t index);

/**
 * Query offset of instruction by instruction name
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     startIndex: Query offset from "startIndex" of instruction (start from 0)
 *     opCode: instruction name, you can use the following 6 instruction names:
 *		CMDQ_CODE_WFE: create via cmdq_op_wait()
 *		CMDQ_CODE_SET_TOKEN: create via cmdq_op_set_event()
 *		CMDQ_CODE_WAIT_NO_CLEAR: create via cmdq_op_wait_no_clear()
 *		CMDQ_CODE_CLEAR_TOKEN: create via cmdq_op_clear_event()
 *		CMDQ_CODE_PREFETCH_ENABLE: create via cmdqRecEnablePrefetch()
 *		CMDQ_CODE_PREFETCH_DISABLE: create via cmdqRecDisablePrefetch()
 *     event: the desired event type to set, clear, or wait
 * Return:
 *     > 0 (index) for offset of instruction; else the error code is returned
 */
	int32_t cmdq_task_query_offset(struct cmdqRecStruct *handle,
		uint32_t startIndex,
		const enum CMDQ_CODE_ENUM opCode,
		enum CMDQ_EVENT_ENUM event);
	int32_t cmdqRecQueryOffset(struct cmdqRecStruct *handle,
		uint32_t startIndex,
		const enum CMDQ_CODE_ENUM opCode,
		enum CMDQ_EVENT_ENUM event);

/**
 * acquire resource by resourceEvent
 * Parameter:
 *     handle: the command queue recorder handle
 *     resourceEvent: the event of resource to control in GCE thread
 * Return:
 *     0 for success; else the error code is returned
 * Note:
 *       mutex protected, be careful
 */
	int32_t cmdq_resource_acquire(struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM resourceEvent);
	int32_t cmdqRecAcquireResource(struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM resourceEvent);

/**
 * acquire resource by resourceEvent and ALSO
 * ADD write instruction to use resource
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
	int32_t cmdq_resource_acquire_and_write(
		struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM resourceEvent,
		uint32_t addr, uint32_t value, uint32_t mask);
	int32_t cmdqRecWriteForResource(
		struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM resourceEvent,
		uint32_t addr, uint32_t value, uint32_t mask);

/**
 * Release resource by ADD INSTRUCTION to set event
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   resourceEvent: the event of resource to control in GCE thread
 * Return:
 *	   0 for success; else the error code is returned
 * Note:
 *       mutex protected, be careful
 *       Remember to flush handle after this API to release resource via GCE
 */
	int32_t cmdq_resource_release(struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM resourceEvent);
	int32_t cmdqRecReleaseResource(struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM resourceEvent);

/**
 * Release resource by ADD INSTRUCTION to set event
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   resourceEvent: the event of resource to control in GCE thread
 *	   addr, value, mask: same as cmdq_op_write_reg
 * Return:
 *	   0 for success; else the error code is returned
 * Note:
 *       mutex protected, be careful
 *	   Order: Add add write instruction at first,
 *		then set resourceEvent instruction
 *       Remember to flush handle after this API to release resource via GCE
 */
	int32_t cmdq_resource_release_and_write(
		struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM resourceEvent,
		uint32_t addr, uint32_t value, uint32_t mask);
	int32_t cmdqRecWriteAndReleaseResource(
		struct cmdqRecStruct *handle,
		enum CMDQ_EVENT_ENUM resourceEvent,
		uint32_t addr, uint32_t value, uint32_t mask);

/* MDP META use */
	struct op_meta;
	struct mdp_submit;

	struct cmdq_command_buffer {
		void *va_base;
		u32 cmd_buf_size;
		u32 avail_buf_size;
	};
	s32 cmdq_op_poll_ex(struct cmdqRecStruct *handle,
		struct cmdq_command_buffer *cmd_buf, u32 addr,
		CMDQ_VARIABLE value, u32 mask);
	s32 cmdq_op_read_reg_to_mem_ex(struct cmdqRecStruct *handle,
		struct cmdq_command_buffer *cmd_buf,
		cmdqBackupSlotHandle h_backup_slot, u32 slot_index, u32 addr);
	s32 cmdq_op_write_reg_ex(struct cmdqRecStruct *handle,
		struct cmdq_command_buffer *cmd_buf, u32 addr,
		CMDQ_VARIABLE value, u32 mask);
	s32 cmdq_op_wait_ex(struct cmdqRecStruct *handle,
		struct cmdq_command_buffer *cmd_buf, enum CMDQ_EVENT_ENUM event);
	s32 cmdq_op_wait_no_clear_ex(struct cmdqRecStruct *handle,
		struct cmdq_command_buffer *cmd_buf, enum CMDQ_EVENT_ENUM event);
	s32 cmdq_op_clear_event_ex(struct cmdqRecStruct *handle,
		struct cmdq_command_buffer *cmd_buf, enum CMDQ_EVENT_ENUM event);
	s32 cmdq_op_set_event_ex(struct cmdqRecStruct *handle,
		struct cmdq_command_buffer *cmd_buf, enum CMDQ_EVENT_ENUM event);
	s32 cmdq_op_acquire_ex(struct cmdqRecStruct *handle,
		struct cmdq_command_buffer *cmd_buf, enum CMDQ_EVENT_ENUM event);
	s32 cmdq_op_write_from_reg_ex(struct cmdqRecStruct *handle,
		struct cmdq_command_buffer *cmd_buf, u32 write_reg, u32 from_reg);
	s32 cmdq_handle_flush_cmd_buf(struct cmdqRecStruct *handle,
		struct cmdq_command_buffer *cmd_buf);
	s32 cmdq_alloc_write_addr(u32 count, dma_addr_t *paStart,
		u32 clt, void *fp);
	s32 cmdq_free_write_addr(dma_addr_t paStart, u32 clt);
	s32 cmdq_free_write_addr_by_node(u32 clt, void *fp);
	s32 cmdq_mdp_handle_create(struct cmdqRecStruct **handle_out);
	s32 cmdq_mdp_handle_flush(struct cmdqRecStruct *handle);
	s32 cmdq_mdp_wait(struct cmdqRecStruct *handle, void *temp);
	s32 cmdq_mdp_handle_sec_setup(struct cmdqSecDataStruct *secData,
		struct cmdqRecStruct *handle);
	void cmdq_mdp_release_task_by_file_node(void *file_node);
	void cmdqCoreReadWriteAddressBatch(u32 *addrs, u32 count, u32 *val_out);
	s32 cmdq_mdp_update_sec_addr_index(struct cmdqRecStruct *handle,
		u32 sec_handle, u32 index, u32 instr_index);
	u32 cmdq_mdp_handle_get_instr_count(struct cmdqRecStruct *handle);
	void cmdq_mdp_meta_replace_sec_addr(struct op_meta *metas,
		struct mdp_submit *user_job, struct cmdqRecStruct *handle);
	void cmdq_mdp_op_readback(struct cmdqRecStruct *handle, u16 engine,
		dma_addr_t addr, u32 param);

#define CMDQ_CLT_MDP 0
#define CMDQ_MAX_USER_PROP_SIZE		(1024)
#define MDP_META_IN_LEGACY_V2
#define CMDQ_SYSTRACE_BEGIN(fmt, args...) do { \
} while (0)

#define CMDQ_SYSTRACE_END() do { \
} while (0)

/* tablet use */
/*
 * Set secure mode to prevent m4u translation fault
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   mode: secure mode type
 * Return:
 *	   0 for success; else the error code is returned
 * Note:
 *	mutex protected, be careful
 *	Remember to flush handle after this API to release resource via GCE
 */
#ifdef CONFIG_MTK_CMDQ_TAB
	int32_t cmdq_task_set_secure_mode(
		struct cmdqRecStruct *handle,
		enum CMDQ_DISP_MODE mode);
	int32_t cmdqRecSetSecureMode(struct cmdqRecStruct *handle,
		enum CMDQ_DISP_MODE mode);
#endif

#ifdef __cplusplus
}
#endif
#endif				/* __CMDQ_RECORD_H__ */
