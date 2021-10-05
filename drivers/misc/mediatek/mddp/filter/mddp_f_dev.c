// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#include <linux/errno.h>
#include <linux/netdevice.h>
#include <linux/string.h>

#include "mddp_debug.h"
#include "mddp_f_dev.h"

/*------------------------------------------------------------------------*/
/* MD Direct Tethering only supports some specified network devices,      */
/* which are defined below                                                */
/*------------------------------------------------------------------------*/
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

static const int mddp_f_support_wan_dev_num =
	sizeof(mddp_f_support_wan_dev_names) /
	sizeof(mddp_f_support_wan_dev_names[0]);

static int mddp_f_lan_dev_cnt_g;
static struct mddp_f_dev_netif mddp_f_lan_dev[MDDP_MAX_LAN_DEV_NUM];
static int mddp_f_wan_dev_cnt_g;
static struct mddp_f_dev_netif mddp_f_wan_dev[MDDP_MAX_WAN_DEV_NUM];

struct net_device *mddp_f_is_support_lan_dev(int ifindex)
{
	int i;
	int active_dev_cnt = mddp_f_lan_dev_cnt_g;

	for (i = 0; (i < MDDP_MAX_LAN_DEV_NUM) && (active_dev_cnt > 0); i++) {
		if (mddp_f_lan_dev[i].ifindex == ifindex) {
			/* Matched! */
			return mddp_f_lan_dev[i].netdev;
		}
	}

	return NULL;
}

struct net_device *mddp_f_is_support_wan_dev(int ifindex)
{
	int i;
	int active_dev_cnt = mddp_f_wan_dev_cnt_g;

	for (i = 0; (i < MDDP_MAX_WAN_DEV_NUM) && (active_dev_cnt > 0); i++) {
		active_dev_cnt--;

		if (mddp_f_wan_dev[i].ifindex == ifindex) {
			/* Matched! */
			return mddp_f_wan_dev[i].netdev;
		}
	}

	return NULL;
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

	return -1;
}

