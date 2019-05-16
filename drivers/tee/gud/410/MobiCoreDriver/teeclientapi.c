// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */

#include <linux/device.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/export.h>

#include "public/GP/tee_client_api.h"
#include "public/mc_user.h"

#include "main.h"
#include "mci/mcinq.h"	/* TA termination codes */
#include "client.h"

/* Macros */
#define _TEEC_GET_PARAM_TYPE(t, i) (((t) >> (4 * (i))) & 0xF)

/* Parameter number */
#define _TEEC_PARAMETER_NUMBER		4

/**teec_shared_memory
 * These error codes are still to be decided by GP and as we do not wish to
 * expose any part of the GP TAF as of yet, for now they will have to live here
 * until we decide what to do about them.
 */
#define TEEC_ERROR_TA_LOCKED		0xFFFF0257
#define TEEC_ERROR_SD_BLOCKED		0xFFFF0258
#define TEEC_ERROR_TARGET_KILLED	0xFFFF0259

static DECLARE_WAIT_QUEUE_HEAD(operations_wq);

static void _lib_uuid_to_array(const struct teec_uuid *uuid, u8 *uuid_array)
{
	u8 *identifier_cursor = (u8 *)uuid;
	/* offsets and syntax constants. See explanations above */
#ifdef S_BIG_ENDIAN
	u32 offsets = 0;
#else
	u32 offsets = 0xF1F1DF13;
#endif
	u32 i;

	for (i = 0; i < sizeof(struct teec_uuid); i++) {
		/* Two-digit hex number */
		s32 offset = ((s32)((offsets & 0xF) << 28)) >> 28;
		u8 number = identifier_cursor[offset];

		offsets >>= 4;
		identifier_cursor++;

		uuid_array[i] = number;
	}
}

static u32 _teec_to_gp_operation(struct teec_operation *teec_op,
				 struct gp_operation *gp_op)
{
	int i;
	int ret = 0;

	for (i = 0; i < _TEEC_PARAMETER_NUMBER; i++) {
		switch (_TEEC_GET_PARAM_TYPE(teec_op->param_types, i)) {
		case TEEC_VALUE_INPUT:
		case TEEC_VALUE_INOUT:
			gp_op->params[i].value.a = teec_op->params[i].value.a;
			gp_op->params[i].value.b = teec_op->params[i].value.b;
			break;
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			gp_op->params[i].tmpref.buffer =
				(uintptr_t)teec_op->params[i].tmpref.buffer;
			gp_op->params[i].tmpref.size =
				teec_op->params[i].tmpref.size;
			break;
		case TEEC_MEMREF_WHOLE:
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
			gp_op->params[i].memref.offset =
				teec_op->params[i].memref.offset;
			gp_op->params[i].memref.size =
				teec_op->params[i].memref.size;
			gp_op->params[i].memref.parent.buffer =
			 (uintptr_t)teec_op->params[i].memref.parent->buffer;
			gp_op->params[i].memref.parent.size =
				teec_op->params[i].memref.parent->size;
			gp_op->params[i].memref.parent.flags =
				teec_op->params[i].memref.parent->flags;
			break;
		case TEEC_NONE:
		case TEEC_VALUE_OUTPUT:
			break;
		default:
			ret = -EINVAL;
		}
	}
	gp_op->param_types = teec_op->param_types;
	return ret;
}

static void _teec_from_gp_operation(struct gp_operation *gp_op,
				    struct teec_operation *teec_op)
{
	int i;

	for (i = 0; i < _TEEC_PARAMETER_NUMBER; i++) {
		switch (_TEEC_GET_PARAM_TYPE(gp_op->param_types, i)) {
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
			teec_op->params[i].value.a = gp_op->params[i].value.a;
			teec_op->params[i].value.b = gp_op->params[i].value.b;
			break;
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			teec_op->params[i].tmpref.size =
				gp_op->params[i].tmpref.size;
			break;
		case TEEC_MEMREF_WHOLE:
			break;
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
			teec_op->params[i].memref.size =
				gp_op->params[i].memref.size;
			break;
		case TEEC_NONE:
		case TEEC_VALUE_INPUT:
			break;
		default:
			break;
		}
	}
}

