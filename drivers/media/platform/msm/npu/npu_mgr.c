/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

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

/* -------------------------------------------------------------------------
 * Defines
 * -------------------------------------------------------------------------
 */
#define LOG_MSG_HEADER_SIZE      20
#define LOG_MSG_START_MSG_INDEX  5
#define LOG_MSG_TOTAL_SIZE_INDEX 0
#define LOG_MSG_MSG_ID_INDEX     1

#define NPU_FW_TIMEOUT_POLL_INTERVAL_MS  20
#define NPU_FW_TIMEOUT_MS                1000

/* -------------------------------------------------------------------------
 * File Scope Function Prototypes
 * -------------------------------------------------------------------------
 */
static void host_irq_wq(struct work_struct *work);
static void turn_off_fw_logging(struct npu_device *npu_dev);
static int wait_for_fw_ready(struct npu_device *npu_dev, uint32_t status_bits);
static struct npu_network *alloc_network(struct npu_host_ctx *ctx);
static struct npu_network *get_network_by_hdl(struct npu_host_ctx *ctx,
	uint32_t hdl);
static struct npu_network *get_network_by_id(struct npu_host_ctx *ctx,
	int64_t id);
static void free_network(struct npu_host_ctx *ctx, int64_t id);
static void app_msg_proc(struct npu_host_ctx *host_ctx, uint32_t *msg);
static void log_msg_proc(struct npu_device *npu_dev, uint32_t *msg);
static void host_session_msg_hdlr(struct npu_device *npu_dev);
static void host_session_log_hdlr(struct npu_device *npu_dev);
static int host_error_hdlr(struct npu_device *npu_dev);

/* -------------------------------------------------------------------------
 * Function Definitions - Init / Deinit
 * -------------------------------------------------------------------------
 */
int fw_init(struct npu_device *npu_dev)
{
	uint32_t reg_val = 0;
	unsigned long flags;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	int ret = 0;

	spin_lock_irqsave(&host_ctx->lock, flags);
	if (host_ctx->fw_state == FW_ENABLED) {
		host_ctx->fw_ref_cnt++;
		spin_unlock_irqrestore(&host_ctx->lock, flags);
		return 0;
	} else if (host_ctx->fw_state == FW_ENABLING ||
		host_ctx->fw_state == FW_DISABLING) {
		spin_unlock_irqrestore(&host_ctx->lock, flags);
		return -EAGAIN;
	}

	host_ctx->fw_state = FW_ENABLING;
	spin_unlock_irqrestore(&host_ctx->lock, flags);

	if (npu_enable_core_power(npu_dev)) {
		ret = -EPERM;
		goto enable_pw_fail;
	}

	if (npu_enable_sys_cache(npu_dev)) {
		ret = -EPERM;
		goto enable_sys_cache_fail;
	}

	/* Boot the NPU subsystem */
	host_ctx->subsystem_handle = subsystem_get_local("npu");
	if (IS_ERR(host_ctx->subsystem_handle)) {
		pr_err("pil load npu fw failed\n");
		ret = -ENODEV;
		goto subsystem_get_fail;
	}

	/* Clear control/status registers */
	REGW(npu_dev, REG_NPU_FW_CTRL_STATUS, 0x0);
	REGW(npu_dev, REG_NPU_HOST_CTRL_VALUE, 0x0);
	REGW(npu_dev, REG_FW_TO_HOST_EVENT, 0x0);
	if (host_ctx->fw_dbg_mode & FW_DBG_MODE_PAUSE) {
		pr_debug("fw_dbg_mode %x\n", host_ctx->fw_dbg_mode);
		REGW(npu_dev, REG_NPU_HOST_CTRL_STATUS,
			HOST_CTRL_STATUS_FW_PAUSE_VAL);
	} else {
		REGW(npu_dev, REG_NPU_HOST_CTRL_STATUS, 0x0);
	}
	/* Read back to flush all registers for fw to read */
	REGR(npu_dev, REG_NPU_HOST_CTRL_STATUS);

	/* Post PIL clocks */
	if (npu_enable_post_pil_clocks(npu_dev)) {
		ret = -EPERM;
		goto enable_post_clk_fail;
	}

	/*
	 * Set logging state and clock gating state
	 * during FW bootup initialization
	 */
	reg_val = REGR(npu_dev, REG_NPU_HOST_CTRL_STATUS);

	/* Enable clock gating only if the HW access platform allows it */
	if (npu_hw_clk_gating_enabled())
		reg_val |= HOST_CTRL_STATUS_BOOT_ENABLE_CLK_GATE_VAL;

	REGW(npu_dev, REG_NPU_HOST_CTRL_STATUS, reg_val);

	/* Initialize the host side IPC */
	npu_host_ipc_pre_init(npu_dev);

	/* Keep reading ctrl status until NPU is ready */
	pr_debug("waiting for status ready from fw\n");

	if (wait_for_fw_ready(npu_dev,
		FW_CTRL_STATUS_MAIN_THREAD_READY_BIT)) {
		ret = -EPERM;
		goto wait_fw_ready_fail;
	}

	npu_host_ipc_post_init(npu_dev);

	if (npu_enable_irq(npu_dev)) {
		ret = -EPERM;
		goto wait_fw_ready_fail;
	}

	/* Set logging state */
	if (!npu_hw_log_enabled()) {
		pr_debug("fw logging disabled\n");
		turn_off_fw_logging(npu_dev);
	}

	spin_lock_irqsave(&host_ctx->lock, flags);
	host_ctx->fw_state = FW_ENABLED;
	host_ctx->fw_ref_cnt++;
	spin_unlock_irqrestore(&host_ctx->lock, flags);
	pr_debug("firmware init complete\n");

	return ret;

wait_fw_ready_fail:
	npu_disable_post_pil_clocks(npu_dev);
enable_post_clk_fail:
	subsystem_put_local(host_ctx->subsystem_handle);
subsystem_get_fail:
	npu_disable_sys_cache(npu_dev);
enable_sys_cache_fail:
	npu_disable_core_power(npu_dev);
enable_pw_fail:
	host_ctx->fw_state = FW_DISABLED;
	return ret;
}

