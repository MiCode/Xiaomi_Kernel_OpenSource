/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/soundwire/soundwire.h>
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include "swrm_registers.h"
#include "swr-wcd-ctrl.h"

u8 mstr_ports[] = {100, 101, 102, 103, 104, 105, 106, 107};
#define SWR_MSTR_PORT_LEN		ARRAY_SIZE(mstr_ports)
#define SWR_NUM_SLV_DEVICES		3 /* This includes dev_num_0 */

struct usecase uc[] = {
	{0, 0, 0},		/* UC0: no ports */
	{1, 1, 2400},		/* UC1: Spkr */
	{1, 4, 600},		/* UC2: Compander */
	{1, 2, 300},		/* UC3: Smart Boost */
	{1, 2, 1200},		/* UC4: VI Sense */
	{4, 9, 4500},		/* UC5: Spkr + Comp + SB + VI */
	{8, 18, 9000},		/* UC6: 2*(Spkr + Comp + SB + VI) */
	{2, 2, 4800},		/* UC7: 2*Spkr */
	{2, 5, 3000},		/* UC8: Spkr + Comp */
	{4, 10, 6000},		/* UC9: 2*(Spkr + Comp) */
};
#define MAX_USECASE	ARRAY_SIZE(uc)

struct port_params pp[MAX_USECASE][SWR_MSTR_PORT_LEN] = {
	/* UC 0 */
	{
	},
	/* UC 1 */
	{
		{8, 1, 0},
	},
	/* UC 2 */
	{
		{32, 2, 0},
	},
	/* UC 3 */
	{
		{64, 12, 15},
	},
	/* UC 4 */
	{
		{16, 7, 0},
	},
	/* UC 5 */
	{
		{8, 1, 0},
		{32, 2, 0},
		{64, 12, 15},
		{16, 7, 0},
	},
	/* UC 6 */
	{
		{8, 1, 0},
		{32, 2, 0},
		{64, 12, 15},
		{16, 7, 0},
		{8, 6, 0},
		{32, 18, 0},
		{64, 13, 15},
		{16, 10, 0},
	},
	/* UC 7 */
	{
		{8, 1, 0},
		{8, 6, 0},

	},
	/* UC 8 */
	{
		{8, 1, 0},
		{32, 2, 0},
	},
	/* UC 9 */
	{
		{8, 1, 0},
		{32, 2, 0},
		{8, 6, 0},
		{32, 18, 0},
	},
};

enum {
	SWR_NOT_PRESENT,
	SWR_ATTACHED_OK,
	SWR_ALERT,
	SWR_RESERVED,
};

static int swrm_get_port_config(struct swr_master *master)
{
	u32 ch_rate = 0;
	u32 num_ch = 0;
	int i, uc_idx;
	u32 portcount = 0;

	for (i = 0; i < master->num_port; i++) {
		if (master->port[i].port_en) {
			ch_rate += master->port[i].ch_rate;
			num_ch += master->port[i].num_ch;
			portcount++;
		}
	}

	for (i = 0; i < ARRAY_SIZE(uc); i++) {
		if ((uc[i].num_port == portcount) &&
		    (uc[i].num_ch == num_ch) &&
		    (uc[i].chrate == ch_rate)) {
			uc_idx = i;
			break;
		}
	}
	if (i >= ARRAY_SIZE(uc)) {
		dev_err(&master->dev,
			"%s: usecase port:%d, num_ch:%d, chrate:%d not found\n",
			__func__, master->num_port, num_ch, ch_rate);
		return -EINVAL;
	}
	for (i = 0; i < master->num_port; i++) {
		if (master->port[i].port_en) {
			master->port[i].sinterval = pp[uc_idx][i].si;
			master->port[i].offset1 = pp[uc_idx][i].off1;
			master->port[i].offset2 = pp[uc_idx][i].off2;
		}
	}
	return 0;
}

static int swrm_get_master_port(u8 *mstr_port_id, u8 slv_port_id)
{
	int i;
	for (i = 0; i < SWR_MSTR_PORT_LEN; i++) {
		if (mstr_ports[i] == slv_port_id) {
			*mstr_port_id = i;
			return 0;
		}
	}
	return -EINVAL;
}

