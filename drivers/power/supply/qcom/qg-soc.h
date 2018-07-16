/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
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

#ifndef __QG_SOC_H__
#define __QG_SOC_H__

int qg_scale_soc(struct qpnp_qg *chip, bool force_soc);
int qg_soc_init(struct qpnp_qg *chip);
void qg_soc_exit(struct qpnp_qg *chip);
int qg_adjust_sys_soc(struct qpnp_qg *chip);

#endif /* __QG_SOC_H__ */