void fw_deinit(struct npu_device *npu_dev, bool fw_alive)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct ipc_cmd_shutdown_pkt cmd_shutdown_pkt;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&host_ctx->lock, flags);
	host_ctx->fw_ref_cnt--;
	pr_debug("fw_ref_cnt %d\n", host_ctx->fw_ref_cnt);
	if ((host_ctx->fw_state != FW_ENABLED) ||
		(host_ctx->fw_ref_cnt > 0 && fw_alive)) {
		spin_unlock_irqrestore(&host_ctx->lock, flags);
		return;
	}
	host_ctx->fw_state = FW_DISABLING;
	spin_unlock_irqrestore(&host_ctx->lock, flags);

	npu_disable_irq(npu_dev);

	if (fw_alive) {
		/* Command header */
		cmd_shutdown_pkt.header.cmd_type = NPU_IPC_CMD_SHUTDOWN;
		cmd_shutdown_pkt.header.size =
			sizeof(struct ipc_cmd_shutdown_pkt);
		cmd_shutdown_pkt.header.trans_id = 1;
		cmd_shutdown_pkt.header.flags = 0xF;
		ret = npu_host_ipc_send_cmd(npu_dev,
			IPC_QUEUE_CMD_HIGH_PRIORITY, &cmd_shutdown_pkt);

		pr_debug("NPU_IPC_CMD_SHUTDOWN sent status: %d\n", ret);

		if (ret) {
			pr_err("npu_host_ipc_send_cmd failed\n");
		} else {
			/* Keep reading ctrl status until NPU shuts down */
			pr_debug("waiting for shutdown status from fw\n");
			if (wait_for_fw_ready(npu_dev,
				FW_CTRL_STATUS_SHUTDOWN_DONE_VAL)) {
				pr_err("wait for fw shutdown timedout\n");
				ret = -ETIMEDOUT;
			}
		}
	}

	npu_disable_post_pil_clocks(npu_dev);
	npu_disable_sys_cache(npu_dev);
	subsystem_put_local(host_ctx->subsystem_handle);
	host_ctx->fw_state = FW_DISABLED;
	npu_disable_core_power(npu_dev);

	pr_debug("firmware deinit complete\n");
	return;
}

int npu_host_init(struct npu_device *npu_dev)
{
	int sts = 0;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	init_completion(&host_ctx->loopback_done);

	host_ctx->sys_cache_disable = 0;
	spin_lock_init(&host_ctx->lock);
	host_ctx->fw_state = FW_DISABLED;
	host_ctx->fw_ref_cnt = 0;
	host_ctx->exec_flags_override = 0;
	host_ctx->fw_dbg_mode = 0;
	atomic_set(&host_ctx->ipc_trans_id, 1);

	host_ctx->wq = npu_create_wq(host_ctx, "irq_hdl", host_irq_wq,
		&host_ctx->irq_work);
	if (!host_ctx->wq)
		sts = -EPERM;

	return sts;
}

void npu_host_deinit(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	npu_destroy_wq(host_ctx->wq);
}

/* -------------------------------------------------------------------------
 * Function Definitions - Interrupt Handler
 * -------------------------------------------------------------------------
 */
irqreturn_t npu_intr_hdler(int irq, void *ptr)
{
	/* Check the interrupt we received */
	/* Currently this is the IPC interrupt */
	struct npu_device *npu_dev = (struct npu_device *)ptr;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	INTERRUPT_ACK(npu_dev, irq);

	/* Check that the event thread currently is running */
	if (host_ctx->wq != 0)
		queue_work(host_ctx->wq, &host_ctx->irq_work);

	return IRQ_HANDLED;
}

