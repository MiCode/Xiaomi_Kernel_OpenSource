/*
 * drivers/video/tegra/dc/dsi_debug.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION, All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/clk.h>
#include <linux/moduleparam.h>
#include <linux/export.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include "dc_reg.h"
#include "dc_priv.h"
#include "dev.h"
#include "dsi_regs.h"
#include "dsi.h"
/* HACK! This needs to come from DT */
#include "../../../../arch/arm/mach-tegra/iomap.h"

#ifdef CONFIG_DEBUG_FS

/*
 * to use these tests, go to d/tegra_dsi in uart/adb shell by:
 * cd d/tegra_dsi
 *
 */

/*
 * to get the dump of panel register , do:
 * cat regs
 *
 */

static int regdump_show(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;
	unsigned long i = 0, j = 0;
	u32 col = 0;
	u32 base[MAX_DSI_INSTANCE] = {TEGRA_DSI_BASE, TEGRA_DSIB_BASE};

	if (!dsi->enabled) {
		seq_puts(s, "DSI controller suspended\n");
		return 0;
	}

	tegra_dc_io_start(dsi->dc);
	tegra_dsi_clk_enable(dsi);

	/* mem dd dump */
	for (i = 0; i < dsi->max_instances; i++) {
		for (col = 0, j = 0; j < 0x64; j++) {
			if (col == 0)
				seq_printf(s, "%08lX:", base[i] + 4*j);
			seq_printf(s, "%c%08lX", col == 2 ? '-' : ' ',
				tegra_dsi_controller_readl(dsi, j, i));
			if (col == 3) {
				seq_puts(s, "\n");
				col = 0;
			} else
				col++;
		}
		seq_puts(s, "\n");
	}

#define DUMP_REG(a)	seq_printf(s, "%-45s | %#05x | %#010lx |\n", \
					#a, a, tegra_dsi_readl(dsi, a));

	DUMP_REG(DSI_INCR_SYNCPT_CNTRL);
	DUMP_REG(DSI_INCR_SYNCPT_ERROR);
	DUMP_REG(DSI_CTXSW);
	DUMP_REG(DSI_POWER_CONTROL);
	DUMP_REG(DSI_INT_ENABLE);
	DUMP_REG(DSI_HOST_DSI_CONTROL);
	DUMP_REG(DSI_CONTROL);
	DUMP_REG(DSI_SOL_DELAY);
	DUMP_REG(DSI_MAX_THRESHOLD);
	DUMP_REG(DSI_TRIGGER);
	DUMP_REG(DSI_TX_CRC);
	DUMP_REG(DSI_STATUS);
	DUMP_REG(DSI_INIT_SEQ_CONTROL);
	DUMP_REG(DSI_INIT_SEQ_DATA_0);
	DUMP_REG(DSI_INIT_SEQ_DATA_1);
	DUMP_REG(DSI_INIT_SEQ_DATA_2);
	DUMP_REG(DSI_INIT_SEQ_DATA_3);
	DUMP_REG(DSI_INIT_SEQ_DATA_4);
	DUMP_REG(DSI_INIT_SEQ_DATA_5);
	DUMP_REG(DSI_INIT_SEQ_DATA_6);
	DUMP_REG(DSI_INIT_SEQ_DATA_7);
	DUMP_REG(DSI_PKT_SEQ_0_LO);
	DUMP_REG(DSI_PKT_SEQ_0_HI);
	DUMP_REG(DSI_PKT_SEQ_1_LO);
	DUMP_REG(DSI_PKT_SEQ_1_HI);
	DUMP_REG(DSI_PKT_SEQ_2_LO);
	DUMP_REG(DSI_PKT_SEQ_2_HI);
	DUMP_REG(DSI_PKT_SEQ_3_LO);
	DUMP_REG(DSI_PKT_SEQ_3_HI);
	DUMP_REG(DSI_PKT_SEQ_4_LO);
	DUMP_REG(DSI_PKT_SEQ_4_HI);
	DUMP_REG(DSI_PKT_SEQ_5_LO);
	DUMP_REG(DSI_PKT_SEQ_5_HI);
	DUMP_REG(DSI_DCS_CMDS);
	DUMP_REG(DSI_PKT_LEN_0_1);
	DUMP_REG(DSI_PKT_LEN_2_3);
	DUMP_REG(DSI_PKT_LEN_4_5);
	DUMP_REG(DSI_PKT_LEN_6_7);
	DUMP_REG(DSI_PHY_TIMING_0);
	DUMP_REG(DSI_PHY_TIMING_1);
	DUMP_REG(DSI_PHY_TIMING_2);
	DUMP_REG(DSI_BTA_TIMING);
	DUMP_REG(DSI_TIMEOUT_0);
	DUMP_REG(DSI_TIMEOUT_1);
	DUMP_REG(DSI_TO_TALLY);
	DUMP_REG(DSI_PAD_CONTROL);
	DUMP_REG(DSI_PAD_CONTROL_CD);
	DUMP_REG(DSI_PAD_CD_STATUS);
	DUMP_REG(DSI_VID_MODE_CONTROL);
	DUMP_REG(DSI_PAD_CONTROL_0_VS1);
	DUMP_REG(DSI_PAD_CONTROL_CD_VS1);
	DUMP_REG(DSI_PAD_CD_STATUS_VS1);
	DUMP_REG(DSI_PAD_CONTROL_1_VS1);
	DUMP_REG(DSI_PAD_CONTROL_2_VS1);
	DUMP_REG(DSI_PAD_CONTROL_3_VS1);
	DUMP_REG(DSI_PAD_CONTROL_4_VS1);
	DUMP_REG(DSI_GANGED_MODE_CONTROL);
	DUMP_REG(DSI_GANGED_MODE_START);
	DUMP_REG(DSI_GANGED_MODE_SIZE);
#undef DUMP_REG

	tegra_dsi_clk_disable(dsi);
	tegra_dc_io_end(dsi->dc);

	return 0;
}

