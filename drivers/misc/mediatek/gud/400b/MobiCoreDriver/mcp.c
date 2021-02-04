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
#include <linux/version.h>
#if KERNEL_VERSION(4, 11, 0) <= LINUX_VERSION_CODE
#include <linux/sched/clock.h>	/* local_clock */
#endif

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
#include "mcp.h"

/* respond timeout for MCP notification, in secs */
#define MCP_TIMEOUT		10
#define MCP_RETRIES		5
#define MCP_NF_QUEUE_SZ		8

static struct {
	struct mutex queue_lock;	/* Lock for MCP messages */
	struct completion complete;
	bool mcp_dead;
	struct mcp_session	mcp_session;	/* Pseudo session for MCP */
	/* Unexpected notification (during MCP open) */
	struct mutex		unexp_notif_mutex;
	struct notification	unexp_notif;
	/* Sessions */
	struct mutex		sessions_lock;
	struct list_head	sessions;
	/* Wait timeout */
	u32			timeout;
	/* Log of last MCP commands */
#define MCP_LOG_SIZE 256
	struct mutex		last_mcp_cmds_mutex; /* Log protection */
	struct mcp_command_info {
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
		enum mcp_result		result;	/* Command result */
		int			errno;	/* Return code */
	}				last_mcp_cmds[MCP_LOG_SIZE];
	int				last_mcp_cmds_index;
} l_ctx;

static const char *mcp_cmd_to_string(enum cmd_id id)
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
	case MC_MCP_CMD_SUSPEND:
		return "suspend";
	case MC_MCP_CMD_RESUME:
		return "resume";
	case MC_MCP_CMD_GET_MOBICORE_VERSION:
		return "get version";
	case MC_MCP_CMD_CLOSE_MCP:
		return "close mcp";
	case MC_MCP_CMD_LOAD_TOKEN:
		return "load token";
	case MC_MCP_CMD_CHECK_LOAD_TA:
		return "check load TA";
	}
	return "unknown";
}

