/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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
#include <linux/delay.h>
#include <linux/kthread.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/of.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <soc/soundwire.h>
#include <soc/swr-wcd.h>
#include "swrm_registers.h"
#include "swr-wcd-ctrl.h"

#define SWR_BROADCAST_CMD_ID            0x0F
#define SWR_AUTO_SUSPEND_DELAY          3 /* delay in sec */
#define SWR_DEV_ID_MASK			0xFFFFFFFF
#define SWR_REG_VAL_PACK(data, dev, id, reg)	\
			((reg) | ((id) << 16) | ((dev) << 20) | ((data) << 24))

/* pm runtime auto suspend timer in msecs */
static int auto_suspend_timer = SWR_AUTO_SUSPEND_DELAY * 1000;
module_param(auto_suspend_timer, int, 0664);
MODULE_PARM_DESC(auto_suspend_timer, "timer for auto suspend");

static u8 mstr_ports[] = {100, 101, 102, 103, 104, 105, 106, 107};
static u8 mstr_port_type[] = {SWR_DAC_PORT, SWR_COMP_PORT, SWR_BOOST_PORT,
			      SWR_DAC_PORT, SWR_COMP_PORT, SWR_BOOST_PORT,
			      SWR_VISENSE_PORT, SWR_VISENSE_PORT};

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
	{3, 7, 3300},		/* UC10: Spkr + Comp + SB */
	{6, 14, 6600},		/* UC11: 2*(Spkr + Comp + SB) */
	{2, 3, 2700},		/* UC12: Spkr + SB */
	{4, 6, 5400},		/* UC13: 2*(Spkr + SB) */
	{3, 5, 3900},		/* UC14: Spkr + SB + VI */
	{6, 10, 7800},		/* UC15: 2*(Spkr + SB + VI) */
	{2, 3, 3600},		/* UC16: Spkr + VI */
	{4, 6, 7200},		/* UC17: 2*(Spkr + VI) */
	{3, 7, 4200},		/* UC18: Spkr + Comp + VI */
	{6, 14, 8400},		/* UC19: 2*(Spkr + Comp + VI) */
};
#define MAX_USECASE	ARRAY_SIZE(uc)

struct port_params pp[MAX_USECASE][SWR_MSTR_PORT_LEN] = {
	/* UC 0 */
	{
		{0, 0, 0},
	},
	/* UC 1 */
	{
		{7, 1, 0},
	},
	/* UC 2 */
	{
		{31, 2, 0},
	},
	/* UC 3 */
	{
		{63, 12, 31},
	},
	/* UC 4 */
	{
		{15, 7, 0},
	},
	/* UC 5 */
	{
		{7, 1, 0},
		{31, 2, 0},
		{63, 12, 31},
		{15, 7, 0},
	},
	/* UC 6 */
	{
		{7, 1, 0},
		{31, 2, 0},
		{63, 12, 31},
		{15, 7, 0},
		{7, 6, 0},
		{31, 18, 0},
		{63, 13, 31},
		{15, 10, 0},
	},
	/* UC 7 */
	{
		{7, 1, 0},
		{7, 6, 0},

	},
	/* UC 8 */
	{
		{7, 1, 0},
		{31, 2, 0},
	},
	/* UC 9 */
	{
		{7, 1, 0},
		{31, 2, 0},
		{7, 6, 0},
		{31, 18, 0},
	},
	/* UC 10 */
	{
		{7, 1, 0},
		{31, 2, 0},
		{63, 12, 31},
	},
	/* UC 11 */
	{
		{7, 1, 0},
		{31, 2, 0},
		{63, 12, 31},
		{7, 6, 0},
		{31, 18, 0},
		{63, 13, 31},
	},
	/* UC 12 */
	{
		{7, 1, 0},
		{63, 12, 31},
	},
	/* UC 13 */
	{
		{7, 1, 0},
		{63, 12, 31},
		{7, 6, 0},
		{63, 13, 31},
	},
	/* UC 14 */
	{
		{7, 1, 0},
		{63, 12, 31},
		{15, 7, 0},
	},
	/* UC 15 */
	{
		{7, 1, 0},
		{63, 12, 31},
		{15, 7, 0},
		{7, 6, 0},
		{63, 13, 31},
		{15, 10, 0},
	},
	/* UC 16 */
	{
		{7, 1, 0},
		{15, 7, 0},
	},
	/* UC 17 */
	{
		{7, 1, 0},
		{15, 7, 0},
		{7, 6, 0},
		{15, 10, 0},
	},
	/* UC 18 */
	{
		{7, 1, 0},
		{31, 2, 0},
		{15, 7, 0},
	},
	/* UC 19 */
	{
		{7, 1, 0},
		{31, 2, 0},
		{15, 7, 0},
		{7, 6, 0},
		{31, 18, 0},
		{15, 10, 0},
	},
};

enum {
	SWR_NOT_PRESENT, /* Device is detached/not present on the bus */
	SWR_ATTACHED_OK, /* Device is attached */
	SWR_ALERT,       /* Device alters master for any interrupts */
	SWR_RESERVED,    /* Reserved */
};

#define SWRM_MAX_PORT_REG    40
#define SWRM_MAX_INIT_REG    8

#define SWR_MSTR_MAX_REG_ADDR	0x1740
#define SWR_MSTR_START_REG_ADDR	0x00
#define SWR_MSTR_MAX_BUF_LEN     32
#define BYTES_PER_LINE          12
#define SWR_MSTR_RD_BUF_LEN      8
#define SWR_MSTR_WR_BUF_LEN      32

static void swrm_copy_data_port_config(struct swr_master *master,
				       u8 inactive_bank);
static struct swr_mstr_ctrl *dbgswrm;
static struct dentry *debugfs_swrm_dent;
static struct dentry *debugfs_peek;
static struct dentry *debugfs_poke;
static struct dentry *debugfs_reg_dump;
static unsigned int read_data;


static bool swrm_is_msm_variant(int val)
{
	return (val == SWRM_VERSION_1_3);
}

static int swrm_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static int get_parameters(char *buf, u32 *param1, int num_of_par)
{
	char *token;
	int base, cnt;

	token = strsep(&buf, " ");
	for (cnt = 0; cnt < num_of_par; cnt++) {
		if (token) {
			if ((token[1] == 'x') || (token[1] == 'X'))
				base = 16;
			else
				base = 10;

			if (kstrtou32(token, base, &param1[cnt]) != 0)
				return -EINVAL;

			token = strsep(&buf, " ");
		} else
			return -EINVAL;
	}
	return 0;
}

static ssize_t swrm_reg_show(char __user *ubuf, size_t count,
					  loff_t *ppos)
{
	int i, reg_val, len;
	ssize_t total = 0;
	char tmp_buf[SWR_MSTR_MAX_BUF_LEN];

	if (!ubuf || !ppos)
		return 0;

	for (i = (((int) *ppos / BYTES_PER_LINE) + SWR_MSTR_START_REG_ADDR);
		i <= SWR_MSTR_MAX_REG_ADDR; i += 4) {
		reg_val = dbgswrm->read(dbgswrm->handle, i);
		len = snprintf(tmp_buf, 25, "0x%.3x: 0x%.2x\n", i, reg_val);
		if ((total + len) >= count - 1)
			break;
		if (copy_to_user((ubuf + total), tmp_buf, len)) {
			pr_err("%s: fail to copy reg dump\n", __func__);
			total = -EFAULT;
			goto copy_err;
		}
		*ppos += len;
		total += len;
	}

copy_err:
	return total;
}

