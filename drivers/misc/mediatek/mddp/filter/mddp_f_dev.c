/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/version.h>
#include <linux/sysctl.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/ioctl.h>
#include "mddp_f_tuple.h"
#include "mddp_f_dev.h"
#include "mddp_track.h"

/*------------------------------------------------------------------------*/
/* MD Direct Tethering only supports some specified network devices,      */
/* which are defined below                                                */
/*------------------------------------------------------------------------*/
const char *mddp_f_support_dev_names[] = {
	"ccmni-lan",
	"ccmni0",
	"ccmni1",
	"ccmni2",
	"ccmni3",
	"ccmni4",
	"ccmni5",
	"ccmni6",
	"ccmni7",
	"ccmni8",
	"ccmni9",
	"ccmni10",
	"ccmni11",
	"ccmni12",
	"ccmni13",
	"ccmni14",
	"ccmni15",
	"ccmni16",
	"ccmni17",
	"ccmni18",
	"ccmni19",
	"ccmni20",
	"ccmni21",
	"ccmni22",
	"ccmni23",
	"ccmni24",
	"ccmni25",
	"ccmni26",
	"ccmni27",
	"ccmni28",
	"ccmni29",
	"ccmni30",
	"ccmni31",
	"rndis0"
};

const char *mddp_f_support_wan_dev_names[] = {
	"ccmni0",
	"ccmni1",
	"ccmni2",
	"ccmni3",
	"ccmni4",
	"ccmni5",
	"ccmni6",
	"ccmni7",
	"ccmni8",
	"ccmni9",
	"ccmni10",
	"ccmni11",
	"ccmni12",
	"ccmni13",
	"ccmni14",
	"ccmni15",
	"ccmni16",
	"ccmni17",
	"ccmni18",
	"ccmni19",
	"ccmni20",
	"ccmni21",
	"ccmni22",
	"ccmni23",
	"ccmni24",
	"ccmni25",
	"ccmni26",
	"ccmni27",
	"ccmni28",
	"ccmni29",
	"ccmni30",
	"ccmni31"
};

const int mddp_f_support_dev_num =
	sizeof(mddp_f_support_dev_names) /
	sizeof(mddp_f_support_dev_names[0]);
const int mddp_f_support_wan_dev_num =
	sizeof(mddp_f_support_wan_dev_names) /
	sizeof(mddp_f_support_wan_dev_names[0]);

static int mddp_f_lan_dev_cnt_g;
static struct mddp_f_dev_netif mddp_f_lan_dev[MDDP_MAX_LAN_DEV_NUM];
static int mddp_f_wan_dev_cnt_g;
static struct mddp_f_dev_netif mddp_f_wan_dev[MDDP_MAX_WAN_DEV_NUM];

bool mddp_f_is_support_lan_dev(char *dev_name)
{
	int i;
	int active_dev_cnt = mddp_f_lan_dev_cnt_g;

	for (i = 0; (i < MDDP_MAX_LAN_DEV_NUM) && (active_dev_cnt > 0); i++) {
		if (strcmp(mddp_f_lan_dev[i].dev_name, dev_name) == 0) {
			/* Matched! */
			return true;
		}
	}

	return false;
}

bool mddp_f_is_support_wan_dev(char *dev_name)
{
	int i;
	int active_dev_cnt = mddp_f_wan_dev_cnt_g;

	for (i = 0; (i < MDDP_MAX_WAN_DEV_NUM) && (active_dev_cnt > 0); i++) {
		active_dev_cnt--;

		if (strcmp(mddp_f_wan_dev[i].dev_name, dev_name) == 0) {
			/* Matched! */
			return true;
		}
	}

	return false;
}

int mddp_f_dev_get_netif_id(char *dev_name)
{
	int i;

	if (strcmp(dev_name, MDDP_USB_BRIDGE_IF_NAME) == 0)
		return MDDP_USB_NETIF_ID;

	for (i = 0; i < mddp_f_support_wan_dev_num; i++) {
		if (strcmp(mddp_f_support_wan_dev_names[i],
				dev_name) == 0) {
			/* Matched! */
			return MDDP_WAN_DEV_NETIF_ID_BASE + i;
		}
	}

	pr_notice("%s: Invalid dev_name[%s].\n", __func__, dev_name);
	WARN_ON(1);

	return -1;
}

bool mddp_f_is_support_dev(char *dev_name)
{
	bool ret1;
	bool ret2;

	ret1 = mddp_f_is_support_lan_dev(dev_name);
	ret2 = mddp_f_is_support_wan_dev(dev_name);

	return ((true == ret1) || (true == ret2));
}

int mddp_f_dev_name_to_id(char *dev_name)
{
	int i;

	for (i = 0; i < mddp_f_support_dev_num; i++) {
		if (strcmp(mddp_f_support_dev_names[i], dev_name) == 0) {
			/* Matched! */
			return i;
		}
	}

	return -1;
}

int mddp_f_dev_name_to_netif_id(char *dev_name)
{
	int i;
	int active_dev_cnt;

	active_dev_cnt = mddp_f_lan_dev_cnt_g;
	for (i = 0; (i < MDDP_MAX_LAN_DEV_NUM) && (active_dev_cnt > 0); i++) {
		if (strcmp(mddp_f_lan_dev[i].dev_name, dev_name) == 0) {
			/* Matched! */
			return mddp_f_lan_dev[i].netif_id;
		}
	}

	active_dev_cnt = mddp_f_wan_dev_cnt_g;
	for (i = 0; (i < MDDP_MAX_WAN_DEV_NUM) && (active_dev_cnt > 0); i++) {
		if (strcmp(mddp_f_wan_dev[i].dev_name, dev_name) == 0) {
			/* Matched! */
			return mddp_f_wan_dev[i].netif_id;
		}
	}

	return -1;
}

