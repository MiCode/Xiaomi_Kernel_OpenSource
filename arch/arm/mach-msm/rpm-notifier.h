/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __ARCH_ARM_MACH_MSM_RPM_NOTIF_H
#define __ARCH_ARM_MACH_MSM_RPM_NOTIF_H

struct msm_rpm_notifier_data {
	uint32_t rsc_type;
	uint32_t rsc_id;
	uint32_t key;
	uint32_t size;
	uint8_t *value;
};

int msm_rpm_register_notifier(struct notifier_block *nb);
int msm_rpm_unregister_notifier(struct notifier_block *nb);

#endif /*__ARCH_ARM_MACH_MSM_RPM_NOTIF_H */
