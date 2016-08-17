/**
 * Copyright (c) 2011 Trusted Logic S.A.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#ifdef CONFIG_TF_TEEC

#include <asm/atomic.h>
#include <asm/system.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/pagemap.h>
#include <linux/time.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include "tf_protocol.h"
#include "tf_defs.h"
#include "tf_util.h"
#include "tf_comm.h"
#include "tf_conn.h"
#include "tf_teec.h"

#include "tee_client_api.h"

#define TF_COMMAND_BYTES(cmd) \
	(sizeof(cmd) - sizeof(struct tf_command_header))
#define TF_COMMAND_SIZE(cmd) \
	(TF_COMMAND_BYTES(cmd) / sizeof(u32))

/* Associate TEEC errors to POSIX/Linux errors. The matching is somewhat
	arbitrary but one-to-one for supported error codes. */
int TEEC_decode_error(TEEC_Result ret)
{
	switch (ret) {
	case TEEC_SUCCESS:                      return 0;
	case TEEC_ERROR_GENERIC:                return -EIO;
	case TEEC_ERROR_ACCESS_DENIED:          return -EPERM;
	case TEEC_ERROR_CANCEL:                 return -ECANCELED;
	case TEEC_ERROR_ACCESS_CONFLICT:        return -EBUSY;
	case TEEC_ERROR_EXCESS_DATA:            return -E2BIG;
	case TEEC_ERROR_BAD_FORMAT:             return -EDOM;
	case TEEC_ERROR_BAD_PARAMETERS:         return -EINVAL;
	case TEEC_ERROR_BAD_STATE:              return -EBADFD;
	case TEEC_ERROR_ITEM_NOT_FOUND:         return -ENOENT;
	case TEEC_ERROR_NOT_IMPLEMENTED:        return -EPROTONOSUPPORT;
	case TEEC_ERROR_NOT_SUPPORTED:          return -ENOSYS;
	case TEEC_ERROR_NO_DATA:                return -ENODATA;
	case TEEC_ERROR_OUT_OF_MEMORY:          return -ENOMEM;
	case TEEC_ERROR_BUSY:                   return -EAGAIN;
	case TEEC_ERROR_COMMUNICATION:          return -EPIPE;
	case TEEC_ERROR_SECURITY:               return -ECONNABORTED;
	case TEEC_ERROR_SHORT_BUFFER:           return -EFBIG;
	default:                                return -EIO;
	}
}
EXPORT_SYMBOL(TEEC_decode_error);

/* Associate POSIX/Linux errors to TEEC errors. The matching is somewhat
	arbitrary, but TEEC_encode_error(TEEC_decode_error(x))==x for supported
	error codes. */
TEEC_Result TEEC_encode_error(int err)
{
	if (err >= 0)
		return S_SUCCESS;

	switch (err) {
	case 0:		          return TEEC_SUCCESS;
	case -EIO:		  return TEEC_ERROR_GENERIC;
	case -EPERM:		  return TEEC_ERROR_ACCESS_DENIED;
	case -ECANCELED:	  return TEEC_ERROR_CANCEL;
	case -EBUSY:		  return TEEC_ERROR_ACCESS_CONFLICT;
	case -E2BIG:		  return TEEC_ERROR_EXCESS_DATA;
	case -EDOM:		  return TEEC_ERROR_BAD_FORMAT;
	case -EINVAL:		  return TEEC_ERROR_BAD_PARAMETERS;
	case -EBADFD:		  return TEEC_ERROR_BAD_STATE;
	case -ENOENT:		  return TEEC_ERROR_ITEM_NOT_FOUND;
	case -EPROTONOSUPPORT:    return TEEC_ERROR_NOT_IMPLEMENTED;
	case -ENOSYS:		  return TEEC_ERROR_NOT_SUPPORTED;
	case -ENODATA:	          return TEEC_ERROR_NO_DATA;
	case -ENOMEM:		  return TEEC_ERROR_OUT_OF_MEMORY;
	case -EAGAIN:		  return TEEC_ERROR_BUSY;
	case -EPIPE:		  return TEEC_ERROR_COMMUNICATION;
	case -ECONNABORTED:	  return TEEC_ERROR_SECURITY;
	case -EFBIG:		  return TEEC_ERROR_SHORT_BUFFER;
	default:                  return TEEC_ERROR_GENERIC;
	}
}
EXPORT_SYMBOL(TEEC_encode_error);

