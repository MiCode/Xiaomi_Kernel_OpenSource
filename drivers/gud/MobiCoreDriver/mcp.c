/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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
#include <linux/completion.h>
#include <linux/circ_buf.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/of_irq.h>

#include "public/mc_linux.h"
#include "public/mc_admin.h"

#include "mci/mcimcp.h"
#include "mci/mcifc.h"
#include "mci/mcinq.h"		/* SID_MCP */

#include "platform.h"		/* IRQ number */
#include "fastcall.h"
#include "debug.h"
#include "logging.h"
#include "mcp.h"

/* respond timeout for MCP notification, in secs */
#define MCP_TIMEOUT		10
#define MCP_RETRIES		5
#define MCP_NF_QUEUE_SZ		8
#define NQ_NUM_ELEMS		16

static void mc_irq_worker(struct work_struct *data);
DECLARE_WORK(irq_work, mc_irq_worker);

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
	struct tbase_session *session;
	struct completion complete;
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
} mcp_ctx;

static inline void mark_mcp_dead(void)
{
	mcp_ctx.mcp_dead = true;
	complete(&mcp_ctx.complete);
}

static inline int mcp_set_sleep_mode_rq(uint16_t sleep_req)
{
	mutex_lock(&mcp_ctx.buffer_lock);
	mcp_ctx.mcp_buffer->mc_flags.sleep_mode.sleep_req = sleep_req;
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

static void mcp_dump_mobicore_status(void)
{
	char uuid_str[33];
	int ret = 0;
	int i;

	if (mcp_ctx.dump.off)
		ret = -EBUSY;

	/* read additional info about exception-point and print */
	dev_err(g_ctx.mcd, "<t-base halted. Status dump:");

	for (i = 0; i < ARRAY_SIZE(status_map); i++) {
		uint32_t info;

		if (!mc_fc_info(status_map[i].index, NULL, &info)) {
			dev_err(g_ctx.mcd, "  %-20s= 0x%08x\n",
				status_map[i].msg, info);
			if (ret >= 0)
				ret = kasnprintf(&mcp_ctx.dump,
						 "%-20s= 0x%08x\n",
						 status_map[i].msg, info);
		}
	}

	/* construct UUID string */
	for (i = 0; i < 4; i++) {
		uint32_t info;
		int j;

		if (mc_fc_info(MC_EXT_INFO_ID_MC_EXC_UUID + i, NULL, &info))
			return;

		for (j = 0; j < sizeof(info); j++) {
			snprintf(&uuid_str[(i * sizeof(info) + j) * 2], 3,
				 "%02x", (info >> (j * 8)) & 0xff);
		}
	}

	dev_err(g_ctx.mcd, "  %-20s= 0x%s\n", "mcExcep.uuid", uuid_str);
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
int mcp_session_waitnotif(struct mcp_session *session, int32_t timeout)
{
	int ret = 0;

	mutex_lock(&session->notif_wait_lock);
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

	if (mcp_session_exitcode(session)) {
		ret = -ECOMM;
		goto end;
	}

	if (!mcp_session_isrunning(session)) {
		ret = -ENXIO;
		goto end;
	}

end:
	mutex_unlock(&session->notif_wait_lock);
	if (ret)
		dev_info(g_ctx.mcd, "%s session %x ec %d ret %d\n", __func__,
			 session->id, session->exit_code, ret);

	return ret;
}

int32_t mcp_session_exitcode(struct mcp_session *session)
{
	int32_t exit_code;

	mutex_lock(&session->exit_code_lock);
	exit_code = session->exit_code;
	mutex_unlock(&session->exit_code_lock);
	if (exit_code)
		dev_info(g_ctx.mcd, "%s session %x ec %d\n", __func__,
			 session->id, exit_code);

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
	struct mcp_flags *flags = &mcp_ctx.mcp_buffer->mc_flags;
	bool ret;

	mutex_lock(&mcp_ctx.buffer_lock);
	ret = flags->sleep_mode.ready_to_sleep & MC_STATE_READY_TO_SLEEP;
	if (!ret) {
		MCDRV_DBG("IDLE=%d!", flags->schedule);
		MCDRV_DBG("Request Sleep=%d!", flags->sleep_mode.sleep_req);
		MCDRV_DBG("Sleep Ready=%d!", flags->sleep_mode.ready_to_sleep);
	}

	mutex_unlock(&mcp_ctx.buffer_lock);
	return ret;
}

bool mcp_get_idle_timeout(int32_t *timeout)
{
	uint32_t schedule;
	bool ret;

	mutex_lock(&mcp_ctx.buffer_lock);
	schedule = mcp_ctx.mcp_buffer->mc_flags.schedule;
	if (schedule == MC_FLAG_SCHEDULE_IDLE) {
		if (g_ctx.f_timeout)
			*timeout = mcp_ctx.mcp_buffer->mc_flags.timeout_ms;
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
	mcp_ctx.mcp_buffer->mc_flags.timeout_ms = -1;
	mutex_unlock(&mcp_ctx.buffer_lock);
}

static inline int wait_mcp_notification(void)
{
	unsigned long timeout = msecs_to_jiffies(MCP_TIMEOUT * 1000);
	int try;

	/*
	 * Total timeout is MCP_TIMEOUT * MCP_RETRIES, but we check for a crash
	 * to try and terminate before then if things go wrong.
	 */
	for (try = 1; try <= MCP_RETRIES; try++) {
		uint32_t status;
		int ret;

		/*
		* Wait non-interruptible to keep MCP synchronised even if caller
		* is interrupted by signal.
		*/
		ret = wait_for_completion_timeout(&mcp_ctx.complete, timeout);
		if (ret > 0)
			return 0;

		MCDRV_ERROR("No answer after %ds", MCP_TIMEOUT * try);

		/* If SWd halted, exit now */
		if (!mc_fc_info(MC_EXT_INFO_ID_MCI_VERSION, &status, NULL) &&
		    (status == MC_STATUS_HALT))
					break;
	}

	/* <t-base halted or dead: dump status */
	mark_mcp_dead();
	mcp_dump_mobicore_status();

	return -ETIME;
}

static int mcp_cmd(union mcp_message *cmd)
{
	int err = 0;
	union mcp_message *msg = &mcp_ctx.mcp_buffer->mcp_message;
	enum cmd_id cmd_id = cmd->cmd_header.cmd_id;

	mutex_lock(&mcp_ctx.queue_lock);
	if (mcp_ctx.mcp_dead)
		goto out;

	/* Copy message to MCP buffer */
	memcpy(msg, cmd, sizeof(*msg));

	/* Poke tbase */
	err = mcp_notify(&mcp_ctx.mcp_session);
	if (!err)
		err = wait_mcp_notification();

	if (err)
		goto out;

	/* Check response ID */
	if (msg->rsp_header.rsp_id != (cmd_id | FLAG_RESPONSE)) {
		MCDRV_ERROR("MCP command got invalid response (0x%X)",
			    msg->rsp_header.rsp_id);
		err = -EBADE;
		goto out;
	}

	/* Convert result */
	switch (msg->rsp_header.result) {
	case MC_MCP_RET_OK:
		err = 0;
		break;
	case MC_MCP_RET_ERR_CLOSE_TASK_FAILED:
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
		MCDRV_ERROR("cmd %d returned %d.", cmd_id,
			    msg->rsp_header.result);
		err = -EPERM;
		goto out;
	}

	/* Copy response back to caller struct */
	memcpy(cmd, msg, sizeof(*cmd));

out:
	mutex_unlock(&mcp_ctx.queue_lock);
	return err;
}

int mcp_get_version(struct mc_version_info *version_info)
{
	union mcp_message cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_GET_MOBICORE_VERSION;
	ret = mcp_cmd(&cmd);
	if (!ret)
		memcpy(version_info, &cmd.rsp_get_version.version_info,
		       sizeof(*version_info));

	return ret;
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
	return mcp_cmd(&cmd);
}

int mcp_load_check(const struct tbase_object *obj,
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
	return mcp_cmd(&cmd);
}

int mcp_open_session(struct mcp_session *session,
		     const struct tbase_object *obj,
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
	ret = mcp_cmd(&cmd);
	if (!ret) {
		session->id = cmd.rsp_open.session_id;
		/* Add to list of sessions */
		mutex_lock(&mcp_ctx.sessions_lock);
		list_add(&session->list, &mcp_ctx.sessions);
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
 * - GP TAs may take time to close, and we get -EBUSY back from mcp_cmd
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
		session->state = MCP_SESSION_CLOSE_PREPARE;

	mutex_unlock(&mcp_ctx.sessions_lock);
	/* Signal an eventual waiter that SWd session is going away */
	complete(&session->completion);
	/* Send MCP command */
	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_CLOSE_SESSION;
	cmd.cmd_close.session_id = session->id;
	ret = mcp_cmd(&cmd);
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
	} else if (ret == -EBUSY) {
		if (session->state == MCP_SESSION_CLOSE_NOTIFIED)
			/* GP TA already closed */
			schedule_work(&session->close_work);

		session->state = MCP_SESSION_CLOSING_GP;
	} else {
		/* Something is not right, assume session is still running */
		session->state = MCP_SESSION_RUNNING;
	}

	mutex_unlock(&mcp_ctx.sessions_lock);
	return ret;
}

int mcp_map(uint32_t session_id, struct mcp_buffer_map *map)
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
	ret = mcp_cmd(&cmd);
	if (!ret)
		map->secure_va = cmd.rsp_map.secure_va;

	return ret;
}

int mcp_unmap(uint32_t session_id, const struct mcp_buffer_map *map)
{
	union mcp_message cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_UNMAP;
	cmd.cmd_unmap.session_id = session_id;
	cmd.cmd_unmap.wsm_type = map->type;
	cmd.cmd_unmap.virtual_buffer_len = map->length;
	cmd.cmd_unmap.secure_va = map->secure_va;
	return mcp_cmd(&cmd);
}

int mcp_multimap(uint32_t session_id, struct mcp_buffer_map *maps)
{
	struct mcp_buffer_map *map = maps;
	union mcp_message cmd;
	struct buffer_map *buf = cmd.cmd_multimap.bufs;
	int ret = 0;
	uint32_t i;

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

	ret = mcp_cmd(&cmd);
	if (ret)
		return ret;

	/* Return secure virtual addresses */
	map = maps;
	for (i = 0; i < MC_MAP_MAX; i++, map++)
		map->secure_va = cmd.rsp_multimap.secure_va[i];

	return 0;
}

int mcp_multiunmap(uint32_t session_id, const struct mcp_buffer_map *maps)
{
	const struct mcp_buffer_map *map = maps;
	union mcp_message cmd;
	struct buffer_unmap *buf = cmd.cmd_multiunmap.bufs;
	uint32_t i;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_MULTIUNMAP;
	cmd.cmd_multiunmap.session_id = session_id;
	for (i = 0; i < MC_MAP_MAX; i++, map++, buf++) {
		buf->secure_va = map->secure_va;
		buf->len_buffer = map->length;
	}

	return mcp_cmd(&cmd);
}

static int mcp_close(void)
{
	union mcp_message cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_CLOSE_MCP;
	return mcp_cmd(&cmd);
}

static inline bool notif_queue_full(void)
{
	struct notification_queue *tx = mcp_ctx.nq.tx;

	return (tx->hdr.write_cnt - tx->hdr.read_cnt) == tx->hdr.queue_size;
}

static inline void notif_queue_push(uint32_t session_id)
{
	struct notification_queue_header *hdr = &mcp_ctx.nq.tx->hdr;
	uint32_t i = hdr->write_cnt % hdr->queue_size;

	mcp_ctx.nq.tx->notification[i].session_id = session_id;
	mcp_ctx.nq.tx->notification[i].payload = 0;
	hdr->write_cnt++;
}

static inline bool mcp_notifications_flush_nolock(void)
{
	bool flushed = false;

	while (!list_empty(&mcp_ctx.notifications) && !notif_queue_full()) {
		struct mcp_session *session;

		session = list_first_entry(&mcp_ctx.notifications,
					   struct mcp_session,
					   notifications_list);
		dev_dbg(g_ctx.mcd, "pop %x\n", session->id);
		notif_queue_push(session->id);
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
		dev_dbg(g_ctx.mcd, "notify MCP");
	else
		dev_dbg(g_ctx.mcd, "notify %x", session->id);

	/* Notify TEE */
	if (!list_empty(&mcp_ctx.notifications) || notif_queue_full()) {
		if (!list_empty(&session->notifications_list)) {
			ret = -EAGAIN;
			dev_dbg(g_ctx.mcd, "skip %x\n", session->id);
		} else {
			list_add(&session->notifications_list,
				 &mcp_ctx.notifications);
			dev_dbg(g_ctx.mcd, "push %x\n", session->id);
		}

		mcp_notifications_flush_nolock();

		if (mcp_ctx.scheduler_cb(MCP_YIELD)) {
			MCDRV_ERROR("MC_SMC_N_YIELD failed");
			ret = -EPROTO;
		}
	} else {
		notif_queue_push(session->id);
		if (mcp_ctx.scheduler_cb(MCP_NSIQ)) {
			MCDRV_ERROR("MC_SMC_N_SIQ failed");
			ret = -EPROTO;
		}
	}

	mutex_unlock(&mcp_ctx.notifications_mutex);
	return ret;
}

static inline void handle_mcp_notif(uint32_t exit_code)
{
	dev_dbg(g_ctx.mcd, "notification from MCP ec %d\n", exit_code);
	complete(&mcp_ctx.complete);
}

static inline void handle_session_notif(uint32_t session_id, uint32_t exit_code)
{
	struct mcp_session *session = NULL, *s;

	dev_dbg(g_ctx.mcd, "notification from %x ec %d\n", session_id,
		exit_code);
	mutex_lock(&mcp_ctx.sessions_lock);
	list_for_each_entry(s, &mcp_ctx.sessions, list) {
		if (s->id == session_id) {
			session = s;
			break;
		}
	}

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
			if (session->state == MCP_SESSION_CLOSE_PREPARE)
				session->state = MCP_SESSION_CLOSE_NOTIFIED;
			else if (session->state == MCP_SESSION_CLOSING_GP)
				schedule_work(&session->close_work);
		}

		/* Unblock waiter */
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

static void mc_irq_worker(struct work_struct *data)
{
	struct notification_queue *rx = mcp_ctx.nq.rx;

	/* Deal with all pending notifications in one go */
	while ((rx->hdr.write_cnt - rx->hdr.read_cnt) > 0) {
		struct notification nf;

		nf = rx->notification[rx->hdr.read_cnt++ % rx->hdr.queue_size];
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

/*
 * This function represents the interrupt function of the mcDrvModule.
 * It signals by incrementing of an event counter and the start of the read
 * waiting queue, the read function a interrupt has occurred.
 */
static irqreturn_t irq_handler(int intr, void *arg)
{
	/* wake up thread to continue handling this interrupt */
	schedule_work(&irq_work);
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

	/* Make sure we have an interrupt number before going on */
#if defined(CONFIG_OF)
	mcp_ctx.irq = irq_of_parse_and_map(g_ctx.mcd->of_node, 0);
#endif
#if defined(MC_INTR_SSIQ)
	if (mcp_ctx.irq <= 0)
		mcp_ctx.irq = MC_INTR_SSIQ;
#endif

	if (mcp_ctx.irq <= 0) {
		MCDRV_ERROR("No IRQ number, aborting");
		return -EINVAL;
	}

	/* Call the INIT fastcall to setup shared buffers */
	ret = mc_fc_init(virt_to_phys(mcp_ctx.base),
			 (uintptr_t)mcp_ctx.mcp_buffer -
				(uintptr_t)mcp_ctx.base,
			 q_len, sizeof(*mcp_ctx.mcp_buffer));
	if (ret)
		return ret;

	/* First empty N-SIQ to setup of the MCI structure */
	ret = mc_fc_nsiq();
	if (ret)
		return ret;

	/*
	 * Wait until <t-base state switches to MC_STATUS_INITIALIZED
	 * It is assumed that <t-base always switches state at a certain
	 * point in time.
	 */
	do {
		uint32_t status = 0;
		uint32_t timeslot;

		ret = mc_fc_info(MC_EXT_INFO_ID_MCI_VERSION, &status, NULL);
		if (ret)
			return ret;

		switch (status) {
		case MC_STATUS_NOT_INITIALIZED:
			/* Switch to <t-base to give it more CPU time. */
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
			MCDRV_ERROR("halt during init, state 0x%x", status);
			return -ENODEV;
		case MC_STATUS_INITIALIZED:
			MCDRV_DBG("ready");
			break;
		default:
			/* MC_STATUS_BAD_INIT or anything else */
			MCDRV_ERROR("MCI init failed, state 0x%x", status);
			return -EIO;
		}
	} while (ret == EAGAIN);

	/* Set up S-SIQ interrupt handler */
	return request_irq(mcp_ctx.irq, irq_handler, IRQF_TRIGGER_RISING,
			   MC_ADMIN_DEVNODE, NULL);
}

void mcp_stop(void)
{
	mcp_close();
	mcp_ctx.scheduler_cb = NULL;
	free_irq(mcp_ctx.irq, NULL);
	flush_work(&irq_work);
}

int mcp_init(void)
{
	size_t q_len;
	unsigned long mci;

	mutex_init(&mcp_ctx.buffer_lock);
	mutex_init(&mcp_ctx.queue_lock);
	init_completion(&mcp_ctx.complete);
	/* Setup notification queue mutex */
	mutex_init(&mcp_ctx.notifications_mutex);
	INIT_LIST_HEAD(&mcp_ctx.notifications);
	mcp_session_init(&mcp_ctx.mcp_session, false, NULL);
	mcp_ctx.mcp_session.id = SID_MCP;
	mutex_init(&mcp_ctx.unexp_notif_mutex);
	INIT_LIST_HEAD(&mcp_ctx.sessions);
	mutex_init(&mcp_ctx.sessions_lock);

	/* NQ_NUM_ELEMS must be power of 2 */
	q_len = ALIGN(2 * (sizeof(struct notification_queue_header) +
			   NQ_NUM_ELEMS * sizeof(struct notification)), 4);
	if (q_len + sizeof(*mcp_ctx.mcp_buffer) > (uint16_t)-1) {
		MCDRV_DBG_WARN("queues too large (more than 64k), sorry...");
		return -EINVAL;
	}

	mcp_ctx.order = get_order(q_len + sizeof(*mcp_ctx.mcp_buffer));
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

	mcp_ctx.mcp_buffer = (void *)ALIGN(mci, 4);
	return 0;
}

void mcp_exit(void)
{
	mark_mcp_dead();
	if (mcp_ctx.dump.off)
		kfree(mcp_ctx.dump.buf);

	free_pages((unsigned long)mcp_ctx.base, mcp_ctx.order);
}
