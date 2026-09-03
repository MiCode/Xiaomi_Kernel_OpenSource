/* SPDX-License-Identifier: GPL-2.0 */
//
// UNISOC APCPU POWER STAT driver
// Copyright (C) 2020 Unisoc, Inc.

#ifndef __SPRD_PDBG_MISC_H__
#define __SPRD_PDBG_MISC_H__

#include <linux/workqueue.h>

enum {
	PDBG_MISC_APSYS_PD_GET,
	PDBG_MISC_APSYS_PD_SET,
};

struct misc_data {
	struct delayed_work engpc_ws_check_work;
	bool engpc_deep_en;
	bool engpc_boot_mode;
};

int sprd_pdbg_misc_init(struct device *dev, struct proc_dir_entry *dir, struct misc_data **data);
#endif /* __SPRD_PDBG_MISC_H__ */
