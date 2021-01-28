// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2013-2017 TRUSTONIC LIMITED
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
#include <linux/version.h>
#include <linux/sched/clock.h>	/* local_clock */

#include "public/GP/tee_client_api.h"	/* GP error codes/origins FIXME move */
#include "public/mc_user.h"
#include "public/mc_admin.h"

#include "mci/mcimcp.h"
#include "mci/mcifc.h"
#include "mci/mcinq.h"		/* SID_MCP */
#include "mci/mcitime.h"	/* struct mcp_time */
#include "mci/mciiwp.h"

#include "main.h"
#include "admin.h"              /* tee_object* for 'blob' */
#include "mmu.h"                /* MMU for 'blob' */
#include "nq.h"
#include "protocol_common.h"
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
	u64 slot;
};

static struct {
	bool			iwp_dead;
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
	/* TEE bad state detection */
	struct notifier_block	tee_stop_notifier;
	/* Log of last commands */
#define LAST_CMDS_SIZE 256
	struct mutex		last_cmds_mutex;	/* Log protection */
	struct command_info {
		u64			cpu_clk;	/* Kernel time */
		pid_t			pid;		/* Caller PID */
		u32			id;		/* IWP command ID */
		u32			session_id;
		char			uuid_str[34];
		enum state {
			UNUSED,		/* Unused slot */
			PENDING,	/* Previous command in progress */
			SENT,		/* Waiting for response */
			COMPLETE,	/* Got result */
			FAILED,		/* Something went wrong */
		}			state;	/* Command processing state */
		struct gp_return	result;	/* Command result */
		int			errno;	/* Return code */
	}				last_cmds[LAST_CMDS_SIZE];
	int				last_cmds_index;
} l_ctx;

static void iwp_notif_handler(u32 id, u32 payload)
{
	struct iwp_session *iwp_session = NULL, *candidate;

	mutex_lock(&l_ctx.sessions_lock);
	list_for_each_entry(candidate, &l_ctx.sessions, list) {
		mc_dev_devel("candidate->slot [%08llx]", candidate->slot);
		/* If id is SID_CANCEL_OPERATION, there is pseudo session */
		if (candidate->slot == payload &&
		    (id != SID_CANCEL_OPERATION || candidate->sid == id)) {
			iwp_session = candidate;
			break;
		}
	}
	mutex_unlock(&l_ctx.sessions_lock);

	if (!iwp_session) {
		mc_dev_err(-ENXIO, "IWP no session found for id=0x%x slot=0x%x",
			   id, payload);
		return;
	}

	mc_dev_devel("IWP: iwp_session [%p] id [%08x] slot [%08x]",
		     iwp_session, id, payload);
	nq_session_state_update(&iwp_session->nq_session, NQ_NOTIF_RECEIVED);
	complete(&iwp_session->completion);
}

void iwp_session_init(struct iwp_session *iwp_session,
		      const struct identity *identity)
{
	nq_session_init(&iwp_session->nq_session, true);
	iwp_session->sid = SID_INVALID;
	iwp_session->slot = INVALID_IWS_SLOT;
	INIT_LIST_HEAD(&iwp_session->list);
	mutex_init(&iwp_session->notif_wait_lock);
	init_completion(&iwp_session->completion);
	mutex_init(&iwp_session->iws_lock);
	iwp_session->state = IWP_SESSION_RUNNING;
	if (identity)
		iwp_session->client_identity = *identity;
}

static u64 iws_slot_get(void)
{
	struct iws *iws;
	u64 slot = INVALID_IWS_SLOT;

	if (fe_ops)
		return (uintptr_t)kzalloc(sizeof(*iws), GFP_KERNEL);

	mutex_lock(&l_ctx.iws_list_lock);
	if (!list_empty(&l_ctx.free_iws)) {
		iws = list_first_entry(&l_ctx.free_iws, struct iws, list);
		slot = iws->slot;
		list_move(&iws->list, &l_ctx.allocd_iws);
		atomic_inc(&g_ctx.c_slots);
		mc_dev_devel("got slot %llu", slot);
	}
	mutex_unlock(&l_ctx.iws_list_lock);
	return slot;
}

