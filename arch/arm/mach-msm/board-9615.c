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
#include <linux/i2c.h>
#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/msm_iomap.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include "timer.h"
#include "devices.h"

static struct platform_device *common_devices[] = {
	&msm9615_device_uart_gsbi4,
	&msm9615_device_qup_i2c_gsbi5,
};

static struct gpiomux_setting gsbi4 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

static struct gpiomux_setting gsbi5 = {
	.func = GPIOMUX_FUNC_1,
	.drv = GPIOMUX_DRV_8MA,
	.pull = GPIOMUX_PULL_NONE,
};

struct msm_gpiomux_config msm9615_gsbi_configs[] __initdata = {
	{
		.gpio      = 12,	/* GSBI4 UART */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4,
		},
	},
	{
		.gpio      = 13,	/* GSBI4 UART */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4,
		},
	},
	{
		.gpio      = 14,	/* GSBI4 UART */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4,
		},
	},
	{
		.gpio      = 15,	/* GSBI4 UART */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi4,
		},
	},
	{
		.gpio      = 16,	/* GSBI5 I2C QUP SCL */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi5,
		},
	},
	{
		.gpio      = 17,	/* GSBI5 I2C QUP SDA */
		.settings = {
			[GPIOMUX_SUSPENDED] = &gsbi5,
		},
	},
};

static int __init gpiomux_init(void)
{
	int rc;

	rc = msm_gpiomux_init(NR_GPIO_IRQS);
	if (rc) {
		pr_err(KERN_ERR "msm_gpiomux_init failed %d\n", rc);
		return rc;
	}
	msm_gpiomux_install(msm9615_gsbi_configs,
			ARRAY_SIZE(msm9615_gsbi_configs));

	return 0;
}

static struct msm_i2c_platform_data msm9615_i2c_qup_gsbi5_pdata = {
	.clk_freq = 100000,
	.src_clk_rate = 24000000,
};

static void __init msm9615_i2c_init(void)
{
	msm9615_device_qup_i2c_gsbi5.dev.platform_data =
					&msm9615_i2c_qup_gsbi5_pdata;
}

static void __init msm9615_common_init(void)
{
	msm9615_device_init();
	gpiomux_init();
	msm9615_i2c_init();
	platform_add_devices(common_devices, ARRAY_SIZE(common_devices));
}

static void __init msm9615_cdp_init(void)
{
	msm9615_common_init();
}

static void __init msm9615_mtp_init(void)
{
	msm9615_common_init();
}

MACHINE_START(MSM9615_CDP, "QCT MSM9615 CDP")
	.map_io = msm9615_map_io,
	.init_irq = msm9615_init_irq,
	.timer = &msm_timer,
	.init_machine = msm9615_cdp_init,
MACHINE_END

MACHINE_START(MSM9615_MTP, "QCT MSM9615 MTP")
	.map_io = msm9615_map_io,
	.init_irq = msm9615_init_irq,
	.timer = &msm_timer,
	.init_machine = msm9615_mtp_init,
MACHINE_END
