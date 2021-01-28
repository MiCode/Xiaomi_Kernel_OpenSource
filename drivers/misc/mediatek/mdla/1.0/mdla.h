/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __MDLA_H__
#define __MDLA_H__

#include <linux/types.h>
#include <linux/seq_file.h>
#include <mdla_ioctl.h>

#define MTK_MDLA_CORE 1

unsigned int mdla_cfg_read(u32 offset);
unsigned int mdla_reg_read(u32 offset);
void mdla_reg_write(u32 value, u32 offset);

#define mdla_reg_set(mask, offset) \
	mdla_reg_write(mdla_reg_read(offset) | (mask), (offset))

#define mdla_reg_clear(mask, offset) \
	mdla_reg_write(mdla_reg_read(offset) & ~(mask), (offset))

u32 mdla_max_cmd_id(void);

extern void *apu_mdla_gsm_top;
extern void *apu_mdla_gsm_base;
extern void *apu_mdla_biu_top;

extern u32 mdla_timeout;
extern u32 mdla_poweroff_time;
extern u32 mdla_e1_detect_timeout;
extern u32 mdla_e1_detect_count;

enum REASON_ENUM {
	REASON_OTHERS = 0,
	REASON_DRVINIT = 1,
	REASON_TIMEOUT = 2,
	REASON_POWERON = 3,
	REASON_MAX
};

void mdla_reset_lock(int ret);

enum command_entry_state {
	CE_NONE = 0,
	CE_QUEUE = 1,
	CE_RUN = 2,
	CE_DONE = 3,
	CE_FIN = 4,
};

enum command_entry_flags {
	CE_NOP = 0x00,
	CE_SYNC = 0x01,
	CE_POLLING = 0x02,
};

struct wait_entry {
	u32 async_id;
	struct list_head list;
	struct ioctl_wait_cmd wt;
};

struct command_entry {
	struct list_head list;
	int flags;
	int state;
	int sync;

	void *kva;    /* Virtual Address for Kernel */
	u32 mva;      /* Physical Address for Device */
	u32 count;
	u32 id;
	u64 khandle;  /* ion kernel handle */
	u8 type;      /* allocate memory type */

	u8 priority;     /* dvfs priority */
	u8 boost_value;  /* dvfs boost value */
	uint32_t bandwidth;

	int result;
	u64 receive_t;   /* time of receive the request (ns) */
	u64 queue_t;     /* time queued in list (ns) */
	u64 poweron_t;   /* dvfs start time */
	u64 req_start_t; /* request stant time (ns) */
	u64 req_end_t;   /* request end time (ns) */
	u64 wait_t;      /* time waited by user */
};

#endif