static inline void mark_mcp_dead(void)
{
	struct mcp_session *session;

	l_ctx.mcp_dead = true;
	complete(&l_ctx.complete);
	/* Signal all potential waiters that SWd is going away */
	list_for_each_entry(session, &l_ctx.sessions, list)
		complete(&session->completion);
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
int mcp_session_waitnotif(struct mcp_session *session, s32 timeout,
			  bool silent_expiry)
{
	int ret = 0;

	mutex_lock(&session->notif_wait_lock);
	if (l_ctx.mcp_dead) {
		ret = -ENOTCONN;
		goto end;
	}

	if (!mcp_session_isrunning(session)) {
		ret = -ENXIO;
		goto end;
	}

	if (mcp_session_exitcode(session)) {
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
		ret = -ENOTCONN;
		goto end;
	}

	if (mcp_session_exitcode(session)) {
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
	if (ret && ((ret != -ETIME) || !silent_expiry)) {
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

s32 mcp_session_exitcode(struct mcp_session *session)
{
	s32 exit_code;

	mutex_lock(&session->exit_code_lock);
	exit_code = session->exit_code;
	mutex_unlock(&session->exit_code_lock);
	if (exit_code)
		mc_dev_info("session %x ec %d", session->sid, exit_code);

	return exit_code;
}

static inline int wait_mcp_notification(void)
{
	unsigned long timeout = msecs_to_jiffies(l_ctx.timeout * 1000);
	int try;

	/*
	 * Total timeout is l_ctx.timeout * MCP_RETRIES, but we check for
	 * a crash to try and terminate before then if things go wrong.
	 */
	for (try = 1; try <= MCP_RETRIES; try++) {
		u32 status;
		int ret;

		/*
		 * Wait non-interruptible to keep MCP synchronised even if
		 * caller is interrupted by signal.
		 */
		ret = wait_for_completion_timeout(&l_ctx.complete, timeout);
		if (ret > 0)
			return 0;

		mc_dev_notice("No answer after %ds", l_ctx.timeout * try);

		/* If SWd halted, exit now */
		if (!mc_fc_info(MC_EXT_INFO_ID_MCI_VERSION, &status, NULL) &&
		    status == MC_STATUS_HALT)
			break;
	}

	/* TEE halted or dead: dump status and SMC log */
	mark_mcp_dead();
	nq_dump_status();

	return -ETIME;
}

static int mcp_cmd(union mcp_message *cmd,
		   /* The fields below are for debug purpose only */
		   u32 in_session_id,
		   u32 *out_session_id,
		   struct mc_uuid_t *uuid)
{
	int err = 0, ret = -ENOTCONN;
	union mcp_message *msg = nq_get_mcp_message();
	enum cmd_id cmd_id = cmd->cmd_header.cmd_id;
	struct mcp_command_info *cmd_info;

	/* Initialize MCP log */
	mutex_lock(&l_ctx.last_mcp_cmds_mutex);
	cmd_info = &l_ctx.last_mcp_cmds[l_ctx.last_mcp_cmds_index];
	cmd_info->cpu_clk = local_clock();
	cmd_info->pid = current->pid;
	cmd_info->cpu_clk = local_clock();
	cmd_info->id = cmd_id;
	cmd_info->session_id = in_session_id;
	if (uuid) {
		/* display UUID because it's an openSession cmd */
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
	cmd_info->result = MC_MCP_RET_OK;
	cmd_info->errno = 0;
	if (++l_ctx.last_mcp_cmds_index >= MCP_LOG_SIZE)
		l_ctx.last_mcp_cmds_index = 0;
	mutex_unlock(&l_ctx.last_mcp_cmds_mutex);

	mutex_lock(&l_ctx.queue_lock);
	if (l_ctx.mcp_dead)
		goto out;

	/* Copy message to MCP buffer */
	memcpy(msg, cmd, sizeof(*msg));

	/* Poke TEE */
	ret = mcp_notify(&l_ctx.mcp_session);
	if (ret)
		goto out;

	/* Update MCP log */
	mutex_lock(&l_ctx.last_mcp_cmds_mutex);
	cmd_info->state = SENT;
	mutex_unlock(&l_ctx.last_mcp_cmds_mutex);
	ret = wait_mcp_notification();
	if (ret)
		goto out;

	/* Check response ID */
	if (msg->rsp_header.rsp_id != (cmd_id | FLAG_RESPONSE)) {
		mc_dev_notice("MCP command got invalid response (0x%X)",
			   msg->rsp_header.rsp_id);
		ret = -EBADE;
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
	case MC_MCP_RET_ERR_SERVICE_KILLED:
		err = -ECONNRESET;
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
	mutex_lock(&l_ctx.last_mcp_cmds_mutex);
	if (ret) {
		cmd_info->state = FAILED;
		cmd_info->errno = -ret;
	} else {
		cmd_info->state = COMPLETE;
		cmd_info->result = msg->rsp_header.result;
		cmd_info->errno = -err;
		/* For open session: get SID */
		if (!err && out_session_id)
			cmd_info->session_id = *out_session_id;
	}
	mutex_unlock(&l_ctx.last_mcp_cmds_mutex);
	mutex_unlock(&l_ctx.queue_lock);
	if (ret) {
		mc_dev_notice("%s: sending failed, ret = %d",
			   mcp_cmd_to_string(cmd_id), ret);
		return ret;
	}

	if (err) {
		if (cmd_id == MC_MCP_CMD_CLOSE_SESSION && err == -EAGAIN)
			mc_dev_devel("%s: try again",
				     mcp_cmd_to_string(cmd_id));
		else
			mc_dev_notice("%s: res %d/ret %d",
				   mcp_cmd_to_string(cmd_id),
				   msg->rsp_header.result, err);
		return err;
	}

	return 0;
}

int mcp_get_version(struct mc_version_info *version_info)
{
	static struct mc_version_info static_version_info;

	/* If cache empty, get version from the SWd and cache it */
	if (!static_version_info.version_nwd) {
		u32 version = MC_VERSION(MCDRVMODULEAPI_VERSION_MAJOR,
					 MCDRVMODULEAPI_VERSION_MINOR);
		union mcp_message cmd;
		int ret;

		memset(&cmd, 0, sizeof(cmd));
		cmd.cmd_header.cmd_id = MC_MCP_CMD_GET_MOBICORE_VERSION;
		ret = mcp_cmd(&cmd, 0, NULL, NULL);
		if (ret)
			return ret;

		memcpy(&static_version_info, &cmd.rsp_get_version.version_info,
		       sizeof(static_version_info));
		/*
		 * The CMP version is meaningless in this case, and is replaced
		 * by the driver's own version.
		 */
		static_version_info.version_nwd = version;
	}

	/* Copy cached version */
	memcpy(version_info, &static_version_info, sizeof(*version_info));
	return 0;
}

int mcp_load_token(uintptr_t data, const struct mcp_buffer_map *map)
{
	union mcp_message cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_LOAD_TOKEN;
	cmd.cmd_load_token.wsm_data_type = map->type;
	cmd.cmd_load_token.adr_load_data = map->phys_addr;
	cmd.cmd_load_token.ofs_load_data = map->offset;
	cmd.cmd_load_token.len_load_data = map->length;
	return mcp_cmd(&cmd, 0, NULL, NULL);
}

int mcp_load_check(const struct tee_object *obj,
		   const struct mcp_buffer_map *map)
{
	const union mclf_header *header;
	union mcp_message cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_CHECK_LOAD_TA;
	/* Data */
	cmd.cmd_check_load.wsm_data_type = map->type;
	cmd.cmd_check_load.adr_load_data = map->phys_addr;
	cmd.cmd_check_load.ofs_load_data = map->offset;
	cmd.cmd_check_load.len_load_data = map->length;
	/* Header */
	header = (union mclf_header *)(obj->data + obj->header_length);
	cmd.cmd_check_load.uuid = header->mclf_header_v2.uuid;
	return mcp_cmd(&cmd, 0, NULL, &cmd.cmd_check_load.uuid);
}

int mcp_open_session(struct mcp_session *session,
		     const struct tee_object *obj,
		     const struct mcp_buffer_map *map,
		     const struct mcp_buffer_map *tci_map)
{
	static DEFINE_MUTEX(local_mutex);
	const union mclf_header *header;
	union mcp_message cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_OPEN_SESSION;
	/* Data */
	cmd.cmd_open.wsm_data_type = map->type;
	cmd.cmd_open.adr_load_data = map->phys_addr;
	cmd.cmd_open.ofs_load_data = map->offset;
	cmd.cmd_open.len_load_data = map->length;
	/* Buffer */
	if (tci_map) {
		cmd.cmd_open.wsmtype_tci = tci_map->type;
		cmd.cmd_open.adr_tci_buffer = tci_map->phys_addr;
		cmd.cmd_open.ofs_tci_buffer = tci_map->offset;
		cmd.cmd_open.len_tci_buffer = tci_map->length;
	} else {
		cmd.cmd_open.wsmtype_tci = WSM_INVALID;
	}
	/* Header */
	header = (union mclf_header *)(obj->data + obj->header_length);
	cmd.cmd_open.uuid = header->mclf_header_v2.uuid;
	cmd.cmd_open.is_gpta = nq_session_is_gp(&session->nq_session);
	/* Reset unexpected notification */
	mutex_lock(&local_mutex);
	l_ctx.unexp_notif.session_id = SID_MCP;	/* Cannot be */
	if (!g_ctx.f_client_login)
		memcpy(&cmd.cmd_open.tl_header, header,
		       sizeof(cmd.cmd_open.tl_header));
	else
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
	int ret;

	/* Signal a potential waiter that SWd session is going away */
	complete(&session->completion);
	/* Send MCP command */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_CLOSE_SESSION;
	cmd.cmd_close.session_id = session->sid;
	ret = mcp_cmd(&cmd, cmd.cmd_close.session_id, NULL, NULL);
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
	mc_dev_devel("close session %x ret %d state %d",
		     session->sid, ret, session->state);
	return ret;
}

/*
 * Session is to be removed from NWd records as SWd is dead
 */
void mcp_kill_session(struct mcp_session *session)
{
	mutex_lock(&l_ctx.sessions_lock);
	list_del(&session->list);
	nq_session_exit(&session->nq_session);
	mutex_unlock(&l_ctx.sessions_lock);
}

int mcp_map(u32 session_id, struct mcp_buffer_map *map)
{
	union mcp_message cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_MAP;
	cmd.cmd_map.session_id = session_id;
	cmd.cmd_map.wsm_type = map->type;
	cmd.cmd_map.adr_buffer = map->phys_addr;
	cmd.cmd_map.ofs_buffer = map->offset;
	cmd.cmd_map.len_buffer = map->length;
	cmd.cmd_map.flags = map->flags;
	ret = mcp_cmd(&cmd, session_id, NULL, NULL);
	if (!ret) {
		map->secure_va = cmd.rsp_map.secure_va;
		atomic_inc(&g_ctx.c_maps);
	}

	return ret;
}

int mcp_unmap(u32 session_id, const struct mcp_buffer_map *map)
{
	union mcp_message cmd;
	int ret;

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
	if (session->sid == SID_MCP)
		mc_dev_devel("notify MCP");
	else
		mc_dev_devel("notify session %x", session->sid);

	return nq_session_notify(&session->nq_session, session->sid, 0);
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
			/*
			 * In GP, the only way to recover the sessions exit code
			 * is to call TEEC_InvokeCommand which will notify. But
			 * notifying a dead session would change the exit code
			 * to ERR_SID_NOT_ACTIVE, hence the check below.
			 */
			if (!nq_session_is_gp(&session->nq_session) ||
			    !session->exit_code ||
			    payload != ERR_SID_NOT_ACTIVE)
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
		complete(&l_ctx.complete);
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

int mcp_start(void)
{
	return 0;
}

void mcp_stop(void)
{
	mcp_close();
}

int mcp_init(void)
{
	mutex_init(&l_ctx.queue_lock);
	init_completion(&l_ctx.complete);
	/* Setup notification queue mutex */
	mcp_session_init(&l_ctx.mcp_session);
	l_ctx.mcp_session.sid = SID_MCP;
	mutex_init(&l_ctx.unexp_notif_mutex);
	INIT_LIST_HEAD(&l_ctx.sessions);
	mutex_init(&l_ctx.sessions_lock);
	mutex_init(&l_ctx.last_mcp_cmds_mutex);

	l_ctx.timeout = MCP_TIMEOUT;
	debugfs_create_u32("mcp_timeout", 0600, g_ctx.debug_dir,
			   &l_ctx.timeout);

	nq_register_notif_handler(mcp_notif_handler, false);

	return 0;
}

void mcp_exit(void)
{
	mark_mcp_dead();
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

int mcp_debug_sessions(struct kasnprintf_buf *buf)
{
	struct mcp_session *session;
	int ret;

	/* Header */
	ret = kasnprintf(buf, "%20s %4s %4s %4s %-15s %-11s\n",
			 "CPU clock", "ID", "type", "ec", "state",
			 "notif state");
	if (ret < 0)
		return ret;

	mutex_lock(&l_ctx.sessions_lock);
	list_for_each_entry(session, &l_ctx.sessions, list) {
		s32 exit_code = mcp_session_exitcode(session);
		struct nq_session *nq_session = &session->nq_session;

		ret = kasnprintf(buf, "%20llu %4x %-4s %4d %-15s %-11s\n",
				 nq_session_notif_cpu_clk(nq_session),
				 session->sid,
				 nq_session_is_gp(nq_session) ? "GP" : "MC",
				 exit_code, state_to_string(session->state),
				 nq_session_state_string(nq_session));
		if (ret < 0)
			break;
	}
	mutex_unlock(&l_ctx.sessions_lock);
	return ret;
}

static inline int show_mcp_log_entry(struct kasnprintf_buf *buf,
				     struct mcp_command_info *cmd_info)
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

	return kasnprintf(buf, "%20llu %5d %-13s %5x %-8s %6d %5d%s\n",
			  cmd_info->cpu_clk, cmd_info->pid,
			  mcp_cmd_to_string(cmd_info->id), cmd_info->session_id,
			  state_str, cmd_info->result, cmd_info->errno,
			  cmd_info->uuid_str);
}

int mcp_debug_mcpcmds(struct kasnprintf_buf *buf)
{
	struct mcp_command_info *cmd_info;
	int i, ret = 0;

	/* Initialize MCP log */
	mutex_lock(&l_ctx.last_mcp_cmds_mutex);
	ret = kasnprintf(buf, "%20s %5s %-13s %5s %-8s %6s %5s %s\n",
			 "CPU clock", "PID", "command", "S-ID",
			 "state", "result", "errno", "UUID");
	if (ret < 0)
		goto out;

	cmd_info = &l_ctx.last_mcp_cmds[l_ctx.last_mcp_cmds_index];
	if (cmd_info->state != UNUSED)
		/* Buffer has wrapped around, dump end (oldest records) */
		for (i = l_ctx.last_mcp_cmds_index; i < MCP_LOG_SIZE; i++) {
			ret = show_mcp_log_entry(buf, cmd_info++);
			if (ret < 0)
				goto out;
		}

	/* Dump first records */
	cmd_info = &l_ctx.last_mcp_cmds[0];
	for (i = 0; i < l_ctx.last_mcp_cmds_index; i++) {
		ret = show_mcp_log_entry(buf, cmd_info++);
		if (ret < 0)
			goto out;
	}

out:
	mutex_unlock(&l_ctx.last_mcp_cmds_mutex);
	return ret;
}
