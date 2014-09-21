/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

/* MSM CPU Subsystem power control operations
 */

#include <linux/bitops.h>
#include <linux/cpu.h>
#include <linux/cpumask.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/smp.h>

#include <soc/qcom/cpu_pwr_ctl.h>
#include <soc/qcom/spm.h>
#include <soc/qcom/scm.h>

#include <asm/barrier.h>
#include <asm/cacheflush.h>

/* CPU power domain register offsets */
#define CPU_PWR_CTL		0x4
#define CPU_PWR_GATE_CTL	0x14
#define LDO_BHS_PWR_CTL		0x28

/* L2 power domain register offsets */
#define L2_PWR_CTL_OVERRIDE	0xc
#define L2_PWR_CTL		0x14
#define L2_PWR_STATUS		0x18
#define	L2_CORE_CBCR		0x58
#define L1_RST_DIS		0x284

#define L2_SPM_STS		0xc
#define L2_VREG_CTL		0x1c

/*
 * struct msm_l2ccc_of_info: represents of data for l2 cache clock controller.
 * @compat: compat string for l2 cache clock controller
 * @l2_pon: l2 cache power on routine
 */
struct msm_l2ccc_of_info {
	const char *compat;
	int (*l2_power_on) (struct device_node *dn, u32 l2_mask, int cpu);
	u32 l2_power_on_mask;
};


static int power_on_l2_msm8916(struct device_node *l2ccc_node, u32 pon_mask,
				int cpu)
{
	u32 pon_status;
	void __iomem *l2_base;

	l2_base = of_iomap(l2ccc_node, 0);
	if (!l2_base)
		return -ENOMEM;

	/* Skip power-on sequence if l2 cache is already powered up*/
	pon_status = (__raw_readl(l2_base + L2_PWR_STATUS) & pon_mask)
				== pon_mask;
	if (pon_status) {
		iounmap(l2_base);
		return 0;
	}

	/* Close L2/SCU Logic GDHS and power up the cache */
	writel_relaxed(0x10D700, l2_base + L2_PWR_CTL);

	/* Assert PRESETDBGn */
	writel_relaxed(0x400000, l2_base + L2_PWR_CTL_OVERRIDE);
	mb();
	udelay(2);

	/* De-assert L2/SCU memory Clamp */
	writel_relaxed(0x101700, l2_base + L2_PWR_CTL);

	/* Wakeup L2/SCU RAMs by deasserting sleep signals */
	writel_relaxed(0x101703, l2_base + L2_PWR_CTL);
	mb();
	udelay(2);

	/* Enable clocks via SW_CLK_EN */
	writel_relaxed(0x01, l2_base + L2_CORE_CBCR);

	/* De-assert L2/SCU logic clamp */
	writel_relaxed(0x101603, l2_base + L2_PWR_CTL);
	mb();
	udelay(2);

	/* De-assert PRESSETDBg */
	writel_relaxed(0x0, l2_base + L2_PWR_CTL_OVERRIDE);

	/* De-assert L2/SCU Logic reset */
	writel_relaxed(0x100203, l2_base + L2_PWR_CTL);
	mb();
	udelay(54);

	/* Turn on the PMIC_APC */
	writel_relaxed(0x10100203, l2_base + L2_PWR_CTL);

	/* Set H/W clock control for the cpu CBC block */
	writel_relaxed(0x03, l2_base + L2_CORE_CBCR);
	mb();
	iounmap(l2_base);

	return 0;
}

static int kick_l2spm_8994(struct device_node *l2ccc_node,
				struct device_node *vctl_node)
{
	struct resource res;
	int val, ret = 0;
	void __iomem *l2spm_base = of_iomap(vctl_node, 0);

	if (!l2spm_base)
		return -ENOMEM;

	if (!(__raw_readl(l2spm_base + L2_SPM_STS) & 0xFFFF0000))
		goto bail_l2_pwr_bit;

