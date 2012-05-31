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

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#ifdef CONFIG_ION_MSM
#include <linux/ion.h>
#endif
#include <linux/memory.h>
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif
#include <linux/regulator/stub-regulator.h>
#include <linux/regulator/machine.h>
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <mach/board.h>
#include <mach/gpio.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#include <mach/msm_memtypes.h>
#include <mach/msm_smd.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/qpnp-int.h>
#include <mach/socinfo.h>
#include "clock.h"
#include "devices.h"
#include "spm.h"

#define MSM_KERNEL_EBI1_MEM_SIZE	0x280000
#ifdef CONFIG_FB_MSM_HDMI_AS_PRIMARY
#define MSM_ION_SF_SIZE 0x4000000 /* 64 Mbytes */
#else
#define MSM_ION_SF_SIZE 0x2800000 /* 40 Mbytes */
#endif
#define MSM_ION_MM_FW_SIZE	0xa00000 /* (10MB) */
#define MSM_ION_MM_SIZE		0x7800000 /* (120MB) */
#define MSM_ION_QSECOM_SIZE	0x100000 /* (1MB) */
#define MSM_ION_MFC_SIZE	SZ_8K
#define MSM_ION_AUDIO_SIZE	0x2B4000
#define MSM_ION_HEAP_NUM	8

#ifdef CONFIG_KERNEL_PMEM_EBI_REGION
static unsigned kernel_ebi1_mem_size = MSM_KERNEL_EBI1_MEM_SIZE;
static int __init kernel_ebi1_mem_size_setup(char *p)
{
	kernel_ebi1_mem_size = memparse(p, NULL);
	return 0;
}
early_param("kernel_ebi1_mem_size", kernel_ebi1_mem_size_setup);
#endif

static struct memtype_reserve msm_copper_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static int msm_copper_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

#ifdef CONFIG_ION_MSM
static struct ion_cp_heap_pdata cp_mm_ion_pdata = {
	.permission_type = IPT_TYPE_MM_CARVEOUT,
	.align = PAGE_SIZE,
};

static struct ion_cp_heap_pdata cp_mfc_ion_pdata = {
	.permission_type = IPT_TYPE_MFC_SHAREDMEM,
	.align = PAGE_SIZE,
};

static struct ion_co_heap_pdata co_ion_pdata = {
	.adjacent_mem_id = INVALID_HEAP_ID,
	.align = PAGE_SIZE,
};

static struct ion_co_heap_pdata fw_co_ion_pdata = {
	.adjacent_mem_id = ION_CP_MM_HEAP_ID,
	.align = SZ_128K,
};

/**
 * These heaps are listed in the order they will be allocated. Due to
 * video hardware restrictions and content protection the FW heap has to
 * be allocated adjacent (below) the MM heap and the MFC heap has to be
 * allocated after the MM heap to ensure MFC heap is not more than 256MB
 * away from the base address of the FW heap.
 * However, the order of FW heap and MM heap doesn't matter since these
 * two heaps are taken care of by separate code to ensure they are adjacent
 * to each other.
 * Don't swap the order unless you know what you are doing!
 */
static struct ion_platform_data ion_pdata = {
	.nr = MSM_ION_HEAP_NUM,
	.heaps = {
		{
			.id	= ION_SYSTEM_HEAP_ID,
			.type	= ION_HEAP_TYPE_SYSTEM,
			.name	= ION_VMALLOC_HEAP_NAME,
		},
		{
			.id	= ION_CP_MM_HEAP_ID,
			.type	= ION_HEAP_TYPE_CP,
			.name	= ION_MM_HEAP_NAME,
			.size	= MSM_ION_MM_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &cp_mm_ion_pdata,
		},
		{
			.id	= ION_MM_FIRMWARE_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_MM_FIRMWARE_HEAP_NAME,
			.size	= MSM_ION_MM_FW_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &fw_co_ion_pdata,
		},
		{
			.id	= ION_CP_MFC_HEAP_ID,
			.type	= ION_HEAP_TYPE_CP,
			.name	= ION_MFC_HEAP_NAME,
			.size	= MSM_ION_MFC_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &cp_mfc_ion_pdata,
		},
		{
			.id	= ION_SF_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_SF_HEAP_NAME,
			.size	= MSM_ION_SF_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &co_ion_pdata,
		},
		{
			.id	= ION_IOMMU_HEAP_ID,
			.type	= ION_HEAP_TYPE_IOMMU,
			.name	= ION_IOMMU_HEAP_NAME,
		},
		{
			.id	= ION_QSECOM_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_QSECOM_HEAP_NAME,
			.size	= MSM_ION_QSECOM_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &co_ion_pdata,
		},
		{
			.id	= ION_AUDIO_HEAP_ID,
			.type	= ION_HEAP_TYPE_CARVEOUT,
			.name	= ION_AUDIO_HEAP_NAME,
			.size	= MSM_ION_AUDIO_SIZE,
			.memory_type = ION_EBI_TYPE,
			.extra_data = (void *) &co_ion_pdata,
		},
	}
};

