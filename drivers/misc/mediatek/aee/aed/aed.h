/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __aed_h
#define __aed_h

#include <generated/autoconf.h>
#include <linux/types.h>
#include <linux/bug.h>
#include <mt-plat/aee.h>
#include <linux/kallsyms.h>
#include <linux/ptrace.h>

#define AE_INVALID              0xAEEFF000
#define AE_NOT_AVAILABLE        0xAEE00000
#define AE_DEFAULT              0xAEE00001

enum AEE_MODE {
	AEE_MODE_MTK_ENG = 1,
	AEE_MODE_MTK_USER,
	AEE_MODE_CUSTOMER_ENG,
	AEE_MODE_CUSTOMER_USER,
	AEE_MODE_NOT_INIT
};

enum AEE_FORCE_RED_SCREEN_VALUE {
	AEE_FORCE_DISABLE_RED_SCREEN = 0,
	AEE_FORCE_RED_SCREEN,
	AEE_FORCE_NOT_SET
};

enum AEE_FORCE_EXP {
	AEE_FORCE_EXP_DISABLE = 0,
	AEE_FORCE_EXP_ENABLE,
	AEE_FORCE_EXP_NOT_SET
};

enum AE_ERR {
	AE_SUCC,
	AE_FAIL
};

enum AE_PASS_METHOD {
	AE_PASS_BY_MEM,
	AE_PASS_BY_FILE,
	AE_PASS_METHOD_END
};

enum AE_CMD_TYPE { AE_REQ, AE_RSP, AE_IND, AE_CMD_TYPE_END };

enum AE_CMD_ID {
	AE_REQ_IDX,

	AE_REQ_CLASS,
	AE_REQ_TYPE,
	AE_REQ_PROCESS,
	AE_REQ_MODULE,
	AE_REQ_BACKTRACE,
	/* Content of response message rule:
	 *   if msg.arg1==AE_PASS_BY_FILE => msg->data=file path
	 */
	AE_REQ_DETAIL,

	AE_REQ_ROOT_LOG_DIR,
	AE_REQ_CURR_LOG_DIR,
	AE_REQ_DFLT_LOG_DIR,
	AE_REQ_MAIN_LOG_FILE_PATH,

	/* fatal event raised, indicate AED to notify users */
	AE_IND_FATAL_RAISED,
	/* exception event raised, indicate AED to notify users */
	AE_IND_EXP_RAISED,
	/* warning event raised, indicate AED to notify users */
	AE_IND_WRN_RAISED,
	/* reminding event raised, indicate AED to notify users */
	AE_IND_REM_RAISED,

	/* arg = AE_ERR */
	AE_IND_LOG_STATUS,
	/* arg = AE_ERR */
	AE_IND_LOG_CLOSE,

	/* arg: dal on|off, seq: beep on|off */
	AE_REQ_SWITCH_DAL_BEEP,
	/* arg: db count */
	AE_REQ_DB_COUNT,
	/* arg: force db path yes\no */
	AE_REQ_DB_FORCE_PATH,
	AE_REQ_SWITCH_EXP_LEVEL,
	/* query if AED is ready for service */
	AE_REQ_IS_AED_READY,
	/* msg->data=file path */
	AE_REQ_COREDUMP,
	/* set read flag msg */
	AE_REQ_SET_READFLAG,
	/* Init notification of client side(application layer) of Exp2Server */
	AE_REQ_E2S_INIT,
	AE_REQ_USERSPACEBACKTRACE = 40,
	AE_REQ_USER_REG,
	AE_REQ_USER_MAPS,
	AE_REQ_TRIGGER_TIME,	/* get db trigger time */
	AE_CMD_ID_END
};

struct AE_Msg {
	enum AE_CMD_TYPE cmdType;	/* command type */
	enum AE_CMD_ID cmdId;	/* command Id */
	union {
		unsigned int seq;	/* sequence number for error checking */
		int pid;	/* process id */
	};
	union {
		unsigned int arg;	/* simple argument */
		enum AE_EXP_CLASS cls;	/* exception/error/defect class */
	};
	union {
		unsigned int len;	/* dynamic length argument */
		int id;		/* desired id */
	};
	unsigned int dbOption;	/* DB dump option */
};

/* Kernel IOCTL interface */
struct aee_dal_show {
	char msg[1024];
};

struct aee_dal_setcolor {
	unsigned int foreground;
	unsigned int background;
	unsigned int screencolor;
};

/* we use MAX_NR_FRAME to control max unwind layer */
#define MAX_AEE_KERNEL_BT 16
#define AEE_NR_FRAME 32

