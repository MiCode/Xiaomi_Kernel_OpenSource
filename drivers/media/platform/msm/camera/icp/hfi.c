/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "HFI-FW %s:%d " fmt, __func__, __LINE__

#include <linux/io.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/random.h>
#include <asm/errno.h>
#include <linux/timer.h>
#include <media/cam_icp.h>
#include "cam_io_util.h"
#include "hfi_reg.h"
#include "hfi_sys_defs.h"
#include "hfi_session_defs.h"
#include "hfi_intf.h"
#include "cam_icp_hw_mgr_intf.h"

#define HFI_VERSION_INFO_MAJOR_VAL  1
#define HFI_VERSION_INFO_MINOR_VAL  1
#define HFI_VERSION_INFO_STEP_VAL   0
#define HFI_VERSION_INFO_STEP_VAL   0
#define HFI_VERSION_INFO_MAJOR_BMSK  0xFF000000
#define HFI_VERSION_INFO_MAJOR_SHFT  24
#define HFI_VERSION_INFO_MINOR_BMSK  0xFFFF00
#define HFI_VERSION_INFO_MINOR_SHFT  8
#define HFI_VERSION_INFO_STEP_BMSK   0xFF
#define HFI_VERSION_INFO_STEP_SHFT  0

#undef  HFI_DBG
#define HFI_DBG(fmt, args...) pr_debug(fmt, ##args)

struct hfi_info *g_hfi;
unsigned int g_icp_mmu_hdl;

int hfi_write_cmd(void *cmd_ptr)
{
	uint32_t size_in_words, empty_space, new_write_idx, read_idx, temp;
	uint32_t *write_q, *write_ptr;
	struct hfi_qtbl *q_tbl;
	struct hfi_q_hdr *q;
	int rc = 0;
	int i = 0;

	if (!cmd_ptr) {
		pr_err("Invalid args\n");
		return -EINVAL;
	}

	if (!g_hfi || g_hfi->hfi_state < FW_START_SENT) {
		pr_err("FW not ready yet\n");
		return -EIO;
	}

	mutex_lock(&g_hfi->cmd_q_lock);

	q_tbl = (struct hfi_qtbl *)g_hfi->map.qtbl.kva;
	q = &q_tbl->q_hdr[Q_CMD];

	write_q = (uint32_t *)g_hfi->map.cmd_q.kva;

	size_in_words = (*(uint32_t *)cmd_ptr) >> BYTE_WORD_SHIFT;
	if (!size_in_words) {
		pr_debug("failed");
		rc = -EINVAL;
		goto err;
	}

	HFI_DBG("size_in_words : %u\n", size_in_words);
	HFI_DBG("q->qhdr_write_idx %x\n", q->qhdr_write_idx);

	read_idx = q->qhdr_read_idx;

	empty_space = (q->qhdr_write_idx >= read_idx) ?
		(q->qhdr_q_size - (q->qhdr_write_idx - read_idx)) :
		(read_idx - q->qhdr_write_idx);
	if (empty_space <= size_in_words) {
		pr_err("failed");
		rc = -EIO;
		goto err;
	}
	HFI_DBG("empty_space : %u\n", empty_space);

	new_write_idx = q->qhdr_write_idx + size_in_words;
	write_ptr = (uint32_t *)(write_q + q->qhdr_write_idx);

	if (new_write_idx < q->qhdr_q_size) {
		memcpy(write_ptr, (uint8_t *)cmd_ptr,
			size_in_words << BYTE_WORD_SHIFT);
	} else {
		new_write_idx -= q->qhdr_q_size;
		temp = (size_in_words - new_write_idx) << BYTE_WORD_SHIFT;
		memcpy(write_ptr, (uint8_t *)cmd_ptr, temp);
		memcpy(write_q, (uint8_t *)cmd_ptr + temp,
			new_write_idx << BYTE_WORD_SHIFT);
	}
	for (i = 0; i < size_in_words; i++)
		pr_debug("%x\n", write_ptr[i]);

	q->qhdr_write_idx = new_write_idx;
	HFI_DBG("q->qhdr_write_idx %x\n", q->qhdr_write_idx);
	cam_io_w((uint32_t)INTR_ENABLE,
		g_hfi->csr_base + HFI_REG_A5_CSR_HOST2ICPINT);
err:
	mutex_unlock(&g_hfi->cmd_q_lock);
	return 0;
}