/*
 * to use colorbar test case use :
 * cat colorbar
 *
 */

static ssize_t colorbar_show(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;

#define RED    0xff0000ff
#define GREEN  0xff00ff00
#define BLUE   0xffff0000

	int i, j;
	u32 *ptr;
	u32 val;
	int width, height;
	dma_addr_t phys_addr;

	dev_info(&dc->ndev->dev, "===== display colorbar test is started ====\n");

	if (!dsi->enabled) {
		dev_info(&dc->ndev->dev, "DSI controller suspended\n");
		goto fail;
	}

	width  = dc->mode.h_active;
	height = dc->mode.v_active;

	ptr = (u32 *)dma_zalloc_coherent(&dc->ndev->dev, width * height * 4,
						&phys_addr, GFP_KERNEL);
	if (!ptr) {
		dev_err(&dc->ndev->dev, "Can't allocate memory for colorbar test\n");
		goto fail;
	}

	for (i = 0; i < height / 4; i++)
		for (j = 0; j < width; j++)
			*(ptr++) = RED;

	for (i = 0; i < height / 4; i++)
		for (j = 0; j < width; j++)
			*(ptr++) = GREEN;

	for (i = 0; i < height / 4; i++)
		for (j = 0; j < width; j++)
			*(ptr++) = BLUE;

	for (i = 0; i < height / 4; i++)
		for (j = 0; j < width; j++)
			*(ptr++) = RED;

	tegra_dc_get(dc);

	tegra_dc_writel(dc, WRITE_MUX_ASSEMBLY | READ_MUX_ASSEMBLY,
				DC_CMD_STATE_ACCESS);

	tegra_dc_writel(dc, WINDOW_A_SELECT, DC_CMD_DISPLAY_WINDOW_HEADER);
	val = 0;
	tegra_dc_writel(dc, val, DC_WIN_WIN_OPTIONS);
	tegra_dc_writel(dc, WIN_A_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, WIN_A_ACT_REQ | GENERAL_ACT_REQ,
				DC_CMD_STATE_CONTROL);

	tegra_dc_writel(dc, WINDOW_C_SELECT, DC_CMD_DISPLAY_WINDOW_HEADER);
	val = 0;
	tegra_dc_writel(dc, val, DC_WIN_WIN_OPTIONS);
	tegra_dc_writel(dc, WIN_C_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, WIN_C_ACT_REQ | GENERAL_ACT_REQ,
				DC_CMD_STATE_CONTROL);

	tegra_dc_writel(dc, WINDOW_B_SELECT, DC_CMD_DISPLAY_WINDOW_HEADER);
	val = WIN_ENABLE;
	tegra_dc_writel(dc, val, DC_WIN_WIN_OPTIONS);
	tegra_dc_writel(dc, WIN_B_UPDATE, DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, WIN_B_ACT_REQ | GENERAL_ACT_REQ,
				DC_CMD_STATE_CONTROL);
	/* 13: TEGRA_WIN_FMT_R8G8B8A8 */
	tegra_dc_writel(dc, 13, DC_WIN_COLOR_DEPTH);
	tegra_dc_writel(dc, 0, DC_WIN_BYTE_SWAP);

	tegra_dc_writel(dc, V_POSITION(0) | H_POSITION(0), DC_WIN_POSITION);
	tegra_dc_writel(dc, V_SIZE(height) | H_SIZE(width), DC_WIN_SIZE);
	tegra_dc_writel(dc, V_PRESCALED_SIZE(height) |
				H_PRESCALED_SIZE(width * 4),
				DC_WIN_PRESCALED_SIZE);

	tegra_dc_writel(dc, V_DDA_INC(0x1000) | H_DDA_INC(0x1000),
				DC_WIN_DDA_INCREMENT);
	tegra_dc_writel(dc, width*4, DC_WIN_LINE_STRIDE);
	tegra_dc_writel(dc, (unsigned long)phys_addr, DC_WINBUF_START_ADDR);
	tegra_dc_writel(dc, 0, DC_WINBUF_ADDR_H_OFFSET);
	tegra_dc_writel(dc, 0, DC_WINBUF_ADDR_V_OFFSET);

	tegra_dc_writel(dc, WIN_B_UPDATE , DC_CMD_STATE_CONTROL);
	tegra_dc_writel(dc, WIN_B_ACT_REQ | GENERAL_ACT_REQ | NC_HOST_TRIG,
				DC_CMD_STATE_CONTROL);

	tegra_dc_put(dc);

fail:
	dev_info(&dc->ndev->dev, "===== display colorbar test is finished ====\n");
	return 0;
}

