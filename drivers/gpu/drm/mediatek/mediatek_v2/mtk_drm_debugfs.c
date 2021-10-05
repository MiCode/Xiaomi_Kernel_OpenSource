// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#if IS_ENABLED(CONFIG_DEBUG_FS)
#include <linux/debugfs.h>
#endif
#if IS_ENABLED(CONFIG_PROC_FS)
#include <linux/proc_fs.h>
#endif
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/uaccess.h>
#include "mtk_drm_ddp_comp.h"
#include "mtk_drm_plane.h"
#include "mtk_drm_crtc.h"
#include "mtk_drm_drv.h"
#include "mtk_drm_debugfs.h"

struct mtk_drm_debugfs_table {
	char name[8];
	unsigned int offset[2];
	unsigned int length[2];
	unsigned long reg_base;
};

/* ------------------------------------------------------------------------- */
/* External variable declarations */
/* ------------------------------------------------------------------------- */
static void __iomem *gdrm_disp1_base[12];
static void __iomem *gdrm_disp2_base[7];
static struct mtk_drm_debugfs_table gdrm_disp1_reg_range[7] = {
	{"OVL0 ", {0, 0xf40}, {0x260, 0x80}, 0},
	{"COLOR0 ", {0x400, 0xc00}, {0x400, 0x100}, 0},
	{"AAL0 ", {0, 0}, {0x100, 0}, 0},
	{"OD0 ", {0, 0}, {0x100, 0}, 0},
	{"RDMA0 ", {0, 0}, {0x100, 0}, 0},
	{"CONFIG ", {0, 0}, {0x120, 0}, 0},
	{"MUTEX ", {0, 0}, {0x100, 0}, 0} };

static struct mtk_drm_debugfs_table gdrm_disp2_reg_range[7] = {
	{"OVL1 ", {0, 0xf40}, {0x260, 0x80}, 0},
	{"COLOR1 ", {0x400, 0xc00}, {0x100, 0x100}, 0},
	{"AAL1 ", {0, 0}, {0x100, 0}, 0},
	{"OD1 ", {0, 0}, {0x100, 0}, 0},
	{"RDMA1 ", {0, 0}, {0x100, 0}, 0},
	{"CONFIG ", {0, 0}, {0x120, 0}, 0},
	{"MUTEX ", {0, 0}, {0x100, 0}, 0} };
static bool dbgfs_alpha;

static void mtk_read_reg(unsigned long addr)
{
	void __iomem *reg_va = 0;

	reg_va = ioremap(addr, sizeof(reg_va));
	DDPMSG("r:0x%8lx = 0x%08x\n", addr, readl(reg_va));
	iounmap(reg_va);
}

static void mtk_write_reg(unsigned long addr, unsigned long val)
{
	void __iomem *reg_va = 0;

	reg_va = ioremap(addr, sizeof(reg_va));
	writel(val, reg_va);
	iounmap(reg_va);
}

/* ------------------------------------------------------------------------- */
/* Debug Options */
/* ------------------------------------------------------------------------- */
static const char STR_HELP[] = "\n"
			 "USAGE\n"
			 "        echo [ACTION]... > mtkdrm\n"
			 "\n"
			 "ACTION\n"
			 "\n"
			 "        dump:\n"
			 "             dump all hw registers\n"
			 "\n"
			 "        regw:addr=val\n"
			 "             write hw register\n"
			 "\n"
			 "        regr:addr\n"
			 "             read hw register\n";

