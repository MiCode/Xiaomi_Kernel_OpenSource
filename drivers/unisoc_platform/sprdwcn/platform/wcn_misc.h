// SPDX-License-Identifier: GPL-2.0-only
/*
 * wcn_misc.h - Unisoc platform header
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#ifndef __WCN_MISC_H__
#define __WCN_MISC_H__

#include <linux/mutex.h>
#include <linux/types.h>
#include <asm-generic/div64.h>
#include "sprd_wcn_glb.h"

/* Hours offset for GM and China-BeiJing */
#define WCN_BTWF_TIME_OFFSET (8)

#define ATCMD_FIFO_MAX	(16)

/*
 * AP use 64 bit for ns time.
 * marlin use 32 bit for ms time
 * we change ns to ms, and remove high bit value.
 * 32bit ms is more than 42days, it's engough
 * for loopcheck debug.
 */
#define NS_TO_MS                    1000000
#define MARLIN_64B_NS_TO_32B_MS(ns) do_div(ns, NS_TO_MS)
//#define MARLIN_64B_NS_TO_32B_MS(ns) ((unsigned int)(ns / 1000000))

enum atcmd_owner {
	/* default AT CMD reply to WCND */
	WCN_ATCMD_WCND = 0x0,
	/* Kernel not deal response info from CP2. 20180515 */
	WCN_ATCMD_KERNEL,
	WCN_ATCMD_LOG,
};

/*
 * Until now, CP2 response every AT CMD to AP side
 * without owner-id.
 * AP side transfer every ATCMD response info to WCND.
 * If AP send AT CMD on kernel layer, and the response
 * info transfer to WCND and caused WCND deal error
 * response CMD.
 * We will save all of the owner-id to the fifo.
 * and dispatch the response ATCMD info to the matched owner.
 */
struct atcmd_fifo {
	enum atcmd_owner owner[ATCMD_FIFO_MAX];
	unsigned int head;
	unsigned int tail;
	struct mutex lock;
};

struct wcn_tm {
	long tm_msec;    /* mili seconds */
	long tm_sec;     /* seconds */
	long tm_min;     /* minutes */
	long tm_hour;    /* hours */
	long tm_mday;    /* day of the month */
	long tm_mon;     /* month */
	long tm_year;    /* year */
};

void mdbg_atcmd_owner_init(void);
void mdbg_atcmd_owner_deinit(void);
long int mdbg_send_atcmd(char *buf, size_t len, enum atcmd_owner owner);
enum atcmd_owner mdbg_atcmd_owner_peek(void);
void mdbg_atcmd_clean(void);
/* AP notify BTWF time by at+aptime=... cmd */
long int wcn_ap_notify_btwf_time(void);
/*
 * Only marlin poweron, CP2 CPU tick starts to run,
 * It can call this function.
 * The time will be sent to marlin with loopcheck CMD.
 * NOTES:If marlin power off, and power on again, it
 * should call this function also.
 */
void marlin_bootup_time_update(void);
unsigned long long marlin_bootup_time_get(void);
char *wcn_get_kernel_time(void);

int wcn_write_zero_to_phy_addr(phys_addr_t phy_addr, u32 size);
int wcn_write_data_to_phy_addr(phys_addr_t phy_addr,
			       void *src_data, u32 size);
int wcn_read_data_from_phy_addr(phys_addr_t phy_addr,
				void *tar_data, u32 size);
void *wcn_mem_ram_vmap_nocache(phys_addr_t start, size_t size,
			       unsigned int *count);
void wcn_mem_ram_unmap(const void *mem, unsigned int count);
#endif
