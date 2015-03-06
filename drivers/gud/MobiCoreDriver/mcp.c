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
#include <linux/wait.h>
#include <linux/circ_buf.h>

#include "public/mc_linux.h"
#include "public/mc_admin.h"

#include "mci/mcimcp.h"
#include "mci/mcifc.h"
#include "mci/mcinq.h"		/* SID_MCP */

#include "main.h"
#include "fastcall.h"
#include "debug.h"
#include "nqueue.h"
#include "mcp.h"
#include "scheduler.h"
#include "api.h"

/* respond timeout for MCP notification, in secs */
#define MCP_TIMEOUT		10
#define MCP_RETRIES		5
#define MCP_NF_QUEUE_SZ		8

struct mcp_context {
	rwlock_t buf_lock;
	struct mutex queue_lock;	/* Lock for MCP messages */
	struct mcp_buffer *mcp_buffer;
	struct tbase_session *session;
	wait_queue_head_t wq;
	int mcp_dead;
	struct {
		int head;
		int tail;
		uint32_t payload[MCP_NF_QUEUE_SZ];
	} nf_queue;
};

static inline void mark_mcp_dead(void)
{
	g_ctx.mcp->mcp_dead = true;
	wake_up_all(&g_ctx.mcp->wq);
}

static inline void dump_sleep_params(struct mcp_flags *flags)
{
	MCDRV_DBG("IDLE=%d!", flags->schedule);
	MCDRV_DBG("Request Sleep=%d!", flags->sleep_mode.sleep_req);
	MCDRV_DBG("Sleep Ready=%d!", flags->sleep_mode.ready_to_sleep);
}

static inline int _mcp_set_sleep_mode_rq(uint16_t rq)
{
	g_ctx.mcp->mcp_buffer->mc_flags.sleep_mode.sleep_req = rq;
	return 0;
}

int mcp_set_sleep_mode_rq(uint16_t rq)
{
	int ret = -ENODEV;

	down_read(&g_ctx.mcp_lock);
	if (g_ctx.mcp)
		ret = _mcp_set_sleep_mode_rq(rq);

	up_read(&g_ctx.mcp_lock);
	return ret;
}

uint16_t mcp_get_sleep_mode_req(void)
{
	uint16_t rq = 0;

	down_read(&g_ctx.mcp_lock);
	if (likely(g_ctx.mcp))
		rq = g_ctx.mcp->mcp_buffer->mc_flags.sleep_mode.sleep_req;

	up_read(&g_ctx.mcp_lock);
	return rq;
}

static inline bool _mc_pm_sleep_ready(void)
{
	struct mcp_flags *f = &g_ctx.mcp->mcp_buffer->mc_flags;

	return f->sleep_mode.ready_to_sleep & MC_STATE_READY_TO_SLEEP;
}

bool mc_pm_sleep_ready(void)
{
	bool ret = false;

	down_read(&g_ctx.mcp_lock);
	if (g_ctx.mcp)
		ret = _mc_pm_sleep_ready();

	up_read(&g_ctx.mcp_lock);
	return ret;
}

bool mcp_get_idle_timeout(int32_t *timeout)
{
	uint32_t schedule;
	bool ret;

	down_read(&g_ctx.mcp_lock);
	schedule = g_ctx.mcp->mcp_buffer->mc_flags.schedule;
	if (schedule == MC_FLAG_SCHEDULE_IDLE) {
		if (g_ctx.f_timeout)
			*timeout = g_ctx.mcp->mcp_buffer->mc_flags.timeout_ms;
		else
			*timeout = -1;

		ret = true;
	} else {
		ret = false;
	}

	up_read(&g_ctx.mcp_lock);
	return ret;
}

void mcp_reset_idle_timeout(void)
{
	down_read(&g_ctx.mcp_lock);
	g_ctx.mcp->mcp_buffer->mc_flags.timeout_ms = -1;
	up_read(&g_ctx.mcp_lock);
}

