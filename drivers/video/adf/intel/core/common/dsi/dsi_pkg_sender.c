/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/

#include <linux/delay.h>
#include <linux/freezer.h>

#include "core/common/dsi/dsi_pkg_sender.h"

#define DSI_DBI_FIFO_TIMEOUT		1000
#define DSI_MAX_RETURN_PACKET_SIZE	512
#define DSI_READ_MAX_COUNT		10000

const char *dsi_error_msgs[] = {
	"[ 0:RX SOT Error]",
	"[ 1:RX SOT Sync Error]",
	"[ 2:RX EOT Sync Error]",
	"[ 3:RX Escape Mode Entry Error]",
	"[ 4:RX LP TX Sync Error",
	"[ 5:RX HS Receive Timeout Error]",
	"[ 6:RX False Control Error]",
	"[ 7:RX ECC Single Bit Error]",
	"[ 8:RX ECC Multibit Error]",
	"[ 9:RX Checksum Error]",
	"[10:RX DSI Data Type Not Recognised]",
	"[11:RX DSI VC ID Invalid]",
	"[12:TX False Control Error]",
	"[13:TX ECC Single Bit Error]",
	"[14:TX ECC Multibit Error]",
	"[15:TX Checksum Error]",
	"[16:TX DSI Data Type Not Recognised]",
	"[17:TX DSI VC ID invalid]",
	"[18:High Contention]",
	"[19:Low contention]",
	"[20:DPI FIFO Under run]",
	"[21:HS TX Timeout]",
	"[22:LP RX Timeout]",
	"[23:Turn Around ACK Timeout]",
	"[24:ACK With No Error]",
	"[25:RX Invalid TX Length]",
	"[26:RX Prot Violation]",
	"[27:HS Generic Write FIFO Full]",
	"[28:LP Generic Write FIFO Full]",
	"[29:Generic Read Data Avail]",
	"[30:Special Packet Sent]",
	"[31:Tearing Effect]",
};

static inline int wait_for_gen_fifo_empty(struct dsi_pkg_sender *sender,
						u32 mask)
{
	u32 gen_fifo_stat_reg = sender->mipi_gen_fifo_stat_reg;
	int retry = 10000;

	if (sender->work_for_slave_panel)
		gen_fifo_stat_reg += MIPI_C_REG_OFFSET;

	while (retry--) {
		if ((mask & REG_READ(gen_fifo_stat_reg)) == mask)
			return 0;
		udelay(3);
	}

	pr_err("fifo is NOT empty 0x%08x\n", REG_READ(gen_fifo_stat_reg));
	if (!IS_ANN())
		debug_dbi_hang(sender);

	sender->status = DSI_CONTROL_ABNORMAL;
	return -EIO;
}

static int wait_for_all_fifos_empty(struct dsi_pkg_sender *sender)
{
	return wait_for_gen_fifo_empty(sender,
		(HS_DATA_FIFO_EMPTY | LP_DATA_FIFO_EMPTY | HS_CTRL_FIFO_EMPTY |
			LP_CTRL_FIFO_EMPTY | DBI_FIFO_EMPTY | DPI_FIFO_EMPTY));
}

static int wait_for_lp_fifos_empty(struct dsi_pkg_sender *sender)
{
	return wait_for_gen_fifo_empty(sender,
		(LP_DATA_FIFO_EMPTY | LP_CTRL_FIFO_EMPTY));
}

static int wait_for_hs_fifos_empty(struct dsi_pkg_sender *sender)
{
	return wait_for_gen_fifo_empty(sender,
		(HS_DATA_FIFO_EMPTY | HS_CTRL_FIFO_EMPTY));
}

static int wait_for_dbi_fifo_empty(struct dsi_pkg_sender *sender)
{
	return wait_for_gen_fifo_empty(sender, (DBI_FIFO_EMPTY));
}

static int wait_for_dpi_fifo_empty(struct dsi_pkg_sender *sender)
{
	return wait_for_gen_fifo_empty(sender, (DPI_FIFO_EMPTY));
}