static ssize_t swrm_debug_read(struct file *file, char __user *ubuf,
				size_t count, loff_t *ppos)
{
	char lbuf[SWR_MSTR_RD_BUF_LEN];
	char *access_str;
	ssize_t ret_cnt;

	if (!count || !file || !ppos || !ubuf)
		return -EINVAL;

	access_str = file->private_data;
	if (*ppos < 0)
		return -EINVAL;

	if (!strcmp(access_str, "swrm_peek")) {
		snprintf(lbuf, sizeof(lbuf), "0x%x\n", read_data);
		ret_cnt = simple_read_from_buffer(ubuf, count, ppos, lbuf,
					       strnlen(lbuf, 7));
	} else if (!strcmp(access_str, "swrm_reg_dump")) {
		ret_cnt = swrm_reg_show(ubuf, count, ppos);
	} else {
		pr_err("%s: %s not permitted to read\n", __func__, access_str);
		ret_cnt = -EPERM;
	}
	return ret_cnt;
}

static ssize_t swrm_debug_write(struct file *filp,
	const char __user *ubuf, size_t cnt, loff_t *ppos)
{
	char lbuf[SWR_MSTR_WR_BUF_LEN];
	int rc;
	u32 param[5];
	char *access_str;

	if (!filp || !ppos || !ubuf)
		return -EINVAL;

	access_str = filp->private_data;
	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';
	if (!strcmp(access_str, "swrm_poke")) {
		/* write */
		rc = get_parameters(lbuf, param, 2);
		if ((param[0] <= SWR_MSTR_MAX_REG_ADDR) &&
			(param[1] <= 0xFFFFFFFF) &&
			(rc == 0))
			rc = dbgswrm->write(dbgswrm->handle, param[0],
					    param[1]);
		else
			rc = -EINVAL;
	} else if (!strcmp(access_str, "swrm_peek")) {
		/* read */
		rc = get_parameters(lbuf, param, 1);
		if ((param[0] <= SWR_MSTR_MAX_REG_ADDR) && (rc == 0))
			read_data = dbgswrm->read(dbgswrm->handle, param[0]);
		else
			rc = -EINVAL;
	}
	if (rc == 0)
		rc = cnt;
	else
		pr_err("%s: rc = %d\n", __func__, rc);

	return rc;
}

static const struct file_operations swrm_debug_ops = {
	.open = swrm_debug_open,
	.write = swrm_debug_write,
	.read = swrm_debug_read,
};

static int swrm_set_ch_map(struct swr_mstr_ctrl *swrm, void *data)
{
	struct swr_mstr_port *pinfo = (struct swr_mstr_port *)data;

	swrm->mstr_port = kzalloc(sizeof(struct swr_mstr_port), GFP_KERNEL);
	if (swrm->mstr_port == NULL)
		return -ENOMEM;
	swrm->mstr_port->num_port = pinfo->num_port;
	swrm->mstr_port->port = kzalloc((pinfo->num_port * sizeof(u8)),
					GFP_KERNEL);
	if (!swrm->mstr_port->port) {
		kfree(swrm->mstr_port);
		swrm->mstr_port = NULL;
		return -ENOMEM;
	}
	memcpy(swrm->mstr_port->port, pinfo->port, pinfo->num_port);
	return 0;
}

static bool swrm_is_port_en(struct swr_master *mstr)
{
	return !!(mstr->num_port);
}

static int swrm_clk_request(struct swr_mstr_ctrl *swrm, bool enable)
{
	if (!swrm->clk || !swrm->handle)
		return -EINVAL;

	if (enable) {
		swrm->clk_ref_count++;
		if (swrm->clk_ref_count == 1) {
			swrm->clk(swrm->handle, true);
			swrm->state = SWR_MSTR_UP;
		}
	} else if (--swrm->clk_ref_count == 0) {
		swrm->clk(swrm->handle, false);
		swrm->state = SWR_MSTR_DOWN;
	} else if (swrm->clk_ref_count < 0) {
		pr_err("%s: swrm clk count mismatch\n", __func__);
		swrm->clk_ref_count = 0;
	}
	return 0;
}

