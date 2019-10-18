// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/cpuidle.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/cpu_pm.h>
#include <linux/syscore_ops.h>
#include <linux/suspend.h>
#include <asm/cpuidle.h>
#include <asm/suspend.h>

#include <mtk_lpm.h>
#include <mtk_lpm_module.h>
#include <mtk_lpm_call.h>
#include <mtk_lpm_type.h>
#include <mtk_lpm_call_type.h>
#include <mtk_power_gs_api.h>

#include "mt6885.h"
#include "mt6885_suspend.h"

unsigned int mt6885_suspend_status;

#define WORLD_CLK_CNTCV_L        (0x10017008)
#define WORLD_CLK_CNTCV_H        (0x1001700C)

void __attribute__((weak)) subsys_if_on(void)
{
	printk_deferred("[name:spm&]NO %s !!!\n", __func__);
}
void __attribute__((weak)) pll_if_on(void)
{
	printk_deferred("[name:spm&]NO %s !!!\n", __func__);
}
void __attribute__((weak)) gpio_dump_regs(void)
{
	printk_deferred("[name:spm&]NO %s !!!\n", __func__);
}

void mtk_suspend_gpio_dbg(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	gpio_dump_regs();
#endif
}
EXPORT_SYMBOL(mtk_suspend_gpio_dbg);

void mtk_suspend_clk_dbg(void)
{
#if !defined(CONFIG_FPGA_EARLY_PORTING)
	pll_if_on();
	subsys_if_on();
#endif /* CONFIG_FPGA_EARLY_PORTING */
}
EXPORT_SYMBOL(mtk_suspend_clk_dbg);

static inline int mt6885_suspend_common_enter(unsigned int *susp_status)
{
	unsigned int status = PLAT_VCORE_LP_MODE
				| PLAT_PMIC_VCORE_SRCLKEN0
				| PLAT_SUSPEND;

	/* maybe need to stop sspm/mcupm mcdi task here */
	if (susp_status)
		*susp_status = status;

	return 0;
}


static inline int mt6885_suspend_common_resume(unsigned int susp_status)
{
	/* Implement suspend common flow here */
	return 0;
}

int mt6885_suspend_prompt(int cpu, const struct mtk_lpm_issuer *issuer)
{
	int ret = 0;
	unsigned int spm_res = 0;

	mt6885_suspend_status = 0;

	printk_deferred("[name:spm&][%s:%d] - prepare suspend enter\n",
			__func__, __LINE__);

	ret = mt6885_suspend_common_enter(&mt6885_suspend_status);

	if (ret)
		goto PLAT_LEAVE_SUSPEND;

	/* Legacy SSPM flow, spm sw resource request flow */
	mt6885_do_mcusys_prepare_pdn(mt6885_suspend_status, &spm_res);

	printk_deferred("[name:spm&][%s:%d] - suspend enter\n",
			__func__, __LINE__);
	printk_deferred("[name:spm&] wlk_cntcv_l = 0x%x, wlk_cntcv_h = 0x%x\n",
		_golden_read_reg(WORLD_CLK_CNTCV_L),
		_golden_read_reg(WORLD_CLK_CNTCV_H));


PLAT_LEAVE_SUSPEND:
	return ret;
}

void mt6885_suspend_reflect(int cpu,
					const struct mtk_lpm_issuer *issuer)
{
	printk_deferred("[name:spm&][%s:%d] - prepare suspend resume\n",
			__func__, __LINE__);

	mt6885_suspend_common_resume(mt6885_suspend_status);
	mt6885_do_mcusys_prepare_on();

	printk_deferred("[name:spm&][%s:%d] - suspend resume\n",
			__func__, __LINE__);

	if (issuer)
		issuer->log(MT_LPM_ISSUER_CPUIDLE, "suspend", NULL);
}

struct mtk_lpm_model mt6885_model_suspend = {
	.flag = MTK_LP_REQ_NONE,
	.op = {
		.prompt = mt6885_suspend_prompt,
		.reflect = mt6885_suspend_reflect,
	}
};

int __init mt6885_model_suspend_init(void)
{
	mtk_lpm_suspend_registry("suspend", &mt6885_model_suspend);

#ifdef CONFIG_PM_SLEEP_DEBUG
	pm_print_times_enabled = false;
#endif
	return 0;
}