static int dsi_error_handler(struct dsi_pkg_sender *sender)
{
	u32 intr_stat_reg = sender->mipi_intr_stat_reg;

	int i;
	u32 mask;
	int err = 0;
	int count = 0;
	u32 intr_stat;

	intr_stat = REG_READ(intr_stat_reg);
	if (!intr_stat)
		return 0;

	for (i = 0; i < 32; i++) {
		mask = (0x00000001UL) << i;
		if (!(intr_stat & mask))
			continue;

		switch (mask) {
		case RX_SOT_ERROR:
		case RX_SOT_SYNC_ERROR:
		case RX_EOT_SYNC_ERROR:
		case RX_ESCAPE_MODE_ENTRY_ERROR:
		case RX_LP_TX_SYNC_ERROR:
		case RX_HS_RECEIVE_TIMEOUT_ERROR:
		case RX_FALSE_CONTROL_ERROR:
		case RX_ECC_SINGLE_BIT_ERROR:
			/*No Action required.*/
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case RX_ECC_MULTI_BIT_ERROR:
			/*No Action required.*/
			REG_WRITE(intr_stat_reg, mask);
			break;
		case RX_CHECKSUM_ERROR:
		case RX_DSI_DATA_TYPE_NOT_RECOGNIZED:
		case RX_DSI_VC_ID_INVALID:
		case TX_FALSE_CONTROL_ERROR:
		case TX_ECC_SINGLE_BIT_ERROR:
			/*No Action required.*/
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case TX_ECC_MULTI_BIT_ERROR:
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			break;
		case TX_CHECKSUM_ERROR:
			/*No Action required.*/
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case TX_DSI_DATA_TYPE_NOT_RECOGNIZED:
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case TX_DSI_VC_ID_INVALID:
			/*No Action required.*/
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case HIGH_CONTENTION:
			REG_WRITE(MIPIA_EOT_DISABLE_REG,
				REG_READ(MIPIA_EOT_DISABLE_REG) |
				LOW_CONTENTION_REC_DISABLE |
				HIGH_CONTENTION_REC_DISABLE);
			while ((REG_READ(intr_stat_reg) & HIGH_CONTENTION)) {
				count++;
				/*
				* Per silicon feedback,
				* if this bit cannot be
				* cleared by 3 times,
				* it should be a real
				* High Contention error.
				*/
				if (count == 4) {
					pr_info("dsi status %s\n",
						dsi_error_msgs[i]);
					break;
				}
				REG_WRITE(intr_stat_reg, mask);
			}
			break;
		case LOW_CONTENTION:
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			break;
		case DPI_FIFO_UNDER_RUN:
			/*No Action required.*/
			pr_debug("dsi status %s\n", dsi_error_msgs[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case HS_TX_TIMEOUT:
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			REG_WRITE(intr_stat_reg, mask);
			err = wait_for_all_fifos_empty(sender);
			break;
		case LP_RX_TIMEOUT:
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			REG_WRITE(intr_stat_reg, mask);
			err = wait_for_all_fifos_empty(sender);
			break;
		case TURN_AROUND_ACK_TIMEOUT:
			/*No Action required.*/
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case ACK_WITH_NO_ERROR:
			/*No Action required.*/
			pr_debug("dsi status %s\n", dsi_error_msgs[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case RX_INVALID_TX_LENGTH:
		case RX_PROT_VIOLATION:
			/*No Action required.*/
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			REG_WRITE(intr_stat_reg, mask);
			break;
		case HS_GENERIC_WR_FIFO_FULL:
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			REG_WRITE(intr_stat_reg, mask);
			err = wait_for_hs_fifos_empty(sender);
			break;
		case LP_GENERIC_WR_FIFO_FULL:
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			REG_WRITE(intr_stat_reg, mask);
			err = wait_for_lp_fifos_empty(sender);
			break;
		case GEN_READ_DATA_AVAIL:
			/*No Action required.*/
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			break;
		case SPL_PKT_SENT_INTERRUPT:
			break;
		case TEARING_EFFECT:
			/*No Action required.*/
			pr_info("dsi status %s\n", dsi_error_msgs[i]);
			break;
		}
	}

	return err;
}

static inline int dbi_cmd_sent(struct dsi_pkg_sender *sender)
{
	u32 retry = 0xffff;
	u32 dbi_cmd_addr_reg = sender->mipi_cmd_addr_reg;
	int ret = 0;

	/*query the command execution status*/
	while (retry--) {
		if (!(REG_READ(dbi_cmd_addr_reg) & CMD_VALID))
			break;
	}

	if (!retry) {
		pr_err("Timeout waiting for DBI Command status\n");
		ret = -EAGAIN;
	}

	return ret;
}

/**
 * NOTE: this interface is abandoned expect for write_mem_start DCS
 * other DCS are sent via generic pkg interfaces
 */
static int send_dcs_pkg(struct dsi_pkg_sender *sender,
			struct dsi_pkg *pkg)
{
	struct dsi_dcs_pkg *dcs_pkg = &pkg->pkg.dcs_pkg;
	u32 dbi_cmd_len_reg = sender->mipi_cmd_len_reg;
	u32 dbi_cmd_addr_reg = sender->mipi_cmd_addr_reg;
	u32 cb_phy = sender->dbi_cb_phy;
	u32 index = 0;
	u8 *cb = (u8 *)sender->dbi_cb_addr;
	int i;
	int ret;

	if (!sender->dbi_pkg_support) {
		pr_err("Trying to send DCS on a non DBI output, abort!\n");
		return -ENOTSUPP;
	}

	pr_debug("Sending DCS pkg 0x%x...\n", dcs_pkg->cmd);

	/*wait for DBI fifo empty*/
	wait_for_dbi_fifo_empty(sender);

	*(cb + (index++)) = dcs_pkg->cmd;
	if (dcs_pkg->param_num) {
		for (i = 0; i < dcs_pkg->param_num; i++)
			*(cb + (index++)) = *(dcs_pkg->param + i);
	}

	REG_WRITE(dbi_cmd_len_reg, (1 + dcs_pkg->param_num));
	REG_WRITE(dbi_cmd_addr_reg,
		(cb_phy << CMD_MEM_ADDR_OFFSET)
		| CMD_VALID
		| ((dcs_pkg->data_src == CMD_DATA_SRC_PIPE) ?
					 CMD_DATA_MODE : 0));

	ret = dbi_cmd_sent(sender);
	if (ret) {
		pr_err("command 0x%x not complete\n", dcs_pkg->cmd);
		return -EAGAIN;
	}

	pr_debug("sent DCS pkg 0x%x...\n", dcs_pkg->cmd);

	return 0;
}

static int __send_short_pkg(struct dsi_pkg_sender *sender,
				struct dsi_pkg *pkg)
{
	u32 hs_gen_ctrl_reg = sender->mipi_hs_gen_ctrl_reg;
	u32 lp_gen_ctrl_reg = sender->mipi_lp_gen_ctrl_reg;
	u32 gen_ctrl_val = 0;
	struct dsi_gen_short_pkg *short_pkg = &pkg->pkg.short_pkg;

	if (sender->work_for_slave_panel) {
		hs_gen_ctrl_reg += MIPI_C_REG_OFFSET;
		lp_gen_ctrl_reg += MIPI_C_REG_OFFSET;
	}
	gen_ctrl_val |= short_pkg->cmd << HS_WORD_COUNT_SHIFT;
	gen_ctrl_val |= 0 << HS_VIRTUAL_CHANNEL_SHIFT;
	gen_ctrl_val |= pkg->pkg_type;
	gen_ctrl_val |= short_pkg->param << HS_MCS_PARAMETER_SHIFT;

	if (pkg->transmission_type == DSI_HS_TRANSMISSION) {
		/*wait for hs fifo empty*/
		wait_for_dbi_fifo_empty(sender);
		wait_for_hs_fifos_empty(sender);

		/*send pkg*/
		REG_WRITE(hs_gen_ctrl_reg, gen_ctrl_val);
	} else if (pkg->transmission_type == DSI_LP_TRANSMISSION) {
		wait_for_dbi_fifo_empty(sender);
		wait_for_lp_fifos_empty(sender);

		/*send pkg*/
		REG_WRITE(lp_gen_ctrl_reg, gen_ctrl_val);
	} else {
		pr_err("Unknown transmission type %d\n",
				pkg->transmission_type);
		return -EINVAL;
	}

	return 0;
}

static int __send_long_pkg(struct dsi_pkg_sender *sender,
				struct dsi_pkg *pkg)
{
	u32 hs_gen_ctrl_reg = sender->mipi_hs_gen_ctrl_reg;
	u32 hs_gen_data_reg = sender->mipi_hs_gen_data_reg;
	u32 lp_gen_ctrl_reg = sender->mipi_lp_gen_ctrl_reg;
	u32 lp_gen_data_reg = sender->mipi_lp_gen_data_reg;
	u32 gen_ctrl_val = 0;
	u8 *dp = NULL;
	u32 reg_val = 0;
	int i;
	int dword_count = 0, remain_byte_count = 0;
	struct dsi_gen_long_pkg *long_pkg = &pkg->pkg.long_pkg;

	dp = long_pkg->data;
	if (sender->work_for_slave_panel) {
		hs_gen_ctrl_reg += MIPI_C_REG_OFFSET;
		hs_gen_data_reg += MIPI_C_REG_OFFSET;
		lp_gen_ctrl_reg += MIPI_C_REG_OFFSET;
		lp_gen_data_reg += MIPI_C_REG_OFFSET;
	}

	/**
	 * Set up word count for long pkg
	 * FIXME: double check word count field.
	 * currently, using the byte counts of the payload as the word count.
	 * ------------------------------------------------------------
	 * | DI |   WC   | ECC|         PAYLOAD              |CHECKSUM|
	 * ------------------------------------------------------------
	 */
	gen_ctrl_val |= (long_pkg->len) << HS_WORD_COUNT_SHIFT;
	gen_ctrl_val |= 0 << HS_VIRTUAL_CHANNEL_SHIFT;
	gen_ctrl_val |= pkg->pkg_type;

	if (pkg->transmission_type == DSI_HS_TRANSMISSION) {
		/*wait for hs ctrl and data fifos to be empty*/
		wait_for_dbi_fifo_empty(sender);
		wait_for_hs_fifos_empty(sender);

		dword_count = long_pkg->len / 4;
		remain_byte_count = long_pkg->len % 4;
		for (i = 0; i < dword_count * 4; i = i + 4) {
			reg_val = 0;
			reg_val = *(dp + i);
			reg_val |= *(dp + i + 1) << 8;
			reg_val |= *(dp + i + 2) << 16;
			reg_val |= *(dp + i + 3) << 24;
			pr_debug("HS Sending data 0x%08x\n", reg_val);
			REG_WRITE(hs_gen_data_reg, reg_val);
		}

		if (remain_byte_count) {
			reg_val = 0;
			for (i = 0; i < remain_byte_count; i++)
				reg_val |=
					*(dp + dword_count * 4 + i) << (8 * i);
			pr_debug("HS Sending data 0x%08x\n", reg_val);
			REG_WRITE(hs_gen_data_reg, reg_val);
		}

		REG_WRITE(hs_gen_ctrl_reg, gen_ctrl_val);
	} else if (pkg->transmission_type == DSI_LP_TRANSMISSION) {
		wait_for_dbi_fifo_empty(sender);
		wait_for_lp_fifos_empty(sender);

		dword_count = long_pkg->len / 4;
		remain_byte_count = long_pkg->len % 4;
		for (i = 0; i < dword_count * 4; i = i + 4) {
			reg_val = 0;
			reg_val = *(dp + i);
			reg_val |= *(dp + i + 1) << 8;
			reg_val |= *(dp + i + 2) << 16;
			reg_val |= *(dp + i + 3) << 24;
			pr_debug("LP Sending data 0x%08x\n", reg_val);
			REG_WRITE(lp_gen_data_reg, reg_val);
		}

		if (remain_byte_count) {
			reg_val = 0;
			for (i = 0; i < remain_byte_count; i++) {
				reg_val |=
					*(dp + dword_count * 4 + i) << (8 * i);
			}
			pr_debug("LP Sending data 0x%08x\n", reg_val);
			REG_WRITE(lp_gen_data_reg, reg_val);
		}

		REG_WRITE(lp_gen_ctrl_reg, gen_ctrl_val);
	} else {
		pr_err("Unknown transmission type %d\n",
				pkg->transmission_type);
		return -EINVAL;
	}

	return 0;

}

static int send_mcs_short_pkg(struct dsi_pkg_sender *sender,
				struct dsi_pkg *pkg)
{
	pr_debug("Sending MCS short pkg...\n");

	return __send_short_pkg(sender, pkg);
}

static int send_mcs_long_pkg(struct dsi_pkg_sender *sender,
				struct dsi_pkg *pkg)
{
	pr_debug("Sending MCS long pkg...\n");

	return __send_long_pkg(sender, pkg);
}

static int send_gen_short_pkg(struct dsi_pkg_sender *sender,
				struct dsi_pkg *pkg)
{
	pr_debug("Sending GEN short pkg...\n");

	return __send_short_pkg(sender, pkg);
}

static int send_gen_long_pkg(struct dsi_pkg_sender *sender,
				struct dsi_pkg *pkg)
{
	pr_debug("Sending GEN long pkg...\n");

	return __send_long_pkg(sender, pkg);
}

static int send_dpi_spk_pkg(struct dsi_pkg_sender *sender,
				struct dsi_pkg *pkg)
{
	u32 dpi_control_reg = sender->mipi_dpi_control_reg;
	u32 intr_stat_reg = sender->mipi_intr_stat_reg;
	u32 dpi_control_val = 0;
	u32 dpi_control_current_setting = 0;
	struct dsi_dpi_spk_pkg *spk_pkg = &pkg->pkg.spk_pkg;
	int retry = 10000;

	dpi_control_val = spk_pkg->cmd;

	if (pkg->transmission_type == DSI_LP_TRANSMISSION)
		dpi_control_val |= HS_LP;

	/*Wait for DPI fifo empty*/
	wait_for_dpi_fifo_empty(sender);

	/*clean spk packet sent interrupt*/
	REG_WRITE(intr_stat_reg, SPL_PKT_SENT_INTERRUPT);
	dpi_control_current_setting =
		REG_READ(dpi_control_reg);

	/*send out spk packet*/
	if (dpi_control_current_setting != dpi_control_val) {
		REG_WRITE(dpi_control_reg, dpi_control_val);

		/*wait for spk packet sent interrupt*/
		while (--retry && !(REG_READ(intr_stat_reg) &
				    SPL_PKT_SENT_INTERRUPT))
			udelay(3);

		if (!retry) {
			pr_err("Fail to send SPK Packet 0x%x\n",
				 spk_pkg->cmd);
			return -EINVAL;
		}
	} else
		/*For SHUT_DOWN and TURN_ON, it is better called by
		symmetrical. so skip duplicate called*/
		pr_warn("skip duplicate setting of DPI control\n");
	return 0;
}

static int send_pkg_prepare(struct dsi_pkg_sender *sender,
				struct dsi_pkg *pkg)
{
	u8 cmd;
	u8 *data;

	pr_debug("Prepare to Send type 0x%x pkg\n", pkg->pkg_type);

	switch (pkg->pkg_type) {
	case DSI_PKG_DCS:
		cmd = pkg->pkg.dcs_pkg.cmd;
		break;
	case DSI_PKG_MCS_SHORT_WRITE_0:
	case DSI_PKG_MCS_SHORT_WRITE_1:
		cmd = pkg->pkg.short_pkg.cmd;
		break;
	case DSI_PKG_MCS_LONG_WRITE:
		data = (u8 *)pkg->pkg.long_pkg.data;
		cmd = *data;
		break;
	default:
		return 0;
	}

	/*this prevents other package sending while doing msleep*/
	sender->status = DSI_PKG_SENDER_BUSY;

	return 0;
}

static int send_pkg_done(struct dsi_pkg_sender *sender,
		struct dsi_pkg *pkg)
{
	u8 cmd;
	u8 *data = NULL;

	pr_debug("Sent type 0x%x pkg\n", pkg->pkg_type);

	switch (pkg->pkg_type) {
	case DSI_PKG_DCS:
		cmd = pkg->pkg.dcs_pkg.cmd;
		break;
	case DSI_PKG_MCS_SHORT_WRITE_0:
	case DSI_PKG_MCS_SHORT_WRITE_1:
		cmd = pkg->pkg.short_pkg.cmd;
		break;
	case DSI_PKG_MCS_LONG_WRITE:
	case DSI_PKG_GEN_LONG_WRITE:
		data = (u8 *)pkg->pkg.long_pkg.data;
		cmd = *data;
		break;
	default:
		return 0;
	}

	/*update panel status*/
	if (unlikely(cmd == enter_sleep_mode))
		sender->panel_mode |= DSI_PANEL_MODE_SLEEP;
	else if (unlikely(cmd == exit_sleep_mode))
		sender->panel_mode &= ~DSI_PANEL_MODE_SLEEP;

	if (sender->status != DSI_CONTROL_ABNORMAL)
		sender->status = DSI_PKG_SENDER_FREE;

	/*after sending pkg done, free the data buffer for mcs long pkg*/
	if (pkg->pkg_type == DSI_PKG_MCS_LONG_WRITE ||
		pkg->pkg_type == DSI_PKG_GEN_LONG_WRITE) {
		if (data != NULL)
			kfree(data);
	}

	return 0;
}

static int do_send_pkg(struct dsi_pkg_sender *sender,
			struct dsi_pkg *pkg)
{
	int ret = 0;

	pr_debug("Sending type 0x%x pkg\n", pkg->pkg_type);

	if (sender->status == DSI_PKG_SENDER_BUSY) {
		pr_err("sender is busy\n");
		return -EAGAIN;
	}

	ret = send_pkg_prepare(sender, pkg);
	if (ret) {
		pr_err("send_pkg_prepare error\n");
		return ret;
	}

	switch (pkg->pkg_type) {
	case DSI_PKG_DCS:
		ret = send_dcs_pkg(sender, pkg);
		break;
	case DSI_PKG_GEN_SHORT_WRITE_0:
	case DSI_PKG_GEN_SHORT_WRITE_1:
	case DSI_PKG_GEN_SHORT_WRITE_2:
	case DSI_PKG_GEN_READ_0:
	case DSI_PKG_GEN_READ_1:
	case DSI_PKG_GEN_READ_2:
		ret = send_gen_short_pkg(sender, pkg);
		break;
	case DSI_PKG_GEN_LONG_WRITE:
		ret = send_gen_long_pkg(sender, pkg);
		break;
	case DSI_PKG_MCS_SHORT_WRITE_0:
	case DSI_PKG_MCS_SHORT_WRITE_1:
	case DSI_PKG_MCS_READ:
		ret = send_mcs_short_pkg(sender, pkg);
		break;
	case DSI_PKG_MCS_LONG_WRITE:
		ret = send_mcs_long_pkg(sender, pkg);
		break;
	case DSI_DPI_SPK:
		ret = send_dpi_spk_pkg(sender, pkg);
		break;
	default:
		pr_err("Invalid pkg type 0x%x\n", pkg->pkg_type);
		ret = -EINVAL;
	}

	send_pkg_done(sender, pkg);

	return ret;
}

static int send_pkg(struct dsi_pkg_sender *sender,
			struct dsi_pkg *pkg)
{
	int err = 0;

	/*handle DSI error*/
	err = dsi_error_handler(sender);
	if (err) {
		pr_err("Error handling failed\n");
		err = -EAGAIN;
		goto send_pkg_err;
	}

	/*send pkg*/
	err = do_send_pkg(sender, pkg);
	if (err) {
		pr_err("sent pkg failed\n");
		dsi_error_handler(sender);
		err = -EAGAIN;
		goto send_pkg_err;
	}

	/*FIXME: should I query complete and fifo empty here?*/
send_pkg_err:
	return err;
}

static struct dsi_pkg *
pkg_sender_get_pkg_locked(struct dsi_pkg_sender *sender)
{
	struct dsi_pkg *pkg;

	if (list_empty(&sender->free_list)) {
		pr_err("No free pkg left\n");
		return NULL;
	}

	pkg = list_first_entry(&sender->free_list, struct dsi_pkg, entry);

	/*detach from free list*/
	list_del_init(&pkg->entry);

	return pkg;
}

static void pkg_sender_put_pkg_locked(struct dsi_pkg_sender *sender,
		struct dsi_pkg *pkg)
{
	memset(pkg, 0, sizeof(struct dsi_pkg));

	INIT_LIST_HEAD(&pkg->entry);

	list_add_tail(&pkg->entry, &sender->free_list);
}

static int dbi_cb_init(struct dsi_pkg_sender *sender, u32 gtt_phys_addr,
	int pipe)
{
	uint32_t phy;
	void *virt_addr = NULL;

	switch (pipe) {
	case 0:
		phy = gtt_phys_addr - 0x1000;
		break;
	case 2:
		phy = gtt_phys_addr - 0x800;
		break;
	default:
		pr_err("Unsupported channel\n");
		return -EINVAL;
	}

	/*mapping*/
	virt_addr = ioremap_nocache(phy, 0x800);
	if (!virt_addr) {
		pr_err("Map DBI command buffer error\n");
		return -ENOMEM;
	}

	if (IS_ANN())
		memset(virt_addr, 0x0, 0x800);

	sender->dbi_cb_phy = phy;
	sender->dbi_cb_addr = virt_addr;

	pr_debug("DBI command buffer initailized. phy %x, addr %p\n",
			phy, virt_addr);

	return 0;
}

static void dbi_cb_destroy(struct dsi_pkg_sender *sender)
{
	pr_debug("\n");

	if (sender && sender->dbi_cb_addr)
		iounmap(sender->dbi_cb_addr);
}

static inline void pkg_sender_queue_pkg(struct dsi_pkg_sender *sender,
					struct dsi_pkg *pkg,
					int delay)
{
	mutex_lock(&sender->lock);

	if (!delay) {
		send_pkg(sender, pkg);

		pkg_sender_put_pkg_locked(sender, pkg);
	} else {
		/*queue it*/
		list_add_tail(&pkg->entry, &sender->pkg_list);
	}

	mutex_unlock(&sender->lock);
}

static inline int process_pkg_list(struct dsi_pkg_sender *sender)
{
	struct dsi_pkg *pkg;
	int ret = 0;

	mutex_lock(&sender->lock);

	while (!list_empty(&sender->pkg_list)) {
		pkg = list_first_entry(&sender->pkg_list,
				struct dsi_pkg, entry);
		ret = send_pkg(sender, pkg);

		if (ret) {
			pr_info("Returning eror from process_pkg_lisgt");
			goto errorunlock;
		}

		list_del_init(&pkg->entry);

		pkg_sender_put_pkg_locked(sender, pkg);
	}

	mutex_unlock(&sender->lock);
	return 0;

errorunlock:
	mutex_unlock(&sender->lock);
	return ret;
}

static int dsi_send_mcs_long(struct dsi_pkg_sender *sender,
				   u8 *data,
				   u32 len,
				   u8 transmission,
				   int delay)
{
	struct dsi_pkg *pkg;
	u8 *pdata = NULL;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		pr_err("No memory\n");
		return -ENOMEM;
	}

	/* alloc a data buffer to save the long pkg data,
	 * free the buffer when send_pkg_done.
	 * */
	pdata = kmalloc(sizeof(u8) * len, GFP_KERNEL);
	if (!pdata) {
		pr_err("No memory for long_pkg data\n");
		return -ENOMEM;
	}

	memcpy(pdata, data, len * sizeof(u8));

	pkg->pkg_type = DSI_PKG_MCS_LONG_WRITE;
	pkg->transmission_type = transmission;
	pkg->pkg.long_pkg.data = pdata;
	pkg->pkg.long_pkg.len = len;

	INIT_LIST_HEAD(&pkg->entry);

	pkg_sender_queue_pkg(sender, pkg, delay);

	return 0;
}

static int dsi_send_mcs_short(struct dsi_pkg_sender *sender,
					u8 cmd, u8 param, u8 param_num,
					u8 transmission,
					int delay)
{
	struct dsi_pkg *pkg;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		pr_err("No memory\n");
		return -ENOMEM;
	}

	if (param_num) {
		pkg->pkg_type = DSI_PKG_MCS_SHORT_WRITE_1;
		pkg->pkg.short_pkg.param = param;
	} else {
		pkg->pkg_type = DSI_PKG_MCS_SHORT_WRITE_0;
		pkg->pkg.short_pkg.param = 0;
	}
	pkg->transmission_type = transmission;
	pkg->pkg.short_pkg.cmd = cmd;

	INIT_LIST_HEAD(&pkg->entry);

	pkg_sender_queue_pkg(sender, pkg, delay);

	return 0;
}

static int dsi_send_gen_short(struct dsi_pkg_sender *sender,
					u8 param0, u8 param1, u8 param_num,
					u8 transmission,
					int delay)
{
	struct dsi_pkg *pkg;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		pr_err("No memory\n");
		return -ENOMEM;
	}

	switch (param_num) {
	case 0:
		pkg->pkg_type = DSI_PKG_GEN_SHORT_WRITE_0;
		pkg->pkg.short_pkg.cmd = 0;
		pkg->pkg.short_pkg.param = 0;
		break;
	case 1:
		pkg->pkg_type = DSI_PKG_GEN_SHORT_WRITE_1;
		pkg->pkg.short_pkg.cmd = param0;
		pkg->pkg.short_pkg.param = 0;
		break;
	case 2:
		pkg->pkg_type = DSI_PKG_GEN_SHORT_WRITE_2;
		pkg->pkg.short_pkg.cmd = param0;
		pkg->pkg.short_pkg.param = param1;
		break;
	}

	pkg->transmission_type = transmission;

	INIT_LIST_HEAD(&pkg->entry);

	pkg_sender_queue_pkg(sender, pkg, delay);

	return 0;
}

static int dsi_send_gen_long(struct dsi_pkg_sender *sender,
				   u8 *data,
				   u32 len,
				   u8 transmission,
				   int delay)
{
	struct dsi_pkg *pkg;
	u8 *pdata = NULL;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		pr_err("No memory\n");
		return -ENOMEM;
	}

	/* alloc a data buffer to save the long pkg data,
	 * free the buffer when send_pkg_done.
	 * */
	pdata = kmalloc(sizeof(u8)*len, GFP_KERNEL);
	if (!pdata) {
		pr_err("No memory for long_pkg data\n");
		return -ENOMEM;
	}

	memcpy(pdata, data, len*sizeof(u8));

	pkg->pkg_type = DSI_PKG_GEN_LONG_WRITE;
	pkg->transmission_type = transmission;
	pkg->pkg.long_pkg.data = pdata;
	pkg->pkg.long_pkg.len = len;

	INIT_LIST_HEAD(&pkg->entry);

	pkg_sender_queue_pkg(sender, pkg, delay);

	return 0;
}

