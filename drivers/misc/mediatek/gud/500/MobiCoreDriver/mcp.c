// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2013-2019 TRUSTONIC LIMITED
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
#include <linux/freezer.h>
#include <asm/barrier.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/version.h>
#include <linux/sched/clock.h>	/* local_clock */

#include "public/mc_user.h"
#include "public/mc_admin.h"

#include "mci/mcimcp.h"
#include "mci/mcifc.h"
#include "mci/mcinq.h"		/* SID_MCP */
#include "mci/mcitime.h"	/* struct mcp_time */
#include "mci/mciiwp.h"

#include "main.h"
#include "mmu.h"		/* MMU for ta and tci */
#include "nq.h"
#include "protocol_common.h"
#include "mcp.h"

/* respond timeout for MCP notification, in secs */
#define MCP_TIMEOUT		10
#define MCP_RETRIES		5
#define MCP_NF_QUEUE_SZ		8

static struct {
	union mcp_message	*buffer;	/* MCP communication buffer */
	struct mutex		buffer_mutex;	/* Lock for the buffer above */
	bool			mcp_dead;
	struct mcp_session	mcp_session;	/* Pseudo session for MCP */
	/* Unexpected notification (during MCP open) */
	struct mutex		unexp_notif_mutex;
	struct notification	unexp_notif;
	/* Sessions */
	struct mutex		sessions_lock;
	struct list_head	sessions;
	/* TEE bad state detection */
	struct notifier_block	tee_stop_notifier;
	u32			timeout_period;
	/* Log of last commands */
#define LAST_CMDS_SIZE 1024
	struct mutex		last_cmds_mutex;	/* Log protection */
	struct command_info {
		u64			cpu_clk;	/* Kernel time */
		pid_t			pid;		/* Caller PID */
		enum cmd_id		id;		/* MCP command ID */
		u32			session_id;
		char			uuid_str[34];
		enum state {
			UNUSED,		/* Unused slot */
			PENDING,	/* Previous command in progress */
			SENT,		/* Waiting for response */
			COMPLETE,	/* Got result */
			FAILED,		/* Something went wrong */
		}			state;	/* Command processing state */
		int			errno;	/* Return code */
		enum mcp_result		result;	/* Command result */
	}				last_cmds[LAST_CMDS_SIZE];
	int				last_cmds_index;
} l_ctx;

static const char *cmd_to_string(enum cmd_id id)
{
	switch (id) {
	case MC_MCP_CMD_ID_INVALID:
		return "invalid";
	case MC_MCP_CMD_OPEN_SESSION:
		return "open session";
	case MC_MCP_CMD_CLOSE_SESSION:
		return "close session";
	case MC_MCP_CMD_MAP:
		return "map";
	case MC_MCP_CMD_UNMAP:
		return "unmap";
	case MC_MCP_CMD_GET_MOBICORE_VERSION:
		return "get version";
	case MC_MCP_CMD_CLOSE_MCP:
		return "close mcp";
	case MC_MCP_CMD_LOAD_TOKEN:
		return "load token";
	case MC_MCP_CMD_LOAD_SYSENC_KEY_SO:
		return "load Key SO";
	}
	return "unknown";
}

static const char *state_to_string(enum mcp_session_state state)
{
	switch (state) {
	case MCP_SESSION_RUNNING:
		return "running";
	case MCP_SESSION_CLOSE_FAILED:
		return "close failed";
	case MCP_SESSION_CLOSED:
		return "closed";
	}
	return "error";
}

static inline void mark_mcp_dead(void)
{
	struct mcp_session *session;

	l_ctx.mcp_dead = true;
	complete(&l_ctx.mcp_session.completion);
	/* Signal all potential waiters that SWd is going away */
	list_for_each_entry(session, &l_ctx.sessions, list)
		complete(&session->completion);
}

static int tee_stop_notifier_fn(struct notifier_block *nb, unsigned long event,
				void *data)
{
	mark_mcp_dead();
	return 0;
}

