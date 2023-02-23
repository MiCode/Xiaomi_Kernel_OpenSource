/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __SCP_RV_H__
#define __SCP_RV_H__

#include <linux/soc/mediatek/mtk_tinysys_ipi.h>

#define SCP_MBOX_TOTAL 5

#define PIN_OUT_SIZE_AUDIO_VOW_1         9
#define PIN_IN_SIZE_AUDIO_VOW_ACK_1      2
#define PIN_IN_SIZE_AUDIO_VOW_1         26
#define PIN_IN_SIZE_AUDIO_ACCDET_1       1
#define PIN_OUT_SIZE_APCCCI_0		 2
#define PIN_OUT_SIZE_DVFS_SET_FREQ_0	 1
#define PIN_OUT_C_SIZE_SLEEP_0           2
#define PIN_OUT_R_SIZE_SLEEP_0           1
#define PIN_OUT_SIZE_TEST_0		 1
#define PIN_OUT_SIZE_AUDIO_ULTRA_SND_0	 9
#define PIN_IN_SIZE_APCCCI_0		 2
#define PIN_IN_SIZE_SCP_ERROR_INFO_0    10
#define PIN_IN_SIZE_SCP_READY_0		 1
#define PIN_IN_SIZE_SCP_RAM_DUMP_0	 2
#define PIN_IN_SIZE_AUDIO_ULTRA_SND_0	 5
#define PIN_IN_SIZE_AUDIO_ULTRA_SND_ACK_0 2
#define PIN_OUT_SIZE_DVFS_SET_FREQ_1	 1
#define PIN_OUT_C_SIZE_SLEEP_1	         2
#define PIN_OUT_R_SIZE_SLEEP_1	         1
#define PIN_OUT_SIZE_TEST_1		 1
#define PIN_OUT_SIZE_LOGGER_CTRL	 6
#define PIN_OUT_SIZE_SCPCTL_1		 2
#define PIN_IN_SIZE_SCP_ERROR_INFO_1	10
#define PIN_IN_SIZE_LOGGER_CTRL		 6
#define PIN_IN_SIZE_SCP_READY_1		 1
#define PIN_IN_SIZE_SCP_RAM_DUMP_1	 2
#define PIN_OUT_SIZE_SCP_MPOOL           4
#define PIN_IN_SIZE_SCP_MPOOL            4
#define PIN_OUT_SIZE_SENSOR_CTRL        16
#define PIN_IN_SIZE_SENSOR_CTRL          2
#define PIN_OUT_SIZE_SENSOR_NOTIFY       7
#define PIN_IN_SIZE_SENSOR_NOTIFY        7
#define PIN_OUT_SIZE_SCP_CONNSYS         3
#define PIN_OUT_SIZE_SCP_HWVOTER_DEBUG   2

/* scp Core ID definition */
enum scp_core_id {
	SCP_A_ID = 0,
	SCP_CORE_TOTAL = 1,
};

enum {
	/* for mbox mapping, please refer to tinysys side */
	IPI_OUT_AUDIO_VOW_1       =  0,
	IPI_IN_AUDIO_VOW_ACK_1	  =  1,
	IPI_IN_AUDIO_VOW_1        =  2,
	IPI_OUT_APCCCI_0          =  3,
	IPI_OUT_DVFS_SET_FREQ_0	  =  4,
	IPI_OUT_C_SLEEP_0         =  5,
	IPI_OUT_TEST_0            =  6,
	IPI_IN_APCCCI_0           =  7,
	IPI_IN_SCP_ERROR_INFO_0   =  8,
	IPI_IN_SCP_READY_0        =  9,
	IPI_IN_SCP_RAM_DUMP_0     = 10,
	IPI_OUT_SCP_MPOOL_0       = 11,
	IPI_IN_SCP_MPOOL_0        = 12,
	IPI_OUT_AUDIO_ULTRA_SND_1 = 13,
	IPI_OUT_DVFS_SET_FREQ_1   = 14,
	IPI_OUT_C_SLEEP_1         = 15,
	IPI_OUT_TEST_1            = 16,
	IPI_OUT_LOGGER_CTRL       = 17,
	IPI_OUT_SCPCTL_1          = 18,
	IPI_IN_AUDIO_ULTRA_SND_1  = 19,
	IPI_IN_SCP_ERROR_INFO_1   = 20,
	IPI_IN_LOGGER_CTRL        = 21,
	IPI_IN_SCP_READY_1        = 22,
	IPI_IN_SCP_RAM_DUMP_1     = 23,
	IPI_OUT_SCP_MPOOL_1       = 24,
	IPI_IN_SCP_MPOOL_1        = 25,
	IPI_OUT_AUDIO_ULTRA_SND_0 =  26,
	IPI_IN_AUDIO_ULTRA_SND_ACK_0 = 27,
	IPI_IN_AUDIO_ULTRA_SND_0  =  28,
	IPI_OUT_SENSOR_CTRL       = 29,
	IPI_IN_SENSOR_CTRL        = 30,
	IPI_OUT_SENSOR_NOTIFY     = 31,
	IPI_IN_SENSOR_NOTIFY      = 32,
	IPI_OUT_SCP_CONNSYS       = 33,
	IPI_IN_SCP_CONNSYS        = 34,
	IPI_OUT_SCP_HWVOTER_DEBUG   = 35,
	IPI_IN_AUDIO_ACCDET_1     = 36,
	IPI_OUT_SCP_AOD           = 37,
	IPI_IN_SCP_AOD            = 38,
	SCP_IPI_COUNT
};

