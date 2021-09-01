/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __HANG_DETECT_H__
#define __HANG_DETECT_H__

#define HD_PROC "hang_detect"
#define HD_INTER 30	/* 1 tick is 30 seconds*/

struct name_list {
	char name[TASK_COMM_LEN + 1];
	struct name_list *next;
};

struct hang_callback_list
{
	void (*fn)(void);
	struct hang_callback_list *next;
};

/* hang detect timeout value*/
#define	COUNT_SWT_INIT		0
#define	COUNT_SWT_NORMAL	10
#define	COUNT_SWT_FIRST		12
#define	COUNT_ANDROID_REBOOT	11
#define	COUNT_SWT_CREATE_DB	14
#define	COUNT_NE_EXCEPION	20
#define	COUNT_AEE_COREDUMP	40
#define	COUNT_COREDUMP_DONE	19

/*monitor hang ioctl*/
#define HANG_KICK _IOR('p', 0x0A, int)
#define HANG_SET_SF_STATE _IOR('p', 0x0C, long long)
#define HANG_GET_SF_STATE _IOW('p', 0x0D, long long)
#define HANG_SET_FLAG _IOW('p', 0x11, int)
#define HANG_SET_REBOOT _IO('p', 0x12)
#define HANG_ADD_WHITE_LIST _IOR('p', 0x13, char [TASK_COMM_LEN])
#define HANG_DEL_WHITE_LIST _IOR('p', 0x14, char [TASK_COMM_LEN])

extern int register_hang_callback(void (*function_addr)(void));
extern void mrdump_regist_hang_bt(void (*fn)(void));

#ifdef CONFIG_MTK_HANG_DETECT_LOG
#define hang_log pr_info
#else
#define hang_log no_printk
#endif

#endif
