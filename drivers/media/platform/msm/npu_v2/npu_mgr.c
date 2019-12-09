/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

/* -------------------------------------------------------------------------
 * Includes
 * -------------------------------------------------------------------------
 */
#include "npu_hw_access.h"
#include "npu_mgr.h"
#include "npu_firmware.h"
#include "npu_hw.h"
#include "npu_host_ipc.h"
#include "npu_common.h"
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>

/* -------------------------------------------------------------------------
 * Defines
 * -------------------------------------------------------------------------
 */
#define LOG_MSG_HEADER_SIZE      20
#define LOG_MSG_START_MSG_INDEX  5
#define LOG_MSG_TOTAL_SIZE_INDEX 0
#define LOG_MSG_MSG_ID_INDEX     1

#define NPU_FW_TIMEOUT_POLL_INTERVAL_MS  10
#define NPU_FW_TIMEOUT_MS                5000

/* -------------------------------------------------------------------------
 * File Scope Function Prototypes
 * -------------------------------------------------------------------------
 */
static void npu_ipc_irq_work(struct work_struct *work);
static void npu_wdg_err_irq_work(struct work_struct *work);
static void npu_bridge_mbox_work(struct work_struct *work);
static void npu_disable_fw_work(struct work_struct *work);
static void npu_update_pwr_work(struct work_struct *work);
static void turn_off_fw_logging(struct npu_device *npu_dev);
static int wait_for_status_ready(struct npu_device *npu_dev,
	uint32_t status_reg, uint32_t status_bits, bool poll);
static int wait_npu_cpc_power_off(struct npu_device *npu_dev);
static struct npu_network *alloc_network(struct npu_host_ctx *ctx,
	struct npu_client *client);
static struct npu_network *get_network_by_hdl(struct npu_host_ctx *ctx,
	struct npu_client *client, uint32_t hdl);
static struct npu_network *get_network_by_id(struct npu_host_ctx *ctx,
	struct npu_client *client, int64_t id);
static void free_network(struct npu_host_ctx *ctx, struct npu_client *client,
	int64_t id);
static int network_get(struct npu_network *network);
static int network_put(struct npu_network *network);
static void app_msg_proc(struct npu_host_ctx *host_ctx, uint32_t *msg);
static void log_msg_proc(struct npu_device *npu_dev, uint32_t *msg);
static void host_session_msg_hdlr(struct npu_device *npu_dev);
static void host_session_log_hdlr(struct npu_device *npu_dev);
static int host_error_hdlr(struct npu_device *npu_dev, bool force);
static int npu_send_network_cmd(struct npu_device *npu_dev,
	struct npu_network *network, void *cmd_ptr,
	struct npu_network_cmd *cmd);
static int npu_send_misc_cmd(struct npu_device *npu_dev, uint32_t q_idx,
	void *cmd_ptr, struct npu_misc_cmd *cmd);
static int npu_queue_event(struct npu_client *client, struct npu_kevent *evt);
static int npu_notify_aop(struct npu_device *npu_dev, bool on);
static int npu_notify_fw_pwr_state(struct npu_device *npu_dev,
	uint32_t pwr_level, bool post);
static int load_fw_nolock(struct npu_device *npu_dev, bool enable);
static void disable_fw_nolock(struct npu_device *npu_dev);
static int update_dcvs_activity(struct npu_device *npu_dev, uint32_t activity);
static void npu_queue_network_cmd(struct npu_network *network,
	struct npu_network_cmd *cmd);
static void npu_dequeue_network_cmd(struct npu_network *network,
	struct npu_network_cmd *cmd);
static struct npu_network_cmd *npu_find_network_cmd(struct npu_network *network,
	uint32_t trans_id);
static struct npu_network_cmd *npu_alloc_network_cmd(struct npu_host_ctx *ctx,
	uint32_t stats_buf_size);
static void npu_free_network_cmd(struct npu_host_ctx *ctx,
	struct npu_network_cmd *cmd);
static struct npu_misc_cmd *npu_alloc_misc_cmd(struct npu_host_ctx *ctx);
static void npu_free_misc_cmd(struct npu_host_ctx *ctx,
	struct npu_misc_cmd *cmd);
static void npu_queue_misc_cmd(struct npu_host_ctx *ctx,
	struct npu_misc_cmd *cmd);
static void npu_dequeue_misc_cmd(struct npu_host_ctx *ctx,
	struct npu_misc_cmd *cmd);
static struct npu_misc_cmd *npu_find_misc_cmd(struct npu_host_ctx *ctx,
	uint32_t trans_id);

/* -------------------------------------------------------------------------
 * Function Definitions - Init / Deinit
 * -------------------------------------------------------------------------
 */

static int wait_npu_cpc_power_off(struct npu_device *npu_dev)
{
	uint32_t reg_val = NPU_CPC_PWR_ON;
	uint32_t wait_cnt = 0, max_wait_ms;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	max_wait_ms = NPU_FW_TIMEOUT_MS;

	do {
		reg_val = npu_tcsr_reg_read(npu_dev, TCSR_NPU_CPC_PWR_ON);
		if (!(reg_val & NPU_CPC_PWR_ON)) {
			NPU_DBG("npu cpc powers off\n");
			break;
		}

		if ((host_ctx->wdg_irq_sts != 0) ||
			(host_ctx->err_irq_sts != 0)) {
			NPU_WARN("fw is in bad state, skip wait\n");
			return -EIO;
		}

		wait_cnt += NPU_FW_TIMEOUT_POLL_INTERVAL_MS;
		if (wait_cnt > max_wait_ms) {
			NPU_ERR("timeout wait for cpc power off\n");
			return -EPERM;
		}
		msleep(NPU_FW_TIMEOUT_POLL_INTERVAL_MS);
	} while (1);

	return 0;
}

static int load_fw_nolock(struct npu_device *npu_dev, bool enable)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	int ret = 0;

	if (host_ctx->fw_state != FW_UNLOADED) {
		NPU_WARN("fw is loaded already\n");
		return 0;
	}

	/* Boot the NPU subsystem */
	reinit_completion(&host_ctx->npu_power_up_done);
	host_ctx->subsystem_handle = subsystem_get_local("npu");
	if (IS_ERR_OR_NULL(host_ctx->subsystem_handle)) {
		NPU_ERR("pil load npu fw failed\n");
		host_ctx->subsystem_handle = NULL;
		ret = -ENODEV;
		goto load_fw_fail;
	}

	ret = wait_for_completion_timeout(
		&host_ctx->npu_power_up_done, NW_PWR_UP_TIMEOUT);
	if (!ret) {
		NPU_ERR("Wait for npu powers up timed out\n");
		ret = -ETIMEDOUT;
		goto load_fw_fail;
	}

	/* Keep reading ctrl status until NPU is ready */
	if (wait_for_status_ready(npu_dev, REG_NPU_FW_CTRL_STATUS,
		FW_CTRL_STATUS_MAIN_THREAD_READY_VAL, false)) {
		ret = -EPERM;
		goto load_fw_fail;
	}

	npu_host_ipc_post_init(npu_dev);
	NPU_DBG("firmware init complete\n");

	host_ctx->fw_state = FW_ENABLED;
	ret = npu_enable_irq(npu_dev);
	if (ret) {
		NPU_ERR("Enable irq failed\n");
		goto load_fw_fail;
	}

	if (enable) {
		ret = npu_notify_fw_pwr_state(npu_dev,
			npu_dev->pwrctrl.active_pwrlevel, true);
		if (ret) {
			NPU_ERR("notify fw pwr on failed\n");
			goto load_fw_fail;
		}
		return ret;
	}

	reinit_completion(&host_ctx->fw_shutdown_done);
	ret = npu_notify_fw_pwr_state(npu_dev, NPU_PWRLEVEL_OFF, false);
	if (ret) {
		NPU_ERR("notify fw pwr off failed\n");
		goto load_fw_fail;
	}

	ret = wait_for_completion_timeout(
		&host_ctx->fw_shutdown_done, NW_RSC_TIMEOUT_MS);
	if (!ret) {
		NPU_ERR("Wait for fw shutdown timedout\n");
		ret = -ETIMEDOUT;
	} else {
		ret = wait_npu_cpc_power_off(npu_dev);
	}

load_fw_fail:
	npu_disable_irq(npu_dev);
	npu_disable_sys_cache(npu_dev);
	npu_disable_core_power(npu_dev);
	npu_notify_aop(npu_dev, false);
	if (!ret) {
		host_ctx->fw_state = FW_LOADED;
	} else {
		if (!IS_ERR_OR_NULL(host_ctx->subsystem_handle))
			subsystem_put_local(host_ctx->subsystem_handle);

		host_ctx->fw_state = FW_UNLOADED;
	}

	return ret;
}

static void npu_load_fw_work(struct work_struct *work)
{
	int ret;
	struct npu_host_ctx *host_ctx;
	struct npu_device *npu_dev;

	host_ctx = container_of(work, struct npu_host_ctx, load_fw_work);
	npu_dev = container_of(host_ctx, struct npu_device, host_ctx);

	mutex_lock(&host_ctx->lock);
	ret = load_fw_nolock(npu_dev, false);
	mutex_unlock(&host_ctx->lock);

	if (ret)
		NPU_ERR("load fw failed %d\n", ret);
}

int load_fw(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	if (host_ctx->auto_pil_disable) {
		NPU_WARN("auto pil is disabled\n");
		return -EINVAL;
	}

	if (host_ctx->wq)
		queue_work(host_ctx->wq, &host_ctx->load_fw_work);

	return 0;
}

int unload_fw(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	if (host_ctx->auto_pil_disable) {
		NPU_WARN("auto pil is disabled\n");
		return 0;
	}

	mutex_lock(&host_ctx->lock);
	if (host_ctx->fw_state == FW_UNLOADED) {
		NPU_INFO("fw is unloaded already\n");
		mutex_unlock(&host_ctx->lock);
		return 0;
	} else if (host_ctx->fw_state == FW_ENABLED) {
		NPU_ERR("fw is enabled now, can't be unloaded\n");
		mutex_unlock(&host_ctx->lock);
		return -EBUSY;
	}

	subsystem_put_local(host_ctx->subsystem_handle);
	host_ctx->fw_state = FW_UNLOADED;
	NPU_DBG("fw is unloaded\n");
	mutex_unlock(&host_ctx->lock);

	return 0;
}

static int enable_fw_nolock(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	int ret = 0;

	if (host_ctx->fw_state == FW_UNLOADED) {
		ret = load_fw_nolock(npu_dev,
			host_ctx->auto_pil_disable ? true : false);
		if (ret) {
			NPU_ERR("load fw failed\n");
			return ret;
		}

		if (host_ctx->auto_pil_disable) {
			host_ctx->fw_error = false;
			host_ctx->fw_ref_cnt++;
			mutex_unlock(&host_ctx->lock);
			goto enable_log;
		}
	}

	if (host_ctx->fw_state == FW_ENABLED) {
		host_ctx->fw_ref_cnt++;
		NPU_DBG("fw_ref_cnt %d\n", host_ctx->fw_ref_cnt);
		return 0;
	}

	npu_notify_aop(npu_dev, true);

	ret = npu_enable_core_power(npu_dev);
	if (ret) {
		NPU_ERR("Enable core power failed\n");
		goto enable_pw_fail;
	}

	ret = npu_enable_sys_cache(npu_dev);
	if (ret) {
		NPU_ERR("Enable sys cache failed\n");
		goto enable_sys_cache_fail;
	}

	/* Initialize the host side IPC before fw boots up */
	npu_host_ipc_pre_init(npu_dev);
	npu_host_ipc_post_init(npu_dev);

	ret = npu_enable_irq(npu_dev);
	if (ret) {
		NPU_ERR("Enable irq failed\n");
		goto enable_irq_fail;
	}

	/* set fw_state to FW_ENABLED before send IPC command */
	host_ctx->fw_state = FW_ENABLED;

	NPU_DBG("NPU powers up\n");

	/* turn on auto ACK for warm boots up */
	npu_cc_reg_write(npu_dev, NPU_CC_NPU_CPC_RSC_CTRL, 3);
	reinit_completion(&host_ctx->fw_bringup_done);
	ret = npu_notify_fw_pwr_state(npu_dev, npu_dev->pwrctrl.active_pwrlevel,
		true);
	if (ret) {
		NPU_ERR("notify fw power state failed\n");
		goto notify_fw_pwr_fail;
	}

	ret = wait_for_completion_timeout(
		&host_ctx->fw_bringup_done, NW_RSC_TIMEOUT_MS);
	if (!ret) {
		NPU_ERR("Wait for fw bringup timedout\n");
		ret = -ETIMEDOUT;
		goto notify_fw_pwr_fail;
	} else {
		ret = 0;
	}

	host_ctx->fw_error = false;
	host_ctx->fw_ref_cnt++;


enable_log:
	/* Set logging state */
	if (!npu_hw_log_enabled()) {
		NPU_DBG("fw logging disabled\n");
		turn_off_fw_logging(npu_dev);
	}

	return ret;

notify_fw_pwr_fail:
	host_ctx->fw_state = FW_LOADED;
	npu_disable_irq(npu_dev);
enable_irq_fail:
	npu_disable_sys_cache(npu_dev);
enable_sys_cache_fail:
	npu_disable_core_power(npu_dev);
enable_pw_fail:
	return ret;
}

