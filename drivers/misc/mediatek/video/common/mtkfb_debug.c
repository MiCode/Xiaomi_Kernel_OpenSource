/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/string.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/fb.h>
#include <linux/vmalloc.h>
#include <linux/sched.h>
#include <linux/debugfs.h>
#include <linux/wait.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/types.h>
#include <mt-plat/aee.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#ifdef CONFIG_MTK_LEGACY
#include <mach/mt_gpio.h>
#include <cust_gpio_usage.h>
#else
#include "disp_dts_gpio.h"
#endif

#include "m4u.h"
#include "cmdq_def.h"
#include "cmdq_record.h"
#include "cmdq_reg.h"
#include "cmdq_core.h"

#include "lcm_drv.h"
#include "ddp_path.h"
#include "ddp_reg.h"
#include "ddp_drv.h"
#include "ddp_wdma.h"
#include "ddp_hal.h"
#include "ddp_aal.h"
#include "ddp_pwm.h"
#include "ddp_dither.h"
#include "ddp_info.h"
#include "ddp_dsi.h"
#include "ddp_rdma.h"
#include "ddp_manager.h"
#include "ddp_met.h"
#include "disp_log.h"
#include "disp_debug.h"
#include "disp_helper.h"
#include "disp_drv_ddp.h"
#include "disp_recorder.h"
#include "disp_session.h"
#include "disp_assert_layer.h"
#include "mtkfb.h"
#include "mtkfb_fence.h"
#include "mtkfb_debug.h"
#include "primary_display.h"
#if defined(CONFIG_ARCH_MT6755)
#include "disp_lowpower.h"
#include "disp_recovery.h"
#endif

#pragma GCC optimize("O0")

/* --------------------------------------------------------------------------- */
/* Global variable declarations */
/* --------------------------------------------------------------------------- */
unsigned int g_enable_uart_log = 0;
unsigned int g_mobilelog = 1;
unsigned int g_fencelog = 0; /*Fence Log*/
#ifdef CONFIG_ARCH_MT6570
unsigned int g_loglevel = 5; /*DISPMSG level is DEFAULT_LEVEL==3*/
#else
unsigned int g_loglevel = 3;
#endif
unsigned int g_rcdlevel = 0;
unsigned int dbg_log_level;
unsigned int irq_log_level;
unsigned char pq_debug_flag = 0;
unsigned char aal_debug_flag = 0;
unsigned int gUltraEnable = 1;
int lcm_mode_status = 0;
int bypass_blank = 0;
struct dentry *disp_debugDir;

/* --------------------------------------------------------------------------- */
/* Local variable declarations */
/* --------------------------------------------------------------------------- */
static struct dentry *dispsys_debugfs;
static struct dentry *dump_debugfs;
static struct dentry *mtkfb_debugfs;
static int debug_init;
static unsigned int dump_to_buffer;

/* --------------------------------------------------------------------------- */
/* DDP debugfs functions */
/* --------------------------------------------------------------------------- */
char dbg_buf[2048];

unsigned int is_reg_addr_valid(unsigned int isVa, unsigned long addr)
{
	unsigned int i = 0;

	for (i = 0; i < DISP_REG_NUM; i++) {
		if ((isVa == 1) && (addr > dispsys_reg[i]) && (addr < dispsys_reg[i] + 0x1000))
			break;
		if ((isVa == 0) && (addr > ddp_reg_pa_base[i])
		    && (addr < ddp_reg_pa_base[i] + 0x1000))
			break;
	}

	if (i < DISP_REG_NUM) {
		DISPMSG("addr valid, isVa=0x%x, addr=0x%lx, module=%s!\n", isVa, addr,
		       ddp_get_reg_module_name(i));
		return 1;
	}

	DISPERR("is_reg_addr_valid return fail, isVa=0x%x, addr=0x%lx!\n", isVa, addr);
	return 0;
}

int ddp_mem_test(void)
{
	return -1;
}

int ddp_lcd_test(void)
{
	return -1;
}

