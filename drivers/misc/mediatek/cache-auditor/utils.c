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
#include <linux/cgroup-defs.h>
#include <linux/cgroup.h>

#include "internal.h"

#undef pr_fmt
#define pr_fmt(fmt) "Cache-Auditor: " fmt

void __iomem *mcusys_base_addr;
const u32 rg_cpu_ic_age_offset = 0x8410;

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

static inline int get_group_id(struct task_struct *task)
{
#if IS_ENABLED(CONFIG_CGROUP_SCHEDTUNE)
	const int subsys_id = schedtune_cgrp_id;
	struct cgroup *grp;

	rcu_read_lock();
	grp = task_cgroup(task, subsys_id);
	rcu_read_unlock();
	return grp->id;
#else
	return 0;
#endif
}

static inline bool is_important(struct task_struct *task)
{
	int grp_id = get_group_id(task);

	return (grp_id == GROUP_TA);
}

static inline bool is_background(struct task_struct *task)
{
	return !is_important(task);
}