/* Encode a TEEC time limit into an SChannel time limit. */
static u64 TEEC_encode_timeout(const TEEC_TimeLimit *timeLimit)
{
	if (timeLimit == NULL)
		return (u64)-1;
	else
		return *timeLimit;
}

/* Convert a timeout into a time limit in our internal format. */
void TEEC_GetTimeLimit(TEEC_Context *sContext,
		       uint32_t nTimeout, /*ms from now*/
		       TEEC_TimeLimit *sTimeLimit)
{
	/*Use the kernel time as the TEE time*/
	struct timeval now;
	do_gettimeofday(&now);
	*sTimeLimit =
		((TEEC_TimeLimit)now.tv_sec * 1000 +
		 now.tv_usec / 1000 +
		 nTimeout);
}
EXPORT_SYMBOL(TEEC_GetTimeLimit);

#define TF_PARAM_TYPE_INPUT_FLAG                0x1
#define TF_PARAM_TYPE_OUTPUT_FLAG               0x2
#define TF_PARAM_TYPE_MEMREF_FLAG               0x4
#define TF_PARAM_TYPE_REGISTERED_MEMREF_FLAG    0x8

/* Update the type of a whole memref with the direction deduced from
	the INPUT and OUTPUT flags of the memref. */
static void TEEC_encode_whole_memref_flags(u16 *param_types,
					   unsigned i,
					   u32 flags)
{
	if (flags & TEEC_MEM_INPUT)
		*param_types |= TF_PARAM_TYPE_INPUT_FLAG << (4*i);
	if (flags & TEEC_MEM_OUTPUT)
		*param_types |= TF_PARAM_TYPE_OUTPUT_FLAG << (4*i);
}

/* Encode the parameters and type of an operation from the TEE API format
	into an SChannel message. */
void TEEC_encode_parameters(u16 *param_types,
			    union tf_command_param *params,
			    TEEC_Operation *operation)
{
	unsigned i;
	if (operation == NULL) {
		*param_types = 0;
		return;
	}
	*param_types = operation->paramTypes;
	for (i = 0; i < 4; i++) {
		unsigned ty = TF_GET_PARAM_TYPE(operation->paramTypes, i);
		TEEC_Parameter *op = operation->params + i;
		if (ty & TF_PARAM_TYPE_REGISTERED_MEMREF_FLAG) {
			TEEC_SharedMemory *sm = op->memref.parent;
			params[i].memref.block = sm->imp._block;
			if (ty == TEEC_MEMREF_WHOLE) {
				TEEC_encode_whole_memref_flags(param_types, i,
							       sm->flags);
				params[i].memref.size = sm->size;
				params[i].memref.offset = 0;
			} else {
				params[i].memref.size = op->memref.size;
				params[i].memref.offset = op->memref.offset;
			}
		} else if (ty & TF_PARAM_TYPE_MEMREF_FLAG) {
			/* Set up what tf_map_temp_shmem (called by
			   tf_open_client_session and
			   tf_invoke_client_command) expects:
			   .descriptor and .offset to both be set to the
			   address of the buffer. */
			u32 address = (u32)op->tmpref.buffer;
			params[i].temp_memref.descriptor = address;
			params[i].temp_memref.size = op->tmpref.size;
			params[i].temp_memref.offset = address;
		} else if (ty & TF_PARAM_TYPE_INPUT_FLAG) {
			params[i].value.a = op->value.a;
			params[i].value.b = op->value.b;
		} else {
			/* output-only value or none, so nothing to do */
		}
	}
}