static void ddp_process_dbg_cmd(char *cmd)
{
	char *tok;

	DISPMSG("cmd: %s\n", cmd);
	memset(dbg_buf, 0, sizeof(dbg_buf));
	while ((tok = strsep(&cmd, " ")) != NULL)
		ddp_process_dbg_opt(tok);
}

static int ddp_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static char cmd_buf[512];

static ssize_t ddp_debug_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	if (strlen(dbg_buf))
		return simple_read_from_buffer(ubuf, count, ppos, dbg_buf, strlen(dbg_buf));
	else
		return simple_read_from_buffer(ubuf, count, ppos, DDP_STR_HELP, strlen(DDP_STR_HELP));

}

static ssize_t ddp_debug_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = sizeof(cmd_buf) - 1;
	size_t ret;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(&cmd_buf, ubuf, count))
		return -EFAULT;

	cmd_buf[count] = 0;

	ddp_process_dbg_cmd(cmd_buf);

	return ret;
}

static const struct file_operations ddp_debug_fops = {
	.read = ddp_debug_read,
	.write = ddp_debug_write,
	.open = ddp_debug_open,
};

unsigned int ddp_debug_dbg_log_level(void)
{
	return dbg_log_level;
}

unsigned int ddp_debug_irq_log_level(void)
{
	return irq_log_level;
}

/* --------------------------------------------------------------------------- */
/* DUMP debugfs functions */
/* --------------------------------------------------------------------------- */
static ssize_t dump_debug_read(struct file *file, char __user *buf, size_t size, loff_t *ppos)
{
#if defined(CONFIG_ARCH_MT6755)
	char *str = "idlemgr disable mtcmos now, all the regs may 0x00000000\n";

	if (is_mipi_enterulps())
		return simple_read_from_buffer(buf, size, ppos, str, strlen(str));
#endif
	dprec_logger_dump_reset();
	dump_to_buffer = 1;

	/* dump all */
	dpmgr_debug_path_status(-1);
	dump_to_buffer = 0;
	return simple_read_from_buffer(buf, size, ppos, dprec_logger_get_dump_addr(),
			       dprec_logger_get_dump_len());

}

static const struct file_operations dump_debug_fops = {
	.read = dump_debug_read,
};

unsigned int ddp_debug_analysis_to_buffer(void)
{
	return dump_to_buffer;
}

/* --------------------------------------------------------------------------- */
/* MTKFB debugfs functions */
/* --------------------------------------------------------------------------- */
static int _draw_line(unsigned long addr, int l, int t, int r, int b,
		      int linepitch, unsigned int color)
{
	int i = 0;

	if (l > r || b < t)
		return -1;

	if (l == r) {		/* vertical line */
		for (i = 0; i < (b - t); i++) {
			*(unsigned long *)(addr + (t + i) * linepitch + l * 4) =
			    color;
		}
	} else if (t == b) {	/* horizontal line */
		for (i = 0; i < (r - l); i++) {
			*(unsigned long *)(addr + t * linepitch + (l + i) * 4) =
			    color;
		}
	} else {		/* tile line, not support now */
		return -1;
	}

	return 0;
}

static int _draw_rect(unsigned long addr, int l, int t, int r, int b, unsigned int linepitch,
		      unsigned int color)
{
	int ret = 0;

	ret += _draw_line(addr, l, t, r, t, linepitch, color);
	ret += _draw_line(addr, l, t, l, b, linepitch, color);
	ret += _draw_line(addr, r, t, r, b, linepitch, color);
	ret += _draw_line(addr, l, b, r, b, linepitch, color);
	return ret;
}

static void _draw_block(unsigned long addr, unsigned int x, unsigned int y,
			unsigned int w, unsigned int h, unsigned int linepitch,
			unsigned int color)
{
	int i = 0;
	int j = 0;
	unsigned long start_addr = addr + linepitch * y + x * 4;

	DISPMSG
	    ("addr=0x%lx, start_addr=0x%lx, x=%d,y=%d,w=%d,h=%d,linepitch=%d, color=0x%08x\n",
	     addr, start_addr, x, y, w, h, linepitch, color);
	for (j = 0; j < h; j++) {
		for (i = 0; i < w; i++) {
			*(unsigned long *)(start_addr + i * 4 + j * linepitch) =
			    color;
		}
	}
}


