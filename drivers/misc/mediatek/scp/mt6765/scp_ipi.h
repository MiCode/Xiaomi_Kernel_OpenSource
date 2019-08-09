/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __SCP_IPI_H
#define __SCP_IPI_H

#include "scp_reg.h"

#define SHARE_BUF_SIZE 288
/* scp awake timout count definition*/
#define SCP_AWAKE_TIMEOUT 100000
#define SCP_IPI_STAMP_SUPPORT 0

/* scp Core ID definition*/
enum scp_core_id {
	SCP_A_ID = 0,
	SCP_CORE_TOTAL = 1,
};

/* scp ipi ID definition
 * need to sync with SCP-side
 */
enum ipi_id {
	IPI_WDT = 0,
	IPI_TEST1,
	IPI_LOGGER_ENABLE,
	IPI_LOGGER_WAKEUP,
	IPI_LOGGER_INIT_A,
	IPI_VOW,                    /* 5 */
	IPI_AUDIO,
	IPI_DVT_TEST,
	IPI_SENSOR,
	IPI_TIME_SYNC,
	IPI_SHF,                    /* 10 */
	IPI_CONSYS,
	IPI_SCP_A_READY,
	IPI_APCCCI,
	IPI_SCP_A_RAM_DUMP,
	IPI_DVFS_DEBUG,             /* 15 */
	IPI_DVFS_FIX_OPP_SET,
	IPI_DVFS_FIX_OPP_EN,
	IPI_DVFS_LIMIT_OPP_SET,
	IPI_DVFS_LIMIT_OPP_EN,
	IPI_DVFS_DISABLE,           /* 20 */
	IPI_DVFS_SLEEP,
	IPI_PMICW_MODE_DEBUG,
	IPI_DVFS_SET_FREQ,
	IPI_CHRE,
	IPI_CHREX,                  /* 25 */
	IPI_SCP_PLL_CTRL,
	IPI_DO_AP_MSG,
	IPI_DO_SCP_MSG,
	IPI_MET_SCP,
	IPI_SCP_TIMER,              /* 30 */
	IPI_SCP_ERROR_INFO,
	IPI_SCPCTL,
	IPI_SCP_LOG_FILTER,
	SCP_NR_IPI,
};

enum scp_ipi_status {
	SCP_IPI_ERROR = -1,
	SCP_IPI_DONE,
	SCP_IPI_BUSY,
};



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

extern enum scp_ipi_status scp_ipi_registration(enum ipi_id id,
	void (*ipi_handler)(int id, void *data, unsigned int len),
	const char *name);
extern enum scp_ipi_status scp_ipi_unregistration(enum ipi_id id);
extern enum scp_ipi_status scp_ipi_send(enum ipi_id id, void *buf,
	unsigned int len, unsigned int wait, enum scp_core_id scp_id);

extern void scp_A_ipi_handler(void);
extern int wake_up_scp(void);

extern unsigned char *scp_send_buff[SCP_CORE_TOTAL];
extern unsigned char *scp_recv_buff[SCP_CORE_TOTAL];
extern char *core_ids[SCP_CORE_TOTAL];

extern void scp_reset_awake_counts(void);
extern int scp_awake_lock(enum scp_core_id scp_id);
extern int scp_awake_unlock(enum scp_core_id scp_id);
extern int scp_awake_counts[];

extern unsigned int is_scp_ready(enum scp_core_id scp_id);
extern void scp_ipi_status_dump(void);
extern void scp_ipi_status_dump_id(enum ipi_id id);
#endif
