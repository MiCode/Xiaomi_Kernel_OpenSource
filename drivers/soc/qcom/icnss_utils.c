/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/slab.h>
#include <soc/qcom/icnss.h>

#define ICNSS_MAX_CH_NUM 45

static DEFINE_MUTEX(unsafe_channel_list_lock);
static DEFINE_SPINLOCK(dfs_nol_info_lock);
static int driver_load_cnt;
static enum cnss_cc_src icnss_cc_source = CNSS_SOURCE_CORE;

static struct icnss_unsafe_channel_list {
	u16 unsafe_ch_count;
	u16 unsafe_ch_list[ICNSS_MAX_CH_NUM];
} unsafe_channel_list;

static struct icnss_dfs_nol_info {
	void *dfs_nol_info;
	u16 dfs_nol_info_len;
} dfs_nol_info;

int icnss_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count)
{
	mutex_lock(&unsafe_channel_list_lock);
	if ((!unsafe_ch_list) || (ch_count > ICNSS_MAX_CH_NUM)) {
		mutex_unlock(&unsafe_channel_list_lock);
		return -EINVAL;
	}

	unsafe_channel_list.unsafe_ch_count = ch_count;

	if (ch_count != 0) {
		memcpy(
		       (char *)unsafe_channel_list.unsafe_ch_list,
		       (char *)unsafe_ch_list, ch_count * sizeof(u16));
	}
	mutex_unlock(&unsafe_channel_list_lock);

	return 0;
}
EXPORT_SYMBOL(icnss_set_wlan_unsafe_channel);

int icnss_get_wlan_unsafe_channel(u16 *unsafe_ch_list,
				  u16 *ch_count, u16 buf_len)
{
	mutex_lock(&unsafe_channel_list_lock);
	if (!unsafe_ch_list || !ch_count) {
		mutex_unlock(&unsafe_channel_list_lock);
		return -EINVAL;
	}

	if (buf_len < (unsafe_channel_list.unsafe_ch_count * sizeof(u16))) {
		mutex_unlock(&unsafe_channel_list_lock);
		return -ENOMEM;
	}

	*ch_count = unsafe_channel_list.unsafe_ch_count;
	memcpy(
		(char *)unsafe_ch_list,
		(char *)unsafe_channel_list.unsafe_ch_list,
		unsafe_channel_list.unsafe_ch_count * sizeof(u16));
	mutex_unlock(&unsafe_channel_list_lock);

	return 0;
}
EXPORT_SYMBOL(icnss_get_wlan_unsafe_channel);

int icnss_wlan_set_dfs_nol(const void *info, u16 info_len)
{
	void *temp;
	void *old_nol_info;
	struct icnss_dfs_nol_info *dfs_info;

	if (!info || !info_len)
		return -EINVAL;

	temp = kmalloc(info_len, GFP_ATOMIC);
	if (!temp)
		return -ENOMEM;

	memcpy(temp, info, info_len);
	spin_lock_bh(&dfs_nol_info_lock);
	dfs_info = &dfs_nol_info;
	old_nol_info = dfs_info->dfs_nol_info;
	dfs_info->dfs_nol_info = temp;
	dfs_info->dfs_nol_info_len = info_len;
	spin_unlock_bh(&dfs_nol_info_lock);
	kfree(old_nol_info);

	return 0;
}
EXPORT_SYMBOL(icnss_wlan_set_dfs_nol);

int icnss_wlan_get_dfs_nol(void *info, u16 info_len)
{
	int len;
	struct icnss_dfs_nol_info *dfs_info;

	if (!info || !info_len)
		return -EINVAL;

	spin_lock_bh(&dfs_nol_info_lock);

	dfs_info = &dfs_nol_info;
	if (dfs_info->dfs_nol_info == NULL ||
	    dfs_info->dfs_nol_info_len == 0) {
		spin_unlock_bh(&dfs_nol_info_lock);
		return -ENOENT;
	}

	len = min(info_len, dfs_info->dfs_nol_info_len);
	memcpy(info, dfs_info->dfs_nol_info, len);
	spin_unlock_bh(&dfs_nol_info_lock);

	return len;
}
EXPORT_SYMBOL(icnss_wlan_get_dfs_nol);

void icnss_increment_driver_load_cnt(void)
{
	++driver_load_cnt;
}
EXPORT_SYMBOL(icnss_increment_driver_load_cnt);

int icnss_get_driver_load_cnt(void)
{
	return driver_load_cnt;
}
EXPORT_SYMBOL(icnss_get_driver_load_cnt);


void icnss_set_cc_source(enum cnss_cc_src cc_source)
{
	icnss_cc_source = cc_source;
}
EXPORT_SYMBOL(icnss_set_cc_source);

enum cnss_cc_src icnss_get_cc_source(void)
{
	return icnss_cc_source;
}
EXPORT_SYMBOL(icnss_get_cc_source);
