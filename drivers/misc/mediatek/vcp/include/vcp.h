/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __VCP_H__
#define __VCP_H__

#include <linux/soc/mediatek/mtk_tinysys_ipi.h>

#define VCP_MBOX_TOTAL 5

/* core1 */
/* definition of slot size for send PINs */
#define PIN_OUT_SIZE_AUDIO_VOW_1         9 /* the following will use mbox 0 */

/* definition of slot size for received PINs */
#define PIN_IN_SIZE_AUDIO_VOW_ACK_1      2 /* the following will use mbox 0 */
#define PIN_IN_SIZE_AUDIO_VOW_1         26 /* the following will use mbox 0 */

/* core0 */
/* definition of slot size for send PINs */
#define PIN_OUT_SIZE_APCCCI_0		 2 /* the following will use mbox 1 */
#define PIN_OUT_SIZE_DVFS_SET_FREQ_0	 1 /* the following will use mbox 1 */
#define PIN_OUT_C_SIZE_SLEEP_0           2 /* the following will use mbox 1 */
#define PIN_OUT_R_SIZE_SLEEP_0           1 /* the following will use mbox 1 */
#define PIN_OUT_SIZE_TEST_0		 1 /* the following will use mbox 1 */
#define PIN_OUT_SIZE_AUDIO_ULTRA_SND_0	 9 /* the following will use mbox 1 */

/* definition of slot size for received PINs */
#define PIN_IN_SIZE_APCCCI_0		 2 /* the following will use mbox 1 */
#define PIN_IN_SIZE_VCP_ERROR_INFO_0    10 /* the following will use mbox 1 */
#define PIN_IN_SIZE_VCP_READY_0		 1 /* the following will use mbox 1 */
#define PIN_IN_SIZE_VCP_RAM_DUMP_0	 2 /* the following will use mbox 1 */
#define PIN_IN_SIZE_AUDIO_ULTRA_SND_0	 5 /* the following will use mbox 1 */
#define PIN_IN_SIZE_AUDIO_ULTRA_SND_ACK_0 2 /* the following will use mbox 1 */
/* ============================================================ */

/* core1 */
/* definition of slot size for send PINs */
#define PIN_OUT_SIZE_DVFS_SET_FREQ_1	 1 /* the following will use mbox 3 */
#define PIN_OUT_C_SIZE_SLEEP_1	         2 /* the following will use mbox 3 */
#define PIN_OUT_R_SIZE_SLEEP_1	         1 /* the following will use mbox 3 */
#define PIN_OUT_SIZE_TEST_1		 1 /* the following will use mbox 3 */
#define PIN_OUT_SIZE_LOGGER_CTRL	 6 /* the following will use mbox 3 */
#define PIN_OUT_SIZE_VCPCTL_1		 2 /* the following will use mbox 3 */

/* definition of slot size for received PINs */
#define PIN_IN_SIZE_VCP_ERROR_INFO_1	10 /* the following will use mbox 3 */
#define PIN_IN_SIZE_LOGGER_CTRL		 6 /* the following will use mbox 3 */
#define PIN_IN_SIZE_VCP_READY_1		 1 /* the following will use mbox 3 */
#define PIN_IN_SIZE_VCP_RAM_DUMP_1	 2 /* the following will use mbox 3 */
/* ============================================================ */

/* this is mbox pool for 2 cores */
#define PIN_OUT_SIZE_VCP_MPOOL          4 /* the following will use mbox 2,4 */
#define PIN_IN_SIZE_VCP_MPOOL           4 /* the following will use mbox 2,4 */

#define PIN_OUT_SIZE_SENSOR_CTRL        16 /* core0 mbox 2 */
#define PIN_IN_SIZE_SENSOR_CTRL          2 /* core0 mbox 2 */
#define PIN_OUT_SIZE_SENSOR_NOTIFY       7 /* core0 mbox 2 */
#define PIN_IN_SIZE_SENSOR_NOTIFY        7 /* core0 mbox 2 */
#define PIN_OUT_SIZE_VCP_CONNSYS         3 /* core0 mbox 4 */
#define PIN_IN_SIZE_VCP_CONNSYS          3 /* core0 mbox 4 */
#define PIN_OUT_SIZE_VDEC	        18
#define PIN_IN_SIZE_VDEC	        18
#define PIN_OUT_SIZE_VENC	        18
#define PIN_IN_SIZE_VENC	        18

/* vcp Core ID definition */
enum vcp_core_id {
	VCP_A_ID = 0,
	VCP_CORE_TOTAL = 1,
};

enum {
/* core1 */
	/* the following will use mbox0 */
	IPI_OUT_VDEC_1	   =  0,
	IPI_IN_VDEC_1	   =  1,

/* core0 */
	/* the following will use mbox1 */
	IPI_OUT_C_SLEEP_0         =  2,
	IPI_OUT_TEST_0            =  3,
	IPI_IN_VCP_ERROR_INFO_0   =  4,
	IPI_IN_VCP_READY_0        =  5,
	IPI_IN_VCP_RAM_DUMP_0     = 6,
	IPI_OUT_VCP_CONNSYS       = 7,
	IPI_IN_VCP_CONNSYS        = 8,

