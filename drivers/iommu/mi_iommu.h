/*
 * Copyright (c) 2019-2020, Xiaomi Mobile Software Comp. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __MI_IOMMU_H
#define __MI_IOMMU_H

#ifdef __KERNEL__

struct rb_node *rdxtree_matched_gap(struct iova_domain *iovad, unsigned long *limit_pfn, unsigned long size, bool size_aligned);
int rdxtree_update_gap(struct iova_domain *iovad, struct rb_node *prev, struct rb_node *next, struct rb_node *new);
int rdxtree_insert_gap(struct iova_domain *iovad, struct rb_node *free);
void mi_iommu_debug_init(struct iova_domain *iovad, const char *name);

#endif	/* __KERNEL__ */
#endif	/* __MI_IOMMU_H */
