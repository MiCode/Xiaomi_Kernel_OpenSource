/*
 * Copyright (c) 2013-2018 TRUSTONIC LIMITED
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

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/completion.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/of_irq.h>
#include <linux/freezer.h>
#include <asm/barrier.h>
#include <linux/irq.h>

#include "public/GP/tee_client_api.h"	/* GP error codes/origins FIXME move */
#include "public/mc_user.h"
#include "public/mc_admin.h"

#include "mci/mcimcp.h"
#include "mci/mcifc.h"
#include "mci/mcinq.h"		/* SID_MCP */
#include "mci/mcitime.h"	/* struct mcp_time */
#include "mci/mciiwp.h"

#include "platform.h"		/* IRQ number */
#include "main.h"
#include "fastcall.h"
#include "logging.h"
#include "nq.h"
#include "iwp.h"

#define IWP_RETRIES		5
#define IWP_TIMEOUT		10
#define INVALID_IWS_SLOT	0xFFFFFFFF

/* Macros */
#define _TEEC_GET_PARAM_TYPE(t, i) (((t) >> (4 * (i))) & 0xF)

/* Parameter number */
#define _TEEC_PARAMETER_NUMBER	4

struct iws {
	struct list_head list;
	u32 offset;
};

static struct {
	struct interworld_session *iws;
	/* Interworld lists lock */
	struct mutex		iws_list_lock;
	/* InterWorld lists */
	struct iws		*iws_list_pool;
	struct list_head	free_iws;
	struct list_head	allocd_iws;
	/* Sessions */
	struct mutex		sessions_lock;
	struct list_head	sessions;
} l_ctx;

static void iwp_notif_handler(u32 id, u32 payload)
{
	struct iwp_session *iwp_session = NULL, *candidate;

	mutex_lock(&l_ctx.sessions_lock);
	list_for_each_entry(candidate, &l_ctx.sessions, list) {
		mc_dev_devel("candidate->slot [%08x]",
			     candidate->slot);
		/* If id is SID_CANCEL_OPERATION, there is pseudo session */
		if (candidate->slot == payload &&
		    (id != SID_CANCEL_OPERATION || candidate->sid == id)) {
			iwp_session = candidate;
			break;
		}
	}
	mutex_unlock(&l_ctx.sessions_lock);

	if (!iwp_session) {
		mc_dev_notice("IWP no session found for id=0x%x slot=0x%x",
			   id, payload);
		return;
	}

	mc_dev_devel("IWP: iwp_session [%p] id [%08x] slot [%08x]",
		     iwp_session, id, payload);
	complete(&iwp_session->completion);
}

void iwp_session_init(struct iwp_session *iwp_session,
		      const struct identity *identity)
{
	nq_session_init(&iwp_session->nq_session, true);
	iwp_session->sid = SID_INVALID;
	iwp_session->slot = (u32)-1;		/* 0 is a valid slot */
	INIT_LIST_HEAD(&iwp_session->list);
	mutex_init(&iwp_session->notif_wait_lock);
	init_completion(&iwp_session->completion);
	mutex_init(&iwp_session->exit_code_lock);
	iwp_session->exit_code = 0;
	mutex_init(&iwp_session->iws_lock);
	iwp_session->state = IWP_SESSION_RUNNING;
	if (identity)
		iwp_session->client_identity = *identity;
}

static u32 iws_slot_get(void)
{
	struct iws *iws;
	u32 ret = INVALID_IWS_SLOT;

	mutex_lock(&l_ctx.iws_list_lock);
	if (!list_empty(&l_ctx.free_iws)) {
		iws = list_first_entry(&l_ctx.free_iws, struct iws, list);
		ret = iws->offset;
		list_move(&iws->list, &l_ctx.allocd_iws);
		atomic_inc(&g_ctx.c_slots);
	}
	mutex_unlock(&l_ctx.iws_list_lock);
	return ret;
}