void mcp_wake_up(uint32_t nf_payload)
{
	down_read(&g_ctx.mcp_lock);
	if (g_ctx.mcp) {
		struct mcp_context *mcp = g_ctx.mcp;

		if ((mcp->nf_queue.head - mcp->nf_queue.tail) <
							MCP_NF_QUEUE_SZ) {
			uint idx = mcp->nf_queue.head & (MCP_NF_QUEUE_SZ - 1);

			mcp->nf_queue.payload[idx] = nf_payload;
			mcp->nf_queue.head++;
		} else {
			mark_mcp_dead();
			mc_dev_dump_mobicore_status();
		}
		wake_up_all(&g_ctx.mcp->wq);
	}
	up_read(&g_ctx.mcp_lock);
}

#ifdef MC_PM_RUNTIME
int mcp_suspend_prepare(void)
{
	int ret = 0;

	/*
	 * We can't go to sleep if MobiCore is not IDLE
	 * or not Ready to sleep
	 */
	down_read(&g_ctx.mcp_lock);
	if (g_ctx.mcp) {
		struct mcp_context *mcp = g_ctx.mcp;

		dump_sleep_params(&mcp->mcp_buffer->mc_flags);
		if (!_mc_pm_sleep_ready()) {
			_mcp_set_sleep_mode_rq(MC_FLAG_REQ_TO_SLEEP);
			schedule_work_on(0, &g_ctx.suspend_work);
			flush_work(&g_ctx.suspend_work);
			if (!_mc_pm_sleep_ready()) {
				dump_sleep_params(&mcp->mcp_buffer->mc_flags);
				_mcp_set_sleep_mode_rq(MC_FLAG_NO_SLEEP_REQ);
				MCDRV_ERROR("MobiCore can't SLEEP!");
				ret = NOTIFY_BAD;
			}
		}
	}

	up_read(&g_ctx.mcp_lock);
	return ret;
}
#endif

int mc_switch_enter(void)
{
	int ret = 0;

	down_read(&g_ctx.mcp_lock);
	if (g_ctx.mcp && !_mc_pm_sleep_ready()) {
		_mcp_set_sleep_mode_rq(MC_FLAG_REQ_TO_SLEEP);
		mc_dev_nsiq();
		/* By this time we should be ready for sleep or we are
		 * in the middle of something important */
		if (!_mc_pm_sleep_ready()) {
			dump_sleep_params(&g_ctx.mcp->mcp_buffer->mc_flags);
			MCDRV_DBG("MobiCore: Don't allow switch!");
			_mcp_set_sleep_mode_rq(MC_FLAG_NO_SLEEP_REQ);
			ret = -EPERM;
		}
	}

	up_read(&g_ctx.mcp_lock);
	return ret;
}

static int wait_mcp_notification(void)
{
	struct mcp_context *mcp = g_ctx.mcp;
	unsigned long tout = msecs_to_jiffies(MCP_TIMEOUT * 1000);
	int try;

	/*
	 * Total timeout is MCP_TIMEOUT * MCP_RETRIES, but we check for a crash
	 * to try and terminate before then if things go wrong.
	 */
	for (try = 1; try <= MCP_RETRIES; try++) {
		uint32_t status;
		/*
		* Wait non-interruptible to keep MCP synchronised even if caller
		* is interrupted by signal.
		*/
		int ret = wait_event_timeout(mcp->wq,
				mcp->nf_queue.head != mcp->nf_queue.tail, tout);

		if (ret > 0) {
			mcp->nf_queue.tail++;
			return 0;
		}

		MCDRV_ERROR("No answer after %ds", MCP_TIMEOUT * try);

		/* If SWd halted, exit now */
		if (!mc_fc_info(MC_EXT_INFO_ID_MCI_VERSION, &status, NULL) &&
		    (status == MC_STATUS_HALT))
					break;
	}

	/* <t-base halted or dead: dump status */
	mark_mcp_dead();
	mc_dev_dump_mobicore_status();

	return -ETIME;
}