static int swrm_get_port_config(struct swr_master *master)
{
	u32 ch_rate = 0;
	u32 num_ch = 0;
	int i, uc_idx;
	u32 portcount = 0;

	for (i = 0; i < SWR_MSTR_PORT_LEN; i++) {
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
	for (i = 0; i < SWR_MSTR_PORT_LEN; i++) {
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

static u32 swrm_get_packed_reg_val(u8 *cmd_id, u8 cmd_data,
				 u8 dev_addr, u16 reg_addr)
{
	u32 val;
	u8 id = *cmd_id;

	if (id != SWR_BROADCAST_CMD_ID) {
		if (id < 14)
			id += 1;
		else
			id = 0;
		*cmd_id = id;
	}
	val = SWR_REG_VAL_PACK(cmd_data, dev_addr, id, reg_addr);

	return val;
}

static int swrm_cmd_fifo_rd_cmd(struct swr_mstr_ctrl *swrm, int *cmd_data,
				 u8 dev_addr, u8 cmd_id, u16 reg_addr,
				 u32 len)
{
	u32 val;
	int ret = 0;

	val = swrm_get_packed_reg_val(&swrm->rcmd_id, len, dev_addr, reg_addr);
	ret = swrm->write(swrm->handle, SWRM_CMD_FIFO_RD_CMD, val);
	if (ret < 0) {
		dev_err(swrm->dev, "%s: reg 0x%x write failed, err:%d\n",
			__func__, val, ret);
		goto err;
	}
	*cmd_data = swrm->read(swrm->handle, SWRM_CMD_FIFO_RD_FIFO_ADDR);
	dev_dbg(swrm->dev,
		"%s: reg: 0x%x, cmd_id: 0x%x, dev_id: 0x%x, cmd_data: 0x%x\n",
		__func__, reg_addr, cmd_id, dev_addr, *cmd_data);
err:
	return ret;
}

static int swrm_cmd_fifo_wr_cmd(struct swr_mstr_ctrl *swrm, u8 cmd_data,
				 u8 dev_addr, u8 cmd_id, u16 reg_addr)
{
	u32 val;
	int ret = 0;

	if (!cmd_id)
		val = swrm_get_packed_reg_val(&swrm->wcmd_id, cmd_data,
					      dev_addr, reg_addr);
	else
		val = swrm_get_packed_reg_val(&cmd_id, cmd_data,
					      dev_addr, reg_addr);

	dev_dbg(swrm->dev,
		"%s: reg: 0x%x, cmd_id: 0x%x, dev_id: 0x%x, cmd_data: 0x%x\n",
		__func__, reg_addr, cmd_id, dev_addr, cmd_data);
	ret = swrm->write(swrm->handle, SWRM_CMD_FIFO_WR_CMD, val);
	if (ret < 0) {
		dev_err(swrm->dev, "%s: reg 0x%x write failed, err:%d\n",
			__func__, val, ret);
		goto err;
	}
	if (cmd_id == 0xF) {
		/*
		 * sleep for 10ms for MSM soundwire variant to allow broadcast
		 * command to complete.
		 */
		if (swrm_is_msm_variant(swrm->version))
			usleep_range(10000, 10100);
		else
			wait_for_completion_timeout(&swrm->broadcast,
						    (2 * HZ/10));
	}
err:
	return ret;
}

static int swrm_read(struct swr_master *master, u8 dev_num, u16 reg_addr,
		     void *buf, u32 len)
{
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	int ret = 0;
	int val;
	u8 *reg_val = (u8 *)buf;

	if (!swrm) {
		dev_err(&master->dev, "%s: swrm is NULL\n", __func__);
		return -EINVAL;
	}

	if (dev_num)
		ret = swrm_cmd_fifo_rd_cmd(swrm, &val, dev_num, 0, reg_addr,
					   len);
	else
		val = swrm->read(swrm->handle, reg_addr);

	if (!ret)
		*reg_val = (u8)val;

	pm_runtime_mark_last_busy(&swrm->pdev->dev);

	return ret;
}

static int swrm_write(struct swr_master *master, u8 dev_num, u16 reg_addr,
		      const void *buf)
{
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	int ret = 0;
	u8 reg_val = *(u8 *)buf;

	if (!swrm) {
		dev_err(&master->dev, "%s: swrm is NULL\n", __func__);
		return -EINVAL;
	}

	if (dev_num)
		ret = swrm_cmd_fifo_wr_cmd(swrm, reg_val, dev_num, 0, reg_addr);
	else
		ret = swrm->write(swrm->handle, reg_addr, reg_val);

	pm_runtime_mark_last_busy(&swrm->pdev->dev);

	return ret;
}

static int swrm_bulk_write(struct swr_master *master, u8 dev_num, void *reg,
			   const void *buf, size_t len)
{
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	int ret = 0;
	int i;
	u32 *val;
	u32 *swr_fifo_reg;

	if (!swrm || !swrm->handle) {
		dev_err(&master->dev, "%s: swrm is NULL\n", __func__);
		return -EINVAL;
	}
	if (len <= 0)
		return -EINVAL;

	if (dev_num) {
		swr_fifo_reg = kcalloc(len, sizeof(u32), GFP_KERNEL);
		if (!swr_fifo_reg) {
			ret = -ENOMEM;
			goto err;
		}
		val = kcalloc(len, sizeof(u32), GFP_KERNEL);
		if (!val) {
			ret = -ENOMEM;
			goto mem_fail;
		}

		for (i = 0; i < len; i++) {
			val[i] = swrm_get_packed_reg_val(&swrm->wcmd_id,
							 ((u8 *)buf)[i],
							 dev_num,
							 ((u16 *)reg)[i]);
			swr_fifo_reg[i] = SWRM_CMD_FIFO_WR_CMD;
		}
		ret = swrm->bulk_write(swrm->handle, swr_fifo_reg, val, len);
		if (ret) {
			dev_err(&master->dev, "%s: bulk write failed\n",
				__func__);
			ret = -EINVAL;
		}
	} else {
		dev_err(&master->dev,
			"%s: No support of Bulk write for master regs\n",
			__func__);
		ret = -EINVAL;
		goto err;
	}
	kfree(val);
mem_fail:
	kfree(swr_fifo_reg);
err:
	pm_runtime_mark_last_busy(&swrm->pdev->dev);
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
	/* apply div2 setting for inactive bank before bank switch */
	swrm_cmd_fifo_wr_cmd(swrm, 0x01, 0xF, 0x00,
			SWRS_SCP_HOST_CLK_DIV2_CTL_BANK(bank));

	swrm_cmd_fifo_wr_cmd(swrm, ((row << 3) | col), 0xF, 0xF,
			SWRS_SCP_FRAME_CTRL_BANK(bank));
}

static struct swr_port_info *swrm_get_port(struct swr_master *master,
					   u8 port_id)
{
	int i;
	struct swr_port_info *port = NULL;

	for (i = 0; i < SWR_MSTR_PORT_LEN; i++) {
		port = &master->port[i];
		if (port->port_id == port_id) {
			dev_dbg(&master->dev, "%s: port_id: %d, index: %d\n",
				__func__, port_id, i);
			return port;
		}
	}

	return NULL;
}

static struct swr_port_info *swrm_get_avail_port(struct swr_master *master)
{
	int i;
	struct swr_port_info *port = NULL;

	for (i = 0; i < SWR_MSTR_PORT_LEN; i++) {
		port = &master->port[i];
		if (port->port_en)
			continue;

		dev_dbg(&master->dev, "%s: port_id: %d, index: %d\n",
			__func__, port->port_id, i);
		return port;
	}

	return NULL;
}

static struct swr_port_info *swrm_get_enabled_port(struct swr_master *master,
						   u8 port_id)
{
	int i;
	struct swr_port_info *port = NULL;

	for (i = 0; i < SWR_MSTR_PORT_LEN; i++) {
		port = &master->port[i];
		if ((port->port_id == port_id) && (port->port_en == true))
			break;
	}
	if (i == SWR_MSTR_PORT_LEN)
		port = NULL;
	return port;
}

static bool swrm_remove_from_group(struct swr_master *master)
{
	struct swr_device *swr_dev;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	bool is_removed = false;

	if (!swrm)
		goto end;

	mutex_lock(&swrm->mlock);
	if ((swrm->num_rx_chs > 1) &&
	    (swrm->num_rx_chs == swrm->num_cfg_devs)) {
		list_for_each_entry(swr_dev, &master->devices,
				dev_list) {
			swr_dev->group_id = SWR_GROUP_NONE;
			master->gr_sid = 0;
		}
		is_removed = true;
	}
	mutex_unlock(&swrm->mlock);

end:
	return is_removed;
}

static void swrm_cleanup_disabled_data_ports(struct swr_master *master,
					     u8 bank)
{
	u32 value;
	struct swr_port_info *port;
	int i;
	int port_type;
	struct swrm_mports *mport, *mport_next = NULL;
	int port_disable_cnt = 0;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);

	if (!swrm) {
		pr_err("%s: swrm is null\n", __func__);
		return;
	}

	dev_dbg(swrm->dev, "%s: master num_port: %d\n", __func__,
		master->num_port);

	mport = list_first_entry_or_null(&swrm->mport_list,
					struct swrm_mports,
					list);
	if (!mport) {
		dev_err(swrm->dev, "%s: list is empty\n", __func__);
		return;
	}

	for (i = 0; i < master->num_port; i++) {
		port = swrm_get_port(master, mstr_ports[mport->id]);
		if (!port || port->ch_en)
			goto inc_loop;

		port_disable_cnt++;
		port_type = mstr_port_type[mport->id];
		value = ((port->ch_en)
				<< SWRM_DP_PORT_CTRL_EN_CHAN_SHFT);
		value |= ((port->offset2)
				<< SWRM_DP_PORT_CTRL_OFFSET2_SHFT);
		value |= ((port->offset1)
				<< SWRM_DP_PORT_CTRL_OFFSET1_SHFT);
		value |= port->sinterval;

		swrm->write(swrm->handle,
			    SWRM_DP_PORT_CTRL_BANK((mport->id+1), bank),
			    value);
		swrm_cmd_fifo_wr_cmd(swrm, 0x00, port->dev_id, 0x00,
				SWRS_DP_CHANNEL_ENABLE_BANK(port_type, bank));

		dev_dbg(swrm->dev, "%s: mport :%d, reg: 0x%x, val: 0x%x\n",
			__func__, mport->id,
			(SWRM_DP_PORT_CTRL_BANK((mport->id+1), bank)), value);

inc_loop:
		mport_next = list_next_entry(mport, list);
		if (port && !port->ch_en) {
			list_del(&mport->list);
			kfree(mport);
		}
		if (!mport_next) {
			dev_err(swrm->dev, "%s: end of list\n", __func__);
			break;
		}
		mport = mport_next;
	}
	master->num_port -= port_disable_cnt;

	dev_dbg(swrm->dev, "%s:disable ports: %d, active ports (rem): %d\n",
		__func__, port_disable_cnt,  master->num_port);
}

static void swrm_slvdev_datapath_control(struct swr_master *master,
					 bool enable)
{
	u8 bank;
	u32 value, n_col;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	int mask = (SWRM_MCP_FRAME_CTRL_BANK_ROW_CTRL_BMSK |
		    SWRM_MCP_FRAME_CTRL_BANK_COL_CTRL_BMSK |
		    SWRM_MCP_FRAME_CTRL_BANK_SSP_PERIOD_BMSK);
	u8 inactive_bank;

	if (!swrm) {
		pr_err("%s: swrm is null\n", __func__);
		return;
	}

	bank = get_inactive_bank_num(swrm);

	dev_dbg(swrm->dev, "%s: enable: %d, cfg_devs: %d\n",
		__func__, enable, swrm->num_cfg_devs);

	if (enable) {
		/* set Row = 48 and col = 16 */
		n_col = SWR_MAX_COL;
	} else {
		/*
		 * Do not change to 48x2 if number of channels configured
		 * as stereo and if disable datapath is called for the
		 * first slave device
		 */
		if (swrm->num_cfg_devs > 0)
			n_col = SWR_MAX_COL;
		else
			n_col = SWR_MIN_COL;

		/*
		 * All ports are already disabled, no need to perform
		 * bank-switch and copy operation. This case can arise
		 * when speaker channels are enabled in stereo mode with
		 * BROADCAST and disabled in GROUP_NONE
		 */
		if (master->num_port == 0)
			return;
	}

	value = swrm->read(swrm->handle, SWRM_MCP_FRAME_CTRL_BANK_ADDR(bank));
	value &= (~mask);
	value |= ((0 << SWRM_MCP_FRAME_CTRL_BANK_ROW_CTRL_SHFT) |
		  (n_col << SWRM_MCP_FRAME_CTRL_BANK_COL_CTRL_SHFT) |
		  (0 << SWRM_MCP_FRAME_CTRL_BANK_SSP_PERIOD_SHFT));
	swrm->write(swrm->handle, SWRM_MCP_FRAME_CTRL_BANK_ADDR(bank), value);

	dev_dbg(swrm->dev, "%s: regaddr: 0x%x, value: 0x%x\n", __func__,
		SWRM_MCP_FRAME_CTRL_BANK_ADDR(bank), value);

	enable_bank_switch(swrm, bank, SWR_MAX_ROW, n_col);

	inactive_bank = bank ? 0 : 1;
	if (enable)
		swrm_copy_data_port_config(master, inactive_bank);
	else
		swrm_cleanup_disabled_data_ports(master, inactive_bank);

	if (!swrm_is_port_en(master)) {
		dev_dbg(&master->dev, "%s: pm_runtime auto suspend triggered\n",
			__func__);
		pm_runtime_mark_last_busy(&swrm->pdev->dev);
		pm_runtime_put_autosuspend(&swrm->pdev->dev);
	}
}

static void swrm_apply_port_config(struct swr_master *master)
{
	u8 bank;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);

	if (!swrm) {
		pr_err("%s: Invalid handle to swr controller\n",
			__func__);
		return;
	}

	bank = get_inactive_bank_num(swrm);
	dev_dbg(swrm->dev, "%s: enter bank: %d master_ports: %d\n",
		__func__, bank, master->num_port);

	swrm_copy_data_port_config(master, bank);
}

static void swrm_copy_data_port_config(struct swr_master *master, u8 bank)
{
	u32 value;
	struct swr_port_info *port;
	int i;
	int port_type;
	struct swrm_mports *mport;
	u32 reg[SWRM_MAX_PORT_REG];
	u32 val[SWRM_MAX_PORT_REG];
	int len = 0;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);

	if (!swrm) {
		pr_err("%s: swrm is null\n", __func__);
		return;
	}

	dev_dbg(swrm->dev, "%s: master num_port: %d\n", __func__,
		master->num_port);

	mport = list_first_entry_or_null(&swrm->mport_list,
					struct swrm_mports,
					list);
	if (!mport) {
		dev_err(swrm->dev, "%s: list is empty\n", __func__);
		return;
	}
	for (i = 0; i < master->num_port; i++) {

		port = swrm_get_enabled_port(master, mstr_ports[mport->id]);
		if (!port)
			continue;
		port_type = mstr_port_type[mport->id];
		if (!port->dev_id || (port->dev_id > master->num_dev)) {
			dev_dbg(swrm->dev, "%s: invalid device id = %d\n",
				__func__, port->dev_id);
			continue;
		}
		value = ((port->ch_en)
				<< SWRM_DP_PORT_CTRL_EN_CHAN_SHFT);
		value |= ((port->offset2)
				<< SWRM_DP_PORT_CTRL_OFFSET2_SHFT);
		value |= ((port->offset1)
				<< SWRM_DP_PORT_CTRL_OFFSET1_SHFT);
		value |= port->sinterval;

		reg[len] = SWRM_DP_PORT_CTRL_BANK((mport->id+1), bank);
		val[len++] = value;

		dev_dbg(swrm->dev, "%s: mport :%d, reg: 0x%x, val: 0x%x\n",
			__func__, mport->id,
			(SWRM_DP_PORT_CTRL_BANK((mport->id+1), bank)), value);

		reg[len] = SWRM_CMD_FIFO_WR_CMD;
		val[len++] = SWR_REG_VAL_PACK(port->ch_en, port->dev_id, 0x00,
				SWRS_DP_CHANNEL_ENABLE_BANK(port_type, bank));

		reg[len] = SWRM_CMD_FIFO_WR_CMD;
		val[len++] = SWR_REG_VAL_PACK(port->sinterval,
				port->dev_id, 0x00,
				SWRS_DP_SAMPLE_CONTROL_1_BANK(port_type, bank));

		reg[len] = SWRM_CMD_FIFO_WR_CMD;
		val[len++] = SWR_REG_VAL_PACK(port->offset1,
				port->dev_id, 0x00,
				SWRS_DP_OFFSET_CONTROL_1_BANK(port_type, bank));

		if (port_type != 0) {
			reg[len] = SWRM_CMD_FIFO_WR_CMD;
			val[len++] = SWR_REG_VAL_PACK(port->offset2,
					port->dev_id, 0x00,
					SWRS_DP_OFFSET_CONTROL_2_BANK(port_type,
									bank));
		}
		mport = list_next_entry(mport, list);
		if (!mport) {
			dev_err(swrm->dev, "%s: end of list\n", __func__);
			break;
		}
	}
	swrm->bulk_write(swrm->handle, reg, val, len);
}

