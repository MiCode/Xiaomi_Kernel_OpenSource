/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_MT8195_PMU_H__
#define __MDLA_MT8195_PMU_H__

struct seq_file;
struct mdla_dev;

void mdla_mt8195_pmu_info_show(struct seq_file *s);

void mdla_mt8195_pmu_init(struct mdla_dev *mdla_info);
void mdla_mt8195_pmu_deinit(struct mdla_dev *mdla_info);

#endif /* __MDLA_MT8195_PMU_H__ */