int enable_fw(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	int ret;

	mutex_lock(&host_ctx->lock);
	ret = enable_fw_nolock(npu_dev);
	mutex_unlock(&host_ctx->lock);

	return ret;
}

static void disable_fw_nolock(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	int ret = 0;

	if (!host_ctx->fw_ref_cnt) {
		NPU_WARN("fw_ref_cnt is 0\n");
		return;
	}

	host_ctx->fw_ref_cnt--;
	NPU_DBG("fw_ref_cnt %d\n", host_ctx->fw_ref_cnt);

	if (host_ctx->fw_state != FW_ENABLED) {
		NPU_ERR("fw is not enabled\n");
		return;
	}

	if (host_ctx->fw_ref_cnt > 0)
		return;

	/* turn on auto ACK for warm shuts down */
	npu_cc_reg_write(npu_dev, NPU_CC_NPU_CPC_RSC_CTRL, 3);
	reinit_completion(&host_ctx->fw_shutdown_done);
	if (npu_notify_fw_pwr_state(npu_dev, NPU_PWRLEVEL_OFF, false)) {
		NPU_WARN("notify fw pwr off failed\n");
		msleep(500);
	}

	if (!host_ctx->auto_pil_disable) {
		ret = wait_for_completion_timeout(
			&host_ctx->fw_shutdown_done, NW_RSC_TIMEOUT_MS);
		if (!ret)
			NPU_ERR("Wait for fw shutdown timedout\n");
		else
			ret = wait_npu_cpc_power_off(npu_dev);
	}

	npu_disable_irq(npu_dev);
	npu_disable_sys_cache(npu_dev);
	npu_disable_core_power(npu_dev);
	host_ctx->fw_state = FW_LOADED;

	NPU_DBG("firmware is disabled\n");
	npu_notify_aop(npu_dev, false);
	complete(&host_ctx->fw_deinit_done);

	if (host_ctx->auto_pil_disable) {
		subsystem_put_local(host_ctx->subsystem_handle);
		host_ctx->fw_state = FW_UNLOADED;
		NPU_DBG("fw is unloaded\n");
	}
}

void disable_fw(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	mutex_lock(&host_ctx->lock);
	disable_fw_nolock(npu_dev);
	mutex_unlock(&host_ctx->lock);
}

/* notify fw current power level */
static int npu_notify_fw_pwr_state(struct npu_device *npu_dev,
	uint32_t pwr_level, bool post)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct ipc_cmd_notify_pwr_pkt pwr_notify_pkt;
	int ret = 0;
	uint32_t reg_val;
	struct npu_misc_cmd *misc_cmd = NULL;

	/* Clear PWR_NOTIFY bits before sending cmd */
	reg_val = REGR(npu_dev, REG_NPU_FW_CTRL_STATUS);
	reg_val &=  ~(FW_CTRL_STATUS_PWR_NOTIFY_ERR_VAL|
		FW_CTRL_STATUS_PWR_NOTIFY_DONE_VAL);
	REGW(npu_dev, REG_NPU_FW_CTRL_STATUS, reg_val);
	REGR(npu_dev, REG_NPU_FW_CTRL_STATUS);

	if (pwr_level == NPU_PWRLEVEL_OFF)
		NPU_DBG("Notify fw power off\n");
	else
		NPU_DBG("Notify fw power level %d [%s]", pwr_level,
			post ? "post" : "pre");

	/* send IPC command to FW */
	pwr_notify_pkt.header.cmd_type = NPU_IPC_CMD_NOTIFY_PWR;
	pwr_notify_pkt.header.size = sizeof(struct ipc_cmd_notify_pwr_pkt);
	pwr_notify_pkt.header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	pwr_notify_pkt.header.flags = 0;
	pwr_notify_pkt.pwr_level = pwr_level;
	pwr_notify_pkt.notify_type = post ?
		NPU_POWER_POST_NOTIFY : NPU_POWER_PRE_NOTIFY;

	misc_cmd = npu_alloc_misc_cmd(host_ctx);
	if (!misc_cmd) {
		NPU_ERR("Can't allocate misc_cmd\n");
		return -ENOMEM;
	}

	misc_cmd->cmd_type = NPU_IPC_CMD_NOTIFY_PWR;
	misc_cmd->trans_id = pwr_notify_pkt.header.trans_id;

	ret = npu_send_misc_cmd(npu_dev, IPC_QUEUE_CMD_HIGH_PRIORITY,
		&pwr_notify_pkt, misc_cmd);

	if (ret) {
		NPU_ERR("NPU_IPC_CMD_NOTIFY_PWR sent failed: %d\n", ret);
	} else {
		ret = wait_for_status_ready(npu_dev, REG_NPU_FW_CTRL_STATUS,
			FW_CTRL_STATUS_PWR_NOTIFY_DONE_VAL, true);
		if (!ret) {
			reg_val = REGR(npu_dev, REG_NPU_FW_CTRL_STATUS);
			if (reg_val & FW_CTRL_STATUS_PWR_NOTIFY_ERR_VAL) {
				NPU_ERR("NOTIfY_PWR failed\n");
				ret = -EPERM;
			}
		}
	}

	npu_free_misc_cmd(host_ctx, misc_cmd);

	return ret;
}

int npu_host_notify_fw_pwr_state(struct npu_device *npu_dev,
	uint32_t pwr_level, bool post)
{
	return npu_notify_fw_pwr_state(npu_dev, pwr_level, post);
}

static int npu_notifier_cb(struct notifier_block *this, unsigned long code,
	void *data)
{
	int ret = 0;
	struct npu_host_ctx *host_ctx =
		container_of(this, struct npu_host_ctx, nb);
	struct npu_device *npu_dev = host_ctx->npu_dev;
	uint32_t reg_val;

	NPU_DBG("notifier code %d\n", code);
	switch (code) {
	case SUBSYS_BEFORE_POWERUP:
	{
		/*
		 * Prepare for loading fw via pil
		 * It will be called during initial load fw
		 * or subsyste restart
		 */
		npu_notify_aop(npu_dev, true);
		ret = npu_enable_core_power(npu_dev);
		if (ret) {
			NPU_WARN("Enable core power failed\n");
			break;
		}

		ret = npu_enable_sys_cache(npu_dev);
		if (ret) {
			NPU_WARN("Enable sys cache failed\n");
			break;
		}

		npu_cc_reg_write(npu_dev, NPU_CC_NPU_CPC_RSC_CTRL, 3);

		/* Clear control/status registers */
		REGW(npu_dev, REG_NPU_FW_CTRL_STATUS, 0x0);
		REGW(npu_dev, REG_NPU_HOST_CTRL_VALUE, 0x0);
		REGW(npu_dev, REG_FW_TO_HOST_EVENT, 0x0);

		NPU_DBG("fw_dbg_mode %x\n", host_ctx->fw_dbg_mode);
		reg_val = 0;
		if (host_ctx->fw_dbg_mode & FW_DBG_MODE_PAUSE)
			reg_val |= HOST_CTRL_STATUS_FW_PAUSE_VAL;

		if (host_ctx->fw_dbg_mode & FW_DBG_DISABLE_WDOG)
			reg_val |= HOST_CTRL_STATUS_DISABLE_WDOG_VAL;

		if (npu_hw_clk_gating_enabled())
			reg_val |= HOST_CTRL_STATUS_BOOT_ENABLE_CLK_GATE_VAL;

		REGW(npu_dev, REG_NPU_HOST_CTRL_STATUS, reg_val);
		/* Read back to flush all registers for fw to read */
		REGR(npu_dev, REG_NPU_HOST_CTRL_STATUS);

		/* Initialize the host side IPC before fw boots up */
		npu_host_ipc_pre_init(npu_dev);
		complete(&host_ctx->npu_power_up_done);
		break;
	}
	case SUBSYS_AFTER_POWERUP:
		break;
	case SUBSYS_BEFORE_SHUTDOWN:
	{
		/* Prepare for unloading fw via PIL */
		if (host_ctx->fw_state == FW_ENABLED) {
			/* only happens during subsystem_restart */
			host_ctx->fw_state = FW_UNLOADED;
			npu_disable_irq(npu_dev);
			npu_disable_sys_cache(npu_dev);
			npu_disable_core_power(npu_dev);
			npu_notify_aop(npu_dev, false);
		}

		/* vote minimum bandwidth before unload npu fw via PIL */
		ret = npu_set_bw(npu_dev, 100, 100);
		if (ret)
			NPU_WARN("Can't update bandwidth\n");

		break;
	}
	case SUBSYS_AFTER_SHUTDOWN:
		ret = npu_set_bw(npu_dev, 0, 0);
		if (ret)
			NPU_WARN("Can't update bandwidth\n");
		break;
	default:
		NPU_DBG("Ignoring event\n");
		break;
	}

	return ret;
}

static void npu_update_pwr_work(struct work_struct *work)
{
	int ret;
	struct npu_host_ctx *host_ctx;
	struct npu_device *npu_dev;

	host_ctx = container_of(work, struct npu_host_ctx, update_pwr_work);
	npu_dev = container_of(host_ctx, struct npu_device, host_ctx);

	mutex_lock(&host_ctx->lock);
	ret = npu_set_power_level(npu_dev, true);
	mutex_unlock(&host_ctx->lock);

	if (ret)
		NPU_ERR("Update power level failed %d\n", ret);
}

int npu_host_update_power(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	if (host_ctx->wq)
		queue_work(host_ctx->wq, &host_ctx->update_pwr_work);

	return 0;
}

int npu_host_init(struct npu_device *npu_dev)
{
	int ret = 0;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	memset(host_ctx, 0, sizeof(*host_ctx));
	init_completion(&host_ctx->fw_deinit_done);
	init_completion(&host_ctx->fw_bringup_done);
	init_completion(&host_ctx->fw_shutdown_done);
	init_completion(&host_ctx->npu_power_up_done);
	mutex_init(&host_ctx->lock);
	spin_lock_init(&host_ctx->bridge_mbox_lock);
	atomic_set(&host_ctx->ipc_trans_id, 1);

	host_ctx->npu_dev = npu_dev;
	host_ctx->nb.notifier_call = npu_notifier_cb;
	host_ctx->notif_hdle = subsys_notif_register_notifier("npu",
		&host_ctx->nb);
	if (IS_ERR(host_ctx->notif_hdle)) {
		NPU_ERR("register event notification failed\n");
		ret = PTR_ERR(host_ctx->notif_hdle);
		host_ctx->notif_hdle = NULL;
		goto fail;
	}

	host_ctx->wq = create_workqueue("npu_general_wq");
	host_ctx->wq_pri =
		alloc_workqueue("npu_ipc_wq", WQ_HIGHPRI | WQ_UNBOUND, 0);
	if (!host_ctx->wq || !host_ctx->wq_pri) {
		ret = -EPERM;
		goto fail;
	} else {
		INIT_WORK(&host_ctx->ipc_irq_work, npu_ipc_irq_work);
		INIT_WORK(&host_ctx->wdg_err_irq_work, npu_wdg_err_irq_work);
		INIT_WORK(&host_ctx->bridge_mbox_work, npu_bridge_mbox_work);
		INIT_WORK(&host_ctx->load_fw_work, npu_load_fw_work);
		INIT_WORK(&host_ctx->update_pwr_work, npu_update_pwr_work);
		INIT_DELAYED_WORK(&host_ctx->disable_fw_work,
			npu_disable_fw_work);
	}

	host_ctx->network_cmd_cache = kmem_cache_create("network_cmd_cache",
		sizeof(struct npu_network_cmd), 0, 0, NULL);
	if (!host_ctx->network_cmd_cache) {
		NPU_ERR("Failed to create network_cmd_cache\n");
		ret = -ENOMEM;
		goto fail;
	}

	host_ctx->misc_cmd_cache = kmem_cache_create("misc_cmd_cache",
		sizeof(struct npu_misc_cmd), 0, 0, NULL);
	if (!host_ctx->misc_cmd_cache) {
		NPU_ERR("Failed to create misc_cmd_cache\n");
		ret = -ENOMEM;
		goto fail;
	}

	host_ctx->stats_buf_cache = kmem_cache_create(
		"stats_buf_cache", NPU_MAX_STATS_BUF_SIZE, 0, 0, NULL);
	if (!host_ctx->stats_buf_cache) {
		NPU_ERR("Failed to create stats_buf_cache\n");
		ret = -ENOMEM;
		goto fail;
	}

	host_ctx->ipc_msg_buf = kzalloc(NPU_IPC_BUF_LENGTH, GFP_KERNEL);
	if (!host_ctx->ipc_msg_buf) {
		NPU_ERR("Failed to allocate ipc buffer\n");
		ret = -ENOMEM;
		goto fail;
	}

	INIT_LIST_HEAD(&host_ctx->misc_cmd_list);
	host_ctx->auto_pil_disable = false;

	return 0;

fail:
	kfree(host_ctx->ipc_msg_buf);
	kmem_cache_destroy(host_ctx->stats_buf_cache);
	kmem_cache_destroy(host_ctx->network_cmd_cache);
	kmem_cache_destroy(host_ctx->misc_cmd_cache);
	if (host_ctx->wq)
		destroy_workqueue(host_ctx->wq);
	if (host_ctx->wq_pri)
		destroy_workqueue(host_ctx->wq_pri);
	if (host_ctx->notif_hdle)
		subsys_notif_unregister_notifier(host_ctx->notif_hdle,
			&host_ctx->nb);
	mutex_destroy(&host_ctx->lock);
	return ret;
}