static u32 get_cmd_rd_fifo_depth(struct swr_mstr_ctrl *swrm)
{
	return (swrm->read(swrm->handle, SWRM_COMP_PARAMS) &
		SWRM_COMP_PARAMS_RD_FIFO_DEPTH) >> 15;
}

static int swrm_cmd_fifo_rd_cmd(struct swr_mstr_ctrl *swrm, u32 *cmd_data,
				 u8 dev_addr, u8 cmd_id, u16 reg_addr,
				 u32 len)
{
	u32 val;
	u32 fifo_cnt;
	int ret = 0;

	do {
		fifo_cnt = ((swrm->read(swrm->handle, SWRM_CMD_FIFO_STATUS) &
			     SWRM_CMD_FIFO_STATUS_RD_CMD_FIFO_CNT_MASK) >> 16);
	} while (fifo_cnt >= (get_cmd_rd_fifo_depth(swrm) - 1));

	if (!cmd_id) {
		cmd_id = swrm->rcmd_id;
		swrm->rcmd_id = ((swrm->rcmd_id < 14) ? (cmd_id++) : 0);
	}
	val = (reg_addr | (cmd_id << 16) | (dev_addr << 20) |
		(len << 24));

	ret = swrm->write(swrm->handle, SWRM_CMD_FIFO_RD_CMD, val);
	if (ret < 0) {
		dev_err(swrm->dev, "%s: reg 0x%x write failed, err:%d\n",
			__func__, val, ret);
		goto err;
	}
	*cmd_data = swrm->read(swrm->handle, SWRM_CMD_FIFO_RD_FIFO_ADDR);
err:
	return ret;
}

static u32 get_cmd_wr_fifo_depth(struct swr_mstr_ctrl *swrm)
{
	return (swrm->read(swrm->handle, SWRM_COMP_PARAMS) &
		SWRM_COMP_PARAMS_WR_FIFO_DEPTH) >> 10;
}

static int swrm_cmd_fifo_wr_cmd(struct swr_mstr_ctrl *swrm, u8 cmd_data,
				 u8 dev_addr, u8 cmd_id, u16 reg_addr)
{
	u32 val;
	u32 fifo_cnt;
	int ret = 0;

	do {
		fifo_cnt = ((swrm->read(swrm->handle, SWRM_CMD_FIFO_STATUS) &
			     SWRM_CMD_FIFO_STATUS_WR_CMD_FIFO_CNT_MASK) >> 8);
	} while (fifo_cnt >= (get_cmd_wr_fifo_depth(swrm) - 1));

	if (!cmd_id) {
		cmd_id = swrm->wcmd_id;
		swrm->wcmd_id = ((swrm->wcmd_id < 14) ? (cmd_id++) : 0);
	}
	val = (reg_addr | (cmd_id << 16) | (dev_addr << 20) |
		(cmd_data << 24));

	ret = swrm->write(swrm->handle, SWRM_CMD_FIFO_WR_CMD, val);
	if (ret < 0) {
		dev_err(swrm->dev, "%s: reg 0x%x write failed, err:%d\n",
			__func__, val, ret);
		goto err;
	}
	if (cmd_id == 0xF)
		wait_for_completion_timeout(&swrm->broadcast, (2 * HZ/10));
err:
	return ret;
}

static int swrm_read(struct swr_master *master, u8 dev_num, u32 reg_addr,
		u32 *buf, u32 len)
{
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);

	if (dev_num) {
		swrm_cmd_fifo_rd_cmd(swrm, buf, dev_num, 0, reg_addr,
					len);
	} else {
		if (swrm->read)
			buf[0] = swrm->read(swrm->handle, reg_addr);
		else {
			dev_err(&master->dev, "%s: handle is NULL 0x%x\n",
				__func__, reg_addr);
			return -EINVAL;
		}
	}
	return 0;
}

static int swrm_write(struct swr_master *master, u8 dev_num, u32 reg_addr,
		u32 *buf)
{
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	int ret = 0;

	if (dev_num) {
		ret = swrm_cmd_fifo_wr_cmd(swrm, buf[0], dev_num, 0, reg_addr);
	} else {
		if (swrm->write) {
			ret = swrm->write(swrm->handle, reg_addr, buf[0]);
		} else {
			dev_err(&master->dev, "%s: handle is NULL 0x%x\n",
				__func__, reg_addr);
			return -EINVAL;
		}
	}
	return ret;
}

