/*
 * Copyright (c) 2013-2016 TRUSTONIC LIMITED
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

#include <linux/device.h>
#include <linux/delay.h>
#include <linux/vmalloc.h>
#include <linux/export.h>

#include "public/GP/tee_client_api.h"
#include "public/mc_user.h"

#include "main.h"
#include "mci/gptci.h"	/* Needs stuff from tee_client_api.h or its includes */
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

/* Local functions */
static u32 _teec_unwind_operation(struct teec_session_imp *session,
				  struct _teec_tci *tci,
				  struct teec_operation *operation,
				  bool copy_values,
				  u32 *return_origin);

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

static u32 _teec_setup_operation(struct teec_session_imp *session,
				 struct _teec_tci *tci,
				 struct teec_operation *operation,
				 u32 *return_origin)
{
	u32 i;
	union _teec_parameter_internal *imp;
	union teec_parameter *ext;
	u32 teec_result = TEEC_SUCCESS;
	int ret;

	mc_dev_devel(" %s()", __func__);

	tci->operation.is_cancelled = false;
	tci->operation.param_types = 0;

	/* operation can be NULL */
	if (operation) {
		u32 n_buf = 0;
		/* Buffers to map to SWd */
		struct mc_ioctl_map map;
		/* Operation parameters for the buffers above */
		union _teec_parameter_internal *imps[_TEEC_PARAMETER_NUMBER] = {
			NULL
		};
		operation->started = 1;
		map.sid = session->session_id;

		/*
		 * This design allows a non-NULL buffer with a size of 0 bytes
		 * to allow trivial integration with any implementations of the
		 * C library malloc, in which is valid to allocate a zero byte
		 * buffer and receive a non-NULL pointer which may not be
		 * de-referenced in return
		 */
		for (i = 0;
		     i < _TEEC_PARAMETER_NUMBER && teec_result == TEEC_SUCCESS;
		     i++) {
			u8 param_type =
			    _TEEC_GET_PARAM_TYPE(operation->param_types, i);

			imp = &tci->operation.params[i];
			ext = &operation->params[i];

			switch (param_type) {
			case TEEC_VALUE_OUTPUT:
				mc_dev_devel("	cycle %d, TEEC_VALUE_OUTPUT",
					     i);
				break;
			case TEEC_NONE:
				mc_dev_devel("	cycle %d, TEEC_NONE", i);
				break;
			case TEEC_VALUE_INPUT:
			case TEEC_VALUE_INOUT: {
				mc_dev_devel
				    ("	cycle %d, TEEC_VALUE_IN*", i);
				imp->value.a = ext->value.a;
				imp->value.b = ext->value.b;
				break;
				}
			case TEEC_MEMREF_TEMP_INPUT:
			case TEEC_MEMREF_TEMP_OUTPUT:
			case TEEC_MEMREF_TEMP_INOUT:
				/*
				 * A Temporary Memory Reference may be null,
				 * which can be used to denote a special case
				 * for the parameter. Output Memory References
				 * that are null are typically used to request
				 * the required output size
				 */
				mc_dev_devel
				    ("	cycle %d, TEEC_MEMREF_TEMP_*",
				     i);
				if ((ext->tmpref.size) &&
				    (ext->tmpref.buffer)) {
					map.bufs[n_buf].va =
					    (uintptr_t)ext->tmpref.buffer;
					map.bufs[n_buf].len =
					    (u32)ext->tmpref.size;
					map.bufs[n_buf].flags =
					    param_type & TEEC_MEM_INOUT;
					imps[n_buf] = imp;
					n_buf++;
				} else {
		mc_dev_devel("	cycle %d, TEEC_TEMP_IN* - zero pointer or size",
			     i);
				}
				break;
			case TEEC_MEMREF_WHOLE:
				mc_dev_devel
				    ("	cycle %d, TEEC_MEMREF_WHOLE",
				     i);
				if (ext->memref.parent->size) {
					map.bufs[n_buf].va =
					  (uintptr_t)ext->memref.parent->buffer;
					map.bufs[n_buf].len =
					    (u32)ext->memref.parent->size;
					map.bufs[n_buf].flags =
					    ext->memref.parent->flags &
						TEEC_MEM_INOUT;
					map.bufs[n_buf].flags |=
						MC_IO_MAP_PERSISTENT;
					imps[n_buf] = imp;
					n_buf++;
				}
				/*
				 * We don't transmit that the mem ref is the
				 * whole shared mem
				 */
				/* Magic number 4 means that it is a mem ref */
				param_type = (u8)ext->memref.parent->flags | 4;
				break;
			case TEEC_MEMREF_PARTIAL_INPUT:
			case TEEC_MEMREF_PARTIAL_OUTPUT:
			case TEEC_MEMREF_PARTIAL_INOUT:
			mc_dev_devel("	cycle %d, TEEC_MEMREF_PARTIAL_*", i);
				/* Check data flow consistency */
				if ((((ext->memref.parent->flags &
				       TEEC_MEM_INOUT) ==
				      TEEC_MEM_INPUT) &&
				      (param_type ==
				      TEEC_MEMREF_PARTIAL_OUTPUT)) ||
				    (((ext->memref.parent->flags
				       & TEEC_MEM_INOUT) ==
				      TEEC_MEM_OUTPUT) &&
				     (param_type ==
					 TEEC_MEMREF_PARTIAL_INPUT))) {
					mc_dev_notice(
					"PARTIAL data flow inconsistency");
					*return_origin = TEEC_ORIGIN_API;
					teec_result =
					    TEEC_ERROR_BAD_PARAMETERS;
					break;
				}
				/* We don't transmit that mem ref is partial */
				param_type &= TEEC_MEMREF_TEMP_INOUT;

				if (ext->memref.offset +
				    ext->memref.size >
				    ext->memref.parent->size) {
					mc_dev_notice
					    ("PARTIAL offset/size error");
					*return_origin = TEEC_ORIGIN_API;
					teec_result =
					    TEEC_ERROR_BAD_PARAMETERS;
					break;
				}
				if (ext->memref.size) {
					map.bufs[n_buf].va =
					   (uintptr_t)ext->memref.parent->buffer
					    + ext->memref.offset;
					map.bufs[n_buf].len =
					    (u32)ext->memref.size;
					map.bufs[n_buf].flags =
					    param_type & TEEC_MEM_INOUT;
					map.bufs[n_buf].flags |=
						MC_IO_MAP_PERSISTENT;
					imps[n_buf] = imp;
					n_buf++;
				}
				break;
			default:
				mc_dev_notice("cycle %d, default", i);
				*return_origin = TEEC_ORIGIN_API;
				teec_result = TEEC_ERROR_BAD_PARAMETERS;
				break;
			}
			tci->operation.param_types |=
				(u32)(param_type << i * 4);
		}

		if (n_buf > MC_MAP_MAX) {
			mc_dev_notice("too many buffers");
			teec_result = TEEC_ERROR_EXCESS_DATA;
		}

		if ((teec_result == TEEC_SUCCESS) &&
		    (tci->operation.is_cancelled)) {
			mc_dev_notice("the operation has been cancelled in COMMS");
			*return_origin = TEEC_ORIGIN_COMMS;
			teec_result = TEEC_ERROR_CANCEL;
		}

		/* Map buffers */
		if ((teec_result == TEEC_SUCCESS) && (n_buf > 0)) {
			for (i = n_buf; i < MC_MAP_MAX; i++)
				map.bufs[i].va = 0;

			ret = client_map_session_wsms(session->context.client,
						      session->session_id,
						      map.bufs);
			if (!ret) {
				for (i = 0; i < MC_MAP_MAX; i++) {
					if (map.bufs[i].va) {
						imps[i]->memref.sva =
						    (u32)map.bufs[i].sva;
						imps[i]->memref.len =
						    map.bufs[i].len;
					}
				}
			} else {
				mc_dev_notice("client map failed: %d", ret);
				*return_origin = TEEC_ORIGIN_COMMS;
				teec_result = TEEC_ERROR_GENERIC;
			}
		}

		if (teec_result != TEEC_SUCCESS) {
			u32 ret_orig_ignored;

			_teec_unwind_operation(session, tci, operation, false,
					       &ret_orig_ignored);
			/* Zeroing out tci->operation */
			memset(&tci->operation, 0, sizeof(tci->operation));
			return teec_result;
		}
	}

	/* Copy version indicator field */
	memcpy(tci->header, "TCIGP000", sizeof(tci->header));

	/* Fill in invalid values for secure world to overwrite */
	tci->return_status = TEEC_ERROR_BAD_STATE;

	/* Signal completion of request writing */
	tci->ready = 1;

	return teec_result;
}

