/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/memory.h>
#ifdef CONFIG_ANDROID_PMEM
#include <linux/android_pmem.h>
#endif
#include <linux/regulator/machine.h>
#include <linux/regulator/krait-regulator.h>
#include <linux/msm_thermal.h>
#include <asm/mach/map.h>
#include <asm/hardware/gic.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#ifdef CONFIG_ION_MSM
#include <mach/ion.h>
#endif
#include <mach/msm_memtypes.h>
#include <mach/msm_smd.h>
#include <mach/restart.h>
#include <mach/rpm-smd.h>
#include <mach/rpm-regulator-smd.h>
#include <mach/socinfo.h>
#include <mach/msm_bus_board.h>
#include "board-dt.h"
#include "clock.h"
#include "devices.h"
#include "spm.h"
#include "modem_notifier.h"
#include "lpm_resources.h"
#include "platsmp.h"


static struct memtype_reserve msm8974_reserve_table[] __initdata = {
	[MEMTYPE_SMI] = {
	},
	[MEMTYPE_EBI0] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
	[MEMTYPE_EBI1] = {
		.flags	=	MEMTYPE_FLAGS_1M_ALIGN,
	},
};

static int msm8974_paddr_to_memtype(unsigned int paddr)
{
	return MEMTYPE_EBI1;
}

static struct reserve_info msm8974_reserve_info __initdata = {
	.memtype_reserve_table = msm8974_reserve_table,
	.paddr_to_memtype = msm8974_paddr_to_memtype,
};

void __init msm_8974_reserve(void)
{
	reserve_info = &msm8974_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_reserve, msm8974_reserve_table);
	msm_reserve();
}

static void __init msm8974_early_memory(void)
{
	reserve_info = &msm8974_reserve_info;
	of_scan_flat_dt(dt_scan_for_memory_hole, msm8974_reserve_table);
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

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init msm8974_add_drivers(void)
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

static struct of_dev_auxdata msm8974_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,hsusb-otg", 0xF9A55000, \
			"msm_otg", NULL),
	OF_DEV_AUXDATA("qcom,ehci-host", 0xF9A55000, \
			"msm_ehci_host", NULL),
	OF_DEV_AUXDATA("qcom,dwc-usb3-msm", 0xF9200000, \
			"msm_dwc3", NULL),
	OF_DEV_AUXDATA("qcom,usb-bam-msm", 0xF9304000, \
			"usb_bam", NULL),
	OF_DEV_AUXDATA("qcom,spi-qup-v2", 0xF9924000, \
			"spi_qsd.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9864000, \
			"msm_sdcc.3", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98E4000, \
			"msm_sdcc.4", NULL),
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

static void __init msm8974_map_io(void)
{
	msm_map_8974_io();
}

void __init msm8974_init(void)
{
	struct of_dev_auxdata *adata = msm8974_auxdata_lookup;
	struct device *parent;

	parent = socinfo_init();
	if (IS_ERR_OR_NULL(parent))
		pr_err("%s: socinfo_init() failed\n", __func__);

	msm_8974_init_gpiomux();
	regulator_has_full_constraints();
	of_platform_populate(NULL, of_default_bus_match_table, adata, NULL);
	msm8974_add_drivers();
}

void __init msm8974_init_very_early(void)
{
	msm8974_early_memory();
}

static const char *msm8974_dt_match[] __initconst = {
	"qcom,msm8974",
	NULL
};

DT_MACHINE_START(MSM8974_DT, "Qualcomm MSM 8974 (Flattened Device Tree)")
	.map_io = msm8974_map_io,
	.init_irq = msm_dt_init_irq,
	.init_machine = msm8974_init,
	.handle_irq = gic_handle_irq,
	.timer = &msm_dt_timer,
	.dt_compat = msm8974_dt_match,
	.reserve = msm_8974_reserve,
	.init_very_early = msm8974_init_very_early,
	.restart = msm_restart,
	.smp = &msm8974_smp_ops,
MACHINE_END