static u32 _teec_convert_error(int errno)
{
	switch (errno) {
	case ENOENT:
		return TEEC_ERROR_ITEM_NOT_FOUND;
	case EACCES:
		return TEEC_ERROR_ACCESS_DENIED;
	case EINVAL:
		return TEEC_ERROR_BAD_PARAMETERS;
	case ENOSPC:
		return TEEC_ERROR_OUT_OF_MEMORY;
	case ECONNREFUSED:
		return TEEC_ERROR_SD_BLOCKED;
	case ECONNABORTED:
		return TEEC_ERROR_TA_LOCKED;
	case ECONNRESET:
		return TEEC_ERROR_TARGET_KILLED;
	case EBUSY:
		return TEEC_ERROR_BUSY;
	case EKEYREJECTED:
		return TEEC_ERROR_SECURITY;
	case ETIME:
		return TEEC_ERROR_TARGET_DEAD;
	default:
		return TEEC_ERROR_GENERIC;
	}
}

/* teec_initialize_context: TEEC_SUCCESS, Another error code from Table 4-2 */
u32 teec_initialize_context(const char *name, struct teec_context *context)
{
	struct tee_client *client;
	int ret;
	(void)name;

	mc_dev_devel("== %s() ==============", __func__);

	if (!context) {
		mc_dev_devel("context is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	/* Make sure TEE was started */
	ret = mc_wait_tee_start();
	if (ret) {
		mc_dev_err(ret, "TEE failed to start, now or in the past");
		return TEEC_ERROR_BAD_STATE;
	}

	/* Create client */
	client = client_create(true);
	if (!client)
		return TEEC_ERROR_OUT_OF_MEMORY;

	/* Store client in context */
	context->imp.client = client;

	return TEEC_SUCCESS;
}
EXPORT_SYMBOL(teec_initialize_context);

/*
 * The implementation of this function MUST NOT be able to fail: after this
 * function returns the Client Application must be able to consider that the
 * Context has been closed
 */
void teec_finalize_context(struct teec_context *context)
{
	mc_dev_devel("== %s() ==============", __func__);

	/* The parameter context MUST point to an initialized TEE Context */
	if (!context) {
		mc_dev_devel("context is NULL");
		return;
	}

	/* The implementation of this function MUST NOT be able to fail: after
	 * this function returns the Client Application must be able to
	 * consider that the Context has been closed
	 */
	client_close(context->imp.client);
	context->imp.client = NULL;
}
EXPORT_SYMBOL(teec_finalize_context);

/*
 * If the return_origin is different from TEEC_ORIGIN_TRUSTED_APP, an error code
 * from Table 4-2. If the return_origin is equal to TEEC_ORIGIN_TRUSTED_APP, a
 * return code defined by the protocol between the Client Application and the
 * Trusted Application
 */
u32 teec_open_session(struct teec_context *context,
		      struct teec_session *session,
		      const struct teec_uuid *destination,
		      u32 connection_method,
		      const void *connection_data,
		      struct teec_operation *operation,
		      u32 *return_origin)
{
	struct mc_uuid_t uuid;
	struct mc_identity identity = {0};
	struct tee_client *client = NULL;
	struct gp_operation gp_op;
	struct gp_return gp_ret;
	int ret = 0, timeout;

	mc_dev_devel("== %s() ==============", __func__);
	gp_ret.value = TEEC_SUCCESS;
	if (return_origin)
		*return_origin = TEEC_ORIGIN_API;

	/* The parameter context MUST point to an initialized TEE Context */
	if (!context) {
		mc_dev_devel("context is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (!context->imp.client) {
		mc_dev_devel("context not initialized");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	client = context->imp.client;

	if (!session) {
		mc_dev_devel("session is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	connection_method = TEEC_TT_LOGIN_KERNEL;
	session->imp.active = false;

	_lib_uuid_to_array(destination, uuid.value);

	memset(&gp_op, 0, sizeof(gp_op));
	if (operation) {
		operation->imp.session = &session->imp;
		ret = _teec_to_gp_operation(operation, &gp_op);
		if (ret)
			return TEEC_ERROR_BAD_PARAMETERS;
	}

	identity.login_type = (enum mc_login_type)connection_method;

	/* Wait for GP loading to be possible, maximum 30s */
	timeout = 30;
	do {
		ret = client_gp_open_session(client, &uuid, &gp_op, &identity,
					     &gp_ret, &session->imp.session_id);
		if (!ret || ret != EAGAIN)
			break;

		msleep(1000);
	} while (--timeout);

	if (ret || gp_ret.value != TEEC_SUCCESS) {
		mc_dev_devel("client_gp_open_session failed(%08x) %08x", ret,
			     gp_ret.value);
		if (ret)
			gp_ret.value = _teec_convert_error(-ret);
		else if (return_origin)
			/* Update origin as it's not the API */
			*return_origin = gp_ret.origin;
	} else {
		mc_dev_devel(" created session ID %x", session->imp.session_id);
		session->imp.context = context->imp;
		session->imp.active = true;
		if (operation)
			_teec_from_gp_operation(&gp_op, operation);
	}

	mc_dev_devel(" %s() = 0x%x", __func__, gp_ret.value);
	return gp_ret.value;
}
EXPORT_SYMBOL(teec_open_session);

u32 teec_invoke_command(struct teec_session *session,
			u32 command_id,
			struct teec_operation *operation,
			u32 *return_origin)
{
	struct tee_client *client = NULL;
	struct gp_operation gp_op = {0};
	struct gp_return gp_ret = {0};
	int ret = 0;

	mc_dev_devel("== %s() ==============", __func__);

	gp_ret.value = TEEC_SUCCESS;
	if (return_origin)
		*return_origin = TEEC_ORIGIN_API;

	if (!session) {
		mc_dev_devel("session is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (!session->imp.active) {
		mc_dev_devel("session is inactive");
		return TEEC_ERROR_BAD_STATE;
	}
	client = session->imp.context.client;

	if (operation) {
		operation->imp.session = &session->imp;
		if (_teec_to_gp_operation(operation, &gp_op))
			return TEEC_ERROR_BAD_PARAMETERS;
	} else {
		gp_op.param_types = 0;
	}

	ret = client_gp_invoke_command(client, session->imp.session_id,
				       command_id, &gp_op, &gp_ret);

	if (ret || gp_ret.value != TEEC_SUCCESS) {
		mc_dev_devel("client_gp_invoke_command failed(%08x) %08x", ret,
			     gp_ret.value);
		if (ret)
			gp_ret.value = _teec_convert_error(-ret);
		else if (return_origin)
			/* Update origin as it's not the API */
			*return_origin = gp_ret.origin;
	} else if (operation) {
		_teec_from_gp_operation(&gp_op, operation);
	}

	mc_dev_devel(" %s() = 0x%x", __func__, gp_ret.value);
	return gp_ret.value;
}
EXPORT_SYMBOL(teec_invoke_command);

void teec_close_session(struct teec_session *session)
{
	int ret = 0;
	struct tee_client *client = NULL;

	mc_dev_devel("== %s() ==============", __func__);

	/* The implementation MUST do nothing if session is NULL */
	if (!session) {
		mc_dev_devel("session is NULL");
		return;
	}
	client = session->imp.context.client;

	if (session->imp.active) {
		ret = client_gp_close_session(client, session->imp.session_id);

		if (ret)
			/* continue even in case of error */
			mc_dev_devel("client_gp_close failed(%08x)", ret);

		session->imp.active = false;
	}

	mc_dev_devel(" %s() = 0x%x", __func__, ret);
}
EXPORT_SYMBOL(teec_close_session);

/*
 * Implementation note. We handle internally 2 kind of pointers : kernel memory
 * (kmalloc, get_pages, ...) and dynamic memory (vmalloc). A global pointer from
 * a kernel module has the same format as a vmalloc buffer. However, our code
 * cannot detect that, so it considers it a kmalloc buffer. The TA trying to use
 * that shared buffer is likely to crash
 */
u32 teec_register_shared_memory(struct teec_context *context,
				struct teec_shared_memory *shared_mem)
{
	struct gp_shared_memory memref;
	struct gp_return gp_ret;
	int ret = 0;

	mc_dev_devel("== %s() ==============", __func__);

	/* The parameter context MUST point to an initialized TEE Context */
	if (!context) {
		mc_dev_devel("context is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	/*
	 * The parameter shared_mem MUST point to the Shared Memory structure
	 * defining the memory region to register
	 */
	if (!shared_mem) {
		mc_dev_devel("shared_mem is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	/*
	 * The buffer field MUST point to the memory region to be shared,
	 * and MUST not be NULL
	 */
	if (!shared_mem->buffer) {
		mc_dev_devel("shared_mem->buffer is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	if (shared_mem->flags & ~TEEC_MEM_INOUT) {
		mc_dev_devel("shared_mem->flags is incorrect");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	if (!shared_mem->flags) {
		mc_dev_devel("shared_mem->flags is incorrect");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	memref.buffer = (uintptr_t)shared_mem->buffer;
	memref.flags = shared_mem->flags;
	memref.size = shared_mem->size;
	ret = client_gp_register_shared_mem(context->imp.client, NULL, NULL,
					    &memref, &gp_ret);

	if (ret)
		return _teec_convert_error(-ret);

	shared_mem->imp.client = context->imp.client;
	shared_mem->imp.implementation_allocated = false;

	return TEEC_SUCCESS;
}
EXPORT_SYMBOL(teec_register_shared_memory);

u32 teec_allocate_shared_memory(struct teec_context *context,
				struct teec_shared_memory *shared_mem)
{
	struct gp_shared_memory memref;
	struct gp_return gp_ret;
	int ret = 0;

	/* No connection to "context"? */
	mc_dev_devel("== %s() ==============", __func__);

	/* The parameter context MUST point to an initialized TEE Context */
	if (!context) {
		mc_dev_devel("context is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	/*
	 * The parameter shared_mem MUST point to the Shared Memory structure
	 * defining the memory region to register
	 */
	if (!shared_mem) {
		mc_dev_devel("shared_mem is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	if (shared_mem->flags & ~TEEC_MEM_INOUT) {
		mc_dev_devel("shared_mem->flags is incorrect");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	if (!shared_mem->flags) {
		mc_dev_devel("shared_mem->flags is incorrect");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	shared_mem->buffer = vmalloc(shared_mem->size);
	if (!shared_mem->buffer)
		return TEEC_ERROR_OUT_OF_MEMORY;

	memref.buffer = (uintptr_t)shared_mem->buffer;
	memref.flags = shared_mem->flags;
	memref.size = shared_mem->size;
	ret = client_gp_register_shared_mem(context->imp.client, NULL, NULL,
					    &memref, &gp_ret);

	if (ret) {
		vfree(shared_mem->buffer);
		shared_mem->buffer = NULL;
		shared_mem->size = 0;
		return _teec_convert_error(-ret);
	}

	shared_mem->imp.client = context->imp.client;
	shared_mem->imp.implementation_allocated = true;

	return TEEC_SUCCESS;
}
EXPORT_SYMBOL(teec_allocate_shared_memory);

void teec_release_shared_memory(struct teec_shared_memory *shared_mem)
{
	struct gp_shared_memory memref;

	/* No connection to "context"? */
	mc_dev_devel("== %s() ==============", __func__);

	/* The implementation MUST do nothing if shared_mem is NULL */
	if (!shared_mem) {
		mc_dev_devel("shared_mem is NULL");
		return;
	}

	memref.buffer = (uintptr_t)shared_mem->buffer;
	memref.flags = shared_mem->flags;
	memref.size = shared_mem->size;
	(void)client_gp_release_shared_mem(shared_mem->imp.client, &memref);

	/*
	 * For a memory buffer allocated using teec_allocate_shared_memory the
	 * Implementation MUST free the underlying memory
	 */
	if (shared_mem->imp.implementation_allocated) {
		if (shared_mem->buffer) {
			vfree(shared_mem->buffer);
			shared_mem->buffer = NULL;
			shared_mem->size = 0;
		}
	}
}
EXPORT_SYMBOL(teec_release_shared_memory);

void teec_request_cancellation(struct teec_operation *operation)
{
	struct teec_session_imp *session;
	int ret;

	mc_dev_devel("== %s() ==============", __func__);

	ret = wait_event_interruptible(operations_wq, operation->started);
	if (ret == -ERESTARTSYS) {
		mc_dev_devel("signal received");
		return;
	}

	mc_dev_devel("operation->started changed from 0 to %d",
		     operation->started);

	if (operation->started > 1) {
		mc_dev_devel("the operation has finished");
		return;
	}

	session = operation->imp.session;
	operation->started = 2;
	wake_up_interruptible(&operations_wq);

	if (!session->active) {
		mc_dev_devel("Corresponding session is not active");
		return;
	}

	/* TODO: handle cancellation */

	/* Signal the Trustlet */
	ret = client_notify_session(session->context.client,
				    session->session_id);
	if (ret)
		mc_dev_devel("Notify failed: %d", ret);
}
EXPORT_SYMBOL(teec_request_cancellation);