static int swrm_connect_port(struct swr_master *master,
			struct swr_params *portinfo)
{
	int i;
	struct swr_port_info *port;
	int ret = 0;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);
	struct swrm_mports *mport;
	struct list_head *ptr, *next;

	dev_dbg(&master->dev, "%s: enter\n", __func__);
	if (!portinfo)
		return -EINVAL;

	if (!swrm) {
		dev_err(&master->dev,
			"%s: Invalid handle to swr controller\n",
			__func__);
		return -EINVAL;
	}

	mutex_lock(&swrm->mlock);
	if (!swrm_is_port_en(master))
		pm_runtime_get_sync(&swrm->pdev->dev);

	for (i = 0; i < portinfo->num_port; i++) {
		mport = kzalloc(sizeof(struct swrm_mports), GFP_KERNEL);
		if (!mport) {
			ret = -ENOMEM;
			goto mem_fail;
		}
		ret = swrm_get_master_port(&mport->id,
						portinfo->port_id[i]);
		if (ret < 0) {
			dev_err(&master->dev,
				"%s: mstr portid for slv port %d not found\n",
				__func__, portinfo->port_id[i]);
			goto port_fail;
		}
		port = swrm_get_avail_port(master);
		if (!port) {
			dev_err(&master->dev,
				"%s: avail ports not found!\n", __func__);
			goto port_fail;
		}
		list_add(&mport->list, &swrm->mport_list);
		port->dev_id = portinfo->dev_id;
		port->port_id = portinfo->port_id[i];
		port->num_ch = portinfo->num_ch[i];
		port->ch_rate = portinfo->ch_rate[i];
		port->ch_en = portinfo->ch_en[i];
		port->port_en = true;
		dev_dbg(&master->dev,
			"%s: mstr port %d, slv port %d ch_rate %d num_ch %d\n",
			__func__, mport->id, port->port_id, port->ch_rate,
			port->num_ch);
	}
	master->num_port += portinfo->num_port;
	if (master->num_port >= SWR_MSTR_PORT_LEN)
		master->num_port = SWR_MSTR_PORT_LEN;

	swrm_get_port_config(master);
	swr_port_response(master, portinfo->tid);
	swrm->num_cfg_devs += 1;
	dev_dbg(&master->dev, "%s: cfg_devs: %d, rx_chs: %d\n",
		__func__, swrm->num_cfg_devs, swrm->num_rx_chs);
	if (swrm->num_rx_chs > 1) {
		if (swrm->num_rx_chs == swrm->num_cfg_devs)
			swrm_apply_port_config(master);
	} else {
		swrm_apply_port_config(master);
	}
	mutex_unlock(&swrm->mlock);
	return 0;

