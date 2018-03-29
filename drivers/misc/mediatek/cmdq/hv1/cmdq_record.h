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
#include "cmdq_def.h"
#include "cmdq_core.h"

struct TaskStruct;

struct cmdqRecStruct {
	uint64_t engineFlag;
	int32_t scenario;
	uint32_t blockSize;	/* command size */
	void *pBuffer;
	uint32_t bufferSize;	/* allocated buffer size */
	struct TaskStruct *pRunningTask;	/* running task after flush() or startLoop() */
	enum CMDQ_HW_THREAD_PRIORITY_ENUM priority;	/* setting high priority. This implies Prefetch ENABLE. */
	bool finalized;		/* set to true after flush() or startLoop() */
	uint32_t prefetchCount;	/* maintenance prefetch instruction */
	cmdqSecDataStruct secData;	/* secure execution data */
};
typedef struct cmdqRecStruct *cmdqRecHandle;
typedef dma_addr_t cmdqBackupSlotHandle;

typedef void *CmdqRecLoopHandle;

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
	int32_t cmdqRecCreate(enum CMDQ_SCENARIO_ENUM scenario, cmdqRecHandle *pHandle);

/**
 * Reset command queue recorder commands
 * Parameter:
 *    handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecReset(cmdqRecHandle handle);

/**
 * Configure as secure task
 * Parameter:
 *     handle: the command queue recorder handle
 *     isSecure: true, execute the command in secure world
 * Return:
 *     0 for success; else the error code is returned
 *
 * Note:
 *     a. Secure CMDQ support when t-base enabled only
 *     b. By default cmdqRecHandle records a normal command,
 *		  please call cmdqRecSetSecure to set command as SECURE after cmdqRecReset
 */
	int32_t cmdqRecSetSecure(cmdqRecHandle handle, const bool isSecure);

/**
 * Add DPAC protection flag
 * Parameter:
 * Note:
 *     a. Secure CMDQ support when t-base enabled only
 *     b. after reset handle, user have to specify protection flag again
 */
	int32_t cmdqRecSecureEnableDAPC(cmdqRecHandle handle, const uint64_t engineFlag);

/**
 * Add flag for M4U security ports
 * Parameter:
 * Note:
 *	   a. Secure CMDQ support when t-base enabled only
 *	   b. after reset handle, user have to specify protection flag again
 */
	int32_t cmdqRecSecureEnablePortSecurity(cmdqRecHandle handle, const uint64_t engineFlag);

/**
 * Append mark command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecMark(cmdqRecHandle handle);

/**
 * Append mark command to enable prefetch
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecEnablePrefetch(cmdqRecHandle handle);

/**
 * Append mark command to disable prefetch
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecDisablePrefetch(cmdqRecHandle handle);

/**
 * Append write command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 *     addr: the specified target register physical address
 *     value: the specified target register value
 *     mask: the specified target register mask
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecWrite(cmdqRecHandle handle, uint32_t addr, uint32_t value, uint32_t mask);

/**
 * Append write command to the update secure buffer address in secure path
 * Parameter:
 *	   handle: the command queue recorder handle
 *	   addr: the specified register physical address about module src/dst buffer address
 *	   type: base handle type
 *     base handle: secure handle of a secure mememory
 *     offset: offset related to base handle (secure buffer = addr(base_handle) + offset)
 *     size: secure buffer size
 *	   mask: 0xFFFF_FFFF
 * Return:
 *	   0 for success; else the error code is returned
 * Note:
 *     support only when secure OS enabled
 */

	int32_t cmdqRecWriteSecure(cmdqRecHandle handle,
				   uint32_t addr,
				   CMDQ_SEC_ADDR_METADATA_TYPE type,
				   uint32_t baseHandle,
				   uint32_t offset, uint32_t size, uint32_t port);

/**
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

	int32_t cmdqRecWriteSecureMask(cmdqRecHandle handle, uint32_t addr,
				       CMDQ_SEC_ADDR_METADATA_TYPE type,
				       uint32_t value, uint32_t mask);

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
	int32_t cmdqRecPoll(cmdqRecHandle handle, uint32_t addr, uint32_t value, uint32_t mask);

/**
 * Append wait command to the recorder
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type to "wait and CLEAR"
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecWait(cmdqRecHandle handle, enum CMDQ_EVENT_ENUM event);

/**
 * like cmdqRecWait, but won't clear the event after
 * leaving wait state.
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type wait for
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecWaitNoClear(cmdqRecHandle handle, enum CMDQ_EVENT_ENUM event);

/**
 * Unconditionally set to given event to 0.
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type to set
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecClearEventToken(cmdqRecHandle handle, enum CMDQ_EVENT_ENUM event);

/**
 * Unconditionally set to given event to 1.
 * Parameter:
 *     handle: the command queue recorder handle
 *     event: the desired event type to set
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecSetEventToken(cmdqRecHandle handle, enum CMDQ_EVENT_ENUM event);
/**
 * Read a register value to a CMDQ general purpose register(GPR)
 * Parameter:
 *     handle: the command queue recorder handle
 *     hwRegAddr: register address to read from
 *     dstDataReg: CMDQ GPR to write to
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecReadToDataRegister(cmdqRecHandle handle, uint32_t hwRegAddr,
					  enum CMDQ_DATA_REGISTER_ENUM dstDataReg);
/**
 * Write a register value from a CMDQ general purpose register(GPR)
 * Parameter:
 *     handle: the command queue recorder handle
 *     srcDataReg: CMDQ GPR to read from
 *     hwRegAddr: register address to write to
 * Return:
 *     0 for success; else the error code is returned
 */
	int32_t cmdqRecWriteFromDataRegister(cmdqRecHandle handle,
					     enum CMDQ_DATA_REGISTER_ENUM srcDataReg,
					     uint32_t hwRegAddr);