	ret = of_address_to_resource(l2ccc_node, 1, &res);
	if (ret)
		goto bail_l2_pwr_bit;

	/* L2 is executing sleep state machine,
	 * let's softly kick it awake
	 */
	val = scm_io_read((u32)res.start);
	val |= BIT(0);
	scm_io_write((u32)res.start, val);

	/* Wait until the SPM status indicates that the PWR_CTL
	 * bits are clear.
	 */
	while (readl_relaxed(l2spm_base + L2_SPM_STS) & 0xFFFF0000) {
		int timeout = 10;

		BUG_ON(!timeout--);
		cpu_relax();
		usleep(100);
	}

	val = scm_io_read((u32)res.start);
	val &= ~BIT(0);
	scm_io_write((u32)res.start, val);

bail_l2_pwr_bit:
	iounmap(l2spm_base);
	return ret;
}

static int power_on_l2_msm8994(struct device_node *l2ccc_node, u32 pon_mask,
				int cpu)
{
	u32 pon_status;
	void __iomem *l2_base;
	int ret = 0;
	uint32_t val;
	struct device_node *vctl_node;

	vctl_node = of_parse_phandle(l2ccc_node, "qcom,vctl-node", 0);

	if (!vctl_node)
		return -ENODEV;

	l2_base = of_iomap(l2ccc_node, 0);
	if (!l2_base)
		return -ENOMEM;

	pon_status = (__raw_readl(l2_base + L2_PWR_CTL) & pon_mask) == pon_mask;

	/* Check L2 SPM Status */
	if (pon_status) {
		ret = kick_l2spm_8994(l2ccc_node, vctl_node);
		iounmap(l2_base);
		return ret;
	}

	/* Need to power on the rail */
	ret = of_property_read_u32(l2ccc_node, "qcom,vctl-val", &val);
	if (ret) {
		iounmap(l2_base);
		pr_err("Unable to read L2 voltage\n");
		return -EFAULT;
	}

	ret = msm_spm_turn_on_cpu_rail(vctl_node, val, cpu, L2_VREG_CTL);
	if (ret) {
		iounmap(l2_base);
		pr_err("Error turning on power rail.\n");
		return -EFAULT;
	}

	/* Enable L1 invalidation by h/w */
	writel_relaxed(0x00000000, l2_base + L1_RST_DIS);
	mb();

	/* Assert PRESETDBGn */
	writel_relaxed(0x00400000 , l2_base + L2_PWR_CTL_OVERRIDE);
	mb();

	/* Close L2/SCU Logic GDHS and power up the cache */
	writel_relaxed(0x00029716 , l2_base + L2_PWR_CTL);
	mb();
	udelay(8);

	/* De-assert L2/SCU memory Clamp */
	writel_relaxed(0x00023716 , l2_base + L2_PWR_CTL);
	mb();

	/* Wakeup L2/SCU RAMs by deasserting sleep signals */
	writel_relaxed(0x0002371E , l2_base + L2_PWR_CTL);
	mb();
	udelay(8);

	/* Un-gate clock and wait for sequential waking up
	 * of L2 rams with a delay of 2*X0 cycles
	 */
	writel_relaxed(0x0002371C , l2_base + L2_PWR_CTL);
	mb();
	udelay(4);

	/* De-assert L2/SCU logic clamp */
	writel_relaxed(0x0002361C , l2_base + L2_PWR_CTL);
	mb();
	udelay(2);

	/* De-assert L2/SCU logic reset */
	writel_relaxed(0x00022218 , l2_base + L2_PWR_CTL);
	mb();
	udelay(4);

	/* Turn on the PMIC_APC */
	writel_relaxed(0x10022218 , l2_base + L2_PWR_CTL);
	mb();

	/* De-assert PRESETDBGn */
	writel_relaxed(0x00000000 , l2_base + L2_PWR_CTL_OVERRIDE);
	mb();
	iounmap(l2_base);

	return 0;
}

