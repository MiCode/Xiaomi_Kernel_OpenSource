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

#include "platform.h"		/* IRQ number */
#include "main.h"
#include "fastcall.h"
#include "logging.h"
#include "mcp.h"

/* respond timeout for MCP notification, in secs */
#define MCP_TIMEOUT		10
#define MCP_RETRIES		5
#define MCP_NF_QUEUE_SZ		8
#define NQ_NUM_ELEMS		64

static const struct {
	unsigned int index;
	const char *msg;
} status_map[] = {
	/**< MobiCore control flags */
	{ MC_EXT_INFO_ID_FLAGS, "flags"},
	/**< MobiCore halt condition code */
	{ MC_EXT_INFO_ID_HALT_CODE, "haltCode"},
	/**< MobiCore halt condition instruction pointer */
	{ MC_EXT_INFO_ID_HALT_IP, "haltIp"},
	/**< MobiCore fault counter */
	{ MC_EXT_INFO_ID_FAULT_CNT, "faultRec.cnt"},
	/**< MobiCore last fault cause */
	{ MC_EXT_INFO_ID_FAULT_CAUSE, "faultRec.cause"},
	/**< MobiCore last fault meta */
	{ MC_EXT_INFO_ID_FAULT_META, "faultRec.meta"},
	/**< MobiCore last fault threadid */
	{ MC_EXT_INFO_ID_FAULT_THREAD, "faultRec.thread"},
	/**< MobiCore last fault instruction pointer */
	{ MC_EXT_INFO_ID_FAULT_IP, "faultRec.ip"},
	/**< MobiCore last fault stack pointer */
	{ MC_EXT_INFO_ID_FAULT_SP, "faultRec.sp"},
	/**< MobiCore last fault ARM arch information */
	{ MC_EXT_INFO_ID_FAULT_ARCH_DFSR, "faultRec.arch.dfsr"},
	/**< MobiCore last fault ARM arch information */
	{ MC_EXT_INFO_ID_FAULT_ARCH_ADFSR, "faultRec.arch.adfsr"},
	/**< MobiCore last fault ARM arch information */
	{ MC_EXT_INFO_ID_FAULT_ARCH_DFAR, "faultRec.arch.dfar"},
	/**< MobiCore last fault ARM arch information */
	{ MC_EXT_INFO_ID_FAULT_ARCH_IFSR, "faultRec.arch.ifsr"},
	/**< MobiCore last fault ARM arch information */
	{ MC_EXT_INFO_ID_FAULT_ARCH_AIFSR, "faultRec.arch.aifsr"},
	/**< MobiCore last fault ARM arch information */
	{ MC_EXT_INFO_ID_FAULT_ARCH_IFAR, "faultRec.arch.ifar"},
	/**< MobiCore configured by Daemon via fc_init flag */
	{ MC_EXT_INFO_ID_MC_CONFIGURED, "mcData.flags"},
	/**< MobiCore exception handler last partner */
	{ MC_EXT_INFO_ID_MC_EXC_PARTNER, "mcExcep.partner"},
	/**< MobiCore exception handler last peer */
	{ MC_EXT_INFO_ID_MC_EXC_IPCPEER, "mcExcep.peer"},
	/**< MobiCore exception handler last IPC message */
	{ MC_EXT_INFO_ID_MC_EXC_IPCMSG, "mcExcep.cause"},
	/**< MobiCore exception handler last IPC data */
	{MC_EXT_INFO_ID_MC_EXC_IPCDATA, "mcExcep.meta"},
};

static struct mcp_context {
	struct mutex buffer_lock;	/* Lock on SWd communication buffer */
	struct mutex queue_lock;	/* Lock for MCP messages */
	struct mcp_buffer *mcp_buffer;
	struct completion complete;
	struct task_struct *irq_bh_thread;
	struct completion irq_bh_complete;
	bool irq_bh_active;
	bool mcp_dead;
	int irq;
	int (*scheduler_cb)(enum mcp_scheduler_commands);
	void (*crashhandler_cb)(void);
	/* MobiCore MCI information */
	unsigned int order;
	union {
		void		*base;
		struct {
			struct notification_queue *tx;
			struct notification_queue *rx;
		} nq;
	};
	/*
	 * This notifications list is to be used to queue notifications when the
	 * notification queue overflows, so no session gets its notification
	 * lost, especially MCP.
	 */
	struct mutex		notifications_mutex;
	struct list_head	notifications;
	struct mcp_session	mcp_session;	/* Pseudo session for MCP */
	/* Unexpected notification (during MCP open) */
	struct mutex		unexp_notif_mutex;
	struct notification	unexp_notif;
	/* Sessions */
	struct mutex		sessions_lock;
	struct list_head	sessions;
	/* Dump buffer */
	struct kasnprintf_buf	dump;
	/* Time */
	struct mcp_time		*time;
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
} mcp_ctx;

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
	case MC_MCP_CMD_MULTIMAP:
		return "multimap";
	case MC_MCP_CMD_MULTIUNMAP:
		return "multiunmap";
	}
	return "unknown";
}

static inline void mark_mcp_dead(void)
{
	struct mcp_session *session;

	mcp_ctx.mcp_dead = true;
	complete(&mcp_ctx.complete);
	/* Signal all potential waiters that SWd is going away */
	list_for_each_entry(session, &mcp_ctx.sessions, list)
		complete(&session->completion);
}

static inline int mcp_set_sleep_mode_rq(u16 sleep_req)
{
	mutex_lock(&mcp_ctx.buffer_lock);
	mcp_ctx.mcp_buffer->flags.sleep_mode.sleep_req = sleep_req;
	mutex_unlock(&mcp_ctx.buffer_lock);
	return 0;
}

