/* Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <asm/arch_timer.h>
#include <asm/mach/time.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/hardware/gic.h>
#include <mach/mpm.h>
#include <mach/qpnp-int.h>
#include <mach/scm.h>

#include "board-dt.h"

#define SCM_SVC_L2CC_PL310	16
#define L2CC_PL310_CTRL_ID	1
#define L2CC_PL310_ON		1

static void __init msm_dt_timer_init(void)
{
	arch_timer_of_register();
}

struct sys_timer msm_dt_timer = {
	.init = msm_dt_timer_init
};

static struct of_device_id irq_match[] __initdata  = {
	{ .compatible = "qcom,msm-qgic2", .data = gic_of_init, },
	{ .compatible = "qcom,msm-gpio", .data = msm_gpio_of_init, },
	{ .compatible = "qcom,spmi-pmic-arb", .data = qpnpint_of_init, },
	{ .compatible = "qcom,wcd9xxx-irq", .data = wcd9xxx_irq_of_init, },
	{}
};

static struct of_device_id mpm_match[] __initdata = {
	{.compatible = "qcom,mpm-v2", },
	{}
};

void __init msm_dt_init_irq(void)
{
	struct device_node *node;

	of_irq_init(irq_match);
	node = of_find_matching_node(NULL, mpm_match);

	WARN_ON(!node);

	if (node)
		of_mpm_init(node);
}

void __init msm_dt_init_irq_nompm(void)
{
	of_irq_init(irq_match);
}

void __init msm_dt_init_irq_l2x0(void)
{
	scm_call_atomic1(SCM_SVC_L2CC_PL310, L2CC_PL310_CTRL_ID, L2CC_PL310_ON);
	l2x0_of_init(0, ~0UL);
	msm_dt_init_irq();
}