static u8 get_inactive_bank_num(struct swr_mstr_ctrl *swrm)
{
	return (swrm->read(swrm->handle, SWRM_MCP_STATUS) &
		SWRM_MCP_STATUS_BANK_NUM_MASK) ? 0 : 1;
}

static void enable_bank_switch(struct swr_mstr_ctrl *swrm, u8 bank,
				u8 row, u8 col)
{
	swrm_cmd_fifo_wr_cmd(swrm, ((row << 3) | col), 0xF, 0,
			SWRS_SCP_FRAME_CTRL_BANK(bank));
}

static void disable_chs_next_bank(struct swr_master *master,
				  u8 bank, u8 *mport)
{
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	struct swr_port_info *port;
	int i;
	u32 value;

	for (i = 0; i < master->num_port; i++) {
		port = &master->port[i];

		swrm_cmd_fifo_wr_cmd(swrm, 0, port->dev_id, 0,
				SWRS_DP_CHANNEL_ENABLE_BANK(i, bank));
		value = ((swrm->read(swrm->handle,
			  SWRM_DP_PORT_CTRL_BANK((mport[i]+1), bank))) &
			  ~SWRM_DP_PORT_CTRL_EN_CHAN_MASK);

		swrm->write(swrm->handle,
			    SWRM_DP_PORT_CTRL_BANK((mport[i]+1), bank), value);
	}
}

static void swrm_apply_port_config(struct swr_master *master, u8 *mport)
{
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	u32 value;
	struct swr_port_info *port;
	u8 bank;
	int i;

	bank = get_inactive_bank_num(swrm);
	dev_dbg(&master->dev, "%s: bank %d\n", __func__, bank);

	for (i = 0; i < master->num_port; i++) {

		port = &master->port[i];
		swrm_cmd_fifo_wr_cmd(swrm, port->ch_en, port->dev_id, 0,
				SWRS_DP_CHANNEL_ENABLE_BANK(i, bank));
		swrm_cmd_fifo_wr_cmd(swrm, port->sinterval, port->dev_id, 0,
				SWRS_DP_SAMPLE_CONTROL_1_BANK(i, bank));
		swrm_cmd_fifo_wr_cmd(swrm, port->offset1, port->dev_id, 0,
				SWRS_DP_OFFSET_CONTROL_1_BANK(i, bank));
		swrm_cmd_fifo_wr_cmd(swrm, port->offset2, port->dev_id, 0,
				SWRS_DP_OFFSET_CONTROL_2_BANK(i, bank));
		swrm_cmd_fifo_wr_cmd(swrm, (SWR_MAX_COL-1), port->dev_id, 0,
				SWRS_DP_HCONTROL_BANK(i, bank));
		swrm_cmd_fifo_wr_cmd(swrm, 1, port->dev_id, 0,
				SWRS_DP_BLOCK_CONTROL_3_BANK(i, bank));

		value = ((port->ch_en)
				<< SWRM_DP_PORT_CTRL_EN_CHAN_SHFT);
		value |= ((port->offset2)
				<< SWRM_DP_PORT_CTRL_OFFSET2_SHFT);
		value |= ((port->offset1)
				<< SWRM_DP_PORT_CTRL_OFFSET1_SHFT);
		value |= port->sinterval;

		swrm->write(swrm->handle,
			    SWRM_DP_PORT_CTRL_BANK((mport[i]+1), bank), value);
	}
	enable_bank_switch(swrm, bank, SWR_MAX_ROW, SWR_MAX_COL);
	disable_chs_next_bank(master, !(!(!bank)), mport);
}

static int swrm_connect_port(struct swr_master *master,
			struct swr_params *portinfo)
{
	int i;
	struct swr_port_info *port;
	int ret = 0;
	u8 mport[SWR_MSTR_PORT_LEN];

	dev_dbg(&master->dev, "%s: enter\n", __func__);
	if (!portinfo)
		return -EINVAL;