static struct platform_device ion_dev = {
	.name = "ion-msm",
	.id = 1,
	.dev = { .platform_data = &ion_pdata },
};

static void __init reserve_ion_memory(void)
{
	msm_copper_reserve_table[MEMTYPE_EBI1].size += MSM_ION_MM_SIZE;
	msm_copper_reserve_table[MEMTYPE_EBI1].size += MSM_ION_MM_FW_SIZE;
	msm_copper_reserve_table[MEMTYPE_EBI1].size += MSM_ION_SF_SIZE;
	msm_copper_reserve_table[MEMTYPE_EBI1].size += MSM_ION_MFC_SIZE;
	msm_copper_reserve_table[MEMTYPE_EBI1].size += MSM_ION_QSECOM_SIZE;
	msm_copper_reserve_table[MEMTYPE_EBI1].size += MSM_ION_AUDIO_SIZE;
#ifdef CONFIG_KERNEL_PMEM_EBI_REGION
	msm_copper_reserve_table[MEMTYPE_EBI1].size += kernel_ebi1_mem_size;
#endif
}
#endif

static struct resource smd_resource[] = {
	{
		.name	= "modem_smd_in",
		.start	= 32 + 17,		/* mss_sw_to_kpss_ipc_irq0  */
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "modem_smsm_in",
		.start	= 32 + 18,		/* mss_sw_to_kpss_ipc_irq1  */
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "adsp_smd_in",
		.start	= 32 + 156,		/* lpass_to_kpss_ipc_irq0  */
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "adsp_smsm_in",
		.start	= 32 + 157,		/* lpass_to_kpss_ipc_irq1  */
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "wcnss_smd_in",
		.start	= 32 + 142,		/* WcnssAppsSmdMedIrq  */
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "wcnss_smsm_in",
		.start	= 32 + 144,		/* RivaAppsWlanSmsmIrq  */
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "rpm_smd_in",
		.start	= 32 + 168,		/* rpm_to_kpss_ipc_irq4  */
		.flags	= IORESOURCE_IRQ,
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
		.subsys_name = "q6",
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
		.irq_config_id = SMD_WCNSS,
		.subsys_name = "wcnss",
		.edge = SMD_APPS_WCNSS,

		.smd_int.irq_name = "wcnss_smd_in",
		.smd_int.flags = IRQF_TRIGGER_RISING,
		.smd_int.irq_id = -1,
		.smd_int.device_name = "smd_dev",
		.smd_int.dev_id = 0,
		.smd_int.out_bit_pos = 1 << 17,
		.smd_int.out_base = (void __iomem *)MSM_APCS_GCC_BASE,
		.smd_int.out_offset = 0x8,

