/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef _UFS_MEDIATEK_DBG_H
#define _UFS_MEDIATEK_DBG_H

#include <linux/device.h>
#include <linux/seq_file.h>
#include <linux/types.h>

/*
 * snprintf may return a value of size or "more" to indicate
 * that the output was truncated, thus be careful of "more"
 * case.
 */
#define SPREAD_PRINTF(buff, size, evt, fmt, args...) \
do { \
	if (buff && size && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
		if (var > 0) { \
			if (var > *(size)) \
				var = *(size); \
			*(size) -= var; \
			*(buff) += var; \
		} \
	} \
	if (evt) \
		seq_printf(evt, fmt, ##args); \
	if (!buff && !evt) { \
		pr_info(fmt, ##args); \
	} \
} while (0)

enum ufsdbg_cmd_type {
	UFSDBG_CMD_LIST_DUMP    = 0,
	UFSDBG_PWR_MODE_DUMP    = 1,
	UFSDBG_HEALTH_DUMP      = 2,
	UFSDBG_CMD_LIST_ENABLE  = 3,
	UFSDBG_CMD_LIST_DISABLE = 4,
	UFSDBG_UNKNOWN
};

enum cmd_hist_event {
	CMD_SEND,
	CMD_COMPLETED,
	CMD_DEV_SEND,
	CMD_DEV_COMPLETED,
	CMD_TM_SEND,
	CMD_TM_COMPLETED,
	CMD_UIC_SEND,
	CMD_UIC_CMPL_GENERAL,
	CMD_UIC_CMPL_PWR_CTRL,
	CMD_REG_TOGGLE,
	CMD_ABORTING,
	CMD_DI_FAIL,
	CMD_DEVICE_RESET,
	CMD_PERF_MODE,
	CMD_DEBUG_PROC,
	CMD_GENERIC,
};

struct cmd_hist_struct {
	enum cmd_hist_event event;
	u8 opcode;
	u8 crypt_en;
	u8 crypt_keyslot;
	u8 cpu;
	u16 tag;
	pid_t pid;
	int transfer_len;
	u64 lba;
	u64 time;
	u64 duration;
};

int ufsdbg_register(struct device *dev);

#endif