static int __read_panel_data(struct dsi_pkg_sender *sender,
				struct dsi_pkg *pkg,
				u8 *data,
				u32 len)
{
	int i;
	u32 gen_data_reg;
	u32 gen_data_value;
	int retry = DSI_READ_MAX_COUNT;
	u8 transmission = pkg->transmission_type;
	int dword_count = 0, remain_byte_count = 0;

	/*Check the len. Max value is 0x40
	based on the generic read FIFO size*/
	if (len * sizeof(*data) > 0x40) {
		len = 0x40 / sizeof(*data);
		pr_err("Bigger than Max.Set the len to Max 0x40 bytes\n");
	}

	/**
	 * do reading.
	 * 0) set the max return pack size
	 * 1) send out generic read request
	 * 2) polling read data avail interrupt
	 * 3) read data
	 */
	mutex_lock(&sender->lock);

	/*Set the Max return pack size*/
	wait_for_all_fifos_empty(sender);
	REG_WRITE(MIPIA_MAX_RETURN_PKT_SIZE_REG, (len*sizeof(*data)) &
		  MAX_RETURN_PKT_SIZE_MASK);
	wait_for_all_fifos_empty(sender);

	REG_WRITE(sender->mipi_intr_stat_reg, GEN_READ_DATA_AVAIL);

	if ((REG_READ(sender->mipi_intr_stat_reg) & GEN_READ_DATA_AVAIL))
		pr_err("Can NOT clean read data valid interrupt\n");

	/*send out read request*/
	send_pkg(sender, pkg);

	pkg_sender_put_pkg_locked(sender, pkg);

	/*polling read data avail interrupt*/
	while (--retry && !(REG_READ(sender->mipi_intr_stat_reg) &
			    GEN_READ_DATA_AVAIL))
		udelay(3);

	if (!retry) {
		mutex_unlock(&sender->lock);
		return -ETIMEDOUT;
	}

	REG_WRITE(sender->mipi_intr_stat_reg, GEN_READ_DATA_AVAIL);

	/*read data*/
	if (transmission == DSI_HS_TRANSMISSION)
		gen_data_reg = sender->mipi_hs_gen_data_reg;
	else if (transmission == DSI_LP_TRANSMISSION)
		gen_data_reg = sender->mipi_lp_gen_data_reg;
	else {
		pr_err("Unknown transmission");
		mutex_unlock(&sender->lock);
		return -EINVAL;
	}

	dword_count = len / 4;
	remain_byte_count = len % 4;
	for (i = 0; i < dword_count * 4; i = i + 4) {
		gen_data_value = REG_READ(gen_data_reg);
		*(data + i)     = gen_data_value & 0x000000FF;
		*(data + i + 1) = (gen_data_value >> 8)  & 0x000000FF;
		*(data + i + 2) = (gen_data_value >> 16) & 0x000000FF;
		*(data + i + 3) = (gen_data_value >> 24) & 0x000000FF;
	}
	if (remain_byte_count) {
		gen_data_value = REG_READ(gen_data_reg);
		for (i = 0; i < remain_byte_count; i++) {
			*(data + dword_count * 4 + i)  =
				(gen_data_value >> (8 * i)) & 0x000000FF;
		}
	}

	mutex_unlock(&sender->lock);

	return len;
}