int g_display_debug_pattern_index;
void _debug_pattern(unsigned long mva, unsigned long va, unsigned int w, unsigned int h,
		    unsigned int linepitch, unsigned int color, unsigned int layerid,
		    unsigned int bufidx)
{
	int ret = 0;
	unsigned long addr = 0;
	unsigned int layer_size = 0;
	unsigned int mapped_size = 0;
	unsigned int bcolor = 0xff808080;

	if (g_display_debug_pattern_index == 0)
		return;

	if (layerid == 0)
		bcolor = 0x0000ffff;
	else if (layerid == 1)
		bcolor = 0x00ff00ff;
	else if (layerid == 2)
		bcolor = 0xff0000ff;
	else if (layerid == 3)
		bcolor = 0xffff00ff;

	if (va) {
		addr = va;
	} else {
		layer_size = linepitch * h;
		ret = m4u_mva_map_kernel(mva, layer_size, &addr, &mapped_size);
		if ((ret < 0) || (mapped_size == 0)) {
			DISPERR("m4u_mva_map_kernel fail in %s to map mva = 0x%lx\n", __func__, mva);
			return;
		}
	}

	switch (g_display_debug_pattern_index) {
	case 1:
		{
			unsigned int resize_factor = layerid + 1;

			_draw_rect(addr, w / 10 * resize_factor + 0,
				   h / 10 * resize_factor + 0,
				   w / 10 * (10 - resize_factor) - 0,
				   h / 10 * (10 - resize_factor) - 0, linepitch,
				   bcolor);
			_draw_rect(addr, w / 10 * resize_factor + 1,
				   h / 10 * resize_factor + 1,
				   w / 10 * (10 - resize_factor) - 1,
				   h / 10 * (10 - resize_factor) - 1, linepitch,
				   bcolor);
			_draw_rect(addr, w / 10 * resize_factor + 2,
				   h / 10 * resize_factor + 2,
				   w / 10 * (10 - resize_factor) - 2,
				   h / 10 * (10 - resize_factor) - 2, linepitch,
				   bcolor);
			_draw_rect(addr, w / 10 * resize_factor + 3,
				   h / 10 * resize_factor + 3,
				   w / 10 * (10 - resize_factor) - 3,
				   h / 10 * (10 - resize_factor) - 3, linepitch,
				   bcolor);
			break;
		}
	case 2:
		{
			int bw = 20;
			int bh = 20;

			_draw_block(addr, bufidx % (w / bw) * bw,
				    bufidx % (w * h / bh / bh) / (w / bh) * bh,
				    bw, bh, linepitch, bcolor);
			break;
		}
	}
#ifndef CONFIG_FPGA_EARLY_PORTING
	/* smp_inner_dcache_flush_all(); */
	/* outer_flush_all();//remove in early porting */
#endif
	if (mapped_size)
		m4u_mva_unmap_kernel(addr, layer_size, addr);
}

static void mtkfb_process_dbg_cmd(char *cmd)
{
	char *tok;

	DISPMSG("[mtkfb_dbg] %s\n", cmd);

	while ((tok = strsep(&cmd, " ")) != NULL)
		mtkfb_process_dbg_opt(tok);
}

static int mtkfb_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