/* ------------------------------------------------------------------------- */
/* Command Processor */
/* ------------------------------------------------------------------------- */
static void process_dbg_opt(char *opt)
{
	if (strncmp(opt, "regw:", 5) == 0) {
		char *p = (char *)opt + 5;
		char *np;
		unsigned long addr, val;
		u64 i;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &addr) != 0)
			goto error;

		if (p == NULL)
			goto error;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &val) != 0)
			goto error;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (addr > gdrm_disp1_reg_range[i].reg_base &&
			    addr < gdrm_disp1_reg_range[i].reg_base +
					    0x1000UL) {
				writel(val, gdrm_disp1_base[i] + addr -
						    gdrm_disp1_reg_range[i]
							    .reg_base);
				break;
			}
		}

		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (addr > gdrm_disp2_reg_range[i].reg_base &&
			    addr < gdrm_disp2_reg_range[i].reg_base +
					    0x1000UL) {
				writel(val, gdrm_disp2_base[i] + addr -
						    gdrm_disp2_reg_range[i]
							    .reg_base);
				break;
			}
		}

	} else if (strncmp(opt, "regr:", 5) == 0) {
		char *p = (char *)opt + 5;
		unsigned long addr;
		u64 i;

		if (kstrtoul(p, 16, &addr) != 0)
			goto error;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (addr >= gdrm_disp1_reg_range[i].reg_base &&
			    addr < gdrm_disp1_reg_range[i].reg_base +
					    0x1000UL) {
				DRM_INFO("%8s Read register 0x%08lX: 0x%08X\n",
					 gdrm_disp1_reg_range[i].name, addr,
					 readl(gdrm_disp1_base[i] + addr -
					       gdrm_disp1_reg_range[i]
						       .reg_base));
				break;
			}
		}

		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (addr >= gdrm_disp2_reg_range[i].reg_base &&
			    addr < gdrm_disp2_reg_range[i].reg_base +
					    0x1000UL) {
				DRM_INFO("%8s Read register 0x%08lX: 0x%08X\n",
					 gdrm_disp2_reg_range[i].name, addr,
					 readl(gdrm_disp2_base[i] + addr -
					       gdrm_disp2_reg_range[i]
						       .reg_base));
				break;
			}
		}

	} else if (strncmp(opt, "autoregr:", 9) == 0) {
		DRM_INFO("Set the register addr for Auto-test\n");
	} else if (strncmp(opt, "dump:", 5) == 0) {
		u64 i;
		u32 j;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (gdrm_disp1_base[i] == NULL)
				continue;
			for (j = gdrm_disp1_reg_range[i].offset[0];
			     j < gdrm_disp1_reg_range[i].offset[0] +
					 gdrm_disp1_reg_range[i].length[0];
			     j += 16UL)
				DRM_INFO("%8s 0x%08lX: %08X %08X %08X %08X\n",
					 gdrm_disp1_reg_range[i].name,
					 gdrm_disp1_reg_range[i].reg_base + j,
					 readl(gdrm_disp1_base[i] + j),
					 readl(gdrm_disp1_base[i] + j + 0x4),
					 readl(gdrm_disp1_base[i] + j + 0x8),
					 readl(gdrm_disp1_base[i] + j + 0xc));

			for (j = gdrm_disp1_reg_range[i].offset[1];
			     j < gdrm_disp1_reg_range[i].offset[1] +
					 gdrm_disp1_reg_range[i].length[1];
			     j += 16UL)
				DRM_INFO("%8s 0x%08lX: %08X %08X %08X %08X\n",
					 gdrm_disp1_reg_range[i].name,
					 gdrm_disp1_reg_range[i].reg_base + j,
					 readl(gdrm_disp1_base[i] + j),
					 readl(gdrm_disp1_base[i] + j + 0x4),
					 readl(gdrm_disp1_base[i] + j + 0x8),
					 readl(gdrm_disp1_base[i] + j + 0xc));
		}
		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (gdrm_disp2_base[i] == NULL)
				continue;
			for (j = gdrm_disp2_reg_range[i].offset[0];
			     j < gdrm_disp2_reg_range[i].offset[0] +
					 gdrm_disp2_reg_range[i].length[0];
			     j += 16)
				DRM_INFO("%8s 0x%08lX: %08X %08X %08X %08X\n",
					 gdrm_disp2_reg_range[i].name,
					 gdrm_disp2_reg_range[i].reg_base + j,
					 readl(gdrm_disp2_base[i] + j),
					 readl(gdrm_disp2_base[i] + j + 0x4),
					 readl(gdrm_disp2_base[i] + j + 0x8),
					 readl(gdrm_disp2_base[i] + j + 0xc));

			for (j = gdrm_disp2_reg_range[i].offset[1];
			     j < gdrm_disp2_reg_range[i].offset[1] +
					 gdrm_disp2_reg_range[i].length[1];
			     j += 16)
				DRM_INFO("%8s 0x%08lX: %08X %08X %08X %08X\n",
					 gdrm_disp2_reg_range[i].name,
					 gdrm_disp2_reg_range[i].reg_base + j,
					 readl(gdrm_disp2_base[i] + j),
					 readl(gdrm_disp2_base[i] + j + 0x4),
					 readl(gdrm_disp2_base[i] + j + 0x8),
					 readl(gdrm_disp2_base[i] + j + 0xc));
		}

	} else if (strncmp(opt, "hdmi:", 5) == 0) {
	} else if (strncmp(opt, "alpha", 5) == 0) {
		if (dbgfs_alpha) {
			DRM_INFO("set src alpha to src alpha\n");
			dbgfs_alpha = false;
		} else {
			DRM_INFO("set src alpha to ONE\n");
			dbgfs_alpha = true;
		}
	} else if (strncmp(opt, "r:", 2) == 0) {
		char *p = (char *)opt + 2;
		unsigned long addr;

		if (kstrtoul(p, 16, &addr) != 0)
			goto error;

		mtk_read_reg(addr);
	} else if (strncmp(opt, "w:", 2) == 0) {
		char *p = (char *)opt + 2;
		char *np;
		unsigned long addr, val;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &addr) != 0)
			goto error;

		if (p == NULL)
			goto error;

		np = strsep(&p, "=");
		if (kstrtoul(np, 16, &val) != 0)
			goto error;

		mtk_write_reg(addr, val);
	} else {
		goto error;
	}

	return;