void npu_host_deinit(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	kfree(host_ctx->ipc_msg_buf);
	kmem_cache_destroy(host_ctx->stats_buf_cache);
	kmem_cache_destroy(host_ctx->network_cmd_cache);
	kmem_cache_destroy(host_ctx->misc_cmd_cache);
	destroy_workqueue(host_ctx->wq);
	destroy_workqueue(host_ctx->wq_pri);
	subsys_notif_unregister_notifier(host_ctx->notif_hdle, &host_ctx->nb);
	mutex_destroy(&host_ctx->lock);
}

/* -------------------------------------------------------------------------
 * Function Definitions - Interrupt Handler
 * -------------------------------------------------------------------------
 */
irqreturn_t npu_ipc_intr_hdlr(int irq, void *ptr)
{
	struct npu_device *npu_dev = (struct npu_device *)ptr;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	INTERRUPT_ACK(npu_dev, irq);

	/* Check that the event thread currently is running */
	if (host_ctx->wq)
		queue_work(host_ctx->wq_pri, &host_ctx->ipc_irq_work);

	return IRQ_HANDLED;
}

irqreturn_t npu_general_intr_hdlr(int irq, void *ptr)
{
	uint32_t reg_val, ack_val;
	struct npu_device *npu_dev = (struct npu_device *)ptr;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	reg_val = npu_cc_reg_read(npu_dev,
		NPU_CC_NPU_MASTERn_GENERAL_IRQ_STATUS(0));
	NPU_DBG("GENERAL_IRQ_STATUS %x\n", reg_val);
	reg_val &= (RSC_SHUTDOWN_REQ_IRQ_STATUS | RSC_BRINGUP_REQ_IRQ_STATUS);
	ack_val = npu_cc_reg_read(npu_dev, NPU_CC_NPU_CPC_RSC_CTRL);

	if (reg_val & RSC_SHUTDOWN_REQ_IRQ_STATUS)
		ack_val |= Q6SS_RSC_SHUTDOWN_ACK_EN;

	if (reg_val & RSC_BRINGUP_REQ_IRQ_STATUS)
		ack_val |= Q6SS_RSC_BRINGUP_ACK_EN;

	npu_cc_reg_write(npu_dev, NPU_CC_NPU_CPC_RSC_CTRL, ack_val);
	npu_cc_reg_write(npu_dev,
		NPU_CC_NPU_MASTERn_GENERAL_IRQ_CLEAR(0), reg_val);

	if (reg_val & RSC_SHUTDOWN_REQ_IRQ_STATUS)
		complete(&host_ctx->fw_shutdown_done);

	if (reg_val & RSC_BRINGUP_REQ_IRQ_STATUS)
		complete(&host_ctx->fw_bringup_done);

	return IRQ_HANDLED;
}

irqreturn_t npu_err_intr_hdlr(int irq, void *ptr)
{
	struct npu_device *npu_dev = (struct npu_device *)ptr;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	host_ctx->err_irq_sts = npu_cc_reg_read(npu_dev,
		NPU_CC_NPU_MASTERn_ERROR_IRQ_STATUS(0));
	npu_cc_reg_write(npu_dev,
		NPU_CC_NPU_MASTERn_ERROR_IRQ_CLEAR(0),
		host_ctx->err_irq_sts);
	NPU_ERR("err_irq_sts %x\n", host_ctx->err_irq_sts);

	if (host_ctx->wq)
		queue_work(host_ctx->wq_pri, &host_ctx->wdg_err_irq_work);

	return IRQ_HANDLED;
}

irqreturn_t npu_wdg_intr_hdlr(int irq, void *ptr)
{
	struct npu_device *npu_dev = (struct npu_device *)ptr;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	host_ctx->wdg_irq_sts = npu_cc_reg_read(npu_dev,
		NPU_CC_NPU_MASTERn_WDOG_BITE_IRQ_STATUS(0));
	NPU_ERR("wdg_irq_sts %x\n", host_ctx->wdg_irq_sts);

	if (host_ctx->wq)
		queue_work(host_ctx->wq_pri, &host_ctx->wdg_err_irq_work);

	return IRQ_HANDLED;
}

/* -------------------------------------------------------------------------
 * Function Definitions - Control
 * -------------------------------------------------------------------------
 */
static int host_error_hdlr(struct npu_device *npu_dev, bool force)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct npu_network *network = NULL;
	struct npu_kevent kevt;
	struct npu_network_cmd *cmd;
	struct npu_misc_cmd *misc_cmd;
	bool fw_alive = true;
	int i, ret = 0;

	mutex_lock(&host_ctx->lock);

	if ((host_ctx->wdg_irq_sts == 0) && (host_ctx->err_irq_sts == 0)
		&& !force) {
		mutex_unlock(&host_ctx->lock);
		return 0;
	}

	if (host_ctx->wdg_irq_sts) {
		NPU_INFO("watchdog irq triggered\n");
		npu_dump_debug_info(npu_dev);
		fw_alive = false;
	}

	/*
	 * if fw is still alive, notify fw before power off
	 * otherwise if ssr happens or notify fw returns failure
	 * delay 500 ms to make sure dsp has finished
	 * its own ssr handling.
	 */
	if (fw_alive) {
		if (npu_notify_fw_pwr_state(npu_dev, NPU_PWRLEVEL_OFF, false)) {
			NPU_WARN("notify fw pwr off failed\n");
			msleep(500);
		}
	} else {
		msleep(500);
	}

	NPU_INFO("npu subsystem is restarting\n");
	reinit_completion(&host_ctx->npu_power_up_done);
	ret = subsystem_restart_dev(host_ctx->subsystem_handle);
	if (ret) {
		NPU_ERR("npu subsystem restart failed\n");
		host_ctx->fw_state = FW_UNLOADED;
		goto fw_start_done;
	}
	NPU_INFO("npu subsystem is restarted\n");

	ret = wait_for_completion_timeout(
		&host_ctx->npu_power_up_done, NW_PWR_UP_TIMEOUT);
	if (!ret) {
		NPU_ERR("Wait for npu powers up timed out\n");
		ret = -ETIMEDOUT;
		goto fw_start_done;
	}

	host_ctx->wdg_irq_sts = 0;
	host_ctx->err_irq_sts = 0;

	/* Keep reading ctrl status until NPU is ready */
	if (wait_for_status_ready(npu_dev, REG_NPU_FW_CTRL_STATUS,
		FW_CTRL_STATUS_MAIN_THREAD_READY_VAL, false)) {
		NPU_ERR("wait for fw status ready timedout\n");
		ret = -EPERM;
		goto fw_start_done;
	}

	npu_host_ipc_post_init(npu_dev);
	NPU_DBG("firmware init complete\n");

	host_ctx->fw_state = FW_ENABLED;

	ret = npu_enable_irq(npu_dev);
	if (ret)
		NPU_ERR("Enable irq failed\n");

fw_start_done:
	/* mark all existing network to error state */
	for (i = 0; i < MAX_LOADED_NETWORK; i++) {
		network = &host_ctx->networks[i];
		if (network->is_valid)
			network->fw_error = true;
	}

	complete(&host_ctx->fw_deinit_done);

	/* flush all pending npu cmds */
	for (i = 0; i < MAX_LOADED_NETWORK; i++) {
		network = &host_ctx->networks[i];
		if (!network->is_valid || !network->fw_error)
			continue;

		if (network->is_async) {
			NPU_DBG("async cmd, queue ssr event\n");
			kevt.evt.type = MSM_NPU_EVENT_TYPE_SSR;
			kevt.evt.u.ssr.network_hdl =
				network->network_hdl;
			if (npu_queue_event(network->client, &kevt))
				NPU_ERR("queue npu event failed\n");

			while (!list_empty(&network->cmd_list)) {
				cmd = list_first_entry(&network->cmd_list,
					struct npu_network_cmd, list);
				npu_dequeue_network_cmd(network, cmd);
				npu_free_network_cmd(host_ctx, cmd);
			}
		} else {
			list_for_each_entry(cmd, &network->cmd_list, list) {
				NPU_DBG("complete network %llx trans_id %d\n",
					network->id, cmd->trans_id);
				complete(&cmd->cmd_done);
			}
		}
	}

	list_for_each_entry(misc_cmd, &host_ctx->misc_cmd_list, list) {
		NPU_DBG("complete misc cmd trans_id %d\n",
			misc_cmd->trans_id);
		complete(&misc_cmd->cmd_done);
	}
	mutex_unlock(&host_ctx->lock);

	return ret;
}

static void npu_ipc_irq_work(struct work_struct *work)
{
	struct npu_host_ctx *host_ctx;
	struct npu_device *npu_dev;

	host_ctx = container_of(work, struct npu_host_ctx, ipc_irq_work);
	npu_dev = container_of(host_ctx, struct npu_device, host_ctx);

	host_session_log_hdlr(npu_dev);
	host_session_msg_hdlr(npu_dev);
}

static void npu_wdg_err_irq_work(struct work_struct *work)
{
	struct npu_host_ctx *host_ctx;
	struct npu_device *npu_dev;

	host_ctx = container_of(work, struct npu_host_ctx, wdg_err_irq_work);
	npu_dev = container_of(host_ctx, struct npu_device, host_ctx);

	host_error_hdlr(npu_dev, false);
}

static void npu_disable_fw_work(struct work_struct *work)
{
	struct npu_host_ctx *host_ctx;
	struct npu_device *npu_dev;

	NPU_DBG("Enter disable fw work\n");
	host_ctx = container_of(work, struct npu_host_ctx,
		disable_fw_work.work);
	npu_dev = container_of(host_ctx, struct npu_device, host_ctx);

	mutex_lock(&host_ctx->lock);
	if (host_ctx->bridge_mbox_pwr_on) {
		disable_fw_nolock(npu_dev);
		host_ctx->bridge_mbox_pwr_on = false;
	}
	mutex_unlock(&host_ctx->lock);
	NPU_DBG("Exit disable fw work\n");
}

static int npu_bridge_mbox_send_data(struct npu_host_ctx *host_ctx,
	struct npu_mbox *mbox, void *data)
{
	NPU_DBG("Generating IRQ for client_id: %u; signal_id: %u\n",
		mbox->client_id, mbox->signal_id);
	mbox_send_message(mbox->chan, NULL);
	mbox_client_txdone(mbox->chan, 0);
	mbox->send_data_pending = false;

	return 0;
}

