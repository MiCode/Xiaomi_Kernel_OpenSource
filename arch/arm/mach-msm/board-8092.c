/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <asm/hardware/gic.h>
#include <asm/arch_timer.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/socinfo.h>
#include <mach/board.h>

#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>

#include "clock.h"

static struct of_device_id irq_match[] __initdata  = {
	{ .compatible = "qcom,msm-qgic2", .data = gic_of_init, },
	{ .compatible = "qcom,msm-gpio", .data = msm_gpio_of_init, },
	{}
};

static struct clk_lookup msm_clocks_dummy[] = {
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "msm_serial_hsl.0", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "msm_serial_hsl.0", OFF),
};

struct clock_init_data mpq8092_clock_init_data __initdata = {
	.table = msm_clocks_dummy,
	.size = ARRAY_SIZE(msm_clocks_dummy),
};

void __init mpq8092_init_irq(void)
{
	of_irq_init(irq_match);
}

static void __init mpq8092_dt_timer_init(void)
{
	arch_timer_of_register();
}

static struct sys_timer mpq8092_dt_timer = {
	.init = mpq8092_dt_timer_init
};

static void __init mpq8092_dt_init_irq(void)
{
	mpq8092_init_irq();
}

static void __init mpq8092_dt_map_io(void)
{
	msm_map_mpq8092_io();
	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

}

static struct of_dev_auxdata mpq8092_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-lsuart-v14", 0xF991F000, \
			"msm_serial_hsl.0", NULL),
	{}
};

static void __init mpq8092_init(struct of_dev_auxdata **adata)
{
	mpq8092_init_gpiomux();
	*adata = mpq8092_auxdata_lookup;
	msm_clock_init(&mpq8092_clock_init_data);
}

static void __init mpq8092_dt_init(void)
{
	struct of_dev_auxdata *adata = NULL;

	mpq8092_init(&adata);
	of_platform_populate(NULL, of_default_bus_match_table, adata, NULL);
}

static const char *mpq8092_dt_match[] __initconst = {
	"qcom,mpq8092-sim",
	NULL
};

DT_MACHINE_START(MSM_DT, "Qualcomm MSM (Flattened Device Tree)")
	.map_io = mpq8092_dt_map_io,
	.init_irq = mpq8092_dt_init_irq,
	.init_machine = mpq8092_dt_init,
	.handle_irq = gic_handle_irq,
	.timer = &mpq8092_dt_timer,
	.dt_compat = mpq8092_dt_match,
MACHINE_END
