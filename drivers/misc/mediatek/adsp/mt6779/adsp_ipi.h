/* SPDX-License-Identifier: GPL-2.0 */
/*
 * adsp_ipi.h --  Mediatek ADSP IPI interface
 *
 * Copyright (c) 2018 MediaTek Inc.
 */


#ifndef __ADSP_IPI_H
#define __ADSP_IPI_H

#include "adsp_reg.h"
#include "adsp_helper.h"

#define SHARE_BUF_SIZE 288
/* adsp awake timeout count definition*/
#define ADSP_AWAKE_TIMEOUT 5000
#define ADSP_IPI_STAMP_SUPPORT 0

/* adsp ipi ID definition
 * need to sync with ADSP-side
 */
enum adsp_ipi_id {
	ADSP_IPI_WDT = 0,
	ADSP_IPI_TEST1,
	ADSP_IPI_LOGGER_ENABLE,
	ADSP_IPI_LOGGER_WAKEUP,
	ADSP_IPI_LOGGER_INIT_A,
	ADSP_IPI_TRAX_ENABLE,
	ADSP_IPI_TRAX_DONE,
	ADSP_IPI_TRAX_INIT_A,
	ADSP_IPI_VOW,
	ADSP_IPI_AUDIO,
	ADSP_IPI_DVT_TEST,
	ADSP_IPI_TIME_SYNC,
	ADSP_IPI_CONSYS,
	ADSP_IPI_ADSP_A_READY,
	ADSP_IPI_APCCCI,
	ADSP_IPI_ADSP_A_RAM_DUMP,
	ADSP_IPI_DVFS_DEBUG,
	ADSP_IPI_DVFS_FIX_OPP_SET,
	ADSP_IPI_DVFS_FIX_OPP_EN,
	ADSP_IPI_DVFS_LIMIT_OPP_SET,
	ADSP_IPI_DVFS_LIMIT_OPP_EN,
	ADSP_IPI_DVFS_SUSPEND,
	ADSP_IPI_DVFS_SLEEP,
	ADSP_IPI_DVFS_WAKE,
	ADSP_IPI_DVFS_SET_FREQ,
	ADSP_IPI_ADSP_PLL_CTRL = 27,
	ADSP_IPI_MET_ADSP = 30,
	ADSP_IPI_ADSP_TIMER = 31,
	ADSP_NR_IPI,
};

enum adsp_ipi_status {
	ADSP_IPI_ERROR = -1,
	ADSP_IPI_DONE,
	ADSP_IPI_BUSY,
};



struct adsp_ipi_desc {
	void (*handler)(int id, void *data, unsigned int len);
	const char *name;
};


struct adsp_share_obj {
	enum adsp_ipi_id id;
	unsigned int len;
	unsigned char reserve[8];
	unsigned char share_buf[SHARE_BUF_SIZE - 16];
};

extern enum adsp_ipi_status adsp_ipi_registration(enum adsp_ipi_id id,
						  void (*ipi_handler)(int id,
						  void *data, unsigned int len),
						  const char *name);
extern enum adsp_ipi_status adsp_ipi_unregistration(enum adsp_ipi_id id);
extern enum adsp_ipi_status adsp_ipi_send(enum adsp_ipi_id id, void *buf,
					  unsigned int len, unsigned int wait,
					  enum adsp_core_id adsp_id);
extern enum adsp_ipi_status adsp_ipi_send_ipc(enum adsp_ipi_id id, void *buf,
					  unsigned int len, unsigned int wait,
					  enum adsp_core_id adsp_id);

extern void adsp_A_ipi_handler(void);

extern char *adsp_core_ids[ADSP_CORE_TOTAL];
extern int adsp_awake_lock(enum adsp_core_id adsp_id);
extern int adsp_awake_unlock(enum adsp_core_id adsp_id);
extern void adsp_awake_init(void);
extern int adsp_awake_dump_list(enum adsp_core_id adsp_id);
extern int adsp_awake_force_lock(enum adsp_core_id adsp_id);
extern int adsp_awake_force_unlock(enum adsp_core_id adsp_id);
extern int adsp_awake_set_normal(enum adsp_core_id adsp_id);
extern int adsp_awake_unlock_adsppll(enum adsp_core_id adsp_id,
				     uint32_t unlock);
#endif
