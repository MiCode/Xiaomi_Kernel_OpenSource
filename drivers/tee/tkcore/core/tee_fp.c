// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2015-2019 TrustKernel Incorporated
 */

#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/errno.h>

#include <linux/tee_fp.h>

#include <tee_kernel_api.h>

#include "tee_fp_priv.h"

static struct TEEC_UUID SENSOR_DETECTOR_TA_UUID = { 0x966d3f7c, 0x04ef, 0x1beb,
	{ 0x08, 0xb7, 0x57, 0xf3, 0x7a, 0x6d, 0x87, 0xf9 } };

#define CMD_READ_CHIPID		0x0
#define CMD_DISABLE			0x1
#define CMD_CONFIG_PADSEL	0x2

int tee_spi_cfg_padsel(uint32_t padsel)
{
	struct TEEC_Context context;
	struct TEEC_Session session;
	struct TEEC_Operation op;

	TEEC_Result r;

	uint32_t returnOrigin;

	pr_info("padsel=0x%x\n", padsel);

	memset(&context, 0, sizeof(context));
	memset(&session, 0, sizeof(session));
	memset(&op, 0, sizeof(op));

	r = TEEC_InitializeContext(NULL, &context);
	if (r != TEEC_SUCCESS) {
		pr_err(
			"TEEC_InitializeContext() failed with 0x%08x\n", r);
		return r;
	}

	r = TEEC_OpenSession(
		&context, &session, &SENSOR_DETECTOR_TA_UUID,
		TEEC_LOGIN_PUBLIC,
		NULL, NULL, &returnOrigin);

	if (r != TEEC_SUCCESS) {
		pr_err(
			"TEEC_OpenSession failed with 0x%x returnOrigun: %u\n",
			r, returnOrigin);
		TEEC_FinalizeContext(&context);
		return r;
	}

	op.paramTypes = TEEC_PARAM_TYPES(
		TEEC_VALUE_INPUT,
		TEEC_NONE,
		TEEC_NONE,
		TEEC_NONE);

	op.params[0].value.a = padsel;

	r = TEEC_InvokeCommand(&session, CMD_CONFIG_PADSEL, &op, &returnOrigin);
	if (r != TEEC_SUCCESS) {
		pr_err(
			"TEEC_InvokeCommand() failed with 0x%08x returnOrigin: %u\n",
			r, returnOrigin);
	}

	TEEC_CloseSession(&session);
	TEEC_FinalizeContext(&context);

	return r;
}
EXPORT_SYMBOL(tee_spi_cfg_padsel);

int tee_spi_transfer(void *conf, uint32_t conf_size,
	void *inbuf, void *outbuf, uint32_t size)
{
	struct TEEC_Context context;
	struct TEEC_Session session;
	struct TEEC_Operation op;

	TEEC_Result r;

	char *buf;
	uint32_t returnOrigin;

	pr_info("conf=%p conf_size=%u inbuf=%p outbuf=%p size=%u\n",
		conf, conf_size, inbuf, outbuf, size);

	if (!conf || !inbuf || !outbuf) {
		pr_err("Bad parameters NULL buf\n");
		return -EINVAL;
	}

	if (size == 0) {
		pr_err("zero buf size\n");
		return -EINVAL;
	}

	memset(&context, 0, sizeof(context));
	memset(&session, 0, sizeof(session));
	memset(&op, 0, sizeof(op));

	memcpy(outbuf, inbuf, size);

	r = TEEC_InitializeContext(NULL, &context);
	if (r != TEEC_SUCCESS) {
		pr_err(
			"TEEC_InitializeContext() failed with 0x%08x\n", r);
		return r;
	}

	r = TEEC_OpenSession(
		&context, &session, &SENSOR_DETECTOR_TA_UUID,
		TEEC_LOGIN_PUBLIC,
		NULL, NULL, &returnOrigin);
	if (r != TEEC_SUCCESS) {
		pr_err(
			"TEEC_OpenSession failed with 0x%x returnOrigun: %u\n",
			r, returnOrigin);
		TEEC_FinalizeContext(&context);
		return r;
	}

	op.paramTypes = TEEC_PARAM_TYPES(
		TEEC_MEMREF_TEMP_INPUT,
		TEEC_MEMREF_TEMP_INOUT,
		TEEC_NONE,
		TEEC_NONE);

	op.params[0].tmpref.buffer = conf;
	op.params[0].tmpref.size = conf_size;

	op.params[1].tmpref.buffer = outbuf;
	op.params[1].tmpref.size = size;

	buf = outbuf;

	r = TEEC_InvokeCommand(&session, CMD_READ_CHIPID, &op, &returnOrigin);
	if (r != TEEC_SUCCESS) {
		pr_err(
			"TEEC_InvokeCommand() failed with 0x%08x returnOrigin: %u\n",
			r, returnOrigin);
	}

	TEEC_CloseSession(&session);
	TEEC_FinalizeContext(&context);

	return r;
}
EXPORT_SYMBOL(tee_spi_transfer);

int tee_spi_transfer_disable(void)
{
	struct TEEC_Context context;
	struct TEEC_Session session;
	struct TEEC_Operation op;

	TEEC_Result r;

	uint32_t returnOrigin;

	memset(&context, 0, sizeof(context));
	memset(&session, 0, sizeof(session));
	memset(&op, 0, sizeof(op));

	r = TEEC_InitializeContext(NULL, &context);
	if (r != TEEC_SUCCESS) {
		pr_err(
			"TEEC_InitializeContext() failed with 0x%08x\n", r);
		return r;
	}

	r = TEEC_OpenSession(
		&context, &session, &SENSOR_DETECTOR_TA_UUID,
		TEEC_LOGIN_PUBLIC,
		NULL, NULL, &returnOrigin);
	if (r != TEEC_SUCCESS) {
		pr_err(
			"TEEC_OpenSession failed with 0x%x returnOrigun: %u\n",
			r, returnOrigin);
		TEEC_FinalizeContext(&context);
		return r;
	}

	op.paramTypes = TEEC_PARAM_TYPES(
		TEEC_NONE,
		TEEC_NONE,
		TEEC_NONE,
		TEEC_NONE);

	r = TEEC_InvokeCommand(&session, CMD_DISABLE, &op, &returnOrigin);
	if (r != TEEC_SUCCESS) {
		pr_err(
			"TEEC_InvokeCommand() failed with 0x%08x returnOrigin: %u\n",
			r, returnOrigin);
	}

	TEEC_CloseSession(&session);
	TEEC_FinalizeContext(&context);

	return r;
}
EXPORT_SYMBOL(tee_spi_transfer_disable);

int tee_fp_init(void)
{
	return 0;
}

void tee_fp_exit(void)
{
}