static int dsi_read_gen(struct dsi_pkg_sender *sender,
				u8 param0,
				u8 param1,
				u8 param_num,
				u8 *data,
				u32 len,
				u8 transmission)
{
	struct dsi_pkg *pkg;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		pr_err("No memory\n");
		return -ENOMEM;
	}

	switch (param_num) {
	case 0:
		pkg->pkg_type = DSI_PKG_GEN_READ_0;
		pkg->pkg.short_pkg.cmd = 0;
		pkg->pkg.short_pkg.param = 0;
		break;
	case 1:
		pkg->pkg_type = DSI_PKG_GEN_READ_1;
		pkg->pkg.short_pkg.cmd = param0;
		pkg->pkg.short_pkg.param = 0;
		break;
	case 2:
		pkg->pkg_type = DSI_PKG_GEN_READ_2;
		pkg->pkg.short_pkg.cmd = param0;
		pkg->pkg.short_pkg.param = param1;
		break;
	}

	pkg->transmission_type = transmission;

	INIT_LIST_HEAD(&pkg->entry);

	return __read_panel_data(sender, pkg, data, len);
}

static int dsi_read_mcs(struct dsi_pkg_sender *sender,
				u8 cmd,
				u8 *data,
				u32 len,
				u8 transmission)
{
	struct dsi_pkg *pkg;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		pr_err("No memory\n");
		return -ENOMEM;
	}

	pkg->pkg_type = DSI_PKG_MCS_READ;
	pkg->pkg.short_pkg.cmd = cmd;
	pkg->pkg.short_pkg.param = 0;

	pkg->transmission_type = transmission;

	INIT_LIST_HEAD(&pkg->entry);

	return __read_panel_data(sender, pkg, data, len);
}