/* Decode updated parameters from an SChannel answer into the TEE API format. */
void TEEC_decode_parameters(union tf_answer_param *params,
			    TEEC_Operation *operation)
{
	unsigned i;

	if (operation == NULL)
		return;

	for (i = 0; i < 4; i++) {
		unsigned ty = TF_GET_PARAM_TYPE(operation->paramTypes, i);
		TEEC_Parameter *op = operation->params + i;
		if (!(ty & TF_PARAM_TYPE_OUTPUT_FLAG)) {
			/* input-only or none, so nothing to do */
		} else if (ty & TF_PARAM_TYPE_REGISTERED_MEMREF_FLAG) {
			op->memref.size = params[i].size.size;
		} else if (ty & TF_PARAM_TYPE_MEMREF_FLAG) {
			op->tmpref.size = params[i].size.size;
		} else {
			op->value.a = params[i].value.a;
			op->value.b = params[i].value.b;
		}
	}
}

/* Start a potentially-cancellable operation. */
void TEEC_start_operation(TEEC_Context *context,
			  TEEC_Session *session,
			  TEEC_Operation *operation)
{
	if (operation != NULL) {
		operation->imp._pSession = session;
		/* Flush the assignment to imp._pSession, so that
		   RequestCancellation can read that field if started==1. */
		barrier();
		operation->started = 1;
	}
}

/* Mark a potentially-cancellable operation as finished. */
void TEEC_finish_operation(TEEC_Operation *operation)
{
	if (operation != NULL) {
		operation->started = 2;
		barrier();
	}
}



TEEC_Result TEEC_InitializeContext(const char *name,
				   TEEC_Context *context)
{
	int error;
	struct tf_connection *connection = NULL;

	error = tf_open(tf_get_device(), NULL, &connection);
	if (error != 0) {
		dprintk(KERN_ERR "TEEC_InitializeContext(%s): "
			"tf_open failed (error %d)!\n",
			(name == NULL ? "(null)" : name), error);
		goto error;
	}
	BUG_ON(connection == NULL);
	connection->owner = TF_CONNECTION_OWNER_KERNEL;

	error = tf_create_device_context(connection);
	if (error != 0) {
		dprintk(KERN_ERR "TEEC_InitializeContext(%s): "
			"tf_create_device_context failed (error %d)!\n",
			(name == NULL ? "(null)" : name), error);
		goto error;
	}

	context->imp._connection = connection;
	/*spin_lock_init(&context->imp._operations_lock);*/
	return S_SUCCESS;

error:
	tf_close(connection);
	return TEEC_encode_error(error);
}
EXPORT_SYMBOL(TEEC_InitializeContext);

void TEEC_FinalizeContext(TEEC_Context *context)
{
	struct tf_connection *connection = context->imp._connection;
	dprintk(KERN_DEBUG "TEEC_FinalizeContext: connection=%p", connection);
	tf_close(connection);
	context->imp._connection = NULL;
}
EXPORT_SYMBOL(TEEC_FinalizeContext);

TEEC_Result TEEC_RegisterSharedMemory(TEEC_Context *context,
				      TEEC_SharedMemory *sharedMem)
{
	union tf_command command_message = { { 0, } };
	struct tf_command_register_shared_memory *cmd =
		&command_message.register_shared_memory;
	union tf_answer answer_message;
	struct tf_answer_register_shared_memory *ans =
		&answer_message.register_shared_memory;
	TEEC_Result ret;
	memset(&sharedMem->imp, 0, sizeof(sharedMem->imp));

	cmd->message_size = TF_COMMAND_SIZE(*cmd);
	cmd->message_type = TF_MESSAGE_TYPE_REGISTER_SHARED_MEMORY;
	cmd->memory_flags = sharedMem->flags;
	cmd->operation_id = (u32)&answer_message;
	cmd->device_context = (u32)context;
	/*cmd->block_id will be set by tf_register_shared_memory*/
	cmd->shared_mem_size = sharedMem->size;
	cmd->shared_mem_start_offset = 0;
	cmd->shared_mem_descriptors[0] = (u32)sharedMem->buffer;

	ret = TEEC_encode_error(
		tf_register_shared_memory(context->imp._connection,
					       &command_message,
					       &answer_message));
	if (ret == TEEC_SUCCESS)
		ret = ans->error_code;

	if (ret == S_SUCCESS) {
		sharedMem->imp._context = context;
		sharedMem->imp._block = ans->block;
	}
	return ret;
}
EXPORT_SYMBOL(TEEC_RegisterSharedMemory);