	/* the following will use mbox2 */
	IPI_OUT_VENC_0	   =  9,
	IPI_IN_VENC_0	   =  10,
	IPI_OUT_VCP_MPOOL_0       = 11,
	IPI_IN_VCP_MPOOL_0        = 12,

/* core1 */
	/* the following will use mbox3 */
	IPI_OUT_C_SLEEP_1         = 13,
	IPI_OUT_TEST_1            = 14,
	IPI_OUT_LOGGER_CTRL       = 15,
	IPI_OUT_VCPCTL_1          = 16,
	IPI_IN_VCP_ERROR_INFO_1   = 17,
	IPI_IN_LOGGER_CTRL        = 18,
	IPI_IN_VCP_READY_1        = 19,
	IPI_IN_VCP_RAM_DUMP_1     =20,

	/* the following will use mbox4 */
	IPI_OUT_VCP_MPOOL_1       = 21,
	IPI_IN_VCP_MPOOL_1        = 22,
	VCP_IPI_COUNT
};

enum vcp_ipi_status {
	VCP_IPI_NOT_READY = -2,
	VCP_IPI_ERROR = -1,
	VCP_IPI_DONE,
	VCP_IPI_BUSY,
};

/* vcp notify event */
enum VCP_NOTIFY_EVENT {
	VCP_EVENT_READY = 0,
	VCP_EVENT_STOP,
	VCP_EVENT_SUSPEND,
};
/* the order of ipi_id should be consistent with IPI_LEGACY_GROUP */
enum vcp_ipi_id {
	IPI_MPOOL,
	IPI_CHRE,
	IPI_CHREX,
	IPI_SENSOR,
	IPI_SENSOR_INIT_START,
	VCP_NR_IPI,
};

#define VENC_WORK_ID 0xFFFFFFFF
#define VDEC_WORK_ID 0xFFFFFFFF

/* vcp reserve memory ID definition*/
enum vcp_reserve_mem_id_t {
	VDEC_MEM_ID,
	VENC_MEM_ID,
	VCP_A_LOGGER_MEM_ID,
	VDEC_SET_PROP_MEM_ID,
	VENC_SET_PROP_MEM_ID,
	VDEC_VCP_LOG_INFO_ID,
	VENC_VCP_LOG_INFO_ID,
	GCE_MEM_ID,
	NUMS_MEM_ID,
};

/* vcp feature ID list */
enum feature_id {
	RTOS_FEATURE_ID,
	VDEC_FEATURE_ID,
	VENC_FEATURE_ID,
	GCE_FEATURE_ID,
	NUM_FEATURE_ID,
};

extern struct mtk_mbox_device vcp_mboxdev;
extern struct mtk_ipi_device vcp_ipidev;
extern struct mtk_mbox_pin_send *vcp_mbox_pin_send;
extern struct mtk_mbox_pin_recv *vcp_mbox_pin_recv;


/* An API to get vcp status */
extern unsigned int is_vcp_ready(enum vcp_core_id vcp_id);
extern unsigned int get_vcp_generation(void);

/* APIs to register new IPI handlers */
extern enum vcp_ipi_status vcp_ipi_registration(enum vcp_ipi_id id,
	void (*ipi_handler)(int id, void *data, unsigned int len),
	const char *name);
extern enum vcp_ipi_status vcp_ipi_unregistration(enum vcp_ipi_id id);

/* A common API to send message to VCP */
extern enum vcp_ipi_status vcp_ipi_send(enum vcp_ipi_id id, void *buf,
	unsigned int len, unsigned int wait, enum vcp_core_id vcp_id);


/* APIs to lock vcp and make vcp awaken */
extern int vcp_awake_lock(void *_vcp_id);
extern int vcp_awake_unlock(void *_vcp_id);

/* APIs for register notification */
extern void vcp_A_register_notify(struct notifier_block *nb);
extern void vcp_A_unregister_notify(struct notifier_block *nb);

/* APIs for hardware semaphore */
extern int get_vcp_semaphore(int flag);
extern int release_vcp_semaphore(int flag);
extern int vcp_get_semaphore_3way(int flag);
extern int vcp_release_semaphore_3way(int flag);

/* APIs for reserved memory */
extern phys_addr_t vcp_get_reserve_mem_phys(enum vcp_reserve_mem_id_t id);
extern phys_addr_t vcp_get_reserve_mem_virt(enum vcp_reserve_mem_id_t id);
extern phys_addr_t vcp_get_reserve_mem_size(enum vcp_reserve_mem_id_t id);

/* APIs for registering function of features */
extern void vcp_register_feature(enum feature_id id);
extern void vcp_deregister_feature(enum feature_id id);

/* APIs for reset vcp */
extern void vcp_wdt_reset(int cpu_id);

#endif