static void npu_bridge_mbox_work(struct work_struct *work)
{
	int i, ret;
	struct npu_host_ctx *host_ctx;
	struct npu_device *npu_dev;
	unsigned long flags;

	NPU_DBG("Enter bridge mbox work\n");
	host_ctx = container_of(work, struct npu_host_ctx, bridge_mbox_work);
	npu_dev = container_of(host_ctx, struct npu_device, host_ctx);

	mutex_lock(&host_ctx->lock);
	if (host_ctx->fw_state == FW_UNLOADED) {
		NPU_WARN("NPU fw is not loaded\n");
		mutex_unlock(&host_ctx->lock);
		return;
	}

	if ((host_ctx->wdg_irq_sts != 0) || (host_ctx->err_irq_sts != 0)) {
		NPU_WARN("SSR is triggered, skip this time\n");
		mutex_unlock(&host_ctx->lock);
		return;
	}

	/* queue or modify delayed work to disable fw */
	mod_delayed_work(host_ctx->wq, &host_ctx->disable_fw_work,
		NPU_MBOX_IDLE_TIMEOUT);

	if (!host_ctx->bridge_mbox_pwr_on) {
		ret = enable_fw_nolock(npu_dev);
		if (ret) {
			mutex_unlock(&host_ctx->lock);
			NPU_ERR("Enable fw failed\n");
			return;
		}
		host_ctx->bridge_mbox_pwr_on = true;
		NPU_DBG("Fw is enabled by mbox\n");
	}

	spin_lock_irqsave(&host_ctx->bridge_mbox_lock, flags);
	for (i = 0; i < NPU_MAX_MBOX_NUM; i++)
		if (npu_dev->mbox[i].send_data_pending)
			npu_bridge_mbox_send_data(host_ctx,
				&npu_dev->mbox[i], NULL);

	spin_unlock_irqrestore(&host_ctx->bridge_mbox_lock, flags);
	mutex_unlock(&host_ctx->lock);
	NPU_DBG("Exit bridge mbox work\n");
}

static void turn_off_fw_logging(struct npu_device *npu_dev)
{
	struct ipc_cmd_log_state_pkt log_packet;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct npu_misc_cmd *misc_cmd = NULL;
	int ret = 0;

	mutex_lock(&host_ctx->lock);
	log_packet.header.cmd_type = NPU_IPC_CMD_CONFIG_LOG;
	log_packet.header.size = sizeof(struct ipc_cmd_log_state_pkt);
	log_packet.header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	log_packet.header.flags = 0xF;
	log_packet.log_state.module_msk = 0;
	log_packet.log_state.level_msk = 0;

	misc_cmd = npu_alloc_misc_cmd(host_ctx);
	if (!misc_cmd) {
		NPU_ERR("Can't allocate misc_cmd\n");
		return;
	}

	misc_cmd->cmd_type = NPU_IPC_CMD_CONFIG_LOG;
	misc_cmd->trans_id = log_packet.header.trans_id;

	ret = npu_send_misc_cmd(npu_dev, IPC_QUEUE_CMD_HIGH_PRIORITY,
		&log_packet, misc_cmd);

	NPU_DBG("NPU_IPC_CMD_CONFIG_LOG sent status: %d\n", ret);

	if (ret)
		NPU_ERR("npu_host_ipc_send_cmd failed\n");

	npu_free_misc_cmd(host_ctx, misc_cmd);
	mutex_unlock(&host_ctx->lock);
}