void mcp_session_init(struct mcp_session *session)
{
	nq_session_init(&session->nq_session, false);
	session->sid = SID_INVALID;
	INIT_LIST_HEAD(&session->list);
	mutex_init(&session->notif_wait_lock);
	init_completion(&session->completion);
	mutex_init(&session->exit_code_lock);
	session->exit_code = 0;
	session->state = MCP_SESSION_RUNNING;
	session->notif_count = 0;
}

static inline bool mcp_session_isrunning(struct mcp_session *session)
{
	bool ret;

	mutex_lock(&l_ctx.sessions_lock);
	ret = session->state == MCP_SESSION_RUNNING;
	mutex_unlock(&l_ctx.sessions_lock);
	return ret;
}

/*
 * session remains valid thanks to the upper layers reference counters, but the
 * SWd session may have died, in which case we are informed.
 */
int mcp_wait(struct mcp_session *session, s32 timeout)
{
	s32 err;
	int ret = 0;

	mutex_lock(&session->notif_wait_lock);
	if (fe_ops) {
		ret = fe_ops->mc_wait(session, timeout);
		mutex_unlock(&session->notif_wait_lock);
		return ret;
	}

	if (l_ctx.mcp_dead) {
		ret = -EHOSTUNREACH;
		goto end;
	}

	if (!mcp_session_isrunning(session)) {
		ret = -ENXIO;
		goto end;
	}

	mcp_get_err(session, &err);
	if (err) {
		ret = -ECOMM;
		goto end;
	}

	if (timeout < 0) {
		ret = wait_for_completion_interruptible(&session->completion);
		if (ret)
			goto end;
	} else {
		ret = wait_for_completion_interruptible_timeout(
			&session->completion, timeout * HZ / 1000);
		if (ret < 0)
			/* Interrupted */
			goto end;

		if (!ret) {
			/* Timed out */
			ret = -ETIME;
			goto end;
		}

		ret = 0;
	}

	if (l_ctx.mcp_dead) {
		ret = -EHOSTUNREACH;
		goto end;
	}

	mcp_get_err(session, &err);
	if (err) {
		ret = -ECOMM;
		goto end;
	}

	if (!mcp_session_isrunning(session)) {
		ret = -ENXIO;
		goto end;
	}

end:
	if (!ret)
		nq_session_state_update(&session->nq_session,
					NQ_NOTIF_CONSUMED);
	else if (ret != -ERESTARTSYS)
		nq_session_state_update(&session->nq_session, NQ_NOTIF_DEAD);

	mutex_unlock(&session->notif_wait_lock);
	if (ret) {
#ifdef CONFIG_FREEZER
		if (ret == -ERESTARTSYS && system_freezing_cnt.counter == 1)
			mc_dev_devel("freezing session %x", session->sid);
		else
#endif
			mc_dev_devel("session %x ec %d ret %d",
				     session->sid, session->exit_code, ret);
	}

	return ret;
}

int mcp_get_err(struct mcp_session *session, s32 *err)
{
	if (fe_ops)
		return fe_ops->mc_get_err(session, err);

	mutex_lock(&session->exit_code_lock);
	*err = session->exit_code;
	mutex_unlock(&session->exit_code_lock);
	if (*err)
		mc_dev_info("session %x ec %d", session->sid, *err);

	return 0;
}

static inline int wait_mcp_notification(void)
{
	unsigned long timeout = msecs_to_jiffies(l_ctx.timeout_period * 1000);
	int try, ret = -ETIME;

	/*
	 * Total timeout is l_ctx.timeout_period * MCP_RETRIES, but we check for
	 * a crash to try and terminate before then if things go wrong.
	 */
	for (try = 1; try <= MCP_RETRIES; try++) {
		/*
		 * Wait non-interruptible to keep MCP synchronised even if
		 * caller is interrupted by signal.
		 */
		if (wait_for_completion_timeout(&l_ctx.mcp_session.completion,
						timeout) > 0)
			return 0;

		mc_dev_err(ret, "no answer after %ds",
			   l_ctx.timeout_period * try);
	}

	mc_dev_err(ret, "timed out waiting for MCP notification");
	nq_signal_tee_hung();
	return ret;
}

