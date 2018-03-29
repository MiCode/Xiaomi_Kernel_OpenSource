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

#ifndef _VDEC_VP9_DEBUG_H_
#define _VDEC_VP9_DEBUG_H_

#include <linux/file.h>
#include <linux/fs.h>
#include <linux/delay.h>

#define VP9_DEC_LOG_EN 1
#define VP9_DEC_LOG_DUMP 0

void vp9_dec_open_log_file(struct vdec_vp9_inst *handle);
void vp9_dec_close_log_file(struct vdec_vp9_inst *handle);
void vp9_dec_fprintf(struct file *filp, const char *fmt, ...);
void vp9_dec_reg_dump(struct vdec_vp9_inst *handle);

#if VP9_DEC_LOG_DUMP
extern struct file *vp9_file_log;
#undef VP9_DEC_LOGE
#undef VP9_DEC_LOGI
#undef VP9_DEC_LOGD

#define VP9_DEC_LOGE(fmt, ...) \
	vp9_dec_fprintf(vp9_file_log, "[VP9][E] "fmt, ##__VA_ARGS__)
#define VP9_DEC_LOGI(fmt, ...) \
	vp9_dec_fprintf(vp9_file_log, "[VP9][I] "fmt, ##__VA_ARGS__)
#define VP9_DEC_LOGD(fmt, ...) \
	vp9_dec_fprintf(vp9_file_log, "[VP9][D] "fmt, ##__VA_ARGS__)
#endif

void vp9_read_misc(struct vdec_vp9_inst *handle, unsigned int addr,
		   unsigned int *val);

#endif