static int wait_for_status_ready(struct npu_device *npu_dev,
	uint32_t status_reg, uint32_t status_bits, bool poll)
{
	uint32_t ctrl_sts = 0;
	uint32_t wait_cnt = 0, max_wait_ms;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	max_wait_ms = (host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT_MS : NPU_FW_TIMEOUT_MS;
	if (poll)
		wait_cnt = max_wait_ms * 10;
	else
		wait_cnt = max_wait_ms / NPU_FW_TIMEOUT_POLL_INTERVAL_MS;

	/* keep reading status register until bits are set */
	do {
		ctrl_sts = REGR(npu_dev, status_reg);
		if ((ctrl_sts & status_bits) == status_bits) {
			NPU_DBG("status %x[reg %x] ready received\n",
				status_bits, status_reg);
			break;
		}

		if (!wait_cnt) {
			NPU_ERR("timeout wait for status %x[%x] in reg %x\n",
				status_bits, ctrl_sts, status_reg);
			return -EPERM;
		}

		if ((host_ctx->wdg_irq_sts != 0) ||
			(host_ctx->err_irq_sts != 0)) {
			NPU_WARN("fw is in bad state, skip wait\n");
			return -EIO;
		}

		if (poll)
			udelay(100);
		else
			msleep(NPU_FW_TIMEOUT_POLL_INTERVAL_MS);

		wait_cnt--;
	} while (1);

	return 0;
}

#define MAX_LEN 128

static int npu_notify_aop(struct npu_device *npu_dev, bool on)
{
	char buf[MAX_LEN];
	struct qmp_pkt pkt;
	int buf_size, rc = 0;

	if (!npu_dev->mbox_aop || !npu_dev->mbox_aop->chan)
		return 0;

	buf_size = scnprintf(buf, MAX_LEN, "{class: bcm, res: npu_on, val: %d}",
		on ? 1 : 0);
	if (buf_size < 0) {
		NPU_ERR("prepare qmp notify buf failed\n");
		return -EINVAL;
	}

	NPU_DBG("send msg %s to aop\n", buf);
	memset(&pkt, 0, sizeof(pkt));
	pkt.size = (buf_size + 3) & ~0x3;
	pkt.data = buf;

	rc = mbox_send_message(npu_dev->mbox_aop->chan, &pkt);
	if (rc < 0)
		NPU_ERR("qmp message send failed, ret=%d\n", rc);

	return rc;
}

/* -------------------------------------------------------------------------
 * Function Definitions - Network Management
 * -------------------------------------------------------------------------
 */
static int network_put(struct npu_network *network)
{
	if (!network)
		return 0;

	return atomic_dec_return(&network->ref_cnt);
}

static int network_get(struct npu_network *network)
{
	if (!network)
		return 0;

	return atomic_inc_return(&network->ref_cnt);
}

static struct npu_network *alloc_network(struct npu_host_ctx *ctx,
	struct npu_client *client)
{
	int32_t i;
	struct npu_network *network = ctx->networks;

	WARN_ON(!mutex_is_locked(&ctx->lock));

	for (i = 0; i < MAX_LOADED_NETWORK; i++) {
		if (network->id == 0)
			break;

		network++;
	}

	if (i == MAX_LOADED_NETWORK) {
		NPU_ERR("No free network\n");
		return NULL;
	}

	memset(network, 0, sizeof(struct npu_network));
	network->id = i + 1;
	network->is_valid = true;
	network->client = client;
	INIT_LIST_HEAD(&network->cmd_list);

	ctx->network_num++;
	return network;
}

static struct npu_network *get_network_by_hdl(struct npu_host_ctx *ctx,
	struct npu_client *client, uint32_t hdl)
{
	int32_t i;
	struct npu_network *network = ctx->networks;

	WARN_ON(!mutex_is_locked(&ctx->lock));

	for (i = 0; i < MAX_LOADED_NETWORK; i++) {
		if (network->network_hdl == hdl)
			break;

		network++;
	}

	if ((i == MAX_LOADED_NETWORK) || !network->is_valid) {
		NPU_ERR("network hdl invalid %d\n", hdl);
		return NULL;
	}

	if (client && (client != network->client)) {
		NPU_ERR("network %lld doesn't belong to this client\n",
			network->id);
		return NULL;
	}

	network_get(network);
	return network;
}

static struct npu_network *get_network_by_id(struct npu_host_ctx *ctx,
	struct npu_client *client, int64_t id)
{
	struct npu_network *network = NULL;

	WARN_ON(!mutex_is_locked(&ctx->lock));

	if (id < 1 || id > MAX_LOADED_NETWORK ||
		!ctx->networks[id - 1].is_valid) {
		NPU_ERR("Invalid network id %d\n", (int32_t)id);
		return NULL;
	}

	network = &ctx->networks[id - 1];
	if (client && (client != network->client)) {
		NPU_ERR("network %lld doesn't belong to this client\n", id);
		return NULL;
	}

	network_get(network);
	return network;
}

static void free_network(struct npu_host_ctx *ctx, struct npu_client *client,
	int64_t id)
{
	struct npu_network *network = NULL;
	struct npu_network_cmd *cmd;

	WARN_ON(!mutex_is_locked(&ctx->lock));

	network = get_network_by_id(ctx, client, id);
	if (network) {
		network_put(network);
		while (!list_empty(&network->cmd_list)) {
			cmd = list_first_entry(&network->cmd_list,
				struct npu_network_cmd, list);
			NPU_WARN("Free cmd %x type %x\n", cmd->cmd_id,
				cmd->cmd_type);
			npu_dequeue_network_cmd(network, cmd);
			npu_free_network_cmd(ctx, cmd);
		}

		if (atomic_read(&network->ref_cnt) == 0) {
			memset(network, 0, sizeof(struct npu_network));
			ctx->network_num--;
		} else {
			NPU_WARN("network %lld:%d is in use\n", network->id,
				atomic_read(&network->ref_cnt));
		}
	}
}

/* -------------------------------------------------------------------------
 * Function Definitions - IPC
 * -------------------------------------------------------------------------
 */
static struct npu_network_cmd *npu_alloc_network_cmd(struct npu_host_ctx *ctx,
	uint32_t stats_buf_size)
{
	struct npu_network_cmd *cmd = NULL;

	cmd = kmem_cache_zalloc(ctx->network_cmd_cache, GFP_KERNEL);
	if (!cmd) {
		NPU_ERR("Can't allocate network cmd\n");
		return NULL;
	}

	init_completion(&cmd->cmd_done);

	if (stats_buf_size == 0)
		return cmd;

	cmd->stats_buf = kmem_cache_zalloc(ctx->stats_buf_cache,
		GFP_KERNEL);
	if (!cmd->stats_buf) {
		kmem_cache_free(ctx->network_cmd_cache, cmd);
		return NULL;
	}
	cmd->stats_buf_size = stats_buf_size;

	return cmd;
}

static void npu_free_network_cmd(struct npu_host_ctx *ctx,
	struct npu_network_cmd *cmd)
{
	if (cmd->stats_buf)
		kmem_cache_free(ctx->stats_buf_cache, cmd->stats_buf);
	kmem_cache_free(ctx->network_cmd_cache, cmd);
}

static int npu_queue_event(struct npu_client *client, struct npu_kevent *evt)
{
	struct npu_kevent *kevt = kmalloc(sizeof(*kevt), GFP_KERNEL);

	if (!kevt)
		return -ENOMEM;

	*kevt = *evt;
	INIT_LIST_HEAD(&kevt->list);
	mutex_lock(&client->list_lock);
	list_add_tail(&kevt->list, &client->evt_list);
	mutex_unlock(&client->list_lock);
	wake_up_interruptible(&client->wait);

	return 0;
}

static void npu_queue_network_cmd(struct npu_network *network,
	struct npu_network_cmd *cmd)
{
	INIT_LIST_HEAD(&cmd->list);
	list_add_tail(&cmd->list, &network->cmd_list);
}

static void npu_dequeue_network_cmd(struct npu_network *network,
	struct npu_network_cmd *cmd)
{
	list_del(&cmd->list);
}

static struct npu_network_cmd *npu_find_network_cmd(struct npu_network *network,
	uint32_t trans_id)
{
	struct npu_network_cmd *cmd;

	list_for_each_entry(cmd, &network->cmd_list, list) {
		if (cmd->trans_id == trans_id) {
			NPU_DBG("find cmd for trans_id %d\n", trans_id);
			return cmd;
		}
	}

	NPU_ERR("can't find cmd for trans_id %d\n", trans_id);
	return NULL;
}

static struct npu_misc_cmd *npu_alloc_misc_cmd(struct npu_host_ctx *ctx)
{
	struct npu_misc_cmd *cmd = NULL;

	cmd = kmem_cache_zalloc(ctx->misc_cmd_cache, GFP_KERNEL);
	if (!cmd)
		return NULL;

	init_completion(&cmd->cmd_done);

	return cmd;
}

static void npu_free_misc_cmd(struct npu_host_ctx *ctx,
	struct npu_misc_cmd *cmd)
{
	kmem_cache_free(ctx->misc_cmd_cache, cmd);
}

static void npu_queue_misc_cmd(struct npu_host_ctx *ctx,
	struct npu_misc_cmd *cmd)
{
	INIT_LIST_HEAD(&cmd->list);
	list_add_tail(&cmd->list, &ctx->misc_cmd_list);
}

static void npu_dequeue_misc_cmd(struct npu_host_ctx *ctx,
	struct npu_misc_cmd *cmd)
{
	list_del(&cmd->list);
}

static struct npu_misc_cmd *npu_find_misc_cmd(struct npu_host_ctx *ctx,
	uint32_t trans_id)
{
	struct npu_misc_cmd *cmd;

	list_for_each_entry(cmd, &ctx->misc_cmd_list, list) {
		if (cmd->trans_id == trans_id) {
			NPU_DBG("find misc cmd for trans_id %d\n", trans_id);
			return cmd;
		}
	}

	NPU_ERR("can't find misc cmd for trans_id %d\n", trans_id);
	return NULL;
}

int npu_process_kevent(struct npu_client *client, struct npu_kevent *kevt)
{
	struct npu_device *npu_dev = client->npu_dev;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	int ret = 0;

	mutex_lock(&host_ctx->lock);

	switch (kevt->evt.type) {
	case MSM_NPU_EVENT_TYPE_EXEC_V2_DONE:
	{
		struct npu_network_cmd *cmd = NULL;
		struct npu_network *network;

		network = get_network_by_hdl(host_ctx,
			client, kevt->reserved[0]);
		if (!network) {
			NPU_ERR("Can't find network %x\n", kevt->reserved[0]);
			ret = -EINVAL;
			break;
		}

		cmd = npu_find_network_cmd(network, kevt->reserved[1]);
		if (!cmd) {
			NPU_ERR("can't find exec cmd with trans_id:%d\n",
				kevt->reserved[1]);
			network_put(network);
			ret = -EINVAL;
			break;
		}

		kevt->evt.reserved[0] = cmd->cmd_id;
		ret = copy_to_user((void __user *)cmd->stats_buf_u,
			(void *)cmd->stats_buf,
			kevt->evt.u.exec_v2_done.stats_buf_size);
		if (ret) {
			NPU_ERR("fail to copy to user\n");
			kevt->evt.u.exec_v2_done.stats_buf_size = 0;
			ret = -EFAULT;
		}

		npu_dequeue_network_cmd(network, cmd);
		npu_free_network_cmd(host_ctx, cmd);
		network_put(network);
		break;
	}
	default:
		break;
	}
	mutex_unlock(&host_ctx->lock);

	return ret;
}

static void app_msg_proc(struct npu_host_ctx *host_ctx, uint32_t *msg)
{
	uint32_t msg_id;
	struct npu_network *network = NULL;
	struct npu_kevent kevt;
	struct npu_device *npu_dev = host_ctx->npu_dev;
	struct npu_network_cmd *network_cmd = NULL;
	struct npu_misc_cmd *misc_cmd = NULL;

	msg_id = msg[1];
	switch (msg_id) {
	case NPU_IPC_MSG_EXECUTE_DONE:
	{
		struct ipc_msg_execute_pkt *exe_rsp_pkt =
			(struct ipc_msg_execute_pkt *)msg;

		NPU_DBG("NPU_IPC_MSG_EXECUTE_DONE status: %d\n",
			exe_rsp_pkt->header.status);
		NPU_DBG("trans_id : %d\n", exe_rsp_pkt->header.trans_id);

		network = get_network_by_hdl(host_ctx, NULL,
			exe_rsp_pkt->network_hdl);
		if (!network) {
			NPU_ERR("can't find network %x\n",
				exe_rsp_pkt->network_hdl);
			break;
		}

		network_cmd = npu_find_network_cmd(network,
			exe_rsp_pkt->header.trans_id);
		if (!network_cmd) {
			NPU_ERR("can't find exec cmd with trans_id:%d\n",
				exe_rsp_pkt->header.trans_id);
			network_put(network);
			break;
		}

		network_cmd->ret_status = exe_rsp_pkt->header.status;

		if (!network_cmd->async) {
			complete(&network_cmd->cmd_done);
		} else {
			NPU_DBG("async cmd, queue event\n");
			kevt.evt.type = MSM_NPU_EVENT_TYPE_EXEC_DONE;
			kevt.evt.u.exec_done.network_hdl =
				exe_rsp_pkt->network_hdl;
			kevt.evt.u.exec_done.exec_result =
				exe_rsp_pkt->header.status;
			if (npu_queue_event(network->client, &kevt))
				NPU_ERR("queue npu event failed\n");
		}
		network_put(network);

		break;
	}
	case NPU_IPC_MSG_EXECUTE_V2_DONE:
	{
		struct ipc_msg_execute_pkt_v2 *exe_rsp_pkt =
			(struct ipc_msg_execute_pkt_v2 *)msg;
		uint32_t stats_size = 0;

		NPU_DBG("NPU_IPC_MSG_EXECUTE_V2_DONE status: %d\n",
			exe_rsp_pkt->header.status);
		NPU_DBG("trans_id : %d\n", exe_rsp_pkt->header.trans_id);

		network = get_network_by_hdl(host_ctx, NULL,
			exe_rsp_pkt->network_hdl);
		if (!network) {
			NPU_ERR("can't find network %x\n",
				exe_rsp_pkt->network_hdl);
			break;
		}

		network_cmd = npu_find_network_cmd(network,
			exe_rsp_pkt->header.trans_id);
		if (!network_cmd) {
			NPU_ERR("can't find exec cmd with trans_id:%d:%d\n",
				exe_rsp_pkt->header.trans_id,
				exe_rsp_pkt->network_hdl);
			network_put(network);
			break;
		}

		NPU_DBG("network id : %lld\n", network->id);
		stats_size = exe_rsp_pkt->header.size - sizeof(*exe_rsp_pkt);
		NPU_DBG("stats_size %d:%d\n", exe_rsp_pkt->header.size,
			stats_size);
		stats_size = stats_size < network_cmd->stats_buf_size ?
			stats_size : network_cmd->stats_buf_size;
		if (stats_size)
			memcpy(network_cmd->stats_buf, exe_rsp_pkt->stats_data,
				stats_size);

		network_cmd->stats_buf_size = stats_size;
		network_cmd->ret_status = exe_rsp_pkt->header.status;

		if (network_cmd->async) {
			NPU_DBG("async cmd, queue event\n");
			kevt.evt.type = MSM_NPU_EVENT_TYPE_EXEC_V2_DONE;
			kevt.evt.u.exec_v2_done.network_hdl =
				exe_rsp_pkt->network_hdl;
			kevt.evt.u.exec_v2_done.exec_result =
				exe_rsp_pkt->header.status;
			kevt.evt.u.exec_v2_done.stats_buf_size = stats_size;
			kevt.reserved[0] = (uint64_t)network->network_hdl;
			kevt.reserved[1] = (uint64_t)network_cmd->trans_id;
			if (npu_queue_event(network->client, &kevt))
				NPU_ERR("queue npu event failed\n");
		} else {
			complete(&network_cmd->cmd_done);
		}
		network_put(network);
		break;
	}
	case NPU_IPC_MSG_LOAD_DONE:
	{
		uint32_t network_id = 0;
		struct ipc_msg_load_pkt *load_rsp_pkt =
			(struct ipc_msg_load_pkt *)msg;

		NPU_DBG("NPU_IPC_MSG_LOAD_DONE status: %d, trans_id: %d\n",
			load_rsp_pkt->header.status,
			load_rsp_pkt->header.trans_id);

		/*
		 * the upper 16 bits in returned network_hdl is
		 * the network ID
		 */
		NPU_DBG("network_hdl: %x\n", load_rsp_pkt->network_hdl);
		network_id = load_rsp_pkt->network_hdl >> 16;
		network = get_network_by_id(host_ctx, NULL, network_id);
		if (!network) {
			NPU_ERR("can't find network %d\n", network_id);
			break;
		}

		network_cmd = npu_find_network_cmd(network,
			load_rsp_pkt->header.trans_id);
		if (!network_cmd) {
			NPU_ERR("can't find load cmd with trans_id:%d\n",
				load_rsp_pkt->header.trans_id);
			network_put(network);
			break;
		}

		network->network_hdl = load_rsp_pkt->network_hdl;
		network_cmd->ret_status = load_rsp_pkt->header.status;

		complete(&network_cmd->cmd_done);
		network_put(network);
		break;
	}
	case NPU_IPC_MSG_UNLOAD_DONE:
	{
		struct ipc_msg_unload_pkt *unload_rsp_pkt =
			(struct ipc_msg_unload_pkt *)msg;

		NPU_DBG("NPU_IPC_MSG_UNLOAD_DONE status: %d, trans_id: %d\n",
			unload_rsp_pkt->header.status,
			unload_rsp_pkt->header.trans_id);

		network = get_network_by_hdl(host_ctx, NULL,
			unload_rsp_pkt->network_hdl);
		if (!network) {
			NPU_ERR("can't find network %x\n",
				unload_rsp_pkt->network_hdl);
			break;
		}

		network_cmd = npu_find_network_cmd(network,
			unload_rsp_pkt->header.trans_id);
		if (!network_cmd) {
			NPU_ERR("can't find unload cmd with trans_id:%d\n",
				unload_rsp_pkt->header.trans_id);
			network_put(network);
			break;
		}

		network_cmd->ret_status = unload_rsp_pkt->header.status;

		complete(&network_cmd->cmd_done);
		network_put(network);
		break;
	}
	case NPU_IPC_MSG_LOOPBACK_DONE:
	{
		struct ipc_msg_loopback_pkt *lb_rsp_pkt =
			(struct ipc_msg_loopback_pkt *)msg;

		NPU_DBG("NPU_IPC_MSG_LOOPBACK_DONE loopbackParams: 0x%x\n",
			lb_rsp_pkt->loopbackParams);

		misc_cmd = npu_find_misc_cmd(host_ctx,
			lb_rsp_pkt->header.trans_id);
		if (!misc_cmd) {
			NPU_ERR("can't find loopback cmd with trans_id:%d\n",
				lb_rsp_pkt->header.trans_id);
			break;
		}

		misc_cmd->ret_status = lb_rsp_pkt->header.status;
		complete_all(&misc_cmd->cmd_done);
		break;
	}
	case NPU_IPC_MSG_SET_PROPERTY_DONE:
	{
		struct ipc_msg_prop_pkt *prop_rsp_pkt =
			(struct ipc_msg_prop_pkt *)msg;
		uint32_t *param = (uint32_t *)((uint8_t *)prop_rsp_pkt +
			sizeof(struct ipc_msg_prop_pkt));
		NPU_DBG("NPU_IPC_MSG_SET_PROPERTY_DONE %d:0x%x:%d\n",
			prop_rsp_pkt->network_hdl,
			prop_rsp_pkt->prop_id,
			param[0]);

		misc_cmd = npu_find_misc_cmd(host_ctx,
			prop_rsp_pkt->header.trans_id);
		if (!misc_cmd) {
			NPU_ERR("can't find set_prop cmd with trans_id:%d\n",
				prop_rsp_pkt->header.trans_id);
			break;
		}

		misc_cmd->ret_status = prop_rsp_pkt->header.status;
		complete(&misc_cmd->cmd_done);
		break;
	}
	case NPU_IPC_MSG_GET_PROPERTY_DONE:
	{
		struct ipc_msg_prop_pkt *prop_rsp_pkt =
			(struct ipc_msg_prop_pkt *)msg;
		uint32_t prop_size = 0;
		uint32_t *prop_data = (uint32_t *)((uint8_t *)prop_rsp_pkt +
			sizeof(struct ipc_msg_header_pkt));

		NPU_DBG("NPU_IPC_MSG_GET_PROPERTY_DONE %d:0x%x:%d:%d\n",
			prop_rsp_pkt->network_hdl,
			prop_rsp_pkt->prop_id,
			prop_rsp_pkt->num_params,
			prop_rsp_pkt->prop_param[0]);

		misc_cmd = npu_find_misc_cmd(host_ctx,
			prop_rsp_pkt->header.trans_id);
		if (!misc_cmd) {
			NPU_ERR("can't find get_prop cmd with trans_id:%d\n",
				prop_rsp_pkt->header.trans_id);
			break;
		}

		misc_cmd->ret_status = prop_rsp_pkt->header.status;

		if (prop_rsp_pkt->num_params > 0) {
			/* Copy prop data to kernel buffer */
			prop_size = prop_rsp_pkt->header.size -
				sizeof(struct ipc_msg_header_pkt);
			if (prop_size > sizeof(struct msm_npu_property)) {
				NPU_WARN("Invalid prop size %d\n", prop_size);
				prop_size = sizeof(struct msm_npu_property);
			}
			memcpy(&misc_cmd->u.prop, prop_data, prop_size);
		}

		complete_all(&misc_cmd->cmd_done);
		break;
	}
	case NPU_IPC_MSG_GENERAL_NOTIFY:
	{
		struct ipc_msg_general_notify_pkt *notify_msg_pkt =
			(struct ipc_msg_general_notify_pkt *)msg;

		NPU_DBG("NPU_IPC_MSG_GENERAL_NOTIFY %d:0x%x:%d\n",
			notify_msg_pkt->network_hdl,
			notify_msg_pkt->notify_id,
			notify_msg_pkt->notify_param[0]);

		switch (notify_msg_pkt->notify_id) {
		case NPU_NOTIFY_DCVS_MODE:
			NPU_DBG("NPU_IPC_MSG_GENERAL_NOTIFY DCVS_MODE %d\n",
				notify_msg_pkt->notify_param[0]);
			update_dcvs_activity(npu_dev,
				notify_msg_pkt->notify_param[0]);
			break;
		default:
			NPU_ERR("Nothing to do\n");
			break;
		}
		break;
	}
	default:
		NPU_ERR("Not supported apps response received %d\n",
			msg_id);
		break;
	}
}

static void host_session_msg_hdlr(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	mutex_lock(&host_ctx->lock);
	if (host_ctx->fw_state != FW_ENABLED) {
		NPU_WARN("handle npu session msg when FW is disabled\n");
		goto skip_read_msg;
	}

	while (npu_host_ipc_read_msg(npu_dev, IPC_QUEUE_APPS_RSP,
		host_ctx->ipc_msg_buf) == 0) {
		NPU_DBG("received from msg queue\n");
		app_msg_proc(host_ctx, host_ctx->ipc_msg_buf);
	}

skip_read_msg:
	mutex_unlock(&host_ctx->lock);
}

static void log_msg_proc(struct npu_device *npu_dev, uint32_t *msg)
{
	uint32_t msg_id;
	uint32_t *log_msg;
	uint32_t size;

	msg_id = msg[LOG_MSG_MSG_ID_INDEX];
	size = msg[LOG_MSG_TOTAL_SIZE_INDEX] - LOG_MSG_HEADER_SIZE;

	switch (msg_id) {
	case NPU_IPC_MSG_EVENT_NOTIFY:
		/* Process the message */
		log_msg = &(msg[LOG_MSG_START_MSG_INDEX]);
		npu_process_log_message(npu_dev, log_msg, size);
		break;
	default:
		NPU_ERR("unsupported log response received %d\n", msg_id);
		break;
	}
}

static void host_session_log_hdlr(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	mutex_lock(&host_ctx->lock);
	if (host_ctx->fw_state != FW_ENABLED) {
		NPU_WARN("handle npu session msg when FW is disabled\n");
		goto skip_read_msg;
	}

	while (npu_host_ipc_read_msg(npu_dev, IPC_QUEUE_LOG,
		host_ctx->ipc_msg_buf) == 0) {
		NPU_DBG("received from log queue\n");
		log_msg_proc(npu_dev, host_ctx->ipc_msg_buf);
	}

skip_read_msg:
	mutex_unlock(&host_ctx->lock);
}

/* -------------------------------------------------------------------------
 * Function Definitions - Functionality
 * -------------------------------------------------------------------------
 */
int32_t npu_host_get_info(struct npu_device *npu_dev,
			struct msm_npu_get_info_ioctl *get_info_ioctl)
{
	get_info_ioctl->firmware_version = FIRMWARE_VERSION;
	get_info_ioctl->flags = npu_dev->pwrctrl.num_pwrlevels;
	return 0;
}

int32_t npu_host_map_buf(struct npu_client *client,
			struct msm_npu_map_buf_ioctl *map_ioctl)
{
	struct npu_device *npu_dev = client->npu_dev;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	int ret;

	mutex_lock(&host_ctx->lock);
	ret = npu_mem_map(client, map_ioctl->buf_ion_hdl, map_ioctl->size,
		&map_ioctl->npu_phys_addr);
	mutex_unlock(&host_ctx->lock);

	return ret;
}

int32_t npu_host_unmap_buf(struct npu_client *client,
			struct msm_npu_unmap_buf_ioctl *unmap_ioctl)
{
	struct npu_device *npu_dev = client->npu_dev;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	/*
	 * Once SSR occurs, all buffers only can be unmapped until
	 * fw is disabled
	 */
	if (host_ctx->fw_error && (host_ctx->fw_state == FW_ENABLED) &&
		!wait_for_completion_timeout(
		&host_ctx->fw_deinit_done, NW_CMD_TIMEOUT))
		NPU_WARN("npu: wait for fw_deinit_done time out\n");

	mutex_lock(&host_ctx->lock);
	npu_mem_unmap(client, unmap_ioctl->buf_ion_hdl,
		unmap_ioctl->npu_phys_addr);
	mutex_unlock(&host_ctx->lock);

	return 0;
}

static int npu_send_network_cmd(struct npu_device *npu_dev,
	struct npu_network *network, void *cmd_ptr,
	struct npu_network_cmd *cmd)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	int ret = 0;

	WARN_ON(!mutex_is_locked(&host_ctx->lock));

	if (network->fw_error || host_ctx->fw_error ||
		(host_ctx->fw_state != FW_ENABLED)) {
		NPU_ERR("fw is in error state or disabled\n");
		ret = -EIO;
	} else {
		if (cmd)
			reinit_completion(&cmd->cmd_done);
		NPU_DBG("Send cmd %d network id %llx trans id %d\n",
			((struct ipc_cmd_header_pkt *)cmd_ptr)->cmd_type,
			network->id,
			((struct ipc_cmd_header_pkt *)cmd_ptr)->trans_id);
		ret = npu_host_ipc_send_cmd(npu_dev,
			IPC_QUEUE_APPS_EXEC, cmd_ptr);
	}

	return ret;
}

