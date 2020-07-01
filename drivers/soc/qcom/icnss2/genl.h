/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved. */

#ifndef __ICNSS_GENL_H__
#define __ICNSS_GENL_H__

enum icnss_genl_msg_type {
	ICNSS_GENL_MSG_TYPE_UNSPEC,
	ICNSS_GENL_MSG_TYPE_QDSS,
};

int icnss_genl_init(void);
void icnss_genl_exit(void);
int icnss_genl_send_msg(void *buff, u8 type,
			char *file_name, u32 total_size);

#endif