/* Passing INVALID_IWS_SLOT is supported */
static void iws_slot_put(u64 slot)
{
	struct iws *iws;
	bool found = false;

	if (fe_ops) {
		kfree((void *)(uintptr_t)slot);
		return;
	}

	mutex_lock(&l_ctx.iws_list_lock);
	list_for_each_entry(iws, &l_ctx.allocd_iws, list) {
		if (slot == iws->slot) {
			list_move(&iws->list, &l_ctx.free_iws);
			atomic_dec(&g_ctx.c_slots);
			found = true;
			mc_dev_devel("put slot %llu", slot);
			break;
		}
	}
	mutex_unlock(&l_ctx.iws_list_lock);

	if (!found)
		mc_dev_err(-EINVAL, "slot %llu not found", slot);
}

static inline struct interworld_session *slot_to_iws(u64 slot)
{
	if (fe_ops)
		return (struct interworld_session *)(uintptr_t)slot;

	return (struct interworld_session *)((uintptr_t)l_ctx.iws + (u32)slot);
}

/*
 * IWP command functions
 */
static int iwp_cmd(struct iwp_session *iwp_session, u32 id,
		   struct teec_uuid *uuid, bool killable)
{
	struct command_info *cmd_info;
	int ret;

	/* Initialize MCP log */
	mutex_lock(&l_ctx.last_cmds_mutex);
	cmd_info = &l_ctx.last_cmds[l_ctx.last_cmds_index];
	memset(cmd_info, 0, sizeof(*cmd_info));
	cmd_info->cpu_clk = local_clock();
	cmd_info->pid = current->pid;
	cmd_info->id = id;
	if (id == SID_OPEN_SESSION || id == SID_OPEN_TA) {
		/* Keep UUID because it's an 'open session' cmd */
		const char *cuuid = (const char *)uuid;
		size_t i;

		cmd_info->uuid_str[0] = ' ';
		for (i = 0; i < sizeof(*uuid); i++) {
			snprintf(&cmd_info->uuid_str[1 + i * 2], 3, "%02x",
				 cuuid[i]);
		}
	} else if (id == SID_CANCEL_OPERATION) {
		struct interworld_session *iws = slot_to_iws(iwp_session->slot);

		if (iws)
			cmd_info->session_id = iws->session_handle;
		else
			cmd_info->session_id = 0;
	} else {
		cmd_info->session_id = iwp_session->sid;
	}

	cmd_info->state = PENDING;
	iwp_set_ret(0, &cmd_info->result);
	if (++l_ctx.last_cmds_index >= LAST_CMDS_SIZE)
		l_ctx.last_cmds_index = 0;
	mutex_unlock(&l_ctx.last_cmds_mutex);

	if (l_ctx.iwp_dead)
		return -EHOSTUNREACH;

	mc_dev_devel("psid [%08x], sid [%08x]", id, iwp_session->sid);
	ret = nq_session_notify(&iwp_session->nq_session, id,
				iwp_session->slot);
	if (ret) {
		mc_dev_err(ret, "sid [%08x]: sending failed", iwp_session->sid);
		mutex_lock(&l_ctx.last_cmds_mutex);
		cmd_info->errno = ret;
		cmd_info->state = FAILED;
		mutex_unlock(&l_ctx.last_cmds_mutex);
		return ret;
	}

	/* Update MCP log */
	mutex_lock(&l_ctx.last_cmds_mutex);
	cmd_info->state = SENT;
	mutex_unlock(&l_ctx.last_cmds_mutex);

	/*
	 * NB: Wait cannot be interruptible as we need an answer from SWd. It's
	 * up to the user-space to request a cancellation (for open session and
	 * command invocation operations.)
	 *
	 * We do provide a way out to make applications killable in some cases
	 * though.
	 */
	if (killable) {
		ret = wait_for_completion_killable(&iwp_session->completion);
		if (ret) {
			iwp_request_cancellation(iwp_session->slot);
			/* Make sure the SWd did not die in the meantime */
			if (l_ctx.iwp_dead)
				return -EHOSTUNREACH;

			wait_for_completion(&iwp_session->completion);
		}
	} else {
		wait_for_completion(&iwp_session->completion);
	}

	if (l_ctx.iwp_dead)
		return -EHOSTUNREACH;

