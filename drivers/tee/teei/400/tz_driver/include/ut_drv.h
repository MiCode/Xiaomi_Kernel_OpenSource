/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef _UT_DRV_H_
#define _UT_DRV_H_

#include <tee_client_api.h>

struct ut_drv_entry {
	struct TEEC_UUID uuid;
	struct TEEC_Session session;
	uint32_t driver_id;
	struct list_head list;
};

struct param_get_drv_id {
	unsigned long dummy;
};

struct param_reg_dci_buf {
	uint64_t phy_addr;
	uint32_t buf_size;
};

struct param_notify_from_ree {
	unsigned long data;
	uint32_t cmd;
};

struct param_drv_open {
	unsigned long data;
};

struct param_drv_ioctl {
	unsigned long data;
	uint32_t cmd;
};

struct param_drv_close {
	unsigned long dummy;
};

struct ut_drv_param {
	uint32_t cmd_id;
	union {
		struct param_get_drv_id get_drv_id;
		struct param_reg_dci_buf reg_dci_buf;
		struct param_notify_from_ree notify_from_ree;
		struct param_drv_open drv_open;
		struct param_drv_ioctl drv_ioctl;
		struct param_drv_close drv_close;
	} u;
};

enum {
	UT_DRV_GET_DRIVER_ID,
	UT_DRV_REGISTER_DCI_BUFFER,
	UT_DRV_FREE_DCI_BUFFER,
	UT_DRV_NOTIFY_FROM_REE,
	UT_DRV_OPEN,
	UT_DRV_IOCTL,
	UT_DRV_CLOSE,
};

static inline void prepare_params(struct TEEC_Operation *op,
					void *data, size_t size)
{
	op->params[0].tmpref.buffer = data;
	op->params[0].tmpref.size = size;
	op->params[1].value.a = 0;

	op->paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					TEEC_VALUE_OUTPUT,
					TEEC_NONE,
					TEEC_NONE);
}

static inline uint32_t get_result(struct TEEC_Operation *op)
{
	return op->params[1].value.a;
}

struct ut_drv_entry *find_ut_drv_entry_by_uuid(struct TEEC_UUID *uuid);
struct ut_drv_entry *find_ut_drv_entry_by_driver_id(uint32_t driver_id);

#endif
