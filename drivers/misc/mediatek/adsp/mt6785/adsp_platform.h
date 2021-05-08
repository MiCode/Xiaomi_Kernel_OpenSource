/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __ADSP_PLATFORM_H__
#define __ADSP_PLATFORM_H__

enum adsp_irq_id {
	ADSP_IRQ_WDT_ID = 0,
	ADSP_IRQ_IPC_ID,
	ADSP_IRQ_NUM, /* id > ADSP_IRQ_NUM: not support */
	ADSP_IRQ_AUDIO_ID,
};

enum adsp_core_id {
	ADSP_A_ID = 0,
	ADSP_CORE_TOTAL,
};

enum adsp_sharedmem_id {
	ADSP_SHAREDMEM_MPUINFO,
	ADSP_SHAREDMEM_TIMESYNC,
	ADSP_SHAREDMEM_IPCBUF,
	ADSP_SHAREDMEM_AUDIO_IPIBUF,
	ADSP_SHAREDMEM_WAKELOCK,
	ADSP_SHAREDMEM_SYS_STATUS,
	ADSP_SHAREDMEM_BUS_MON_DUMP,
	ADSP_SHAREDMEM_INFRA_BUS_DUMP,
	ADSP_SHAREDMEM_NUM,
};

/* semaphore */
#define SEMA_TIMEOUT        5000
#define SEMA_WAY_BITS       2
#define SEMA_CTRL_BIT       1

/* platform method */
void adsp_mt_sw_reset(u32 cid);
void adsp_mt_run(u32 cid);
void adsp_mt_stop(u32 cid);
void adsp_mt_clear(void);
void adsp_mt_clr_sw_reset(void);
void adsp_mt_clr_sysirq(u32 cid);
void adsp_mt_clr_spm(u32 cid);
void adsp_mt_disable_wdt(u32 cid);
void adsp_mt_set_sw_int(u32 cid);
u32 adsp_mt_check_sw_int(u32 cid);
void adsp_mt_set_bootup_mark(u32 cid);

bool check_hifi_status(u32 mask);
bool is_adsp_axibus_idle(void);
u32 switch_adsp_clk_ctrl_cg(bool en, u32 mask);
u32 switch_adsp_uart_ctrl_cg(bool en, u32 mask);
void adsp_platform_init(void);

/* mt6785 only: system usage */
void adsp_bus_sleep_protect(uint32_t enable);
void adsp_way_en_ctrl(uint32_t enable);

extern rwlock_t access_rwlock;
#endif

