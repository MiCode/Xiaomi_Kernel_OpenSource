/*
 * Copyright (c) 2014 MediaTek Inc.
 * Author: jitao shi <jitao.shi@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/debugfs.h>
#include <linux/phy/phy.h>
#include <drm/drmP.h>
#include <drm/drm_mipi_dsi.h>
#include <linux/uaccess.h>
#include <video/videomode.h>
#include "mtk_drm_drv.h"
#include "mtk_dsi.h"
#include "mtk_mipi_tx.h"

#define  HELP_INFO_DSI \
	"\n" \
	"USAGE\n" \
	"	echo [ACTION]... > /sys/kernel/debug/mtk_dsi\n" \
	"\n" \
	"ACTION\n" \
	"\n" \
	"	regr:addr\n" \
	"	regw:addr=value\n" \
	"	dump\n" \

#define  MTK_DSI_BASE		0x1400c000
#define  MTK_MIPI_TX_BASE	0x10010000

static int mtk_dsi_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t mtk_dsi_debug_read(struct file *file, char __user *ubuf,
				   size_t count, loff_t *ppos)
{
	return simple_read_from_buffer(ubuf, count, ppos, HELP_INFO_DSI,
				       strlen(HELP_INFO_DSI));
}

static int mtk_dsi_process_cmd(char *cmd, struct mtk_dsi *dsi)
{
	char *np, *opt;
	struct mtk_mipi_tx *mipi_tx = phy_get_drvdata(dsi->phy);
	unsigned long addr;

	dev_info(dsi->dev, "dbg cmd: %s\n", cmd);
	opt = strsep(&cmd, " ");

	if (strncmp(opt, "regr:", 5) == 0) {
		char *p = (char *)opt + 5;

		if (kstrtoul(p, 16, &addr))
			goto errcode;

		if ((addr & MTK_MIPI_TX_BASE) == MTK_MIPI_TX_BASE)
			DRM_INFO("mipitx read register 0x%08lX: 0x%08X\n",
				addr, readl(mipi_tx->regs + addr - MTK_MIPI_TX_BASE));
		else if ((addr & MTK_DSI_BASE) == MTK_DSI_BASE)
			DRM_INFO("dsi read register 0x%08lX: 0x%08X\n", addr, readl(dsi->regs + addr - MTK_DSI_BASE));
		else
			goto errcode;
	} else if (strncmp(opt, "regw:", 5) == 0) {
		char *p = (char *)opt + 5;
		unsigned long val;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &addr))
			goto errcode;

		if (p == NULL)
			goto errcode;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &val))
			goto errcode;

		if ((addr & MTK_MIPI_TX_BASE) == MTK_MIPI_TX_BASE)
			writel(val, mipi_tx->regs + addr - MTK_MIPI_TX_BASE);
		else if ((addr & MTK_DSI_BASE) == MTK_DSI_BASE)
			writel(val, dsi->regs + addr - MTK_DSI_BASE);
		else
			goto errcode;
	} else if (strncmp(opt, "dump", 4) == 0) {
		mtk_dsi_dump_registers(dsi);
	} else {
		goto errcode;
	}

	return 0;

errcode:
	dev_err(dsi->dev, "invalid dbg command\n");
	return -1;
}

static ssize_t mtk_dsi_debug_write(struct file *file, const char __user *ubuf,
				    size_t count, loff_t *ppos)
{
	char str_buf[64];
	size_t ret;
	struct mtk_dsi *dsi;

	dsi = file->private_data;
	ret = count;
	memset(str_buf, 0, sizeof(str_buf));

	if (count > sizeof(str_buf))
		count = sizeof(str_buf);

	if (copy_from_user(str_buf, ubuf, count))
		return -EFAULT;

	str_buf[count] = 0;

	mtk_dsi_process_cmd(str_buf, dsi);

	return ret;
}

static const struct file_operations mtk_dsi_debug_fops = {
	.read = mtk_dsi_debug_read,
	.write = mtk_dsi_debug_write,
	.open = mtk_dsi_debug_open,
};

int mtk_drm_dsi_debugfs_init(struct mtk_dsi *dsi)
{
	dsi->debugfs =
	    debugfs_create_file("mtk_dsi", S_IFREG | S_IRUGO |
				S_IWUSR | S_IWGRP, NULL, (void *)dsi,
				&mtk_dsi_debug_fops);

	if (IS_ERR(dsi->debugfs))
		return PTR_ERR(dsi->debugfs);

	return 0;
}

void mtk_drm_dsi_debugfs_exit(struct mtk_dsi *dsi)
{
	debugfs_remove(dsi->debugfs);
}