	/* Update MCP log */
	mutex_lock(&l_ctx.last_cmds_mutex);
	{
		struct interworld_session *iws = slot_to_iws(iwp_session->slot);

		cmd_info->result.origin = iws->return_origin;
		cmd_info->result.value = iws->status;
		if (id == SID_OPEN_SESSION || id == SID_OPEN_TA)
			cmd_info->session_id = iws->session_handle;
	}
	cmd_info->state = COMPLETE;
	mutex_unlock(&l_ctx.last_cmds_mutex);
	nq_session_state_update(&iwp_session->nq_session, NQ_NOTIF_CONSUMED);
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
	case -EHOSTUNREACH:
		/* Tee crashed */
		gp_ret->value = TEEC_ERROR_TARGET_DEAD;
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

int iwp_register_shared_mem(struct tee_mmu *mmu, u32 *sva,
			    struct gp_return *gp_ret)
{
	int ret;

	if (fe_ops)
		return fe_ops->gp_register_shared_mem(mmu, sva, gp_ret);

	ret = mcp_map(SID_MEMORY_REFERENCE, mmu, sva);
	/* iwp_set_ret would override the origin if called after */
	ret = iwp_set_ret(ret, gp_ret);
	if (ret)
		gp_ret->origin = TEEC_ORIGIN_TEE;

