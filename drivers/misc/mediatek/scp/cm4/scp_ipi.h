/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SCP_IPI_H
#define __SCP_IPI_H

#include "scp_reg.h"
#include <include/scp.h>

#define SHARE_BUF_SIZE 288
/* scp awake timeout count definition*/
#define SCP_AWAKE_TIMEOUT 100000
#define SCP_IPI_STAMP_SUPPORT 0

struct scp_ipi_desc {
	void (*handler)(int id, void *data, unsigned int len);
#if SCP_IPI_STAMP_SUPPORT
#define SCP_IPI_ID_STAMP_SIZE 5
	unsigned long long recv_timestamp[SCP_IPI_ID_STAMP_SIZE];
	unsigned long long handler_timestamp[SCP_IPI_ID_STAMP_SIZE];
	unsigned long long send_timestamp[SCP_IPI_ID_STAMP_SIZE];
	unsigned int recv_flag[SCP_IPI_ID_STAMP_SIZE];
	unsigned int send_flag[SCP_IPI_ID_STAMP_SIZE];
#endif
	unsigned int recv_count;
	unsigned int success_count;
	unsigned int busy_count;
	unsigned int error_count;
	const char *name;
};

struct share_obj {
	enum ipi_id id;
	unsigned int len;
	unsigned char reserve[8];
	unsigned char share_buf[SHARE_BUF_SIZE - 16];
};

extern void scp_A_ipi_handler(void);
extern int wake_up_scp(void);

extern unsigned char *scp_send_buff[SCP_CORE_TOTAL];
extern unsigned char *scp_recv_buff[SCP_CORE_TOTAL];
extern char *core_ids[SCP_CORE_TOTAL];

extern void scp_reset_awake_counts(void);
extern int scp_awake_counts[];

extern void scp_ipi_status_dump(void);
extern void scp_ipi_status_dump_id(enum ipi_id id);
#endif