#define TEEC_POINTER_TO_ZERO_SIZED_BUFFER ((void *)0x010)

TEEC_Result TEEC_AllocateSharedMemory(TEEC_Context *context,
				      TEEC_SharedMemory *sharedMem)
{
	TEEC_Result ret;
	dprintk(KERN_DEBUG "TEEC_AllocateSharedMemory: requested=%lu",
		(unsigned long)sharedMem->size);
	if (sharedMem->size == 0) {
		/* Allocating 0 bytes must return a non-NULL pointer, but the
		   pointer doesn't need to be to memory that is mapped
		   anywhere. So we return a pointer into an unmapped page. */
		sharedMem->buffer = TEEC_POINTER_TO_ZERO_SIZED_BUFFER;
	} else {
		sharedMem->buffer = internal_vmalloc(sharedMem->size);
		if (sharedMem->buffer == NULL) {
			dprintk(KERN_INFO "TEEC_AllocateSharedMemory: could "
				"not allocate %lu bytes",
				(unsigned long)sharedMem->size);
			return TEEC_ERROR_OUT_OF_MEMORY;
		}
	}

	ret = TEEC_RegisterSharedMemory(context, sharedMem);
	if (ret == TEEC_SUCCESS) {
		sharedMem->imp._allocated = 1;
	} else {
		internal_vfree(sharedMem->buffer);
		sharedMem->buffer = NULL;
		memset(&sharedMem->imp, 0, sizeof(sharedMem->imp));
	}
	return ret;
}
EXPORT_SYMBOL(TEEC_AllocateSharedMemory);

void TEEC_ReleaseSharedMemory(TEEC_SharedMemory *sharedMem)
{
	TEEC_Context *context = sharedMem->imp._context;
	union tf_command command_message = { { 0, } };
	struct tf_command_release_shared_memory *cmd =
		&command_message.release_shared_memory;
	union tf_answer answer_message;

	cmd->message_size = TF_COMMAND_SIZE(*cmd);
	cmd->message_type = TF_MESSAGE_TYPE_RELEASE_SHARED_MEMORY;
	cmd->operation_id = (u32)&answer_message;
	cmd->device_context = (u32)context;
	cmd->block = sharedMem->imp._block;

	tf_release_shared_memory(context->imp._connection,
				      &command_message,
				      &answer_message);
	if (sharedMem->imp._allocated) {
		if (sharedMem->buffer != TEEC_POINTER_TO_ZERO_SIZED_BUFFER)
			internal_vfree(sharedMem->buffer);
		sharedMem->buffer = NULL;
		sharedMem->size = 0;
	}
	memset(&sharedMem->imp, 0, sizeof(sharedMem->imp));
}
EXPORT_SYMBOL(TEEC_ReleaseSharedMemory);

