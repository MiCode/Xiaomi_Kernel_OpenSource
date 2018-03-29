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

#ifndef __CMDQ_DRIVER_H__
#define __CMDQ_DRIVER_H__

#include <linux/kernel.h>
#include "cmdq_def.h"

struct cmdqUsageInfoStruct {
	uint32_t count[CMDQ_MAX_ENGINE_COUNT];	/* [OUT] current engine ref count */
};

struct cmdqJobStruct {
	struct cmdqCommandStruct command;	/* [IN] the job to perform */
	cmdqJobHandle_t hJob;	/* [OUT] handle to resulting job */
};

struct cmdqJobResultStruct {
	cmdqJobHandle_t hJob;	/* [IN]  Job handle from CMDQ_IOCTL_ASYNC_JOB_EXEC */
	uint64_t engineFlag;	/* [OUT] engine flag passed down originally */
	struct cmdqRegValueStruct regValue;	/* [IN/OUT] read register values, if any. */
	/*as input, the "count" field must represent */
	/*buffer space pointed by "regValues". */
	/*Upon return, CMDQ driver fills "count" with */
	/*actual requested register count. */
	/*However, if the input "count" is too small, */
									/*-ENOMEM is returned, and "count" is filled*/
	/*with requested register count. */
	struct cmdqReadAddressStruct readAddress;	/* [IN/OUT] physical address to read */
};

struct cmdqWriteAddressStruct {
	uint32_t count;		/* [IN] count of the writable buffer (unit is # of uint32_t, NOT in byte) */
	/* [OUT] When Alloc, this is the resulting PA. It is guaranteed to be continuous. */
	/* [IN]  When Free, please pass returned address down to ioctl. */
	/*  */
	/* indeed param startPA should be UNSIGNED LONG type for 64 bit kernel. */
	/* Considering our plartform supports max 4GB RAM(upper-32bit don't care for SW) */
	/* and consistent common code interface, remain uint32_t type. */
	uint32_t startPA;	/* [OUT] When Alloc, this is the resulting PA. It is guaranteed to be continuous. */
	/* [IN]  When Free, please pass returned address down to ioctl. */
};

#define CMDQ_IOCTL_MAGIC_NUMBER 'x'

#define CMDQ_IOCTL_LOCK_MUTEX   _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 1, int)
#define CMDQ_IOCTL_UNLOCK_MUTEX _IOR(CMDQ_IOCTL_MAGIC_NUMBER, 2, int)
#define CMDQ_IOCTL_EXEC_COMMAND _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 3, struct cmdqCommandStruct)
#define CMDQ_IOCTL_QUERY_USAGE  _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 4, struct cmdqUsageInfoStruct)

/*
 Async operations
*/
#define CMDQ_IOCTL_ASYNC_JOB_EXEC _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 5, struct cmdqJobStruct)
#define CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE _IOR(CMDQ_IOCTL_MAGIC_NUMBER, 6, struct cmdqJobResultStruct)

#define CMDQ_IOCTL_ALLOC_WRITE_ADDRESS _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 7, struct cmdqWriteAddressStruct)
#define CMDQ_IOCTL_FREE_WRITE_ADDRESS _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 8, struct cmdqWriteAddressStruct)
#define CMDQ_IOCTL_READ_ADDRESS_VALUE _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 9, struct cmdqReadAddressStruct)

/*
 Chip capability query. output parameter is a bit field.
 Bit definition is enum CMDQ_CAP_BITS.
*/
#define CMDQ_IOCTL_QUERY_CAP_BITS _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 10, int)

/*
copy HDCP version from src handle to dst handle
*/
#define CMDQ_IOCTL_SYNC_BUF_HDCP_VERSION _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 11, struct cmdqSyncHandleHdcpStruct)

enum IOCTL_RETURN_CODE {
	IOCTL_RET_SUCCESS										=	0,
	/* for ioctl compat */
	IOCTL_RET_DEPRECATE_LOCK_MUTEX							=	1,
	IOCTL_RET_DEPRECATE_UNLOCK_MUTEX						=	2,
	IOCTL_RET_UNRECOGNIZED_COMPAT_IOCTL						=	3,
	IOCTL_RET_CONFIG_COMPAT_NOT_OPEN						=	4,
	/* for ioctl */
	IOCTL_RET_LOCK_MUTEX_FAIL								=	5,
	IOCTL_RET_COPY_MUTEX_NUM_TO_USER_FAIL					=	6,
	IOCLT_RET_COPY_MUTEX_NUM_FROM_USER_FAIL					=	7,
	IOCTL_RET_RELEASE_MUTEX_FAIL							=	8,
	IOCTL_RET_COPY_EXEC_CMD_FROM_USER_FAIL					=	9,
	IOCTL_RET_IS_SUSPEND_WHEN_EXEC_CMD						=	10,
	IOCTL_RET_PROCESS_CMD_REQUEST_FAIL						=	11,
	IOCLT_RET_QUERY_USAGE_FAIL								=	12,
	IOCTL_RET_COPY_USAGE_TO_USER_FAIL						=	13,
	IOCTL_RET_COPY_ASYNC_JOB_EXEC_FROM_USER_FAIL			=	14,
	IOCTL_RET_IS_SUSPEND_WHEN_ASYNC_JOB_EXEC				=	15,
	IOCTL_RET_NOT_SUPPORT_SEC_PATH_FOR_ASYNC_JOB_EXEC		=	16,
	IOCTL_RET_CREATE_REG_ADDR_BUF_FAIL						=	17,
	IOCLT_RET_COPY_ASYNC_JOB_EXEC_TO_USER_FAIL				=	18,
	IOCTL_RET_SUBMIT_TASK_ASYNC_FAILED						=	19,
	IOCTL_RET_COPY_ASYNC_JOB_WAIT_AND_CLOSE_FROM_USER_FAIL	=	20,
	IOCTL_RET_IS_SUSPEND_WHEN_ASYNC_JOB_WAIT_AND_CLOSE		=	21,
	IOCTL_RET_INVALID_TASK_PTR								=	22,
	IOCTL_RET_COPY_JOB_RESULT_TO_USER1_FAIL					=	23,
	IOCTL_RET_NOT_ENOUGH_REGISTER_BUFFER					=	24,
	IOCTL_RET_COPY_JOB_RESULT_TO_USER2_FAIL					=	25,
	IOCTL_RET_NO_REG_VAL_BUFFER								=	26,
	IOCTL_RET_WAIT_RESULT_AND_RELEASE_TASK_FAIL				=	27,
	IOCLT_RET_COPY_REG_VALUE_TO_USER_FAIL					=	28,
	IOCTL_RET_COPY_ALLOC_WRITE_ADDR_FROM_USER_FAIL			=	29,
	IOCTL_RET_ALLOC_WRITE_ADDR_FAIL							=	30,
	IOCTL_RET_COPY_ALLOC_WRITE_ADDR_TO_USER_FAIL			=	31,
	IOCTL_RET_COPY_FREE_WRITE_ADDR_FROM_USER_FAIL			=	32,
	IOCTL_RET_FREE_WRITE_ADDR_FAIL							=	33,
	IOCTL_RET_COPY_READ_ADDR_VAL_FROM_USER_FAIL				=	34,
	IOCTL_RET_COPY_CAP_BITS_TO_USER_FAIL					=	35,
	IOCTL_RET_COPY_HDCP_VERSION_FROM_USER_FAIL				=	36,
	IOCTL_RET_SVP_NOT_SUPPORT								=	37,
	IOCTL_RET_UNRECOGNIZED_IOCTL							=	38,

};

#endif				/* __CMDQ_DRIVER_H__ */
