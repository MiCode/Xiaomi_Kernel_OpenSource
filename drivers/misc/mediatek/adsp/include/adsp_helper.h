/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 MediaTek Inc.
 */

#ifndef __ADSP_HELPER_H__
#define __ADSP_HELPER_H__

#include <linux/notifier.h>

/* reset recovery feature kernel option*/
#define CFG_RECOVERY_SUPPORT

/* ipi share buffer size: it will remove later */
#define SHARE_BUF_SIZE  256

/* adsp feature PRI list */
/* The higher number, higher priority */
enum adsp_feature_pri {
	AUDIO_HAL_FEATURE_PRI = 0,
	ADSP_LOGGER_FEATURE_PRI,
	SPK_PROTECT_FEATURE_PRI,
	A2DP_PLAYBACK_FEATURE_PRI,
	AURISYS_FEATURE_PRI,
	DEEPBUF_FEATURE_PRI,
	OFFLOAD_FEATURE_PRI,
	PRIMARY_FEATURE_PRI,
	VOIP_FEATURE_PRI,
	CAPTURE_UL1_FEATURE_PRI,
	AUDIO_DATAPROVIDER_FEATURE_PRI,
	AUDIO_PLAYBACK_FEATURE_PRI,
	VOICE_CALL_FEATURE_PRI,
	AUDIO_CONTROLLER_FEATURE_PRI,
	SYSTEM_FEATURE_PRI,
};

/* adsp feature ID list */
enum adsp_feature_id {
	SYSTEM_FEATURE_ID		= 0,
	ADSP_LOGGER_FEATURE_ID		= 1,
	AURISYS_FEATURE_ID		= 2,
	AUDIO_CONTROLLER_FEATURE_ID	= 3,
	PRIMARY_FEATURE_ID		= 4,
	FAST_FEATURE_ID			= 5,
	DEEPBUF_FEATURE_ID		= 6,
	OFFLOAD_FEATURE_ID		= 7,
	AUDIO_PLAYBACK_FEATURE_ID	= 8,
	AUDIO_MUSIC_FEATURE_ID		= 9,
	RESERVED0_FEATURE_ID		= 10,
	RESERVED1_FEATURE_ID		= 11,
	CAPTURE_UL1_FEATURE_ID		= 12,
	AUDIO_DATAPROVIDER_FEATURE_ID	= 13,
	VOICE_CALL_FEATURE_ID		= 14,
	VOIP_FEATURE_ID			= 15,
	SPK_PROTECT_FEATURE_ID		= 16,
	CALL_FINAL_FEATURE_ID		= 17,
	A2DP_PLAYBACK_FEATURE_ID	= 18,
	KTV_FEATURE_ID			= 19,
	CAPTURE_RAW_FEATURE_ID		= 20,
	FM_ADSP_FEATURE_ID		= 21,
	VOICE_CALL_SUB_FEATURE_ID	= 22,
	BLEDL_FEATURE_ID                = 23,
	BLEUL_FEATURE_ID                = 24,
	BLEDEC_FEATURE_ID               = 25,
	BLEENC_FEATURE_ID               = 26,
	BLE_CALL_DL_FEATURE_ID          = 27,
	BLE_CALL_UL_FEATURE_ID          = 28,
	ADSP_NUM_FEATURE_ID,
};

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
	ADSP_IPI_SLB_INIT = 26,
	ADSP_IPI_ADSP_PLL_CTRL = 27,
	ADSP_IPI_MET_ADSP = 30,
	ADSP_IPI_ADSP_TIMER = 31,
	ADSP_NR_IPI,
};