		.smsm_int.irq_name = "wcnss_smsm_in",
		.smsm_int.flags = IRQF_TRIGGER_RISING,
		.smsm_int.irq_id = -1,
		.smsm_int.device_name = "smsm_dev",
		.smsm_int.dev_id = 0,
		.smsm_int.out_bit_pos = 1 << 19,
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

struct platform_device msm_device_smd_copper = {
	.name	= "msm_smd",
	.id	= -1,
	.resource = smd_resource,
	.num_resources = ARRAY_SIZE(smd_resource),
	.dev = {
		.platform_data = &smd_platform_data,
	}
};

static void __init msm_copper_calculate_reserve_sizes(void)
{
#ifdef CONFIG_ION_MSM
	reserve_ion_memory();
#endif
}

static struct reserve_info msm_copper_reserve_info __initdata = {
	.memtype_reserve_table = msm_copper_reserve_table,
	.calculate_reserve_sizes = msm_copper_calculate_reserve_sizes,
	.paddr_to_memtype = msm_copper_paddr_to_memtype,
};

static void __init msm_copper_early_memory(void)
{
	reserve_info = &msm_copper_reserve_info;
}

void __init msm_copper_reserve(void)
{
	msm_reserve();
}

static struct platform_device android_usb_device = {
	.name	= "android_usb",
	.id	= -1,
};

#define SHARED_IMEM_TZ_BASE 0xFE805720
static struct resource copper_tzlog_resources[] = {
	{
		.start = SHARED_IMEM_TZ_BASE,
		.end = SHARED_IMEM_TZ_BASE + SZ_4K - 1,
		.flags = IORESOURCE_MEM,
	},
};

struct platform_device copper_device_tz_log = {
	.name		= "tz_log",
	.id		= 0,
	.num_resources	= ARRAY_SIZE(copper_tzlog_resources),
	.resource	= copper_tzlog_resources,
};

#ifdef CONFIG_HW_RANDOM_MSM
/* PRNG device */
#define MSM_PRNG_PHYS  0xF9BFF000
static struct resource rng_resources = {
	.flags = IORESOURCE_MEM,
	.start = MSM_PRNG_PHYS,
	.end   = MSM_PRNG_PHYS + SZ_512 - 1,
};

struct platform_device msm8974_device_rng = {
	.name          = "msm_rng",
	.id            = 0,
	.num_resources = 1,
	.resource      = &rng_resources,
};
#endif


void __init msm_copper_add_devices(void)
{
#ifdef CONFIG_ION_MSM
	platform_device_register(&ion_dev);
#endif
	platform_device_register(&msm_device_smd_copper);
	platform_device_register(&android_usb_device);
	platform_add_devices(msm_copper_stub_regulator_devices,
					msm_copper_stub_regulator_devices_len);
	platform_device_register(&copper_device_tz_log);
#ifdef CONFIG_HW_RANDOM_MSM
	platform_device_register(&msm8974_device_rng);
#endif
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msm_copper_add_drivers(void)
{
	msm_smd_init();
	msm_rpm_driver_init();
	rpm_regulator_smd_driver_init();
	msm_spm_device_init();
	regulator_stub_init();
}

static struct of_device_id irq_match[] __initdata  = {
	{ .compatible = "qcom,msm-qgic2", .data = gic_of_init, },
	{ .compatible = "qcom,msm-gpio", .data = msm_gpio_of_init, },
	{ .compatible = "qcom,spmi-pmic-arb", .data = qpnpint_of_init, },
	{}
};

void __init msm_copper_init_irq(void)
{
	of_irq_init(irq_match);
}

static struct clk_lookup msm_clocks_dummy[] = {
	CLK_DUMMY("xo",		XO_CLK,		NULL,	OFF),
	CLK_DUMMY("xo",		XO_CLK,		"pil_pronto",		OFF),
	CLK_DUMMY("core_clk",	BLSP2_UART_CLK,	"msm_serial_hsl.0",	OFF),
	CLK_DUMMY("iface_clk",	BLSP2_UART_CLK,	"msm_serial_hsl.0",	OFF),
	CLK_DUMMY("core_clk",	SDC1_CLK,	NULL,			OFF),
	CLK_DUMMY("iface_clk",	SDC1_P_CLK,	NULL,			OFF),
	CLK_DUMMY("core_clk",	SDC3_CLK,	NULL,			OFF),
	CLK_DUMMY("iface_clk",	SDC3_P_CLK,	NULL,			OFF),
	CLK_DUMMY("phy_clk", NULL, "msm_otg", OFF),
	CLK_DUMMY("core_clk", NULL, "msm_otg", OFF),
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

static struct of_dev_auxdata msm_copper_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-lsuart-v14", 0xF991F000, \
			"msm_serial_hsl.0", NULL),
	OF_DEV_AUXDATA("qcom,hsusb-otg", 0xF9A55000, \
			"msm_otg", NULL),
	OF_DEV_AUXDATA("qcom,spi-qup-v2", 0xF9924000, \
			"spi_qsd.1", NULL),
	OF_DEV_AUXDATA("qcom,spmi-pmic-arb", 0xFC4C0000, \
			"spmi-pmic-arb.0", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9864000, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98E4000, \
			"msm_sdcc.4", NULL),
	OF_DEV_AUXDATA("qcom,pil-q6v5-lpass",   0xFE200000, \
			"pil-q6v5-lpass", NULL),
	OF_DEV_AUXDATA("qcom,pil-pronto", 0xFB21B000, \
			"pil_pronto", NULL),
	{}
};

void __init msm_copper_init(struct of_dev_auxdata **adata)
{
	msm_copper_init_gpiomux();

	if (machine_is_copper_rumi())
		msm_clock_init(&msm_dummy_clock_init_data);
	else
		msm_clock_init(&msmcopper_clock_init_data);

	*adata = msm_copper_auxdata_lookup;

	regulator_has_full_constraints();
}

void __init msm_copper_very_early(void)
{
	msm_copper_early_memory();
}