int debug_get_info(unsigned char *stringbuf, int buf_len)
{
	int n = 0;

	DISPFUNC();

	n += mtkfb_get_debug_state(stringbuf + n, buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += primary_display_get_debug_state(stringbuf + n, buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += disp_sync_get_debug_info(stringbuf + n, buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += dprec_logger_get_result_string_all(stringbuf + n, buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += disp_helper_get_option_list(stringbuf + n, buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += dprec_logger_get_buf(DPREC_LOGGER_ERROR, stringbuf + n, buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	n += dprec_logger_get_buf(DPREC_LOGGER_FENCE, stringbuf + n, buf_len - n);
	DISPMSG("%s,%d, n=%d\n", __func__, __LINE__, n);

	stringbuf[n++] = 0;
	return n;
}

void debug_info_dump_to_printk(char *buf, int buf_len)
{
	int i = 0;
	int n = buf_len;

	for (i = 0; i < n; i += 256)
		DISPMSG("%s", buf + i);
}

static ssize_t mtkfb_debug_read(struct file *file, char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = DPREC_ERROR_LOG_BUFFER_LENGTH - 1;
	static int n;

	if (*ppos != 0 || !is_buffer_init)
		goto out;

	n = mtkfb_get_debug_state(debug_buffer + n, debug_bufmax - n);

	n += primary_display_get_debug_state(debug_buffer + n, debug_bufmax - n);

	n += disp_sync_get_debug_info(debug_buffer + n, debug_bufmax - n);

	n += dprec_logger_get_result_string_all(debug_buffer + n, debug_bufmax - n);

	n += disp_helper_get_option_list(debug_buffer + n, debug_bufmax - n);

	n += dprec_logger_get_buf(DPREC_LOGGER_ERROR, debug_buffer + n, debug_bufmax - n);

	n += dprec_logger_get_buf(DPREC_LOGGER_FENCE, debug_buffer + n, debug_bufmax - n);

	n += dprec_logger_get_buf(DPREC_LOGGER_DUMP, debug_buffer + n, debug_bufmax - n);

	n += dprec_logger_get_buf(DPREC_LOGGER_DEBUG, debug_buffer + n, debug_bufmax - n);

	n += dprec_logger_get_buf(DPREC_LOGGER_STATUS, debug_buffer + n, debug_bufmax - n);

	debug_buffer[n++] = 0;
out:
	return simple_read_from_buffer(ubuf, count, ppos, debug_buffer, n);
}

static ssize_t mtkfb_debug_write(struct file *file, const char __user *ubuf, size_t count, loff_t *ppos)
{
	const int debug_bufmax = DPREC_ERROR_LOG_BUFFER_LENGTH - 1;
	size_t ret = 0;

	if (!is_buffer_init)
		goto out;

	ret = count;

	if (count > debug_bufmax)
		count = debug_bufmax;

	if (copy_from_user(debug_buffer, ubuf, count))
		return -EFAULT;

	debug_buffer[count] = 0;

	mtkfb_process_dbg_cmd(debug_buffer);

out:
	return ret;
}

static const struct file_operations mtkfb_debug_fops = {
	.read = mtkfb_debug_read,
	.write = mtkfb_debug_write,
	.open = mtkfb_debug_open,
};

/* --------------------------------------------------------------------------- */
/* Debugfs register in debug system */
/* --------------------------------------------------------------------------- */
void DBG_Init(void)
{
	if (!debug_init) {
		debug_init = 1;
		mtkfb_debugfs = debugfs_create_file("mtkfb",
						  S_IFREG | S_IRUGO, NULL, (void *)0, &mtkfb_debug_fops);

		dispsys_debugfs = debugfs_create_file("dispsys",
						      S_IFREG | S_IRUGO, NULL, (void *)0, &ddp_debug_fops);

		disp_debugDir = debugfs_create_dir("disp", NULL);
		if (disp_debugDir) {
			dump_debugfs = debugfs_create_file("dump",
							   S_IFREG | S_IRUGO, disp_debugDir, NULL,
							   &dump_debug_fops);

			/* by Chip, sub debugfs define, and sub debugfs must be in disp folder. */
			sub_debug_init();
		}
	}
}

void DBG_Deinit(void)
{
	/* by Chip, sub debugfs remove */
	sub_debug_deinit();

	debugfs_remove(mtkfb_debugfs);
	debugfs_remove(dispsys_debugfs);
	debugfs_remove(dump_debugfs);
	debugfs_remove_recursive(disp_debugDir);
	debug_init = 0;
}