int hfi_read_message(uint32_t *pmsg, uint8_t q_id)
{
	struct hfi_qtbl *q_tbl_ptr;
	struct hfi_q_hdr *q;
	uint32_t new_read_idx, size_in_words, temp;
	uint32_t *read_q, *read_ptr;
	int rc = 0;
	int i = 0;

	if (!pmsg || q_id > Q_DBG) {
		pr_err("Inavlid args\n");
		return -EINVAL;
	}

	q_tbl_ptr = (struct hfi_qtbl *)g_hfi->map.qtbl.kva;
	q = &q_tbl_ptr->q_hdr[q_id];

	if ((g_hfi->hfi_state < FW_START_SENT) ||
		(q->qhdr_read_idx == q->qhdr_write_idx)) {
		pr_debug("FW or Q not ready, hfi state : %u, r idx : %u, w idx : %u\n",
			g_hfi->hfi_state, q->qhdr_read_idx, q->qhdr_write_idx);
		return -EIO;
	}

	mutex_lock(&g_hfi->msg_q_lock);

	if (q_id == Q_CMD)
		read_q = (uint32_t *)g_hfi->map.cmd_q.kva;
	else if (q_id == Q_MSG)
		read_q = (uint32_t *)g_hfi->map.msg_q.kva;
	else
		read_q = (uint32_t *)g_hfi->map.dbg_q.kva;

	read_ptr = (uint32_t *)(read_q + q->qhdr_read_idx);
	size_in_words = (*read_ptr) >> BYTE_WORD_SHIFT;

	HFI_DBG("size_in_words : %u\n", size_in_words);
	HFI_DBG("read_ptr : %pK\n", (void *)read_ptr);

	if ((size_in_words == 0) ||
		(size_in_words > ICP_HFI_MAX_MSG_SIZE_IN_WORDS)) {
		pr_err("invalid HFI message packet size - 0x%08x\n",
			size_in_words << BYTE_WORD_SHIFT);
		q->qhdr_read_idx = q->qhdr_write_idx;
		rc = -EIO;
		goto err;
	}

	new_read_idx = q->qhdr_read_idx + size_in_words;
	HFI_DBG("new_read_idx : %u\n", new_read_idx);

	if (new_read_idx < q->qhdr_q_size) {
		memcpy(pmsg, read_ptr, size_in_words << BYTE_WORD_SHIFT);
	} else {
		new_read_idx -= q->qhdr_q_size;
		temp = (size_in_words - new_read_idx) << BYTE_WORD_SHIFT;
		memcpy(pmsg, read_ptr, temp);
		memcpy((uint8_t *)pmsg + temp, read_q,
			new_read_idx << BYTE_WORD_SHIFT);
	}

	for (i = 0; i < size_in_words; i++)
		pr_debug("%x\n", read_ptr[i]);

	q->qhdr_read_idx = new_read_idx;
err:
	mutex_unlock(&g_hfi->msg_q_lock);
	HFI_DBG("Exit\n");
	return 0;
}

void hfi_send_system_cmd(uint32_t type, uint64_t data, uint32_t size)
{
	switch (type) {
	case HFI_CMD_SYS_INIT: {
		struct hfi_cmd_sys_init init;

		memset(&init, 0, sizeof(init));

		init.size = sizeof(struct hfi_cmd_sys_init);
		init.pkt_type = type;
		hfi_write_cmd(&init);
	}
		break;
	case HFI_CMD_SYS_PC_PREP: {
		struct hfi_cmd_pc_prep prep;

		prep.size = sizeof(struct hfi_cmd_pc_prep);
		prep.pkt_type = type;
		hfi_write_cmd(&prep);
	}
		break;
	case HFI_CMD_SYS_SET_PROPERTY: {
		struct hfi_cmd_prop prop;

		if ((uint32_t)data == (uint32_t)HFI_PROP_SYS_DEBUG_CFG) {
			prop.size = sizeof(struct hfi_cmd_prop);
			prop.pkt_type = type;
			prop.num_prop = 1;
			prop.prop_data[0] = HFI_PROP_SYS_DEBUG_CFG;
			hfi_write_cmd(&prop);
		}
	}
		break;
	case HFI_CMD_SYS_GET_PROPERTY:
		break;
	case HFI_CMD_SYS_PING: {
		struct hfi_cmd_ping_pkt ping;

		ping.size = sizeof(struct hfi_cmd_ping_pkt);
		ping.pkt_type = type;
		ping.user_data = (uint64_t)data;
		hfi_write_cmd(&ping);
	}
		break;
	case HFI_CMD_SYS_RESET: {
		struct hfi_cmd_sys_reset_pkt reset;

		reset.size = sizeof(struct hfi_cmd_sys_reset_pkt);
		reset.pkt_type = type;
		reset.user_data = (uint64_t)data;
		hfi_write_cmd(&reset);
	}
		break;
	case HFI_CMD_IPEBPS_CREATE_HANDLE: {
		struct hfi_cmd_create_handle handle;

		handle.size = sizeof(struct hfi_cmd_create_handle);
		handle.pkt_type = type;
		handle.handle_type = (uint32_t)data;
		handle.user_data1 = 0;
		hfi_write_cmd(&handle);
	}
		break;
	case HFI_CMD_IPEBPS_ASYNC_COMMAND_INDIRECT:
		break;
	default:
		pr_err("command not supported :%d\n", type);
		break;
	}
}


