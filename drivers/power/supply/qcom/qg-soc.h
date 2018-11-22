/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
 */

#ifndef __QG_SOC_H__
#define __QG_SOC_H__

int qg_scale_soc(struct qpnp_qg *chip, bool force_soc);
int qg_soc_init(struct qpnp_qg *chip);
void qg_soc_exit(struct qpnp_qg *chip);
int qg_adjust_sys_soc(struct qpnp_qg *chip);

extern struct device_attribute dev_attr_soc_interval_ms;
extern struct device_attribute dev_attr_soc_cold_interval_ms;
extern struct device_attribute dev_attr_maint_soc_update_ms;

#endif /* __QG_SOC_H__ */
