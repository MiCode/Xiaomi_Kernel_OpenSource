/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */
#ifndef __SSPM_DEFINE_H__
#define __SSPM_DEFINE_H__

#define SSPM_SHARE_BUFFER_SUPPORT

#define SSPM_PLT_SERV_SUPPORT       (1)
#define SSPM_LOGGER_SUPPORT         (1)
#define SSPM_MBOX_SHARE_SUPPORT     (1)

#define PLT_INIT		0x504C5401
#define PLT_LOG_ENABLE		0x504C5402
#define PLT_SSC_INIT		0x504C5403

struct plt_msg_s {
	unsigned int cmd;
	union {
		struct {
			unsigned int phys;
			unsigned int size;
		} ctrl;
		struct {
			unsigned int enable;
		} logger;
	} u;
};

#endif
