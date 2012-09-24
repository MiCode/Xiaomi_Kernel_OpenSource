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
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/memory.h>
#include <asm/mach/map.h>
#include <asm/hardware/cache-l2x0.h>
#include <asm/hardware/gic.h>
#include <asm/arch_timer.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <mach/socinfo.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/clk-provider.h>
#include <mach/qpnp-int.h>
#include <mach/msm_memtypes.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include "clock.h"
#include "modem_notifier.h"

#define MSM_KERNEL_EBI_SIZE	0x51000

static struct memtype_reserve msm9625_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static int msm9625_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static void __init msm9625_calculate_reserve_sizes(void)
{
	msm9625_reserve_table[MEMTYPE_EBI1].size += MSM_KERNEL_EBI_SIZE;
}

static struct reserve_info msm9625_reserve_info __initdata = {
	.memtype_reserve_table = msm9625_reserve_table,
	.calculate_reserve_sizes = msm9625_calculate_reserve_sizes,
	.paddr_to_memtype = msm9625_paddr_to_memtype,
};


#define L2CC_AUX_CTRL	((0x1 << L2X0_AUX_CTRL_SHARE_OVERRIDE_SHIFT) | \
			(0x2 << L2X0_AUX_CTRL_WAY_SIZE_SHIFT) | \
			(0x1 << L2X0_AUX_CTRL_EVNT_MON_BUS_EN_SHIFT))

static struct clk_lookup msm_clocks_dummy[] = {
	CLK_DUMMY("core_clk",   BLSP1_UART_CLK, "msm_serial_hsl.0", OFF),
	CLK_DUMMY("iface_clk",  BLSP1_UART_CLK, "msm_serial_hsl.0", OFF),
	CLK_DUMMY("phy_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("core_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("alt_core_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("iface_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("xo", NULL, "msm_otg", OFF),
	CLK_DUMMY("dfab_clk",	DFAB_CLK,	NULL, 0),
	CLK_DUMMY("dma_bam_pclk",	DMA_BAM_P_CLK,	NULL, 0),
	CLK_DUMMY("mem_clk",	NULL,	NULL, 0),
	CLK_DUMMY("core_clk",	SPI_CLK,	"spi_qsd.1",	OFF),
	CLK_DUMMY("iface_clk",	SPI_P_CLK,	"spi_qsd.1",	OFF),
	CLK_DUMMY("core_clk",	NULL,	"f9966000.i2c", 0),
	CLK_DUMMY("iface_clk",	NULL,	"f9966000.i2c", 0),
	CLK_DUMMY("core_clk",	NULL,	"fe12f000.slim",	OFF),
};

struct clock_init_data msm_dummy_clock_init_data __initdata = {
	.table = msm_clocks_dummy,
	.size = ARRAY_SIZE(msm_clocks_dummy),
};

static struct of_device_id irq_match[] __initdata  = {
	{ .compatible = "qcom,msm-qgic2", .data = gic_of_init, },
	{ .compatible = "qcom,msm-gpio", .data = msm_gpio_of_init, },
	{ .compatible = "qcom,spmi-pmic-arb", .data = qpnpint_of_init, },
	{}
};

static const char *msm9625_dt_match[] __initconst = {
	"qcom,msm9625",
	NULL
};

static struct of_dev_auxdata msm9625_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-lsuart-v14", 0xF991F000, \
			"msm_serial_hsl.0", NULL),
	OF_DEV_AUXDATA("qcom,spi-qup-v2", 0xF9928000, \
			"spi_qsd.1", NULL),
	OF_DEV_AUXDATA("qcom,spmi-pmic-arb", 0xFC4C0000, \
			"spmi-pmic-arb.0", NULL),
	{}
};

void __init msm9625_init_irq(void)
{
	l2x0_of_init(L2CC_AUX_CTRL, L2X0_AUX_CTRL_MASK);
	of_irq_init(irq_match);
}

static void __init msm_dt_timer_init(void)
{
	arch_timer_of_register();
}

static struct sys_timer msm_dt_timer = {
	.init = msm_dt_timer_init
};

static void __init msm9625_reserve(void)
{
	reserve_info = &msm9625_reserve_info;
	msm_reserve();
}

static struct resource smd_resource[] = {
	{
		.name   = "modem_smd_in",
		.start  = 32 + 25,              /* mss_sw_to_kpss_ipc_irq0  */
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "modem_smsm_in",
		.start  = 32 + 26,              /* mss_sw_to_kpss_ipc_irq1  */
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "adsp_smd_in",
		.start  = 32 + 156,             /* lpass_to_kpss_ipc_irq0  */
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "adsp_smsm_in",
		.start  = 32 + 157,             /* lpass_to_kpss_ipc_irq1  */
		.flags  = IORESOURCE_IRQ,
	},
	{
		.name   = "rpm_smd_in",
		.start  = 32 + 168,             /* rpm_to_kpss_ipc_irq4  */
		.flags  = IORESOURCE_IRQ,
	},
};

static struct smd_subsystem_config smd_config_list[] = {
	{
		.irq_config_id = SMD_MODEM,
		.subsys_name = "modem",
		.edge = SMD_APPS_MODEM,