error:
	DRM_ERROR("Parse command error!\n\n%s", STR_HELP);
}

static void process_dbg_cmd(char *cmd)
{
	char *tok;

	while ((tok = strsep(&cmd, " ")) != NULL)
		process_dbg_opt(tok);
}

/* ------------------------------------------------------------------------- */
/* Debug FileSystem Routines */
/* ------------------------------------------------------------------------- */
static int debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static char dis_cmd_buf[512];
static ssize_t debug_read(struct file *file, char __user *ubuf, size_t count,
			  loff_t *ppos)
{
	if (strncmp(dis_cmd_buf, "regr:", 5) == 0) {
		char read_buf[512] = {0};
		char *p = (char *)dis_cmd_buf + 5;
		unsigned long addr;
		int ret;
		u64 i;

		if (kstrtoul(p, 16, &addr) != 0)
			return 0;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (addr >= gdrm_disp1_reg_range[i].reg_base &&
			    addr < gdrm_disp1_reg_range[i].reg_base +
					    0x1000UL) {
				ret = sprintf(
					read_buf,
					"%8s Read register 0x%08lX: 0x%08X\n",
					gdrm_disp1_reg_range[i].name, addr,
					readl(gdrm_disp1_base[i] + addr -
					      gdrm_disp1_reg_range[i]
						      .reg_base));
				if (ret <= 0L)
					DRM_INFO("autoregr fail\n");
				break;
			}
		}

		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (addr >= gdrm_disp2_reg_range[i].reg_base &&
			    addr < gdrm_disp2_reg_range[i].reg_base +
					    0x1000UL) {
				ret = sprintf(
					read_buf,
					"%8s Read register 0x%08lX: 0x%08X\n",
					gdrm_disp2_reg_range[i].name, addr,
					readl(gdrm_disp2_base[i] + addr -
					      gdrm_disp2_reg_range[i]
						      .reg_base));
				if (ret <= 0L)
					DRM_INFO("autoregr fail\n");
				break;
			}
		}

		return simple_read_from_buffer(ubuf, count, ppos, read_buf,
					       strlen(read_buf));
	} else if (strncmp(dis_cmd_buf, "autoregr:", 9) == 0) {
		char read_buf[512] = {0};
		char read_buf2[512] = {0};
		char *p = (char *)dis_cmd_buf + 9;
		unsigned long addr;
		unsigned long addr2;
		int ret;
		u64 i;

		if (kstrtoul(p, 16, &addr) != 0)
			return 0;

		for (i = 0; i < ARRAY_SIZE(gdrm_disp1_reg_range); i++) {
			if (addr >= gdrm_disp1_reg_range[i].reg_base &&
			    addr < gdrm_disp1_reg_range[i].reg_base +
					    0x1000UL) {
				ret = sprintf(
					read_buf,
					"%8s Read register 0x%08lX: 0x%08X\n",
					gdrm_disp1_reg_range[i].name, addr,
					readl(gdrm_disp1_base[i] + addr -
					      gdrm_disp1_reg_range[i]
						      .reg_base));
				if (ret <= 0L)
					DRM_INFO("autoregr fail\n");
				break;
			}
		}
		addr2 = addr + 0x1000ULL;
		for (i = 0; i < ARRAY_SIZE(gdrm_disp2_reg_range); i++) {
			if (addr2 >= gdrm_disp2_reg_range[i].reg_base &&
			    addr2 < gdrm_disp2_reg_range[i].reg_base +
					    0x1000UL) {
				ret = sprintf(
					read_buf2,
					"%8s Read register 0x%08lX: 0x%08X\n",
					gdrm_disp2_reg_range[i].name, addr2,
					readl(gdrm_disp2_base[i] + addr2 -
					      gdrm_disp2_reg_range[i]
						      .reg_base));
				if (ret <= 0L)
					DRM_INFO("autoregr fail\n");
				break;
			}
		}
		p = strncat(read_buf, read_buf2,
				(sizeof(read_buf) - strlen(read_buf) - 1));
		if (p == NULL)
			DRM_INFO("autoregr strcat fail\n");
		return simple_read_from_buffer(ubuf, count, ppos, read_buf,
					       strlen(read_buf));
	} else {
		return simple_read_from_buffer(ubuf, count, ppos, STR_HELP,
					       strlen(STR_HELP));
	}
}