/* -------------------------------------------------------------------------
 * Function Definitions - Control
 * -------------------------------------------------------------------------
 */
static int host_error_hdlr(struct npu_device *npu_dev)
{
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	struct npu_network *network = NULL;
	bool fw_alive = true;
	int i;

	if ((host_ctx->wdg_irq_sts == 0) && (host_ctx->err_irq_sts == 0))
		return 0;

	if (host_ctx->wdg_irq_sts) {
		pr_info("watchdog irq triggered\n");
		fw_alive = false;
	}

	fw_deinit(npu_dev, fw_alive);
	fw_init(npu_dev);

	host_ctx->wdg_irq_sts = 0;
	host_ctx->err_irq_sts = 0;

	/* flush all pending npu cmds with error */
	for (i = 0; i < MAX_LOADED_NETWORK; i++) {
		network = &host_ctx->networks[i];
		if (network->id != 0) {
			network->fw_error = true;
			complete(&network->cmd_done);
		}
	}

	complete_all(&host_ctx->loopback_done);

	return 1;
}

static void host_irq_wq(struct work_struct *work)
{
	struct npu_host_ctx *host_ctx;
	struct npu_device *npu_dev;

	host_ctx = container_of(work, struct npu_host_ctx, irq_work);
	npu_dev = container_of(host_ctx, struct npu_device, host_ctx);

	if (host_error_hdlr(npu_dev))
		return;

	host_session_log_hdlr(npu_dev);
	host_session_msg_hdlr(npu_dev);
}

static void turn_off_fw_logging(struct npu_device *npu_dev)
{
	struct ipc_cmd_log_state_pkt log_packet;
	int ret = 0;

	log_packet.header.cmd_type = NPU_IPC_CMD_CONFIG_LOG;
	log_packet.header.size = sizeof(struct ipc_cmd_log_state_pkt);
	log_packet.header.trans_id = 1;
	log_packet.header.flags = 0xF;
	log_packet.log_state.module_msk = 0;
	log_packet.log_state.level_msk = 0;
	ret = npu_host_ipc_send_cmd(npu_dev, IPC_QUEUE_CMD_HIGH_PRIORITY,
		&log_packet);

	pr_debug("NPU_IPC_CMD_CONFIG_LOG sent status: %d\n", ret);

	if (ret)
		pr_err("npu_host_ipc_send_cmd failed\n");
}