static int mcp_cmd(union mcp_message *cmd)
{
	int err = 0;
	struct mcp_context *mcp = g_ctx.mcp;
	union mcp_message *msg = &mcp->mcp_buffer->mcp_message;
	enum cmd_id cmd_id;

	mutex_lock(&mcp->queue_lock);
	if (g_ctx.mcp->mcp_dead)
		goto out;

	/* Copy message to MCP buffer */
	memcpy(msg, cmd, sizeof(*msg));
	cmd_id = msg->cmd_header.cmd_id;

	/* poke tbase */
	err = mc_dev_notify(SID_MCP);
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
	/* TODO CPI: extract error assessment out to caller */
	switch (msg->rsp_header.result) {
	case MC_MCP_RET_OK:
		err = 0;
		break;
	case MC_MCP_RET_ERR_CLOSE_TASK_FAILED:
		err = -EBUSY;
		break;
	case MC_MCP_RET_ERR_OUT_OF_RESOURCES:
		err = -ENOSPC;
		break;
	case MC_MCP_RET_ERR_UNKNOWN_UUID:
		err = -ENOENT;
		break;
	default:
		MCDRV_ERROR("cmd %i returned %d.", cmd_id,
			    msg->rsp_header.result);
		err = -EPERM;
		goto out;
	}

	/* Copy response back to caller struct */
	memcpy(cmd, msg, sizeof(*cmd));

out:
	mutex_unlock(&mcp->queue_lock);
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

int mcp_load_token(uintptr_t data, size_t len, const struct tbase_mmu *mmu)
{
	union mcp_message cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_LOAD_TOKEN;
	cmd.cmd_load_token.wsm_data_type = tbase_mmu_type(mmu);
	cmd.cmd_load_token.adr_load_data = tbase_mmu_phys(mmu);
	cmd.cmd_load_token.ofs_load_data = (u32)(uintptr_t)data & ~PAGE_MASK;
	cmd.cmd_load_token.len_load_data = len;
	return mcp_cmd(&cmd);
}

int mcp_load_check(const struct tbase_object *obj,
		   const struct tbase_mmu *mmu)
{
	const union mclf_header *header;
	union mcp_message cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_CHECK_LOAD_TA;
	/* Data */
	cmd.cmd_check_load.wsm_data_type = tbase_mmu_type(mmu);
	cmd.cmd_check_load.adr_load_data = tbase_mmu_phys(mmu);
	cmd.cmd_check_load.ofs_load_data =
		(u32)(uintptr_t)obj->data & ~PAGE_MASK;
	cmd.cmd_check_load.len_load_data = obj->length;
	/* Header */
	header = (union mclf_header *)(obj->data + obj->header_length);
	memcpy(&cmd.cmd_open.tl_header, header, sizeof(cmd.cmd_open.tl_header));
	cmd.cmd_check_load.uuid = header->mclf_header_v2.uuid;
	return mcp_cmd(&cmd);
}

int mcp_open_session(const struct tbase_object *obj,
		     const struct tbase_mmu *mmu,
		     bool is_gp_uuid,
		     const void *buf, size_t buf_len,
		     const struct tbase_mmu *buf_mmu,
		     uint32_t *session_id)
{
	const union mclf_header *header;
	union mcp_message cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_OPEN_SESSION;
	/* Data */
	cmd.cmd_open.wsm_data_type = tbase_mmu_type(mmu);
	cmd.cmd_open.adr_load_data = tbase_mmu_phys(mmu);
	cmd.cmd_open.ofs_load_data = (u32)(uintptr_t)(obj->data) & ~PAGE_MASK;
	cmd.cmd_open.len_load_data = obj->length;
	/* Buffer */
	if (buf) {
		cmd.cmd_open.wsmtype_tci = tbase_mmu_type(buf_mmu);
		cmd.cmd_open.adr_tci_buffer = tbase_mmu_phys(buf_mmu);
		cmd.cmd_open.ofs_tci_buffer = (u32)(uintptr_t)buf & ~PAGE_MASK;
		cmd.cmd_open.len_tci_buffer = buf_len;
	} else {
		cmd.cmd_open.wsmtype_tci = WSM_INVALID;
	}
	/* Header */
	header = (union mclf_header *)(obj->data + obj->header_length);
	memcpy(&cmd.cmd_open.tl_header, header, sizeof(cmd.cmd_open.tl_header));
	cmd.cmd_check_load.uuid = header->mclf_header_v2.uuid;
	cmd.cmd_open.is_gpta = is_gp_uuid;
	/* Send MCP open command */
	ret = mcp_cmd(&cmd);
	if (!ret)
		*session_id = cmd.rsp_open.session_id;

	return ret;
}

int mcp_close_session(uint32_t session_id)
{
	union mcp_message cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_CLOSE_SESSION;
	cmd.cmd_close.session_id = session_id;
	return mcp_cmd(&cmd);
}

int mcp_map(uint32_t session_id, const void *buf, size_t buf_len,
	    const struct tbase_mmu *mmu, uint32_t *secure_va)
{
	union mcp_message cmd;
	int ret;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_MAP;
	cmd.cmd_map.session_id = session_id;
	cmd.cmd_map.wsm_type = tbase_mmu_type(mmu);
	cmd.cmd_map.adr_buffer = tbase_mmu_phys(mmu);
	cmd.cmd_map.ofs_buffer = (u32)(uintptr_t)buf & (~PAGE_MASK);
	cmd.cmd_map.len_buffer = buf_len;
	ret = mcp_cmd(&cmd);
	if (!ret)
		*secure_va = cmd.rsp_map.secure_va;

	return ret;
}

int mcp_unmap(uint32_t session_id, uint32_t secure_va, size_t buf_len,
	      const struct tbase_mmu *mmu)
{
	union mcp_message cmd;

	memset(&cmd, 0, sizeof(cmd));
	cmd.cmd_header.cmd_id = MC_MCP_CMD_UNMAP;
	cmd.cmd_unmap.session_id = session_id;
	cmd.cmd_unmap.wsm_type = tbase_mmu_type(mmu);
	cmd.cmd_unmap.secure_va = secure_va;
	cmd.cmd_unmap.virtual_buffer_len = buf_len;
	return mcp_cmd(&cmd);
}

int mcp_init(void *mcp_buffer_base)
{
	int ret = 0;
	struct mcp_context *mcp;

	if (g_ctx.mcp) {
		MCDRV_ERROR("double initialisation!");
		return -EINVAL;
	}

	mcp = kzalloc(sizeof(*mcp), GFP_KERNEL);
	if (!mcp) {
		MCDRV_ERROR("could not allocate MCP context");
		return -ENOMEM;
	}

	rwlock_init(&mcp->buf_lock);
	mutex_init(&mcp->queue_lock);
	init_waitqueue_head(&mcp->wq);
	mcp->mcp_dead = false;
	mcp->mcp_buffer = mcp_buffer_base;
	mcp->session = session_create(g_ctx.mcore_client, SID_MCP);

	if (!mcp->session) {
		MCDRV_ERROR("Session creation failed");
		kfree(mcp);
		ret = -ENOMEM;
	} else {
		down_write(&g_ctx.mcp_lock);
		g_ctx.mcp = mcp;
		up_write(&g_ctx.mcp_lock);
	}
	MCDRV_DBG_VERBOSE("done");
	return ret;
}

void mcp_cleanup(void)
{
	MCDRV_DBG_VERBOSE("enter");
	if (g_ctx.mcp) {
		struct mcp_context *mcp = g_ctx.mcp;

		/* Should call MCP CLOSE but scheduler is already stopped */
		mark_mcp_dead();
		down_write(&g_ctx.mcp_lock);
		g_ctx.mcp = NULL;
		up_write(&g_ctx.mcp_lock);
		session_put(mcp->session);
		kfree(mcp);
	}
	MCDRV_DBG_VERBOSE("exit");
}