port_fail:
	kfree(mport);
mem_fail:
	list_for_each_safe(ptr, next, &swrm->mport_list) {
		mport = list_entry(ptr, struct swrm_mports, list);
		for (i = 0; i < portinfo->num_port; i++) {
			if (portinfo->port_id[i] == mstr_ports[mport->id]) {
				port = swrm_get_port(master,
						portinfo->port_id[i]);
				if (port)
					port->ch_en = false;
				list_del(&mport->list);
				kfree(mport);
				break;
			}
		}
	}
	mutex_unlock(&swrm->mlock);
	return ret;
}

static int swrm_disconnect_port(struct swr_master *master,
			struct swr_params *portinfo)
{
	int i;
	struct swr_port_info *port;
	u8 bank;
	u32 value;
	int ret = 0;
	u8 mport_id = 0;
	int port_type = 0;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(master);

	if (!swrm) {
		dev_err(&master->dev,
			"%s: Invalid handle to swr controller\n",
			__func__);
		return -EINVAL;
	}

	if (!portinfo) {
		dev_err(&master->dev, "%s: portinfo is NULL\n", __func__);
		return -EINVAL;
	}
	mutex_lock(&swrm->mlock);
	bank = get_inactive_bank_num(swrm);
	for (i = 0; i < portinfo->num_port; i++) {
		ret = swrm_get_master_port(&mport_id,
						portinfo->port_id[i]);
		if (ret < 0) {
			dev_err(&master->dev,
				"%s: mstr portid for slv port %d not found\n",
				__func__, portinfo->port_id[i]);
			mutex_unlock(&swrm->mlock);
			return -EINVAL;
		}
		port = swrm_get_enabled_port(master, portinfo->port_id[i]);
		if (!port) {
			dev_dbg(&master->dev, "%s: port %d already disabled\n",
				__func__, portinfo->port_id[i]);
			continue;
		}
		port_type = mstr_port_type[mport_id];
		port->dev_id = portinfo->dev_id;
		port->port_en = false;
		port->ch_en = 0;
		value = port->ch_en << SWRM_DP_PORT_CTRL_EN_CHAN_SHFT;
		value |= (port->offset2 << SWRM_DP_PORT_CTRL_OFFSET2_SHFT);
		value |= (port->offset1 << SWRM_DP_PORT_CTRL_OFFSET1_SHFT);
		value |= port->sinterval;


		swrm->write(swrm->handle,
			    SWRM_DP_PORT_CTRL_BANK((mport_id+1), bank),
			    value);
		swrm_cmd_fifo_wr_cmd(swrm, 0x00, port->dev_id, 0x00,
				SWRS_DP_CHANNEL_ENABLE_BANK(port_type, bank));
	}

	swr_port_response(master, portinfo->tid);
	swrm->num_cfg_devs -= 1;
	dev_dbg(&master->dev, "%s: cfg_devs: %d, rx_chs: %d, active ports: %d\n",
		__func__, swrm->num_cfg_devs, swrm->num_rx_chs,
		master->num_port);
	mutex_unlock(&swrm->mlock);

	return 0;
}

