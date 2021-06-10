/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 */

#ifndef __TEEI_CLIENT_MAIN_H__
#define __TEEI_CLIENT_MAIN_H__

#include <linux/suspend.h>

#include <teei_ioc.h>
#include "teei_smc_struct.h"
#include "teei_client.h"
/* #define UT_DEBUG */
#define CANCEL_BUFF_SIZE                (4096)
#define TEEI_CONFIG_FULL_PATH_DEV_NAME	"/dev/teei_config"
#define TEEI_CONFIG_DEV			"teei_config"
#define MIN_BC_NUM			(4)
#define MAX_LC_NUM			(3)
#define TEEI_CPU_0

#define TEEI_TA 0x01
#define TEEI_DRV 0x00

enum {
	TZ_CALL_PREPARE,
	TZ_CALL_RETURNED,
};

struct tz_driver_state {
	struct mutex smc_lock;
	struct atomic_notifier_head notifier;
	struct platform_device *tz_log_pdev;
};

struct tz_driver_state *get_tz_drv_state(void);

extern unsigned int soter_error_flag;

extern struct workqueue_struct *secure_wq;
extern struct semaphore boot_sema;
extern struct smc_call_struct smc_call_entry;
extern unsigned long boot_vfs_addr;
extern unsigned long boot_soter_flag;
extern int keymaster_call_flag;
extern struct completion boot_decryto_lock;
extern struct task_struct *teei_switch_task;
extern struct kthread_worker ut_fastcall_worker;
extern unsigned long spi_ready_flag;
extern struct list_head g_block_link;

int get_current_cpuid(void);

void *tz_malloc_shared_mem(size_t size, int flags);
void tz_free_shared_mem(void *addr, size_t size);
void *tz_malloc(size_t size, int flags);

int handle_switch_core(int cpu);

int is_teei_ready(void);
int teei_new_capi_init(void);
int handle_new_capi_call(void *args);

int tz_load_drv_by_str(const char *buf);
int tz_load_ta_by_str(const char *buf);

int teei_handle_move_core(int cpu);
int teei_move_cpu_context(int target_cpu_id, int original_cpu_id);

void teei_cpus_read_lock(void);
void teei_cpus_read_unlock(void);
void teei_cpus_write_lock(void);
void teei_cpus_write_unlock(void);

int teei_set_switch_pri(unsigned long policy);
#endif /* __TEEI_CLIENT_MAIN_H__ */