static ssize_t debug_crashdump_read(struct file *file, char __user *user_buf,
				    size_t count, loff_t *ppos)
{
	if (mcp_ctx.dump.off)
		return simple_read_from_buffer(user_buf, count, ppos,
					       mcp_ctx.dump.buf,
					       mcp_ctx.dump.off);

	return 0;
}

static const struct file_operations mc_debug_crashdump_ops = {
	.read = debug_crashdump_read,
	.llseek = default_llseek,
};

static ssize_t debug_smclog_read(struct file *file, char __user *user_buf,
				 size_t count, loff_t *ppos)
{
	return debug_generic_read(file, user_buf, count, ppos,
				  mc_fastcall_debug_smclog);
}

static const struct file_operations mc_debug_smclog_ops = {
	.read = debug_smclog_read,
	.llseek = default_llseek,
	.release = debug_generic_release,
};

static void mcp_dump_mobicore_status(void)
{
	char uuid_str[33];
	int ret = 0;
	size_t i;

	if (mcp_ctx.dump.off)
		ret = -EBUSY;

	mc_dev_notice("TEE halted. Status dump:");
	for (i = 0; i < (size_t)ARRAY_SIZE(status_map); i++) {
		u32 info;

		if (!mc_fc_info(status_map[i].index, NULL, &info)) {
			mc_dev_notice("  %-20s= 0x%08x\n",
				   status_map[i].msg, info);
			if (ret >= 0)
				ret = kasnprintf(&mcp_ctx.dump,
						 "%-20s= 0x%08x\n",
						 status_map[i].msg, info);
		}
	}

	/* construct UUID string */
	for (i = 0; i < 4; i++) {
		u32 info;
		size_t j;

		if (mc_fc_info(MC_EXT_INFO_ID_MC_EXC_UUID + i, NULL, &info))
			return;

		for (j = 0; j < sizeof(info); j++) {
			snprintf(&uuid_str[(i * sizeof(info) + j) * 2], 3,
				 "%02x", (info >> (j * 8)) & 0xff);
		}
	}

	mc_dev_notice("  %-20s= 0x%s\n", "mcExcep.uuid", uuid_str);
	if (ret >= 0)
		ret = kasnprintf(&mcp_ctx.dump, "%-20s= 0x%s\n", "mcExcep.uuid",
				 uuid_str);

	if (ret < 0) {
		kfree(mcp_ctx.dump.buf);
		mcp_ctx.dump.off = 0;
		return;
	}

	debugfs_create_file("crashdump", 0400, g_ctx.debug_dir, NULL,
			    &mc_debug_crashdump_ops);
	debugfs_create_file("last_smc_commands", 0400, g_ctx.debug_dir, NULL,
			    &mc_debug_smclog_ops);
	if (mcp_ctx.crashhandler_cb)
		mcp_ctx.crashhandler_cb();
}

void mcp_session_init(struct mcp_session *session, bool is_gp,
		      const struct identity *identity)
{
	/* close_work is initialized by the caller */
	INIT_LIST_HEAD(&session->list);
	INIT_LIST_HEAD(&session->notifications_list);
	mutex_init(&session->notif_wait_lock);
	init_completion(&session->completion);
	mutex_init(&session->exit_code_lock);
	session->state = MCP_SESSION_RUNNING;
	session->is_gp = is_gp;
	if (is_gp)
		session->identity = *identity;
}

