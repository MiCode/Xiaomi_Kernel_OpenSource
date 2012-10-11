/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/memory.h>
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <asm/arch_timer.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#include <mach/msm_memtypes.h>
#include <mach/socinfo.h>
#include <mach/board.h>
#include <mach/clk-provider.h>
#include "clock.h"

static struct clk_lookup msm_clocks_dummy[] = {
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "msm_serial_hsl.0", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "msm_serial_hsl.0", OFF),
	CLK_DUMMY("iface_clk",  HSUSB_IFACE_CLK, "msm_otg", OFF),
	CLK_DUMMY("core_clk",	HSUSB_CORE_CLK, "msm_otg", OFF),
};

struct clock_init_data msm_dummy_clock_init_data __initdata = {
	.table = msm_clocks_dummy,
	.size = ARRAY_SIZE(msm_clocks_dummy),
};

static struct of_device_id irq_match[] __initdata  = {
	{ .compatible = "qcom,msm-qgic2", .data = gic_of_init, },
	{ .compatible = "qcom,msm-gpio", .data = msm_gpio_of_init, },
	{}
};

static struct of_dev_auxdata msm8226_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-lsuart-v14", 0xF991F000, \
			"msm_serial_hsl.0", NULL),
	OF_DEV_AUXDATA("qcom,hsusb-otg", 0xF9A55000, \
			"msm_otg", NULL),
	{}
};

static void __init msm8226_dt_timer_init(void)
{
	arch_timer_of_register();
}

static struct sys_timer msm8226_dt_timer = {
	.init = msm8226_dt_timer_init
};

void __init msm8226_init_irq(void)
{
	of_irq_init(irq_match);
}

void __init msm8226_init(struct of_dev_auxdata **adata)
{
	msm8226_init_gpiomux();

	msm_clock_init(&msm_dummy_clock_init_data);

	*adata = msm8226_auxdata_lookup;
}

void __init msm8226_dt_init(void)
{
	struct of_dev_auxdata *adata = NULL;
	msm8226_init(&adata);

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	of_platform_populate(NULL, of_default_bus_match_table, adata, NULL);
}


static const char *msm8226_dt_match[] __initconst = {
	"qcom,msm8226",
	NULL
};

DT_MACHINE_START(MSM8226_DT, "Qualcomm MSM 8226 (Flattened Device Tree)")
	.map_io = msm_map_msm8226_io,
	.init_irq = msm8226_init_irq,
	.init_machine = msm8226_dt_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm8226_dt_timer,
	.dt_compat = msm8226_dt_match,
MACHINE_END
