/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_irq.h>
#include <linux/memory.h>
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif
#include <linux/regulator/machine.h>
#include <linux/regulator/krait-regulator.h>
#include <linux/msm_thermal.h>
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <mach/board.h>
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
#include <mach/msm_bus_board.h>
#include <mach/mpm.h>
#include "clock.h"
#include "devices.h"
#include "spm.h"
#include "modem_notifier.h"
#include "lpm_resources.h"

#define MSM_KERNEL_EBI1_MEM_SIZE	0x280000

#ifdef CONFIG_KERNEL_PMEM_EBI_REGION
static unsigned kernel_ebi1_mem_size = MSM_KERNEL_EBI1_MEM_SIZE;
static int __init kernel_ebi1_mem_size_setup(char *p)
{
	kernel_ebi1_mem_size = memparse(p, NULL);
	return 0;
}
early_param("kernel_ebi1_mem_size", kernel_ebi1_mem_size_setup);
#endif

static struct memtype_reserve msm_8974_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static int msm_8974_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static void __init reserve_ebi_memory(void)
{
#ifdef CONFIG_KERNEL_PMEM_EBI_REGION
	msm_8974_reserve_table[MEMTYPE_EBI1].size += kernel_ebi1_mem_size;
#endif
}

static struct resource smd_resource[] = {
	{
		.name	= "modem_smd_in",
		.start	= 32 + 25,		/* mss_sw_to_kpss_ipc_irq0  */
		.flags	= IORESOURCE_IRQ,
	},
	{
		.name	= "modem_smsm_in",
		.start	= 32 + 26,		/* mss_sw_to_kpss_ipc_irq1  */
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

struct platform_device msm_device_smd_8974 = {
	.name	= "msm_smd",
	.id	= -1,
	.resource = smd_resource,
	.num_resources = ARRAY_SIZE(smd_resource),
	.dev = {
		.platform_data = &smd_platform_data,
	}
};

static void __init msm_8974_calculate_reserve_sizes(void)
{
	reserve_ebi_memory();
}

static struct reserve_info msm_8974_reserve_info __initdata = {
	.memtype_reserve_table = msm_8974_reserve_table,
	.calculate_reserve_sizes = msm_8974_calculate_reserve_sizes,
	.paddr_to_memtype = msm_8974_paddr_to_memtype,
};

static void __init msm_8974_early_memory(void)
{
	reserve_info = &msm_8974_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, msm_8974_reserve_table);
}

void __init msm_8974_reserve(void)
{
	msm_reserve();
}

#define BIMC_BASE	0xfc380000
#define BIMC_SIZE	0x0006A000
#define SYS_NOC_BASE	0xfc460000
#define PERIPH_NOC_BASE 0xFC468000
#define OCMEM_NOC_BASE	0xfc470000
#define	MMSS_NOC_BASE	0xfc478000
#define CONFIG_NOC_BASE	0xfc480000
#define NOC_SIZE	0x00004000

static struct resource bimc_res[] = {
	{
		.start = BIMC_BASE,
		.end = BIMC_BASE + BIMC_SIZE,
		.flags = IORESOURCE_MEM,
		.name = "bimc_mem",
	},
};

static struct resource ocmem_noc_res[] = {
	{
		.start = OCMEM_NOC_BASE,
		.end = OCMEM_NOC_BASE + NOC_SIZE,
		.flags = IORESOURCE_MEM,
		.name = "ocmem_noc_mem",
	},
};

static struct resource mmss_noc_res[] = {
	{
		.start = MMSS_NOC_BASE,
		.end = MMSS_NOC_BASE + NOC_SIZE,
		.flags = IORESOURCE_MEM,
		.name = "mmss_noc_mem",
	},
};

static struct resource sys_noc_res[] = {
	{
		.start = SYS_NOC_BASE,
		.end = SYS_NOC_BASE + NOC_SIZE,
		.flags = IORESOURCE_MEM,
		.name = "sys_noc_mem",
	},
};

static struct resource config_noc_res[] = {
	{
		.start = CONFIG_NOC_BASE,
		.end = CONFIG_NOC_BASE + NOC_SIZE,
		.flags = IORESOURCE_MEM,
		.name = "config_noc_mem",
	},
};

static struct resource periph_noc_res[] = {
	{
		.start = PERIPH_NOC_BASE,
		.end = PERIPH_NOC_BASE + NOC_SIZE,
		.flags = IORESOURCE_MEM,
		.name = "periph_noc_mem",
	},
};

static struct platform_device msm_bus_sys_noc = {
	.name  = "msm_bus_fabric",
	.id    =  MSM_BUS_FAB_SYS_NOC,
	.num_resources = ARRAY_SIZE(sys_noc_res),
	.resource = sys_noc_res,
};

static struct platform_device msm_bus_bimc = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_BIMC,
	.num_resources = ARRAY_SIZE(bimc_res),
	.resource = bimc_res,
};