int hfi_get_hw_caps(void *query_buf)
{
	int i = 0;
	struct cam_icp_query_cap_cmd *query_cmd = NULL;

	if (!query_buf) {
		pr_err("%s: query buf is NULL\n", __func__);
		return -EINVAL;
	}

	query_cmd = (struct cam_icp_query_cap_cmd *)query_buf;
	query_cmd->fw_version.major = 0x12;
	query_cmd->fw_version.minor = 0x12;
	query_cmd->fw_version.revision = 0x12;

	query_cmd->api_version.major = 0x13;
	query_cmd->api_version.minor = 0x13;
	query_cmd->api_version.revision = 0x13;

	query_cmd->num_ipe = 2;
	query_cmd->num_bps = 1;

	for (i = 0; i < CAM_ICP_DEV_TYPE_MAX; i++) {
		query_cmd->dev_ver[i].dev_type = i;
		query_cmd->dev_ver[i].hw_ver.major = 0x34 + i;
		query_cmd->dev_ver[i].hw_ver.minor = 0x34 + i;
		query_cmd->dev_ver[i].hw_ver.incr = 0x34 + i;
	}
	return 0;
}


void cam_hfi_enable_cpu(void __iomem *icp_base)
{
	cam_io_w((uint32_t)ICP_FLAG_CSR_A5_EN,
			icp_base + HFI_REG_A5_CSR_A5_CONTROL);
	cam_io_w((uint32_t)0x10, icp_base + HFI_REG_A5_CSR_NSEC_RESET);
}

