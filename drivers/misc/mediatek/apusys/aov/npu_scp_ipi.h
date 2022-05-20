/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __NPU_SCP_IPI_H__
#define __NPU_SCP_IPI_H__

#include <linux/types.h>

enum NPU_SCP_IPI_CMD {
	NPU_SCP_RESPONSE = 1,
	NPU_SCP_STATE_CHANGE,
	NPU_SCP_TEST,
	NPU_SCP_SYSTEM,
	NPU_SCP_NP_MDW,
	NPU_SCP_RECOVERY,
};

enum NPU_SCP_STATE_CHANGE_ACT {
	NPU_SCP_STATE_CHANGE_TO_SUSPEND = 1,
	NPU_SCP_STATE_CHANGE_TO_RESUME,
};

enum NPU_SCP_SYSTEM_ACT {
	NPU_SCP_SYSTEM_GET_VERSION = 1,
	NPU_SCP_SYSTEM_FUNCTION_ENABLE,
	NPU_SCP_SYSTEM_FUNCTION_DISABLE,
	NPU_SCP_SYSTEM_FORCE_TO_SUSPEND,
	NPU_SCP_SYSTEM_FORCE_TO_RESUME,
};

enum NPU_SCP_TEST_ACT {
	NPU_SCP_TEST_START = 1,
	NPU_SCP_TEST_STOP,
};

enum NPU_SCP_NP_MDW_ACT {
	NPU_SCP_NP_MDW_ACK,
	NPU_SCP_NP_MDW_TO_APMCU,
	NPU_SCP_NP_MDW_TO_SCP,
};

enum NPU_SCP_RECOVERY_ACT {
	NPU_SCP_RECOVERY_ACK,
	NPU_SCP_RECOVERY_TO_APMCU,
	NPU_SCP_RECOVERY_TO_SCP,
};

#define SCP_IPI_TIMEOUT_MS (10)
#define TESTCASE_TIMEOUT_MS (10000)

#define NPU_SCP_RET_OK			(0)
#define NPU_SCP_RET_TEST_START_ERR	(-1)
#define NPU_SCP_RET_TEST_STOP_ERR	(-2)
#define NPU_SCP_RET_TEST_ERROR		(-3)
#define NPU_SCP_RET_TEST_ABORTED	(-4)
#define NPU_SCP_RET_TEST_EMPTY		(-5)

struct npu_scp_ipi_param {
	uint32_t cmd;
	uint32_t act;
	uint32_t arg;
	uint32_t ret;
};

int npu_scp_ipi_send(struct npu_scp_ipi_param *send_msg, struct npu_scp_ipi_param *recv_msg,
		     uint32_t timeout_ms);

#endif // __NPU_SCP_IPI_H__
