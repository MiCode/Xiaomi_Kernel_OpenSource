/* SPDX-License-Identifier: GPL-2.0-only
 *
 * UFS Host Controller driver for Unisoc specific extensions
 *
 * Copyright (C) 2022 Unisoc, Inc.
 *
 */

#ifndef _UFS_SPRD_PWR_DEBUG_H_
#define _UFS_SPRD_PWR_DEBUG_H_
void ufs_sprd_pwr_change_compare(struct ufs_hba *hba,
		enum ufs_notify_change_status status,
		struct ufs_pa_layer_attr *final_params,
		int *err);
#endif/* _UFS_SPRD_PWR_DEBUG_H_  */