static u32 _teec_unwind_operation(struct teec_session_imp *session,
				  struct _teec_tci *tci,
				  struct teec_operation *operation,
				  bool copy_values,
				  u32 *return_origin)
{
	u32 i;
	union _teec_parameter_internal *imp;
	union teec_parameter		ext_tmp[_TEEC_PARAMETER_NUMBER];
	u32		n_buf = 0;
	struct mc_ioctl_map	map;
	int			ret;

	/* Operation can be NULL */
	if (!operation)
		return TEEC_SUCCESS;

	mc_dev_devel(" %s()", __func__);

	operation->started = 2;
	/* Buffers to unmap from SWd */
	map.sid = session->session_id;

	memcpy(ext_tmp, operation->params, sizeof(ext_tmp));

	/*
	 * No matter what the return status and return origin are, memory
	 * mappings that were set up at the beginning of the operation need to
	 * be cleaned up at the end of the operation. Clear len to unMap further
	 */
	for (i = 0; i < _TEEC_PARAMETER_NUMBER; i++) {
		imp = &tci->operation.params[i];

		switch (_TEEC_GET_PARAM_TYPE(operation->param_types, i)) {
		case TEEC_VALUE_INPUT:
			mc_dev_devel("	cycle %d, TEEC_VALUE_INPUT", i);
			break;
		case TEEC_NONE:
			mc_dev_devel("	cycle %d, TEEC_NONE", i);
			break;
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT: {
			mc_dev_devel("	cycle %d, TEEC_VALUE_*OUT", i);
			if (copy_values) {
				ext_tmp[i].value.a = imp->value.a;
				ext_tmp[i].value.b = imp->value.b;
			}
			break;
		}
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_INOUT: {
			mc_dev_devel("	cycle %d, TEEC_TEMP*", i);
			if (copy_values &&
			    (_TEEC_GET_PARAM_TYPE(operation->param_types, i) !=
					TEEC_MEMREF_TEMP_INPUT))
				ext_tmp[i].tmpref.size =
					imp->memref.output_size;

			if (imp->memref.len > 0) {
				map.bufs[n_buf].va =
					(uintptr_t)ext_tmp[i].tmpref.buffer;
				map.bufs[n_buf].sva = imp->memref.sva;
				map.bufs[n_buf].len = imp->memref.len;
				n_buf++;
			}
			break;
		}
		case TEEC_MEMREF_WHOLE: {
			mc_dev_devel("	cycle %d, TEEC_MEMREF_WHOLE", i);
			if ((copy_values) &&
			    (ext_tmp[i].memref.parent->flags !=
					TEEC_MEM_INPUT)) {
				ext_tmp[i].memref.size =
					imp->memref.output_size;
			}
			if (imp->memref.len > 0) {
				map.bufs[n_buf].va =
				    (uintptr_t)ext_tmp[i].memref.parent->buffer;
				map.bufs[n_buf].sva = imp->memref.sva;
				map.bufs[n_buf].len = imp->memref.len;
				n_buf++;
			}
			break;
		}
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
		case TEEC_MEMREF_PARTIAL_INPUT: {
			mc_dev_devel("	cycle %d, TEEC_MEMREF_PARTIAL*", i);
			if (copy_values &&
			    (_TEEC_GET_PARAM_TYPE(operation->param_types, i) !=
					TEEC_MEMREF_PARTIAL_INPUT)) {
				ext_tmp[i].memref.size =
					imp->memref.output_size;
			}
			if (imp->memref.len > 0) {
				map.bufs[n_buf].va =
				   (uintptr_t)ext_tmp[i].memref.parent->buffer +
				   ext_tmp[i].memref.offset;
				map.bufs[n_buf].sva = imp->memref.sva;
				map.bufs[n_buf].len = imp->memref.len;
				n_buf++;
			}
			break;
		}
		default:
			mc_dev_notice("cycle %d, bad parameter", i);
		}
	}

	if (n_buf > MC_MAP_MAX) {
		mc_dev_notice("too many buffers");
		*return_origin = TEEC_ERROR_COMMUNICATION;
		return TEEC_ERROR_EXCESS_DATA;
	}

	for (i = n_buf; i < MC_MAP_MAX; i++)
		map.bufs[i].va = 0;

	if (n_buf > 0) {
		/* This function assumes that we cannot handle errors */
		ret = client_unmap_session_wsms(session->context.client,
						session->session_id,
						map.bufs);
		if (ret < 0)
			mc_dev_notice("client unmap failed: %d", ret);
	}

	/* Some sanity checks */
	if (!tci->return_origin ||
	    ((tci->return_origin != TEEC_ORIGIN_TRUSTED_APP) &&
	     (tci->return_status == TEEC_SUCCESS))) {
		*return_origin = TEEC_ORIGIN_COMMS;
		return TEEC_ERROR_COMMUNICATION;
	}

	/* The code to copy parameters out must only be executed if the
	 * application processed the message, i.e. only if
	 * tci->return_origin == TEEC_ORIGIN_TRUSTED_APP, regardless of the
	 * return status
	 */
	if (tci->return_origin == TEEC_ORIGIN_TRUSTED_APP && copy_values)
		memcpy(operation->params, ext_tmp, sizeof(operation->params));

	*return_origin = tci->return_origin;
	return tci->return_status;
}