	return ret;
}

int iwp_release_shared_mem(struct mcp_buffer_map *map)
{
	if (fe_ops)
		return fe_ops->gp_release_shared_mem(map);

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
				struct gp_temp_memref *tmpref;

				tmpref = &operation->params[i].tmpref;
				/* Prepare buffer to map */
				bufs[i].va = tmpref->buffer;
				if (tmpref->size > BUFFER_LENGTH_MAX) {
					mc_dev_err(-EINVAL,
						   "buffer size %llu too big",
						   tmpref->size);
					return -EINVAL;
				}

				bufs[i].len = tmpref->size;
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
	iws->params[i].tmpref.physical_address = map->addr;
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
		else if (maps[i].map.addr)
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

static const char *origin_to_string(u32 origin)
{
	switch (origin) {
	case TEEC_ORIGIN_API:
		return "API";
	case TEEC_ORIGIN_COMMS:
		return "COMMS";
	case TEEC_ORIGIN_TEE:
		return "TEE";
	case TEEC_ORIGIN_TRUSTED_APP:
		return "TRUSTED_APP";
	}
	return "UNKNOWN";
}

static const char *value_to_string(u32 value)
{
	switch (value) {
	case TEEC_SUCCESS:
		return "SUCCESS";
	case TEEC_ERROR_GENERIC:
		return "GENERIC";
	case TEEC_ERROR_ACCESS_DENIED:
		return "ACCESS_DENIED";
	case TEEC_ERROR_CANCEL:
		return "CANCEL";
	case TEEC_ERROR_ACCESS_CONFLICT:
		return "ACCESS_CONFLICT";
	case TEEC_ERROR_EXCESS_DATA:
		return "EXCESS_DATA";
	case TEEC_ERROR_BAD_FORMAT:
		return "BAD_FORMAT";
	case TEEC_ERROR_BAD_PARAMETERS:
		return "BAD_PARAMETERS";
	case TEEC_ERROR_BAD_STATE:
		return "BAD_STATE";
	case TEEC_ERROR_ITEM_NOT_FOUND:
		return "ITEM_NOT_FOUND";
	case TEEC_ERROR_NOT_IMPLEMENTED:
		return "NOT_IMPLEMENTED";
	case TEEC_ERROR_NOT_SUPPORTED:
		return "NOT_SUPPORTED";
	case TEEC_ERROR_NO_DATA:
		return "NO_DATA";
	case TEEC_ERROR_OUT_OF_MEMORY:
		return "OUT_OF_MEMORY";
	case TEEC_ERROR_BUSY:
		return "BUSY";
	case TEEC_ERROR_COMMUNICATION:
		return "COMMUNICATION";
	case TEEC_ERROR_SECURITY:
		return "SECURITY";
	case TEEC_ERROR_SHORT_BUFFER:
		return "SHORT_BUFFER";
	case TEEC_ERROR_TARGET_DEAD:
		return "TARGET_DEAD";
	case TEEC_ERROR_STORAGE_NO_SPACE:
		return "STORAGE_NO_SPACE";
	}
	return NULL;
}

static const char *cmd_to_string(u32 id)
{
	switch (id) {
	case SID_OPEN_SESSION:
		return "open session";
	case SID_INVOKE_COMMAND:
		return "invoke command";
	case SID_CLOSE_SESSION:
		return "close session";
	case SID_CANCEL_OPERATION:
		return "cancel operation";
	case SID_MEMORY_REFERENCE:
		return "memory reference";
	case SID_OPEN_TA:
		return "open TA";
	case SID_REQ_TA:
		return "request TA";
	}
	return "unknown";
}

static const char *state_to_string(enum iwp_session_state state)
{
	switch (state) {
	case IWP_SESSION_RUNNING:
		return "running";
	case IWP_SESSION_CLOSE_REQUESTED:
		return "close requested";
	case IWP_SESSION_CLOSED:
		return "closed";
	}
	return "error";
}

int iwp_open_session_prepare(
	struct iwp_session *iwp_session,
	struct gp_operation *operation,
	struct mc_ioctl_buffer *bufs,
	struct gp_shared_memory **parents,
	struct gp_return *gp_ret)
{
	struct interworld_session *iws;
	u64 slot, op_slot;
	int ret = 0;

	/* Get session final slot */
	slot = iws_slot_get();
	mc_dev_devel("slot [%08llx]", slot);
	if (slot == INVALID_IWS_SLOT) {
		ret = -ENOMEM;
		mc_dev_err(ret, "can't get slot");
		return iwp_set_ret(ret, gp_ret);
	}

	/* Get session temporary slot */
	op_slot = iws_slot_get();
	mc_dev_devel("op_slot [%08llx]", op_slot);
	if (op_slot == INVALID_IWS_SLOT) {
		ret = -ENOMEM;
		mc_dev_err(ret, "can't get op_slot");
		iws_slot_put(slot);
		return iwp_set_ret(ret, gp_ret);
	}

	mutex_lock(&iwp_session->iws_lock);

	/* Prepare final session: refer to temporary slot in final one */
	iwp_session->slot = slot;
	iws = slot_to_iws(slot);
	memset(iws, 0, sizeof(*iws));

	/* Prepare temporary session */
	iwp_session->op_slot = op_slot;
	iws = slot_to_iws(op_slot);
	memset(iws, 0, sizeof(*iws));

	if (operation) {
		ret = iwp_operation_to_iws(operation, iws, bufs, parents);
		if (ret)
			iwp_open_session_abort(iwp_session);
	}

	return iwp_set_ret(ret, gp_ret);
}

void iwp_open_session_abort(struct iwp_session *iwp_session)
{
	iws_slot_put(iwp_session->slot);
	iws_slot_put(iwp_session->op_slot);
	mutex_unlock(&iwp_session->iws_lock);
}

/*
 * Like open session except we pass the TA blob from NWd to SWd
 */
int iwp_open_session(
	struct iwp_session *iwp_session,
	const struct mc_uuid_t *uuid,
	struct gp_operation *operation,
	const struct iwp_buffer_map *maps,
	struct interworld_session *iws_in,
	struct tee_mmu **mmus,
	struct gp_return *gp_ret,
	const char vm_id[16])
{
	struct interworld_session *iws = slot_to_iws(iwp_session->slot);
	struct interworld_session *op_iws = slot_to_iws(iwp_session->op_slot);
	struct tee_object *obj = NULL;
	struct tee_mmu *obj_mmu = NULL;
	struct mcp_buffer_map obj_map;
	int ret;

	/* Operation is NULL when called from Xen BE */
	if (operation) {
		/* Login info */
		op_iws->login = iwp_session->client_identity.login_type;
		mc_dev_devel("iws->login [%08x]", op_iws->login);
		memcpy(&op_iws->client_uuid,
		       iwp_session->client_identity.login_data,
		       sizeof(op_iws->client_uuid));

		/* Put ingoing operation in temporary IWS */
		iwp_iws_set_refs(op_iws, maps);
	} else {
		struct mcp_buffer_map map;
		int i;

		*op_iws = *iws_in;

		/* Insert correct mapping in operation */
		for (i = 0; i < 4; i++) {
			if (!mmus[i])
				continue;

			tee_mmu_buffer(mmus[i], &map);
			iwp_iws_set_tmpref(op_iws, i, &map);
		}
	}

	// Set the vm id field of the interwolrd_session struct
	memcpy(op_iws->vm_id, vm_id, sizeof(op_iws->vm_id));
	mc_dev_devel("Virtual Machine id: %s ", vm_id);

	/* For the SWd to find the TA slot from the main one */
	iws->command_id = (u32)iwp_session->op_slot;

	/* TA blob handling */
	if (!fe_ops) {
		union mclf_header *header;

		obj = tee_object_get(uuid, true);
		if (IS_ERR(obj)) {
			/* Tell SWd to load TA from SFS as not in registry */
			if (PTR_ERR(obj) == -ENOENT)
				obj = tee_object_select(uuid);

			if (IS_ERR(obj))
				return PTR_ERR(obj);
		}

		/* Convert UUID */
		header = (union mclf_header *)(&obj->data);
		mcuuid_to_tee_uuid(&header->mclf_header_v2.uuid,
				   &op_iws->target_uuid);

		/* Create mapping for blob (alloc'd by driver => task = NULL) */
		{
			struct mc_ioctl_buffer buf = {
				.va = (uintptr_t)obj->data,
				.len = obj->length,
				.flags = MC_IO_MAP_INPUT,
			};

			obj_mmu = tee_mmu_create(NULL, &buf);
			if (IS_ERR(obj_mmu)) {
				ret = PTR_ERR(obj_mmu);
				goto err_mmu;
			}

			iws->param_types = TEEC_MEMREF_TEMP_INPUT;
			tee_mmu_buffer(obj_mmu, &obj_map);
			iwp_iws_set_tmpref(iws, 0, &obj_map);
			mc_dev_devel("wsm_type [%04x], offset [%04x]",
				     obj_map.type, obj_map.offset);
			mc_dev_devel("size [%08x], physical_address [%08llx]",
				     obj_map.length, obj_map.addr);
		}
	}

	/* Add to local list of sessions so we can receive the notification */
	mutex_lock(&l_ctx.sessions_lock);
	list_add_tail(&iwp_session->list, &l_ctx.sessions);
	mutex_unlock(&l_ctx.sessions_lock);

	/* Send IWP open command */
	if (fe_ops)
		ret = fe_ops->gp_open_session(iwp_session, uuid, maps, iws,
					      op_iws, gp_ret);
	else
		ret = iwp_cmd(iwp_session, SID_OPEN_TA, &op_iws->target_uuid,
			      true);

	/* Temporary slot is not needed any more */
	iws_slot_put(iwp_session->op_slot);
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
		if (operation)
			iwp_iws_to_operation(iws, operation);
		else
			*iws_in = *iws;

	} else {
		/* Remove from list of sessions */
		mutex_lock(&l_ctx.sessions_lock);
		list_del(&iwp_session->list);
		mutex_unlock(&l_ctx.sessions_lock);
		iws_slot_put(iwp_session->slot);
		mc_dev_devel("failed: %s from %s, ret %d",
			     value_to_string(gp_ret->value),
			     origin_to_string(gp_ret->origin), ret);
	}

	mutex_unlock(&iwp_session->iws_lock);

	/* Blob not needed as re-mapped by the SWd */
	if (obj_mmu)
		tee_mmu_put(obj_mmu);

err_mmu:
	/* Delete secure object */
	if (obj)
		tee_object_free(obj);

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
	int ret = 0;

	if (fe_ops) {
		ret = fe_ops->gp_close_session(iwp_session);
	} else {
		mutex_lock(&iwp_session->iws_lock);
		iwp_session->state = IWP_SESSION_CLOSE_REQUESTED;

		/* Send IWP open command */
		ret = iwp_cmd(iwp_session, SID_CLOSE_SESSION, NULL, false);
		mutex_unlock(&iwp_session->iws_lock);
	}

	iwp_session_release(iwp_session);
	mc_dev_devel("close session %x ret %d state %s", iwp_session->sid,
		     ret, state_to_string(iwp_session->state));
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
	int ret = 0;

	if (iwp_session->state != IWP_SESSION_RUNNING)
		return iwp_set_ret(-EBADFD, gp_ret);

	mutex_lock(&iwp_session->iws_lock);
	iws = slot_to_iws(iwp_session->slot);
	memset(iws, 0, sizeof(*iws));
	iws->session_handle = iwp_session->sid;
	if (operation) {
		iws->command_id = command_id;
		ret = iwp_operation_to_iws(operation, iws, bufs, parents);
		if (ret)
			iwp_invoke_command_abort(iwp_session);
	}

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
	struct interworld_session *iws_in,
	struct tee_mmu **mmus,
	struct gp_return *gp_ret)
{
	struct interworld_session *iws = slot_to_iws(iwp_session->slot);
	int ret = 0;