/* Passing INVALID_IWS_SLOT is supported */
static void iws_slot_put(u32 offset)
{
	struct iws *iws;

	mutex_lock(&l_ctx.iws_list_lock);
	list_for_each_entry(iws, &l_ctx.allocd_iws, list) {
		if (offset == iws->offset) {
			list_move(&iws->list, &l_ctx.free_iws);
			atomic_dec(&g_ctx.c_slots);
			break;
		}
	}
	mutex_unlock(&l_ctx.iws_list_lock);
}

static inline struct interworld_session *slot_to_iws(u32 slot)
{
	return (struct interworld_session *)((uintptr_t)l_ctx.iws + slot);
}

/*
 * IWP command functions
 */
static int iwp_cmd(struct iwp_session *iwp_session, u32 id)
{
	struct completion *completion;
	int ret;

	mc_dev_devel("psid [%08x], sid [%08x]", id, iwp_session->sid);
	ret = nq_session_notify(&iwp_session->nq_session, id,
				iwp_session->slot);
	if (ret) {
		mc_dev_notice("sid [%08x]: sending failed, ret = %d",
			   iwp_session->sid, ret);
		return ret;
	}

	completion = &iwp_session->completion;

	/*
	 * NB: Wait cannot be interruptible as we need an answer from SWd. It's
	 * up to the user-space to request a cancellation (for open session and
	 * command invocation operations.)
	 */
	wait_for_completion(completion);
	return 0;
}

/*
 * Convert errno into GP error and set origin to COMMS.
 * Note: -ECHILD is used to tell the caller that we have a GP error in value, so
 * we return 0 on success and -ECHILD on error. If -ECHILD is given, we assume
 * that value is already correctly set.
 */
int iwp_set_ret(int ret, struct gp_return *gp_ret)
{
	if (ret == -ECHILD) {
		/* Already set */
		return ret;
	}

	gp_ret->origin = TEEC_ORIGIN_COMMS;
	switch (ret) {
	case 0:
		gp_ret->origin = TEEC_ORIGIN_TRUSTED_APP;
		gp_ret->value = TEEC_SUCCESS;
		return 0;
	case -EACCES:
		gp_ret->value = TEEC_ERROR_ACCESS_DENIED;
		break;
	case -EBUSY:
		gp_ret->value = TEEC_ERROR_BUSY;
		break;
	case -ECANCELED:
		gp_ret->value = TEEC_ERROR_CANCEL;
		break;
	case -EINVAL:
	case -EFAULT:
		gp_ret->value = TEEC_ERROR_BAD_PARAMETERS;
		break;
	case -EKEYREJECTED:
		gp_ret->value = TEEC_ERROR_SECURITY;
		break;
	case -ENOENT:
		gp_ret->value = TEEC_ERROR_ITEM_NOT_FOUND;
		break;
	case -ENOMEM:
		gp_ret->value = TEEC_ERROR_OUT_OF_MEMORY;
		break;
	case -ENXIO:
		/* Session not found or not running */
		gp_ret->value = TEEC_ERROR_BAD_STATE;
		break;
	default:
		gp_ret->value = TEEC_ERROR_GENERIC;
	}
	return -ECHILD;
}

int iwp_register_shared_mem(struct mcp_buffer_map *map,
			    struct gp_return *gp_ret)
{
	int ret = 0;

	ret = mcp_map(SID_MEMORY_REFERENCE, map);
	/* iwp_set_ret would override the origin if called after */
	ret = iwp_set_ret(ret, gp_ret);
	if (ret)
		gp_ret->origin = TEEC_ORIGIN_TEE;

	return ret;
}

int iwp_release_shared_mem(struct mcp_buffer_map *map)
{
	return mcp_unmap(SID_MEMORY_REFERENCE, map);
}

static int iwp_operation_to_iws(struct gp_operation *operation,
				struct interworld_session *iws,
				struct mc_ioctl_buffer *bufs,
				struct gp_shared_memory **parents)
{
	int param_type, i;