	for (i = 0; i < portinfo->num_port; i++) {
		ret = swrm_get_master_port(&mport[i],
						portinfo->port_id[i]);
		if (ret < 0) {
			dev_err(&master->dev,
				"%s: mstr portid for slv port %d not found\n",
				__func__, portinfo->port_id[i]);
			return -EINVAL;
		}
		port = &master->port[i];
		port->dev_id = portinfo->dev_id;
		port->port_id = portinfo->port_id[i];
		port->num_ch = portinfo->num_ch[i];
		port->ch_rate = portinfo->ch_rate[i];
		port->ch_en = portinfo->ch_en[i];
		port->port_en = true;
		dev_dbg(&master->dev,
			"%s: port_id %d ch_rate %d num_ch %d ch_en %d\n",
			__func__, port->port_id, port->ch_rate, port->num_ch,
			port->ch_en);
	}
	master->num_port += portinfo->num_port;
	if (master->num_port >= SWR_MSTR_PORT_LEN)
		master->num_port = SWR_MSTR_PORT_LEN;

	swrm_get_port_config(master);
	swr_port_response(master, portinfo->tid);
	swrm_apply_port_config(master, &mport[0]);
	return 0;
}

static int swrm_disconnect_port(struct swr_master *master,
			struct swr_params *portinfo)
{
	int i;
	struct swr_port_info *port;
	u8 mport[SWR_MSTR_PORT_LEN];
	int ret = 0;

