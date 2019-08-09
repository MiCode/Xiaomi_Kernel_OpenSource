/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/of_address.h>

#undef pr_fmt
#define pr_fmt(fmt) "Cache-Auditor: " fmt

void __iomem *mcusys_base_addr;
const u32 rg_cpu_ic_age_offset = 0x8410;

unsigned int ctl_background_prio = 2;

// Do not export tuning interface due to 2 is the best experiment result.
// module_param(ctl_background_prio, uint, 0660);

int __init init_cache_priority(void)
{
	struct device_node *node;

	pr_info("Init cache priority control");
	node = of_find_compatible_node(NULL, NULL, "mediatek,mcucfg");
	if (!node) {
		pr_notice("[MCUSYS] find mediatek,mcucfg node failed\n");
		return -EINVAL;
	}

	mcusys_base_addr = of_iomap(node, 0);
	if (IS_ERR(mcusys_base_addr))
		return -EINVAL;

	return 0;
}

static inline void write_reg(u32 val, u32 offset)
{
	writel(val, (mcusys_base_addr + rg_cpu_ic_age_offset + offset));
}

static inline u32 read_reg(u32 offset)
{
	return readl((mcusys_base_addr + offset));
}

static inline void set_cpu_init_aging(int cpu, int priority)
{
#define CPU_REG_WIDTH (0x4)
	write_reg(priority, cpu * CPU_REG_WIDTH);
}

void release_cache_control(int cpu)
{
	set_cpu_init_aging(cpu, 3);
}

void apply_cache_control(int cpu)
{
	set_cpu_init_aging(cpu, ctl_background_prio);
}

void config_partition(int gid)
{
	/*
	 * group id:
	 * group 0: 0xf, full cache way
	 * group 1: 0x1, 0b0001 cache way
	 */
	asm volatile(
			 // per-cpu: CLUSTERTHREADSID_EL1
			"msr	s3_0_c15_c4_0, %0\n"
			:: "r" (gid)
	);
}
