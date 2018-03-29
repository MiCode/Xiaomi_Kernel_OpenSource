/*
 * Copyright (c) 2015 MediaTek Inc.
 * Author: Daniel Hsiao <daniel.hsiao@mediatek.com>
 *             Kai-Sean Yang <kai-sean.yang@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/delay.h>

#include "mtk_vcodec_drv.h"
#include "vdec_vp9_if.h"
#include "vdec_vp9_core.h"
#include "vdec_vp9_debug.h"

static int vp9_dec_klib_fwrite(char *buf, int len, struct file *filp)
{
	int writelen;
	mm_segment_t oldfs;

	if (filp == NULL)
		return -ENOENT;
	if (filp->f_op->write == NULL)
		return -ENOSYS;
	if (((filp->f_flags & O_ACCMODE) & (O_WRONLY | O_RDWR)) == 0)
		return -EACCES;

	oldfs = get_fs();
	set_fs(KERNEL_DS);
	writelen = filp->f_op->write(filp, buf, len, &filp->f_pos);
	set_fs(oldfs);

	return 0;
}

void vp9_dec_klib_fputs(char *str, struct file *filp)
{
	vp9_dec_klib_fwrite(str, strlen(str), filp);
}

void vp9_dec_fprintf(struct file *filp, const char *fmt, ...)
{
	static char s_buf[1024];
	va_list args;

	if (!filp)
		return;

	va_start(args, fmt);
	vsprintf(s_buf, fmt, args);
	va_end(args);

	vp9_dec_klib_fputs(s_buf, filp);
}

#if VP9_DEC_LOG_DUMP
struct file *vp9_file_log = NULL;
#endif

void vp9_dec_open_log_file(struct vdec_vp9_inst *handle)
{
	char *log_file_name = "/tmp/vp9_dec_kernel_x.log";

	log_file_name[20] = '0' + ((struct mtk_vcodec_ctx *)handle->ctx)->idx;
	handle->log = filp_open(log_file_name, O_RDWR | O_CREAT, 0);
	if (!handle->log || IS_ERR(handle->log)) {
		handle->log = 0;
		pr_err("fail to create the log file %s\n", log_file_name);
	}
#if VP9_DEC_LOG_DUMP
	vp9_file_log = handle->log;
#endif
}

void vp9_dec_close_log_file(struct vdec_vp9_inst *handle)
{
	if (handle->log) {
		filp_close(handle->log, NULL);
#if VP9_DEC_LOG_DUMP
		vp9_file_log = NULL;
#endif
	}
}

void vp9_dec_reg_dump(struct vdec_vp9_inst *handle)
{
	unsigned int i;
	unsigned int val;

	if (handle->frm_cnt > 0)
		return;

	vp9_dec_fprintf(handle->log, "==== VP9 Reg Dump Start ====\n");

	for (i = 0; i <= 0x10C; i += 4) {
		val = readl(handle->hw_reg_base.sys + i);
		vp9_dec_fprintf(handle->log, "VDEC_SYS_%d (0x%08x) = 0x%08x\n",
				i/4, 0x16000000 + i, val);
	}
	for (i = 0; i <= 0x160; i += 4) {
		val = readl(handle->hw_reg_base.misc + i);
		vp9_dec_fprintf(handle->log, "VDEC_MISC_%d (0x%08x) = 0x%08x\n",
				i/4, 0x16020000 + i, val);
	}
	for (i = 95*4; i <= 102*4; i += 4) {
		val = readl(handle->hw_reg_base.misc + i);
		vp9_dec_fprintf(handle->log, "VDEC_MISC_%d (0x%08x) = 0x%08x\n",
				i/4, 0x16020000 + i, val);
	}
	for (i = 0x84; i <= 0x3f4; i += 4) {
		val = readl(handle->hw_reg_base.ld + i);
		vp9_dec_fprintf(handle->log, "VDEC_VLD_%d (0x%08x) = 0x%08x\n",
				i/4, 0x16021000 + i, val);
	}
	for (i = 0; i <= 0x100; i += 4) {
		val = readl(handle->hw_reg_base.top + i);
		vp9_dec_fprintf(handle->log,
				"VDEC_VLD_TOP_%d (0x%08x) = 0x%08x\n", i/4,
				0x16021800 + i, val);
	}
	for (i = 0; i <= 0xcec; i += 4) {
		val = readl(handle->hw_reg_base.cm + i);
		vp9_dec_fprintf(handle->log, "VDEC_MC_%d (0x%08x) = 0x%08x\n",
				i/4, 0x16022000 + i, val);
	}
	for (i = 0; i <= 0x344; i += 4) {
		val = readl(handle->hw_reg_base.av + i);
		vp9_dec_fprintf(handle->log,
				"VDEC_AVC_MV_%d (0x%08x) = 0x%08x\n", i/4,
				0x16024000 + i, val);
	}
	for (i = 0; i <= 0x190; i += 4) {
		val = readl(handle->hw_reg_base.hwb + i);
		vp9_dec_fprintf(handle->log,
				"VDEC_VP8_VLD_%d (0x%08x) = 0x%08x\n", i/4,
				0x16027800 + i, val);
	}
	for (i = 0xa4; i <= 0x1f8; i += 4) {
		val = readl(handle->hw_reg_base.hwg + i);
		vp9_dec_fprintf(handle->log,
				"VDEC_VP9_VLD_%d (0x%08x) = 0x%08x\n", i/4,
				0x16028400 + i, val);
	}
	vp9_dec_fprintf(handle->log, "==== VP9 Reg Dump End ====\n");
}

void vp9_read_misc(struct vdec_vp9_inst *handle, unsigned int addr,
		   unsigned int *val)
{
	*val = readl(handle->hw_reg_base.misc + addr * 4);

	if (handle->show_reg)
		pr_info("		RISCRead_MISC(%u); // 0x%08x\n", addr,
			*val);
}