	iws->param_types = 0;
	for (i = 0; i < _TEEC_PARAMETER_NUMBER; i++) {
		/* Reset reference for temporary memory */
		bufs[i].va = 0;
		/* Reset reference for registered memory */
		parents[i] = NULL;
		param_type = _TEEC_GET_PARAM_TYPE(operation->param_types, i);

		switch (param_type) {
		case TEEC_NONE:
		case TEEC_VALUE_OUTPUT:
			break;
		case TEEC_VALUE_INPUT:
		case TEEC_VALUE_INOUT:
			iws->params[i].value.a = operation->params[i].value.a;
			iws->params[i].value.b = operation->params[i].value.b;
			break;
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			if (operation->params[i].tmpref.buffer) {
				/* Prepare buffer to map */
				bufs[i].va = operation->params[i].tmpref.buffer;
				bufs[i].len = operation->params[i].tmpref.size;
				if (param_type == TEEC_MEMREF_TEMP_INPUT)
					bufs[i].flags = MC_IO_MAP_INPUT;
				else if (param_type == TEEC_MEMREF_TEMP_OUTPUT)
					bufs[i].flags = MC_IO_MAP_OUTPUT;
				else
					bufs[i].flags = MC_IO_MAP_INPUT_OUTPUT;
			} else {
				if (operation->params[i].tmpref.size)
					return -EINVAL;

				/* Null buffer, won't get mapped */
				iws->params[i].tmpref.physical_address = 0;
				iws->params[i].tmpref.size = 0;
				iws->params[i].tmpref.offset = 0;
				iws->params[i].tmpref.wsm_type = WSM_INVALID;
			}
			break;
		case TEEC_MEMREF_WHOLE:
			parents[i] = &operation->params[i].memref.parent;
			iws->params[i].memref.offset = 0;
			iws->params[i].memref.size =
				operation->params[i].memref.parent.size;
			break;
		case TEEC_MEMREF_PARTIAL_INPUT:
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
			parents[i] = &operation->params[i].memref.parent;
			iws->params[i].memref.offset =
				operation->params[i].memref.offset;
			iws->params[i].memref.size =
				operation->params[i].memref.size;
			break;
		default:
			return -EINVAL;
		}

		iws->param_types |= (u32)(param_type << (i * 4));
	}

	return 0;
}

static inline void iwp_iws_set_tmpref(struct interworld_session *iws, int i,
				      const struct mcp_buffer_map *map)
{
	iws->params[i].tmpref.physical_address = map->phys_addr;
	iws->params[i].tmpref.size = map->length;
	iws->params[i].tmpref.offset = map->offset;
	iws->params[i].tmpref.wsm_type = map->type;
}

static inline void iwp_iws_set_memref(struct interworld_session *iws, int i,
				      u32 sva)
{
	iws->params[i].memref.memref_handle = sva;
}

static inline void iwp_iws_set_refs(struct interworld_session *iws,
				    const struct iwp_buffer_map *maps)
{
	int i;

	for (i = 0; i < _TEEC_PARAMETER_NUMBER; i++)
		if (maps[i].sva)
			iwp_iws_set_memref(iws, i, maps[i].sva);
		else if (maps[i].map.phys_addr)
			iwp_iws_set_tmpref(iws, i, &maps[i].map);
}

static void iwp_iws_to_operation(const struct interworld_session *iws,
				 struct gp_operation *operation)
{
	int i;

	for (i = 0; i < _TEEC_PARAMETER_NUMBER; i++) {
		switch (_TEEC_GET_PARAM_TYPE(operation->param_types, i)) {
		case TEEC_VALUE_OUTPUT:
		case TEEC_VALUE_INOUT:
			operation->params[i].value.a = iws->params[i].value.a;
			operation->params[i].value.b = iws->params[i].value.b;
			break;
		case TEEC_MEMREF_TEMP_OUTPUT:
		case TEEC_MEMREF_TEMP_INOUT:
			operation->params[i].tmpref.size =
				iws->params[i].tmpref.size;
			break;
		case TEEC_MEMREF_WHOLE:
			if (operation->params[i].memref.parent.flags !=
			    TEEC_MEM_INPUT)
				operation->params[i].memref.size =
					iws->params[i].tmpref.size;
			break;
		case TEEC_MEMREF_PARTIAL_OUTPUT:
		case TEEC_MEMREF_PARTIAL_INOUT:
			operation->params[i].memref.size =
				iws->params[i].tmpref.size;
			break;
		case TEEC_NONE:
		case TEEC_VALUE_INPUT:
		case TEEC_MEMREF_TEMP_INPUT:
		case TEEC_MEMREF_PARTIAL_INPUT:
			break;
		default:
			/* Error caught by iwp_operation_to_iws() */
			break;
		}
	}
}