static int mcp_cmd(union mcp_message *cmd,
		   /* The fields below are for debug purpose only */
		   u32 in_session_id,
		   u32 *out_session_id,
		   struct mc_uuid_t *uuid)
{
	int err = 0, ret = -EHOSTUNREACH;
	union mcp_message *msg;
	enum cmd_id cmd_id = cmd->cmd_header.cmd_id;
	struct command_info *cmd_info;

	/* Initialize MCP log */
	mutex_lock(&l_ctx.last_cmds_mutex);
	cmd_info = &l_ctx.last_cmds[l_ctx.last_cmds_index];
	cmd_info->cpu_clk = local_clock();
	cmd_info->pid = current->pid;
	cmd_info->id = cmd_id;
	cmd_info->session_id = in_session_id;
	if (uuid) {
		/* Keep UUID because it's an 'open session' cmd */
		size_t i;

		cmd_info->uuid_str[0] = ' ';
		for (i = 0; i < sizeof(uuid->value); i++) {
			snprintf(&cmd_info->uuid_str[1 + i * 2], 3, "%02x",
				 uuid->value[i]);
		}
	} else {
		cmd_info->uuid_str[0] = '\0';
	}

	cmd_info->state = PENDING;
	cmd_info->errno = 0;
	cmd_info->result = MC_MCP_RET_OK;
	if (++l_ctx.last_cmds_index >= LAST_CMDS_SIZE)
		l_ctx.last_cmds_index = 0;
	mutex_unlock(&l_ctx.last_cmds_mutex);

	mutex_lock(&l_ctx.buffer_mutex);
	msg = l_ctx.buffer;
	if (l_ctx.mcp_dead)
		goto out;

	/* Copy message to MCP buffer */
	memcpy(msg, cmd, sizeof(*msg));

	/* Send MCP notification, with cmd_id as payload for debug purpose */
	nq_session_notify(&l_ctx.mcp_session.nq_session, l_ctx.mcp_session.sid,
			  cmd_id);

	/* Update MCP log */
	mutex_lock(&l_ctx.last_cmds_mutex);
	cmd_info->state = SENT;
	mutex_unlock(&l_ctx.last_cmds_mutex);
	ret = wait_mcp_notification();
	if (ret)
		goto out;

	/* Check response ID */
	if (msg->rsp_header.rsp_id != (cmd_id | FLAG_RESPONSE)) {
		ret = -EBADE;
		mc_dev_err(ret, "MCP command got invalid response (0x%X)",
			   msg->rsp_header.rsp_id);
		goto out;
	}

	/* Convert result */
	switch (msg->rsp_header.result) {
	case MC_MCP_RET_OK:
		err = 0;
		break;
	case MC_MCP_RET_ERR_CLOSE_TASK_FAILED:
		err = -EAGAIN;
		break;
	case MC_MCP_RET_ERR_NO_MORE_SESSIONS:
		err = -EBUSY;
		break;
	case MC_MCP_RET_ERR_OUT_OF_RESOURCES:
		err = -ENOSPC;
		break;
	case MC_MCP_RET_ERR_UNKNOWN_UUID:
		err = -ENOENT;
		break;
	case MC_MCP_RET_ERR_WRONG_PUBLIC_KEY:
		err = -EKEYREJECTED;
		break;
	case MC_MCP_RET_ERR_SERVICE_BLOCKED:
		err = -ECONNREFUSED;
		break;
	case MC_MCP_RET_ERR_SERVICE_LOCKED:
		err = -ECONNABORTED;
		break;
	case MC_MCP_RET_ERR_SYSTEM_NOT_READY:
		err = -EAGAIN;
		break;
	case MC_MCP_RET_ERR_DOWNGRADE_NOT_AUTHORIZED:
		err = -EPERM;
		break;
	default:
		err = -EPERM;
	}