/* teec_initialize_context: TEEC_SUCCESS, Another error code from Table 4-2 */
u32 teec_initialize_context(const char *name, struct teec_context *context)
{
	struct tee_client *client;
	(void)name;

	mc_dev_devel("== %s() ==============", __func__);

	if (!context) {
		mc_dev_notice("context is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	/* Create client */
	client = client_create(true);
	if (!client)
		return -ENOMEM;

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
		mc_dev_notice("context is NULL");
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

static void _teec_delete_tci(struct teec_session_imp *session_imp)
{
	if (session_imp->tci) {
		free_page((unsigned long)session_imp->tci);
		session_imp->tci = NULL;
	}
}

static void _teec_close_session(struct teec_session_imp *session_imp)
{
	int ret;

	ret = client_remove_session(session_imp->context.client,
				    session_imp->session_id);
	if (ret)
		mc_dev_notice("%s failed: %d", __func__, ret);

	session_imp->active = false;
}

static u32 _teec_call_ta(struct teec_session_imp *session,
			 struct teec_operation *operation,
			 u32 *return_origin)
{
	struct _teec_tci *tci = (struct _teec_tci *)(session->tci);
	u32 teec_res;
	u32 teec_error = TEEC_SUCCESS;
	int ret;

	mc_dev_devel(" %s()", __func__);

	/* Phase 1: start the operation and wait for the result */
	teec_res = _teec_setup_operation(session, tci, operation,
					 return_origin);
	if (teec_res != TEEC_SUCCESS) {
		mc_dev_notice("_teec_setup_operation failed (%08x)", teec_res);
		return teec_res;
	}

	/* Signal the Trusted App */
	ret = client_notify_session(session->context.client,
				    session->session_id);
	if (ret) {
		mc_dev_notice("Notify failed: %d", ret);
		teec_error = TEEC_ERROR_COMMUNICATION;
	} else {
		/* Wait for the Trusted App response */
		ret = client_waitnotif_session(session->context.client,
					       session->session_id, -1, false);
		if (ret) {
			teec_error = TEEC_ERROR_COMMUNICATION;
			if (ret == ECOMM) {
				s32 exit_code = ERR_INVALID_SID;

				client_get_session_exitcode(
					session->context.client,
					session->session_id,
					&exit_code);
				switch (exit_code) {
				case TA_EXIT_CODE_FINISHED:
					/*
					 * We may get here if the
					 * TA_OpenSessionEntryPoint returns an
					 * error and TA goes fast through
					 * DestroyEntryPoint and exits the TA
					 */
					teec_error = TEEC_SUCCESS;
					break;
				case ERR_SESSION_KILLED:
					teec_error = TEEC_ERROR_TARGET_KILLED;
					break;
				case ERR_INVALID_SID:
				case ERR_SID_NOT_ACTIVE:
					mc_dev_notice
					    ("mcWaitNotification failed: %d",
					     ret);
					mc_dev_notice
					  ("mcGetSessionErrorCode returned %d",
					   exit_code);
					break;
				default:
					mc_dev_notice("Target is DEAD");
					*return_origin = TEEC_ORIGIN_TEE;
					teec_error = TEEC_ERROR_TARGET_DEAD;
					break;
				}
			}
		}
	}

	/*
	 * Phase 2: Return values and cleanup unmap memory and copy values if no
	 * error
	 */
	teec_res = _teec_unwind_operation(session, tci, operation,
					  (teec_error == TEEC_SUCCESS),
					  return_origin);
	if (teec_res != TEEC_SUCCESS)
		/* continue even in case of error */
		mc_dev_notice("_teec_unwind_operation (%08x)", teec_res);

	/* Cleanup */
	if (teec_error != TEEC_SUCCESS) {
		if (teec_error == TEEC_ERROR_COMMUNICATION)
			*return_origin = TEEC_ORIGIN_COMMS;

		/*
		 * Previous interactions failed, either TA is dead or
		 * communication error
		 */
		_teec_close_session(session);
		_teec_delete_tci(session);
	}
	return teec_error;
}

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
	u32 teec_res;
	u32 return_origin_local = TEEC_ORIGIN_API;
	struct mc_uuid_t tauuid;
	int ret = 0;
	void *bulk_buf;
	int timeout;
	struct _teec_tci *tci;
	struct mc_identity identity;

	mc_dev_devel("== %s() ==============", __func__);
	/* The parameter context MUST point to an initialized TEE Context */
	if (!context) {
		mc_dev_notice("context is NULL");
		if (return_origin)
			*return_origin = TEEC_ORIGIN_API;

		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (!context->imp.client) {
		mc_dev_notice("context not initialized");
		if (return_origin)
			*return_origin = TEEC_ORIGIN_API;

		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (!session) {
		mc_dev_notice("session is NULL");
		if (return_origin)
			*return_origin = TEEC_ORIGIN_API;

		return TEEC_ERROR_BAD_PARAMETERS;
	}

	connection_method = TEEC_LOGIN_KERNEL;
	session->imp.active = false;
	_lib_uuid_to_array(destination, tauuid.value);
	if (operation)
		operation->imp.session = &session->imp;

	/*
	 * Allocate a 4kB page filled with zero, and set session->imp.tci to its
	 * address
	 */
	session->imp.tci = NULL;
	bulk_buf = (void *)get_zeroed_page(GFP_KERNEL);
	if (!bulk_buf) {
		mc_dev_notice("get_zeroed_page failed on tci buffer allocation");
		if (return_origin)
			*return_origin = TEEC_ORIGIN_API;

		return TEEC_ERROR_OUT_OF_MEMORY;
	}

	session->imp.tci = bulk_buf;

	mutex_init(&session->imp.mutex_tci);
	mutex_lock(&session->imp.mutex_tci);

	/* Fill the TCI buffer session.tci with the destination UUID */
	tci = (struct _teec_tci *)(session->imp.tci);
	memcpy(&tci->destination, destination, sizeof(tci->destination));

	identity.login_type = (enum mc_login_type)connection_method;

	/* Wait for GP loading to be possible, maximum 30s */
	timeout = 30;
	do {
		ret = client_open_session(context->imp.client,
					  &session->imp.session_id, &tauuid,
					  (uintptr_t)tci,
					  sizeof(struct _teec_tci), true,
					  &identity, 0, 0);
		if (!ret || (ret != EAGAIN))
			break;

		msleep(1000);
	} while (--timeout);

	if (ret) {
		mc_dev_notice("%s failed: %d", __func__, ret);
		if (return_origin)
			*return_origin = TEEC_ORIGIN_COMMS;

		switch (ret) {
		case ENOENT:
			teec_res = TEEC_ERROR_ITEM_NOT_FOUND;
			break;
		case EACCES:
			teec_res = TEEC_ERROR_ACCESS_DENIED;
			break;
		case EINVAL:
			teec_res = TEEC_ERROR_NOT_IMPLEMENTED;
			break;
		case ENOSPC:
			teec_res = TEEC_ERROR_OUT_OF_MEMORY;
			break;
		case ECONNREFUSED:
			teec_res = TEEC_ERROR_SD_BLOCKED;
			break;
		case ECONNABORTED:
			teec_res = TEEC_ERROR_TA_LOCKED;
			break;
		case ECONNRESET:
			teec_res = TEEC_ERROR_TARGET_KILLED;
			break;
		case EBUSY:
			teec_res = TEEC_ERROR_BUSY;
			break;
		case EKEYREJECTED:
			teec_res = TEEC_ERROR_SECURITY;
			break;
		default:
			teec_res = TEEC_ERROR_GENERIC;
		}
		goto error;
	}

	session->imp.context = context->imp;
	session->imp.active = true;
	mc_dev_info(" created session ID %x", session->imp.session_id);

	/* Let TA go through entry points */
	mc_dev_devel(" let TA go through entry points");
	tci->operation.type = _TA_OPERATION_OPEN_SESSION;
	teec_res = _teec_call_ta(&session->imp, operation,
				 &return_origin_local);

	/* Check for error on communication level */
	if (teec_res != TEEC_SUCCESS) {
		mc_dev_notice("_teec_call_ta failed(%08x)", teec_res);
		/*
		 * Nothing to do here because _teec_call_ta closes broken
		 * sessions
		 */
		if (return_origin)
			*return_origin = return_origin_local;

		goto error;
	}
	mc_dev_devel(" no errors in com layer");

	/* Check for error from TA */
	if (return_origin)
		*return_origin = tci->return_origin;

	teec_res = tci->return_status;
	if (teec_res != TEEC_SUCCESS) {
		mc_dev_notice("TA OpenSession EP failed(%08x)", teec_res);
		goto error;
	}

	mc_dev_devel(" %s() = TEEC_SUCCESS ", __func__);
	mutex_unlock(&session->imp.mutex_tci);

	if (return_origin)
		*return_origin = TEEC_ORIGIN_TRUSTED_APP;

	return TEEC_SUCCESS;

error:
	if (session->imp.active) {
		/*
		 * After notifying us, TA went to destroy EP, so close session
		 * now
		 */
		_teec_close_session(&session->imp);
	}

	mutex_unlock(&session->imp.mutex_tci);
	_teec_delete_tci(&session->imp);
	mc_dev_devel(" %s() = 0x%x", __func__, teec_res);
	return teec_res;
}
EXPORT_SYMBOL(teec_open_session);

u32 teec_invoke_command(struct teec_session *session,
			u32 command_id,
			struct teec_operation *operation,
			u32 *return_origin)
{
	u32 teec_res;
	u32 return_origin_local = TEEC_ORIGIN_API;
	struct _teec_tci *tci;

	mc_dev_devel("== %s() ==============", __func__);

	if (!session) {
		mc_dev_notice("session is NULL");
		if (return_origin)
			*return_origin = TEEC_ORIGIN_API;

		return TEEC_ERROR_BAD_PARAMETERS;
	}

	if (!session->imp.active) {
		mc_dev_notice("session is inactive");
		if (return_origin)
			*return_origin = TEEC_ORIGIN_API;

		return TEEC_ERROR_BAD_STATE;
	}

	if (operation)
		operation->imp.session = &session->imp;

	mutex_lock(&session->imp.mutex_tci);

	/* Call TA */
	tci = (struct _teec_tci *)(session->imp.tci);
	tci->operation.command_id = command_id;
	tci->operation.type = _TA_OPERATION_INVOKE_COMMAND;
	teec_res = _teec_call_ta(&session->imp, operation,
				 &return_origin_local);
	if (teec_res != TEEC_SUCCESS) {
		mc_dev_notice("_teec_call_ta failed(%08x)", teec_res);
		if (return_origin)
			*return_origin = return_origin_local;

	} else {
		if (return_origin)
			*return_origin = tci->return_origin;

		teec_res = tci->return_status;
	}

	mutex_unlock(&session->imp.mutex_tci);
	mc_dev_devel(" %s() = 0x%x", __func__, teec_res);
	return teec_res;
}
EXPORT_SYMBOL(teec_invoke_command);

void teec_close_session(struct teec_session *session)
{
	u32 teec_res = TEEC_SUCCESS;
	u32 return_origin;
	struct _teec_tci *tci;

	mc_dev_devel("== %s() ==============", __func__);

	/* The implementation MUST do nothing if session is NULL */
	if (!session) {
		mc_dev_notice("session is NULL");
		return;
	}

	if (session->imp.active) {
		/* Let TA go through CloseSession and Destroy entry points */
		mc_dev_devel(" let TA go through close entry points");
		mutex_lock(&session->imp.mutex_tci);
		tci = (struct _teec_tci *)(session->imp.tci);
		tci->operation.type = _TA_OPERATION_CLOSE_SESSION;
		teec_res = _teec_call_ta(&session->imp, NULL, &return_origin);
		if (teec_res != TEEC_SUCCESS)
			/* continue even in case of error */
			mc_dev_notice("_teec_call_ta failed(%08x)", teec_res);

		if (session->imp.active)
			_teec_close_session(&session->imp);

		mutex_unlock(&session->imp.mutex_tci);
	}

	_teec_delete_tci(&session->imp);

	mc_dev_devel(" %s() = 0x%x", __func__, teec_res);
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
	mc_dev_devel("== %s() ==============", __func__);

	/* The parameter context MUST point to an initialized TEE Context */
	if (!context) {
		mc_dev_notice("context is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	/*
	 * The parameter shared_mem MUST point to the Shared Memory structure
	 * defining the memory region to register
	 */
	if (!shared_mem) {
		mc_dev_notice("shared_mem is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	/*
	 * The buffer field MUST point to the memory region to be shared,
	 * and MUST not be NULL
	 */
	if (!shared_mem->buffer) {
		mc_dev_notice("shared_mem->buffer is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	if (shared_mem->flags & ~TEEC_MEM_INOUT) {
		mc_dev_notice("shared_mem->flags is incorrect");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	if (!shared_mem->flags) {
		mc_dev_notice("shared_mem->flags is incorrect");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	shared_mem->imp.implementation_allocated = false;
	return TEEC_SUCCESS;
}
EXPORT_SYMBOL(teec_register_shared_memory);

u32 teec_allocate_shared_memory(struct teec_context *context,
				struct teec_shared_memory *shared_mem)
{
	/* No connection to "context"? */
	mc_dev_devel("== %s() ==============", __func__);

	/* The parameter context MUST point to an initialized TEE Context */
	if (!context) {
		mc_dev_notice("context is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	/*
	 * The parameter shared_mem MUST point to the Shared Memory structure
	 * defining the memory region to register
	 */
	if (!shared_mem) {
		mc_dev_notice("shared_mem is NULL");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	if (shared_mem->flags & ~TEEC_MEM_INOUT) {
		mc_dev_notice("shared_mem->flags is incorrect");
		return TEEC_ERROR_BAD_PARAMETERS;
	}
	if (!shared_mem->flags) {
		mc_dev_notice("shared_mem->flags is incorrect");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	shared_mem->buffer = vmalloc(shared_mem->size);
	if (!shared_mem->buffer)
		return TEEC_ERROR_OUT_OF_MEMORY;

	shared_mem->imp.implementation_allocated = true;

	return TEEC_SUCCESS;
}
EXPORT_SYMBOL(teec_allocate_shared_memory);

void teec_release_shared_memory(struct teec_shared_memory *shared_mem)
{
	/* No connection to "context"? */
	mc_dev_devel("== %s() ==============", __func__);

	/* The implementation MUST do nothing if shared_mem is NULL */
	if (!shared_mem) {
		mc_dev_notice("shared_mem is NULL");
		return;
	}

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
	struct _teec_tci *tci;
	int ret;

	mc_dev_devel("== %s() ==============", __func__);

	while (!operation->started)
		;

	mc_dev_devel("while(operation->started ==0) passed");

	if (operation->started > 1) {
		mc_dev_devel("The operation has finished");
		return;
	}

	session = operation->imp.session;
	operation->started = 2;

	if (!session->active) {
		mc_dev_devel("Corresponding session is not active");
		return;
	}

	tci = (struct _teec_tci *)(session->tci);
	tci->operation.is_cancelled = true;

	/* Signal the Trustlet */
	ret = client_notify_session(session->context.client,
				    session->session_id);
	if (ret)
		mc_dev_notice("Notify failed: %d", ret);
}
EXPORT_SYMBOL(teec_request_cancellation);