int cam_hfi_init(uint8_t event_driven_mode, struct hfi_mem_info *hfi_mem,
		void __iomem *icp_base, bool debug)
{
	int rc = 0;
	struct hfi_qtbl *qtbl;
	struct hfi_qtbl_hdr *qtbl_hdr;
	struct hfi_q_hdr *cmd_q_hdr, *msg_q_hdr, *dbg_q_hdr;
	uint32_t hw_version, fw_version;
	uint32_t status;

	if (!g_hfi) {
		g_hfi = kzalloc(sizeof(struct hfi_info), GFP_KERNEL);
		if (!g_hfi) {
			rc = -ENOMEM;
			goto alloc_fail;
		}
	}

	pr_debug("g_hfi: %pK\n", (void *)g_hfi);
	if (g_hfi->hfi_state != INVALID) {
		pr_err("hfi_init: invalid state\n");
		return -EINVAL;
	}

	g_hfi->hfi_state = FW_LOAD_DONE;
	memcpy(&g_hfi->map, hfi_mem, sizeof(g_hfi->map));

	if (debug) {
		cam_io_w_mb(
		(uint32_t)(ICP_FLAG_CSR_A5_EN | ICP_FLAG_CSR_WAKE_UP_EN |
		ICP_CSR_EDBGRQ | ICP_CSR_DBGSWENABLE),
		icp_base + HFI_REG_A5_CSR_A5_CONTROL);
		msleep(100);
		cam_io_w_mb((uint32_t)(ICP_FLAG_CSR_A5_EN |
		ICP_FLAG_CSR_WAKE_UP_EN | ICP_CSR_EN_CLKGATE_WFI),
		icp_base + HFI_REG_A5_CSR_A5_CONTROL);
	} else {
		cam_io_w((uint32_t)ICP_FLAG_CSR_A5_EN |
			ICP_FLAG_CSR_WAKE_UP_EN | ICP_CSR_EN_CLKGATE_WFI,
			icp_base + HFI_REG_A5_CSR_A5_CONTROL);
	}

	mutex_init(&g_hfi->cmd_q_lock);
	mutex_init(&g_hfi->msg_q_lock);

	g_hfi->csr_base = icp_base;

	qtbl = (struct hfi_qtbl *)hfi_mem->qtbl.kva;
	qtbl_hdr = &qtbl->q_tbl_hdr;
	qtbl_hdr->qtbl_version = 0xFFFFFFFF;
	qtbl_hdr->qtbl_size = sizeof(struct hfi_qtbl);
	qtbl_hdr->qtbl_qhdr0_offset = sizeof(struct hfi_qtbl_hdr);
	qtbl_hdr->qtbl_qhdr_size = sizeof(struct hfi_q_hdr);
	qtbl_hdr->qtbl_num_q = ICP_HFI_NUMBER_OF_QS;
	qtbl_hdr->qtbl_num_active_q = ICP_HFI_NUMBER_OF_QS;

	/* setup host-to-firmware command queue */
	pr_debug("updating the command queue info\n");
	cmd_q_hdr = &qtbl->q_hdr[Q_CMD];
	cmd_q_hdr->qhdr_status = QHDR_ACTIVE;
	cmd_q_hdr->qhdr_start_addr = hfi_mem->cmd_q.iova;
	cmd_q_hdr->qhdr_q_size =  ICP_CMD_Q_SIZE_IN_BYTES >> BYTE_WORD_SHIFT;
	cmd_q_hdr->qhdr_pkt_size = ICP_HFI_VAR_SIZE_PKT;
	cmd_q_hdr->qhdr_pkt_drop_cnt = RESET;
	cmd_q_hdr->qhdr_read_idx = RESET;
	cmd_q_hdr->qhdr_write_idx = RESET;

	/* setup firmware-to-Host message queue */
	pr_debug("updating the message queue info\n");
	msg_q_hdr = &qtbl->q_hdr[Q_MSG];
	msg_q_hdr->qhdr_status = QHDR_ACTIVE;
	msg_q_hdr->qhdr_start_addr = hfi_mem->msg_q.iova;
	msg_q_hdr->qhdr_q_size = ICP_MSG_Q_SIZE_IN_BYTES >> BYTE_WORD_SHIFT;
	msg_q_hdr->qhdr_pkt_size = ICP_HFI_VAR_SIZE_PKT;
	msg_q_hdr->qhdr_pkt_drop_cnt = RESET;
	msg_q_hdr->qhdr_read_idx = RESET;
	msg_q_hdr->qhdr_write_idx = RESET;

	/* setup firmware-to-Host message queue */
	pr_debug("updating the debug queue info\n");
	dbg_q_hdr = &qtbl->q_hdr[Q_DBG];
	dbg_q_hdr->qhdr_status = QHDR_ACTIVE;
	dbg_q_hdr->qhdr_start_addr = hfi_mem->dbg_q.iova;
	dbg_q_hdr->qhdr_q_size = ICP_DBG_Q_SIZE_IN_BYTES >> BYTE_WORD_SHIFT;
	dbg_q_hdr->qhdr_pkt_size = ICP_HFI_VAR_SIZE_PKT;
	dbg_q_hdr->qhdr_pkt_drop_cnt = RESET;
	dbg_q_hdr->qhdr_read_idx = RESET;
	dbg_q_hdr->qhdr_write_idx = RESET;
	pr_debug("Done updating the debug queue info\n");

	switch (event_driven_mode) {
	case INTR_MODE:
		cmd_q_hdr->qhdr_type = Q_CMD;
		cmd_q_hdr->qhdr_rx_wm = SET;
		cmd_q_hdr->qhdr_tx_wm = SET;
		cmd_q_hdr->qhdr_rx_req = SET;
		cmd_q_hdr->qhdr_tx_req = RESET;
		cmd_q_hdr->qhdr_rx_irq_status = RESET;
		cmd_q_hdr->qhdr_tx_irq_status = RESET;

		msg_q_hdr->qhdr_type = Q_MSG;
		msg_q_hdr->qhdr_rx_wm = SET;
		msg_q_hdr->qhdr_tx_wm = SET;
		msg_q_hdr->qhdr_rx_req = SET;
		msg_q_hdr->qhdr_tx_req = RESET;
		msg_q_hdr->qhdr_rx_irq_status = RESET;
		msg_q_hdr->qhdr_tx_irq_status = RESET;

		dbg_q_hdr->qhdr_type = Q_DBG;
		dbg_q_hdr->qhdr_rx_wm = SET;
		dbg_q_hdr->qhdr_tx_wm = SET;
		dbg_q_hdr->qhdr_rx_req = SET;
		dbg_q_hdr->qhdr_tx_req = RESET;
		dbg_q_hdr->qhdr_rx_irq_status = RESET;
		dbg_q_hdr->qhdr_tx_irq_status = RESET;

		break;

	case POLL_MODE:
		cmd_q_hdr->qhdr_type = Q_CMD | TX_EVENT_POLL_MODE_2 |
			RX_EVENT_POLL_MODE_2;
		msg_q_hdr->qhdr_type = Q_MSG | TX_EVENT_POLL_MODE_2 |
			RX_EVENT_POLL_MODE_2;
		dbg_q_hdr->qhdr_type = Q_DBG | TX_EVENT_POLL_MODE_2 |
			RX_EVENT_POLL_MODE_2;
		break;

	case WM_MODE:
		cmd_q_hdr->qhdr_type = Q_CMD | TX_EVENT_DRIVEN_MODE_2 |
			RX_EVENT_DRIVEN_MODE_2;
		cmd_q_hdr->qhdr_rx_wm = SET;
		cmd_q_hdr->qhdr_tx_wm = SET;
		cmd_q_hdr->qhdr_rx_req = RESET;
		cmd_q_hdr->qhdr_tx_req = SET;
		cmd_q_hdr->qhdr_rx_irq_status = RESET;
		cmd_q_hdr->qhdr_tx_irq_status = RESET;

		msg_q_hdr->qhdr_type = Q_MSG | TX_EVENT_DRIVEN_MODE_2 |
			RX_EVENT_DRIVEN_MODE_2;
		msg_q_hdr->qhdr_rx_wm = SET;
		msg_q_hdr->qhdr_tx_wm = SET;
		msg_q_hdr->qhdr_rx_req = SET;
		msg_q_hdr->qhdr_tx_req = RESET;
		msg_q_hdr->qhdr_rx_irq_status = RESET;
		msg_q_hdr->qhdr_tx_irq_status = RESET;

		dbg_q_hdr->qhdr_type = Q_DBG | TX_EVENT_DRIVEN_MODE_2 |
			RX_EVENT_DRIVEN_MODE_2;
		dbg_q_hdr->qhdr_rx_wm = SET;
		dbg_q_hdr->qhdr_tx_wm = SET;
		dbg_q_hdr->qhdr_rx_req = SET;
		dbg_q_hdr->qhdr_tx_req = RESET;
		dbg_q_hdr->qhdr_rx_irq_status = RESET;
		dbg_q_hdr->qhdr_tx_irq_status = RESET;
		break;

	default:
		pr_err("Invalid event driven mode :%u", event_driven_mode);
		break;
	}

	cam_io_w((uint32_t)hfi_mem->qtbl.iova, icp_base + HFI_REG_QTBL_PTR);
	cam_io_w((uint32_t)0x7400000, icp_base + HFI_REG_SHARED_MEM_PTR);
	cam_io_w((uint32_t)0x6400000, icp_base + HFI_REG_SHARED_MEM_SIZE);
	cam_io_w((uint32_t)hfi_mem->sec_heap.iova,
		icp_base + HFI_REG_UNCACHED_HEAP_PTR);
	cam_io_w((uint32_t)hfi_mem->sec_heap.len,
		icp_base + HFI_REG_UNCACHED_HEAP_SIZE);
	cam_io_w((uint32_t)ICP_INIT_REQUEST_SET,
		icp_base + HFI_REG_HOST_ICP_INIT_REQUEST);

	hw_version = cam_io_r(icp_base + HFI_REG_A5_HW_VERSION);
	pr_debug("hw version : %u[%x]\n", hw_version, hw_version);

	do {
		msleep(500);
		status = cam_io_r(icp_base + HFI_REG_ICP_HOST_INIT_RESPONSE);
	} while (status != ICP_INIT_RESP_SUCCESS);

	if (status == ICP_INIT_RESP_SUCCESS) {
		g_hfi->hfi_state = FW_RESP_DONE;
		rc = 0;
	} else {
		rc = -ENODEV;
		pr_err("FW initialization failed");
		goto regions_fail;
	}

	fw_version = cam_io_r(icp_base + HFI_REG_FW_VERSION);
	g_hfi->hfi_state = FW_START_SENT;

	pr_debug("fw version : %u[%x]\n", fw_version, fw_version);
	pr_debug("hfi init is successful\n");
	cam_io_w((uint32_t)INTR_ENABLE, icp_base + HFI_REG_A5_CSR_A2HOSTINTEN);

	return rc;
regions_fail:
	kzfree(g_hfi);
alloc_fail:
	return rc;
}


void cam_hfi_deinit(void)
{
	kfree(g_hfi);
	g_hfi = NULL;
}

void icp_enable_fw_debug(void)
{
	hfi_send_system_cmd(HFI_CMD_SYS_SET_PROPERTY,
		(uint64_t)HFI_PROP_SYS_DEBUG_CFG, 0);
}

int icp_ping_fw(void)
{
	hfi_send_system_cmd(HFI_CMD_SYS_PING,
		(uint64_t)0x12123434, 0);

	return 0;
}