static inline void mcuuid_to_tee_uuid(const struct mc_uuid_t *in,
				      struct teec_uuid *out)
{
	/*
	 * Warning: this code works only on little-endian platforms.
	 */
	out->time_low = in->value[3] +
		(in->value[2] << 8) +
		(in->value[1] << 16) +
		(in->value[0] << 24);
	out->time_mid = in->value[5] +
		(in->value[4] << 8);
	out->time_hi_and_version = in->value[7] +
		(in->value[6] << 8);
	memcpy(out->clock_seq_and_node, in->value + 8, 8);
}

int iwp_open_session_prepare(
	struct iwp_session *iwp_session,
	const struct tee_object *obj,
	struct gp_operation *operation,
	struct mc_ioctl_buffer *bufs,
	struct gp_shared_memory **parents,
	struct gp_return *gp_ret)
{
	struct interworld_session *iws;
	union mclf_header *header;
	u32 slot, temp_slot;
	int ret;

	/* Get session final slot */
	slot = iws_slot_get();
	mc_dev_devel("slot [%08x]", slot);
	if (slot == INVALID_IWS_SLOT) {
		mc_dev_notice("can't get slot");
		return iwp_set_ret(-ENOMEM, gp_ret);
	}

	/* Get session temporary slot */
	temp_slot = iws_slot_get();
	mc_dev_devel("temp_slot [%08x]", temp_slot);
	if (temp_slot == INVALID_IWS_SLOT) {
		mc_dev_notice("can't get temp_slot");
		iws_slot_put(slot);
		return iwp_set_ret(-ENOMEM, gp_ret);
	}

	mutex_lock(&iwp_session->iws_lock);

	/* Prepare final session: refer to temporary slot in final one */
	iwp_session->slot = slot;
	iws = slot_to_iws(slot);
	memset(iws, 0, sizeof(*iws));
	iws->command_id = temp_slot;

	/* Prepare temporary session */
	iws = slot_to_iws(temp_slot);
	memset(iws, 0, sizeof(*iws));
	header = (union mclf_header *)(obj->data + obj->header_length);
	mcuuid_to_tee_uuid(&header->mclf_header_v2.uuid, &iws->target_uuid);
	iws->login = iwp_session->client_identity.login_type;
	mc_dev_devel("iws->login [%08x]", iws->login);
	memcpy(&iws->client_uuid, iwp_session->client_identity.login_data,
	       sizeof(iws->client_uuid));
	ret = iwp_operation_to_iws(operation, iws, bufs, parents);
	if (ret)
		iwp_open_session_abort(iwp_session);

	return iwp_set_ret(ret, gp_ret);
}

void iwp_open_session_abort(struct iwp_session *iwp_session)
{
	struct interworld_session *iws = slot_to_iws(iwp_session->slot);

	iws_slot_put(iws->command_id);
	iws_slot_put(iwp_session->slot);
	mutex_unlock(&iwp_session->iws_lock);
}

/*
 * Like open session except we pass the TA blob from NWd to SWd
 */
int iwp_open_session(
	struct iwp_session *iwp_session,
	struct gp_operation *operation,
	struct mcp_buffer_map *ta_map,
	const struct iwp_buffer_map *maps,
	struct gp_return *gp_ret)
{
	struct interworld_session *iws = slot_to_iws(iwp_session->slot);
	/* command_id is zero'd by the SWd */
	u32 temp_slot = iws->command_id;
	struct interworld_session *temp_iws = slot_to_iws(temp_slot);
	int ret;