		.smd_int.irq_name = "modem_smd_in",
		.smd_int.flags = IRQF_TRIGGER_RISING,
		.smd_int.irq_id = -1,
		.smd_int.device_name = "smd_dev",
		.smd_int.dev_id = 0,
		.smd_int.out_bit_pos = 1 << 12,
		.smd_int.out_base = (void __iomem *)MSM_APCS_GCC_BASE,
		.smd_int.out_offset = 0x8,

		.smsm_int.irq_name = "modem_smsm_in",
		.smsm_int.flags = IRQF_TRIGGER_RISING,
		.smsm_int.irq_id = -1,
		.smsm_int.device_name = "smsm_dev",
		.smsm_int.dev_id = 0,
		.smsm_int.out_bit_pos = 1 << 13,
		.smsm_int.out_base = (void __iomem *)MSM_APCS_GCC_BASE,
		.smsm_int.out_offset = 0x8,
	},
	{
		.irq_config_id = SMD_Q6,
		.subsys_name = "adsp",
		.edge = SMD_APPS_QDSP,

		.smd_int.irq_name = "adsp_smd_in",
		.smd_int.flags = IRQF_TRIGGER_RISING,
		.smd_int.irq_id = -1,
		.smd_int.device_name = "smd_dev",
		.smd_int.dev_id = 0,
		.smd_int.out_bit_pos = 1 << 8,
		.smd_int.out_base = (void __iomem *)MSM_APCS_GCC_BASE,
		.smd_int.out_offset = 0x8,

		.smsm_int.irq_name = "adsp_smsm_in",
		.smsm_int.flags = IRQF_TRIGGER_RISING,
		.smsm_int.irq_id = -1,
		.smsm_int.device_name = "smsm_dev",
		.smsm_int.dev_id = 0,
		.smsm_int.out_bit_pos = 1 << 9,
		.smsm_int.out_base = (void __iomem *)MSM_APCS_GCC_BASE,
		.smsm_int.out_offset = 0x8,
	},
	{
		.irq_config_id = SMD_RPM,
		.subsys_name = NULL, /* do not use PIL to load RPM */
		.edge = SMD_APPS_RPM,

		.smd_int.irq_name = "rpm_smd_in",
		.smd_int.flags = IRQF_TRIGGER_RISING,
		.smd_int.irq_id = -1,
		.smd_int.device_name = "smd_dev",
		.smd_int.dev_id = 0,
		.smd_int.out_bit_pos = 1 << 0,
		.smd_int.out_base = (void __iomem *)MSM_APCS_GCC_BASE,
		.smd_int.out_offset = 0x8,

		.smsm_int.irq_name = NULL, /* RPM does not support SMSM */
		.smsm_int.flags = 0,
		.smsm_int.irq_id = 0,
		.smsm_int.device_name = NULL,
		.smsm_int.dev_id = 0,
		.smsm_int.out_bit_pos = 0,
		.smsm_int.out_base = NULL,
		.smsm_int.out_offset = 0,
	},
};

static struct smd_smem_regions aux_smem_areas[] = {
	{
		.phys_addr = (void *)(0xfc428000),
		.size = 0x4000,
	},
};

static struct smd_subsystem_restart_config smd_ssr_cfg = {
	.disable_smsm_reset_handshake = 1,
};

static struct smd_platform smd_platform_data = {
	.num_ss_configs = ARRAY_SIZE(smd_config_list),
	.smd_ss_configs = smd_config_list,
	.smd_ssr_config = &smd_ssr_cfg,
	.num_smem_areas = ARRAY_SIZE(aux_smem_areas),
	.smd_smem_areas = aux_smem_areas,
};

struct platform_device msm_device_smd_9625 = {
	.name   = "msm_smd",
	.id     = -1,
	.resource = smd_resource,
	.num_resources = ARRAY_SIZE(smd_resource),
	.dev = {
		.platform_data = &smd_platform_data,
	}
};

void __init msm9625_add_devices(void)
{
	platform_device_register(&msm_device_smd_9625);
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here.
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msm9625_add_drivers(void)
{
	msm_init_modem_notifier_list();
	msm_smd_init();
	msm_rpm_driver_init();
	rpm_regulator_smd_driver_init();
}

void __init msm9625_init(void)
{
	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm9625_init_gpiomux();
	msm_clock_init(&msm_dummy_clock_init_data);
	of_platform_populate(NULL, of_default_bus_match_table,
			msm9625_auxdata_lookup, NULL);
	msm9625_add_devices();
	msm9625_add_drivers();
}

DT_MACHINE_START(MSM_DT, "Qualcomm MSM (Flattened Device Tree)")
	.map_io = msm_map_msm9625_io,
	.init_irq = msm9625_init_irq,
	.init_machine = msm9625_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm9625_dt_match,
	.reserve = msm9625_reserve,
MACHINE_END