enum scp_ipi_status {
	SCP_IPI_NOT_READY = -2,
	SCP_IPI_ERROR = -1,
	SCP_IPI_DONE,
	SCP_IPI_BUSY,
};

/* scp notify event */
enum SCP_NOTIFY_EVENT {
	SCP_EVENT_READY = 0,
	SCP_EVENT_STOP,
	SCP_EVENT_NOTIFYING,
};
/* the order of ipi_id should be consistent with IPI_LEGACY_GROUP */
enum ipi_id {
	IPI_MPOOL,
	IPI_CHRE,
	IPI_CHREX,
	IPI_SENSOR,
	IPI_SENSOR_INIT_START,
	IPI_ELLIPTIC,
	SCP_NR_IPI,
};

/* scp reserve memory ID definition*/
enum scp_reserve_mem_id_t {
	SCP_A_SECDUMP_MEM_ID = 0,   /* please keep SCP_A_SECDUMP_MEM_ID=0 */
	VOW_MEM_ID,
	SENS_MEM_ID,
	SCP_A_LOGGER_MEM_ID,
	AUDIO_IPI_MEM_ID,
	VOW_BARGEIN_MEM_ID,
	SCP_DRV_PARAMS_MEM_ID,
	SENS_SUPER_MEM_ID,
	SENS_LIST_MEM_ID,
	SENS_DEBUG_MEM_ID,
	SENS_CUSTOM_W_MEM_ID,
	SENS_CUSTOM_R_MEM_ID,
#ifdef CONFIG_MTK_ULTRASND_PROXIMITY
	ULTRA_MEM_ID,
	SCP_ELLIPTIC_DEBUG_MEM,
#endif
	NUMS_MEM_ID,
};

/* scp feature ID list */
enum feature_id {
	VOW_FEATURE_ID = 0,
	SENS_FEATURE_ID = 1,
	FLP_FEATURE_ID = 2,
	RTOS_FEATURE_ID = 3,
	SPEAKER_PROTECT_FEATURE_ID = 4,
	VCORE_TEST_FEATURE_ID = 5,
	VOW_BARGEIN_FEATURE_ID = 6,
	VOW_DUMP_FEATURE_ID = 7,
	VOW_VENDOR_M_FEATURE_ID = 8,
	VOW_VENDOR_A_FEATURE_ID = 9,
	VOW_VENDOR_G_FEATURE_ID = 10,
	VOW_DUAL_MIC_FEATURE_ID = 11,
	VOW_DUAL_MIC_BARGE_IN_FEATURE_ID = 12,
	ULTRA_FEATURE_ID = 13,
	NUM_FEATURE_ID = 14,
};

extern struct mtk_mbox_device scp_mboxdev;
extern struct mtk_ipi_device scp_ipidev;
extern struct mtk_mbox_pin_send *scp_mbox_pin_send;
extern struct mtk_mbox_pin_recv *scp_mbox_pin_recv;


/* An API to get scp status */
extern unsigned int is_scp_ready(enum scp_core_id scp_id);

/* APIs to register new IPI handlers */
extern enum scp_ipi_status scp_ipi_registration(enum ipi_id id,
	void (*ipi_handler)(int id, void *data, unsigned int len),
	const char *name);
extern enum scp_ipi_status scp_ipi_unregistration(enum ipi_id id);

/* A common API to send message to SCP */
extern enum scp_ipi_status scp_ipi_send(enum ipi_id id, void *buf,
	unsigned int len, unsigned int wait, enum scp_core_id scp_id);


/* APIs to lock scp and make scp awaken */
extern int scp_awake_lock(void *_scp_id);
extern int scp_awake_unlock(void *_scp_id);

/* APIs for register notification */
extern void scp_A_register_notify(struct notifier_block *nb);
extern void scp_A_unregister_notify(struct notifier_block *nb);

/* APIs for hardware semaphore */
extern int get_scp_semaphore(int flag);
extern int release_scp_semaphore(int flag);
extern int scp_get_semaphore_3way(int flag);
extern int scp_release_semaphore_3way(int flag);

/* APIs for reserved memory */
extern phys_addr_t scp_get_reserve_mem_phys(enum scp_reserve_mem_id_t id);
extern phys_addr_t scp_get_reserve_mem_virt(enum scp_reserve_mem_id_t id);
extern phys_addr_t scp_get_reserve_mem_size(enum scp_reserve_mem_id_t id);

/* APIs for registering function of features */
extern void scp_register_feature(enum feature_id id);
extern void scp_deregister_feature(enum feature_id id);
extern void scp_register_sensor(enum feature_id id,
		int sensor_id);
extern void scp_deregister_sensor(enum feature_id id,
		int sensor_id);

/* APIs for reset scp */
extern void scp_wdt_reset(int cpu_id);

#endif