	/* Put ingoing operation in temporary IWS */
	iwp_iws_set_refs(temp_iws, maps);

	/* TA blob handling */
	iws->param_types = TEEC_MEMREF_TEMP_INPUT;
	iws->session_handle = 0;
	iwp_iws_set_tmpref(iws, 0, ta_map);

	mc_dev_devel("wsm_type [%04x], offset [%04x]", ta_map->type,
		     ta_map->offset);
	mc_dev_devel("size [%08x], physical_address [%08x %08x]",
		     ta_map->length, (u32)(ta_map->phys_addr >> 32),
		     (u32)(ta_map->phys_addr & 0xFFFFFFFF));

	/* Add to local list of sessions so we can receive the notification */
	mutex_lock(&l_ctx.sessions_lock);
	list_add_tail(&iwp_session->list, &l_ctx.sessions);
	mutex_unlock(&l_ctx.sessions_lock);

	/* Send IWP open command */
	ret = iwp_cmd(iwp_session, SID_OPEN_TA);
	/* Temporary slot is not needed any more */
	iws_slot_put(temp_slot);
	/* Treat remote errors as errors, just use a specific errno */
	if (!ret && iws->status != TEEC_SUCCESS) {
		gp_ret->origin = iws->return_origin;
		gp_ret->value = iws->status;
		ret = -ECHILD;
	}

	if (!ret) {
		/* set unique identifier for list search */
		iwp_session->sid = iws->session_handle;
		/* Get outgoing operation from main IWS */
		iwp_iws_to_operation(iws, operation);
	} else {
		/* Remove from list of sessions */
		mutex_lock(&l_ctx.sessions_lock);
		list_del(&iwp_session->list);
		mutex_unlock(&l_ctx.sessions_lock);
		iws_slot_put(iwp_session->slot);
		mc_dev_devel("failed with ret [%08x]", ret);
	}

	mutex_unlock(&iwp_session->iws_lock);
	return iwp_set_ret(ret, gp_ret);
}

static void iwp_session_release(
	struct iwp_session *iwp_session)
{
	iwp_session->state = IWP_SESSION_CLOSED;

	/* Remove from list of sessions */
	mutex_lock(&l_ctx.sessions_lock);
	list_del(&iwp_session->list);
	mutex_unlock(&l_ctx.sessions_lock);

	nq_session_exit(&iwp_session->nq_session);
	iws_slot_put(iwp_session->slot);
}

/*
 * Legacy and GP TAs close differently:
 * - GP TAs always send a notification with payload, whether on close or crash
 * - GP TAs may take time to close
 */
int iwp_close_session(
	struct iwp_session *iwp_session)
{
	int ret;

	/* state is either IWP_SESSION_RUNNING or IWP_SESSION_CLOSING_GP */
	mutex_lock(&iwp_session->iws_lock);
	if (iwp_session->state == IWP_SESSION_RUNNING)
		iwp_session->state = IWP_SESSION_CLOSE_REQUESTED;

	/* Send IWP open command */
	ret = iwp_cmd(iwp_session, SID_CLOSE_SESSION);
	mutex_unlock(&iwp_session->iws_lock);
	iwp_session_release(iwp_session);
	mc_dev_devel("close session %x ret %d state %d", iwp_session->sid,
		     ret, iwp_session->state);
	return ret;
}

int iwp_invoke_command_prepare(
	struct iwp_session *iwp_session,
	u32 command_id,
	struct gp_operation *operation,
	struct mc_ioctl_buffer *bufs,
	struct gp_shared_memory **parents,
	struct gp_return *gp_ret)
{
	struct interworld_session *iws;
	int ret;

	if (iwp_session->state != IWP_SESSION_RUNNING)
		return iwp_set_ret(-EBADFD, gp_ret);

	mutex_lock(&iwp_session->iws_lock);
	iws = slot_to_iws(iwp_session->slot);
	memset(iws, 0, sizeof(*iws));
	iws->session_handle = iwp_session->sid;
	iws->command_id = command_id;
	ret = iwp_operation_to_iws(operation, iws, bufs, parents);
	if (ret)
		iwp_invoke_command_abort(iwp_session);

