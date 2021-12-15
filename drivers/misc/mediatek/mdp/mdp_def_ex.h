/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef __MDP_DEF_EX_H__
#define __MDP_DEF_EX_H__

#include <linux/kernel.h>

#define MDP_LIMIT_DRIVER_DEVICE_NAME         "mtk_mdp_limit"

enum CMDQ_META_OP {
	CMDQ_MOP_WRITE = 0,
	CMDQ_MOP_READ,
	CMDQ_MOP_POLL,
	CMDQ_MOP_WAIT,
	CMDQ_MOP_WAIT_NO_CLEAR,
	CMDQ_MOP_CLEAR,
	CMDQ_MOP_SET,
	CMDQ_MOP_ACQUIRE,
	CMDQ_MOP_WRITE_FD,
	CMDQ_MOP_WRITE_FROM_REG,
	CMDQ_MOP_WRITE_SEC,
	CMDQ_MOP_READBACK,
	CMDQ_MOP_WRITE_RDMA,
	CMDQ_MOP_WRITE_FD_RDMA,
	CMDQ_MOP_NOP,
};

#define CMDQ_EVENT_WAIT 0x80008001
#define CMDQ_EVENT_WAIT_NO_CLEAR 0x00008001
#define CMDQ_EVENT_CLEAR 0x80000000
#define CMDQ_EVENT_SET 0x80010000
#define CMDQ_EVENT_ACQUIRE 0x80018000

struct op_meta {
	/* read/write/poll/wait/set */
	uint8_t op;
	/* id of hw */
	uint16_t engine;
	union {
		/* register offset */
		uint16_t offset;
		/* event id */
		uint16_t event;
		/* for rdma cpr */
		struct {
			uint8_t pipe_idx;
			uint8_t cpr_idx;
		};
	};
	union {
		/* value to write */
		uint32_t value;
		/* for read */
		uint32_t readback_id;
		/* write FD */
		uint32_t fd;
		/* write sec */
		uint32_t sec_handle;
		/* write from register */
		struct {
			uint16_t from_offset;
			uint16_t from_engine;
		};
	};
	union {
		/* for write with mask */
		uint32_t mask;
		/* Offset for fd */
		uint32_t fd_offset;
		/* index in cmdqSecDataStruct.addrMetadatas */
		uint32_t sec_index;
	};
};

struct hw_meta {
	/* register offset */
	uint16_t offset;
	/* id of hw */
	uint16_t engine;
};

struct mdp_submit {
	/* [IN] mdp operations */
	cmdqU32Ptr_t metas;
	uint32_t meta_count;
	/* [IN] task schedule priority. this is NOT HW thread priority. */
	uint32_t priority;
	/* [IN] bit flag of engines used. */
	uint64_t engine_flag;
	/* [IN] task property for pmqos */
	uint32_t prop_size;
	uint64_t prop_addr;
	/* [IN] readback extension, each bit for 1 readback feature */
	uint64_t readback_ext;
	/* [IN] ids to represent readback register */
	uint32_t read_count_v1;
	cmdqU32Ptr_t hw_metas_read_v1;
	/* [OUT] mdp job id in kernel */
	uint64_t job_id;
	/* [IN] secure execution data */
	struct cmdqSecDataStruct secData;
};

struct mdp_read_v1 {
	uint32_t count;
	/* [OUT] values output buffer */
	cmdqU32Ptr_t ret_values;
};

struct mdp_read_readback {
	uint32_t count;
	/* [IN] ids to represent readback ids */
	cmdqU32Ptr_t ids;
	/* [OUT] register values output buffer */
	cmdqU32Ptr_t ret_values;
};

struct mdp_wait {
	/* [IN] mdp job id in kernel */
	uint64_t job_id;

	/* [IN/OUT] mdp readback by input readback ids */
	struct mdp_read_v1 read_v1_result;

	/* [IN/OUT] mdp readback by input readback ids */
	struct mdp_read_readback read_result;
};

struct mdp_readback {
	/* [IN] count of the writable buffer
	 * (unit is # of u32, NOT in byte)
	 */
	uint32_t count;

	/* [OUT] When Alloc, this is the resulting PA.
	 * It is guaranteed to be continuous.
	 * [IN]  When Free, please pass returned address down to ioctl.
	 *
	 * indeed param startPA should be UNSIGNED LONG type for 64 bit kernel
	 * Considering our plartform supports max 4GB RAM
	 * (upper-32bit don't care for SW)
	 * and consistent common code interface, remain u32 type.
	 */
	uint32_t start_id;
};

struct mdp_simulate {
	cmdqU32Ptr_t metas;
	uint32_t meta_count;
	cmdqU32Ptr_t commands;
	uint32_t command_size;
	cmdqU32Ptr_t result_size;
};

#define CMDQ_IOCTL_MAGIC_NUMBER 'x'

#define CMDQ_IOCTL_ASYNC_EXEC _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 20, \
	struct mdp_submit)
#define CMDQ_IOCTL_ASYNC_WAIT _IOR(CMDQ_IOCTL_MAGIC_NUMBER, 21, \
	struct mdp_wait)

#define CMDQ_IOCTL_ALLOC_READBACK_SLOTS _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 22, \
	struct mdp_readback)
#define CMDQ_IOCTL_FREE_READBACK_SLOTS _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 23, \
	struct mdp_readback)
#define CMDQ_IOCTL_READ_READBACK_SLOTS _IOW(CMDQ_IOCTL_MAGIC_NUMBER, 24, \
	struct mdp_read_readback)

#define CMDQ_IOCTL_SIMULATE _IOR(CMDQ_IOCTL_MAGIC_NUMBER, 25, \
	struct mdp_simulate)

#endif	/* __MDP_DEF_EX_H__ */
