/* SPDX-License-Identifier: GPL-2.0 */
//
// UNISOC APCPU POWER STAT driver
// Copyright (C) 2020 Unisoc, Inc.

#ifndef __SPRD_WAKEUP_INFO_H__
#define __SPRD_WAKEUP_INFO_H__

#include <linux/kfifo.h>
#include <linux/sipc.h>

enum {
	SPRD_PDBG_WS_DOMAIN_ID_GIC,
	SPRD_PDBG_WS_DOMAIN_ID_GPIO,
	SPRD_PDBG_WS_DOMAIN_ID_ANA,
	SPRD_PDBG_WS_DOMAIN_ID_ANA_EIC,
	SPRD_PDBG_WS_DOMAIN_ID_AP_EIC_DBNC,
	SPRD_PDBG_WS_DOMAIN_ID_AP_EIC_LATCH,
	SPRD_PDBG_WS_DOMAIN_ID_AP_EIC_ASYNC,
	SPRD_PDBG_WS_DOMAIN_ID_AP_EIC_SYNC,
	SPRD_PDBG_WS_DOMAIN_ID_MAX,
};

#define WS_LOG_BUF_MAX (512)

struct wakeup_info {
	int virq;
	u32 wakeup_cnt;
	u8 dst;
	u8 channel;
	struct list_head list;
};

struct ws_irq_domain {
	int domain_id;
	void *priv_data;
	struct list_head list;
};

struct wakeup_info_data {
	struct list_head ws_irq_domain_list;
	struct delayed_work irq_domain_work;
	struct delayed_work ws_update_work;
	struct wakeup_source *ws_update;
	pdbg_notify_cb notify_cb;
	struct kfifo ws_fifo;
	spinlock_t kfifo_in_lock;
	rwlock_t rw_lock;
	const char *irq_domain_names[SPRD_PDBG_WS_DOMAIN_ID_MAX];
	struct list_head wakeup_info_lists[SIPC_ID_NR];
	struct mutex wakeup_info_mutex;
	ktime_t last_monotime; /* monotonic time before last suspend */
	ktime_t curr_monotime; /* monotonic time after last suspend */
	ktime_t last_stime; /* monotonic boottime offset before last suspend */
	ktime_t curr_stime; /* monotonic boottime offset after last suspend */
	struct rtc_time ws_record_start;
	char log_buf[WS_LOG_BUF_MAX];
	int irq_domain_retry_cnt;
	int last_nr_irqs;
};

int sprd_pdbg_ws_info_init(struct device *dev, struct proc_dir_entry *dir,
			   struct wakeup_info_data **data);
#endif /* __SPRD_WAKEUP_INFO_H__ */