static ssize_t debug_write(struct file *file, const char __user *ubuf,
			   size_t count, loff_t *ppos)
{
	const u64 debug_bufmax = sizeof(dis_cmd_buf) - 1ULL;
	ssize_t ret;

	ret = (ssize_t)count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&dis_cmd_buf, ubuf, count) != 0ULL)
		return -EFAULT;

	dis_cmd_buf[count] = '\0';

	process_dbg_cmd(dis_cmd_buf);

	return ret;
}
#if S_ENABLED(CONFIG_DEBUG_FS)
static struct dentry *mtkdrm_dbgfs;
#endif

#if 0
static struct proc_dir_entry *mtkdrm_dbgfs;
#endif

static const struct file_operations debug_fops = {
	.read = debug_read, .write = debug_write, .open = debug_open,
};

bool force_alpha(void)
{
	return dbgfs_alpha;
}

void mtk_drm_debugfs_init(struct drm_device *dev, struct mtk_drm_private *priv)
{
	void __iomem *mutex_regs;
	unsigned long mutex_phys;
	struct device_node *np;
	struct resource res;
	int i;
	enum mtk_ddp_comp_id comp_id;
	const struct mtk_crtc_path_data *main_path_data;
	const struct mtk_crtc_path_data *ext_path_data;
	int ret;

	DRM_DEBUG_DRIVER("%s\n", __func__);
#if IS_ENABLED(CONFIG_DEBUG_FS)
	mtkdrm_dbgfs = debugfs_create_file("mtkdrm", 0644, NULL, (void *)0,
					   &debug_fops);
#endif
#if 0
	mtkdrm_dbgfs = proc_create("mtkdrm", S_IFREG | 0644, NULL,
					   &debug_fops);
	if (!mtkdrm_dbgfs) {
		DDPPR_ERR("[%s %d]failed to create mtkdrm in /proc\n",
			__func__, __LINE__);
		goto out;
	}
#endif
	/* TODO: The debugfs_init would be different in latest kernel version,
	 * so we will refine the debugfs with multiple path in latest verion.
	 */
	main_path_data = priv->data->main_path_data;
	for (i = 0; i < priv->data->main_path_data->path_len[DDP_MAJOR][0] - 1;
	     i++) {
		comp_id = main_path_data->path[DDP_MAJOR][0][i];
		np = priv->comp_node[comp_id];
		if (!priv->ddp_comp[comp_id])
			continue;
		gdrm_disp1_base[i] = priv->ddp_comp[comp_id]->regs;

		ret = of_address_to_resource(np, 0, &res);
		if (ret < 0)
			DRM_INFO("comp_node[%d] map address fail\n", i);
		gdrm_disp1_reg_range[i].reg_base = res.start;
	}

	gdrm_disp1_base[i] = priv->config_regs;
	gdrm_disp1_reg_range[i++].reg_base = 0x14000000;
	mutex_regs = of_iomap(priv->mutex_node, 0);
	ret = of_address_to_resource(priv->mutex_node, 0, &res);
	if (ret < 0)
		DRM_INFO("mutex_node map address fail\n");

	mutex_phys = res.start;
	gdrm_disp1_base[i] = mutex_regs;
	gdrm_disp1_reg_range[i++].reg_base = mutex_phys;

	/* TODO: The debugfs_init would be different in latest kernel version,
	 * so we will refine the debugfs with multiple path in latest verion.
	 */
	ext_path_data = priv->data->ext_path_data;
	if (ext_path_data) {
		for (i = 0; i < ext_path_data->path_len[DDP_MAJOR][0] - 1;
		     i++) {
			comp_id = ext_path_data->path[DDP_MAJOR][0][i];
			np = priv->comp_node[comp_id];
			if (!priv->ddp_comp[comp_id])
				continue;
			gdrm_disp2_base[i] = of_iomap(np, 0);
			ret = of_address_to_resource(np, 0, &res);
			if (ret < 0)
				DRM_INFO("comp_node[%d] map address fail\n", i);
			gdrm_disp2_reg_range[i].reg_base = res.start;
		}
		gdrm_disp2_base[i] = priv->config_regs;
		gdrm_disp2_reg_range[i++].reg_base = 0x14000000;
		gdrm_disp2_base[i] = mutex_regs;
		gdrm_disp2_reg_range[i].reg_base = mutex_phys;
	}
out:
	DRM_DEBUG_DRIVER("%s..done\n", __func__);
}

void mtk_drm_debugfs_deinit(void)
{
#if IS_ENABLED(CONFIG_DEBUG_FS)
	debugfs_remove(mtkdrm_dbgfs);
#endif
#if 0
	if (mtkdrm_dbgfs) {
		proc_remove(mtkdrm_dbgfs);
		mtkdrm_dbgfs = NULL;
	}
#endif
}