static int swrm_check_slave_change_status(struct swr_mstr_ctrl *swrm,
					int status, u8 *devnum)
{
	int i;
	int new_sts = status;
	int ret = SWR_NOT_PRESENT;

	if (status != swrm->slave_status) {
		for (i = 0; i < (swrm->master.num_dev + 1); i++) {
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
	u32 value, intr_sts;
	int status, chg_sts, i;
	u8 devnum = 0;
	int ret = IRQ_HANDLED;

	mutex_lock(&swrm->reslock);
	swrm_clk_request(swrm, true);
	mutex_unlock(&swrm->reslock);

	intr_sts = swrm->read(swrm->handle, SWRM_INTERRUPT_STATUS);
	intr_sts &= SWRM_INTERRUPT_STATUS_RMSK;
	for (i = 0; i < SWRM_INTERRUPT_MAX; i++) {
		value = intr_sts & (1 << i);
		if (!value)
			continue;

		swrm->write(swrm->handle, SWRM_INTERRUPT_CLEAR, value);
		switch (value) {
		case SWRM_INTERRUPT_STATUS_SLAVE_PEND_IRQ:
			dev_dbg(swrm->dev, "SWR slave pend irq\n");
			break;
		case SWRM_INTERRUPT_STATUS_NEW_SLAVE_ATTACHED:
			dev_dbg(swrm->dev, "SWR new slave attached\n");
			break;
		case SWRM_INTERRUPT_STATUS_CHANGE_ENUM_SLAVE_STATUS:
			status = swrm->read(swrm->handle, SWRM_MCP_SLV_STATUS);
			if (status == swrm->slave_status) {
				dev_dbg(swrm->dev,
					"%s: No change in slave status: %d\n",
					__func__, status);
				break;
			}
			chg_sts = swrm_check_slave_change_status(swrm, status,
								&devnum);
			switch (chg_sts) {
			case SWR_NOT_PRESENT:
				dev_dbg(swrm->dev, "device %d got detached\n",
					devnum);
				break;
			case SWR_ATTACHED_OK:
				dev_dbg(swrm->dev, "device %d got attached\n",
					devnum);
				break;
			case SWR_ALERT:
				dev_dbg(swrm->dev,
					"device %d has pending interrupt\n",
					devnum);
				break;
			}
			break;
		case SWRM_INTERRUPT_STATUS_MASTER_CLASH_DET:
			dev_err_ratelimited(swrm->dev, "SWR bus clash detected\n");
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
			value = swrm->read(swrm->handle, SWRM_CMD_FIFO_STATUS);
			dev_err_ratelimited(swrm->dev,
			"SWR CMD error, fifo status 0x%x, flushing fifo\n",
					    value);
			swrm->write(swrm->handle, SWRM_CMD_FIFO_CMD, 0x1);
			break;
		case SWRM_INTERRUPT_STATUS_DOUT_PORT_COLLISION:
			dev_dbg(swrm->dev, "SWR Port collision detected\n");
			break;
		case SWRM_INTERRUPT_STATUS_READ_EN_RD_VALID_MISMATCH:
			dev_dbg(swrm->dev, "SWR read enable valid mismatch\n");
			break;
		case SWRM_INTERRUPT_STATUS_SPECIAL_CMD_ID_FINISHED:
			complete(&swrm->broadcast);
			dev_dbg(swrm->dev, "SWR cmd id finished\n");
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
			dev_err_ratelimited(swrm->dev, "SWR unknown interrupt\n");
			ret = IRQ_NONE;
			break;
		}
	}

	mutex_lock(&swrm->reslock);
	swrm_clk_request(swrm, false);
	mutex_unlock(&swrm->reslock);
	return ret;
}

static int swrm_get_device_status(struct swr_mstr_ctrl *swrm, u8 devnum)
{
	u32 val;

	swrm->slave_status = swrm->read(swrm->handle, SWRM_MCP_SLV_STATUS);
	val = (swrm->slave_status >> (devnum * 2));
	val &= SWRM_MCP_SLV_STATUS_MASK;
	return val;
}

static int swrm_get_logical_dev_num(struct swr_master *mstr, u64 dev_id,
				u8 *dev_num)
{
	int i;
	u64 id = 0;
	int ret = -EINVAL;
	struct swr_mstr_ctrl *swrm = swr_get_ctrl_data(mstr);
	struct swr_device *swr_dev;
	u32 num_dev = 0;

	if (!swrm) {
		pr_err("%s: Invalid handle to swr controller\n",
			__func__);
		return ret;
	}
	if (swrm->num_dev)
		num_dev = swrm->num_dev;
	else
		num_dev = mstr->num_dev;

	pm_runtime_get_sync(&swrm->pdev->dev);
	for (i = 1; i < (num_dev + 1); i++) {
		id = ((u64)(swrm->read(swrm->handle,
			    SWRM_ENUMERATOR_SLAVE_DEV_ID_2(i))) << 32);
		id |= swrm->read(swrm->handle,
			    SWRM_ENUMERATOR_SLAVE_DEV_ID_1(i));
		/*
		 * As pm_runtime_get_sync() brings all slaves out of reset
		 * update logical device number for all slaves.
		 */
		list_for_each_entry(swr_dev, &mstr->devices, dev_list) {
			if (swr_dev->addr == (id & SWR_DEV_ID_MASK)) {
				u32 status = swrm_get_device_status(swrm, i);

				if ((status == 0x01) || (status == 0x02)) {
					swr_dev->dev_num = i;
					if ((id & SWR_DEV_ID_MASK) == dev_id) {
						*dev_num = i;
						ret = 0;
					}
					dev_dbg(swrm->dev, "%s: devnum %d is assigned for dev addr %lx\n",
						__func__, i, swr_dev->addr);
				}
			}
		}
	}
	if (ret)
		dev_err(swrm->dev, "%s: device 0x%llx is not ready\n",
			__func__, dev_id);

	pm_runtime_mark_last_busy(&swrm->pdev->dev);
	pm_runtime_put_autosuspend(&swrm->pdev->dev);
	return ret;
}
static int swrm_master_init(struct swr_mstr_ctrl *swrm)
{
	int ret = 0;
	u32 val;
	u8 row_ctrl = SWR_MAX_ROW;
	u8 col_ctrl = SWR_MIN_COL;
	u8 ssp_period = 1;
	u8 retry_cmd_num = 3;
	u32 reg[SWRM_MAX_INIT_REG];
	u32 value[SWRM_MAX_INIT_REG];
	int len = 0;

	/* Clear Rows and Cols */
	val = ((row_ctrl << SWRM_MCP_FRAME_CTRL_BANK_ROW_CTRL_SHFT) |
		(col_ctrl << SWRM_MCP_FRAME_CTRL_BANK_COL_CTRL_SHFT) |
		(ssp_period << SWRM_MCP_FRAME_CTRL_BANK_SSP_PERIOD_SHFT));

	reg[len] = SWRM_MCP_FRAME_CTRL_BANK_ADDR(0);
	value[len++] = val;

	/* Set Auto enumeration flag */
	reg[len] = SWRM_ENUMERATOR_CFG_ADDR;
	value[len++] = 1;

	/* Mask soundwire interrupts */
	reg[len] = SWRM_INTERRUPT_MASK_ADDR;
	value[len++] = 0x1FFFD;

	/* Configure No pings */
	val = swrm->read(swrm->handle, SWRM_MCP_CFG_ADDR);
	val &= ~SWRM_MCP_CFG_MAX_NUM_OF_CMD_NO_PINGS_BMSK;
	val |= (0x1f << SWRM_MCP_CFG_MAX_NUM_OF_CMD_NO_PINGS_SHFT);
	reg[len] = SWRM_MCP_CFG_ADDR;
	value[len++] = val;

	/* Configure number of retries of a read/write cmd */
	val = (retry_cmd_num << SWRM_CMD_FIFO_CFG_NUM_OF_CMD_RETRY_SHFT);
	reg[len] = SWRM_CMD_FIFO_CFG_ADDR;
	value[len++] = val;

	/* Set IRQ to PULSE */
	reg[len] = SWRM_COMP_CFG_ADDR;
	value[len++] = 0x02;

	reg[len] = SWRM_COMP_CFG_ADDR;
	value[len++] = 0x03;

	reg[len] = SWRM_INTERRUPT_CLEAR;
	value[len++] = 0x08;

	swrm->bulk_write(swrm->handle, reg, value, len);

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
		ret = -ENOMEM;
		goto err_memory_fail;
	}
	swrm->dev = &pdev->dev;
	swrm->pdev = pdev;
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
	swrm->bulk_write = pdata->bulk_write;
	if (!swrm->bulk_write) {
		dev_err(&pdev->dev, "%s: swrm->bulk_write is NULL\n",
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
	swrm->master.bulk_write = swrm_bulk_write;
	swrm->master.get_logical_dev_num = swrm_get_logical_dev_num;
	swrm->master.connect_port = swrm_connect_port;
	swrm->master.disconnect_port = swrm_disconnect_port;
	swrm->master.slvdev_datapath_control = swrm_slvdev_datapath_control;
	swrm->master.remove_from_group = swrm_remove_from_group;
	swrm->master.dev.parent = &pdev->dev;
	swrm->master.dev.of_node = pdev->dev.of_node;
	swrm->master.num_port = 0;
	swrm->num_enum_slaves = 0;
	swrm->rcmd_id = 0;
	swrm->wcmd_id = 0;
	swrm->slave_status = 0;
	swrm->num_rx_chs = 0;
	swrm->clk_ref_count = 0;
	swrm->state = SWR_MSTR_RESUME;
	init_completion(&swrm->reset);
	init_completion(&swrm->broadcast);
	mutex_init(&swrm->mlock);
	INIT_LIST_HEAD(&swrm->mport_list);
	mutex_init(&swrm->reslock);

	ret = of_property_read_u32(swrm->dev->of_node, "qcom,swr-num-dev",
				   &swrm->num_dev);
	if (ret)
		dev_dbg(&pdev->dev, "%s: Looking up %s property failed\n",
			__func__, "qcom,swr-num-dev");
	else {
		if (swrm->num_dev > SWR_MAX_SLAVE_DEVICES) {
			dev_err(&pdev->dev, "%s: num_dev %d > max limit %d\n",
				__func__, swrm->num_dev, SWR_MAX_SLAVE_DEVICES);
			ret = -EINVAL;
			goto err_pdata_fail;
		}
	}
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

	/* Add devices registered with board-info as the
	 * controller will be up now
	 */
	swr_master_add_boarddevices(&swrm->master);
	mutex_lock(&swrm->mlock);
	swrm_clk_request(swrm, true);
	ret = swrm_master_init(swrm);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"%s: Error in master Initializaiton, err %d\n",
			__func__, ret);
		mutex_unlock(&swrm->mlock);
		goto err_mstr_fail;
	}
	swrm->version = swrm->read(swrm->handle, SWRM_COMP_HW_VERSION);

	mutex_unlock(&swrm->mlock);

	if (pdev->dev.of_node)
		of_register_swr_devices(&swrm->master);

	dbgswrm = swrm;
	debugfs_swrm_dent = debugfs_create_dir(dev_name(&pdev->dev), 0);
	if (!IS_ERR(debugfs_swrm_dent)) {
		debugfs_peek = debugfs_create_file("swrm_peek",
				S_IFREG | 0444, debugfs_swrm_dent,
				(void *) "swrm_peek", &swrm_debug_ops);

		debugfs_poke = debugfs_create_file("swrm_poke",
				S_IFREG | 0444, debugfs_swrm_dent,
				(void *) "swrm_poke", &swrm_debug_ops);

		debugfs_reg_dump = debugfs_create_file("swrm_reg_dump",
				   S_IFREG | 0444, debugfs_swrm_dent,
				   (void *) "swrm_reg_dump",
				   &swrm_debug_ops);
	}
	pm_runtime_set_autosuspend_delay(&pdev->dev, auto_suspend_timer);
	pm_runtime_use_autosuspend(&pdev->dev);
	pm_runtime_set_active(&pdev->dev);
	pm_runtime_enable(&pdev->dev);
	pm_runtime_mark_last_busy(&pdev->dev);

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

	if (swrm->reg_irq)
		swrm->reg_irq(swrm->handle, swr_mstr_interrupt,
				swrm, SWR_IRQ_FREE);
	if (swrm->mstr_port) {
		kfree(swrm->mstr_port->port);
		swrm->mstr_port->port = NULL;
		kfree(swrm->mstr_port);
		swrm->mstr_port = NULL;
	}
	pm_runtime_disable(&pdev->dev);
	pm_runtime_set_suspended(&pdev->dev);
	swr_unregister_master(&swrm->master);
	mutex_destroy(&swrm->mlock);
	mutex_destroy(&swrm->reslock);
	kfree(swrm);
	return 0;
}

static int swrm_clk_pause(struct swr_mstr_ctrl *swrm)
{
	u32 val;

	dev_dbg(swrm->dev, "%s: state: %d\n", __func__, swrm->state);
	swrm->write(swrm->handle, SWRM_INTERRUPT_MASK_ADDR, 0x1FDFD);
	val = swrm->read(swrm->handle, SWRM_MCP_CFG_ADDR);
	val |= SWRM_MCP_CFG_BUS_CLK_PAUSE_BMSK;
	swrm->write(swrm->handle, SWRM_MCP_CFG_ADDR, val);
	swrm->state = SWR_MSTR_PAUSE;

	return 0;
}

#ifdef CONFIG_PM
static int swrm_runtime_resume(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct swr_mstr_ctrl *swrm = platform_get_drvdata(pdev);
	int ret = 0;
	struct swr_master *mstr = &swrm->master;
	struct swr_device *swr_dev;

	dev_dbg(dev, "%s: pm_runtime: resume, state:%d\n",
		__func__, swrm->state);
	mutex_lock(&swrm->reslock);
	if ((swrm->state == SWR_MSTR_PAUSE) ||
	    (swrm->state == SWR_MSTR_DOWN)) {
		if (swrm->state == SWR_MSTR_DOWN) {
			if (swrm_clk_request(swrm, true))
				goto exit;
		}
		list_for_each_entry(swr_dev, &mstr->devices, dev_list) {
			ret = swr_device_up(swr_dev);
			if (ret) {
				dev_err(dev,
					"%s: failed to wakeup swr dev %d\n",
					__func__, swr_dev->dev_num);
				swrm_clk_request(swrm, false);
				goto exit;
			}
		}
		swrm->write(swrm->handle, SWRM_COMP_SW_RESET, 0x01);
		swrm->write(swrm->handle, SWRM_COMP_SW_RESET, 0x01);
		swrm_master_init(swrm);
	}
exit:
	pm_runtime_set_autosuspend_delay(&pdev->dev, auto_suspend_timer);
	mutex_unlock(&swrm->reslock);
	return ret;
}

static int swrm_runtime_suspend(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct swr_mstr_ctrl *swrm = platform_get_drvdata(pdev);
	int ret = 0;
	struct swr_master *mstr = &swrm->master;
	struct swr_device *swr_dev;

	dev_dbg(dev, "%s: pm_runtime: suspend state: %d\n",
		__func__, swrm->state);
	mutex_lock(&swrm->reslock);
	if ((swrm->state == SWR_MSTR_RESUME) ||
	    (swrm->state == SWR_MSTR_UP)) {
		if (swrm_is_port_en(&swrm->master)) {
			dev_dbg(dev, "%s ports are enabled\n", __func__);
			ret = -EBUSY;
			goto exit;
		}
		swrm_clk_pause(swrm);
		swrm->write(swrm->handle, SWRM_COMP_CFG_ADDR, 0x00);
		list_for_each_entry(swr_dev, &mstr->devices, dev_list) {
			ret = swr_device_down(swr_dev);
			if (ret) {
				dev_err(dev,
					"%s: failed to shutdown swr dev %d\n",
					__func__, swr_dev->dev_num);
				goto exit;
			}
		}
		swrm_clk_request(swrm, false);
	}
exit:
	mutex_unlock(&swrm->reslock);
	return ret;
}
#endif /* CONFIG_PM */

static int swrm_device_down(struct device *dev)
{
	struct platform_device *pdev = to_platform_device(dev);
	struct swr_mstr_ctrl *swrm = platform_get_drvdata(pdev);
	int ret = 0;
	struct swr_master *mstr = &swrm->master;
	struct swr_device *swr_dev;

	dev_dbg(dev, "%s: swrm state: %d\n", __func__, swrm->state);
	mutex_lock(&swrm->reslock);
	if ((swrm->state == SWR_MSTR_RESUME) ||
	    (swrm->state == SWR_MSTR_UP)) {
		list_for_each_entry(swr_dev, &mstr->devices, dev_list) {
			ret = swr_device_down(swr_dev);
			if (ret)
				dev_err(dev,
					"%s: failed to shutdown swr dev %d\n",
					__func__, swr_dev->dev_num);
		}
		dev_dbg(dev, "%s: Shutting down SWRM\n", __func__);
		pm_runtime_disable(dev);
		pm_runtime_set_suspended(dev);
		pm_runtime_enable(dev);
		swrm_clk_request(swrm, false);
	}
	mutex_unlock(&swrm->reslock);
	return ret;
}

/**
 * swrm_wcd_notify - parent device can notify to soundwire master through
 * this function
 * @pdev: pointer to platform device structure
 * @id: command id from parent to the soundwire master
 * @data: data from parent device to soundwire master
 */
int swrm_wcd_notify(struct platform_device *pdev, u32 id, void *data)
{
	struct swr_mstr_ctrl *swrm;
	int ret = 0;
	struct swr_master *mstr;
	struct swr_device *swr_dev;

	if (!pdev) {
		pr_err("%s: pdev is NULL\n", __func__);
		return -EINVAL;
	}
	swrm = platform_get_drvdata(pdev);
	if (!swrm) {
		dev_err(&pdev->dev, "%s: swrm is NULL\n", __func__);
		return -EINVAL;
	}
	mstr = &swrm->master;

	switch (id) {
	case SWR_CH_MAP:
		if (!data) {
			dev_err(swrm->dev, "%s: data is NULL\n", __func__);
			ret = -EINVAL;
		} else {
			ret = swrm_set_ch_map(swrm, data);
		}
		break;
	case SWR_DEVICE_DOWN:
		dev_dbg(swrm->dev, "%s: swr master down called\n", __func__);
		mutex_lock(&swrm->mlock);
		if ((swrm->state == SWR_MSTR_PAUSE) ||
		    (swrm->state == SWR_MSTR_DOWN))
			dev_dbg(swrm->dev, "%s: SWR master is already Down: %d\n",
				__func__, swrm->state);
		else
			swrm_device_down(&pdev->dev);
		mutex_unlock(&swrm->mlock);
		break;
	case SWR_DEVICE_UP:
		dev_dbg(swrm->dev, "%s: swr master up called\n", __func__);
		mutex_lock(&swrm->mlock);
		mutex_lock(&swrm->reslock);
		if ((swrm->state == SWR_MSTR_RESUME) ||
		    (swrm->state == SWR_MSTR_UP)) {
			dev_dbg(swrm->dev, "%s: SWR master is already UP: %d\n",
				__func__, swrm->state);
			list_for_each_entry(swr_dev, &mstr->devices, dev_list)
				swr_reset_device(swr_dev);
		} else {
			pm_runtime_mark_last_busy(&pdev->dev);
			mutex_unlock(&swrm->reslock);
			pm_runtime_get_sync(&pdev->dev);
			mutex_lock(&swrm->reslock);
			list_for_each_entry(swr_dev, &mstr->devices, dev_list) {
				ret = swr_reset_device(swr_dev);
				if (ret) {
					dev_err(swrm->dev,
						"%s: failed to reset swr device %d\n",
						__func__, swr_dev->dev_num);
					swrm_clk_request(swrm, false);
				}
			}
			pm_runtime_mark_last_busy(&pdev->dev);
			pm_runtime_put_autosuspend(&pdev->dev);
		}
		mutex_unlock(&swrm->reslock);
		mutex_unlock(&swrm->mlock);
		break;
	case SWR_SET_NUM_RX_CH:
		if (!data) {
			dev_err(swrm->dev, "%s: data is NULL\n", __func__);
			ret = -EINVAL;
		} else {
			mutex_lock(&swrm->mlock);
			swrm->num_rx_chs = *(int *)data;
			if ((swrm->num_rx_chs > 1) && !swrm->num_cfg_devs) {
				list_for_each_entry(swr_dev, &mstr->devices,
						    dev_list) {
					ret = swr_set_device_group(swr_dev,
								SWR_BROADCAST);
					if (ret)
						dev_err(swrm->dev,
							"%s: set num ch failed\n",
							__func__);
				}
			} else {
				list_for_each_entry(swr_dev, &mstr->devices,
						    dev_list) {
					ret = swr_set_device_group(swr_dev,
								SWR_GROUP_NONE);
					if (ret)
						dev_err(swrm->dev,
							"%s: set num ch failed\n",
							__func__);
				}
			}
			mutex_unlock(&swrm->mlock);
		}
		break;
	default:
		dev_err(swrm->dev, "%s: swr master unknown id %d\n",
			__func__, id);
		break;
	}
	return ret;
}
EXPORT_SYMBOL(swrm_wcd_notify);

#ifdef CONFIG_PM_SLEEP
static int swrm_suspend(struct device *dev)
{
	int ret = -EBUSY;
	struct platform_device *pdev = to_platform_device(dev);
	struct swr_mstr_ctrl *swrm = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s: system suspend, state: %d\n", __func__, swrm->state);
	if (!pm_runtime_enabled(dev) || !pm_runtime_suspended(dev)) {
		ret = swrm_runtime_suspend(dev);
		if (!ret) {
			/*
			 * Synchronize runtime-pm and system-pm states:
			 * At this point, we are already suspended. If
			 * runtime-pm still thinks its active, then
			 * make sure its status is in sync with HW
			 * status. The three below calls let the
			 * runtime-pm know that we are suspended
			 * already without re-invoking the suspend
			 * callback
			 */
			pm_runtime_disable(dev);
			pm_runtime_set_suspended(dev);
			pm_runtime_enable(dev);
		}
	}
	if (ret == -EBUSY) {
		/*
		 * There is a possibility that some audio stream is active
		 * during suspend. We dont want to return suspend failure in
		 * that case so that display and relevant components can still
		 * go to suspend.
		 * If there is some other error, then it should be passed-on
		 * to system level suspend
		 */
		ret = 0;
	}
	return ret;
}

static int swrm_resume(struct device *dev)
{
	int ret = 0;
	struct platform_device *pdev = to_platform_device(dev);
	struct swr_mstr_ctrl *swrm = platform_get_drvdata(pdev);

	dev_dbg(dev, "%s: system resume, state: %d\n", __func__, swrm->state);
	if (!pm_runtime_enabled(dev) || !pm_runtime_suspend(dev)) {
		ret = swrm_runtime_resume(dev);
		if (!ret) {
			pm_runtime_mark_last_busy(dev);
			pm_request_autosuspend(dev);
		}
	}
	return ret;
}
#endif /* CONFIG_PM_SLEEP */

static const struct dev_pm_ops swrm_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(
		swrm_suspend,
		swrm_resume
	)
	SET_RUNTIME_PM_OPS(
		swrm_runtime_suspend,
		swrm_runtime_resume,
		NULL
	)
};

static const struct of_device_id swrm_dt_match[] = {
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
		.pm = &swrm_dev_pm_ops,
		.of_match_table = swrm_dt_match,
	},
};

static int __init swrm_init(void)
{
	return platform_driver_register(&swr_mstr_driver);
}
module_init(swrm_init);

static void __exit swrm_exit(void)
{
	platform_driver_unregister(&swr_mstr_driver);
}
module_exit(swrm_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("WCD SoundWire Controller");
MODULE_ALIAS("platform:swr-wcd");