	if (!portinfo) {
		dev_err(&master->dev, "%s: portinfo is NULL\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < portinfo->num_port; i++) {
		ret = swrm_get_master_port(&mport[i],
						portinfo->port_id[i]);
		if (ret < 0) {
			dev_err(&master->dev,
				"%s: mstr portid for slv port %d not found\n",
				__func__, portinfo->port_id[i]);
			return -EINVAL;
		}
		port = &master->port[i];
		port->dev_id = portinfo->dev_id;
		port->port_id = portinfo->port_id[i];
		port->num_ch = portinfo->num_ch[i];
		port->ch_rate = portinfo->ch_rate[i];
		port->ch_en = portinfo->ch_en[i];
		port->port_en = false;
	}
	if (master->num_port >= SWR_MSTR_PORT_LEN)
		master->num_port = SWR_MSTR_PORT_LEN;

	swrm_get_port_config(master);
	master->num_port -= portinfo->num_port;
	swr_port_response(master, portinfo->tid);
	swrm_apply_port_config(master, &mport[0]);

	return 0;
}

static int swrm_check_slave_change_status(struct swr_mstr_ctrl *swrm,
					int status, u8 *devnum)
{
	int i;
	int new_sts = status;
	int ret = SWR_NOT_PRESENT;

	if (status != swrm->slave_status) {
		for (i = 0; i < SWR_NUM_SLV_DEVICES; i++) {
			if ((status & SWRM_MCP_SLV_STATUS_MASK) !=
			    (swrm->slave_status & SWRM_MCP_SLV_STATUS_MASK)) {
				ret = (status & SWRM_MCP_SLV_STATUS_MASK);
				*devnum = i;
				break;
			}
			status >>= 2;
			swrm->slave_status >>= 2;
		}
		swrm->slave_status = new_sts;
	}
	return ret;
}

static irqreturn_t swr_mstr_interrupt(int irq, void *dev)
{
	struct swr_mstr_ctrl *swrm = dev;
	u32 value;
	int status;
	u8 devnum;

	value = swrm->read(swrm->handle, SWRM_INTERRUPT_STATUS);
	value &= SWRM_INTERRUPT_STATUS_RMSK;
	swrm->write(swrm->handle, SWRM_INTERRUPT_CLEAR, value);

	switch (value) {
	case SWRM_INTERRUPT_STATUS_SLAVE_PEND_IRQ:
		break;
	case SWRM_INTERRUPT_STATUS_NEW_SLAVE_ATTACHED:
		break;
	case SWRM_INTERRUPT_STATUS_CHANGE_ENUM_SLAVE_STATUS:
		status = swrm->read(swrm->handle, SWRM_MCP_SLV_STATUS);
		switch (swrm_check_slave_change_status(swrm, status, &devnum)) {
		case SWR_NOT_PRESENT:
			dev_dbg(swrm->dev, "device %d got detached\n",
				devnum);
			break;
		case SWR_ATTACHED_OK:
			dev_dbg(swrm->dev, "device %d got attached\n",
				devnum);
			break;
		case SWR_ALERT:
			dev_dbg(swrm->dev, "device %d has pending interrupt\n",
				devnum);
			break;
		}
		break;
	case SWRM_INTERRUPT_STATUS_MASTER_CLASH_DET:
		dev_err(swrm->dev, "SWR bus clash detected\n");
		break;
	case SWRM_INTERRUPT_STATUS_RD_FIFO_OVERFLOW:
		dev_dbg(swrm->dev, "SWR read FIFO overflow\n");
		break;
	case SWRM_INTERRUPT_STATUS_RD_FIFO_UNDERFLOW:
		dev_dbg(swrm->dev, "SWR read FIFO underflow\n");
		break;
	case SWRM_INTERRUPT_STATUS_WR_CMD_FIFO_OVERFLOW:
		dev_dbg(swrm->dev, "SWR write FIFO overflow\n");
		break;
	case SWRM_INTERRUPT_STATUS_CMD_ERROR:
		dev_err(swrm->dev, "SWR CMD error detected\n");
		break;
	case SWRM_INTERRUPT_STATUS_DOUT_PORT_COLLISION:
		dev_dbg(swrm->dev, "SWR Port collision detected\n");
		break;
	case SWRM_INTERRUPT_STATUS_READ_EN_RD_VALID_MISMATCH:
		dev_dbg(swrm->dev, "SWR read enable valid mismatch\n");
		break;
	case SWRM_INTERRUPT_STATUS_SPECIAL_CMD_ID_FINISHED:
		complete(&swrm->broadcast);
		break;
	case SWRM_INTERRUPT_STATUS_NEW_SLAVE_AUTO_ENUM_FINISHED:
		break;
	case SWRM_INTERRUPT_STATUS_AUTO_ENUM_FAILED:
		break;
	case SWRM_INTERRUPT_STATUS_AUTO_ENUM_TABLE_IS_FULL:
		break;
	case SWRM_INTERRUPT_STATUS_BUS_RESET_FINISHED:
		complete(&swrm->reset);
		break;
	case SWRM_INTERRUPT_STATUS_CLK_STOP_FINISHED:
		break;
	default:
		break;
	}
	return IRQ_HANDLED;
}

static int swrm_get_device_status(struct swr_mstr_ctrl *swrm, u8 devnum)
{
	u32 val = (swrm->slave_status >> (devnum * 2));
	val &= SWRM_MCP_SLV_STATUS_MASK;
	return val;
}

static int swrm_get_auto_enum_slaves(struct swr_mstr_ctrl *swrm)
{
	int i;
	u32 val, tval;
	u32 status;
	u32 device_id1[SWR_NUM_SLV_DEVICES];
	u32 device_id2[SWR_NUM_SLV_DEVICES];

	val = swrm->read(swrm->handle, SWRM_MCP_SLV_STATUS);
	tval = val;

	for (i = 0; i < SWR_NUM_SLV_DEVICES; i++) {
		status = (val & SWRM_MCP_SLV_STATUS_MASK);
		dev_dbg(swrm->dev, "%s slave %d status %d\n",
			__func__, i, status);
		if (status == 0x01)
			swrm->num_enum_slaves++;
		val = (val >> 2);
	}

	for (i = 1; i < SWR_NUM_SLV_DEVICES; i++) {
		if (((tval >> 2) & SWRM_MCP_SLV_STATUS_MASK) == 0x01) {
			device_id1[i] = swrm->read(swrm->handle,
					    SWRM_ENUMERATOR_SLAVE_DEV_ID_1(i));
			device_id2[i] = swrm->read(swrm->handle,
					    SWRM_ENUMERATOR_SLAVE_DEV_ID_2(i));
			dev_dbg(swrm->dev,
				"%s: dev_id1 %d, dev_id2 %d of slave %d\n",
				__func__, device_id1[i], device_id2[i], i);
		}
	}
	return 0;
}

static int swrm_get_logical_dev_num(struct swr_master *mstr, u64 dev_id,
				u8 *dev_num)
{
	int i;
	u64 id;
	int ret = -EINVAL;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(mstr);

	for (i = 1; i < SWR_NUM_SLV_DEVICES; i++) {
		id = ((u64)(swrm->read(swrm->handle,
			    SWRM_ENUMERATOR_SLAVE_DEV_ID_2(i))) << 32);
		id |= swrm->read(swrm->handle,
			    SWRM_ENUMERATOR_SLAVE_DEV_ID_1(i));
		if (id == dev_id) {
			if (swrm_get_device_status(swrm, i) == 0x01) {
				*dev_num = i;
				ret = 0;
			} else {
				dev_err(swrm->dev, "%s: device is not ready\n",
					 __func__);
			}
			goto found;
		}
	}
	dev_err(swrm->dev, "%s: device id does not match\n", __func__);
found:
	return ret;
}

static int swrm_master_init(struct swr_mstr_ctrl *swrm)
{
	int ret = 0;
	u32 mask, val;
	u8 row_ctrl = SWR_MAX_ROW;
	u8 col_ctrl = SWR_MIN_COL;
	u8 ping_val = 0;
	u8 retry_cmd_num = 3;

	/* Clear Rows and Cols */
	mask = (SWRM_MCP_FRAME_CTRL_BANK_ROW_CTRL_BMSK |
		SWRM_MCP_FRAME_CTRL_BANK_COL_CTRL_BMSK);
	val = swrm->read(swrm->handle, SWRM_MCP_FRAME_CTRL_BANK_ADDR(0));
	val &= (~mask);
	val |= ((row_ctrl << SWRM_MCP_FRAME_CTRL_BANK_ROW_CTRL_SHFT) |
		(col_ctrl << SWRM_MCP_FRAME_CTRL_BANK_COL_CTRL_SHFT));
	swrm->write(swrm->handle, SWRM_MCP_FRAME_CTRL_BANK_ADDR(0), val);

	/* Clear all interrupts*/
	swrm->write(swrm->handle, SWRM_INTERRUPT_MASK_ADDR, 0);

	/* Set Auto enumeration flag */
	swrm->write(swrm->handle, SWRM_ENUMERATOR_CFG_ADDR, 1);

	/* Interrupt when Auto Enum Table is Full */
	swrm->write(swrm->handle, SWRM_INTERRUPT_MASK_ADDR,
		    1 << SWRM_INTERRUPT_MASK_AUTO_ENUM_TABLE_IS_FULL_SHFT);

	/* Configure NO_PINGS */
	val = swrm->read(swrm->handle, SWRM_MCP_CFG_ADDR);
	val &= ~SWRM_MCP_CFG_MAX_NUM_OF_CMD_NO_PINGS_BMSK;
	val |= (ping_val << SWRM_MCP_CFG_MAX_NUM_OF_CMD_NO_PINGS_SHFT);
	swrm->write(swrm->handle, SWRM_MCP_CFG_ADDR, val);

	/* Configure number of retries of a read/write cmd */
	val = swrm->read(swrm->handle, SWRM_CMD_FIFO_CFG_ADDR);
	val &= ~SWRM_CMD_FIFO_CFG_NUM_OF_CMD_RETRY_BMSK;
	val |= (retry_cmd_num << SWRM_CMD_FIFO_CFG_NUM_OF_CMD_RETRY_SHFT);
	swrm->write(swrm->handle, SWRM_CMD_FIFO_CFG_ADDR, val);

	/* Set IRQ LEVEL and enable*/
	val = (swrm->read(swrm->handle,
			SWRM_COMP_CFG_ADDR) | SWRM_COMP_CFG_RMSK);
	swrm->write(swrm->handle, SWRM_COMP_CFG_ADDR, val);

	/* Read the auto enum status */
	swrm_get_auto_enum_slaves(swrm);

	return ret;
}

static int swrm_probe(struct platform_device *pdev)
{
	struct swr_mstr_ctrl *swrm;
	struct swr_ctrl_platform_data *pdata;
	int ret;

	/* Allocate soundwire master driver structure */
	swrm = kzalloc(sizeof(struct swr_mstr_ctrl), GFP_KERNEL);
	if (!swrm) {
		dev_err(&pdev->dev, "%s: no memory for swr mstr controller\n",
			 __func__);
		ret = -ENOMEM;
		goto err_memory_fail;
	}
	swrm->dev = &pdev->dev;
	platform_set_drvdata(pdev, swrm);
	swr_set_ctrl_data(&swrm->master, swrm);
	pdata = dev_get_platdata(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "%s: pdata from parent is NULL\n",
			__func__);
		ret = -EINVAL;
		goto err_pdata_fail;
	}
	swrm->handle = (void *)pdata->handle;
	if (!swrm->handle) {
		dev_err(&pdev->dev, "%s: swrm->handle is NULL\n",
			__func__);
		ret = -EINVAL;
		goto err_pdata_fail;
	}
	swrm->read = pdata->read;
	if (!swrm->read) {
		dev_err(&pdev->dev, "%s: swrm->read is NULL\n",
			__func__);
		ret = -EINVAL;
		goto err_pdata_fail;
	}
	swrm->write = pdata->write;
	if (!swrm->write) {
		dev_err(&pdev->dev, "%s: swrm->write is NULL\n",
			__func__);
		ret = -EINVAL;
		goto err_pdata_fail;
	}
	swrm->clk = pdata->clk;
	if (!swrm->clk) {
		dev_err(&pdev->dev, "%s: swrm->clk is NULL\n",
			__func__);
		ret = -EINVAL;
		goto err_pdata_fail;
	}
	swrm->reg_irq = pdata->reg_irq;
	if (!swrm->reg_irq) {
		dev_err(&pdev->dev, "%s: swrm->reg_irq is NULL\n",
			__func__);
		ret = -EINVAL;
		goto err_pdata_fail;
	}
	swrm->master.read = swrm_read;
	swrm->master.write = swrm_write;
	swrm->master.get_logical_dev_num = swrm_get_logical_dev_num;
	swrm->master.connect_port = swrm_connect_port;
	swrm->master.disconnect_port = swrm_disconnect_port;
	swrm->master.dev.parent = &pdev->dev;
	swrm->master.dev.of_node = pdev->dev.of_node;
	swrm->master.num_port = 0;
	swrm->num_enum_slaves = 0;
	swrm->rcmd_id = 0;
	swrm->wcmd_id = 0;
	swrm->slave_status = 0;
	init_completion(&swrm->reset);
	init_completion(&swrm->broadcast);
	mutex_init(&swrm->mlock);

