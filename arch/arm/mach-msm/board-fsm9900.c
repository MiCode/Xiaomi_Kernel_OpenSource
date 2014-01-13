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
#include <asm/mach/map.h>
#include <asm/mach/arch.h>
#include <mach/board.h>
#include <mach/gpiomux.h>
#include <mach/msm_iomap.h>
#include <mach/msm_smd.h>
#include <mach/restart.h>
#include <soc/qcom/socinfo.h>
#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"

#define FSM9900_MAC0_FUSE_PHYS	0xFC4B8440
#define FSM9900_MAC1_FUSE_PHYS	0xFC4B8448
#define FSM9900_MAC_FUSE_SIZE	0x10

static struct of_dev_auxdata fsm9900_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, \
			"msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, \
			"msm_sdcc.2", NULL),
	{}
};

static const char mac_addr_prop_name[] = "mac-address";

void __init fsm9900_reserve(void)
{
}

static void __init fsm9900_early_memory(void)
{
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

void __init fsm9900_init_very_early(void)
{
	fsm9900_early_memory();
}

static const char *fsm9900_dt_match[] __initconst = {
	"qcom,fsm9900",
	NULL
};

DT_MACHINE_START(FSM9900_DT, "Qualcomm FSM 9900 (Flattened Device Tree)")
	.map_io			= fsm9900_map_io,
	.init_machine		= fsm9900_init,
	.dt_compat		= fsm9900_dt_match,
	.reserve		= fsm9900_reserve,
	.init_very_early	= fsm9900_init_very_early,
	.restart		= msm_restart,
	.smp			= &msm8974_smp_ops,
MACHINE_END