TEEC_Result TEEC_OpenSessionEx(TEEC_Context *context,
			       TEEC_Session *session,
			       const TEEC_TimeLimit *timeLimit,
			       const TEEC_UUID * destination,
			       u32 connectionMethod,
			       void *connectionData,
			       TEEC_Operation *operation,
			       u32 *errorOrigin)
{
	union tf_command command_message = { { 0, } };
	struct tf_command_open_client_session *cmd =
		&command_message.open_client_session;
	union tf_answer answer_message = { { 0, } };
	struct tf_answer_open_client_session *ans =
		&answer_message.open_client_session;
	TEEC_Result ret;

	/* Note that we set the message size to the whole size of the
	   structure. tf_open_client_session will adjust it down
	   to trim the unnecessary portion of the login_data field. */
	cmd->message_size = TF_COMMAND_SIZE(*cmd);
	cmd->message_type = TF_MESSAGE_TYPE_OPEN_CLIENT_SESSION;
	cmd->operation_id = (u32)&answer_message;
	cmd->device_context = (u32)context;
	cmd->cancellation_id = (u32)operation;
	cmd->timeout = TEEC_encode_timeout(timeLimit);
	memcpy(&cmd->destination_uuid, destination,
	       sizeof(cmd->destination_uuid));
	cmd->login_type = connectionMethod;
	TEEC_encode_parameters(&cmd->param_types, cmd->params, operation);

	switch (connectionMethod) {
	case TEEC_LOGIN_PRIVILEGED:
	case TEEC_LOGIN_PUBLIC:
		break;
	case TEEC_LOGIN_APPLICATION:
	case TEEC_LOGIN_USER:
	case TEEC_LOGIN_USER_APPLICATION:
	case TEEC_LOGIN_GROUP:
	case TEEC_LOGIN_GROUP_APPLICATION:
	default:
		return TEEC_ERROR_NOT_IMPLEMENTED;
	}

	TEEC_start_operation(context, session, operation);

	ret = TEEC_encode_error(
		tf_open_client_session(context->imp._connection,
					    &command_message,
					    &answer_message));

	TEEC_finish_operation(operation);
	TEEC_decode_parameters(ans->answers, operation);
	if (errorOrigin != NULL) {
		*errorOrigin = (ret == TEEC_SUCCESS ?
				ans->error_origin :
				TEEC_ORIGIN_COMMS);
	}

	if (ret == TEEC_SUCCESS)
		ret = ans->error_code;

	if (ret == S_SUCCESS) {
		session->imp._client_session = ans->client_session;
		session->imp._context = context;
	}
	return ret;
}
EXPORT_SYMBOL(TEEC_OpenSessionEx);

TEEC_Result TEEC_OpenSession(TEEC_Context *context,
			     TEEC_Session *session,
			     const TEEC_UUID * destination,
			     u32 connectionMethod,
			     void *connectionData,
			     TEEC_Operation *operation,
			     u32 *errorOrigin)
{
	return TEEC_OpenSessionEx(context, session,
				  NULL, /*timeLimit*/
				  destination,
				  connectionMethod, connectionData,
				  operation, errorOrigin);
}
EXPORT_SYMBOL(TEEC_OpenSession);

void TEEC_CloseSession(TEEC_Session *session)
{
	if (session != NULL) {
		TEEC_Context *context = session->imp._context;
		union tf_command command_message = { { 0, } };
		struct tf_command_close_client_session *cmd =
			&command_message.close_client_session;
		union tf_answer answer_message;

		cmd->message_size = TF_COMMAND_SIZE(*cmd);
		cmd->message_type = TF_MESSAGE_TYPE_CLOSE_CLIENT_SESSION;
		cmd->operation_id = (u32)&answer_message;
		cmd->device_context = (u32)context;
		cmd->client_session = session->imp._client_session;

		tf_close_client_session(context->imp._connection,
					     &command_message,
					     &answer_message);

		session->imp._client_session = 0;
		session->imp._context = NULL;
	}
}
EXPORT_SYMBOL(TEEC_CloseSession);