static int npu_send_misc_cmd(struct npu_device *npu_dev, uint32_t q_idx,
	void *cmd_ptr, struct npu_misc_cmd *cmd)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	int ret = 0;

	WARN_ON(!mutex_is_locked(&host_ctx->lock));

	if (host_ctx->fw_error || (host_ctx->fw_state != FW_ENABLED)) {
		NPU_ERR("fw is in error state or disabled\n");
		ret = -EIO;
	} else {
		NPU_DBG("Send cmd %d\n", cmd->cmd_type);
		reinit_completion(&cmd->cmd_done);
		ret = npu_host_ipc_send_cmd(npu_dev, q_idx, cmd_ptr);
	}

	return ret;
}

static void host_copy_patch_data_v2(struct npu_patch_tuple_v2 *param,
	struct msm_npu_patch_info_v2 *patch_info)
{
	param->value = patch_info->value;
	param->chunk_id = patch_info->chunk_id;
	param->loc_offset = patch_info->loc_offset;
	param->instruction_size_in_bytes =
		patch_info->instruction_size_in_bytes;
	param->shift_value_in_bits = patch_info->shift_value_in_bits;
	param->variable_size_in_bits = patch_info->variable_size_in_bits;
	NPU_DBG("copy_patch_data_v2: %x %d %x %x %x %x\n",
		param->value,
		param->chunk_id,
		param->loc_offset,
		param->instruction_size_in_bytes,
		param->shift_value_in_bits,
		param->variable_size_in_bits);
}

static uint32_t find_networks_perf_mode(struct npu_host_ctx *host_ctx)
{
	struct npu_network *network;
	uint32_t max_perf_mode = 0;
	int i = 0;

	network = host_ctx->networks;

	if (!host_ctx->network_num) {
		/* if no network exists, set to the lowest level */
		max_perf_mode = 1;
	} else {
		/* find the max level among all the networks */
		for (i = 0; i < MAX_LOADED_NETWORK; i++) {
			if ((network->id != 0) &&
				(network->cur_perf_mode != 0) &&
				(network->cur_perf_mode > max_perf_mode))
				max_perf_mode = network->cur_perf_mode;
			network++;
		}
	}
	NPU_DBG("max perf mode for networks: %d\n", max_perf_mode);

	return max_perf_mode;
}

static int set_perf_mode(struct npu_device *npu_dev)
{
	int ret = 0;
	uint32_t networks_perf_mode;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	networks_perf_mode = find_networks_perf_mode(host_ctx);

	if (npu_dev->pwrctrl.perf_mode_override)
		networks_perf_mode = npu_dev->pwrctrl.perf_mode_override;

	if (npu_dev->pwrctrl.cur_dcvs_activity != NPU_DCVS_ACTIVITY_MAX_PERF)
		networks_perf_mode = min_t(uint32_t, networks_perf_mode,
			npu_dev->pwrctrl.cur_dcvs_activity);
	ret = npu_set_uc_power_level(npu_dev, networks_perf_mode);
	if (ret)
		NPU_ERR("set uc power level %d failed\n", networks_perf_mode);

	return ret;
}

static int update_dcvs_activity(struct npu_device *npu_dev, uint32_t activity)
{
	npu_dev->pwrctrl.cur_dcvs_activity = activity;
	NPU_DBG("update dcvs activity to %d\n", activity);

	return set_perf_mode(npu_dev);
}

int32_t npu_host_set_fw_property(struct npu_device *npu_dev,
	struct msm_npu_property *property)
{
	int ret = 0, i;
	uint32_t prop_param, prop_id;
	struct ipc_cmd_prop_pkt *prop_packet = NULL;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	uint32_t num_of_params, pkt_size;
	struct npu_misc_cmd *misc_cmd = NULL;

	prop_id = property->prop_id;
	num_of_params = min_t(uint32_t, property->num_of_params,
		(uint32_t)PROP_PARAM_MAX_SIZE);
	pkt_size = sizeof(*prop_packet) + num_of_params * sizeof(uint32_t);
	prop_packet = kzalloc(pkt_size, GFP_KERNEL);

	if (!prop_packet)
		return -ENOMEM;

	switch (prop_id) {
	case MSM_NPU_PROP_ID_DCVS_MODE:
		prop_param = min_t(uint32_t, property->prop_param[0],
			(uint32_t)(npu_dev->pwrctrl.num_pwrlevels - 1));
		property->prop_param[0] = prop_param;
		NPU_DBG("setting dcvs_mode to %d[%d:%d]\n", prop_param,
			property->prop_param[0],
			(uint32_t)(npu_dev->pwrctrl.num_pwrlevels - 1));

		if (property->network_hdl == 0) {
			npu_dev->pwrctrl.dcvs_mode = prop_param;
			NPU_DBG("Set global dcvs mode %d\n", prop_param);
		}
		break;
	default:
		NPU_ERR("unsupported property %d\n", property->prop_id);
		goto set_prop_exit;
	}

	prop_packet->header.cmd_type = NPU_IPC_CMD_SET_PROPERTY;
	prop_packet->header.size = pkt_size;
	prop_packet->header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	prop_packet->header.flags = 0;

	prop_packet->prop_id = prop_id;
	prop_packet->num_params = num_of_params;
	prop_packet->network_hdl = property->network_hdl;
	for (i = 0; i < num_of_params; i++)
		prop_packet->prop_param[i] = property->prop_param[i];

	mutex_lock(&host_ctx->lock);
	misc_cmd = npu_alloc_misc_cmd(host_ctx);
	if (!misc_cmd) {
		NPU_ERR("Can't allocate misc_cmd\n");
		ret = -ENOMEM;
		goto set_prop_exit;
	}

	misc_cmd->cmd_type = NPU_IPC_CMD_SET_PROPERTY;
	misc_cmd->trans_id = prop_packet->header.trans_id;
	npu_queue_misc_cmd(host_ctx, misc_cmd);

	ret = npu_send_misc_cmd(npu_dev, IPC_QUEUE_APPS_EXEC,
		prop_packet, misc_cmd);
	NPU_DBG("NPU_IPC_CMD_SET_PROPERTY sent status: %d\n", ret);

	if (ret) {
		NPU_ERR("NPU_IPC_CMD_SET_PROPERTY failed\n");
		goto free_misc_cmd;
	}
	mutex_unlock(&host_ctx->lock);

	ret = wait_for_completion_interruptible_timeout(
		&misc_cmd->cmd_done,
		(host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT : NW_CMD_TIMEOUT);

	mutex_lock(&host_ctx->lock);
	if (!ret) {
		NPU_ERR("NPU_IPC_CMD_SET_PROPERTY time out\n");
		ret = -ETIMEDOUT;
		goto free_misc_cmd;
	} else if (ret < 0) {
		NPU_ERR("Wait for set_property done interrupted by signal\n");
		goto free_misc_cmd;
	}

	ret = misc_cmd->ret_status;
	if (ret)
		NPU_ERR("set fw property failed %d\n", ret);

free_misc_cmd:
	npu_dequeue_misc_cmd(host_ctx, misc_cmd);
	npu_free_misc_cmd(host_ctx, misc_cmd);
set_prop_exit:
	mutex_unlock(&host_ctx->lock);
	kfree(prop_packet);
	return ret;
}

int32_t npu_host_get_fw_property(struct npu_device *npu_dev,
	struct msm_npu_property *property)
{
	int ret = 0, i;
	struct ipc_cmd_prop_pkt *prop_packet = NULL;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct msm_npu_property *prop_from_fw;
	uint32_t num_of_params, pkt_size;
	struct npu_misc_cmd *misc_cmd = NULL;

	if (property->prop_id < MSM_NPU_FW_PROP_ID_START) {
		NPU_ERR("Not supported fw property id %x\n",
			property->prop_id);
		return -EINVAL;
	}

	num_of_params = min_t(uint32_t, property->num_of_params,
		(uint32_t)PROP_PARAM_MAX_SIZE);
	pkt_size = sizeof(*prop_packet) + num_of_params * sizeof(uint32_t);
	prop_packet = kzalloc(pkt_size, GFP_KERNEL);

