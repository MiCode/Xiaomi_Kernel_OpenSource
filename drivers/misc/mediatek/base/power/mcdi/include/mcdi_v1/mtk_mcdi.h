/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __MTK_MCDI_H__
#define __MTK_MCDI_H__

#include <linux/arm-smccc.h>

#ifdef CONFIG_ARM64
#define MTK_SIP_SMC_AARCH_BIT                   0x40000000
#else
#define MTK_SIP_SMC_AARCH_BIT                   0x00000000
#endif

/* MCDI related SMC call */
#define MTK_SIP_KERNEL_MCDI_ARGS \
	(0x82000231 | MTK_SIP_SMC_AARCH_BIT)

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


static inline size_t mt_secure_call(size_t function_id, size_t arg0,
				    size_t arg1, size_t arg2, size_t arg3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, arg3, 0, 0, 0,
			      &res);

	return res.a0;
}

#endif /* __MTK_MCDI_H__ */