static int wait_for_fw_ready(struct npu_device *npu_dev, uint32_t status_bits)
{
	uint32_t ctrl_sts = 0;
	uint32_t wait_cnt = 0, max_wait_ms;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	max_wait_ms = (host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT_MS : NPU_FW_TIMEOUT_MS;

	/* keep reading ctrl status until NPU is ready */
	while ((ctrl_sts & status_bits) != status_bits) {
		ctrl_sts = REGR(npu_dev, REG_NPU_FW_CTRL_STATUS);
		msleep(NPU_FW_TIMEOUT_POLL_INTERVAL_MS);
		wait_cnt += NPU_FW_TIMEOUT_POLL_INTERVAL_MS;
		if (wait_cnt >= max_wait_ms) {
			pr_err("timeout wait for status %x in %s\n",
				status_bits, __func__);
			return -EPERM;
		}
	}
	pr_debug("status %x ready from fw received\n", status_bits);
	return 0;
}

/* -------------------------------------------------------------------------
 * Function Definitions - Network Management
 * -------------------------------------------------------------------------
 */
static struct npu_network *alloc_network(struct npu_host_ctx *ctx)
{
	int32_t i;
	struct npu_network *network = ctx->networks;
	unsigned long flags;

	spin_lock_irqsave(&ctx->lock, flags);
	for (i = 0; i < MAX_LOADED_NETWORK; i++) {
		if (network->id == 0) {
			network->id = i + 1;
			network->network_hdl = 0;
			break;
		}
		network++;
	}
	if (i >= MAX_LOADED_NETWORK)
		network = NULL;
	else
		ctx->network_num++;
	spin_unlock_irqrestore(&ctx->lock, flags);

	if (network) {
		init_completion(&network->cmd_done);
		network->is_valid = true;
		network->fw_error = false;
	}

	return network;
}

static struct npu_network *get_network_by_hdl(struct npu_host_ctx *ctx,
	uint32_t hdl)
{
	int32_t i;
	struct npu_network *network = ctx->networks;

	for (i = 0; i < MAX_LOADED_NETWORK; i++) {
		if (network->network_hdl == hdl)
			break;

		network++;
	}
	if ((i == MAX_LOADED_NETWORK) || !network->is_valid) {
		pr_err("network hdl invalid %d\n", hdl);
		network = NULL;
	}

	return network;
}

static struct npu_network *get_network_by_id(struct npu_host_ctx *ctx,
	int64_t id)
{
	if (id < 1 || id > MAX_LOADED_NETWORK ||
		!ctx->networks[id - 1].is_valid) {
		pr_err("network id invalid %d\n", (int32_t)id);
		return NULL;
	}

	return &ctx->networks[id - 1];
}

static void free_network(struct npu_host_ctx *ctx, int64_t id)
{
	struct npu_network *network = get_network_by_id(ctx, id);
	unsigned long flags;

	if (network) {
		spin_lock_irqsave(&ctx->lock, flags);
		memset(network, 0, sizeof(struct npu_network));
		ctx->network_num--;
		spin_unlock_irqrestore(&ctx->lock, flags);
	}
}

/* -------------------------------------------------------------------------
 * Function Definitions - IPC
 * -------------------------------------------------------------------------
 */
static void app_msg_proc(struct npu_host_ctx *host_ctx, uint32_t *msg)
{
	uint32_t msg_id;
	struct npu_network *network = NULL;

	msg_id = msg[1];
	switch (msg_id) {
	case NPU_IPC_MSG_EXECUTE_DONE:
	{
		struct ipc_msg_execute_pkt *exe_rsp_pkt =
			(struct ipc_msg_execute_pkt *)msg;

		pr_debug("NPU_IPC_MSG_EXECUTE_DONE status: %d\n",
			exe_rsp_pkt->header.status);
		pr_debug("trans_id : %d", exe_rsp_pkt->header.trans_id);
		pr_debug("e2e_IPC_time: %d (in tick count)\n",
			exe_rsp_pkt->stats.e2e_ipc_tick_count);
		pr_debug("aco_load_time: %d (in tick count)\n",
			exe_rsp_pkt->stats.aco_load_tick_count);
		pr_debug("aco_execute_time: %d (in tick count)\n",
			exe_rsp_pkt->stats.aco_execution_tick_count);
		pr_debug("total_num_layers: %d\n",
			exe_rsp_pkt->stats.exe_stats.total_num_layers);

		network = get_network_by_hdl(host_ctx,
			exe_rsp_pkt->network_hdl);
		if (!network) {
			pr_err("can't find network %x\n",
				exe_rsp_pkt->network_hdl);
			break;
		}

		complete(&network->cmd_done);
		break;
	}
	case NPU_IPC_MSG_EXECUTE_V2_DONE:
	{
		struct ipc_msg_execute_pkt_v2 *exe_rsp_pkt =
			(struct ipc_msg_execute_pkt_v2 *)msg;
		uint32_t stats_size = 0;

		pr_debug("NPU_IPC_MSG_EXECUTE_V2_DONE status: %d\n",
			exe_rsp_pkt->header.status);
		pr_debug("trans_id : %d", exe_rsp_pkt->header.trans_id);

		network = get_network_by_hdl(host_ctx,
			exe_rsp_pkt->network_hdl);
		if (!network) {
			pr_err("can't find network %x\n",
				exe_rsp_pkt->network_hdl);
			break;
		}

		stats_size = exe_rsp_pkt->header.size - sizeof(*exe_rsp_pkt);
		pr_debug("stats_size %d:%d\n", exe_rsp_pkt->header.size,
			stats_size);
		stats_size = stats_size < network->stats_buf_size ?
			stats_size : network->stats_buf_size;
		network->stats_buf_size = stats_size;
		if (stats_size)
			memcpy(network->stats_buf, exe_rsp_pkt->stats_data,
				stats_size);

		complete(&network->cmd_done);
		break;
	}
	case NPU_IPC_MSG_LOAD_DONE:
	{
		uint32_t network_id = 0;
		struct ipc_msg_load_pkt *load_rsp_pkt =
			(struct ipc_msg_load_pkt *)msg;

		pr_debug("NPU_IPC_MSG_LOAD_DONE status: %d, trans_id: %d\n",
			load_rsp_pkt->header.status,
			load_rsp_pkt->header.trans_id);

		/*
		 * the upper 16 bits in returned network_hdl is
		 * the network ID
		 */
		pr_debug("network_hdl: %x\n", load_rsp_pkt->network_hdl);
		network_id = load_rsp_pkt->network_hdl >> 16;
		network = get_network_by_id(host_ctx, network_id);
		if (!network) {
			pr_err("can't find network %d\n", network_id);
			break;
		}
		network->network_hdl = load_rsp_pkt->network_hdl;
		complete(&network->cmd_done);
		break;
	}
	case NPU_IPC_MSG_UNLOAD_DONE:
	{
		struct ipc_msg_unload_pkt *unload_rsp_pkt =
			(struct ipc_msg_unload_pkt *)msg;

		pr_debug("NPU_IPC_MSG_UNLOAD_DONE status: %d, trans_id: %d\n",
			unload_rsp_pkt->header.status,
			unload_rsp_pkt->header.trans_id);

		network = get_network_by_hdl(host_ctx,
			unload_rsp_pkt->network_hdl);
		if (!network) {
			pr_err("can't find network %x\n",
				unload_rsp_pkt->network_hdl);
			break;
		}

		complete(&network->cmd_done);
		break;
	}
	case NPU_IPC_MSG_LOOPBACK_DONE:
	{
		struct ipc_msg_loopback_pkt *lb_rsp_pkt =
			(struct ipc_msg_loopback_pkt *)msg;

		pr_debug("NPU_IPC_MSG_LOOPBACK_DONE loopbackParams: 0x%x\n",
			lb_rsp_pkt->loopbackParams);
		complete_all(&host_ctx->loopback_done);
		break;
	}
	default:
		pr_err("Not supported apps response received %d\n",
			msg_id);
		break;
	}
}

static void host_session_msg_hdlr(struct npu_device *npu_dev)
{
	uint32_t *msg;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	msg = kzalloc(sizeof(uint32_t) * NPU_IPC_BUF_LENGTH, GFP_KERNEL);
	if (!msg)
		return;
	while (npu_host_ipc_read_msg(npu_dev, IPC_QUEUE_APPS_RSP, msg) == 0) {
		pr_debug("received from msg queue\n");
		app_msg_proc(host_ctx, msg);
	}
	kfree(msg);
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
		pr_err("unsupported log response received %d\n", msg_id);
		break;
	}
}