	/* Operation is NULL when called from Xen BE */
	if (operation) {
		/* Update IWS with operation maps */
		iwp_iws_set_refs(iws, maps);
	} else {
		struct mcp_buffer_map map;
		int i;

		*iws = *iws_in;

		/* Insert correct mapping in operation */
		for (i = 0; i < 4; i++) {
			if (!mmus[i])
				continue;

			tee_mmu_buffer(mmus[i], &map);
			iwp_iws_set_tmpref(iws, i, &map);
		}
	}

	if (fe_ops)
		ret = fe_ops->gp_invoke_command(iwp_session, maps, iws, gp_ret);
	else
		ret = iwp_cmd(iwp_session, SID_INVOKE_COMMAND, NULL, true);

	/* Treat remote errors as errors, just use a specific errno */
	if (!ret && iws->status != TEEC_SUCCESS)
		ret = -ECHILD;

	if (operation)
		iwp_iws_to_operation(iws, operation);
	else
		*iws_in = *iws;

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
	u64 slot)
{
	/* Pseudo IWP session for cancellation */
	struct iwp_session iwp_session;
	int ret;

	if (fe_ops)
		return fe_ops->gp_request_cancellation(
			(uintptr_t)slot_to_iws(slot));

	iwp_session_init(&iwp_session, NULL);
	/* sid is local. Set is to SID_CANCEL_OPERATION to make things clear */
	iwp_session.sid = SID_CANCEL_OPERATION;
	iwp_session.slot = slot;
	mutex_lock(&l_ctx.sessions_lock);
	list_add_tail(&iwp_session.list, &l_ctx.sessions);
	mutex_unlock(&l_ctx.sessions_lock);
	ret = iwp_cmd(&iwp_session, SID_CANCEL_OPERATION, NULL, false);
	mutex_lock(&l_ctx.sessions_lock);
	list_del(&iwp_session.list);
	mutex_unlock(&l_ctx.sessions_lock);
	return ret;
}

static int debug_sessions(struct kasnprintf_buf *buf)
{
	struct iwp_session *session;
	int ret;

	/* Header */
	ret = kasnprintf(buf, "%20s %4s %-15s %-11s %7s\n",
			 "CPU clock", "ID", "state", "notif state", "slot");
	if (ret < 0)
		return ret;

	mutex_lock(&l_ctx.sessions_lock);
	list_for_each_entry(session, &l_ctx.sessions, list) {
		const char *state_str;
		u64 cpu_clk;

		state_str = nq_session_state(&session->nq_session, &cpu_clk);
		ret = kasnprintf(buf, "%20llu %4x %-15s %-11s %7llu\n", cpu_clk,
				 session->sid == SID_INVALID ? 0 : session->sid,
				 state_to_string(session->state), state_str,
				 session->slot);
		if (ret < 0)
			break;
	}
	mutex_unlock(&l_ctx.sessions_lock);
	return ret;
}

static ssize_t debug_sessions_read(struct file *file, char __user *user_buf,
				   size_t count, loff_t *ppos)
{
	return debug_generic_read(file, user_buf, count, ppos,
				  debug_sessions);
}

static const struct file_operations debug_sessions_ops = {
	.read = debug_sessions_read,
	.llseek = default_llseek,
	.open = debug_generic_open,
	.release = debug_generic_release,
};

static inline int show_log_entry(struct kasnprintf_buf *buf,
				 struct command_info *cmd_info)
{
	const char *state_str = "unknown";
	const char *value_str = value_to_string(cmd_info->result.value);
	char value[16];