struct aee_ioctl {
	__u32 pid;
	__u32 detail;
	__u32 size;
	__u32 pad;
	__u64 in;
	__u64 out;
};


struct aee_thread_user_stack {
	pid_t tid;
	int StackLength;
	/* 8k stack ,define to char only for match 64bit/32bit */
	unsigned char Userspace_Stack[8192];
};

struct aee_siginfo {
	pid_t tid;
	int si_signo;
	int si_errno;
	int si_code;
	uintptr_t fault_addr;
};

/* Show string on DAL layer  */
#define AEEIOCTL_DAL_SHOW       _IOW('p', 0x01, struct aee_dal_show)
#define AEEIOCTL_DAL_CLEAN      _IO('p', 0x02)	/* Clear DAL layer */
/* RGB color 0x00RRGGBB */
#define AEEIOCTL_SETCOLOR       _IOW('p', 0x03, struct aee_dal_setcolor)
#define AEEIOCTL_GET_PROCESS_BT _IOW('p', 0x04, struct aee_ioctl)
#define AEEIOCTL_GET_SMP_INFO   _IOR('p', 0x05, int)
#define AEEIOCTL_SET_AEE_MODE   _IOR('p', 0x06, int)
#define AEEIOCTL_GET_THREAD_REG _IOW('p', 0x07, struct aee_thread_reg)
#define AEEIOCTL_CHECK_SUID_DUMPABLE _IOR('p', 0x08, int)
/* AED debug support */
#define AEEIOCTL_WDT_KICK_POWERKEY _IOR('p', 0x09, int)

#define AEEIOCTL_RT_MON_Kick _IOR('p', 0x0A, int)
#define AEEIOCTL_SET_FORECE_RED_SCREEN _IOR('p', 0x0B, int)
#define AEEIOCTL_SET_SF_STATE _IOR('p', 0x0C, long long)
#define AEEIOCTL_GET_SF_STATE _IOW('p', 0x0D, long long)
#define AEEIOCTL_USER_IOCTL_TO_KERNEL_WANING _IOR('p', 0x0E, int)
#define AEEIOCTL_SET_AEE_FORCE_EXP _IOR('p', 0x0F, int)
#define AEEIOCTL_GET_AEE_SIGINFO _IOW('p', 0x10, struct aee_siginfo)
#define AEEIOCTL_SET_HANG_FLAG _IOW('p', 0x11, int)
#define AEEIOCTL_SET_HANG_REBOOT _IO('p', 0x12)
#define AEEIOCTL_GET_THREAD_RMS  _IOW('p', 0x13, struct unwind_info_rms)
#define AEEIOCTL_GET_THREAD_STACK_RAW  _IOW('p', 0x14, struct unwind_info_stack)


#define AED_FILE_OPS(entry) \
	static const struct file_operations proc_##entry##_fops = { \
		.read = proc_##entry##_read, \
		.write = proc_##entry##_write, \
	}

#define AED_FILE_OPS_RO(entry) \
	static const struct file_operations proc_##entry##_fops = { \
		.read = proc_##entry##_read, \
	}

#define  AED_PROC_ENTRY(name, entry, mode)\
	({if (!proc_create(#name, S_IFREG | mode, aed_proc_dir, \
		&proc_##entry##_fops)) \
		pr_info("proc_create %s failed\n", #name); })


struct proc_dir_entry;

int aed_proc_debug_init(struct proc_dir_entry *aed_proc_dir);
int aed_proc_debug_done(struct proc_dir_entry *aed_proc_dir);

void aee_rr_proc_init(struct proc_dir_entry *aed_proc_dir);
void aee_rr_proc_done(struct proc_dir_entry *aed_proc_dir);

void dram_console_init(struct proc_dir_entry *aed_proc_dir);
void dram_console_done(struct proc_dir_entry *aed_proc_dir);

extern struct atomic_notifier_head panic_notifier_list;
extern int ksysfs_bootinfo_init(void);
extern void ksysfs_bootinfo_exit(void);
extern int aee_dump_ccci_debug_info(int md_id, void **addr, int *size);
extern void show_stack(struct task_struct *tsk, unsigned long *sp);
extern int aee_mode;
extern void aee_kernel_RT_Monitor_api(int lParam);
extern void mlog_get_buffer(char **ptr, int *size)__attribute__((weak));
extern void get_msdc_aee_buffer(unsigned long *buff,
	unsigned long *size)__attribute__((weak));
extern void show_task_mem(void)__attribute__((weak));
void show_native_bt_by_pid(int task_pid);
#endif