static void host_session_log_hdlr(struct npu_device *npu_dev)
{
	uint32_t *msg;

	msg = kzalloc(sizeof(uint32_t) * NPU_IPC_BUF_LENGTH, GFP_KERNEL);

	if (!msg)
		return;
	while (npu_host_ipc_read_msg(npu_dev, IPC_QUEUE_LOG, msg) == 0) {
		pr_debug("received from log queue\n");
		log_msg_proc(npu_dev, msg);
	}
	kfree(msg);
}

/* -------------------------------------------------------------------------
 * Function Definitions - Functionality
 * -------------------------------------------------------------------------
 */
int32_t npu_host_get_info(struct npu_device *npu_dev,
			struct msm_npu_get_info_ioctl *get_info_ioctl)
{
	get_info_ioctl->firmware_version = FIRMWARE_VERSION;
	return 0;
}

int32_t npu_host_map_buf(struct npu_device *npu_dev,
			struct msm_npu_map_buf_ioctl *map_ioctl)
{
	npu_mem_map(npu_dev, map_ioctl->buf_ion_hdl, map_ioctl->size,
		&map_ioctl->npu_phys_addr);
	return 0;
}

int32_t npu_host_unmap_buf(struct npu_device *npu_dev,
			struct msm_npu_unmap_buf_ioctl *unmap_ioctl)
{
	npu_mem_unmap(npu_dev, unmap_ioctl->buf_ion_hdl,
		unmap_ioctl->npu_phys_addr);
	return 0;
}

static void host_copy_patch_data(struct npu_patch_tuple *param, uint32_t value,
		struct msm_npu_layer *layer_info)
{
	param->value = value;
	param->chunk_id = layer_info->patch_info.chunk_id;
	param->loc_offset = layer_info->patch_info.loc_offset;
	param->instruction_size_in_bytes =
		layer_info->patch_info.instruction_size_in_bytes;
	param->shift_value_in_bits =
		layer_info->patch_info.shift_value_in_bits;
	param->variable_size_in_bits =
		layer_info->patch_info.variable_size_in_bits;

	pr_debug("copy_patch_data: %x %d %x %x %x %x\n",
		param->value,
		param->chunk_id,
		param->loc_offset,
		param->instruction_size_in_bytes,
		param->shift_value_in_bits,
		param->variable_size_in_bits);
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
	pr_debug("copy_patch_data_v2: %x %d %x %x %x %x\n",
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

	/* find the max level among all the networks */
	for (i = 0; i < host_ctx->network_num; i++) {
		if ((network->perf_mode != 0) &&
			(network->perf_mode > max_perf_mode))
			max_perf_mode = network->perf_mode;
		network++;
	}
	pr_debug("max perf mode for networks: %d\n", max_perf_mode);

	return max_perf_mode;
}

int32_t npu_host_load_network(struct npu_device *npu_dev,
			struct msm_npu_load_network_ioctl *load_ioctl)
{
	int ret = 0;
	struct npu_network *network;
	struct ipc_cmd_load_pkt load_packet;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	uint32_t networks_perf_mode = 0;

	ret = fw_init(npu_dev);
	if (ret)
		return ret;

	network = alloc_network(host_ctx);
	if (!network) {
		ret = -ENOMEM;
		goto err_deinit_fw;
	}

	network->buf_hdl = load_ioctl->buf_ion_hdl;
	network->size = load_ioctl->buf_size;
	network->phy_add = load_ioctl->buf_phys_addr;
	network->first_block_size = load_ioctl->first_block_size;
	network->priority = load_ioctl->priority;
	network->perf_mode = load_ioctl->perf_mode;

