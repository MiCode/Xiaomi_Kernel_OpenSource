/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __SCP_CM4_H__
#define __SCP_CM4_H__

#include <linux/soc/mediatek/mtk_tinysys_ipi.h>
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
	IPI_SCP_LOG_FILTER = 33,
	IPI_SENSOR_INIT_START = 34,
	SCP_NR_IPI,
};

enum scp_ipi_status {
	SCP_IPI_ERROR = -1,
	SCP_IPI_DONE,
	SCP_IPI_BUSY,
};

/* scp notify event */
enum SCP_NOTIFY_EVENT {
	SCP_EVENT_READY = 0,
	SCP_EVENT_STOP,
};

/* scp semaphore definition*/
enum SEMAPHORE_FLAG {
	SEMAPHORE_CLK_CFG_5 = 0,
	SEMAPHORE_PTP,
	SEMAPHORE_I2C0,
	SEMAPHORE_I2C1,
	SEMAPHORE_TOUCH,
	SEMAPHORE_APDMA,
	SEMAPHORE_SENSOR,
	SEMAPHORE_SCP_A_AWAKE,
	SEMAPHORE_SCP_B_AWAKE,
	NR_FLAG = 9,
};

/* scp reserve memory ID definition*/
enum scp_reserve_mem_id_t {
	VOW_MEM_ID,
	SENS_MEM_ID,
	MP3_MEM_ID,
	FLP_MEM_ID,
	SCP_A_LOGGER_MEM_ID,
	AUDIO_IPI_MEM_ID,
	SPK_PROTECT_MEM_ID,
	SPK_PROTECT_DUMP_MEM_ID,
	VOW_BARGEIN_MEM_ID,
	SCP_DRV_PARAMS_MEM_ID,
	ULTRA_MEM_ID,
	NUMS_MEM_ID,
};

/* scp feature ID list */
enum feature_id {
	VOW_FEATURE_ID,
	OPEN_DSP_FEATURE_ID,
	SENS_FEATURE_ID,
	MP3_FEATURE_ID,
	FLP_FEATURE_ID,
	RTOS_FEATURE_ID,
	SPEAKER_PROTECT_FEATURE_ID,
	VCORE_TEST_FEATURE_ID,
	VOW_BARGEIN_FEATURE_ID,
	VOW_DUMP_FEATURE_ID,
	VOW_VENDOR_M_FEATURE_ID,
	VOW_VENDOR_A_FEATURE_ID,
	VOW_VENDOR_G_FEATURE_ID,
	ULTRA_FEATURE_ID,
	NUM_FEATURE_ID,
};

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
extern int scp_awake_lock(enum scp_core_id scp_id);
extern int scp_awake_unlock(enum scp_core_id scp_id);

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

#endif

