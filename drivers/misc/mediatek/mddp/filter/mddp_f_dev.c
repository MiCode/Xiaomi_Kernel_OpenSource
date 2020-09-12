// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/string.h>

#include "mddp_debug.h"
#include "mddp_f_dev.h"

/*------------------------------------------------------------------------*/
/* MD Direct Tethering only supports some specified network devices,      */
/* which are defined below                                                */
/*------------------------------------------------------------------------*/
static const char * const mddp_f_support_dev_names[] = {
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

static const char * const mddp_f_support_wan_dev_names[] = {
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

static const int mddp_f_support_dev_num =
	sizeof(mddp_f_support_dev_names) /
	sizeof(mddp_f_support_dev_names[0]);
static const int mddp_f_support_wan_dev_num =
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

static int mddp_f_dev_get_netif_id(char *dev_name)
{
	int i;

	for (i = 0; i < mddp_f_support_wan_dev_num; i++) {
		if (strcmp(mddp_f_support_wan_dev_names[i],
				dev_name) == 0) {
			/* Matched! */
			return MDDP_WAN_DEV_NETIF_ID_BASE + i;
		}
	}

	MDDP_F_LOG(MDDP_LL_ERR,
			"%s: Invalid dev_name[%s].\n", __func__, dev_name);
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
		if (!strcmp(mddp_f_support_dev_names[i], dev_name)) {
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
		MDDP_F_LOG(MDDP_LL_ERR,
				"%s: LAN device is full[%d].\n", __func__, i);
		WARN_ON(1);
		ret = -ENOSPC;
	}

	/* Set LAN device entry */
	strcpy(mddp_f_lan_dev[id].dev_name, dev_name);
	mddp_f_lan_dev[id].netif_id = netif_id;

	mddp_f_lan_dev[id].is_valid = true;

	mddp_f_lan_dev_cnt_g++;

	MDDP_F_LOG(MDDP_LL_NOTICE,
			"%s: Add LAN device[%s], netif_id[%x], lan_dev_id[%d], total_device_num[%d].\n",
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
		MDDP_F_LOG(MDDP_LL_ERR,
				"%s: WAN device is full[%d].\n", __func__, i);
		ret = -ENOSPC;
		WARN_ON(1);
	}

	/* Set WAN device entry */
	strcpy(mddp_f_wan_dev[id].dev_name, dev_name);
	mddp_f_wan_dev[id].netif_id = mddp_f_dev_get_netif_id(dev_name);
	mddp_f_wan_dev[id].is_valid = true;

	mddp_f_wan_dev_cnt_g++;

	MDDP_F_LOG(MDDP_LL_NOTICE,
			"%s: Add WAN device[%s], wan_dev_id[%d], total_device_num[%d].\n",
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
		if (strcmp(mddp_f_lan_dev[i].dev_name, dev_name) == 0) {
			id = i;
			break;
		}
	}

	if (i >= MDDP_MAX_LAN_DEV_NUM) {
		MDDP_F_LOG(MDDP_LL_ERR,
				"%s: Cannot find LAN device[%s].\n",
				__func__, dev_name);
		ret = -EINVAL;
		WARN_ON(1);
	}

	/* Reset LAN device entry */
	mddp_f_lan_dev[id].is_valid = false;
	memset(mddp_f_lan_dev[id].dev_name, 0, sizeof(char) * IFNAMSIZ);
	mddp_f_lan_dev[id].netif_id = -1;

	mddp_f_lan_dev_cnt_g--;
	MDDP_F_LOG(MDDP_LL_NOTICE,
			"%s: Delete LAN device[%s], lan_dev_id[%d], remaining_device_num[%d].\n",
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
		if (strcmp(mddp_f_wan_dev[i].dev_name, dev_name) == 0) {
			id = i;
			break;
		}
	}

	if (i >= MDDP_MAX_WAN_DEV_NUM) {
		MDDP_F_LOG(MDDP_LL_ERR,
				"%s: Cannot find WAN device[%s].\n",
				__func__, dev_name);
		ret = -EINVAL;
		WARN_ON(1);
	}

	/* Reset WAN device entry */
	mddp_f_wan_dev[id].is_valid = false;
	memset(mddp_f_wan_dev[id].dev_name, 0, sizeof(char) * IFNAMSIZ);
	mddp_f_wan_dev[id].netif_id = -1;

	mddp_f_wan_dev_cnt_g--;
	MDDP_F_LOG(MDDP_LL_NOTICE,
			"%s: Delete WAN device[%s], wan_dev_id[%d], remaining_device_num[%d].\n",
			__func__, dev_name, id,
			mddp_f_lan_dev_cnt_g);

	return ret;
}