	networks_perf_mode = find_networks_perf_mode(host_ctx);

	ret = npu_set_uc_power_level(npu_dev, networks_perf_mode);
	if (ret) {
		pr_err("network load failed due to power level set\n");
		goto error_free_network;
	}

	load_packet.header.cmd_type = NPU_IPC_CMD_LOAD;
	load_packet.header.size = sizeof(struct ipc_cmd_load_pkt);
	load_packet.header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	load_packet.header.flags = 0;

	/* ACO Buffer. Use the npu mapped aco address */
	load_packet.buf_pkt.address = (uint64_t)network->phy_add;
	load_packet.buf_pkt.buf_size = network->first_block_size;
	load_packet.buf_pkt.network_id = network->id;

	/* NPU_IPC_CMD_LOAD will go onto IPC_QUEUE_APPS_EXEC */
	reinit_completion(&network->cmd_done);
	ret = npu_host_ipc_send_cmd(npu_dev,
		IPC_QUEUE_APPS_EXEC, &load_packet);

	pr_debug("NPU_IPC_CMD_LOAD sent status: %d\n", ret);

	if (ret) {
		ret = -EIO;
		goto error_free_network;
	}

	if (!wait_for_completion_interruptible_timeout(
		&network->cmd_done,
		(host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT : NW_CMD_TIMEOUT)) {
		pr_err_ratelimited("npu: NPU_IPC_CMD_LOAD time out\n");
		ret = -ETIMEDOUT;
		goto error_free_network;
	} else if (network->fw_error) {
		ret = -EIO;
		pr_err("load cmd returns with error\n");
		goto error_free_network;
	}

	load_ioctl->network_hdl = network->network_hdl;

	return ret;

error_free_network:
	free_network(host_ctx, network->id);
err_deinit_fw:
	fw_deinit(npu_dev, true);
	return ret;
}

int32_t npu_host_load_network_v2(struct npu_device *npu_dev,
			struct msm_npu_load_network_ioctl_v2 *load_ioctl,
			struct msm_npu_patch_info_v2 *patch_info)
{
	int ret = 0, i;
	struct npu_network *network;
	struct ipc_cmd_load_pkt_v2 *load_packet = NULL;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	uint32_t networks_perf_mode = 0;
	uint32_t num_patch_params, pkt_size;

	ret = fw_init(npu_dev);
	if (ret)
		return ret;

	network = alloc_network(host_ctx);
	if (!network) {
		ret = -ENOMEM;
		goto err_deinit_fw;
	}

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
	network->perf_mode = load_ioctl->perf_mode;
	network->num_layers = load_ioctl->num_layers;

	networks_perf_mode = find_networks_perf_mode(host_ctx);

	ret = npu_set_uc_power_level(npu_dev, networks_perf_mode);
	if (ret) {
		pr_err("network load failed due to power level set\n");
		goto error_free_network;
	}

	load_packet->header.cmd_type = NPU_IPC_CMD_LOAD_V2;
	load_packet->header.size = pkt_size;
	load_packet->header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	load_packet->header.flags = 0;

	/* ACO Buffer. Use the npu mapped aco address */
	load_packet->buf_pkt.address = (uint32_t)network->phy_add;
	load_packet->buf_pkt.buf_size = network->first_block_size;
	load_packet->buf_pkt.network_id = network->id;
	load_packet->buf_pkt.num_layers = network->num_layers;
	load_packet->num_patch_params = num_patch_params;

	/* NPU_IPC_CMD_LOAD_V2 will go onto IPC_QUEUE_APPS_EXEC */
	reinit_completion(&network->cmd_done);
	ret = npu_host_ipc_send_cmd(npu_dev,
		IPC_QUEUE_APPS_EXEC, load_packet);

	pr_debug("NPU_IPC_CMD_LOAD_V2 sent status: %d\n", ret);

	if (ret) {
		ret = -EIO;
		goto error_free_network;
	}

	if (!wait_for_completion_interruptible_timeout(
		&network->cmd_done,
		(host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT : NW_CMD_TIMEOUT)) {
		pr_err_ratelimited("npu: NPU_IPC_CMD_LOAD time out\n");
		ret = -ETIMEDOUT;
		goto error_free_network;
	} else if (network->fw_error) {
		ret = -EIO;
		pr_err("load cmd returns with error\n");
		goto error_free_network;
	}

	load_ioctl->network_hdl = network->network_hdl;

	return ret;

error_free_network:
	kfree(load_packet);
	free_network(host_ctx, network->id);
err_deinit_fw:
	fw_deinit(npu_dev, true);
	return ret;
}

int32_t npu_host_unload_network(struct npu_device *npu_dev,
			struct msm_npu_unload_network_ioctl *unload)
{
	int ret = 0;
	struct ipc_cmd_unload_pkt unload_packet;
	struct npu_network *network;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;