	/* Copy response back to caller struct */
	memcpy(cmd, msg, sizeof(*cmd));

out:
	/* Update MCP log */
	mutex_lock(&l_ctx.last_cmds_mutex);
	if (ret) {
		cmd_info->state = FAILED;
		cmd_info->errno = -ret;
	} else {
		cmd_info->state = COMPLETE;
		cmd_info->errno = -err;
		cmd_info->result = msg->rsp_header.result;
		/* For open session: get SID */
		if (!err && out_session_id)
			cmd_info->session_id = *out_session_id;
	}
	mutex_unlock(&l_ctx.last_cmds_mutex);
	mutex_unlock(&l_ctx.buffer_mutex);
	if (ret) {
		mc_dev_err(ret, "%s: sending failed", cmd_to_string(cmd_id));
		return ret;
	}

	if (err) {
		if (cmd_id == MC_MCP_CMD_CLOSE_SESSION && err == -EAGAIN)
			mc_dev_devel("%s: try again",
				     cmd_to_string(cmd_id));
		else
			mc_dev_err(err, "%s: res %d", cmd_to_string(cmd_id),
				   msg->rsp_header.result);
		return err;
	}

	return 0;
}

static inline int __mcp_get_version(struct mc_version_info *version_info)
{
	union mcp_message cmd;
	u32 version;
	int ret;

	if (fe_ops)
		return fe_ops->mc_get_version(version_info);

	version = MC_VERSION(MCDRVMODULEAPI_VERSION_MAJOR,
			     MCDRVMODULEAPI_VERSION_MINOR);

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_GET_MOBICORE_VERSION;
	ret = mcp_cmd(&cmd, 0, NULL, NULL);
	if (ret)
		return ret;

	memcpy(version_info, &cmd.rsp_get_version.version_info,
	       sizeof(*version_info));
	/*
	 * The CMP version is meaningless in this case, and is replaced
	 * by the driver's own version.
	 */
	version_info->version_nwd = version;
	return 0;
}

int mcp_get_version(struct mc_version_info *version_info)
{
	static struct mc_version_info static_version_info;

	/* If cache empty, get version from the SWd and cache it */
	if (!static_version_info.version_nwd) {
		int ret = __mcp_get_version(&static_version_info);

		if (ret)
			return ret;
	}

	/* Copy cached version */
	memcpy(version_info, &static_version_info, sizeof(*version_info));
	nq_set_version_ptr(static_version_info.product_id);
	return 0;
}

int mcp_load_token(uintptr_t data, const struct mcp_buffer_map *map)
{
	union mcp_message cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_LOAD_TOKEN;
	cmd.cmd_load_token.wsm_data_type = map->type;
	cmd.cmd_load_token.adr_load_data = map->addr;
	cmd.cmd_load_token.ofs_load_data = map->offset;
	cmd.cmd_load_token.len_load_data = map->length;
	return mcp_cmd(&cmd, 0, NULL, NULL);
}

int mcp_load_key_so(uintptr_t data, const struct mcp_buffer_map *map)
{
	union mcp_message cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_LOAD_SYSENC_KEY_SO;
	cmd.cmd_load_key_so.wsm_data_type = map->type;
	cmd.cmd_load_key_so.adr_load_data = map->addr;
	cmd.cmd_load_key_so.ofs_load_data = map->offset;
	cmd.cmd_load_key_so.len_load_data = map->length;
	return mcp_cmd(&cmd, 0, NULL, NULL);
}