TEEC_Result TEEC_InvokeCommandEx(TEEC_Session *session,
				    const TEEC_TimeLimit *timeLimit,
				    u32 commandID,
				    TEEC_Operation *operation,
				    u32 *errorOrigin)
{
	TEEC_Context *context = session->imp._context;
	union tf_command command_message = { { 0, } };
	struct tf_command_invoke_client_command *cmd =
		&command_message.invoke_client_command;
	union tf_answer answer_message = { { 0, } };
	struct tf_answer_invoke_client_command *ans =
		&answer_message.invoke_client_command;
	TEEC_Result ret;

	cmd->message_size = TF_COMMAND_SIZE(*cmd);
	cmd->message_type = TF_MESSAGE_TYPE_INVOKE_CLIENT_COMMAND;
	cmd->operation_id = (u32)&answer_message;
	cmd->device_context = (u32)context;
	cmd->client_session = session->imp._client_session;
	cmd->timeout = TEEC_encode_timeout(timeLimit);
	cmd->cancellation_id = (u32)operation;
	cmd->client_command_identifier = commandID;
	TEEC_encode_parameters(&cmd->param_types, cmd->params, operation);

	TEEC_start_operation(context, session, operation);

	ret = TEEC_encode_error(
		tf_invoke_client_command(context->imp._connection,
					      &command_message,
					      &answer_message));

	TEEC_finish_operation(operation);
	TEEC_decode_parameters(ans->answers, operation);
	if (errorOrigin != NULL) {
		*errorOrigin = (ret == TEEC_SUCCESS ?
				ans->error_origin :
				TEEC_ORIGIN_COMMS);
	}

	if (ret == TEEC_SUCCESS)
		ret = ans->error_code;
	return ret;
}
EXPORT_SYMBOL(TEEC_InvokeCommandEx);

TEEC_Result TEEC_InvokeCommand(TEEC_Session *session,
			       u32 commandID,
			       TEEC_Operation *operation,
			       u32 *errorOrigin)
{
	return TEEC_InvokeCommandEx(session,
				    NULL, /*timeLimit*/
				    commandID,
				    operation, errorOrigin);
}
EXPORT_SYMBOL(TEEC_InvokeCommand);

TEEC_Result TEEC_send_cancellation_message(TEEC_Context *context,
					   u32 client_session,
					   u32 cancellation_id)
{
	union tf_command command_message = { { 0, } };
	struct tf_command_cancel_client_operation *cmd =
		&command_message.cancel_client_operation;
	union tf_answer answer_message = { { 0, } };
	struct tf_answer_cancel_client_operation *ans =
		&answer_message.cancel_client_operation;
	TEEC_Result ret;

	cmd->message_size = TF_COMMAND_SIZE(*cmd);
	cmd->message_type = TF_MESSAGE_TYPE_CANCEL_CLIENT_COMMAND;
	cmd->operation_id = (u32)&answer_message;
	cmd->device_context = (u32)context;
	cmd->client_session = client_session;
	cmd->cancellation_id = cancellation_id;

	ret = TEEC_encode_error(
		tf_cancel_client_command(context->imp._connection,
					      &command_message,
					      &answer_message));

	if (ret == TEEC_SUCCESS)
		ret = ans->error_code;
	return ret;
}

void TEEC_RequestCancellation(TEEC_Operation *operation)
{
	TEEC_Result ret;
	while (1) {
		u32 state = operation->started;
		switch (state) {
		case 0: /*The operation data structure isn't initialized yet*/
			break;

		case 1: /*operation is in progress in the client*/
			ret = TEEC_send_cancellation_message(
				operation->imp._pSession->imp._context,
				operation->imp._pSession->imp._client_session,
				(u32)operation);
			if (ret == TEEC_SUCCESS) {
				/*The cancellation was successful*/
				return;
			}
			/* The command has either not reached the secure world
			   yet or has completed already. Either way, retry. */
			break;

		case 2: /*operation has completed already*/
			return;
		}
		/* Since we're busy-waiting for the operation to be started
		   or finished, yield. */
		schedule();
	}
}
EXPORT_SYMBOL(TEEC_RequestCancellation);

#endif /* defined(CONFIG_TF_TEEC) */