	switch (cmd_info->state) {
	case UNUSED:
		state_str = "unused";
		break;
	case PENDING:
		state_str = "pending";
		break;
	case SENT:
		state_str = "sent";
		break;
	case COMPLETE:
		state_str = "complete";
		break;
	case FAILED:
		state_str = "failed";
		break;
	}

	if (!value_str) {
		snprintf(value, sizeof(value), "%08x", cmd_info->result.value);
		value_str = value;
	}

	return kasnprintf(buf, "%20llu %5d %-16s %5x %-8s %5d %-11s %-17s%s\n",
			  cmd_info->cpu_clk, cmd_info->pid,
			  cmd_to_string(cmd_info->id), cmd_info->session_id,
			  state_str, cmd_info->errno,
			  origin_to_string(cmd_info->result.origin), value_str,
			  cmd_info->uuid_str);
}

static int debug_last_cmds(struct kasnprintf_buf *buf)
{
	struct command_info *cmd_info;
	int i, ret = 0;

	/* Initialize MCP log */
	mutex_lock(&l_ctx.last_cmds_mutex);
	ret = kasnprintf(buf, "%20s %5s %-16s %5s %-8s %5s %-11s %-17s%s\n",
			 "CPU clock", "PID", "command", "S-ID",
			 "state", "errno", "origin", "value", "UUID");
	if (ret < 0)
		goto out;

	cmd_info = &l_ctx.last_cmds[l_ctx.last_cmds_index];
	if (cmd_info->state != UNUSED)
		/* Buffer has wrapped around, dump end (oldest records) */
		for (i = l_ctx.last_cmds_index; i < LAST_CMDS_SIZE; i++) {
			ret = show_log_entry(buf, cmd_info++);
			if (ret < 0)
				goto out;
		}

	/* Dump first records */
	cmd_info = &l_ctx.last_cmds[0];
	for (i = 0; i < l_ctx.last_cmds_index; i++) {
		ret = show_log_entry(buf, cmd_info++);
		if (ret < 0)
			goto out;
	}

out:
	mutex_unlock(&l_ctx.last_cmds_mutex);
	return ret;
}

static ssize_t debug_last_cmds_read(struct file *file, char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	return debug_generic_read(file, user_buf, count, ppos, debug_last_cmds);
}

static const struct file_operations debug_last_cmds_ops = {
	.read = debug_last_cmds_read,
	.llseek = default_llseek,
	.open = debug_generic_open,
	.release = debug_generic_release,
};

static inline void mark_iwp_dead(void)
{
	struct iwp_session *session;

	l_ctx.iwp_dead = true;
	/* Signal all potential waiters that SWd is going away */
	mutex_lock(&l_ctx.sessions_lock);
	list_for_each_entry(session, &l_ctx.sessions, list)
		complete(&session->completion);
	mutex_unlock(&l_ctx.sessions_lock);
}

static int tee_stop_notifier_fn(struct notifier_block *nb, unsigned long event,
				void *data)
{
	mark_iwp_dead();
	return 0;
}

int iwp_init(void)
{
	int i;

	l_ctx.iws = nq_get_iwp_buffer();
	INIT_LIST_HEAD(&l_ctx.free_iws);
	INIT_LIST_HEAD(&l_ctx.allocd_iws);
	l_ctx.iws_list_pool = kcalloc(MAX_IW_SESSION, sizeof(struct iws),
				      GFP_KERNEL);
	if (!l_ctx.iws_list_pool)
		return -ENOMEM;

	for (i = 0; i < MAX_IW_SESSION; i++) {
		l_ctx.iws_list_pool[i].slot =
			i * sizeof(struct interworld_session);
		list_add(&l_ctx.iws_list_pool[i].list, &l_ctx.free_iws);
	}

	mutex_init(&l_ctx.iws_list_lock);
	INIT_LIST_HEAD(&l_ctx.sessions);
	mutex_init(&l_ctx.sessions_lock);
	nq_register_notif_handler(iwp_notif_handler, true);
	l_ctx.tee_stop_notifier.notifier_call = tee_stop_notifier_fn;
	nq_register_tee_stop_notifier(&l_ctx.tee_stop_notifier);
	/* Debugfs */
	mutex_init(&l_ctx.last_cmds_mutex);
	return 0;
}

void iwp_exit(void)
{
	mark_iwp_dead();
	nq_unregister_tee_stop_notifier(&l_ctx.tee_stop_notifier);
}

int iwp_start(void)
{
	/* Create debugfs sessions and last commands entries */
	debugfs_create_file("iwp_sessions", 0400, g_ctx.debug_dir, NULL,
			    &debug_sessions_ops);
	debugfs_create_file("last_iwp_commands", 0400, g_ctx.debug_dir, NULL,
			    &debug_last_cmds_ops);
	return 0;
}

void iwp_stop(void)
{
}