int mcp_open_session(struct mcp_session *session, struct mcp_open_info *info,
		     bool tci_in_use)
{
	static DEFINE_MUTEX(local_mutex);
	struct mcp_buffer_map map;
	union mcp_message cmd;
	int ret;

	if (fe_ops) {
		ret = fe_ops->mc_open_session(session, info);
		if (ret)
			return ret;

		/* Add to list of sessions */
		mutex_lock(&l_ctx.sessions_lock);
		list_add_tail(&session->list, &l_ctx.sessions);
		mutex_unlock(&l_ctx.sessions_lock);
		return 0;
	}

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_OPEN_SESSION;
	/* Data */
	cmd.cmd_open.uuid = *info->uuid;
	tee_mmu_buffer(info->ta_mmu, &map);
	cmd.cmd_open.wsm_data_type = map.type;
	cmd.cmd_open.adr_load_data = map.addr;
	cmd.cmd_open.ofs_load_data = map.offset;
	cmd.cmd_open.len_load_data = map.length;
	/* Buffer */
	if (tci_in_use) {
		tee_mmu_buffer(info->tci_mmu, &map);
		cmd.cmd_open.wsmtype_tci = map.type;
		cmd.cmd_open.adr_tci_buffer = map.addr;
		cmd.cmd_open.ofs_tci_buffer = map.offset;
		cmd.cmd_open.len_tci_buffer = map.length;
	} else {
		cmd.cmd_open.wsmtype_tci = WSM_INVALID;
	}

	/* Reset unexpected notification */
	mutex_lock(&local_mutex);
	l_ctx.unexp_notif.session_id = SID_MCP;	/* Cannot be */
	cmd.cmd_open.cmd_open_data.mclf_magic = MC_GP_CLIENT_AUTH_MAGIC;

	/* Send MCP open command */
	ret = mcp_cmd(&cmd, 0, &cmd.rsp_open.session_id, &cmd.cmd_open.uuid);
	/* Make sure we have a valid session ID */
	if (!ret && !cmd.rsp_open.session_id)
		ret = -EBADE;

	if (!ret) {
		session->sid = cmd.rsp_open.session_id;
		/* Add to list of sessions */
		mutex_lock(&l_ctx.sessions_lock);
		list_add_tail(&session->list, &l_ctx.sessions);
		mutex_unlock(&l_ctx.sessions_lock);
		/* Check for spurious notification */
		mutex_lock(&l_ctx.unexp_notif_mutex);
		if (l_ctx.unexp_notif.session_id == session->sid) {
			mutex_lock(&session->exit_code_lock);
			session->exit_code = l_ctx.unexp_notif.payload;
			mutex_unlock(&session->exit_code_lock);
			nq_session_state_update(&session->nq_session,
						NQ_NOTIF_RECEIVED);
			complete(&session->completion);
		}

		mutex_unlock(&l_ctx.unexp_notif_mutex);
	}

	mutex_unlock(&local_mutex);
	return ret;
}

/*
 * Legacy and GP TAs close differently:
 * - GP TAs always send a notification with payload, whether on close or crash
 * - Legacy TAs only send a notification with payload on crash
 * - GP TAs may take time to close, and we get -EAGAIN back from mcp_cmd
 * - Legacy TAs always close when asked, unless they are driver in which case
 *   they just don't close at all
 */
int mcp_close_session(struct mcp_session *session)
{
	union mcp_message cmd;
	/* ret's value is always set, but some compilers complain */
	int ret = -ENXIO;

	if (fe_ops) {
		ret = fe_ops->mc_close_session(session);
	} else {
		/* Signal a potential waiter that SWd session is going away */
		complete(&session->completion);
		/* Send MCP command */
		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd_header.cmd_id = MC_MCP_CMD_CLOSE_SESSION;
		cmd.cmd_close.session_id = session->sid;
		ret = mcp_cmd(&cmd, cmd.cmd_close.session_id, NULL, NULL);
	}

	mutex_lock(&l_ctx.sessions_lock);
	if (!ret) {
		session->state = MCP_SESSION_CLOSED;
		list_del(&session->list);
		nq_session_exit(&session->nq_session);
	} else {
		/* Something is not right, assume session is still running */
		session->state = MCP_SESSION_CLOSE_FAILED;
	}
	mutex_unlock(&l_ctx.sessions_lock);
	mc_dev_devel("close session %x ret %d state %s",
		     session->sid, ret, state_to_string(session->state));
	return ret;
}

