/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __ADSP_PLATFORM_H__
#define __ADSP_PLATFORM_H__

struct adspsys_priv;

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

/* platform method */
void adsp_mt_set_swirq(u32 cid);
u32 adsp_mt_check_swirq(u32 cid);
void adsp_mt_clr_sysirq(u32 cid);
void adsp_mt_clr_auidoirq(u32 cid);
void adsp_mt_clr_spm(u32 cid);
void adsp_mt_disable_wdt(u32 cid);
void adsp_mt_toggle_semaphore(u32 bit);
u32 adsp_mt_get_semaphore(u32 bit);
bool check_hifi_status(u32 mask);
bool is_adsp_axibus_idle(void);
bool is_infrabus_timeout(void);

void adsp_hardware_init(struct adspsys_priv *adspsys);

#endif

