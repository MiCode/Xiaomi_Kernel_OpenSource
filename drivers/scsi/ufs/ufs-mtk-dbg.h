/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __MT_UFS_DEUBG__
#define __MT_UFS_DEUBG__

#include "ufs-mtk.h"

struct ufs_cmd_hlist_struct {
	enum ufs_trace_event event;
	u8 opcode;
	u8 lun;
	u8 crypted;
	u8 keyslot;
	pid_t pid;
	u32 cpu;
	u32 tag;
	u32 transfer_len;
	sector_t lba;
	u64 time;
	u64 duration;
	struct request *rq;
	unsigned long long ppn;
	u32 region;
	u32 subregion;
	u32 resv;
};

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

#define SPREAD_DEV_PRINTF(buff, size, evt, dev, fmt, args...) \
do { \
	if (buff && size && *(size)) { \
		unsigned long var = snprintf(*(buff), *(size), fmt, ##args); \
		if (var > 0) { \
			*(size) -= var; \
			*(buff) += var; \
		} \
	} \
	if (evt) \
		seq_printf(evt, fmt, ##args); \
	if (!buff && !evt) { \
		dev_info(dev, fmt, ##args); \
	} \
} while (0)

enum {
	UFS_CMDS_DUMP = 0,
	UFS_GET_PWR_MODE = 1,
	UFS_DUMP_HEALTH_DESCRIPTOR = 2,
	UFS_CMD_HIST_BEGIN = 3,
	UFS_CMD_HIST_STOP = 4,
	UFS_CMD_QOS_ON = 5,
	UFS_CMD_QOS_OFF = 6,
	UFS_CMD_UNKNOWN
};

#define UFS_PRINFO_PROC_MSG(evt, fmt, args...) \
do { \
	if (evt == NULL) { \
		pr_info(fmt, ##args); \
	} \
	else { \
		seq_printf(evt, fmt, ##args); \
	} \
} while (0)

#define UFS_DEVINFO_PROC_MSG(evt, dev, fmt, args...) \
do {	\
	if (evt == NULL) { \
		dev_info(dev, fmt, ##args); \
	} \
	else { \
		seq_printf(evt, fmt, ##args); \
	} \
} while (0)


#define UFS_PRDBG_PROC_MSG(evt, fmt, args...) \
do {	\
	if (evt == NULL) { \
		pr_dbg(fmt, ##args); \
	} \
	else { \
		seq_printf(evt, fmt, ##args); \
	} \
} while (0)

#define UFS_DEVDBG_PROC_MSG(evt, dev, fmt, args...) \
do {	\
	if (evt == NULL) { \
		dev_dbg(dev, fmt, ##args); \
	} \
	else { \
		seq_printf(evt, fmt, ##args); \
	} \
} while (0)

int ufs_mtk_debug_proc_init(struct ufs_hba *hba);
void ufs_mtk_dbg_add_trace(struct ufs_hba *hba,
	enum ufs_trace_event event, u32 tag,
	u8 lun, u32 transfer_len, sector_t lba, u8 opcode,
	unsigned long long ppn, u32 region, u32 subregion, u32 resv);
void ufs_mtk_dbg_stop_trace(struct ufs_hba *hba);
void ufs_mtk_dbg_start_trace(struct ufs_hba *hba);
void ufs_mtk_dbg_hang_detect_dump(void);
void ufs_mtk_dbg_proc_dump(struct seq_file *m);
void ufs_mtk_dme_cmd_log(struct ufs_hba *hba, struct uic_command *ucmd,
	enum ufs_trace_event event);
void ufs_mtk_dbg_dump_trace(char **buff, unsigned long *size,
	u32 latest_cnt, struct seq_file *m);

#endif
