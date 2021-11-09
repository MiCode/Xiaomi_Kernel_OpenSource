/* SPDX-License-Identifier: GPL-2.0 */
/*
 * adsp_logger.h --  Mediatek ADSP logger
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: HsinYi Chang <hsin-yi.chang@mediatek.com>
 */

#ifndef __ADSP_LOGGER_H__
#define __ADSP_LOGGER_H__

#define ADSP_LOGGER_UT              (0)

struct log_ctrl_s {
	unsigned int base;
	unsigned int size;
	unsigned int enable;
	unsigned int info_ofs;
	unsigned int buff_ofs;
	unsigned int buff_size;
	unsigned char resv1[104]; /* dummy bytes for 128-byte align */
};

struct buffer_info_s {
	unsigned int r_pos;
	unsigned char resv1[124]; /* dummy bytes for 128-byte align */
	unsigned int w_pos;
	unsigned char resv2[124]; /* dummy bytes for 128-byte align */
};

int adsp_logger_init(void);

/* device fops */
ssize_t adsp_A_log_if_read(struct file *file, char __user *data,
			   size_t len, loff_t *ppos);
int adsp_A_log_if_open(struct inode *inode, struct file *file);
unsigned int adsp_A_log_if_poll(struct file *file, poll_table *wait);

#if ADSP_TRAX
struct trax_ctrl_s {
	unsigned int done;
	unsigned int length;
	unsigned int enable;
	unsigned int initiated;
};

int adsp_trax_init(void);
#endif

#endif /* __ADSP_LOGGER_H__ */