int mddp_f_dev_to_netif_id(struct net_device *netdev)
{
	int i;
	int active_dev_cnt;

	active_dev_cnt = mddp_f_lan_dev_cnt_g;
	for (i = 0; (i < MDDP_MAX_LAN_DEV_NUM) && (active_dev_cnt > 0); i++) {
		if (mddp_f_lan_dev[i].netdev == netdev) {
			/* Matched! */
			return mddp_f_lan_dev[i].netif_id;
		}
	}

	active_dev_cnt = mddp_f_wan_dev_cnt_g;
	for (i = 0; (i < MDDP_MAX_WAN_DEV_NUM) && (active_dev_cnt > 0); i++) {
		if (mddp_f_wan_dev[i].netdev == netdev) {
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
	struct net_device *dev;
	int i;

	/* Find unused id */
	for (i = 0; i < MDDP_MAX_LAN_DEV_NUM; i++) {
		if (mddp_f_lan_dev[i].is_valid == false)
			break;
	}

	if (i >= MDDP_MAX_LAN_DEV_NUM) {
		MDDP_F_LOG(MDDP_LL_ERR,
				"%s: LAN device is full[%d].\n", __func__, i);
		return false;
	}

	dev = dev_get_by_name(&init_net, dev_name);
	if (unlikely(!dev))
		return false;

	/* Set LAN device entry */
	strlcpy(mddp_f_lan_dev[i].dev_name, dev_name, IFNAMSIZ);
	mddp_f_lan_dev[i].netdev = dev;
	mddp_f_lan_dev[i].ifindex = dev->ifindex;
	mddp_f_lan_dev[i].netif_id = netif_id;
	mddp_f_lan_dev[i].is_valid = true;

	mddp_f_lan_dev_cnt_g++;

	MDDP_F_LOG(MDDP_LL_NOTICE,
			"%s: Add LAN device[%s], netif_id[%x], lan_dev_id[%d], total_device_num[%d].\n",
			__func__, dev_name, netif_id,
			i, mddp_f_lan_dev_cnt_g);

	return true;
}

bool mddp_f_dev_add_wan_dev(char *dev_name)
{
	struct net_device *dev;
	int i;

	/* Find unused id */
	for (i = 0; i < MDDP_MAX_WAN_DEV_NUM; i++) {
		if (mddp_f_wan_dev[i].is_valid == false)
			break;
	}

	if (i >= MDDP_MAX_WAN_DEV_NUM) {
		MDDP_F_LOG(MDDP_LL_ERR,
				"%s: WAN device is full[%d].\n", __func__, i);
		return false;
	}

	dev = dev_get_by_name(&init_net, dev_name);
	if (unlikely(!dev))
		return false;
	mddp_f_wan_netdev_set(dev);

	/* Set WAN device entry */
	strlcpy(mddp_f_wan_dev[i].dev_name, dev_name, IFNAMSIZ);
	mddp_f_wan_dev[i].netdev = dev;
	mddp_f_wan_dev[i].ifindex = dev->ifindex;
	mddp_f_wan_dev[i].netif_id = mddp_f_dev_get_netif_id(dev_name);
	if (mddp_f_wan_dev[i].netif_id < 0) {
		dev_put(dev);
		return false;
	}
	mddp_f_wan_dev[i].is_valid = true;

	mddp_f_wan_dev_cnt_g++;

	MDDP_F_LOG(MDDP_LL_NOTICE,
			"%s: Add WAN device[%s], wan_dev_id[%d], total_device_num[%d].\n",
			__func__, dev_name, i,
			mddp_f_wan_dev_cnt_g);

	return true;
}

void mddp_f_dev_del_lan_dev(char *dev_name)
{
	int i;
	int active_dev_cnt = mddp_f_lan_dev_cnt_g;

	for (i = 0; (i < MDDP_MAX_LAN_DEV_NUM) && (active_dev_cnt > 0); i++) {
		if (strcmp(mddp_f_lan_dev[i].dev_name, dev_name) == 0)
			break;
	}

	if (i >= MDDP_MAX_LAN_DEV_NUM) {
		MDDP_F_LOG(MDDP_LL_ERR,
				"%s: Cannot find LAN device[%s].\n",
				__func__, dev_name);
		return;
	}

	/* Reset LAN device entry */
	mddp_f_lan_dev[i].is_valid = false;
	memset(mddp_f_lan_dev[i].dev_name, 0, sizeof(char) * IFNAMSIZ);
	mddp_f_lan_dev[i].netif_id = -1;
	dev_put(mddp_f_lan_dev[i].netdev);

	mddp_f_lan_dev_cnt_g--;
	MDDP_F_LOG(MDDP_LL_NOTICE,
			"%s: Delete LAN device[%s], lan_dev_id[%d], remaining_device_num[%d].\n",
			__func__, dev_name, i,
			mddp_f_lan_dev_cnt_g);
}

void mddp_f_dev_del_wan_dev(char *dev_name)
{
	int i;
	int active_dev_cnt = mddp_f_wan_dev_cnt_g;

	for (i = 0; (i < MDDP_MAX_WAN_DEV_NUM) && (active_dev_cnt > 0); i++) {
		if (strcmp(mddp_f_wan_dev[i].dev_name, dev_name) == 0)
			break;
	}

	if (i >= MDDP_MAX_WAN_DEV_NUM) {
		MDDP_F_LOG(MDDP_LL_ERR,
				"%s: Cannot find WAN device[%s].\n",
				__func__, dev_name);
		return;
	}

	/* Reset WAN device entry */
	mddp_f_wan_dev[i].is_valid = false;
	memset(mddp_f_wan_dev[i].dev_name, 0, sizeof(char) * IFNAMSIZ);
	mddp_f_wan_dev[i].netif_id = -1;
	dev_put(mddp_f_wan_dev[i].netdev);

	mddp_f_wan_dev_cnt_g--;
	MDDP_F_LOG(MDDP_LL_NOTICE,
			"%s: Delete WAN device[%s], wan_dev_id[%d], remaining_device_num[%d].\n",
			__func__, dev_name, i,
			mddp_f_wan_dev_cnt_g);
}
