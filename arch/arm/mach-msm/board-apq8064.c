/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/msm_ssbi.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/hardware/gic.h>

#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <linux/usb/msm_hsusb.h>
#include <linux/usb/android.h>
#include <mach/socinfo.h>
#include <mach/msm_spi.h>
#include "timer.h"
#include "devices.h"
#include <mach/gpio.h>
#include <mach/gpiomux.h>

static int __init gpiomux_init(void)
{
	int rc;

	rc = msm_gpiomux_init(NR_GPIO_IRQS);
	if (rc) {
		pr_err(KERN_ERR "msm_gpiomux_init failed %d\n", rc);
		return rc;
	}
	return 0;
}

static void __init apq8064_map_io(void)
{
	msm_map_apq8064_io();
}

static void __init apq8064_init_irq(void)
{
	unsigned int i;
	gic_init(0, GIC_PPI_START, MSM_QGIC_DIST_BASE,
						(void *)MSM_QGIC_CPU_BASE);

	/* Edge trigger PPIs except AVS_SVICINT and AVS_SVICINTSWDONE */
	writel_relaxed(0xFFFFD7FF, MSM_QGIC_DIST_BASE + GIC_DIST_CONFIG + 4);

	writel_relaxed(0x0000FFFF, MSM_QGIC_DIST_BASE + GIC_DIST_ENABLE_SET);
	mb();

	/*
	 * FIXME: Not installing AVS_SVICINT and AVS_SVICINTSWDONE yet
	 * as they are configured as level, which does not play nice with
	 * handle_percpu_irq.
	 */
	for (i = GIC_PPI_START; i < GIC_SPI_START; i++) {
		if (i != AVS_SVICINT && i != AVS_SVICINTSWDONE)
			irq_set_handler(i, handle_percpu_irq);
	}
}

static struct platform_device *common_devices[] __initdata = {
	&msm_device_dmov,
	&apq8064_device_uart_gsbi3,
	&apq8064_device_qup_spi_gsbi5,
	&apq8064_device_ssbi_pmic1,
	&apq8064_device_ssbi_pmic2,
};

static struct msm_spi_platform_data apq8064_qup_spi_gsbi5_pdata = {
	.max_clock_speed = 26000000,
};

static struct msm_otg_platform_data msm_otg_pdata = {
	.mode			= USB_PERIPHERAL,
	.otg_control		= OTG_PHY_CONTROL,
	.phy_type		= SNPS_28NM_INTEGRATED_PHY,
	.pclk_src_name		= "dfab_usb_hs_clk",
};

static struct msm_ssbi_platform_data apq8064_ssbi_pm8921_pdata __devinitdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
	.slave	= {
		.name			= "pm8921-core",
	},
};

static struct msm_ssbi_platform_data apq8064_ssbi_pm8821_pdata __devinitdata = {
	.controller_type = MSM_SBI_CTRL_PMIC_ARBITER,
	.slave	= {
		.name			= "pm8821-core",
	},
};

static void __init apq8064_common_init(void)
{
	if (socinfo_init() < 0)
		pr_err("socinfo_init() failed!\n");
	msm_clock_init(msm_clocks_8064_dummy, msm_num_clocks_8064_dummy);
	gpiomux_init();

	apq8064_device_qup_spi_gsbi5.dev.platform_data =
						&apq8064_qup_spi_gsbi5_pdata;
	apq8064_device_ssbi_pmic1.dev.platform_data =
				&apq8064_ssbi_pm8921_pdata;
	apq8064_device_ssbi_pmic2.dev.platform_data =
				&apq8064_ssbi_pm8821_pdata;
	platform_add_devices(common_devices, ARRAY_SIZE(common_devices));
}

static void __init apq8064_sim_init(void)
{
	apq8064_common_init();
}

MACHINE_START(APQ8064_SIM, "QCT APQ8064 SIMULATOR")
	.map_io = apq8064_map_io,
	.init_irq = apq8064_init_irq,
	.timer = &msm_timer,
	.init_machine = apq8064_sim_init,
MACHINE_END

