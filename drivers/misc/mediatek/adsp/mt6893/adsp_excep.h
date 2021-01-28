/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __ADSP_EXCEP_H__
#define __ADSP_EXCEP_H__

#include <linux/interrupt.h>

#define ADSP_AED_STR_LEN    (512)
#define ADSP_WDT_TIMEOUT    (60 * HZ)    /* 60 seconds*/
#define ADSP_KE_DUMP_LEN    (256 * 1024)
#define ADSP_RESET_RETRY_MAXTIME  (7)

enum adsp_excep_id {
	EXCEP_LOAD_FIRMWARE = 0,
	EXCEP_RESET,
	EXCEP_BOOTUP,
	EXCEP_RUNTIME,
	EXCEP_KERNEL,
	ADSP_NR_EXCEP,
};

/* adsp reg dump */
struct adsp_coredump {
	u32 reserved_0[67];
	u32 pc;
	u32 exccause;
	u32 excvaddr;
	u32 reserved_1[7];
	u8 task_name[16];
	u32 reserved_2[47];
	u8 assert_log[512];
};

struct adsp_exception_control {
	int excep_id;
	void *priv_data;
	void *buf_backup;
	size_t buf_size;

	struct mutex lock;
	struct workqueue_struct *workq;
	struct wait_queue_head *waitq;
	struct work_struct aed_work;
	struct wakeup_source wakeup_lock;
	struct completion done;

	struct timer_list wdt_timer;
	unsigned int wdt_counter;
};

void adsp_wdt_handler(int irq, void *data, int cid);
bool adsp_aed_dispatch(enum adsp_excep_id type, void *data);
int init_adsp_exception_control(struct workqueue_struct *wq,
				struct wait_queue_head *waitq);

#endif
