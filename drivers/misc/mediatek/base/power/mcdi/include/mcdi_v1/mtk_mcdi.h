/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __MTK_MCDI_H__
#define __MTK_MCDI_H__

enum {
	MCDI_SMC_EVENT_ASYNC_WAKEUP_EN = 0,
	MCDI_SMC_EVENT_DUMP_TRACE_DATA,
	MCDI_SMC_EVENT_LAST_CORE_REQ,
	MCDI_SMC_EVENT_LAST_CORE_CLR,
	MCDI_SMC_EVENT_GIC_DPG_SET,

	NF_MCDI_SMC_EVENT
};

extern void aee_rr_rec_mcdi_val(int id, unsigned int val);

/* mtk_menu */
unsigned int get_menu_predict_us(void);
unsigned int get_menu_next_timer_us(void);

/* main */
int wfi_enter(int cpu);
int mcdi_enter(int cpu);
bool _mcdi_task_pause(bool paused);
void mcdi_avail_cpu_mask(unsigned int cpu_mask);
void _mcdi_cpu_iso_mask(unsigned int iso_mask);
void mcdi_wakeup_all_cpu(void);
bool __mcdi_pause(unsigned int id, bool paused);

#endif /* __MTK_MCDI_H__ */
