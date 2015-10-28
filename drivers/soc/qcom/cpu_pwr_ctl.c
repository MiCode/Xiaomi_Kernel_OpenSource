/* Copyright (c) 2014-2015, The Linux Foundation. All rights reserved.
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
#include <asm/smp_plat.h>

/* CPU power domain register offsets */
#define CPU_PWR_CTL			0x4
#define CPU_PWR_GATE_CTL		0x14

/* L2 power domain register offsets */
#define L2_PWR_CTL_OVERRIDE		0xc
#define L2_PWR_CTL			0x14
#define L2_PWR_STATUS			0x18
#define L2_CORE_CBCR			0x58

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


static int power_on_l2_msmtitanium(struct device_node *l2ccc_node, u32 pon_mask,
				int cpu)
{
	u32 pon_status;
	void __iomem *l2_base;

	l2_base = of_iomap(l2ccc_node, 0);
	if (!l2_base)
		return -ENOMEM;

	/* Skip power-on sequence if l2 cache is already powered up */
	pon_status = (__raw_readl(l2_base + L2_PWR_STATUS) & pon_mask)
				== pon_mask;
	if (pon_status) {
		iounmap(l2_base);
		return 0;
	}

	/* Close Few of the head-switches for L2SCU logic */
	writel_relaxed(0x10F700, l2_base + L2_PWR_CTL);
	mb();
	udelay(2);

	/* Close Rest of the head-switches for L2SCU logic */
	writel_relaxed(0x410F700, l2_base + L2_PWR_CTL);
	mb();
	udelay(2);

	/* Assert PRESETDBG */
	writel_relaxed(0x400000, l2_base + L2_PWR_CTL_OVERRIDE);
	mb();
	udelay(2);

	/* De-assert L2/SCU memory Clamp */
	writel_relaxed(0x4103700, l2_base + L2_PWR_CTL);
	mb();
	/* Assert L2 memory slp_nret_n */
	writel_relaxed(0x4103703, l2_base + L2_PWR_CTL);
	mb();
	udelay(4);
	/* Assert L2 memory slp_ret_n */
	writel_relaxed(0x4101703, l2_base + L2_PWR_CTL);
	mb();
	udelay(4);

	/* Assert L2 memory wl_en_clk */
	writel_relaxed(0x4101783, l2_base + L2_PWR_CTL);
	mb();
	udelay(1);
	/* De-assert L2 memory wl_en_clk */
	writel_relaxed(0x4101703, l2_base + L2_PWR_CTL);
	mb();


	/* Enable clocks via SW_CLK_EN */
	writel_relaxed(0x01, l2_base + L2_CORE_CBCR);
	mb();

	/* De-assert L2/SCU logic clamp */
	writel_relaxed(0x4101603, l2_base + L2_PWR_CTL);
	mb();
	udelay(2);

	/* De-assert PRESETDBG */
	writel_relaxed(0x0, l2_base + L2_PWR_CTL_OVERRIDE);
	mb();

	/* De-assert L2/SCU Logic reset */
	writel_relaxed(0x4100203, l2_base + L2_PWR_CTL);
	mb();
	udelay(54);

	/* Turn on the PMIC_APC */
	writel_relaxed(0x14100203, l2_base + L2_PWR_CTL);
	mb();

	/* Set H/W clock control for the cluster CBC block */
	writel_relaxed(0x03, l2_base + L2_CORE_CBCR);
	mb();
	iounmap(l2_base);

	return 0;
}

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

static int power_on_l2_msm8937(struct device_node *l2ccc_node, u32 pon_mask,
				int cpu)
{
	u32 pon_status;
	void __iomem *l2_base;

	l2_base = of_iomap(l2ccc_node, 0);
	if (!l2_base)
		return -ENOMEM;

	/* Skip power-on sequence if l2 cache is already powered up */
	pon_status = (__raw_readl(l2_base + L2_PWR_STATUS) & pon_mask)
				== pon_mask;
	if (pon_status) {
		iounmap(l2_base);
		return 0;
	}

	/* Close GDHS of L2SCU logic */
	writel_relaxed(0x0010D700, l2_base + L2_PWR_CTL);
	mb();

	/* Assert PRESETDBG */
	writel_relaxed(0x00400000, l2_base + L2_PWR_CTL_OVERRIDE);
	mb();
	udelay(2);

	/* De-assert L2/SCU memory clamp */
	writel_relaxed(0x00101700, l2_base + L2_PWR_CTL);
	mb();

	/* Wakeup L2/SCU RAMs - deassert slp signals */
	writel_relaxed(0x00101703, l2_base + L2_PWR_CTL);
	mb();
	udelay(2);

	/* Enable clocks using SW CLK EN */
	writel_relaxed(0x00000001, l2_base + L2_CORE_CBCR);
	mb();

	/* De-assert L2/SCU logic Clamp */
	writel_relaxed(0x00101603, l2_base + L2_PWR_CTL);
	mb();
	udelay(2);

	/* De-assert PRESETDBG */
	writel_relaxed(0x00000000, l2_base + L2_PWR_CTL_OVERRIDE);
	mb();

	/* De-assert L2/SCU logic reset */
	writel_relaxed(0x00100203, l2_base + L2_PWR_CTL);
	mb();
	udelay(54);

	/* Set PMIC_APC_ON */
	writel_relaxed(0x10100203, l2_base + L2_PWR_CTL);
	mb();

	/* Set H/W clock control for the cluster CBC block */
	writel_relaxed(0x00000003, l2_base + L2_CORE_CBCR);
	mb();

	iounmap(l2_base);

	return 0;
}

