/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_V1_X_PMU_H__
#define __MDLA_V1_X_PMU_H__

struct seq_file;
struct mdla_dev;

void mdla_v1_x_pmu_info_show(struct seq_file *s);

void mdla_v1_x_pmu_init(struct mdla_dev *mdla_info);
void mdla_v1_x_pmu_deinit(struct mdla_dev *mdla_info);

#endif /* __MDLA_V1_X_PMU_H__ */