const char *mddp_f_data_usage_id_to_dev_name(int id)
{
	if (id < 0 || id >= mddp_f_support_wan_dev_num) {
		pr_notice("%s: Invalid ID[%d].\n", __func__, id);
		WARN_ON(1);
		return NULL;
	}

	return mddp_f_support_wan_dev_names[id];
}

int mddp_f_data_usage_wan_dev_name_to_id(char *dev_name)
{
	int i;

	for (i = 0; i < mddp_f_support_wan_dev_num; i++) {
		if (strcmp(mddp_f_support_wan_dev_names[i], dev_name) == 0) {
			/* Matched! */
			return i;
		}
	}

	return -1;
}

bool mddp_f_dev_add_lan_dev(char *dev_name, int netif_id)
{
	int i;
	int id = 0;
	int ret = 0;

	/* Find unused id */
	for (i = 0; i < MDDP_MAX_LAN_DEV_NUM; i++) {
		if (mddp_f_lan_dev[i].is_valid == false) {
			id = i;
			break;
		}
	}

	if (i >= MDDP_MAX_LAN_DEV_NUM) {
		pr_notice("%s: LAN device is full[%d].\n", __func__, i);
		WARN_ON(1);
		ret = -ENOSPC;
	}

	/* Set LAN device entry */
	if (strcmp(dev_name, "mdbr0") == 0) {
		strcpy(mddp_f_lan_dev[id].dev_name, MDDP_USB_BRIDGE_IF_NAME);
		mddp_f_lan_dev[id].netif_id =
			mddp_f_dev_get_netif_id(MDDP_USB_BRIDGE_IF_NAME);
	} else {
		strcpy(mddp_f_lan_dev[id].dev_name, dev_name);
		mddp_f_lan_dev[id].netif_id = netif_id;
	}

	mddp_f_lan_dev[id].is_valid = true;

	mddp_f_lan_dev_cnt_g++;

	pr_notice("%s: Add LAN device[%s], netif_id[%x], lan_dev_id[%d], total_device_num[%d].\n",
			__func__, dev_name, netif_id,
			id, mddp_f_lan_dev_cnt_g);

	return ret;
}

bool mddp_f_dev_add_wan_dev(char *dev_name)
{
	int i;
	int id = 0;
	int ret = 0;

	/* Find unused id */
	for (i = 0; i < MDDP_MAX_WAN_DEV_NUM; i++) {
		if (mddp_f_wan_dev[i].is_valid == false) {
			id = i;
			break;
		}
	}

	if (i >= MDDP_MAX_WAN_DEV_NUM) {
		pr_notice("%s: WAN device is full[%d].\n", __func__, i);
		ret = -ENOSPC;
		WARN_ON(1);
	}

	/* Set WAN device entry */
	strcpy(mddp_f_wan_dev[id].dev_name, dev_name);
	mddp_f_wan_dev[id].netif_id = mddp_f_dev_get_netif_id(dev_name);
	mddp_f_wan_dev[id].is_valid = true;

	mddp_f_wan_dev_cnt_g++;

	pr_notice("%s: Add WAN device[%s], wan_dev_id[%d], total_device_num[%d].\n",
			__func__, dev_name, id,
			mddp_f_wan_dev_cnt_g);

	return ret;
}

bool mddp_f_dev_del_lan_dev(char *dev_name)
{
	int i;
	int id = 0;
	int ret = 0;
	int active_dev_cnt = mddp_f_lan_dev_cnt_g;

	for (i = 0; (i < MDDP_MAX_LAN_DEV_NUM) && (active_dev_cnt > 0); i++) {
		if (strcmp(mddp_f_lan_dev[i].dev_name, dev_name) == 0)
			id = i;
	}

	if (i >= MDDP_MAX_LAN_DEV_NUM) {
		pr_notice("%s: Cannot find LAN device[%s].\n",
				__func__, dev_name);
		ret = -EINVAL;
		WARN_ON(1);
	}

	/* Reset LAN device entry */
	mddp_f_lan_dev[id].is_valid = false;
	memset(mddp_f_lan_dev[id].dev_name, 0, sizeof(char) * IFNAMSIZ);
	mddp_f_lan_dev[id].netif_id = -1;

	mddp_f_lan_dev_cnt_g--;
	pr_notice("%s: Delete LAN device[%s], lan_dev_id[%d], remaining_device_num[%d].\n",
			__func__, dev_name, id,
			mddp_f_lan_dev_cnt_g);

	return ret;
}
bool mddp_f_dev_del_wan_dev(char *dev_name)
{
	int i;
	int id = 0;
	int ret = 0;
	int active_dev_cnt = mddp_f_wan_dev_cnt_g;

	for (i = 0; (i < MDDP_MAX_WAN_DEV_NUM) && (active_dev_cnt > 0); i++) {
		if (strcmp(mddp_f_wan_dev[i].dev_name, dev_name) == 0)
			id = i;
	}

	if (i >= MDDP_MAX_WAN_DEV_NUM) {
		pr_notice("%s: Cannot find WAN device[%s].\n",
				__func__, dev_name);
		ret = -EINVAL;
		WARN_ON(1);
	}

	/* Reset WAN device entry */
	mddp_f_wan_dev[id].is_valid = false;
	memset(mddp_f_wan_dev[id].dev_name, 0, sizeof(char) * IFNAMSIZ);
	mddp_f_wan_dev[id].netif_id = -1;

	mddp_f_wan_dev_cnt_g--;
	pr_notice("%s: Delete WAN device[%s], wan_dev_id[%d], remaining_device_num[%d].\n",
			__func__, dev_name, id,
			mddp_f_lan_dev_cnt_g);

	return ret;
}

