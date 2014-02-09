/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include <linux/errno.h>
#include <linux/etherdevice.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_platform.h>
#include <linux/of_fdt.h>
#include <linux/of_irq.h>
#include <linux/msm_tsens.h>
#include <linux/msm_thermal.h>
#include <linux/clk/msm-clk-provider.h>
#include <linux/regulator/rpm-smd-regulator.h>
#include <asm/mach/arch.h>
#include <soc/qcom/socinfo.h>
#include <mach/board.h>
#include <mach/msm_memtypes.h>
#include <soc/qcom/smem.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/pm.h>
#include <soc/qcom/rpm-smd.h>
#include <mach/msm_smd.h>
#include <mach/restart.h>

#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <linux/regulator/qpnp-regulator.h>
#include <linux/regulator/krait-regulator.h>

#include "board-dt.h"
#include "clock.h"
#include "platsmp.h"

#define MPQ8092_MAC_FUSE_PHYS     0xfc4bc0e0
#define MPQ8092_MAC_FUSE_SIZE     0x10

static const char mac_addr_prop_name[] = "mac-address";

static void __init mpq8092_early_memory(void)
{
	of_scan_flat_dt(dt_scan_for_memory_hole, NULL);
}

static void __init mpq8092_dt_reserve(void)
{
	of_scan_flat_dt(dt_scan_for_memory_reserve, NULL);
}

static void __init mpq8092_map_io(void)
{
	msm_map_mpq8092_io();
}

static struct of_dev_auxdata mpq8092_auxdata_lookup[] __initdata = {
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF9824000, "msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,msm-sdcc", 0xF98A4000, "msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF9824900, "msm_sdcc.1", NULL),
	OF_DEV_AUXDATA("qcom,sdhci-msm", 0xF98A4900, "msm_sdcc.2", NULL),
	OF_DEV_AUXDATA("qcom,msm_pcie", 0xFC520000, "msm_pcie", NULL),
	{}
};

static int emac_dt_update(int cell, phys_addr_t addr, unsigned long size)
{
	static int offset[ETH_ALEN] = { 5, 4, 3, 2, 1, 0};
	void __iomem *fuse_reg;
	struct device_node *np = NULL;
	struct property *pmac = NULL;
	struct property *pp = NULL;
	u8 buf[ETH_ALEN];
	int n, retval = 0;

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

/*
 * Used to satisfy dependencies for devices that need to be
 * run early or in a particular order. Most likely your device doesn't fall
 * into this category, and thus the driver should not be added here. The
 * EPROBE_DEFER can satisfy most dependency problems.
 */
void __init mpq8092_add_drivers(void)
{
	msm_smd_init();
	msm_rpm_driver_init();
	msm_pm_sleep_status_init();
	msm_spm_device_init();
	rpm_smd_regulator_driver_init();
	qpnp_regulator_init();
	krait_power_init();
	if (of_board_is_rumi())
		msm_clock_init(&mpq8092_rumi_clock_init_data);
	else
		msm_clock_init(&mpq8092_clock_init_data);
	tsens_tm_init_driver();
	msm_thermal_device_init();

	emac_dt_update(0, MPQ8092_MAC_FUSE_PHYS, MPQ8092_MAC_FUSE_SIZE);
}


static void __init mpq8092_init(void)
{
	struct of_dev_auxdata *adata = mpq8092_auxdata_lookup;

	/*
	 * populate devices from DT first so smem probe will get called as part
	 * of msm_smem_init.  socinfo_init needs smem support so call
	 * msm_smem_init before it.
	 */
	board_dt_populate(adata);

	msm_smem_init();

	if (socinfo_init() < 0)
		pr_err("%s: socinfo_init() failed\n", __func__);

	mpq8092_init_gpiomux();
	mpq8092_add_drivers();
}

static const char *mpq8092_dt_match[] __initconst = {
	"qcom,mpq8092",
	NULL
};

DT_MACHINE_START(MSM8092_DT, "Qualcomm MSM 8092 (Flattened Device Tree)")
	.map_io			= mpq8092_map_io,
	.init_machine		= mpq8092_init,
	.dt_compat		= mpq8092_dt_match,
	.reserve		= mpq8092_dt_reserve,
	.init_very_early	= mpq8092_early_memory,
	.restart		= msm_restart,
	.smp			= &msm8974_smp_ops,
MACHINE_END
