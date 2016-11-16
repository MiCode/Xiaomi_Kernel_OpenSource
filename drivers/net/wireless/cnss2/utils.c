/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#define CNSS_MAX_CH_NUM			45

#include <linux/module.h>
#include <linux/slab.h>

static DEFINE_MUTEX(unsafe_channel_list_lock);
static DEFINE_MUTEX(dfs_nol_info_lock);

static struct cnss_unsafe_channel_list {
	u16 unsafe_ch_count;
	u16 unsafe_ch_list[CNSS_MAX_CH_NUM];
} unsafe_channel_list;

static struct cnss_dfs_nol_info {
	void *dfs_nol_info;
	u16 dfs_nol_info_len;
} dfs_nol_info;

int cnss_set_wlan_unsafe_channel(u16 *unsafe_ch_list, u16 ch_count)
{
	mutex_lock(&unsafe_channel_list_lock);
	if ((!unsafe_ch_list) || (ch_count > CNSS_MAX_CH_NUM)) {
		mutex_unlock(&unsafe_channel_list_lock);
		return -EINVAL;
	}

	unsafe_channel_list.unsafe_ch_count = ch_count;

	if (ch_count != 0) {
		memcpy((char *)unsafe_channel_list.unsafe_ch_list,
		       (char *)unsafe_ch_list, ch_count * sizeof(u16));
	}
	mutex_unlock(&unsafe_channel_list_lock);

	return 0;
}
EXPORT_SYMBOL(cnss_set_wlan_unsafe_channel);

int cnss_get_wlan_unsafe_channel(u16 *unsafe_ch_list,
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
	memcpy((char *)unsafe_ch_list,
	       (char *)unsafe_channel_list.unsafe_ch_list,
	       unsafe_channel_list.unsafe_ch_count * sizeof(u16));
	mutex_unlock(&unsafe_channel_list_lock);

	return 0;
}
EXPORT_SYMBOL(cnss_get_wlan_unsafe_channel);

int cnss_wlan_set_dfs_nol(const void *info, u16 info_len)
{
	void *temp;
	struct cnss_dfs_nol_info *dfs_info;

	mutex_lock(&dfs_nol_info_lock);
	if (!info || !info_len) {
		mutex_unlock(&dfs_nol_info_lock);
		return -EINVAL;
	}

	temp = kmalloc(info_len, GFP_KERNEL);
	if (!temp) {
		mutex_unlock(&dfs_nol_info_lock);
		return -ENOMEM;
	}

	memcpy(temp, info, info_len);
	dfs_info = &dfs_nol_info;
	kfree(dfs_info->dfs_nol_info);

	dfs_info->dfs_nol_info = temp;
	dfs_info->dfs_nol_info_len = info_len;
	mutex_unlock(&dfs_nol_info_lock);

	return 0;
}
EXPORT_SYMBOL(cnss_wlan_set_dfs_nol);

int cnss_wlan_get_dfs_nol(void *info, u16 info_len)
{
	int len;
	struct cnss_dfs_nol_info *dfs_info;

	mutex_lock(&dfs_nol_info_lock);
	if (!info || !info_len) {
		mutex_unlock(&dfs_nol_info_lock);
		return -EINVAL;
	}

	dfs_info = &dfs_nol_info;

	if (dfs_info->dfs_nol_info == NULL || dfs_info->dfs_nol_info_len == 0) {
		mutex_unlock(&dfs_nol_info_lock);
		return -ENOENT;
	}

	len = min(info_len, dfs_info->dfs_nol_info_len);

	memcpy(info, dfs_info->dfs_nol_info, len);
	mutex_unlock(&dfs_nol_info_lock);

	return len;
}
EXPORT_SYMBOL(cnss_wlan_get_dfs_nol);