	ret = swrm->reg_irq(swrm->handle, swr_mstr_interrupt, swrm,
			    SWR_IRQ_REGISTER);
	if (ret) {
		dev_err(&pdev->dev, "%s: IRQ register failed ret %d\n",
			__func__, ret);
		goto err_irq_fail;
	}

	ret = swr_register_master(&swrm->master);
	if (ret) {
		dev_err(&pdev->dev, "%s: error adding swr master\n", __func__);
		goto err_mstr_fail;
	}

	if (pdev->dev.of_node)
		of_register_swr_devices(&swrm->master);

	/* Add devices registered with board-info as the
	   controller will be up now
	 */
	swr_master_add_boarddevices(&swrm->master);
	mutex_lock(&swrm->mlock);
	ret = swrm_master_init(swrm);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"%s: Error in master Initializaiton, err %d\n",
			__func__, ret);
		mutex_unlock(&swrm->mlock);
		goto err_mstr_fail;
	}
	mutex_unlock(&swrm->mlock);

	return 0;
err_mstr_fail:
	swrm->reg_irq(swrm->handle, swr_mstr_interrupt,
			swrm, SWR_IRQ_FREE);
err_irq_fail:
err_pdata_fail:
	kfree(swrm);
err_memory_fail:
	return ret;
}

static int swrm_remove(struct platform_device *pdev)
{
	struct swr_mstr_ctrl *swrm = platform_get_drvdata(pdev);

	swrm->reg_irq(swrm->handle, swr_mstr_interrupt,
			swrm, SWR_IRQ_FREE);
	swr_unregister_master(&swrm->master);
	mutex_destroy(&swrm->mlock);
	kfree(swrm);
	return 0;
}

static struct of_device_id swrm_dt_match[] = {
	{
		.compatible = "qcom,swr-wcd",
	},
	{}
};

static struct platform_driver swr_mstr_driver = {
	.probe = swrm_probe,
	.remove = swrm_remove,
	.driver = {
		.name = SWR_WCD_NAME,
		.owner = THIS_MODULE,
		.of_match_table = swrm_dt_match,
	},
};

static int __init swrm_init(void)
{
	return platform_driver_register(&swr_mstr_driver);
}
subsys_initcall(swrm_init);

static void __exit swrm_exit(void)
{
	platform_driver_unregister(&swr_mstr_driver);
}
module_exit(swrm_exit);


MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("WCD SoundWire Controller");
MODULE_ALIAS("platform:swr-wcd");
