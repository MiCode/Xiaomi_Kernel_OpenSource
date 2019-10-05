/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __TEEI_CLIENT_MAIN_H__
#define __TEEI_CLIENT_MAIN_H__

#include <linux/suspend.h>

#ifdef TUI_SUPPORT
#define POWER_DOWN			"power-detect"
#endif
#include <teei_ioc.h>
#include "teei_smc_struct.h"
#include "teei_client.h"
#include "teei_capi.h"
/* #define UT_DEBUG */
#define CANCEL_BUFF_SIZE                (4096)
#define TEEI_CONFIG_FULL_PATH_DEV_NAME	"/dev/teei_config"
#define TEEI_CONFIG_DEV			"teei_config"
#define MIN_BC_NUM			(4)
#define MAX_LC_NUM			(3)
#define TEEI_CPU_0

#define TEEI_TA 0x01
#define TEEI_DRV 0x00

extern struct teei_context *teei_create_context(int dev_count);
extern struct teei_session *teei_create_session(struct teei_context *cont);
extern int teei_client_context_init(void *private_data, void *argp);
extern int teei_client_context_close(void *private_data, void *argp);
extern int teei_client_session_init(void *private_data, void *argp);
extern int teei_client_session_open(void *private_data, void *argp);
extern int teei_client_session_close(void *private_data, void *argp);
extern int teei_client_send_cmd(void *private_data, void *argp);
extern int teei_client_operation_release(void *private_data, void *argp);
extern int teei_client_prepare_encode(void *private_data,
					struct teei_client_encode_cmd *enc,
					struct teei_encode **penc_context,
					struct teei_session **psession);
extern int teei_client_encode_uint32(void *private_data, void *argp);
extern int teei_client_encode_array(void *private_data, void *argp);
extern int teei_client_encode_mem_ref(void *private_data, void *argp);
extern int teei_client_encode_uint32_64bit(void *private_data, void *argp);
extern int teei_client_encode_array_64bit(void *private_data, void *argp);
extern int teei_client_encode_mem_ref_64bit(void *private_data, void *argp);
extern int teei_client_prepare_decode(void *private_data,
					struct teei_client_encode_cmd *dec,
					struct teei_encode **pdec_context);
extern int teei_client_decode_uint32(void *private_data, void *argp);
extern int teei_client_decode_array_space(void *private_data, void *argp);
extern int teei_client_get_decode_type(void *private_data, void *argp);
extern int teei_client_shared_mem_alloc(void *private_data, void *argp);
extern int teei_client_shared_mem_free(void *private_data, void *argp);
extern int teei_client_service_exit(void *private_data);
extern void *__teei_client_map_mem(unsigned long dev_file_id,
				unsigned long size, unsigned long user_addr);
extern long __teei_client_open_dev(void);

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
int tz_call_notifier_register(struct notifier_block *n);
int tz_call_notifier_unregister(struct notifier_block *n);

extern unsigned long device_file_cnt;
extern struct semaphore api_lock;
extern struct semaphore fp_api_lock;
extern struct semaphore keymaster_api_lock;
extern struct semaphore capi_mutex;
extern struct workqueue_struct *secure_wq;
extern struct workqueue_struct *bdrv_wq;
extern unsigned long fdrv_message_buff;
extern unsigned long bdrv_message_buff;
extern unsigned long message_buff;
extern struct semaphore fdrv_sema;
extern struct semaphore boot_sema;
extern struct semaphore fdrv_lock;
extern struct completion global_down_lock;
extern unsigned long teei_config_flag;
extern struct semaphore smc_lock;
extern int fp_call_flag;
extern int forward_call_flag;
extern struct smc_call_struct smc_call_entry;
extern int irq_call_flag;
extern unsigned int soter_error_flag;
extern unsigned long boot_vfs_addr;
extern unsigned long boot_soter_flag;
extern int keymaster_call_flag;
extern struct completion boot_decryto_lock;
extern struct task_struct *teei_switch_task;
extern struct kthread_worker ut_fastcall_worker;
extern unsigned long ut_pm_count;
extern struct mutex device_cnt_mutex;
extern unsigned long spi_ready_flag;

void ut_pm_mutex_lock(struct mutex *lock);
void ut_pm_mutex_unlock(struct mutex *lock);
void tz_free_shared_mem(void *addr, size_t size);
int get_current_cpuid(void);
void *tz_malloc_shared_mem(size_t size, int flags);
void secondary_init_cmdbuf(void *info);
void secondary_boot_stage2(void *info);
int handle_switch_core(int cpu);
int handle_move_core(int cpu);
void *tz_malloc(size_t size, int flags);
void secondary_load_tee(void *info);
void secondary_load_tee(void *info);
void secondary_boot_stage1(void *info);
int is_teei_ready(void);
int init_sysfs(struct platform_device *pdev);
void remove_sysfs(struct platform_device *pdev);
int teei_new_capi_init(void);
int handle_new_capi_call(void *args);
void notify_smc_completed(void);
int tz_load_drv_by_str(const char *buf);
int tz_load_ta_by_str(const char *buf);
int tz_move_core(uint32_t cpu_id);
#endif /* __TEEI_CLIENT_MAIN_H__ */