static const struct msm_l2ccc_of_info l2ccc_info[] = {
	{
		.compat = "qcom,8916-l2ccc",
		.l2_power_on = power_on_l2_msm8916,
		.l2_power_on_mask = BIT(9),
	},
	{
		.compat = "qcom,titanium-l2ccc",
		.l2_power_on = power_on_l2_msmtitanium,
		.l2_power_on_mask = BIT(9) | BIT(28),
	},
	{
		.compat = "qcom,8937-l2ccc",
		.l2_power_on = power_on_l2_msm8937,
		.l2_power_on_mask = BIT(9) | BIT(28),
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

static inline void msmtitanium_unclamp_cpu(void __iomem *reg)
{
	/* Deassert CPU in sleep state */
	writel_relaxed(0x00000033, reg + CPU_PWR_CTL);
	mb();

	/* Program skew between en_few and en_rest to 16 XO clk cycles,
	close Core logic head switch*/
	writel_relaxed(0x10000001, reg + CPU_PWR_GATE_CTL);
	mb();
	udelay(2);

	/* De-assert coremem clamp */
	writel_relaxed(0x00000031, reg + CPU_PWR_CTL);
	mb();

	/* De-assert Core memory slp_nret_n */
	writel_relaxed(0x00000039, reg + CPU_PWR_CTL);
	mb();
	udelay(2);

	/* De-assert Core memory slp_ret_n */
	writel_relaxed(0x00000239, reg + CPU_PWR_CTL);
	mb();
	udelay(2);

	/* Assert WL_EN_CLK */
	writel_relaxed(0x00004239, reg + CPU_PWR_CTL);
	mb();
	udelay(2);

	/* De-assert WL_EN_CLK */
	writel_relaxed(0x00000239, reg + CPU_PWR_CTL);
	mb();

	/* Deassert Clamp */
	writel_relaxed(0x00000238, reg + CPU_PWR_CTL);
	mb();
	udelay(2);

	/* Deassert Core-n reset */
	writel_relaxed(0x00000208, reg + CPU_PWR_CTL);
	mb();

	/* Assert PWRDUP; */
	writel_relaxed(0x00000288, reg + CPU_PWR_CTL);
	mb();
}

int msmtitanium_unclamp_secondary_arm_cpu(unsigned int cpu)
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
		goto out_l2ccc;
	}

	/*
	* Ensure L2-cache of the CPU is powered on before
	* unclamping cpu power rails.
	*/
	ret = power_on_l2_cache(l2ccc_node, cpu);
	if (ret) {
		pr_err("L2 cache power up failed for CPU%d\n", cpu);
		goto out_acc_reg;
	}

	reg = of_iomap(acc_node, 0);
	if (!reg) {
		ret = -ENOMEM;
		goto out_acc_reg;
	}

	msmtitanium_unclamp_cpu(reg);

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

static inline void msm8937_unclamp_cpu(void __iomem *reg)
{
	/* Assert reset */
	writel_relaxed(0x00000033, reg + CPU_PWR_CTL);
	mb();

	/* Program skew between en_few and en_rest to 16 XO clk cycles,
	close Core logic head switch*/
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

	/* De-assert clamp */
	writel_relaxed(0x00020038, reg + CPU_PWR_CTL);
	mb();
	udelay(2);

	/* De-assert core-n reset */
	writel_relaxed(0x00020008, reg + CPU_PWR_CTL);
	mb();

	/* Assert PWRDUP; */
	writel_relaxed(0x00020088, reg + CPU_PWR_CTL);
	mb();
}


int msm8937_unclamp_secondary_arm_cpu(unsigned int cpu)
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
		goto out_l2ccc;
	}

	/*
	* Ensure L2-cache of the CPU is powered on before
	* unclamping cpu power rails.
	*/
	ret = power_on_l2_cache(l2ccc_node, cpu);
	if (ret) {
		pr_err("L2 cache power up failed for CPU%d\n", cpu);
		goto out_acc_reg;
	}

	reg = of_iomap(acc_node, 0);
	if (!reg) {
		ret = -ENOMEM;
		goto out_acc_reg;
	}

	msm8937_unclamp_cpu(reg);

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