static int dsi_send_dpi_spk_pkg(struct dsi_pkg_sender *sender,
				u32 spk_pkg,
				u8 transmission)
{
	struct dsi_pkg *pkg;

	mutex_lock(&sender->lock);

	pkg = pkg_sender_get_pkg_locked(sender);

	mutex_unlock(&sender->lock);

	if (!pkg) {
		pr_err("No memory\n");
		return -ENOMEM;
	}

	pkg->pkg_type = DSI_DPI_SPK;
	pkg->transmission_type = transmission;
	pkg->pkg.spk_pkg.cmd = spk_pkg;

	INIT_LIST_HEAD(&pkg->entry);

	pkg_sender_queue_pkg(sender, pkg, 0);

	return 0;
}

int dsi_cmds_kick_out(struct dsi_pkg_sender *sender)
{
	return process_pkg_list(sender);
}

int dsi_status_check(struct dsi_pkg_sender *sender)
{
	return dsi_error_handler(sender);
}

int dsi_check_fifo_empty(struct dsi_pkg_sender *sender)
{
	if (!sender) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	if (!sender->dbi_pkg_support) {
		pr_err("No DBI pkg sending on this sender\n");
		return -ENOTSUPP;
	}

	/* FIXME: need to check DPI_FIFO_EMPTY as well? */
	return REG_READ(sender->mipi_gen_fifo_stat_reg) & DBI_FIFO_EMPTY;
}

