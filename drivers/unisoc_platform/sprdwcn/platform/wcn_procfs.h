// SPDX-License-Identifier: GPL-2.0-only
/*
 * wcn_procfs.h - Unisoc platform header
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

#ifndef _WCN_PROCFS
#define _WCN_PROCFS

#define MDBG_SNAP_SHOOT_SIZE		(32*1024)
#define MDBG_WRITE_SIZE			(64)
#define MDBG_ASSERT_SIZE		(1024)
#define MDBG_LOOPCHECK_SIZE		(128)
#define MDBG_AT_CMD_SIZE		(128)

void mdbg_fs_channel_init(void);
void mdbg_fs_channel_destroy(void);
int proc_fs_init(void);
int mdbg_memory_alloc(void);
void proc_fs_exit(void);
int get_loopcheck_status(void);
void wakeup_loopcheck_int(void);
void loopcheck_ready_clear(void);
void loopcheck_ready_set(void);
void mdbg_assert_interface(char *str);
#endif
