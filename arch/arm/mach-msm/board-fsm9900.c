/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
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
#include <linux/etherdevice.h>
#include <linux/kernel.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/memory.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/uio_driver.h>
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <soc/qcom/restart.h>
#include <soc/qcom/socinfo.h>
#include <soc/qcom/smd.h>
#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"

#define FSM9900_MAC0_FUSE_PHYS	0xFC4B8440
#define FSM9900_MAC1_FUSE_PHYS	0xFC4B8448
#define FSM9900_MAC_FUSE_SIZE	0x10

#define FSM9900_FSM_ID_FUSE_PHYS	0xFC4BC0A0
#define FSM9900_FSM_ID_FUSE_SIZE	0x2

#define FSM_ID_MASK	0x000FFFFF
#define FSM_ID_FSM9900	0x0080f

#define FSM9900_QDSP6_0_DEBUG_DUMP_PHYS	0x25200000
#define FSM9900_QDSP6_1_DEBUG_DUMP_PHYS	0x25280000
#define FSM9900_QDSP6_2_DEBUG_DUMP_PHYS	0x25300000
#define FSM9900_SCLTE_DEBUG_DUMP_PHYS	0x25180000
#define FSM9900_SCLTE_DEBUG_TRACE_PHYS	0x1f100000
#define FSM9900_SCLTE_CDU_PHYS		0x1d000000
#define FSM9900_SCLTE_CB_TRACE_PHYS	0x3aa00000
#define FSM9900_SCLTE_RF_CAL_PHYS	0x3a000000
#define FSM9900_SCLTE_ETH_TRACE_PHYS	0x2fe8c000
#define FSM9900_SCLTE_DDR_PHYS		0x00100000
#define FSM9900_SCLTE_GEN_DBG_PHYS	0xf6000000

#define FSM9900_UIO_VERSION "1.0"

static struct of_dev_auxdata fsm9900_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, "msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, "msm_sdcc.2", NULL),
	{}
};

static struct uio_info fsm9900_uio_info[] = {
	{
		.name = "fsm9900-uio0",
		.version = FSM9900_UIO_VERSION,
	},
	{
		.name = "fsm9900-uio1",
		.version = FSM9900_UIO_VERSION,
	},
	{
		.name = "fsm9900-uio2",
		.version = FSM9900_UIO_VERSION,
	},
};

