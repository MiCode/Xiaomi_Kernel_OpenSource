/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __RT_MINITOR__H__
#define __RT_MINITOR__H__
/*		rt_monitor		*/
#define SS_WDT_CTRL_SET_PARA	1  /*	systemserver watchdog control cmd	*/
#define SF_WDT_CTRL_SET_PARA	11 /*    surfacefilinger watchdog control cmd  */
#define SF_WDT_CTRL_GET_PARA	12 /*    surfacefilinger watchdog control cmd  */

#define CMD_TYPE_FLAG	(0x3f) /* bit(0) to bit(6) indicate cmd type */
#define CMD_NEW_TYPE	(0x30) /* bit(4) and bit(5) are 1 indicate that the cmd is new type */
#define CMD_NEW_VALUE	(0xffc0) /* bit(6) to bit(15) indicate that the value in new way */
#define GET_REAL_CMD(x)	(x & CMD_TYPE_FLAG)
#define SS_WDT_CTRL_SET_NEW_PARA	(SS_WDT_CTRL_SET_PARA + CMD_NEW_TYPE)
#define GET_TIMEOUT_VALUE(x)	((x & CMD_NEW_VALUE) >> 6)

#define HANG_INFO_MAX (1 * 1024 * 1024)
#define MAX_STRING_SIZE 256
#define WAIT_BOOT_COMPLETE 120	/*wait boot 120 s */
#define MAX_KERNEL_BT 16    /* MAX_NR_FRAME for max unwind layer */
#define NR_FRAME 32
#define SYMBOL_SIZE_L 140
#define SYMBOL_SIZE_S 80
#define CORE_TASK_NAME_SIZE 20
#define CORE_TASK_NUM_MAX 20
#define TASK_STATE_TO_CHAR_STR "RSDTtZXxKWP"


#define SYSDUMP_PROC_BUF_LEN    16
extern void get_native_hang_monitor_buffer(unsigned long *addr, unsigned long *size,
						unsigned long *start);

struct thread_backtrace_info {
	__u32 pid;
	__u32 nr_entries;
	struct backtrace_frame *entries;
};

struct backtrace_frame {
	__u64 pc;
	__u64 lr;
	__u32 pad[5];
	char pc_symbol[SYMBOL_SIZE_S];
	char lr_symbol[SYMBOL_SIZE_L];
};

struct core_task_info {
	int pid;
	char name[CORE_TASK_NAME_SIZE];
};

#if IS_ENABLED(CONFIG_SPRD_MODULES_NOTIFY)
extern int sprd_modules_init(void);
extern void  sprd_modules_exit(void);
#else
static inline int sprd_modules_init(void) { return 0; }
static inline void  sprd_modules_exit(void) {}
#endif
#endif /*	__RT_MINITOR__H__	*/