/*
 * Session is to be removed from NWd records as SWd has been wiped clean
 */
void mcp_cleanup_session(struct mcp_session *session)
{
	mutex_lock(&l_ctx.sessions_lock);
	session->state = MCP_SESSION_CLOSED;
	list_del(&session->list);
	nq_session_exit(&session->nq_session);
	mutex_unlock(&l_ctx.sessions_lock);
}

int mcp_map(u32 session_id, struct tee_mmu *mmu, u32 *sva)
{
	struct mcp_buffer_map map;
	union mcp_message cmd;
	int ret;

	if (fe_ops)
		return fe_ops->mc_map(session_id, mmu, sva);

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_MAP;
	cmd.cmd_map.session_id = session_id;
	tee_mmu_buffer(mmu, &map);
	cmd.cmd_map.wsm_type = map.type;
	cmd.cmd_map.adr_buffer = map.addr;
	cmd.cmd_map.ofs_buffer = map.offset;
	cmd.cmd_map.len_buffer = map.length;
	cmd.cmd_map.flags = map.flags;
	ret = mcp_cmd(&cmd, session_id, NULL, NULL);
	if (!ret) {
		*sva = cmd.rsp_map.secure_va;
		atomic_inc(&g_ctx.c_maps);
	}

	return ret;
}

int mcp_unmap(u32 session_id, const struct mcp_buffer_map *map)
{
	union mcp_message cmd;
	int ret;

	if (fe_ops)
		return fe_ops->mc_unmap(session_id, map);

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_UNMAP;
	cmd.cmd_unmap.session_id = session_id;
	cmd.cmd_unmap.wsm_type = map->type;
	cmd.cmd_unmap.virtual_buffer_len = map->length;
	cmd.cmd_unmap.secure_va = map->secure_va;
	ret = mcp_cmd(&cmd, session_id, NULL, NULL);
	if (!ret)
		atomic_dec(&g_ctx.c_maps);

	return ret;
}

static int mcp_close(void)
{
	union mcp_message cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_CLOSE_MCP;
	return mcp_cmd(&cmd, 0, NULL, NULL);
}

int mcp_notify(struct mcp_session *session)
{
	if (l_ctx.mcp_dead)
		return -EHOSTUNREACH;

	if (session->sid == SID_MCP)
		mc_dev_devel("notify MCP");
	else
		mc_dev_devel("notify session %x", session->sid);

	if (fe_ops)
		return fe_ops->mc_notify(session);

	/* Put notif_count as payload for debug purpose */
	return nq_session_notify(&session->nq_session, session->sid,
				 ++session->notif_count);
}

static inline void session_notif_handler(struct mcp_session *session, u32 id,
					 u32 payload)
{
	mutex_lock(&l_ctx.sessions_lock);
	mc_dev_devel("MCP notif from session %x exit code %d state %d",
		     id, payload, session ? session->state : -1);
	if (session) {
		/* TA has terminated */
		if (payload) {
			/* Update exit code, or not */
			mutex_lock(&session->exit_code_lock);
			session->exit_code = payload;
			mutex_unlock(&session->exit_code_lock);
		}

		nq_session_state_update(&session->nq_session,
					NQ_NOTIF_RECEIVED);

		/* Unblock waiter */
		complete(&session->completion);
	}
	mutex_unlock(&l_ctx.sessions_lock);

	/* Unknown session, probably being started */
	if (!session) {
		mutex_lock(&l_ctx.unexp_notif_mutex);
		l_ctx.unexp_notif.session_id = id;
		l_ctx.unexp_notif.payload = payload;
		mutex_unlock(&l_ctx.unexp_notif_mutex);
	}
}

