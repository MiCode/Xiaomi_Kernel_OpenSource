/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * spreadtrum create in 2021/11/19
 *
 * ufs health report for vendor
 *
 */

#ifndef __UFS_SPRD_HEALTH_DEVICE_H__
#define __UFS_SPRD_HEALTH_DEVICE_H__

void ufs_sprd_sysfs_add_health_device_nodes(struct ufs_hba *hba);
#define UFS_VENDOR_YMTC	0xA9B

#endif