	if (!prop_packet)
		return -ENOMEM;

	prop_packet->header.cmd_type = NPU_IPC_CMD_GET_PROPERTY;
	prop_packet->header.size = pkt_size;
	prop_packet->header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	prop_packet->header.flags = 0;

	prop_packet->prop_id = property->prop_id;
	prop_packet->num_params = num_of_params;
	prop_packet->network_hdl = property->network_hdl;
	for (i = 0; i < num_of_params; i++)
		prop_packet->prop_param[i] = property->prop_param[i];

	mutex_lock(&host_ctx->lock);
	misc_cmd = npu_alloc_misc_cmd(host_ctx);
	if (!misc_cmd) {
		NPU_ERR("Can't allocate misc_cmd\n");
		ret = -ENOMEM;
		goto get_prop_exit;
	}

	misc_cmd->cmd_type = NPU_IPC_CMD_GET_PROPERTY;
	misc_cmd->trans_id = prop_packet->header.trans_id;
	npu_queue_misc_cmd(host_ctx, misc_cmd);

	ret = npu_send_misc_cmd(npu_dev, IPC_QUEUE_APPS_EXEC,
		prop_packet, misc_cmd);
	NPU_DBG("NPU_IPC_CMD_GET_PROPERTY sent status: %d\n", ret);

	if (ret) {
		NPU_ERR("NPU_IPC_CMD_GET_PROPERTY failed\n");
		goto free_misc_cmd;
	}
	mutex_unlock(&host_ctx->lock);

	ret = wait_for_completion_interruptible_timeout(
		&misc_cmd->cmd_done,
		(host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT : NW_CMD_TIMEOUT);

	mutex_lock(&host_ctx->lock);
	if (!ret) {
		pr_err_ratelimited("npu: NPU_IPC_CMD_GET_PROPERTY time out\n");
		ret = -ETIMEDOUT;
		goto free_misc_cmd;
	} else if (ret < 0) {
		NPU_ERR("Wait for get_property done interrupted by signal\n");
		goto free_misc_cmd;
	}

	ret = misc_cmd->ret_status;
	if (!ret) {
		/* Return prop data retrieved from fw to user */
		prop_from_fw = &misc_cmd->u.prop;
		if (property->prop_id == prop_from_fw->prop_id &&
			property->network_hdl == prop_from_fw->network_hdl) {
			property->num_of_params = num_of_params;
			for (i = 0; i < num_of_params; i++)
				property->prop_param[i] =
					prop_from_fw->prop_param[i];
		}
	} else {
		NPU_ERR("get fw property failed %d\n", ret);
	}

free_misc_cmd:
	npu_dequeue_misc_cmd(host_ctx, misc_cmd);
	npu_free_misc_cmd(host_ctx, misc_cmd);
get_prop_exit:
	mutex_unlock(&host_ctx->lock);
	kfree(prop_packet);
	return ret;
}

int32_t npu_host_load_network_v2(struct npu_client *client,
			struct msm_npu_load_network_ioctl_v2 *load_ioctl,
			struct msm_npu_patch_info_v2 *patch_info)
{
	int ret = 0, retry_cnt = 1, i;
	struct npu_device *npu_dev = client->npu_dev;
	struct npu_pwrctrl *pwr = &npu_dev->pwrctrl;
	struct npu_network *network;
	struct ipc_cmd_load_pkt_v2 *load_packet = NULL;
	struct ipc_cmd_unload_pkt unload_packet;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct npu_network_cmd *load_cmd = NULL;
	uint32_t num_patch_params, pkt_size;

	ret = enable_fw(npu_dev);
	if (ret)
		return ret;

	mutex_lock(&host_ctx->lock);
	network = alloc_network(host_ctx, client);
	if (!network) {
		ret = -ENOMEM;
		goto err_deinit_fw;
	}

	network_get(network);
	num_patch_params = load_ioctl->patch_info_num;
	pkt_size = sizeof(*load_packet) +
		num_patch_params * sizeof(struct npu_patch_tuple_v2);
	load_packet = kzalloc(pkt_size, GFP_KERNEL);

	if (!load_packet) {
		ret = -ENOMEM;
		goto error_free_network;
	}

	for (i = 0; i < num_patch_params; i++)
		host_copy_patch_data_v2(&load_packet->patch_params[i],
			&patch_info[i]);

	network->buf_hdl = load_ioctl->buf_ion_hdl;
	network->size = load_ioctl->buf_size;
	network->phy_add = load_ioctl->buf_phys_addr;
	network->first_block_size = load_ioctl->first_block_size;
	network->priority = load_ioctl->priority;
	network->cur_perf_mode = network->init_perf_mode =
		(load_ioctl->perf_mode == PERF_MODE_DEFAULT) ?
		pwr->num_pwrlevels : load_ioctl->perf_mode;
	network->num_layers = load_ioctl->num_layers;

	/* verify mapped physical address */
	if (!npu_mem_verify_addr(client, network->phy_add)) {
		NPU_ERR("Invalid network address %llx\n", network->phy_add);
		ret = -EINVAL;
		goto error_free_network;
	}

	ret = set_perf_mode(npu_dev);
	if (ret) {
		NPU_ERR("set_perf_mode failed\n");
		goto error_free_network;
	}

	load_packet->header.cmd_type = NPU_IPC_CMD_LOAD_V2;
	load_packet->header.size = pkt_size;
	load_packet->header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	load_packet->header.flags = load_ioctl->flags;

	/* ACO Buffer. Use the npu mapped aco address */
	load_packet->buf_pkt.address = (uint32_t)network->phy_add;
	load_packet->buf_pkt.buf_size = network->first_block_size;
	load_packet->buf_pkt.network_id = network->id;
	load_packet->buf_pkt.num_layers = network->num_layers;
	load_packet->num_patch_params = num_patch_params;

	load_cmd = npu_alloc_network_cmd(host_ctx, 0);
	if (!load_cmd) {
		NPU_ERR("Can't allocate load_cmd\n");
		ret = -ENOMEM;
		goto error_free_network;
	}

	load_cmd->cmd_id = 0;
	load_cmd->cmd_type = NPU_IPC_CMD_LOAD_V2;
	load_cmd->trans_id = load_packet->header.trans_id;
	load_cmd->async = false;
	npu_queue_network_cmd(network, load_cmd);

	/* NPU_IPC_CMD_LOAD_V2 will go onto IPC_QUEUE_APPS_EXEC */
	ret = npu_send_network_cmd(npu_dev, network, load_packet, load_cmd);
	if (ret) {
		NPU_ERR("NPU_IPC_CMD_LOAD_V2 sent failed: %d\n", ret);
		goto free_load_cmd;
	}

	mutex_unlock(&host_ctx->lock);

retry:
	ret = wait_for_completion_timeout(
		&load_cmd->cmd_done,
		(host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT : NW_CMD_TIMEOUT);

	mutex_lock(&host_ctx->lock);

	if (network->fw_error) {
		ret = -EIO;
		NPU_ERR("fw is in error state during load_v2 network\n");
		goto free_load_cmd;
	}

	if (!ret) {
		NPU_ERR("npu: NPU_IPC_CMD_LOAD time out %lld:%d\n",
			network->id, load_cmd->trans_id);
		if (retry_cnt > 0) {
			NPU_WARN("Retry IPC queue\n");
			retry_cnt--;
			mutex_unlock(&host_ctx->lock);
			host_session_msg_hdlr(npu_dev);
			goto retry;
		}

		npu_dump_debug_info(npu_dev);
		ret = -ETIMEDOUT;
		goto error_load_network;
	}

	ret = load_cmd->ret_status;
	if (ret) {
		NPU_ERR("load network failed status %d\n", ret);
		goto free_load_cmd;
	}

	load_ioctl->network_hdl = network->network_hdl;
	network->is_active = true;
	kfree(load_packet);
	npu_dequeue_network_cmd(network, load_cmd);
	npu_free_network_cmd(host_ctx, load_cmd);
	network_put(network);
	mutex_unlock(&host_ctx->lock);

	return ret;

error_load_network:
	NPU_DBG("Unload network %lld\n", network->id);
	/* send NPU_IPC_CMD_UNLOAD command to fw */
	unload_packet.header.cmd_type = NPU_IPC_CMD_UNLOAD;
	unload_packet.header.size = sizeof(struct ipc_cmd_unload_pkt);
	unload_packet.header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	unload_packet.header.flags = 0;
	unload_packet.network_hdl = (uint32_t)network->network_hdl;
	npu_send_network_cmd(npu_dev, network, &unload_packet, NULL);
	/* wait 200 ms to make sure fw has processed this command */
	msleep(200);
free_load_cmd:
	npu_dequeue_network_cmd(network, load_cmd);
	npu_free_network_cmd(host_ctx, load_cmd);
error_free_network:
	kfree(load_packet);
	network_put(network);
	free_network(host_ctx, client, network->id);
err_deinit_fw:
	mutex_unlock(&host_ctx->lock);
	disable_fw(npu_dev);
	return ret;
}

int32_t npu_host_unload_network(struct npu_client *client,
			struct msm_npu_unload_network_ioctl *unload)
{
	int ret = 0, retry_cnt = 1;
	struct npu_device *npu_dev = client->npu_dev;
	struct ipc_cmd_unload_pkt unload_packet;
	struct npu_network *network;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct npu_network_cmd *unload_cmd = NULL;

	/* get the corresponding network for ipc trans id purpose */
	mutex_lock(&host_ctx->lock);
	network = get_network_by_hdl(host_ctx, client,
		unload->network_hdl);
	if (!network) {
		mutex_unlock(&host_ctx->lock);
		return -EINVAL;
	}

	if (!network->is_active) {
		NPU_ERR("network is not active\n");
		network_put(network);
		mutex_unlock(&host_ctx->lock);
		return -EINVAL;
	}

	if (network->fw_error) {
		NPU_ERR("fw in error state, skip unload network in fw\n");
		goto free_network;
	}

	NPU_DBG("Unload network %lld\n", network->id);
	/* prepare IPC packet for UNLOAD */
	unload_packet.header.cmd_type = NPU_IPC_CMD_UNLOAD;
	unload_packet.header.size = sizeof(struct ipc_cmd_unload_pkt);
	unload_packet.header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	unload_packet.header.flags = 0;
	unload_packet.network_hdl = (uint32_t)network->network_hdl;

	unload_cmd = npu_alloc_network_cmd(host_ctx, 0);
	if (!unload_cmd) {
		NPU_ERR("Can't allocate unload_cmd\n");
		ret = -ENOMEM;
		goto free_network;
	}

	unload_cmd->cmd_id = 0;
	unload_cmd->cmd_type = NPU_IPC_CMD_UNLOAD;
	unload_cmd->trans_id = unload_packet.header.trans_id;
	unload_cmd->async = false;
	npu_queue_network_cmd(network, unload_cmd);

	/* NPU_IPC_CMD_UNLOAD will go onto IPC_QUEUE_APPS_EXEC */
	ret = npu_send_network_cmd(npu_dev, network, &unload_packet,
		unload_cmd);

	if (ret) {
		NPU_ERR("NPU_IPC_CMD_UNLOAD sent failed: %d\n", ret);
		/*
		 * If another command is running on this network,
		 * don't free_network now.
		 */
		if (ret == -EBUSY) {
			NPU_ERR("Network is running, retry later\n");
			npu_dequeue_network_cmd(network, unload_cmd);
			npu_free_network_cmd(host_ctx, unload_cmd);
			network_put(network);
			mutex_unlock(&host_ctx->lock);
			return ret;
		}
		goto free_unload_cmd;
	}