static const struct msm_l2ccc_of_info l2ccc_info[] = {
	{
		.compat = "qcom,8994-l2ccc",
		.l2_power_on = power_on_l2_msm8994,
		.l2_power_on_mask = (BIT(9) | BIT(28)),
	},
	{
		.compat = "qcom,8916-l2ccc",
		.l2_power_on = power_on_l2_msm8916,
		.l2_power_on_mask = BIT(9),
	},
};

static int power_on_l2_cache(struct device_node *l2ccc_node, int cpu)
{
	int ret, i;
	const char *compat;

	ret = of_property_read_string(l2ccc_node, "compatible", &compat);
	if (ret)
		return ret;

	for (i = 0; i < ARRAY_SIZE(l2ccc_info); i++) {
		const struct msm_l2ccc_of_info *ptr = &l2ccc_info[i];

		if (!of_compat_cmp(ptr->compat, compat, strlen(compat)))
				return ptr->l2_power_on(l2ccc_node,
						ptr->l2_power_on_mask, cpu);
	}
	pr_err("Compat string not found for L2CCC node\n");
	return -EIO;
}

int msm8994_unclamp_secondary_arm_cpu(unsigned int cpu)
{

	int ret = 0;
	struct device_node *cpu_node, *acc_node, *l2_node, *l2ccc_node;
	void __iomem *acc_reg, *ldo_bhs_reg;

	cpu_node = of_get_cpu_node(cpu, NULL);
	if (!cpu_node)
		return -ENODEV;

	acc_node = of_parse_phandle(cpu_node, "qcom,acc", 0);
	if (!acc_node) {
			ret = -ENODEV;
			goto out_acc;
	}

	l2_node = of_parse_phandle(cpu_node, "next-level-cache", 0);
	if (!l2_node) {
		ret = -ENODEV;
		goto out_l2;
	}

	l2ccc_node = of_parse_phandle(l2_node, "power-domain", 0);
	if (!l2ccc_node) {
		ret = -ENODEV;
		goto out_l2;
	}

	/*
	 * Ensure L2-cache of the CPU is powered on before
	 * unclamping cpu power rails.
	 */

	ret = power_on_l2_cache(l2ccc_node, cpu);
	if (ret) {
		pr_err("L2 cache power up failed for CPU%d\n", cpu);
		goto out_l2ccc;
	}

	ldo_bhs_reg = of_iomap(acc_node, 0);
	if (!ldo_bhs_reg) {
		ret = -ENOMEM;
		goto out_bhs_reg;
	}

	acc_reg = of_iomap(acc_node, 1);
	if (!acc_reg) {
		ret = -ENOMEM;
		goto out_acc_reg;
	}

	/* Assert head switch enable few */
	writel_relaxed(0x00000001, acc_reg + CPU_PWR_GATE_CTL);
	mb();
	udelay(1);

	/* Assert head switch enable rest */
	writel_relaxed(0x00000003, acc_reg + CPU_PWR_GATE_CTL);
	mb();
	udelay(1);

	/* De-assert coremem clamp. This is asserted by default */
	writel_relaxed(0x00000079, acc_reg + CPU_PWR_CTL);
	mb();
	udelay(2);

	/* Close coremem array gdhs */
	writel_relaxed(0x0000007D, acc_reg + CPU_PWR_CTL);
	mb();
	udelay(2);

	/* De-assert clamp */
	writel_relaxed(0x0000003D, acc_reg + CPU_PWR_CTL);
	mb();

	/* De-assert clamp */
	writel_relaxed(0x0000003C, acc_reg + CPU_PWR_CTL);
	mb();
	udelay(1);

	/* De-assert core0 reset */
	writel_relaxed(0x0000000C, acc_reg + CPU_PWR_CTL);
	mb();

	/* Assert PWRDUP */
	writel_relaxed(0x0000008C, acc_reg + CPU_PWR_CTL);
	mb();

	iounmap(acc_reg);
out_acc_reg:
	iounmap(ldo_bhs_reg);
out_bhs_reg:
	of_node_put(l2ccc_node);
out_l2ccc:
	of_node_put(l2_node);
out_l2:
	of_node_put(acc_node);
out_acc:
	of_node_put(cpu_node);

	return ret;
}