	/* get the corresponding network for ipc trans id purpose */
	network = get_network_by_hdl(host_ctx, unload->network_hdl);
	if (!network)
		return -EINVAL;

	if (network->fw_error) {
		pr_err("fw in error state, skip unload network in fw\n");
		goto skip_fw;
	}

	/* prepare IPC packet for UNLOAD */
	unload_packet.header.cmd_type = NPU_IPC_CMD_UNLOAD;
	unload_packet.header.size = sizeof(struct ipc_cmd_unload_pkt);
	unload_packet.header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	unload_packet.header.flags = 0;
	unload_packet.network_hdl = (uint32_t)network->network_hdl;

	/* NPU_IPC_CMD_UNLOAD will go onto IPC_QUEUE_APPS_EXEC */
	reinit_completion(&network->cmd_done);
	ret = npu_host_ipc_send_cmd(npu_dev, IPC_QUEUE_APPS_EXEC,
		&unload_packet);

	if (ret) {
		pr_err("NPU_IPC_CMD_UNLOAD sent failed: %d\n", ret);
		return -EIO;
	}

	if (!wait_for_completion_interruptible_timeout(&network->cmd_done,
		(host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT : NW_CMD_TIMEOUT)) {
		pr_err_ratelimited("npu: NPU_IPC_CMD_UNLOAD time out\n");
		ret = -ETIMEDOUT;
	} else if (network->fw_error) {
		ret = -EIO;
		pr_err("unload cmd returns with error\n");
	}

skip_fw:
	/*
	 * free the network on the kernel if the corresponding ACO
	 * handle is unloaded on the firmware side
	 */
	free_network(host_ctx, network->id);
	fw_deinit(npu_dev, true);

	return ret;
}

int32_t npu_host_exec_network(struct npu_device *npu_dev,
			struct msm_npu_exec_network_ioctl *exec_ioctl)
{
	struct ipc_cmd_execute_pkt exec_packet;
	/* npu mapped addr */
	uint64_t input_addr = 0, output_addr = 0;
	uint64_t input_off, output_off;
	int32_t ret;
	struct npu_network *network;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	int i = 0;

	network = get_network_by_hdl(host_ctx, exec_ioctl->network_hdl);

	if (!network)
		return -EINVAL;

	if (network->fw_error)
		return -EIO;

	memset(&exec_packet, 0, sizeof(exec_packet));
	if (exec_ioctl->patching_required) {
		if (exec_ioctl->input_layer_num == 1)
			input_addr = exec_ioctl->input_layers[0].buf_phys_addr;
		if (exec_ioctl->output_layer_num == 1)
			output_addr =
				exec_ioctl->output_layers[0].buf_phys_addr;
		exec_packet.patch_params.num_params = 2;
		input_off = (uint64_t)input_addr;
		output_off = (uint64_t)output_addr;
		host_copy_patch_data(&exec_packet.patch_params.param[0],
			(uint32_t)input_off, &exec_ioctl->input_layers[0]);
		host_copy_patch_data(&exec_packet.patch_params.param[1],
			(uint32_t)output_off, &exec_ioctl->output_layers[0]);
	} else {
		exec_packet.patch_params.num_params = 0;
	}

	exec_packet.header.cmd_type = NPU_IPC_CMD_EXECUTE;
	exec_packet.header.size = sizeof(struct ipc_cmd_execute_pkt);
	exec_packet.header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	exec_packet.header.flags = 0xF;
	exec_packet.network_hdl = network->network_hdl;

	/* Send it on the high priority queue */
	reinit_completion(&network->cmd_done);
	ret = npu_host_ipc_send_cmd(npu_dev, IPC_QUEUE_APPS_EXEC, &exec_packet);

	pr_debug("NPU_IPC_CMD_EXECUTE sent status: %d\n", ret);

	if (!wait_for_completion_interruptible_timeout(
		&network->cmd_done,
		(host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT : NW_CMD_TIMEOUT)) {
		pr_err_ratelimited("npu: NPU_IPC_CMD_EXECUTE time out\n");
		/* dump debug stats */
		npu_dump_debug_timeout_stats(npu_dev);
		ret = -ETIMEDOUT;
	} else if (network->fw_error) {
		ret = -EIO;
		pr_err("execute cmd returns with error\n");
	}

	/* Invalidate output buffers */
	for (i = 0; i < exec_ioctl->output_layer_num; i++) {
		if (exec_ioctl->output_layer_num == 1) {
			npu_mem_invalidate(npu_dev,
				exec_ioctl->output_layers[i].buf_hdl);
		}
	}

	return ret;
}

int32_t npu_host_exec_network_v2(struct npu_device *npu_dev,
	struct msm_npu_exec_network_ioctl_v2 *exec_ioctl,
	struct msm_npu_patch_buf_info *patch_buf_info)
{
	struct ipc_cmd_execute_pkt_v2 *exec_packet;
	int32_t ret;
	struct npu_network *network;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	uint32_t num_patch_params, pkt_size;
	int i;

