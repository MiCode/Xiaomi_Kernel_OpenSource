/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __ADSP_HELPER_H__
#define __ADSP_HELPER_H__

#include <linux/notifier.h>
#include "adsp_reserved_mem.h"
#include "adsp_feature_define.h"

enum adsp_ipi_id {
	ADSP_IPI_WDT = 0,
	ADSP_IPI_TEST1,
	ADSP_IPI_LOGGER_ENABLE,
	ADSP_IPI_LOGGER_WAKEUP,
	ADSP_IPI_LOGGER_INIT,
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

enum adsp_status {
	ADSP_ERROR = -1,
	ADSP_OK,
	ADSP_SEMAPHORE_BUSY,
};

/* adsp notify event */
enum ADSP_NOTIFY_EVENT {
	ADSP_EVENT_STOP = 0,
	ADSP_EVENT_READY,
};

enum semaphore_3way_flag {
	SEMA_3WAY_UART =  0,
	SEMA_3WAY_C2C_A = 1,
	SEMA_3WAY_C2C_B = 2,
	SEMA_3WAY_DVFS =  3,
	SEMA_3WAY_AUDIO = 4,
	SEMA_3WAY_AUDIOREG = 5,
	SEMA_3WAY_NUM =   7,
};

#define SEMA_AUDIO             SEMA_3WAY_AUDIO
#define SEMA_AUDIOREG          SEMA_3WAY_AUDIOREG
#define ADSP_OSTIMER_BUFFER    (adsp_timesync_ptr)

extern void *adsp_timesync_ptr;

extern enum adsp_ipi_status adsp_ipi_registration(enum adsp_ipi_id id,
						  void (*ipi_handler)(int id,
						  void *data, unsigned int len),
						  const char *name);
extern enum adsp_ipi_status adsp_ipi_unregistration(enum adsp_ipi_id id);
extern enum adsp_ipi_status adsp_push_message(enum adsp_ipi_id id, void *buf,
			unsigned int len, unsigned int wait_ms,
			unsigned int core_id);
extern enum adsp_ipi_status adsp_send_message(enum adsp_ipi_id id, void *buf,
			unsigned int len, unsigned int wait,
			unsigned int core_id);
extern int is_adsp_ready(u32 cid);
extern bool is_adsp_feature_in_active(void);
extern int adsp_feature_in_which_core(enum adsp_feature_id fid);
extern int adsp_register_feature(enum adsp_feature_id fid);
extern int adsp_deregister_feature(enum adsp_feature_id fid);

extern void adsp_register_notify(struct notifier_block *nb);
extern void adsp_unregister_notify(struct notifier_block *nb);
extern void reset_hal_feature_table(void);

/* If device interrupt is not connected, return -ENOTCONN. */
extern int adsp_irq_registration(u32 core_id, u32 irq_id, void *handler,
				 void *data);

/* semaphore */
extern int get_adsp_semaphore(unsigned int flags);
extern int release_adsp_semaphore(unsigned int flags);

extern void adsp_register_notify(struct notifier_block *nb);
extern void adsp_enable_dsp_clk(bool enable);
#endif