static void mcp_notif_handler(u32 id, u32 payload)
{
	if (id == SID_MCP) {
		/* MCP notification */
		mc_dev_devel("notification from MCP");
		complete(&l_ctx.mcp_session.completion);
	} else {
		/* Session notification */
		struct mcp_session *session = NULL, *candidate;

		mutex_lock(&l_ctx.sessions_lock);
		list_for_each_entry(candidate, &l_ctx.sessions, list) {
			if (candidate->sid == id) {
				session = candidate;
				break;
			}
		}
		mutex_unlock(&l_ctx.sessions_lock);

		/* session is NULL if id not found */
		session_notif_handler(session, id, payload);
	}
}

static int debug_sessions(struct kasnprintf_buf *buf)
{
	struct mcp_session *session;
	int ret;

	/* Header */
	ret = kasnprintf(buf, "%20s %4s %-15s %-11s %4s\n",
			 "CPU clock", "ID", "state", "notif state", "ec");
	if (ret < 0)
		return ret;

	mutex_lock(&l_ctx.sessions_lock);
	list_for_each_entry(session, &l_ctx.sessions, list) {
		const char *state_str;
		u64 cpu_clk;
		s32 err;

		state_str = nq_session_state(&session->nq_session, &cpu_clk);
		mcp_get_err(session, &err);
		ret = kasnprintf(buf, "%20llu %4x %-15s %-11s %4d\n", cpu_clk,
				 session->sid, state_to_string(session->state),
				 state_str, err);
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

	return kasnprintf(buf, "%20llu %5d %-16s %5x %-8s %5d %6d%s\n",
			  cmd_info->cpu_clk, cmd_info->pid,
			  cmd_to_string(cmd_info->id), cmd_info->session_id,
			  state_str, cmd_info->errno, cmd_info->result,
			  cmd_info->uuid_str);
}

static int debug_last_cmds(struct kasnprintf_buf *buf)
{
	struct command_info *cmd_info;
	int i, ret = 0;

	/* Initialize MCP log */
	mutex_lock(&l_ctx.last_cmds_mutex);
	ret = kasnprintf(buf, "%20s %5s %-16s %5s %-8s %5s %6s %s\n",
			 "CPU clock", "PID", "command", "S-ID",
			 "state", "errno", "result", "UUID");
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

int mcp_init(void)
{
	l_ctx.buffer = nq_get_mcp_buffer();
	mutex_init(&l_ctx.buffer_mutex);
	init_completion(&l_ctx.mcp_session.completion);
	/* Setup notification queue mutex */
	mcp_session_init(&l_ctx.mcp_session);
	l_ctx.mcp_session.sid = SID_MCP;
	mutex_init(&l_ctx.unexp_notif_mutex);
	INIT_LIST_HEAD(&l_ctx.sessions);
	mutex_init(&l_ctx.sessions_lock);
	mutex_init(&l_ctx.last_cmds_mutex);

	l_ctx.timeout_period = MCP_TIMEOUT;

	nq_register_notif_handler(mcp_notif_handler, false);
	l_ctx.tee_stop_notifier.notifier_call = tee_stop_notifier_fn;
	nq_register_tee_stop_notifier(&l_ctx.tee_stop_notifier);

	return 0;
}

void mcp_exit(void)
{
	mark_mcp_dead();
	nq_unregister_tee_stop_notifier(&l_ctx.tee_stop_notifier);
}

int mcp_start(void)
{
	/* Create debugfs sessions and last commands entries */
	debugfs_create_file("mcp_sessions", 0400, g_ctx.debug_dir, NULL,
			    &debug_sessions_ops);
	debugfs_create_file("last_mcp_commands", 0400, g_ctx.debug_dir, NULL,
			    &debug_last_cmds_ops);
	debugfs_create_u32("mcp_timeout", 0600, g_ctx.debug_dir,
			   &l_ctx.timeout_period);
	return 0;
}

void mcp_stop(void)
{
	mcp_close();
}
