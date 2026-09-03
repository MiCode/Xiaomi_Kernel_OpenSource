/* SPDX-License-Identifier: GPL-2.0 */
//
// UNISOC APCPU POWER STAT driver
// Copyright (C) 2020 Unisoc, Inc.

#ifndef __SPRD_REGS_INFO_H__
#define __SPRD_REGS_INFO_H__

#define REG_INFO_PER_MAX (128)
#define REGS_LOG_BUF_MAX      (REG_INFO_PER_MAX * PDBG_INFO_MAX)

struct regs_info_data {
	pdbg_notify_cb notify_cb;
	struct mutex regs_info_mutex;
	char log_buf[REGS_LOG_BUF_MAX];
};

int sprd_regs_info_init(struct device *dev, struct proc_dir_entry *dir,
			struct regs_info_data **data);
#endif /* __SPRD_REGS_INFO_H__ */