	mutex_unlock(&host_ctx->lock);

retry:
	ret = wait_for_completion_timeout(
		&unload_cmd->cmd_done,
		(host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT : NW_CMD_TIMEOUT);

	mutex_lock(&host_ctx->lock);

	if (network->fw_error) {
		ret = -EIO;
		NPU_ERR("fw is in error state during unload network\n");
		goto free_network;
	}

	if (!ret) {
		NPU_ERR("npu: NPU_IPC_CMD_UNLOAD time out %llx:%d\n",
			network->id, unload_cmd->trans_id);
		if (retry_cnt > 0) {
			NPU_WARN("Retry IPC queue\n");
			retry_cnt--;
			mutex_unlock(&host_ctx->lock);
			host_session_msg_hdlr(npu_dev);
			goto retry;
		}

		npu_dump_debug_info(npu_dev);
		ret = -ETIMEDOUT;
		goto free_unload_cmd;
	}

	ret = unload_cmd->ret_status;
	NPU_DBG("unload network status %d\n", ret);

free_unload_cmd:
	npu_dequeue_network_cmd(network, unload_cmd);
	npu_free_network_cmd(host_ctx, unload_cmd);
free_network:
	/*
	 * free the network on the kernel if the corresponding ACO
	 * handle is unloaded on the firmware side
	 */
	network_put(network);
	free_network(host_ctx, client, network->id);

	/* recalculate uc_power_level after unload network */
	if (npu_dev->pwrctrl.cur_dcvs_activity)
		set_perf_mode(npu_dev);

	mutex_unlock(&host_ctx->lock);

	disable_fw(npu_dev);

	return ret;
}

int32_t npu_host_exec_network_v2(struct npu_client *client,
	struct msm_npu_exec_network_ioctl_v2 *exec_ioctl,
	struct msm_npu_patch_buf_info *patch_buf_info)
{
	struct npu_device *npu_dev = client->npu_dev;
	struct ipc_cmd_execute_pkt_v2 *exec_packet;
	struct npu_network_cmd *exec_cmd = NULL;
	int32_t ret;
	struct npu_network *network;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	uint32_t num_patch_params, pkt_size;
	bool async_ioctl = !!exec_ioctl->async;
	int i, retry_cnt = 1;

	mutex_lock(&host_ctx->lock);
	network = get_network_by_hdl(host_ctx, client,
		exec_ioctl->network_hdl);

	if (!network) {
		mutex_unlock(&host_ctx->lock);
		return -EINVAL;
	}

	if (!network->is_active) {
		NPU_ERR("network is not active\n");
		ret = -EINVAL;
		goto exec_v2_done;
	}

	if (network->fw_error) {
		NPU_ERR("fw is in error state\n");
		ret = -EIO;
		goto exec_v2_done;
	}

	if (network->is_async && !async_ioctl) {
		NPU_ERR("network is in async mode\n");
		ret = -EINVAL;
		goto exec_v2_done;
	}

	network->is_async = async_ioctl;

	NPU_DBG("execute_v2 network %lld\n", network->id);
	num_patch_params = exec_ioctl->patch_buf_info_num;
	pkt_size = num_patch_params * sizeof(struct npu_patch_params_v2) +
		sizeof(*exec_packet);
	exec_packet = kzalloc(pkt_size, GFP_KERNEL);

	if (!exec_packet) {
		ret = -ENOMEM;
		goto exec_v2_done;
	}

	for (i = 0; i < num_patch_params; i++) {
		exec_packet->patch_params[i].id = patch_buf_info[i].buf_id;
		NPU_DBG("%d: patch_id: %x\n", i,
			exec_packet->patch_params[i].id);
		exec_packet->patch_params[i].value =
			patch_buf_info[i].buf_phys_addr;
		NPU_DBG("%d: patch value: %x\n", i,
			exec_packet->patch_params[i].value);

		/* verify mapped physical address */
		if (!npu_mem_verify_addr(client,
			patch_buf_info[i].buf_phys_addr)) {
			NPU_ERR("Invalid patch value\n");
			ret = -EINVAL;
			goto free_exec_packet;
		}
	}

	exec_packet->header.cmd_type = NPU_IPC_CMD_EXECUTE_V2;
	exec_packet->header.size = pkt_size;
	exec_packet->header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	exec_packet->header.flags = host_ctx->exec_flags_override > 0 ?
		host_ctx->exec_flags_override : exec_ioctl->flags;
	exec_packet->network_hdl = network->network_hdl;
	exec_packet->num_patch_params = num_patch_params;

	exec_cmd = npu_alloc_network_cmd(host_ctx, exec_ioctl->stats_buf_size);
	if (!exec_cmd) {
		NPU_ERR("Can't allocate exec_cmd\n");
		ret = -ENOMEM;
		goto free_exec_packet;
	}

	exec_cmd->stats_buf_u = (void __user *)exec_ioctl->stats_buf_addr;
	exec_cmd->cmd_id = exec_ioctl->async;
	exec_cmd->cmd_type = NPU_IPC_CMD_EXECUTE_V2;
	exec_cmd->trans_id = exec_packet->header.trans_id;
	exec_cmd->async = async_ioctl;
	npu_queue_network_cmd(network, exec_cmd);

	NPU_DBG("Execute_v2 flags %x stats_buf_size %d\n",
		exec_packet->header.flags, exec_ioctl->stats_buf_size);

	ret = npu_send_network_cmd(npu_dev, network, exec_packet, exec_cmd);

	if (ret) {
		NPU_ERR("NPU_IPC_CMD_EXECUTE_V2 sent failed: %d\n", ret);
		goto free_exec_cmd;
	}

	if (async_ioctl) {
		NPU_DBG("Async ioctl, return now\n");
		goto free_exec_packet;
	}

	mutex_unlock(&host_ctx->lock);

retry:
	ret = wait_for_completion_timeout(
		&exec_cmd->cmd_done,
		(host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT : NW_CMD_TIMEOUT);

	mutex_lock(&host_ctx->lock);
	if (network->fw_error) {
		ret = -EIO;
		NPU_ERR("fw is in error state during execute_v2 network\n");
		goto free_exec_cmd;
	}

	if (!ret) {
		NPU_ERR("npu: %llx:%d NPU_IPC_CMD_EXECUTE_V2 time out\n",
			network->id, exec_cmd->trans_id);
		if (retry_cnt > 0) {
			NPU_WARN("Retry IPC queue\n");
			retry_cnt--;
			mutex_unlock(&host_ctx->lock);
			host_session_msg_hdlr(npu_dev);
			goto retry;
		}

		npu_dump_debug_info(npu_dev);
		ret = -ETIMEDOUT;
		goto free_exec_packet;
	}

	ret = exec_cmd->ret_status;
	if (ret) {
		NPU_ERR("execution failed %d\n", ret);
		goto free_exec_cmd;
	}

	exec_ioctl->stats_buf_size = exec_cmd->stats_buf_size;
	if (copy_to_user(
		(void __user *)exec_ioctl->stats_buf_addr,
		exec_cmd->stats_buf,
		exec_ioctl->stats_buf_size)) {
		NPU_ERR("copy stats to user failed\n");
		exec_ioctl->stats_buf_size = 0;
	}

free_exec_cmd:
	npu_dequeue_network_cmd(network, exec_cmd);
	npu_free_network_cmd(host_ctx, exec_cmd);
free_exec_packet:
	kfree(exec_packet);
exec_v2_done:
	network_put(network);
	mutex_unlock(&host_ctx->lock);

	/*
	 * treat network execution timed out as error in order to
	 * force npu fw to stop execution
	 */
	if (ret == -ETIMEDOUT) {
		NPU_ERR("Error handling after execution failure\n");
		host_error_hdlr(npu_dev, true);
	}

	return ret;
}

int32_t npu_host_loopback_test(struct npu_device *npu_dev)
{
	struct ipc_cmd_loopback_pkt loopback_packet;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct npu_misc_cmd *misc_cmd = NULL;
	int32_t ret;

	ret = enable_fw(npu_dev);
	if (ret)
		return ret;

	mutex_lock(&host_ctx->lock);

	loopback_packet.header.cmd_type = NPU_IPC_CMD_LOOPBACK;
	loopback_packet.header.size = sizeof(struct ipc_cmd_loopback_pkt);
	loopback_packet.header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	loopback_packet.header.flags = 0;
	loopback_packet.loopbackParams = 15;

	misc_cmd = npu_alloc_misc_cmd(host_ctx);
	if (!misc_cmd) {
		NPU_ERR("Can't allocate misc_cmd\n");
		ret = -ENOMEM;
		goto loopback_exit;
	}

	misc_cmd->cmd_type = NPU_IPC_CMD_LOOPBACK;
	misc_cmd->trans_id = loopback_packet.header.trans_id;
	npu_queue_misc_cmd(host_ctx, misc_cmd);

	ret = npu_send_misc_cmd(npu_dev, IPC_QUEUE_APPS_EXEC, &loopback_packet,
		misc_cmd);

	if (ret) {
		NPU_ERR("NPU_IPC_CMD_LOOPBACK sent failed: %d\n", ret);
		goto free_misc_cmd;
	}

	mutex_unlock(&host_ctx->lock);

	ret = wait_for_completion_interruptible_timeout(
		&misc_cmd->cmd_done,
		(host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT : NW_CMD_TIMEOUT);

	mutex_lock(&host_ctx->lock);

	if (!ret) {
		NPU_ERR("npu: NPU_IPC_CMD_LOOPBACK time out\n");
		npu_dump_debug_info(npu_dev);
		ret = -ETIMEDOUT;
	} else if (ret < 0) {
		NPU_ERR("Wait for loopback done interrupted by signal\n");
	} else {
		ret = misc_cmd->ret_status;
	}

free_misc_cmd:
	npu_dequeue_misc_cmd(host_ctx, misc_cmd);
	npu_free_misc_cmd(host_ctx, misc_cmd);
loopback_exit:
	mutex_unlock(&host_ctx->lock);
	disable_fw(npu_dev);

	return ret;
}

void npu_host_cleanup_networks(struct npu_client *client)
{
	int i;
	struct npu_device *npu_dev = client->npu_dev;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct msm_npu_unload_network_ioctl unload_req;
	struct msm_npu_unmap_buf_ioctl unmap_req;
	struct npu_network *network;
	struct npu_ion_buf *ion_buf;

	for (i = 0; i < MAX_LOADED_NETWORK; i++) {
		network = &host_ctx->networks[i];
		if (network->client == client) {
			NPU_WARN("network %d is not unloaded before close\n",
				network->network_hdl);
			unload_req.network_hdl = network->network_hdl;
			npu_host_unload_network(client, &unload_req);
		}
	}

	/* unmap all remaining buffers */
	while (!list_empty(&client->mapped_buffer_list)) {
		ion_buf = list_first_entry(&client->mapped_buffer_list,
			struct npu_ion_buf, list);
		NPU_DBG("unmap buffer %x:%llx\n", ion_buf->fd, ion_buf->iova);
		unmap_req.buf_ion_hdl = ion_buf->fd;
		unmap_req.npu_phys_addr = ion_buf->iova;
		npu_host_unmap_buf(client, &unmap_req);
	}
}

/*
 * set network or global perf_mode
 * if network_hdl is 0, set global perf_mode_override
 * otherwise set network perf_mode: if perf_mode is 0,
 * change network perf_mode to initial perf_mode from
 * load_network
 */
int32_t npu_host_set_perf_mode(struct npu_client *client, uint32_t network_hdl,
	uint32_t perf_mode)
{
	int ret = 0;
	struct npu_device *npu_dev = client->npu_dev;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct npu_network *network = NULL;

	mutex_lock(&host_ctx->lock);

	if (network_hdl == 0) {
		NPU_DBG("change perf_mode_override to %d\n", perf_mode);
		npu_dev->pwrctrl.perf_mode_override = perf_mode;
	} else {
		network = get_network_by_hdl(host_ctx, client, network_hdl);
		if (!network) {
			NPU_ERR("invalid network handle %x\n", network_hdl);
			mutex_unlock(&host_ctx->lock);
			return -EINVAL;
		}

		if (perf_mode == 0) {
			network->cur_perf_mode = network->init_perf_mode;
			NPU_DBG("change network %d perf_mode back to %d\n",
				network_hdl, network->cur_perf_mode);
		} else {
			network->cur_perf_mode = perf_mode;
			NPU_DBG("change network %d perf_mode to %d\n",
				network_hdl, network->cur_perf_mode);
		}
	}

	ret = set_perf_mode(npu_dev);
	if (ret)
		NPU_ERR("set_perf_mode failed\n");

	if (network)
		network_put(network);
	mutex_unlock(&host_ctx->lock);

	return ret;
}

/*
 * get the currently set network or global perf_mode
 * if network_hdl is 0, get global perf_mode_override
 * otherwise get network perf_mode
 */
int32_t npu_host_get_perf_mode(struct npu_client *client, uint32_t network_hdl)
{
	int param_val = 0;
	struct npu_device *npu_dev = client->npu_dev;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct npu_network *network = NULL;

	mutex_lock(&host_ctx->lock);

	if (network_hdl == 0) {
		param_val = npu_dev->pwrctrl.perf_mode_override;
	} else {
		network = get_network_by_hdl(host_ctx, client, network_hdl);
		if (!network) {
			NPU_ERR("invalid network handle %x\n", network_hdl);
			mutex_unlock(&host_ctx->lock);
			return -EINVAL;
		}
		param_val = network->cur_perf_mode;
		network_put(network);
	}

	mutex_unlock(&host_ctx->lock);

	return param_val;
}

void npu_host_suspend(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	flush_delayed_work(&host_ctx->disable_fw_work);
}
