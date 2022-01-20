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
	MCDI_SMC_EVENT_CPC_CONFIG,
	MCDI_SMC_EVENT_MCUPM_FW_STA,

	NF_MCDI_SMC_EVENT
};

extern void aee_rr_rec_mcdi_val(int id, unsigned int val);
extern unsigned long long notrace sched_clock(void);

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

#ifndef CONFIG_MACH_MT6739
static inline size_t mt_secure_call(size_t function_id, size_t arg0,
				    size_t arg1, size_t arg2, size_t arg3)
{
	struct arm_smccc_res res;

	arm_smccc_smc(function_id, arg0, arg1, arg2, arg3, 0, 0, 0,
			      &res);

	return res.a0;
}
#else
#ifndef mt_secure_call
#define mt_secure_call(x1, x2, x3, x4, x5) ({\
	struct arm_smccc_res res;\
	mtk_idle_smc_impl(x1, x2, x3, x4, x5, res);\
	res.a0; })
#endif
#endif

#define mtk_idle_smc_impl(p1, p2, p3, p4, p5, res) \
			arm_smccc_smc(p1, p2, p3, p4,\
			p5, 0, 0, 0, &res)


#ifndef SMC_CALL
/* SMC call's marco */
#define SMC_CALL(_name, _arg0, _arg1, _arg2) ({\
	struct arm_smccc_res res;\
	mtk_idle_smc_impl(MTK_SIP_KERNEL_SPM_##_name,\
			_arg0, _arg1, _arg2, 0, res);\
	res.a0; })

#endif

#endif /* __MTK_MCDI_H__ */
