/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

int gsp_core_sysfs_init(struct gsp_core *core);

void gsp_core_sysfs_destroy(struct gsp_core *core);

int gsp_dev_sysfs_init(struct gsp_dev *gsp);

void gsp_dev_sysfs_destroy(struct gsp_dev *gsp);
