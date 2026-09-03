/* SPDX-License-Identifier: GPL-2.0
 *
 * UNISOC APCPU POWER DEBUG driver
 *
 * Copyright (C) 2020 Unisoc, Inc.
 */

#ifndef __SPRD_PDBG_DRV_H__
#define __SPRD_PDBG_DRV_H__

#define TOP_IRQ_MAX   (10)
struct notifier_block;

#pragma pack(push, 1)
struct subsys_slp_info {
	u64 total_time;
	u64 total_slp_time;
	u64 last_enter_time;
	u64 last_exit_time;
	u64 total_slp_cnt;
	u32 cur_slp_state;
	u32 boot_cnt;
	u32 last_ws; /* last wakeup source*/
	u32 ws_cnt[TOP_IRQ_MAX];/* wakeup source count*/
	u32 reserve[40];
};
#pragma pack(pop)

enum {
	SYS_SOC,
	SYS_AP,
	SYS_PHYCP,
	SYS_PSCP,
	SYS_PUBCP,
	SYS_WTLCP,
	SYS_WCN_BTWF,
	SYS_WCN_GNSS,
	SYS_MAX
};

enum {
	PDBG_NB_SYS_WCN_BTWF_SLP_GET = SYS_WCN_BTWF,
	PDBG_NB_SYS_WCN_GNSS_SLP_GET = SYS_WCN_GNSS,
	PDBG_NB_WS_UPDATE = SYS_MAX,
};

#if IS_ENABLED(CONFIG_SPRD_POWER_DEBUG)
void sprd_pdbg_msg_print(const char *format, ...);
int sprd_pdbg_notify_register(struct notifier_block *nb);
int sprd_pdbg_notify_unregister(struct notifier_block *nb);
#else
static inline void sprd_pdbg_msg_print(const char *format, ...) { }
static inline int sprd_pdbg_notify_register(struct notifier_block *nb) { return 0; }
static inline int sprd_pdbg_notify_unregister(struct notifier_block *nb) { return 0; }
#endif//CONFIG_SPRD_POWER_DEBUG

#endif /* __SPRD_PDBG_DRV_H__ */
