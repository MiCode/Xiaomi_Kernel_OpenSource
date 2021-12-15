/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#ifndef TZ_MOD_H
#define TZ_MOD_H


/*****************************************************************************
 * MODULE DEFINITION
 *****************************************************************************/
#define MODULE_NAME	    "[MTEE_MOD]"
#define TZ_DEV_NAME        "trustzone"
#define MAJOR_DEV_NUM   197

/*****************************************************************************
 * IOCTL DEFINITION
 *****************************************************************************/
#define MTEE_IOC_MAGIC       'T'
#define MTEE_CMD_OPEN_SESSION \
		_IOWR(MTEE_IOC_MAGIC,  1, struct kree_session_cmd_param)
#define MTEE_CMD_CLOSE_SESSION \
		_IOWR(MTEE_IOC_MAGIC,  2, struct kree_session_cmd_param)
#define MTEE_CMD_TEE_SERVICE \
		_IOWR(MTEE_IOC_MAGIC,  3, struct kree_tee_service_cmd_param)
#define MTEE_CMD_SHM_REG \
		_IOWR(MTEE_IOC_MAGIC,  4, struct kree_tee_service_cmd_param)
#define MTEE_CMD_SHM_UNREG \
		_IOWR(MTEE_IOC_MAGIC,  5, struct kree_tee_service_cmd_param)
#define MTEE_CMD_SHM_REG_WITH_TAG \
		_IOWR(MTEE_IOC_MAGIC,  6, struct kree_tee_service_cmd_param)
#define MTEE_CMD_OPEN_SESSION_WITH_TAG \
		_IOWR(MTEE_IOC_MAGIC,  7, struct kree_session_tag_cmd_param)


#define DEV_IOC_MAXNR       (10)

/* param for open/close session */
struct kree_session_cmd_param {
	int32_t ret;
	int32_t handle;
	uint64_t data;
};

/* param for open/close session with tag information */
struct kree_session_tag_cmd_param {
	int32_t ret;
	int32_t handle;
	uint64_t data;
	uint64_t tag;
	uint32_t tag_size;
};

/* param for tee service call */
struct kree_tee_service_cmd_param {
	int32_t ret;
	int32_t handle;
	uint32_t command;
	uint32_t paramTypes;
	uint64_t param;
};

/* param for shared memory */
struct kree_sharedmemory_cmd_param {
	int32_t ret;
	uint32_t session;
	uint32_t mem_handle;
	uint32_t command;
	uint64_t buffer;
	uint32_t size;
	uint32_t control;	/* 0 = write, 1 = read only */
};

/* param for shared memory with tag information */
struct kree_sharedmemory_tag_cmd_param {
	int32_t ret;
	uint32_t session;
	uint32_t mem_handle;
	uint32_t command;
	uint64_t buffer;
	uint32_t size;
	uint32_t control;	/* 0 = write, 1 = read only */
	uint64_t tag;
	uint32_t tag_size;
};

#endif				/* end of DEVFINO_H */