int dsi_send_dcs(struct dsi_pkg_sender *sender,
			u8 dcs, u8 *param, u32 param_num, u8 data_src,
			int delay)
{
	u32 cb_phy;
	u32 index = 0;
	u8 *cb;
	int retry = 1;
	u8 *dst = NULL;
	u8 *pSendparam = NULL;
	int err = 0;
	int i;
	int loop_num = 1;
	int offset = 0;

	if (!sender) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	cb_phy = sender->dbi_cb_phy;
	cb = (u8 *)sender->dbi_cb_addr;

	if (!sender->dbi_pkg_support) {
		pr_err("No DBI pkg sending on this sender\n");
		return -ENOTSUPP;
	}

	/*
	 * If dcs is write_mem_start, send it directly using
	 * DSI adapter interface
	 */
	if (dcs == write_mem_start) {

		/**
		 * query whether DBI FIFO is empty,
		 * if not sleep the drv and wait for it to become empty.
		 * The MIPI frame done interrupt will wake up the drv.
		 */
		mutex_lock(&sender->lock);
		for (i = 0; i < loop_num; i++) {
			if (i != 0)
				offset = MIPI_C_REG_OFFSET;

			retry = DSI_DBI_FIFO_TIMEOUT;
			while (retry &&
				!(REG_READ(
				sender->mipi_gen_fifo_stat_reg + offset) &
				DBI_FIFO_EMPTY)) {
				udelay(500);
				retry--;
			}

			/*if DBI FIFO timeout, drop this frame*/
			if (!retry) {
				pr_err("DBI FIFO timeout, drop frame\n");
				mutex_unlock(&sender->lock);
				if (!IS_ANN()) {
					debug_dbi_hang(sender);
					panic("DBI FIFO timeout, drop frame\n");
				}
				return -EIO;
			}

			if (i != 0)
				sender->work_for_slave_panel = true;

			/*wait for generic fifo*/
			if (REG_READ(MIPIA_HS_LS_DBI_ENABLE_REG + offset) &
				DBI_HS_LS_SWITCH_RE)
				wait_for_lp_fifos_empty(sender);
			else
				wait_for_hs_fifos_empty(sender);
			sender->work_for_slave_panel = false;
		}

		/*record the last screen update timestamp*/
		atomic64_set(&sender->last_screen_update,
			atomic64_read(&sender->te_seq));
		*(cb + (index++)) = write_mem_start;

		/* Set write_mem_start to mipi C first */
		REG_WRITE(sender->mipi_cmd_len_reg, 1);
		REG_WRITE(sender->mipi_cmd_addr_reg,
			cb_phy | CMD_DATA_MODE | CMD_VALID);

		retry = DSI_DBI_FIFO_TIMEOUT;
		while (retry && (REG_READ(sender->mipi_cmd_addr_reg) &
				 CMD_VALID)) {
			udelay(1);
			retry--;
		}
		mutex_unlock(&sender->lock);
		return 0;
	}

	if (param_num == 0)
		err =  dsi_send_mcs_short_hs(sender, dcs, 0, 0, delay);
	else if (param_num == 1)
		err =  dsi_send_mcs_short_hs(sender, dcs, param[0], 1,
				delay);
	else if (param_num > 1) {
		/*transfer to dcs package*/
		pSendparam = kmalloc(sizeof(u8) * (param_num + 1), GFP_KERNEL);
		if (!pSendparam) {
			pr_err("No memory\n");
			return -ENOMEM;
		}

		(*pSendparam) = dcs;

		dst = pSendparam + 1;
		memcpy(dst, param, param_num);

		err = dsi_send_mcs_long_hs(sender, pSendparam,
				param_num + 1, delay);

		/*free pkg*/
		kfree(pSendparam);
	}

	return err;
}

