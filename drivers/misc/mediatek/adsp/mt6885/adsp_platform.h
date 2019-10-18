/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ADSP_PLATFORM_H__
#define __ADSP_PLATFORM_H__

enum adsp_irq_id {
	ADSP_IRQ_IPC_ID = 0,
	ADSP_IRQ_WDT_ID,
	ADSP_IRQ_AUDIO_ID,
	ADSP_IRQ_NUM,
};

enum adsp_core_id {
	ADSP_A_ID = 0,
	ADSP_B_ID = 1,
	ADSP_CORE_TOTAL,
};

enum adsp_sharedmem_id {
	ADSP_SHAREDMEM_BOOTUP_MARK = 0,
	ADSP_SHAREDMEM_SYS_STATUS,
	ADSP_SHAREDMEM_MPUINFO,
	ADSP_SHAREDMEM_WAKELOCK,
	ADSP_SHAREDMEM_IPCBUF,
	ADSP_SHAREDMEM_C2C_0_BUF,
	ADSP_SHAREDMEM_C2C_1_BUF,
	ADSP_SHAREDMEM_C2C_BUFINFO,
	ADSP_SHAREDMEM_TIMESYNC,
	ADSP_SHAREDMEM_DVFSSYNC,
	ADSP_SHAREDMEM_SLEEPSYNC,
	ADSP_SHAREDMEM_BUS_MON_DUMP,
	ADSP_SHAREDMEM_INFRA_BUS_DUMP,
	ADSP_SHAREDMEM_LATMON_DUMP,
	ADSP_SHAREDMEM_NUM,
};

/* semaphore */
#define SEMA_TIMEOUT        5000
#define SEMA_WAY_BITS       3
#define SEMA_CTRL_BIT       2

enum semaphore_3way_flag {
	SEMA_3WAY_UART = 0,
	SEMA_3WAY_C2C = 1,
	SEMA_3WAY_DVFS = 2,
	SEMA_3WAY_NUM = 7,
};

/* platform method */
void adsp_mt_sw_reset(int cid);
void adsp_mt_run(int cid);
void adsp_mt_stop(int cid);
void adsp_mt_clear(void);
void adsp_mt_clr_sw_reset(void);
void adsp_mt_clr_spm(int cid);
void adsp_mt_disable_wdt(int cid);

bool check_hifi_status(int mask);
bool is_adsp_axibus_idle(void);
u32 switch_adsp_clk_ctrl_cg(bool en, int mask);
u32 switch_adsp_uart_ctrl_cg(bool en, int mask);
void set_adsp_dram_remapping(u32 addr, u32 size);
void adsp_platform_init(void);

extern rwlock_t access_rwlock;
#endif