static int regdump_open(struct inode *inode, struct file *file)
{
	return single_open(file, regdump_show, inode->i_private);
}

static const struct file_operations regdump_fops = {
	.open = regdump_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int colorbar_open(struct inode *inode, struct file *file)
{
	return single_open(file, colorbar_show, inode->i_private);
}

static const struct file_operations colorbar_fops = {
	.open = colorbar_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static u32 max_ret_payload_size;
static u32 panel_reg_addr;

/*
 * read panel get is for reading value of a panel register
 * you need to give both register address and return payload size as input
 * the first parameter is register address
 * and second is payload size , test case for jdi panel use :
 * echo 0xA1 0x05 > read_panel
 * and then do :
 * cat read_panel
 *
 */

static ssize_t read_panel_get(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;
	int err = 0;
	u8 buf[300] = {0};
	int j = 0 , b = 0 , k;
	u32 payload_size = 0;

	if (!dsi->enabled) {
		dev_info(&dc->ndev->dev, " controller suspended\n");
	return -EINVAL;
}

	seq_printf(s, "max ret payload size:0x%x\npanel reg addr:0x%x\n",
					max_ret_payload_size, panel_reg_addr);
	if (max_ret_payload_size == 0) {
		seq_puts(s, "echo was not successful\n");
	return err;
}
	err = tegra_dsi_read_data(dsi->dc, dsi,
				max_ret_payload_size,
				panel_reg_addr, buf);

	seq_printf(s, " Read data[%d] ", b);

	for (b = 1; b < (max_ret_payload_size+1); b++) {
		j = (b*4)-1;
		for (k = j; k > (j-4); k--)
			if ((k%4) == 0 && b != max_ret_payload_size) {
				seq_printf(s, " %x  ", buf[k]);
				seq_printf(s, "\n Read data[%d] ", b);
			}
			else
				seq_printf(s, " %x ", buf[k]);
	}
	seq_puts(s, "\n");

	switch (buf[0]) {
	case DSI_ESCAPE_CMD:
		seq_printf(s, "escape cmd[0x%x]\n", buf[0]);
		break;
	case DSI_ACK_NO_ERR:
		seq_printf(s,
			"Panel ack, no err[0x%x]\n", buf[0]);
		goto fail;
		break;
	default:
		seq_puts(s, "Invalid read response\n");
		break;
	}

	switch (buf[4] & 0xff) {
	case GEN_LONG_RD_RES:
		/* Fall through */
	case DCS_LONG_RD_RES:
		payload_size = (buf[5] |
				(buf[6] << 8)) & 0xFFFF;
		seq_printf(s, "Long read response Packet\n"
				"payload_size[0x%x]\n", payload_size);
		break;
	case GEN_1_BYTE_SHORT_RD_RES:
		/* Fall through */
	case DCS_1_BYTE_SHORT_RD_RES:
		payload_size = 1;
		seq_printf(s, "Short read response Packet\n"
			"payload_size[0x%x]\n", payload_size);
		break;
	case GEN_2_BYTE_SHORT_RD_RES:
		/* Fall through */
	case DCS_2_BYTE_SHORT_RD_RES:
		payload_size = 2;
		seq_printf(s, "Short read response Packet\n"
			"payload_size[0x%x]\n", payload_size);
		break;
	case ACK_ERR_RES:
		payload_size = 2;
		seq_printf(s, "Acknowledge error report response\n"
			"Packet payload_size[0x%x]\n", payload_size);
		break;
	default:
		seq_puts(s, "Invalid response packet\n");
		break;
	}
fail:
	return err;
}

static ssize_t read_panel_set(struct file *file, const char  *buf,
						size_t count, loff_t *off)
{
	struct seq_file *s = file->private_data;
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;

	if (sscanf(buf, "%x %x", &max_ret_payload_size, &panel_reg_addr) != 2)
		return -EINVAL;
	dev_info(&dc->ndev->dev, "max ret payload size:0x%x\npanel reg addr:0x%x\n",
			max_ret_payload_size, panel_reg_addr);

		return count;
}

static int read_panel_open(struct inode *inode, struct file *file)
{
	return single_open(file, read_panel_get, inode->i_private);
}

static const struct file_operations read_panel_fops = {
	.open = read_panel_open,
	.read = seq_read,
	.write = read_panel_set,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * panel sanity check returns whether the last packet was with error or not
 * to use panel sanity check do :
 * cat panel_sanity
 *
 */

static int panel_sanity_check(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;
	struct sanity_status *san = NULL;
	int err = 0;

	san = devm_kzalloc(&dc->ndev->dev, sizeof(*san), GFP_KERNEL);
	if (!san) {
		dev_info(&dc->ndev->dev, "No memory available\n");
		return err;
	}

	tegra_dsi_enable_read_debug(dsi);
	err = tegra_dsi_panel_sanity_check(dc, dsi, san);
	tegra_dsi_disable_read_debug(dsi);

	if (err < 0)
		seq_puts(s, "Sanity check failed\n");
	else
		seq_puts(s, "Sanity check successful\n");

	return err;
}

static int sanity_panel_open(struct inode *inode, struct file *file)
{
	return single_open(file, panel_sanity_check, inode->i_private);
}

static const struct file_operations sanity_panel_fops = {
	.open = sanity_panel_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * to send host dcs cmd
 * you have to give all three parameters as input
 * the test-commands are (to be used as cmd_value )
 * DSI_DCS_SET_DISPLAY_ON			0x29
 * DSI_DCS_SET_DISPLAY_OFF			0x28
 *
 * the data_id are
 * DSI_DCS_WRITE_0_PARAM			 0x05
 * DSI_DCS_WRITE_1_PARAM			 0x15
 *
 * the second comand i.e. command_value1 can be put as 0x0
 *
 * to test ,give:
 * echo 0x05 0x28 0x0 > host_cmd_v_blank_dcs
 * to see the effect, do:
 * cat host_cmd_v_blank_dcs
 *
 */

static u32 command_value;
static u32 data_id;
static u32 command_value1;

static int send_host_cmd_v_blank_dcs(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;
	int err;

	struct tegra_dsi_cmd user_command[] = {
	DSI_CMD_SHORT(data_id, command_value, command_value1),
	DSI_DLY_MS(20),
	};

	if (!dsi->enabled) {
		seq_puts(s, "DSI controller suspended\n");
		return 0;
	}

	seq_printf(s, "data_id taken :0x%x\n", data_id);
	seq_printf(s, "command value taken :0x%x\n", command_value);
	seq_printf(s, "second command value taken :0x%x\n", command_value1);

	err = tegra_dsi_start_host_cmd_v_blank_dcs(dsi, user_command);

	return err;
}

static ssize_t host_cmd_v_blank_dcs_get_cmd(struct file *file,
				const char  *buf, size_t count, loff_t *off)
{
	struct seq_file *s = file->private_data;
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;

	if (!dsi->enabled) {
		dev_info(&dc->ndev->dev, "DSI controller suspended\n");
		return count;
	}

	if (sscanf(buf, "%x %x %x", &data_id, &command_value, &command_value1)
			!= 3)
		return -EINVAL;
	dev_info(&dc->ndev->dev, "data id taken :0x%x\n", data_id);
	dev_info(&dc->ndev->dev, "command value taken :0x%x\n", command_value);
	dev_info(&dc->ndev->dev, "second command value taken :0x%x\n",
							 command_value1);
	return count;
}

static int host_cmd_v_blank_dcs_open(struct inode *inode, struct file *file)
{
	return single_open(file, send_host_cmd_v_blank_dcs, inode->i_private);
}

static const struct file_operations host_cmd_v_blank_dcs_fops = {
	.open = host_cmd_v_blank_dcs_open,
	.read = seq_read,
	.write = host_cmd_v_blank_dcs_get_cmd,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * to remove the functioality of send_host_cmd_v_blank_dcs
 * use:
 * cat remove_host_cmd_dcs
 *
 */

static int remove_host_cmd_dcs(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;

	tegra_dsi_stop_host_cmd_v_blank_dcs(dsi);
	seq_puts(s, "host_cmd_v_blank_dcs stopped\n");

	return 0;
}

static int rm_host_cmd_dcs_open(struct inode *inode, struct file *file)
{
	return single_open(file, remove_host_cmd_dcs, inode->i_private);
}

static const struct file_operations remove_host_cmd_dcs_fops = {
	.open = rm_host_cmd_dcs_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*
 * to write host cmd to check functioanlity of write_data
 * you have to give all three parameters as input
 * the commands are (to be used as cmd_value)
 * DSI_DCS_SET_DISPLAY_ON			0x29
 * DSI_DCS_SET_DISPLAY_OFF			0x28
 *
 * the data_id are
 * DSI_DCS_WRITE_0_PARAM			 0x05
 * DSI_DCS_WRITE_1_PARAM			0x15
 *
 * the second comand i.e. command_value1 can be put as 0x0
 *
 * to test give:
 * echo 0x05 0x28 0x0 > write_data
 * to see the effect do:
 * cat write_data
 *
 */

static int send_write_data_cmd(struct seq_file *s, void *unused)
{
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;
	int err;
	u8 del = 100;

	struct tegra_dsi_cmd user_command[] = {
	DSI_CMD_SHORT(data_id, command_value, command_value1),
	DSI_DLY_MS(20),
	};

	seq_printf(s, "data_id taken :0x%x\n", data_id);
	seq_printf(s, "command value taken :0x%x\n", command_value);
	seq_printf(s, "second command value taken :0x%x\n", command_value1);

	err = tegra_dsi_write_data(dc, dsi, user_command, del);

	return err;
}

static ssize_t write_data_get_cmd(struct file *file,
				const char  *buf, size_t count, loff_t *off)
{
	struct seq_file *s = file->private_data;
	struct tegra_dc_dsi_data *dsi = s->private;
	struct tegra_dc *dc = dsi->dc;

	if (sscanf(buf, "%x %x %x", &data_id,
				&command_value, &command_value1) != 3)
		return -EINVAL;
	dev_info(&dc->ndev->dev, "data_id taken :0x%x\n", data_id);
	dev_info(&dc->ndev->dev, "command value taken :0x%x\n", command_value);
	dev_info(&dc->ndev->dev, "second command value taken :0x%x\n",
					command_value1);

	return count;
}

static int write_data_open(struct inode *inode, struct file *file)
{
	return single_open(file, send_write_data_cmd, inode->i_private);
}

static const struct file_operations write_data_fops = {
	.open = write_data_open,
	.read = seq_read,
	.write = write_data_get_cmd,
	.llseek = seq_lseek,
	.release = single_release,
};

static struct dentry *dsidir;

void tegra_dc_dsi_debug_create(struct tegra_dc_dsi_data *dsi)
{
	struct dentry *retval;

	dsidir = debugfs_create_dir("tegra_dsi", NULL);
	if (!dsidir)
		return;
	retval = debugfs_create_file("regs", S_IRUGO, dsidir, dsi,
		&regdump_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("colorbar", S_IRUGO, dsidir, dsi,
		&colorbar_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("read_panel", S_IRUGO|S_IWUSR, dsidir,
				dsi, &read_panel_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("panel_sanity", S_IRUGO, dsidir,
				dsi, &sanity_panel_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("host_cmd_v_blank_dcs", S_IRUGO|S_IWUSR,
				 dsidir, dsi, &host_cmd_v_blank_dcs_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("remove_host_cmd_dcs", S_IRUGO|S_IWUSR,
				 dsidir, dsi, &remove_host_cmd_dcs_fops);
	if (!retval)
		goto free_out;
	retval = debugfs_create_file("write_data", S_IRUGO|S_IWUSR,
				 dsidir, dsi, &write_data_fops);
	if (!retval)
		goto free_out;
	return;
free_out:
	debugfs_remove_recursive(dsidir);
	dsidir = NULL;
	return;
}
EXPORT_SYMBOL(tegra_dc_dsi_debug_create);
#else
void tegra_dc_dsi_debug_create(struct tegra_dc_dsi_data *dsi)
{}
EXPORT_SYMBOL(tegra_dc_dsi_debug_create);

#endif