static struct platform_device msm_bus_mmss_noc = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_MMSS_NOC,
	.num_resources = ARRAY_SIZE(mmss_noc_res),
	.resource = mmss_noc_res,
};

static struct platform_device msm_bus_ocmem_noc = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_OCMEM_NOC,
	.num_resources = ARRAY_SIZE(ocmem_noc_res),
	.resource = ocmem_noc_res,
};

static struct platform_device msm_bus_periph_noc = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_PERIPH_NOC,
	.num_resources = ARRAY_SIZE(periph_noc_res),
	.resource = periph_noc_res,
};

static struct platform_device msm_bus_config_noc = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_CONFIG_NOC,
	.num_resources = ARRAY_SIZE(config_noc_res),
	.resource = config_noc_res,
};

static struct platform_device msm_bus_ocmem_vnoc = {
	.name  = "msm_bus_fabric",
	.id    = MSM_BUS_FAB_OCMEM_VNOC,
};

static struct platform_device msm_fm_platform_init = {
	.name  = "iris_fm",
	.id    = -1,
};

static struct platform_device *msm_bus_8974_devices[] = {
	&msm_bus_sys_noc,
	&msm_bus_bimc,
	&msm_bus_mmss_noc,
	&msm_bus_ocmem_noc,
	&msm_bus_periph_noc,
	&msm_bus_config_noc,
	&msm_bus_ocmem_vnoc,
	&msm_fm_platform_init,
};

static ssize_t mxt336s_vkeys_show(struct kobject *kobj,
			struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, 200,
	__stringify(EV_KEY) ":" __stringify(KEY_BACK) ":62:1345:90:90" \
	":" __stringify(EV_KEY) ":" __stringify(KEY_MENU) ":240:1345:90:90" \
	":" __stringify(EV_KEY) ":" __stringify(KEY_HOME) ":470:1345:90:90" \
	":" __stringify(EV_KEY) ":" __stringify(KEY_SEARCH) ":658:1345:90:90" \
	"\n");
}

static struct kobj_attribute mxt336s_vkeys_attr = {
	.attr = {
		.mode = S_IRUGO,
	},
	.show = &mxt336s_vkeys_show,
};

static struct attribute *mxt336s_properties_attrs[] = {
	&mxt336s_vkeys_attr.attr,
	NULL
};

static struct attribute_group mxt336s_properties_attr_group = {
	.attrs = mxt336s_properties_attrs,
};

static void mxt_init_vkeys_8974(void)
{
	int rc = 0;
	static struct kobject *mxt336s_properties_kobj;

	mxt336s_vkeys_attr.attr.name = "virtualkeys.atmel_mxt_ts";
	mxt336s_properties_kobj = kobject_create_and_add("board_properties",
								NULL);
	if (mxt336s_properties_kobj)
		rc = sysfs_create_group(mxt336s_properties_kobj,
					&mxt336s_properties_attr_group);
	if (!mxt336s_properties_kobj || rc)
		pr_err("%s: failed to create board_properties\n",
				__func__);

	return;
}

static void __init msm8974_init_buses(void)
{
#ifdef CONFIG_MSM_BUS_SCALING
	msm_bus_sys_noc.dev.platform_data =
		&msm_bus_8974_sys_noc_pdata;
	msm_bus_bimc.dev.platform_data = &msm_bus_8974_bimc_pdata;
	msm_bus_mmss_noc.dev.platform_data = &msm_bus_8974_mmss_noc_pdata;
	msm_bus_ocmem_noc.dev.platform_data = &msm_bus_8974_ocmem_noc_pdata;
	msm_bus_periph_noc.dev.platform_data = &msm_bus_8974_periph_noc_pdata;
	msm_bus_config_noc.dev.platform_data = &msm_bus_8974_config_noc_pdata;
	msm_bus_ocmem_vnoc.dev.platform_data = &msm_bus_8974_ocmem_vnoc_pdata;
#endif
	platform_add_devices(msm_bus_8974_devices,
				ARRAY_SIZE(msm_bus_8974_devices));
};

