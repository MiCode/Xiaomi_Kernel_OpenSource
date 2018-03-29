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

typedef struct cmdqUsageInfoStruct {
	uint32_t count[CMDQ_MAX_ENGINE_COUNT];	/* [OUT] current engine ref count */
} cmdqUsageInfoStruct;

typedef struct cmdqJobStruct {
	struct cmdqCommandStruct command;	/* [IN] the job to perform */
	cmdqJobHandle_t hJob;	/* [OUT] handle to resulting job */
} cmdqJobStruct;

typedef struct cmdqJobResultStruct {
	cmdqJobHandle_t hJob;	/* [IN]  Job handle from CMDQ_IOCTL_ASYNC_JOB_EXEC */
	uint64_t engineFlag;	/* [OUT] engine flag passed down originally */

	/* [IN/OUT] read register values, if any. */
	/* as input, the "count" field must represent */
	/* buffer space pointed by "regValues". */
	/* Upon return, CMDQ driver fills "count" with */
	/* actual requested register count. */
	/* However, if the input "count" is too small, */
	/* -ENOMEM is returned, and "count" is filled */
	/* with requested register count. */
	cmdqRegValueStruct regValue;

	cmdqReadAddressStruct readAddress;	/* [IN/OUT] physical address to read */
} cmdqJobResultStruct;

typedef struct cmdqWriteAddressStruct {
	/* [IN] count of the writable buffer (unit is # of uint32_t, NOT in byte) */
	uint32_t count;

	/* [OUT] When Alloc, this is the resulting PA. It is guaranteed to be continuous. */
	/* [IN]  When Free, please pass returned address down to ioctl. */
	/*  */
	/* indeed param startPA should be UNSIGNED LONG type for 64 bit kernel. */
	/* Considering our plartform supports max 4GB RAM(upper-32bit don't care for SW) */
	/* and consistent common code interface, remain uint32_t type. */
	uint32_t startPA;
} cmdqWriteAddressStruct;

#define CMDQ_IOCTL_MAGIC_NUMBER 'x'

#define CMDQ_IOCTL_LOCK_MUTEX   _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 1, int)
#define CMDQ_IOCTL_UNLOCK_MUTEX _IOR(CMDQ_IOCTL_MAGIC_NUMBER, 2, int)
#define CMDQ_IOCTL_EXEC_COMMAND _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 3, cmdqCommandStruct)
#define CMDQ_IOCTL_QUERY_USAGE  _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 4, cmdqUsageInfoStruct)

/*  */
/* Async operations */
/*  */
#define CMDQ_IOCTL_ASYNC_JOB_EXEC _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 5, cmdqJobStruct)
#define CMDQ_IOCTL_ASYNC_JOB_WAIT_AND_CLOSE _IOR(CMDQ_IOCTL_MAGIC_NUMBER, 6, cmdqJobResultStruct)

#define CMDQ_IOCTL_ALLOC_WRITE_ADDRESS _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 7, cmdqWriteAddressStruct)
#define CMDQ_IOCTL_FREE_WRITE_ADDRESS _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 8, cmdqWriteAddressStruct)
#define CMDQ_IOCTL_READ_ADDRESS_VALUE _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 9, cmdqReadAddressStruct)

/*  */
/* Chip capability query. output parameter is a bit field. */
/* Bit definition is CMDQ_CAP_BITS. */
/*  */
#define CMDQ_IOCTL_QUERY_CAP_BITS _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 10, int)

/*  */
/* HW info. from DTS */
/*  */
#define CMDQ_IOCTL_QUERY_DTS _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 11, cmdqDTSDataStruct)

/*  */
/* Notify MDP will use specified engine before really use. */
/* input int is same as EngineFlag. */
/*  */
#define CMDQ_IOCTL_NOTIFY_ENGINE _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 12, uint64_t)

#endif				/* __CMDQ_DRIVER_H__ */