/* FIXME: to use another sender to issue write_mem_start. */
int dsi_send_dual_dcs(struct dsi_pkg_sender *sender,
			u8 dcs, u8 *param, u32 param_num, u8 data_src,
			int delay, bool is_dual_link)
{
	u32 cb_phy;
	u32 index = 0;
	u8 *cb;
	int retry = 1;
	int i;
	int loop_num = 1;
	int offset = 0;

	if (!sender) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	cb_phy = sender->dbi_cb_phy;
	cb = (u8 *)sender->dbi_cb_addr;

	if (!sender->dbi_pkg_support) {
		pr_err("No DBI pkg sending on this sender\n");
		return -ENOTSUPP;
	}

	/**
	 * query whether DBI FIFO is empty,
	 * if not sleep the drv and wait for it to become empty.
	 * The MIPI frame done interrupt will wake up the drv.
	 */
	if (is_dual_link)
		loop_num = 2;
	mutex_lock(&sender->lock);
	for (i = 0; i < loop_num; i++) {
		if (i != 0)
			offset = MIPI_C_REG_OFFSET;

		retry = DSI_DBI_FIFO_TIMEOUT;
		while (retry &&
			!(REG_READ(sender->mipi_gen_fifo_stat_reg + offset) &
			DBI_FIFO_EMPTY)) {
			udelay(500);
			retry--;
		}

		/*if DBI FIFO timeout, drop this frame*/
		if (!retry) {
			pr_err("DBI FIFO timeout, drop frame\n");
			mutex_unlock(&sender->lock);
			if (!IS_ANN()) {
				debug_dbi_hang(sender);
				panic("DBI FIFO timeout, drop frame\n");
			}
			return -EIO;
		}

		if (i != 0)
			sender->work_for_slave_panel = true;

		/*wait for generic fifo*/
		if (REG_READ(MIPIA_HS_LS_DBI_ENABLE_REG + offset) &
			DBI_HS_LS_SWITCH_RE)
			wait_for_lp_fifos_empty(sender);
		else
			wait_for_hs_fifos_empty(sender);
		sender->work_for_slave_panel = false;
	}

	/*record the last screen update timestamp*/
	atomic64_set(&sender->last_screen_update,
			atomic64_read(&sender->te_seq));
	*(cb + (index++)) = write_mem_start;

	pr_info("--> Sending write_mem_start\n");

	/* Set write_mem_start to mipi C first */
	if (is_dual_link)
		REG_WRITE(sender->mipi_cmd_len_reg + MIPI_C_REG_OFFSET, 1);
	REG_WRITE(sender->mipi_cmd_len_reg, 1);
	if (is_dual_link)
		REG_WRITE(sender->mipi_cmd_addr_reg + MIPI_C_REG_OFFSET,
			cb_phy | CMD_DATA_MODE | CMD_VALID);
	REG_WRITE(sender->mipi_cmd_addr_reg,
		cb_phy | CMD_DATA_MODE | CMD_VALID);

	if (is_dual_link) {
		retry = DSI_DBI_FIFO_TIMEOUT;
		while (retry &&
			(REG_READ(
			sender->mipi_cmd_addr_reg + MIPI_C_REG_OFFSET) &
			CMD_VALID)) {
			udelay(1);
			retry--;
		}
	}

	retry = DSI_DBI_FIFO_TIMEOUT;
	while (retry && (REG_READ(sender->mipi_cmd_addr_reg) & CMD_VALID)) {
		udelay(1);
		retry--;
	}
	mutex_unlock(&sender->lock);
	return 0;
}

int dsi_send_mcs_short_hs(struct dsi_pkg_sender *sender,
				u8 cmd, u8 param, u8 param_num, int delay)
{
	if (!sender) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	return dsi_send_mcs_short(sender, cmd, param, param_num,
			DSI_HS_TRANSMISSION, delay);
}

int dsi_send_mcs_short_lp(struct dsi_pkg_sender *sender,
				u8 cmd, u8 param, u8 param_num, int delay)
{
	if (!sender) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	return dsi_send_mcs_short(sender, cmd, param, param_num,
			DSI_LP_TRANSMISSION, delay);
}

int dsi_send_mcs_long_hs(struct dsi_pkg_sender *sender,
				u8 *data,
				u32 len,
				int delay)
{
	if (!sender || !data || !len) {
		pr_err("Invalid parameters\n");
		return -EINVAL;
	}

	return dsi_send_mcs_long(sender, data, len,
			DSI_HS_TRANSMISSION, delay);
}

int dsi_send_mcs_long_lp(struct dsi_pkg_sender *sender,
				u8 *data,
				u32 len,
				int delay)
{
	if (!sender || !data || !len) {
		pr_err("Invalid parameters\n");
		return -EINVAL;
	}

	return dsi_send_mcs_long(sender, data, len,
			DSI_LP_TRANSMISSION, delay);
}

int dsi_send_gen_short_hs(struct dsi_pkg_sender *sender,
				u8 param0, u8 param1, u8 param_num, int delay)
{
	if (!sender) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	return dsi_send_gen_short(sender, param0, param1, param_num,
			DSI_HS_TRANSMISSION, delay);
}

int dsi_send_gen_short_lp(struct dsi_pkg_sender *sender,
				u8 param0, u8 param1, u8 param_num, int delay)
{
	if (!sender || param_num < 0 || param_num > 2) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	return dsi_send_gen_short(sender, param0, param1, param_num,
			DSI_LP_TRANSMISSION, delay);
}

int dsi_send_gen_long_hs(struct dsi_pkg_sender *sender,
				u8 *data,
				u32 len,
				int delay)
{
	if (!sender || !data || !len) {
		pr_err("Invalid parameters\n");
		return -EINVAL;
	}

	return dsi_send_gen_long(sender, data, len,
			DSI_HS_TRANSMISSION, delay);
}

int dsi_send_gen_long_lp(struct dsi_pkg_sender *sender,
				u8 *data,
				u32 len,
				int delay)
{
	if (!sender || !data || !len) {
		pr_err("Invalid parameters\n");
		return -EINVAL;
	}

	return dsi_send_gen_long(sender, data, len,
			DSI_LP_TRANSMISSION, delay);
}

int dsi_read_gen_hs(struct dsi_pkg_sender *sender,
			u8 param0,
			u8 param1,
			u8 param_num,
			u8 *data,
			u32 len)
{
	if (!sender || !data || param_num < 0 || param_num > 2
		|| !data || !len) {
		pr_err("Invalid parameters\n");
		return -EINVAL;
	}

	return dsi_read_gen(sender, param0, param1, param_num,
				data, len, DSI_HS_TRANSMISSION);

}

int dsi_read_gen_lp(struct dsi_pkg_sender *sender,
			u8 param0,
			u8 param1,
			u8 param_num,
			u8 *data,
			u32 len)
{
	if (!sender || !data || param_num < 0 || param_num > 2
		|| !data || !len) {
		pr_err("Invalid parameters\n");
		return -EINVAL;
	}

	return dsi_read_gen(sender, param0, param1, param_num,
				data, len, DSI_LP_TRANSMISSION);
}

int dsi_read_mcs_hs(struct dsi_pkg_sender *sender,
			u8 cmd,
			u8 *data,
			u32 len)
{
	if (!sender || !data || !len) {
		pr_err("Invalid parameters\n");
		return -EINVAL;
	}

	return dsi_read_mcs(sender, cmd, data, len,
				DSI_HS_TRANSMISSION);
}
EXPORT_SYMBOL(dsi_read_mcs_hs);