static struct resource fsm9900_uio0_resources[] = {
	{
		.start = FSM9900_QDSP6_0_DEBUG_DUMP_PHYS,
		.end   = FSM9900_QDSP6_0_DEBUG_DUMP_PHYS + SZ_512K - 1,
		.name  = "qdsp6_0_debug_dump",
		.flags = IORESOURCE_MEM,
	},
	{
		.start = FSM9900_QDSP6_1_DEBUG_DUMP_PHYS,
		.end   = FSM9900_QDSP6_1_DEBUG_DUMP_PHYS + SZ_512K - 1,
		.name  = "qdsp6_1_debug_dump",
		.flags = IORESOURCE_MEM,
	},
	{
		.start = FSM9900_QDSP6_2_DEBUG_DUMP_PHYS,
		.end   = FSM9900_QDSP6_2_DEBUG_DUMP_PHYS + SZ_1M - 1,
		.name  = "qdsp6_2_debug_dump",
		.flags = IORESOURCE_MEM,
	},
	{
		.start = FSM9900_SCLTE_DEBUG_DUMP_PHYS,
		.end   = FSM9900_SCLTE_DEBUG_DUMP_PHYS + SZ_512K - 1,
		.name  = "sclte_debug_dump",
		.flags = IORESOURCE_MEM,
	},
	{
		.start = FSM9900_SCLTE_DEBUG_TRACE_PHYS,
		.end   = FSM9900_SCLTE_DEBUG_TRACE_PHYS + SZ_512K - 1,
		.name  = "sclte_debug_trace",
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device fsm9900_uio0_device = {
	.name = "uio_pdrv",
	.id = 0,
	.dev = {
		.platform_data = &fsm9900_uio_info[0]
	},
	.num_resources = ARRAY_SIZE(fsm9900_uio0_resources),
	.resource = fsm9900_uio0_resources,
};

static struct resource fsm9900_uio1_resources[] = {
	{
		.start = FSM9900_SCLTE_CDU_PHYS,
		.end   = FSM9900_SCLTE_CDU_PHYS + SZ_16M - 1,
		.name  = "sclte_cdu",
		.flags = IORESOURCE_MEM,
	},
	{
		.start = FSM9900_SCLTE_CB_TRACE_PHYS,
		.end   = FSM9900_SCLTE_CB_TRACE_PHYS + SZ_2M - 1,
		.name  = "sclte_cb_trace",
		.flags = IORESOURCE_MEM,
	},
	{
		.start = FSM9900_SCLTE_RF_CAL_PHYS,
		.end   = FSM9900_SCLTE_RF_CAL_PHYS + SZ_8M + SZ_2M - 1,
		.name  = "sclte_rf_cal",
		.flags = IORESOURCE_MEM,
	},
	{
		.start = FSM9900_SCLTE_ETH_TRACE_PHYS,
		.end   = FSM9900_SCLTE_ETH_TRACE_PHYS + SZ_1M - 1,
		.name  = "sclte_eth_trace",
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device fsm9900_uio1_device = {
	.name = "uio_pdrv",
	.id = 1,
	.dev = {
		.platform_data = &fsm9900_uio_info[1]
	},
	.num_resources = ARRAY_SIZE(fsm9900_uio1_resources),
	.resource = fsm9900_uio1_resources,
};

static struct resource fsm9900_uio2_resources[] = {
	{
		.start = FSM9900_SCLTE_DDR_PHYS,
		.end   = FSM9900_SCLTE_DDR_PHYS + 181 * SZ_1M - 1,
		.name  = "sclte_ddr",
		.flags = IORESOURCE_MEM,
	},
	{
		.start = FSM9900_SCLTE_GEN_DBG_PHYS,
		.end   = FSM9900_SCLTE_GEN_DBG_PHYS + SZ_32M - 1,
		.name  = "sclte_gen_dbg",
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device fsm9900_uio2_device = {
	.name = "uio_pdrv",
	.id = 2,
	.dev = {
		.platform_data = &fsm9900_uio_info[2]
	},
	.num_resources = ARRAY_SIZE(fsm9900_uio2_resources),
	.resource = fsm9900_uio2_resources,
};

static struct platform_device *fsm9900_uio_devices[] = {
	&fsm9900_uio0_device,
	&fsm9900_uio1_device,
};

static const char mac_addr_prop_name[] = "mac-address";

void __init fsm9900_reserve(void)
{
}

static bool is_fsm9900(void)
{
	void __iomem *fuse_reg;
	u32 fsm_id;

	fuse_reg = ioremap(FSM9900_FSM_ID_FUSE_PHYS,
			   FSM9900_FSM_ID_FUSE_SIZE);
	if (!fuse_reg) {
		pr_err("failed to ioremap fuse to read fsm id");
		return false;
	}

	fsm_id = ioread16(fuse_reg) & FSM_ID_MASK;
	iounmap(fuse_reg);

	return (fsm_id == FSM_ID_FSM9900) ? true : false;
}

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init fsm9900_add_drivers(void)
{
	msm_smd_init();
	if (of_board_is_rumi())
		msm_clock_init(&fsm9900_dummy_clock_init_data);
	else
		msm_clock_init(&fsm9900_clock_init_data);
	platform_add_devices(fsm9900_uio_devices,
			     ARRAY_SIZE(fsm9900_uio_devices));
	if (is_fsm9900())
		platform_device_register(&fsm9900_uio2_device);
}

static void __init fsm9900_map_io(void)
{
	msm_map_fsm9900_io();
}

static int emac_dt_update(int cell, phys_addr_t addr, unsigned long size)
{
	/*
	 * Use an array for the fuse. Corrected fuse data may be located
	 * at a different offsets.
	 */
	static int offset[ETH_ALEN] = { 0, 1, 2, 3, 4, 5};
	void __iomem *fuse_reg;
	struct device_node *np = NULL;
	struct property *pmac = NULL;
	struct property *pp = NULL;
	u8 buf[ETH_ALEN];
	int n;
	int retval = 0;

	fuse_reg = ioremap(addr, size);
	if (!fuse_reg) {
		pr_err("failed to ioremap efuse to read mac address");
		return -ENOMEM;
	}

	for (n = 0; n < ETH_ALEN; n++)
		buf[n] = ioread8(fuse_reg + offset[n]);

	iounmap(fuse_reg);

	if (!is_valid_ether_addr(buf)) {
		pr_err("invalid MAC address in efuse\n");
		return -ENODATA;
	}

	pmac = kzalloc(sizeof(*pmac) + ETH_ALEN, GFP_KERNEL);
	if (!pmac) {
		pr_err("failed to alloc memory for mac address\n");
		return -ENOMEM;
	}

	pmac->value = pmac + 1;
	pmac->length = ETH_ALEN;
	pmac->name = (char *)mac_addr_prop_name;
	memcpy(pmac->value, buf, ETH_ALEN);

	for_each_compatible_node(np, NULL, "qcom,emac") {
		if (of_property_read_u32(np, "cell-index", &n))
			continue;
		if (n == cell)
			break;
	}

	if (!np) {
		pr_err("failed to find dt node for emac%d", cell);
		retval = -ENODEV;
		goto out;
	}

	pp = of_find_property(np, pmac->name, NULL);
	if (pp)
		of_update_property(np, pmac);
	else
		of_add_property(np, pmac);

out:
	if (retval && pmac)
		kfree(pmac);

	return retval;
}

int __init fsm9900_emac_dt_update(void)
{
	emac_dt_update(0, FSM9900_MAC0_FUSE_PHYS, FSM9900_MAC_FUSE_SIZE);
	emac_dt_update(1, FSM9900_MAC1_FUSE_PHYS, FSM9900_MAC_FUSE_SIZE);
	return 0;
}

void __init fsm9900_init(void)
{
	struct of_dev_auxdata *adata = fsm9900_auxdata_lookup;

	/*
	 * populate devices from DT first so smem probe will get called as part
	 * of msm_smem_init.  socinfo_init needs smem support so call
	 * msm_smem_init before it.
	 */
	board_dt_populate(adata);

	msm_smem_init();

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	fsm9900_init_gpiomux();
	fsm9900_emac_dt_update();

	fsm9900_add_drivers();
}

static const char *fsm9900_dt_match[] __initconst = {
	"qcom,fsm9900",
	NULL
};

DT_MACHINE_START(FSM9900_DT,
		"Qualcomm Technologies, Inc. FSM 9900 (Flattened Device Tree)")
	.map_io			= fsm9900_map_io,
	.init_machine		= fsm9900_init,
	.dt_compat		= fsm9900_dt_match,
	.reserve		= fsm9900_reserve,
	.smp			= &msm8974_smp_ops,
MACHINE_END