int msm_unclamp_secondary_arm_cpu(unsigned int cpu)
{

	int ret = 0;
	struct device_node *cpu_node, *acc_node, *l2_node, *l2ccc_node;
	void __iomem *reg;

	cpu_node = of_get_cpu_node(cpu, NULL);
	if (!cpu_node)
		return -ENODEV;

	acc_node = of_parse_phandle(cpu_node, "qcom,acc", 0);
	if (!acc_node) {
			ret = -ENODEV;
			goto out_acc;
	}

	l2_node = of_parse_phandle(cpu_node, "next-level-cache", 0);
	if (!l2_node) {
		ret = -ENODEV;
		goto out_l2;
	}

	l2ccc_node = of_parse_phandle(l2_node, "power-domain", 0);
	if (!l2ccc_node) {
		ret = -ENODEV;
		goto out_l2;
	}

	/* Ensure L2-cache of the CPU is powered on before
	 * unclamping cpu power rails.
	 */
	ret = power_on_l2_cache(l2ccc_node, cpu);
	if (ret) {
		pr_err("L2 cache power up failed for CPU%d\n", cpu);
		goto out_l2ccc;
	}

	reg = of_iomap(acc_node, 0);
	if (!reg) {
		ret = -ENOMEM;
		goto out_acc_reg;
	}

	/* Assert Reset on cpu-n */
	writel_relaxed(0x00000033, reg + CPU_PWR_CTL);
	mb();

	/*Program skew to 16 X0 clock cycles*/
	writel_relaxed(0x10000001, reg + CPU_PWR_GATE_CTL);
	mb();
	udelay(2);

	/* De-assert coremem clamp */
	writel_relaxed(0x00000031, reg + CPU_PWR_CTL);
	mb();

	/* Close coremem array gdhs */
	writel_relaxed(0x00000039, reg + CPU_PWR_CTL);
	mb();
	udelay(2);

	/* De-assert cpu-n clamp */
	writel_relaxed(0x00020038, reg + CPU_PWR_CTL);
	mb();
	udelay(2);

	/* De-assert cpu-n reset */
	writel_relaxed(0x00020008, reg + CPU_PWR_CTL);
	mb();

	/* Assert PWRDUP signal on core-n */
	writel_relaxed(0x00020088, reg + CPU_PWR_CTL);
	mb();

	/* Secondary CPU-N is now alive */
	iounmap(reg);
out_acc_reg:
	of_node_put(l2ccc_node);
out_l2ccc:
	of_node_put(l2_node);
out_l2:
	of_node_put(acc_node);
out_acc:
	of_node_put(cpu_node);

	return ret;
}

int msm_unclamp_secondary_arm_cpu_sim(unsigned int cpu)
{
	int ret = 0;
	struct device_node *cpu_node, *acc_node;
	void __iomem *reg;

	cpu_node = of_get_cpu_node(cpu, NULL);
	if (!cpu_node) {
		ret = -ENODEV;
		goto out_acc;
	}

	acc_node = of_parse_phandle(cpu_node, "qcom,acc", 0);
	if (!acc_node) {
		ret = -ENODEV;
		goto out_acc;
	}

	reg = of_iomap(acc_node, 0);
	if (!reg) {
		ret = -ENOMEM;
		goto out_acc;
	}

	writel_relaxed(0x800, reg + CPU_PWR_CTL);
	writel_relaxed(0x3FFF, reg + CPU_PWR_GATE_CTL);
	mb();
	iounmap(reg);

out_acc:
	of_node_put(cpu_node);

	return ret;
}
