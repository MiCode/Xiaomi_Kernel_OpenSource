/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __TEEI_COMMON_H_
#define __TEEI_COMMON_H_

#include <linux/types.h>
#include <notify_queue.h>
#include <mt-plat/met_drv.h>

#define TEEI_MAX_REQ_PARAMS  12
#define TEEI_MAX_RES_PARAMS  8
#define TEEI_1K_SIZE 1024

/**
 * @brief Command status
 */
enum teei_cmd_status {
	TEEI_STATUS_INCOMPLETE = 0,
	TEEI_STATUS_COMPLETE,
	TEEI_STATUS_MAX  = 0x7FFFFFFF
};


/**
 * @brief Parameters type
 */
enum teeic_param_type {
	TEEIC_PARAM_IN = 0,
	TEEIC_PARAM_OUT
};

/**
 * @brief Shared memory for Notification
 */
struct teeic_notify_data {
	int dev_file_id;
	int service_id;
	int client_pid;
	int session_id;
	int enc_id;
};


enum teeic_param_value {
	TEEIC_PARAM_A = 0,
	TEEIC_PARAM_B
};

enum teeic_param_pos {
	TEEIC_PARAM_1ST = 0,
	TEEIC_PARAM_2ND,
	TEEIC_PARAM_3TD,
	TEEIC_PARAM_4TH
};


/**
 * @brief Metadata used for encoding/decoding
 */
struct teei_encode_meta {
	int type;
	int len;			/* data length */
	unsigned long long usr_addr;		/* data address in user space */
	int ret_len;			/* return sizeof data */
	int value_flag;			/* value of a or b */
	int param_pos;			/* param order */
	int param_pos_type;		/* param type */
};

/**
 * @brief SMC command structure
 */
#pragma pack(1)
struct teei_smc_cmd {
	u32 teei_cmd_type;
	u32 id;
	u32 context;
	u32 enc_id;

	u32 src_id;
	u32 src_context;

	u32 req_buf_len;
	u32 resp_buf_len;
	u32 ret_resp_buf_len;
	u32 info_buf_len;
	u32 cmd_status;
	u64 req_buf_phys;
	u64 resp_buf_phys;
	u64 meta_data_phys;
	u64 info_buf_phys;
	u32 dev_file_id;
	u32 error_code;
	u64 teei_sema;
};
#pragma pack()

struct message_head {
	unsigned int invalid_flag;
	unsigned int message_type;
	unsigned int child_type;
	unsigned int param_length;
};

struct fdrv_message_head {
	unsigned int driver_type;
	unsigned int fdrv_param_length;
};

#pragma pack(1)
struct create_fdrv_struct {
	u32 fdrv_type;
	u64 fdrv_phy_addr;
	u32 fdrv_size;
};
#pragma pack()

struct ack_fast_call_struct {
	int retVal;
};

struct fdrv_call_struct {
	int fdrv_call_type;
	int fdrv_call_buff_size;
	int retVal;
};

struct service_handler {
	unsigned int sysno;
	void *param_buf;
	unsigned int size;
	long (*init)(struct service_handler *handler);
	void (*deinit)(struct service_handler *handler);
	int (*handle)(struct NQ_entry *handler);
};

/* [KTRACE] Begin-End */
#ifdef CONFIG_MICROTRUST_TZ_DRIVER_MTK_TRACING
#define KATRACE_MESSAGE_LENGTH	1024
#define BEGINED_PID		(current->tgid)

static inline void tracing_mark_write(const char *buf)
{
	TRACE_PUTS(buf);
}

#define WRITE_MSG(format, pid, name) { \
	char buf[KATRACE_MESSAGE_LENGTH]; \
	int len = snprintf(buf, sizeof(buf), format, pid, name); \
	if (len >= (int) sizeof(buf)) { \
		int name_len = strlen(name) - (len - sizeof(buf)) - 1; \
		len = snprintf(buf, sizeof(buf), "B|%d|%.*s", BEGINED_PID, \
				name_len, name); \
	} \
	tracing_mark_write(buf); \
}

#define KATRACE_BEGIN(name)	katrace_begin_body(name)
static inline void katrace_begin_body(const char *name)
{
	WRITE_MSG("B|%d|%s", BEGINED_PID, name);
}

#define KATRACE_END(name)	katrace_end(name)
static inline void katrace_end(const char *name)
{
	WRITE_MSG("E|%d|%s", BEGINED_PID, name);
}
#else
#define KATRACE_BEGIN(name)	\
	do {			\
	} while (0)
#define KATRACE_END(name)	\
	do {			\
	} while (0)
#endif

#endif /* __TEEI_COMMON_H_ */
