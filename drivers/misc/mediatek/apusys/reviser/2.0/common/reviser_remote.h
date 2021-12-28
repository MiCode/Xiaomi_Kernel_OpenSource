/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */


#ifndef __APUSYS_REVISER_REMOTE_H__
#define __APUSYS_REVISER_REMOTE_H__


#define REVISER_REMOTE_TIMEOUT 30000

struct reviser_remote_info {
	bool init;
};
struct reviser_remote_lock {
	struct mutex mutex_cmd;
	struct mutex mutex_ipi;
	struct mutex mutex_mgr;
	spinlock_t lock_rx;
	wait_queue_head_t wait_rx;
};

struct reviser_msg_mgr {
	struct reviser_remote_lock lock;
	struct reviser_remote_info info;
	struct list_head list_rx;
	uint32_t count;
	uint32_t send_sn;
};

bool reviser_is_remote(void);
int reviser_remote_init(void);
void reviser_remote_exit(void);
int reviser_remote_send_cmd_sync(void *drvinfo, void *request, void *reply, uint32_t timeout);
int reviser_remote_rx_cb(void *data, int len);
int reviser_remote_sync_sn(void *drvinfo, uint32_t sn);
#endif