static inline bool mcp_session_isrunning(struct mcp_session *session)
{
	bool ret;

	mutex_lock(&mcp_ctx.sessions_lock);
	ret = session->state == MCP_SESSION_RUNNING;
	mutex_unlock(&mcp_ctx.sessions_lock);
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
	if (mcp_ctx.mcp_dead) {
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

	if (mcp_ctx.mcp_dead) {
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
	mutex_lock(&mcp_ctx.notifications_mutex);
	if (!ret)
		session->notif_state = MCP_NOTIF_CONSUMED;
	else if (ret != -ERESTARTSYS)
		session->notif_state = MCP_NOTIF_DEAD;
	session->notif_cpu_clk = local_clock();
	mutex_unlock(&mcp_ctx.notifications_mutex);

	mutex_unlock(&session->notif_wait_lock);
	if (ret && ((ret != -ETIME) || !silent_expiry)) {
#ifdef CONFIG_FREEZER
		if (ret == -ERESTARTSYS && system_freezing_cnt.counter == 1)
			mc_dev_devel("freezing session %x\n", session->id);
		else
#endif
			mc_dev_devel("session %x ec %d ret %d\n",
				     session->id, session->exit_code, ret);
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
		mc_dev_info("session %x ec %d\n", session->id, exit_code);

	return exit_code;
}

int mcp_suspend(void)
{
	return mcp_set_sleep_mode_rq(MC_FLAG_REQ_TO_SLEEP);
}

int mcp_resume(void)
{
	return mcp_set_sleep_mode_rq(MC_FLAG_NO_SLEEP_REQ);
}

bool mcp_suspended(void)
{
	struct mcp_flags *flags = &mcp_ctx.mcp_buffer->flags;
	bool ret;

	mutex_lock(&mcp_ctx.buffer_lock);
	ret = flags->sleep_mode.ready_to_sleep & MC_STATE_READY_TO_SLEEP;
	if (!ret) {
		mc_dev_devel("IDLE=%d\n", flags->schedule);
		mc_dev_devel("Request Sleep=%d\n", flags->sleep_mode.sleep_req);
		mc_dev_devel("Sleep Ready=%d\n",
			     flags->sleep_mode.ready_to_sleep);
	}

	mutex_unlock(&mcp_ctx.buffer_lock);
	return ret;
}

bool mcp_get_idle_timeout(s32 *timeout)
{
	u32 schedule;
	bool ret;

	mutex_lock(&mcp_ctx.buffer_lock);
	schedule = mcp_ctx.mcp_buffer->flags.schedule;
	if (schedule == MC_FLAG_SCHEDULE_IDLE) {
		if (g_ctx.f_timeout)
			*timeout = mcp_ctx.mcp_buffer->flags.timeout_ms;
		else
			*timeout = -1;

		ret = true;
	} else {
		ret = false;
	}

	mutex_unlock(&mcp_ctx.buffer_lock);
	return ret;
}

void mcp_reset_idle_timeout(void)
{
	mutex_lock(&mcp_ctx.buffer_lock);
	mcp_ctx.mcp_buffer->flags.timeout_ms = -1;
	mutex_unlock(&mcp_ctx.buffer_lock);
}

static inline int wait_mcp_notification(void)
{
	unsigned long timeout = msecs_to_jiffies(mcp_ctx.timeout * 1000);
	int try;

	/*
	 * Total timeout is mcp_ctx.timeout * MCP_RETRIES, but we check for
	 * a crash to try and terminate before then if things go wrong.
	 */
	for (try = 1; try <= MCP_RETRIES; try++) {
		u32 status;
		int ret;

		/*
		 * Wait non-interruptible to keep MCP synchronised even if
		 * caller is interrupted by signal.
		 */
		ret = wait_for_completion_timeout(&mcp_ctx.complete, timeout);
		if (ret > 0)
			return 0;

		mc_dev_notice("No answer after %ds\n", mcp_ctx.timeout * try);

		/* If SWd halted, exit now */
		if (!mc_fc_info(MC_EXT_INFO_ID_MCI_VERSION, &status, NULL) &&
		    (status == MC_STATUS_HALT))
			break;
	}

	/* TEE halted or dead: dump status and SMC log */
	mark_mcp_dead();
	mcp_dump_mobicore_status();

	return -ETIME;
}

static int mcp_cmd(union mcp_message *cmd,
		   /* The fields below are for debug purpose only */
		   u32 in_session_id,
		   u32 *out_session_id,
		   struct mc_uuid_t *uuid)
{
	int err = 0, ret = -ENOTCONN;
	union mcp_message *msg = &mcp_ctx.mcp_buffer->message;
	enum cmd_id cmd_id = cmd->cmd_header.cmd_id;
	struct mcp_command_info *cmd_info;

	/* Initialize MCP log */
	mutex_lock(&mcp_ctx.last_mcp_cmds_mutex);
	cmd_info = &mcp_ctx.last_mcp_cmds[mcp_ctx.last_mcp_cmds_index];
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
	if (++mcp_ctx.last_mcp_cmds_index >= MCP_LOG_SIZE)
		mcp_ctx.last_mcp_cmds_index = 0;
	mutex_unlock(&mcp_ctx.last_mcp_cmds_mutex);

	mutex_lock(&mcp_ctx.queue_lock);
	if (mcp_ctx.mcp_dead)
		goto out;

	/* Copy message to MCP buffer */
	memcpy(msg, cmd, sizeof(*msg));

	/* Poke TEE */
	ret = mcp_notify(&mcp_ctx.mcp_session);
	if (ret)
		goto out;

	/* Update MCP log */
	mutex_lock(&mcp_ctx.last_mcp_cmds_mutex);
	cmd_info->state = SENT;
	mutex_unlock(&mcp_ctx.last_mcp_cmds_mutex);
	ret = wait_mcp_notification();
	if (ret)
		goto out;

	/* Check response ID */
	if (msg->rsp_header.rsp_id != (cmd_id | FLAG_RESPONSE)) {
		mc_dev_notice("MCP command got invalid response (0x%X)\n",
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
	default:
		err = -EPERM;
	}

	/* Copy response back to caller struct */
	memcpy(cmd, msg, sizeof(*cmd));

out:
	/* Update MCP log */
	mutex_lock(&mcp_ctx.last_mcp_cmds_mutex);
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
	mutex_unlock(&mcp_ctx.last_mcp_cmds_mutex);
	mutex_unlock(&mcp_ctx.queue_lock);
	if (ret) {
		mc_dev_notice("%s: sending failed, ret = %d\n",
			   mcp_cmd_to_string(cmd_id), ret);
		return ret;
	}

	if (err) {
		if ((cmd_id == MC_MCP_CMD_CLOSE_SESSION) && (err == -EAGAIN))
			mc_dev_devel("%s: try again\n",
				     mcp_cmd_to_string(cmd_id));
		else
			mc_dev_notice("%s: res %d/ret %d\n",
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
	cmd.cmd_open.is_gpta = session->is_gp;
	/* Reset unexpected notification */
	mutex_lock(&local_mutex);
	mcp_ctx.unexp_notif.session_id = SID_MCP;	/* Cannot be */
	if (!g_ctx.f_client_login) {
		memcpy(&cmd.cmd_open.tl_header, header,
		       sizeof(cmd.cmd_open.tl_header));
	} else {
		cmd.cmd_open.cmd_open_data.mclf_magic = MC_GP_CLIENT_AUTH_MAGIC;
		if (session->is_gp)
			cmd.cmd_open.cmd_open_data.identity = session->identity;
	}

	/* Send MCP open command */
	ret = mcp_cmd(&cmd, 0, &cmd.rsp_open.session_id, &cmd.cmd_open.uuid);
	/* Make sure we have a valid session ID */
	if (!ret && !cmd.rsp_open.session_id)
		ret = -EBADE;

	if (!ret) {
		session->id = cmd.rsp_open.session_id;
		/* Add to list of sessions */
		mutex_lock(&mcp_ctx.sessions_lock);
		list_add_tail(&session->list, &mcp_ctx.sessions);
		mutex_unlock(&mcp_ctx.sessions_lock);
		/* Check for spurious notification */
		mutex_lock(&mcp_ctx.unexp_notif_mutex);
		if (mcp_ctx.unexp_notif.session_id == session->id) {
			mutex_lock(&session->exit_code_lock);
			session->exit_code = mcp_ctx.unexp_notif.payload;
			mutex_unlock(&session->exit_code_lock);
			complete(&session->completion);
		}

		mutex_unlock(&mcp_ctx.unexp_notif_mutex);
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

	/* state is either MCP_SESSION_RUNNING or MCP_SESSION_CLOSING_GP */
	mutex_lock(&mcp_ctx.sessions_lock);
	if (session->state == MCP_SESSION_RUNNING)
		session->state = MCP_SESSION_CLOSE_REQUESTED;

	mutex_unlock(&mcp_ctx.sessions_lock);
	/* Signal a potential waiter that SWd session is going away */
	complete(&session->completion);
	/* Send MCP command */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_CLOSE_SESSION;
	cmd.cmd_close.session_id = session->id;
	ret = mcp_cmd(&cmd, session->id, NULL, NULL);
	mutex_lock(&mcp_ctx.sessions_lock);
	/*
	 * The GP TA may already have sent its exit code, in which case the
	 * state has also been changed to MCP_SESSION_CLOSE_NOTIFIED.
	 */
	if (!ret) {
		session->state = MCP_SESSION_CLOSED;
		list_del(&session->list);
		mutex_lock(&mcp_ctx.notifications_mutex);
		list_del(&session->notifications_list);
		mutex_unlock(&mcp_ctx.notifications_mutex);
	} else if (ret == -EAGAIN) {
		if (session->state == MCP_SESSION_CLOSE_NOTIFIED)
			/* GP TA already closed */
			schedule_work(&session->close_work);

		session->state = MCP_SESSION_CLOSING_GP;
	} else {
		/* Something is not right, assume session is still running */
		session->state = MCP_SESSION_CLOSE_FAILED;
	}
	mutex_unlock(&mcp_ctx.sessions_lock);
	mc_dev_devel("close session %x ret %d state %d\n", session->id, ret,
		     session->state);
	return ret;
}

/*
 * Session is to be removed from NWd records as SWd is dead
 */
void mcp_kill_session(struct mcp_session *session)
{
	mutex_lock(&mcp_ctx.sessions_lock);
	list_del(&session->list);
	mutex_lock(&mcp_ctx.notifications_mutex);
	list_del(&session->notifications_list);
	mutex_unlock(&mcp_ctx.notifications_mutex);
	mutex_unlock(&mcp_ctx.sessions_lock);
}

static int _mcp_map(u32 session_id, struct mcp_buffer_map *map)
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
	ret = mcp_cmd(&cmd, session_id, NULL, NULL);
	if (!ret)
		map->secure_va = cmd.rsp_map.secure_va;

	return ret;
}

static int _mcp_unmap(u32 session_id, const struct mcp_buffer_map *map)
{
	union mcp_message cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_UNMAP;
	cmd.cmd_unmap.session_id = session_id;
	cmd.cmd_unmap.wsm_type = map->type;
	cmd.cmd_unmap.virtual_buffer_len = map->length;
	cmd.cmd_unmap.secure_va = map->secure_va;
	return mcp_cmd(&cmd, session_id, NULL, NULL);
}

static int _mcp_multimap(u32 session_id, struct mcp_buffer_map *maps)
{
	struct mcp_buffer_map *map = maps;
	union mcp_message cmd;
	struct buffer_map *buf = cmd.cmd_multimap.bufs;
	int ret = 0;
	u32 i;

	/* Prepare command */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_MULTIMAP;
	cmd.cmd_multimap.session_id = session_id;
	for (i = 0; i < MC_MAP_MAX; i++, map++, buf++) {
		buf->wsm_type = map->type;
		buf->adr_buffer = map->phys_addr;
		buf->ofs_buffer = map->offset;
		buf->len_buffer = map->length;
	}

	ret = mcp_cmd(&cmd, session_id, NULL, NULL);
	if (ret)
		return ret;

	/* Return secure virtual addresses */
	map = maps;
	for (i = 0; i < MC_MAP_MAX; i++, map++)
		map->secure_va = cmd.rsp_multimap.secure_va[i];

	return 0;
}

static int _mcp_multiunmap(u32 session_id, const struct mcp_buffer_map *maps)
{
	const struct mcp_buffer_map *map = maps;
	union mcp_message cmd;
	struct buffer_unmap *buf = cmd.cmd_multiunmap.bufs;
	u32 i;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_MULTIUNMAP;
	cmd.cmd_multiunmap.session_id = session_id;
	for (i = 0; i < MC_MAP_MAX; i++, map++, buf++) {
		buf->secure_va = map->secure_va;
		buf->len_buffer = map->length;
	}

	return mcp_cmd(&cmd, session_id, NULL, NULL);
}

int mcp_multimap(u32 session_id, struct mcp_buffer_map *maps, bool use_multimap)
{
	int i, ret = 0;

	/* Use multimap feature if available */
	if (g_ctx.f_multimap && use_multimap) {
		/* Send MCP message to map buffers in SWd */
		ret = _mcp_multimap(session_id, maps);
		if (!ret) {
			for (i = 0; i < MC_MAP_MAX; i++)
				if (maps[i].secure_va)
					atomic_inc(&g_ctx.c_maps);
		} else {
			mc_dev_devel("multimap failed: %d\n", ret);
		}

		return ret;
	}

	/* Revert to old-style map */
	for (i = 0; i < MC_MAP_MAX; i++) {
		if (maps[i].type == WSM_INVALID)
			continue;

		/* Send MCP message to map buffer in SWd */
		ret = _mcp_map(session_id, &maps[i]);
		if (ret) {
			mc_dev_devel("maps[%d] map failed: %d\n", i, ret);
			break;
		}

		atomic_inc(&g_ctx.c_maps);
	}

	/* On failure, unmap what was mapped */
	if (ret) {
		for (i = 0; i < MC_MAP_MAX; i++) {
			if ((maps[i].type == WSM_INVALID) || !maps[i].secure_va)
				continue;

			if (_mcp_unmap(session_id, &maps[i]))
				mc_dev_devel("maps[%d] unmap failed: %d\n",
					     i, ret);
			else
				atomic_dec(&g_ctx.c_maps);
		}
	}

	return ret;
}

int mcp_multiunmap(u32 session_id, struct mcp_buffer_map *maps,
		   bool use_multimap)
{
	int i, ret = 0;

	/* Use multimap feature if available */
	if (g_ctx.f_multimap && use_multimap) {
		/* Send MCP command to unmap buffers in SWd */
		ret = _mcp_multiunmap(session_id, maps);
		if (ret) {
			mc_dev_devel("mcp_multiunmap failed: %d\n", ret);
		} else {
			for (i = 0; i < MC_MAP_MAX; i++)
				if (maps[i].secure_va)
					atomic_dec(&g_ctx.c_maps);
		}
	} else {
		for (i = 0; i < MC_MAP_MAX; i++) {
			if (!maps[i].secure_va)
				continue;

			/* Send MCP command to unmap buffer in SWd */
			ret = _mcp_unmap(session_id, &maps[i]);
			if (ret)
				mc_dev_devel("maps[%d] unmap failed: %d\n",
					     i, ret);
				/* Keep going */
			else
				atomic_dec(&g_ctx.c_maps);
		}
	}

	return ret;
}

static int mcp_close(void)
{
	union mcp_message cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_CLOSE_MCP;
	return mcp_cmd(&cmd, 0, NULL, NULL);
}

static inline bool notif_queue_full(void)
{
	struct notification_queue *tx = mcp_ctx.nq.tx;

	return (tx->hdr.write_cnt - tx->hdr.read_cnt) == tx->hdr.queue_size;
}

static inline void notif_queue_push(u32 session_id)
{
	struct notification_queue_header *hdr = &mcp_ctx.nq.tx->hdr;
	u32 i = hdr->write_cnt % hdr->queue_size;

	mcp_ctx.nq.tx->notification[i].session_id = session_id;
	mcp_ctx.nq.tx->notification[i].payload = 0;
	/*
	 * Ensure notification[] is written before we update the counter
	 * We want a ARM dmb() / ARM64 dmb(sy) here
	 */
	smp_mb();

	hdr->write_cnt++;
	/*
	 * Ensure write_cnt is written before new notification
	 * We want a ARM dsb() / ARM64 dsb(sy) here
	 */
	rmb();
}

static inline bool mcp_notifications_flush_nolock(void)
{
	bool flushed = false;

	while (!list_empty(&mcp_ctx.notifications) && !notif_queue_full()) {
		struct mcp_session *session;

		session = list_first_entry(&mcp_ctx.notifications,
					   struct mcp_session,
					   notifications_list);
		mc_dev_devel("pop %x\n", session->id);
		notif_queue_push(session->id);
		session->notif_state = MCP_NOTIF_SENT;
		session->notif_cpu_clk = local_clock();
		list_del_init(&session->notifications_list);
		flushed = true;
	}

	return flushed;
}

bool mcp_notifications_flush(void)
{
	bool flushed = false;

	mutex_lock(&mcp_ctx.notifications_mutex);
	flushed = mcp_notifications_flush_nolock();
	mutex_unlock(&mcp_ctx.notifications_mutex);
	return flushed;
}

int mcp_notify(struct mcp_session *session)
{
	int ret = 0;

	if (!mcp_ctx.scheduler_cb)
		return -EAGAIN;

	mutex_lock(&mcp_ctx.notifications_mutex);
	if (session->id == SID_MCP)
		mc_dev_devel("notify MCP\n");
	else
		mc_dev_devel("notify %x\n", session->id);

	/* Notify TEE */
	if (!list_empty(&mcp_ctx.notifications) || notif_queue_full()) {
		if (!list_empty(&session->notifications_list)) {
			ret = -EAGAIN;
			mc_dev_devel("skip %x\n", session->id);
		} else {
			list_add_tail(&session->notifications_list,
				      &mcp_ctx.notifications);
			session->notif_state = MCP_NOTIF_QUEUED;
			session->notif_cpu_clk = local_clock();
			mc_dev_devel("push %x\n", session->id);
		}

		mcp_notifications_flush_nolock();

		if (mcp_ctx.scheduler_cb(MCP_YIELD)) {
			mc_dev_notice("MC_SMC_N_YIELD failed\n");
			ret = -EPROTO;
		}
	} else {
		notif_queue_push(session->id);
		session->notif_state = MCP_NOTIF_SENT;
		session->notif_cpu_clk = local_clock();
		if (mcp_ctx.scheduler_cb(MCP_NSIQ)) {
			mc_dev_notice("MC_SMC_N_SIQ failed\n");
			ret = -EPROTO;
		}
	}

	mutex_unlock(&mcp_ctx.notifications_mutex);
	return ret;
}

void mcp_update_time(void)
{
	struct timespec tm;

	getnstimeofday(&tm);
	mcp_ctx.time->seconds = tm.tv_sec;
	mcp_ctx.time->nsec = tm.tv_nsec;
}

static inline void handle_mcp_notif(u32 exit_code)
{
	mc_dev_devel("notification from MCP ec %d\n", exit_code);
	complete(&mcp_ctx.complete);
}

static inline void handle_session_notif(u32 session_id, u32 exit_code)
{
	struct mcp_session *session = NULL, *s;

	mutex_lock(&mcp_ctx.sessions_lock);
	list_for_each_entry(s, &mcp_ctx.sessions, list) {
		if (s->id == session_id) {
			session = s;
			break;
		}
	}

	mc_dev_devel("notification from session %x exit code %d state %d\n",
		     session_id, exit_code, session ? session->state : -1);
	if (session) {
		/* TA has terminated */
		if (exit_code) {
			/* Update exit code, or not */
			mutex_lock(&session->exit_code_lock);
			/*
			 * In GP, the only way to recover the sessions exit code
			 * is to call TEEC_InvokeCommand which will notify. But
			 * notifying a dead session would change the exit code
			 * to ERR_SID_NOT_ACTIVE, hence the check below.
			 */
			if (!session->is_gp || !session->exit_code ||
			    (exit_code != ERR_SID_NOT_ACTIVE))
				session->exit_code = exit_code;

			mutex_unlock(&session->exit_code_lock);

			/* Update state or schedule close worker */
			if (session->state == MCP_SESSION_CLOSE_REQUESTED)
				session->state = MCP_SESSION_CLOSE_NOTIFIED;
			else if (session->state == MCP_SESSION_CLOSING_GP)
				schedule_work(&session->close_work);
		}

		/* Unblock waiter */
		mutex_lock(&mcp_ctx.notifications_mutex);
		session->notif_state = MCP_NOTIF_RECEIVED;
		session->notif_cpu_clk = local_clock();
		mutex_unlock(&mcp_ctx.notifications_mutex);
		complete(&session->completion);
	}
	mutex_unlock(&mcp_ctx.sessions_lock);

	/* Unknown session, probably being started */
	if (!session) {
		mutex_lock(&mcp_ctx.unexp_notif_mutex);
		mcp_ctx.unexp_notif.session_id = session_id;
		mcp_ctx.unexp_notif.payload = exit_code;
		mutex_unlock(&mcp_ctx.unexp_notif_mutex);
	}
}

static int irq_bh_worker(void *arg)
{
	struct notification_queue *rx = mcp_ctx.nq.rx;

	while (mcp_ctx.irq_bh_active) {
		wait_for_completion_killable(&mcp_ctx.irq_bh_complete);

		/* Deal with all pending notifications in one go */
		while ((rx->hdr.write_cnt - rx->hdr.read_cnt) > 0) {
			struct notification nf;

			nf = rx->notification[
				rx->hdr.read_cnt % rx->hdr.queue_size];

			/*
			 * Ensure read_cnt writing happens after buffer read
			 * We want a ARM dmb() / ARM64 dmb(sy) here
			 */
			smp_mb();
			rx->hdr.read_cnt++;
			/*
			 * Ensure read_cnt writing finishes before reader
			 * We want a ARM dsb() / ARM64 dsb(sy) here
			 */
			rmb();

			if (nf.session_id == SID_MCP)
				handle_mcp_notif(nf.payload);
			else
				handle_session_notif(nf.session_id, nf.payload);
		}

		/*
		 * Finished processing notifications. It does not matter whether
		 * there actually were any notification or not.  S-SIQs can also
		 * be triggered by an SWd driver which was waiting for a FIQ.
		 * In this case the S-SIQ tells NWd that SWd is no longer idle
		 * an will need scheduling again.
		 */
		if (mcp_ctx.scheduler_cb)
			mcp_ctx.scheduler_cb(MCP_NSIQ);
	}
	return 0;
}

/*
 * This function represents the interrupt function of the mcDrvModule.
 * It signals by incrementing of an event counter and the start of the read
 * waiting queue, the read function a interrupt has occurred.
 */
static irqreturn_t irq_handler(int intr, void *arg)
{
	/* wake up thread to continue handling this interrupt */
	complete(&mcp_ctx.irq_bh_complete);
	return IRQ_HANDLED;
}

void mcp_register_scheduler(int (*scheduler_cb)(enum mcp_scheduler_commands))
{
	mcp_ctx.scheduler_cb = scheduler_cb;
}

void mcp_register_crashhandler(void (*crashhandler_cb)(void))
{
	mcp_ctx.crashhandler_cb = crashhandler_cb;
}

int mcp_start(void)
{
	size_t q_len = ALIGN(2 * (sizeof(struct notification_queue_header) +
		NQ_NUM_ELEMS * sizeof(struct notification)), 4);
	int ret;
	struct irq_data *irq_d;

	/* Make sure we have an interrupt number before going on */
#if defined(CONFIG_OF)
	mcp_ctx.irq = irq_of_parse_and_map(g_ctx.mcd->of_node, 0);
	mc_dev_info("SSIQ from dts is 0x%08x\n", mcp_ctx.irq);
#endif
#if defined(MC_INTR_SSIQ)
	mc_dev_info("MC_INTR_SSIQ is 0x%08x\n", MC_INTR_SSIQ);
	if (mcp_ctx.irq <= 0)
		mcp_ctx.irq = MC_INTR_SSIQ;
#endif

	if (mcp_ctx.irq <= 0) {
		mc_dev_notice("No IRQ number, aborting\n");
		return -EINVAL;
	}

	mc_dev_info("FINAL SSIQ is 0x%08x\n", mcp_ctx.irq);

	/*
	 * Initialize the time structure for SWd
	 * At this stage, we don't know if the SWd needs to get the REE time and
	 * we set it anyway.
	 */
	mcp_update_time();

	/* Call the INIT fastcall to setup shared buffers */
	ret = mc_fc_init(virt_to_phys(mcp_ctx.base),
			 (uintptr_t)mcp_ctx.mcp_buffer -
				(uintptr_t)mcp_ctx.base,
			 q_len, sizeof(*mcp_ctx.mcp_buffer));
	if (ret)
		return ret;

	/* Set initialization values */
#if defined(MC_INTR_SSIQ_SWD)
	mcp_ctx.mcp_buffer->message.init_values.flags |= MC_IV_FLAG_IRQ;
	mcp_ctx.mcp_buffer->message.init_values.irq = MC_INTR_SSIQ_SWD;
#endif
	mcp_ctx.mcp_buffer->message.init_values.flags |= MC_IV_FLAG_TIME;

	irq_d = irq_get_irq_data(mcp_ctx.irq);
	if (irq_d) {
#ifdef CONFIG_MTK_SYSIRQ
		if (irq_d->parent_data) {
			mcp_ctx.mcp_buffer->message.init_values.flags |=
				MC_IV_FLAG_IRQ;
			mcp_ctx.mcp_buffer->message.init_values.irq =
				irq_d->parent_data->hwirq;
			mc_dev_info("irq_d->parent_data->hwirq is 0x%lx\n",
				irq_d->parent_data->hwirq);
		}
#else
		mcp_ctx.mcp_buffer->message.init_values.flags |= MC_IV_FLAG_IRQ;
		mcp_ctx.mcp_buffer->message.init_values.irq = irq_d->hwirq;
		mc_dev_info("irq_d->hwirq is 0x%lx\n", irq_d->hwirq);
#endif
	}
	mcp_ctx.mcp_buffer->message.init_values.time_ofs =
		(u32)((uintptr_t)mcp_ctx.time - (uintptr_t)mcp_ctx.base);
	mcp_ctx.mcp_buffer->message.init_values.time_len =
			sizeof(*mcp_ctx.time);
	/* First empty N-SIQ to setup of the MCI structure */
	ret = mc_fc_nsiq();
	if (ret)
		return ret;

	/*
	 * Wait until the TEE state switches to MC_STATUS_INITIALIZED
	 * It is assumed that it always switches state at some point
	 */
	do {
		u32 status = 0;
		u32 timeslot;

		ret = mc_fc_info(MC_EXT_INFO_ID_MCI_VERSION, &status, NULL);
		if (ret)
			return ret;

		switch (status) {
		case MC_STATUS_NOT_INITIALIZED:
			/* Switch to the TEE to give it more CPU time. */
			ret = EAGAIN;
			for (timeslot = 0; timeslot < 10; timeslot++) {
				int tmp_ret = mc_fc_yield();

				if (tmp_ret)
					return tmp_ret;
			}

			/* No need to loop like mad */
			if (ret == EAGAIN)
				usleep_range(100, 500);

			break;
		case MC_STATUS_HALT:
			mcp_dump_mobicore_status();
			mc_dev_notice("halt during init, state 0x%x\n", status);
			return -ENODEV;
		case MC_STATUS_INITIALIZED:
			mc_dev_devel("ready\n");
			break;
		default:
			/* MC_STATUS_BAD_INIT or anything else */
			mc_dev_notice("MCI init failed, state 0x%x\n", status);
			return -EIO;
		}
	} while (ret == EAGAIN);

	/* Set up S-SIQ interrupt handler and its bottom-half */
	mcp_ctx.irq_bh_active = true;
	mcp_ctx.irq_bh_thread = kthread_run(irq_bh_worker, NULL, "tee_irq_bh");
	if (IS_ERR(mcp_ctx.irq_bh_thread)) {
		mc_dev_notice("irq_bh_worker thread creation failed\n");
		return PTR_ERR(mcp_ctx.irq_bh_thread);
	}
	set_user_nice(mcp_ctx.irq_bh_thread, -20);
	return request_irq(mcp_ctx.irq, irq_handler, IRQF_TRIGGER_RISING,
			   "trustonic", NULL);
}

void mcp_stop(void)
{
	mcp_close();
	mcp_ctx.scheduler_cb = NULL;
	free_irq(mcp_ctx.irq, NULL);
	mcp_ctx.irq_bh_active = false;
	kthread_stop(mcp_ctx.irq_bh_thread);
}

int mcp_init(void)
{
	size_t q_len;
	unsigned long mci;

	mutex_init(&mcp_ctx.buffer_lock);
	mutex_init(&mcp_ctx.queue_lock);
	init_completion(&mcp_ctx.complete);
	init_completion(&mcp_ctx.irq_bh_complete);
	/* Setup notification queue mutex */
	mutex_init(&mcp_ctx.notifications_mutex);
	INIT_LIST_HEAD(&mcp_ctx.notifications);
	mcp_session_init(&mcp_ctx.mcp_session, false, NULL);
	mcp_ctx.mcp_session.id = SID_MCP;
	mutex_init(&mcp_ctx.unexp_notif_mutex);
	INIT_LIST_HEAD(&mcp_ctx.sessions);
	mutex_init(&mcp_ctx.sessions_lock);
	mutex_init(&mcp_ctx.last_mcp_cmds_mutex);

	/* NQ_NUM_ELEMS must be power of 2 */
	q_len = ALIGN(2 * (sizeof(struct notification_queue_header) +
			   NQ_NUM_ELEMS * sizeof(struct notification)), 4);
	if (q_len + sizeof(*mcp_ctx.time) + sizeof(*mcp_ctx.mcp_buffer) >
	    (u16)-1) {
		mc_dev_notice("queues too large (more than 64k), sorry\n");
		return -EINVAL;
	}

	mcp_ctx.order = get_order(q_len + sizeof(*mcp_ctx.time) +
				  sizeof(*mcp_ctx.mcp_buffer));
	mci = __get_free_pages(GFP_USER | __GFP_ZERO, mcp_ctx.order);
	if (!mci)
		return -ENOMEM;

	mcp_ctx.nq.tx = (struct notification_queue *)mci;
	mcp_ctx.nq.tx->hdr.queue_size = NQ_NUM_ELEMS;
	mci += sizeof(struct notification_queue_header) +
	    mcp_ctx.nq.tx->hdr.queue_size * sizeof(struct notification);

	mcp_ctx.nq.rx = (struct notification_queue *)mci;
	mcp_ctx.nq.rx->hdr.queue_size = NQ_NUM_ELEMS;
	mci += sizeof(struct notification_queue_header) +
	    mcp_ctx.nq.rx->hdr.queue_size * sizeof(struct notification);

	mcp_ctx.mcp_buffer = (void *)ALIGN(mci, 8);
	mci += sizeof(struct mcp_buffer);

	mcp_ctx.time = (void *)ALIGN(mci, 8);

	mcp_ctx.timeout = MCP_TIMEOUT;
	debugfs_create_u32("mcp_timeout", 0600, g_ctx.debug_dir,
			   &mcp_ctx.timeout);

	return 0;
}

void mcp_exit(void)
{
	mark_mcp_dead();
	if (mcp_ctx.dump.off)
		kfree(mcp_ctx.dump.buf);

	free_pages((unsigned long)mcp_ctx.base, mcp_ctx.order);
}

static const char *state_to_string(enum mcp_session_state state)
{
	switch (state) {
	case MCP_SESSION_RUNNING:
		return "running";
	case MCP_SESSION_CLOSE_FAILED:
		return "close failed";
	case MCP_SESSION_CLOSE_REQUESTED:
		return "close requested";
	case MCP_SESSION_CLOSE_NOTIFIED:
		return "close notified";
	case MCP_SESSION_CLOSING_GP:
		return "closing";
	case MCP_SESSION_CLOSED:
		return "closed";
	}
	return "error";
}

static const char *notif_state_to_string(enum mcp_notification_state state)
{
	switch (state) {
	case MCP_NOTIF_IDLE:
		return "idle";
	case MCP_NOTIF_QUEUED:
		return "queued";
	case MCP_NOTIF_SENT:
		return "sent";
	case MCP_NOTIF_RECEIVED:
		return "received";
	case MCP_NOTIF_CONSUMED:
		return "consumed";
	case MCP_NOTIF_DEAD:
		return "dead";
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

	mutex_lock(&mcp_ctx.sessions_lock);
	list_for_each_entry(session, &mcp_ctx.sessions, list) {
		s32 exit_code = mcp_session_exitcode(session);

		ret = kasnprintf(buf, "%20llu %4x %-4s %4d %-15s %-11s\n",
				 session->notif_cpu_clk, session->id,
				 session->is_gp ? "GP" : "MC", exit_code,
				 state_to_string(session->state),
				 notif_state_to_string(session->notif_state));
		if (ret < 0)
			break;
	}
	mutex_unlock(&mcp_ctx.sessions_lock);
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

	return kasnprintf(buf, "%20llu %5d %-13s %4x %-8s %6d %5d%s\n",
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
	mutex_lock(&mcp_ctx.last_mcp_cmds_mutex);
	ret = kasnprintf(buf, "%20s %5s %-13s %4s %-8s %6s %5s %s\n",
			 "CPU clock", "PID", "command", "S-ID",
			 "state", "result", "errno", "UUID");
	if (ret < 0)
		goto out;

	cmd_info = &mcp_ctx.last_mcp_cmds[mcp_ctx.last_mcp_cmds_index];
	if (cmd_info->state != UNUSED)
		/* Buffer has wrapped around, dump end (oldest records) */
		for (i = mcp_ctx.last_mcp_cmds_index; i < MCP_LOG_SIZE; i++) {
			ret = show_mcp_log_entry(buf, cmd_info++);
			if (ret < 0)
				goto out;
		}

	/* Dump first records */
	cmd_info = &mcp_ctx.last_mcp_cmds[0];
	for (i = 0; i < mcp_ctx.last_mcp_cmds_index; i++) {
		ret = show_mcp_log_entry(buf, cmd_info++);
		if (ret < 0)
			goto out;
	}

out:
	mutex_unlock(&mcp_ctx.last_mcp_cmds_mutex);
	return ret;
}
