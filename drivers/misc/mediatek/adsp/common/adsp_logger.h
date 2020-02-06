/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ADSP_LOGGER_H__
#define __ADSP_LOGGER_H__

#define ADSP_LOGGER_UT              (1)

struct log_ctrl_s {
	unsigned int base;
	unsigned int size;
	unsigned int enable;
	unsigned int info_ofs;
	unsigned int buff_ofs;
	unsigned int buff_size;
	unsigned int inited;     /* ap used */
	struct mutex lock;       /* ap used */
	struct delayed_work work; /* ap used */
};

struct buffer_info_s {
	unsigned int r_pos;
	unsigned char resv1[124]; /* dummy bytes for 128-byte align */
	unsigned int w_pos;
	unsigned char resv2[124]; /* dummy bytes for 128-byte align */
};

struct log_ctrl_s *adsp_logger_init(int mem_id);
unsigned int adsp_log_poll(struct log_ctrl_s *ctrl);
ssize_t adsp_log_read(struct log_ctrl_s *ctrl, char __user *userbuf,
		      size_t len);
ssize_t adsp_log_enable(struct log_ctrl_s *ctrl, int cid, u32 enable);
ssize_t adsp_dump_log_state(struct log_ctrl_s *ctrl, char *buffer, int size);

#endif /* __ADSP_LOGGER_H__ */