	return iwp_set_ret(ret, gp_ret);
}

void iwp_invoke_command_abort(
	struct iwp_session *iwp_session)
{
	mutex_unlock(&iwp_session->iws_lock);
}

int iwp_invoke_command(
	struct iwp_session *iwp_session,
	struct gp_operation *operation,
	const struct iwp_buffer_map *maps,
	struct gp_return *gp_ret)
{
	struct interworld_session *iws = slot_to_iws(iwp_session->slot);
	int ret = 0;

	/* Update IWS with operation maps */
	iwp_iws_set_refs(iws, maps);
	ret = iwp_cmd(iwp_session, SID_INVOKE_COMMAND);
	/* Treat remote errors as errors, just use a specific errno */
	if (!ret && iws->status != TEEC_SUCCESS)
		ret = -ECHILD;

	iwp_iws_to_operation(iws, operation);
	if (ret && (ret != -ECHILD)) {
		ret = iwp_set_ret(ret, gp_ret);
		mc_dev_devel("failed with ret [%08x]", ret);
	} else {
		gp_ret->origin = iws->return_origin;
		gp_ret->value = iws->status;
	}
	mutex_unlock(&iwp_session->iws_lock);
	return ret;
}

int iwp_request_cancellation(
	u32 slot)
{
	/* Pseudo IWP session for cancellation */
	struct iwp_session iwp_session;
	int ret;

	iwp_session_init(&iwp_session, NULL);
	/* sid is local. Set is to SID_CANCEL_OPERATION to make things clear */
	iwp_session.sid = SID_CANCEL_OPERATION;
	iwp_session.slot = slot;
	mutex_lock(&l_ctx.sessions_lock);
	list_add_tail(&iwp_session.list, &l_ctx.sessions);
	mutex_unlock(&l_ctx.sessions_lock);
	ret = iwp_cmd(&iwp_session, SID_CANCEL_OPERATION);
	mutex_lock(&l_ctx.sessions_lock);
	list_del(&iwp_session.list);
	mutex_unlock(&l_ctx.sessions_lock);
	return ret;
}

int iwp_init(void)
{
	int i;
#ifdef TA2TA_READY
	u32 first_iws = INVALID_IWS_SLOT;
#endif /*TA2TA_READY*/

	l_ctx.iws = nq_get_iwp_buffer();
	INIT_LIST_HEAD(&l_ctx.free_iws);
	INIT_LIST_HEAD(&l_ctx.allocd_iws);
	l_ctx.iws_list_pool = kcalloc(MAX_IW_SESSION, sizeof(struct iws),
				      GFP_KERNEL);
	if (!l_ctx.iws_list_pool)
		return -ENOMEM;

#ifndef TA2TA_READY
	for (i = 0; i < MAX_IW_SESSION; i++) {
		l_ctx.iws_list_pool[i].offset =
			i * sizeof(struct interworld_session);
		list_add(&l_ctx.iws_list_pool[i].list, &l_ctx.free_iws);
	}
#else
	for (i = MAX_IW_SESSION - 1; i >= 0; i--) {
		l_ctx.iws_list_pool[i].offset =
			i * sizeof(struct interworld_session);
		list_add(&l_ctx.iws_list_pool[i].list, &l_ctx.free_iws);
	}
#endif /*TA2TA_READY*/

	mutex_init(&l_ctx.iws_list_lock);
	INIT_LIST_HEAD(&l_ctx.sessions);
	mutex_init(&l_ctx.sessions_lock);

#ifdef TA2TA_READY
	/* allocate special session for request from SWd */
	first_iws = iws_slot_get();
	if (first_iws != 0) {
		mc_dev_notice("first_iws [%08x]", first_iws);
		return -ERANGE;
	}
#endif /*TA2TA_READY*/

	nq_register_notif_handler(iwp_notif_handler, true);
	return 0;
}

void iwp_exit(void)
{
}

int iwp_start(void)
{
	return 0;
}

void iwp_stop(void)
{
}