void __init msm_8974_add_devices(void)
{
	platform_device_register(&msm_device_smd_8974);
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msm_8974_add_drivers(void)
{
	msm_init_modem_notifier_list();
	msm_smd_init();
	msm_rpm_driver_init();
	msm_lpmrs_module_init();
	rpm_regulator_smd_driver_init();
	msm_spm_device_init();
	krait_power_init();
	if (machine_is_msm8974_rumi())
		msm_clock_init(&msm8974_rumi_clock_init_data);
	else
		msm_clock_init(&msm8974_clock_init_data);
	msm8974_init_buses();
	msm_thermal_device_init();
	mxt_init_vkeys_8974();
}

static struct of_device_id irq_match[] __initdata  = {
	{ .compatible = "qcom,msm-qgic2", .data = gic_of_init, },
	{ .compatible = "qcom,msm-gpio", .data = msm_gpio_of_init, },
	{ .compatible = "qcom,spmi-pmic-arb", .data = qpnpint_of_init, },
	{}
};
static struct of_device_id mpm_match[] __initdata = {
	{.compatible = "qcom,mpm-v2", },
	{},
};

void __init msm_8974_init_irq(void)
{
	struct device_node *node;

	of_irq_init(irq_match);
	node = of_find_matching_node(NULL, mpm_match);

	WARN_ON(!node);

	if (node)
		of_mpm_init(node);
}

static struct of_dev_auxdata msm_8974_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,hsusb-otg", 0xF9A55000, \
			"msm_otg", NULL),
	OF_DEV_AUXDATA("qcom,dwc-usb3-msm", 0xF9200000, \
			"msm_dwc3", NULL),
	OF_DEV_AUXDATA("qcom,usb-bam-msm", 0xF9304000, \
			"usb_bam", NULL),
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
	OF_DEV_AUXDATA("qcom,pil-q6v5-mss", 0xFC880000, "pil-q6v5-mss", NULL),
	OF_DEV_AUXDATA("qcom,pil-mba",     0xFC820000, "pil-mba", NULL),
	OF_DEV_AUXDATA("qcom,pil-pronto", 0xFB21B000, \
			"pil_pronto", NULL),
	OF_DEV_AUXDATA("arm,coresight-tmc", 0xFC322000, \
			"coresight-tmc-etr", NULL),
	OF_DEV_AUXDATA("arm,coresight-tpiu", 0xFC318000, \
			"coresight-tpiu", NULL),
	OF_DEV_AUXDATA("qcom,coresight-replicator", 0xFC31C000, \
			"coresight-replicator", NULL),
	OF_DEV_AUXDATA("arm,coresight-tmc", 0xFC307000, \
			"coresight-tmc-etf", NULL),
	OF_DEV_AUXDATA("arm,coresight-funnel", 0xFC31B000, \
			"coresight-funnel-merg", NULL),
	OF_DEV_AUXDATA("arm,coresight-funnel", 0xFC319000, \
			"coresight-funnel-in0", NULL),
	OF_DEV_AUXDATA("arm,coresight-funnel", 0xFC31A000, \
			"coresight-funnel-in1", NULL),
	OF_DEV_AUXDATA("arm,coresight-funnel", 0xFC345000, \
			"coresight-funnel-kpss", NULL),
	OF_DEV_AUXDATA("arm,coresight-funnel", 0xFC364000, \
			"coresight-funnel-mmss", NULL),
	OF_DEV_AUXDATA("arm,coresight-stm", 0xFC321000, \
			"coresight-stm", NULL),
	OF_DEV_AUXDATA("arm,coresight-etm", 0xFC33C000, \
			"coresight-etm0", NULL),
	OF_DEV_AUXDATA("arm,coresight-etm", 0xFC33D000, \
			"coresight-etm1", NULL),
	OF_DEV_AUXDATA("arm,coresight-etm", 0xFC33E000, \
			"coresight-etm2", NULL),
	OF_DEV_AUXDATA("arm,coresight-etm", 0xFC33F000, \
			"coresight-etm3", NULL),
	OF_DEV_AUXDATA("qcom,msm-rng", 0xF9BFF000, \
			"msm_rng", NULL),
	OF_DEV_AUXDATA("qcom,qseecom", 0xFE806000, \
			"qseecom", NULL),
	OF_DEV_AUXDATA("qcom,mdss_mdp", 0xFD900000, "mdp.0", NULL),
	OF_DEV_AUXDATA("qcom,msm-tsens", 0xFC4A8000, \
			"msm-tsens", NULL),
	OF_DEV_AUXDATA("qcom,qcedev", 0xFD440000, \
			"qcedev.0", NULL),
	OF_DEV_AUXDATA("qcom,qcrypto", 0xFD440000, \
			"qcrypto.0", NULL),
	{}
};

void __init msm_8974_init(struct of_dev_auxdata **adata)
{
	msm_8974_init_gpiomux();

	*adata = msm_8974_auxdata_lookup;

	regulator_has_full_constraints();
}

void __init msm_8974_very_early(void)
{
	msm_8974_early_memory();
}
