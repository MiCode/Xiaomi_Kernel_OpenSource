/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef __TEEI_CLIENT_H_
#define __TEEI_CLIENT_H_

#include <tee_client_api.h>
#include <teei_ioc.h>
#define TEEI_CLIENT_FULL_PATH_DEV_NAME "/dev/teei_client"
#define TEEI_CLIENT_DEV "teei_client"


/** IOCTL request */


/**
 * @brief Encode command structure
 */
struct teei_client_encode_cmd {
	unsigned int len;
	unsigned long long data;
	int   offset;
	int   flags;
	int   param_type;

	int encode_id;
	int session_id;
	unsigned int cmd_id;
	int value_flag;
	int param_pos;
	int param_pos_type;
	int return_value;
	int return_origin;
};

/**
 * @brief Session details structure
 */
struct ser_ses_id {
	/* int service_id; */
	int session_id;
	struct TEEC_UUID uuid;
	int return_value;
	int return_origin;
	int paramtype;
	int params[8];
};
/**
 * @brief Session details structure
 */
struct user_ses_init {
	int session_id;
};

struct ctx_data {
	char name[255]; /* context name */
	int ctx_ret; /* context return */
};

/**
 * @brief Shared memory information for the session
 */
struct teei_session_shared_mem_info {
	/* unsigned long service_id; */
	unsigned int session_id;
	u64 user_mem_addr;
};

/**
 * @brief Shared memory used for smc processing
 */
struct teei_smc_cdata {
	int cmd_addr;
	int size;
	int valid_flag;
	int ret_val;
};

/* For general service */
#define TEEI_CLIENT_IOCTL_SEND_CMD_REQ \
	_IOWR(TEEI_CLIENT_IOC_MAGIC, 3, struct teei_client_encode_cmd)
#define TEEI_CLIENT_IOCTL_SES_OPEN_REQ \
	_IOW(TEEI_CLIENT_IOC_MAGIC, 4, struct ser_ses_id)
#define TEEI_CLIENT_IOCTL_SES_CLOSE_REQ \
	_IOWR(TEEI_CLIENT_IOC_MAGIC, 5, struct ser_ses_id)
#define TEEI_CLIENT_IOCTL_SHR_MEM_FREE_REQ \
	_IOWR(TEEI_CLIENT_IOC_MAGIC, 6, struct teei_session_shared_mem_info)

#define TEEI_CLIENT_IOCTL_ENC_UINT32 \
	_IOWR(TEEI_CLIENT_IOC_MAGIC, 7, struct teei_client_encode_cmd)
#define TEEI_CLIENT_IOCTL_ENC_ARRAY \
	_IOWR(TEEI_CLIENT_IOC_MAGIC, 8, struct teei_client_encode_cmd)
#define TEEI_CLIENT_IOCTL_ENC_ARRAY_SPACE \
	_IOWR(TEEI_CLIENT_IOC_MAGIC, 9, struct teei_client_encode_cmd)
#define TEEI_CLIENT_IOCTL_ENC_MEM_REF \
	_IOWR(TEEI_CLIENT_IOC_MAGIC, 10, struct teei_client_encode_cmd)

#define TEEI_CLIENT_IOCTL_DEC_UINT32 \
	_IOWR(TEEI_CLIENT_IOC_MAGIC, 11, struct teei_client_encode_cmd)
#define TEEI_CLIENT_IOCTL_DEC_ARRAY_SPACE \
	_IOWR(TEEI_CLIENT_IOC_MAGIC, 12, struct teei_client_encode_cmd)
#define TEEI_CLIENT_IOCTL_OPERATION_RELEASE \
	_IOWR(TEEI_CLIENT_IOC_MAGIC, 13, struct teei_client_encode_cmd)
#define TEEI_CLIENT_IOCTL_SHR_MEM_ALLOCATE_REQ \
	_IOWR(TEEI_CLIENT_IOC_MAGIC, 14, struct teei_session_shared_mem_info)
#define TEEI_CLIENT_IOCTL_GET_DECODE_TYPE \
	_IOWR(TEEI_CLIENT_IOC_MAGIC, 15, struct teei_client_encode_cmd)
/* add by lodovico */
#define TEEI_CLIENT_IOCTL_INITCONTEXT_REQ \
	_IOW(TEEI_CLIENT_IOC_MAGIC, 16, struct ctx_data)
#define TEEI_CLIENT_IOCTL_CLOSECONTEXT_REQ \
	_IOW(TEEI_CLIENT_IOC_MAGIC, 17, struct ctx_data)
/* add end */
#define TEEI_CLIENT_IOCTL_SES_INIT_REQ \
	_IOW(TEEI_CLIENT_IOC_MAGIC, 18, struct user_ses_init)

#define TEEI_GET_TEEI_CONFIG_STAT \
	_IOW(TEEI_CLIENT_IOC_MAGIC, 0x1001, struct ctx_data)

#define TEEI_CANCEL_COMMAND \
	_IOW(TEEI_CLIENT_IOC_MAGIC, 0x1002, struct ctx_data)
#endif /* __TEEI_CLIENT_H_ */