int dsi_read_mcs_lp(struct dsi_pkg_sender *sender,
			u8 cmd,
			u8 *data,
			u32 len)
{
	if (!sender || !data || !len) {
		pr_err("Invalid parameters\n");
		return -EINVAL;
	}

	return dsi_read_mcs(sender, cmd, data, len,
				DSI_LP_TRANSMISSION);
}

int dsi_send_dpi_spk_pkg_hs(struct dsi_pkg_sender *sender,
				u32 spk_pkg)
{
	if (!sender) {
		pr_err("Invalid parameters\n");
		return -EINVAL;
	}

	return dsi_send_dpi_spk_pkg(sender, spk_pkg,
				DSI_HS_TRANSMISSION);
}

int dsi_send_dpi_spk_pkg_lp(struct dsi_pkg_sender *sender,
				u32 spk_pkg)
{
	if (!sender) {
		pr_err("Invalid parameters\n");
		return -EINVAL;
	}

	return dsi_send_dpi_spk_pkg(sender, spk_pkg,
				DSI_LP_TRANSMISSION);
}

int dsi_wait_for_fifos_empty(struct dsi_pkg_sender *sender)
{
	return wait_for_all_fifos_empty(sender);
}

void dsi_report_te(struct dsi_pkg_sender *sender)
{
	if (sender)
		atomic64_inc(&sender->te_seq);
}

int dsi_pkg_sender_init(struct dsi_pkg_sender *sender, u32 gtt_phy_addr,
	int type, int pipe)
{
	int ret;
	struct dsi_pkg *pkg, *tmp;
	int i;

	pr_debug("\n");

	if (!sender) {
		pr_err("Invalid parameter\n");
		return -EINVAL;
	}

	memset(sender, 0, sizeof(struct dsi_pkg_sender));

	sender->pipe = pipe;
	sender->pkg_num = 0;
	sender->panel_mode = 0;
	sender->status = DSI_PKG_SENDER_FREE;

	/*int dbi command buffer*/
	if (type == DSI_DBI) {
		sender->dbi_pkg_support = 1;
		ret = dbi_cb_init(sender, gtt_phy_addr, pipe);
		if (ret) {
			pr_err("DBI command buffer map failed\n");
			goto mapping_err;
		}
	}

	/*init regs*/
	if (pipe == 0) {
		sender->dpll_reg = DPLL_CTRL_A;
		sender->dspcntr_reg = DSPACNTR;
		sender->pipeconf_reg = PIPEACONF;
		sender->dsplinoff_reg = DSPALINOFF;
		sender->dspsurf_reg = DSPASURF;
		sender->pipestat_reg = PIPEASTAT;

		sender->mipi_intr_stat_reg = MIPIA_INTR_STAT_REG;
		sender->mipi_lp_gen_data_reg = MIPIA_LP_GEN_DATA_REG;
		sender->mipi_hs_gen_data_reg = MIPIA_HS_GEN_DATA_REG;
		sender->mipi_lp_gen_ctrl_reg = MIPIA_LP_GEN_CTRL_REG;
		sender->mipi_hs_gen_ctrl_reg = MIPIA_HS_GEN_CTRL_REG;
		sender->mipi_gen_fifo_stat_reg = MIPIA_GEN_FIFO_STAT_REG;
		sender->mipi_data_addr_reg = MIPIA_DATA_ADD;
		sender->mipi_data_len_reg = MIPIA_DATA_LEN;
		sender->mipi_cmd_addr_reg = MIPIA_CMD_ADD;
		sender->mipi_cmd_len_reg = MIPIA_CMD_LEN;
		sender->mipi_dpi_control_reg = MIPIA_DPI_CTRL_REG;
	} else if (pipe == 2) {
		sender->dpll_reg = DPLL_CTRL_A;
		sender->dspcntr_reg = DSPCCNTR;
		sender->pipeconf_reg = PIPECCONF;
		sender->dsplinoff_reg = DSPCLINOFF;
		sender->dspsurf_reg = DSPCSURF;
		sender->pipestat_reg = PIPECSTAT;

		sender->mipi_intr_stat_reg = MIPIC_INTR_STAT_REG;
		sender->mipi_lp_gen_data_reg = MIPIC_LP_GEN_DATA_REG;
		sender->mipi_hs_gen_data_reg = MIPIC_HS_GEN_DATA_REG;
		sender->mipi_lp_gen_ctrl_reg = MIPIC_LP_GEN_CTRL_REG;
		sender->mipi_hs_gen_ctrl_reg = MIPIC_HS_GEN_CTRL_REG;
		sender->mipi_gen_fifo_stat_reg = MIPIC_GEN_FIFO_STAT_REG;
		sender->mipi_data_addr_reg = MIPIC_DATA_ADD;
		sender->mipi_data_len_reg = MIPIC_DATA_LEN;
		sender->mipi_cmd_addr_reg = MIPIC_CMD_ADD;
		sender->mipi_cmd_len_reg = MIPIC_CMD_LEN;
		sender->mipi_dpi_control_reg = MIPIC_DPI_CTRL_REG;
	}

	/*init pkg list*/
	INIT_LIST_HEAD(&sender->pkg_list);
	INIT_LIST_HEAD(&sender->free_list);

	/*init lock*/
	mutex_init(&sender->lock);

	/*allocate free pkg pool*/
	for (i = 0; i < MAX_PKG_NUM; i++) {
		pkg = kzalloc(sizeof(struct dsi_pkg), GFP_KERNEL);
		if (!pkg) {
			ret = -ENOMEM;
			goto pkg_alloc_err;
		}

		INIT_LIST_HEAD(&pkg->entry);

		/*append to free list*/
		list_add_tail(&pkg->entry, &sender->free_list);
	}

	/*init te & screen update seqs*/
	atomic64_set(&sender->te_seq, 0);
	atomic64_set(&sender->last_screen_update, 0);

	pr_debug("initialized\n");

	return 0;

pkg_alloc_err:
	list_for_each_entry_safe(pkg, tmp, &sender->free_list, entry) {
		list_del(&pkg->entry);
		kfree(pkg);
	}

	/*free mapped command buffer*/
	dbi_cb_destroy(sender);
mapping_err:
	return ret;
}

void dsi_pkg_sender_destroy(struct dsi_pkg_sender *sender)
{
	struct dsi_pkg *pkg, *tmp;

	if (!sender || IS_ERR(sender))
		return;

	/*free pkg pool*/
	list_for_each_entry_safe(pkg, tmp, &sender->free_list, entry) {
		list_del(&pkg->entry);
		kfree(pkg);
	}

	/*free pkg list*/
	list_for_each_entry_safe(pkg, tmp, &sender->pkg_list, entry) {
		list_del(&pkg->entry);
		kfree(pkg);
	}

	/*free mapped command buffer*/
	dbi_cb_destroy(sender);

	pr_debug("destroyed\n");
}