/* adsp reserve memory ID definition*/
enum adsp_reserve_mem_id_t {
	ADSP_A_IPI_DMA_MEM_ID = 0,
	ADSP_A_LOGGER_MEM_ID,
	ADSP_A_DEBUG_DUMP_MEM_ID,
	ADSP_A_CORE_DUMP_MEM_ID,
	ADSP_B_DEBUG_DUMP_MEM_ID,
	ADSP_B_IPI_DMA_MEM_ID,
	ADSP_B_LOGGER_MEM_ID,
	ADSP_B_CORE_DUMP_MEM_ID,
	ADSP_C2C_MEM_ID,
#ifndef CONFIG_FPGA_EARLY_PORTING
	ADSP_AUDIO_COMMON_MEM_ID,
#endif
	ADSP_NUMS_MEM_ID,
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

enum adsp_irq_id {
	ADSP_IRQ_IPC_ID = 0,
	ADSP_IRQ_WDT_ID,
	ADSP_IRQ_AUDIO_ID,
	ADSP_IRQ_NUM,
};

enum adsp_core_id {
	ADSP_A_ID = 0,
	ADSP_B_ID = 1,
	ADSP_CORE_TOTAL, /* max capability of core */
};

/* adsp notify event */
enum ADSP_NOTIFY_EVENT {
	ADSP_EVENT_STOP = 0,
	ADSP_EVENT_READY,
};

enum semaphore_id {
	SEMA_UART =  0,
	SEMA_C2C_A = 1,
	SEMA_C2C_B = 2,
	SEMA_DVFS =  3,
	SEMA_AUDIO = 4,
	SEMA_AUDIOREG = 5,
};

/* adsp system/feature status */
extern int is_adsp_ready(u32 cid);
extern u32 get_adsp_core_total(void);
extern bool is_adsp_feature_in_active(void);
extern int adsp_feature_in_which_core(enum adsp_feature_id fid);
extern int adsp_register_feature(enum adsp_feature_id fid);
extern int adsp_deregister_feature(enum adsp_feature_id fid);

/* adsp reserved memory API */
extern phys_addr_t adsp_get_reserve_mem_phys(enum adsp_reserve_mem_id_t id);
extern void *adsp_get_reserve_mem_virt(enum adsp_reserve_mem_id_t id);
extern size_t adsp_get_reserve_mem_size(enum adsp_reserve_mem_id_t id);

/* adsp interrupt to other user, If it's not existed, return -ENOTCONN. */
#define adsp_irq_registration(cid, irq_id, handler, data)  \
		adsp_threaded_irq_registration(cid, irq_id, handler, NULL, data)
extern int adsp_threaded_irq_registration(u32 core_id, u32 irq_id,
					  void *handler, void *thread_fn, void *data);

/* adsp hw semaphore */
extern int get_adsp_semaphore(unsigned int flags);
extern int release_adsp_semaphore(unsigned int flags);

/* notify event when adsp crash/recovery */
extern void adsp_register_notify(struct notifier_block *nb);
extern void adsp_unregister_notify(struct notifier_block *nb);
extern void reset_hal_feature_table(void);

/* API for audio_ipi */
extern enum adsp_ipi_status adsp_send_message(enum adsp_ipi_id id, void *buf,
					      unsigned int len, unsigned int wait,
					      unsigned int core_id);

extern enum adsp_ipi_status adsp_ipi_registration(enum adsp_ipi_id id,
						  void (*ipi_handler)(int id,
						  void *data, unsigned int len),
						  const char *name);
extern enum adsp_ipi_status adsp_ipi_unregistration(enum adsp_ipi_id id);

extern void hook_ipi_queue_send_msg_handler(
	int (*send_msg_handler)(
		uint32_t core_id, /* enum adsp_core_id */
		uint32_t ipi_id,  /* enum adsp_ipi_id */
		void *buf,
		uint32_t len,
		uint32_t wait_ms));
extern void unhook_ipi_queue_send_msg_handler(void);

extern void hook_ipi_queue_recv_msg_hanlder(
	int (*recv_msg_hanlder)(
		uint32_t core_id, /* enum adsp_core_id */
		uint32_t ipi_id,  /* enum adsp_ipi_id */
		void *buf,
		uint32_t len,
		void (*ipi_handler)(int ipi_id, void *buf, unsigned int len)));
extern void unhook_ipi_queue_recv_msg_hanlder(void);

#endif