	network = get_network_by_hdl(host_ctx, exec_ioctl->network_hdl);

	if (!network)
		return -EINVAL;

	if (network->fw_error)
		return -EIO;

	num_patch_params = exec_ioctl->patch_buf_info_num;
	pkt_size = num_patch_params * sizeof(struct npu_patch_params_v2) +
		sizeof(*exec_packet);
	exec_packet = kzalloc(pkt_size, GFP_KERNEL);

	if (!exec_packet)
		return -ENOMEM;

	for (i = 0; i < num_patch_params; i++) {
		exec_packet->patch_params[i].id = patch_buf_info[i].buf_id;
		pr_debug("%d: patch_id: %x\n", i,
			exec_packet->patch_params[i].id);
		exec_packet->patch_params[i].value =
			patch_buf_info[i].buf_phys_addr;
		pr_debug("%d: patch value: %x\n", i,
			exec_packet->patch_params[i].value);
	}

	exec_packet->header.cmd_type = NPU_IPC_CMD_EXECUTE_V2;
	exec_packet->header.size = pkt_size;
	exec_packet->header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	exec_packet->header.flags = host_ctx->exec_flags_override > 0 ?
		host_ctx->exec_flags_override : exec_ioctl->flags;
	exec_packet->network_hdl = network->network_hdl;
	exec_packet->num_patch_params = num_patch_params;

	/* allocate stats_buf to be filled after execution */
	network->stats_buf = kzalloc(exec_ioctl->stats_buf_size, GFP_KERNEL);
	if (!network->stats_buf)
		network->stats_buf_size = 0;
	else
		network->stats_buf_size = exec_ioctl->stats_buf_size;

	pr_debug("Execute_v2 flags %x stats_buf_size %d\n",
		exec_packet->header.flags, exec_ioctl->stats_buf_size);

	/* Send it on the high priority queue */
	reinit_completion(&network->cmd_done);
	ret = npu_host_ipc_send_cmd(npu_dev, IPC_QUEUE_APPS_EXEC, exec_packet);

	pr_debug("NPU_IPC_CMD_EXECUTE_V2 sent status: %d\n", ret);

	if (!wait_for_completion_interruptible_timeout(
		&network->cmd_done,
		(host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT : NW_CMD_TIMEOUT)) {
		pr_err_ratelimited("npu: NPU_IPC_CMD_EXECUTE_V2 time out\n");
		/* dump debug stats */
		npu_dump_debug_timeout_stats(npu_dev);
		ret = -ETIMEDOUT;
	} else if (network->fw_error) {
		ret = -EIO;
		pr_err("execute cmd returns with error\n");
	}

	if (!ret) {
		exec_ioctl->stats_buf_size = network->stats_buf_size;
		if (exec_ioctl->stats_buf_size) {
			if (copy_to_user(
				(void __user *)exec_ioctl->stats_buf_addr,
				network->stats_buf, exec_ioctl->stats_buf_size))
				pr_err("copy stats to user failed\n");
		}
	}

	kfree(network->stats_buf);
	kfree(exec_packet);
	return ret;
}

int32_t npu_host_loopback_test(struct npu_device *npu_dev)
{
	struct ipc_cmd_loopback_pkt loopback_packet;
	struct npu_host_ctx *host_ctx = &npu_dev->host_ctx;
	int32_t ret;

	ret = fw_init(npu_dev);
	if (ret)
		return ret;

	loopback_packet.header.cmd_type = NPU_IPC_CMD_LOOPBACK;
	loopback_packet.header.size = sizeof(struct ipc_cmd_loopback_pkt);
	loopback_packet.header.trans_id =
		atomic_add_return(1, &host_ctx->ipc_trans_id);
	loopback_packet.header.flags = 0;
	loopback_packet.loopbackParams = 15;

	reinit_completion(&host_ctx->loopback_done);
	ret = npu_host_ipc_send_cmd(npu_dev,
		 IPC_QUEUE_APPS_EXEC, &loopback_packet);

	if (ret) {
		pr_err("NPU_IPC_CMD_LOOPBACK sent failed: %d\n", ret);
	} else if (!wait_for_completion_interruptible_timeout(
		&host_ctx->loopback_done,
		(host_ctx->fw_dbg_mode & FW_DBG_MODE_INC_TIMEOUT) ?
		NW_DEBUG_TIMEOUT : NW_CMD_TIMEOUT)) {
		pr_err_ratelimited("npu: NPU_IPC_CMD_LOOPBACK time out\n");
		ret = -ETIMEDOUT;
	}

	fw_deinit(npu_dev, true);

	return ret;
}