/**
 *  Allocate 32-bit register backup slot
 *
 */
	int32_t cmdqBackupAllocateSlot(cmdqBackupSlotHandle *phBackupSlot, uint32_t slotCount);

/**
 *  Read 32-bit register backup slot by index
 *
 */
	int32_t cmdqBackupReadSlot(cmdqBackupSlotHandle hBackupSlot, uint32_t slotIndex,
				   uint32_t *value);

/**
 *  Use CPU to write value into 32-bit register backup slot by index directly.
 *
 */
	int32_t cmdqBackupWriteSlot(cmdqBackupSlotHandle hBackupSlot, uint32_t slotIndex,
				    uint32_t value);


/**
 *  Free allocated backup slot. DO NOT free them before corresponding
 *  task finishes. Becareful on AsyncFlush use cases.
 *
 */
	int32_t cmdqBackupFreeSlot(cmdqBackupSlotHandle hBackupSlot);


/**
 *  Insert instructions to backup given 32-bit HW register
 *  to a backup slot.
 *  You can use cmdqBackupReadSlot() to retrieve the result
 *  AFTER cmdqRecFlush() returns, or INSIDE the callback of cmdqRecFlushAsyncCallback().
 *
 */
	int32_t cmdqRecBackupRegisterToSlot(cmdqRecHandle handle,
					    cmdqBackupSlotHandle hBackupSlot,
					    uint32_t slotIndex, uint32_t addr);
/**
 *  Insert instructions to write 32-bit HW register
 *  from a backup slot.
 *  You can use cmdqBackupReadSlot() to retrieve the result
 *  AFTER cmdqRecFlush() returns, or INSIDE the callback of cmdqRecFlushAsyncCallback().
 *
 */
	int32_t cmdqRecBackupWriteRegisterFromSlot(cmdqRecHandle handle,
						   cmdqBackupSlotHandle hBackupSlot,
						   uint32_t slotIndex, uint32_t addr);

/**
 *  Insert instructions to update slot with given 32-bit value
 *  You can use cmdqBackupReadSlot() to retrieve the result
 *  AFTER cmdqRecFlush() returns, or INSIDE the callback of cmdqRecFlushAsyncCallback().
 *
 */
	int32_t cmdqRecBackupUpdateSlot(cmdqRecHandle handle,
					cmdqBackupSlotHandle hBackupSlot,
					uint32_t slotIndex, uint32_t value);

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
	int32_t cmdqRecFlush(cmdqRecHandle handle);


/**
 *  Flush the command; Also at the end of the command, backup registers
 *  appointed by addrArray.
 *
 */
	int32_t cmdqRecFlushAndReadRegister(cmdqRecHandle handle, uint32_t regCount,
					    uint32_t *addrArray, uint32_t *valueArray);

/**
 * Trigger CMDQ to asynchronously execute the recorded commands
 * Parameter:
 *     handle: the command queue recorder handle
 * Return:
 *     0 for successfully start execution; else the error code is returned
 * Note:
 *     This is an ASYNC function. When the function
 *     returned, it may or may not be finished. There is no way to retrieve the result.
 */
	int32_t cmdqRecFlushAsync(cmdqRecHandle handle);

	int32_t cmdqRecFlushAsyncCallback(cmdqRecHandle handle, CmdqAsyncFlushCB callback,
					  uint32_t userData);

/**
 * Trigger CMDQ to execute the recorded commands in loop.
 * each loop completion generates callback in interrupt context.
 *
 * Parameter:
 *     handle: the command queue recorder handle
 *     irqCallback: this CmdqInterruptCB callback is called after each loop completion.
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
	int32_t cmdqRecStartLoop(cmdqRecHandle handle);

/**
 * Unconditionally stops the loop thread.
 * Must call after cmdqRecStartLoop().
 */
	int32_t cmdqRecStopLoop(cmdqRecHandle handle);

/**
 * returns current count of instructions in given handle
 */
	int32_t cmdqRecGetInstructionCount(cmdqRecHandle handle);

/**
 * Dump command buffer to kernel log
 * This is for debugging purpose.
 */
	int32_t cmdqRecDumpCommand(cmdqRecHandle handle);

/**
 * Destroy command queue recorder handle
 * Parameter:
 *     handle: the command queue recorder handle
 */
	void cmdqRecDestroy(cmdqRecHandle handle);

	int32_t cmdqRecSetSecureMode(cmdqRecHandle handle, enum CMDQ_DISP_MODE mode);

	int32_t cmdqRecWaitThreadIdleWithTimeout(int threadID, unsigned int retryCount);

#ifdef __cplusplus
}
#endif
#endif				/* __CMDQ_RECORD_H__ */
