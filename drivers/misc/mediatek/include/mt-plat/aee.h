/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#if !defined(__AEE_H__)
#define __AEE_H__

#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/ratelimit.h>
#include <linux/sched.h>

#define AEE_MODULE_NAME_LENGTH 64
#define AEE_PROCESS_NAME_LENGTH 256
#define AEE_BACKTRACE_LENGTH 3072
#define MODULES_INFO_BUF_SIZE SZ_16K


enum AEE_REBOOT_MODE {
	AEE_REBOOT_MODE_NORMAL = 0,
	AEE_REBOOT_MODE_KERNEL_OOPS,
	AEE_REBOOT_MODE_KERNEL_PANIC,
	AEE_REBOOT_MODE_NESTED_EXCEPTION,
	AEE_REBOOT_MODE_WDT,
	AEE_REBOOT_MODE_MANUAL_KDUMP,
	AEE_REBOOT_MODE_MRDUMP_KEY,
	AEE_REBOOT_MODE_GZ_KE,
	AEE_REBOOT_MODE_GZ_WDT,
	AEE_REBOOT_MODE_HANG_DETECT,
};

#define AEE_SZ_SYMBOL_L 140
#define AEE_SZ_SYMBOL_S 80
struct aee_bt_frame {
	__u64 pc;
	__u64 lr;
	__u32 pad[5];
	/* Now we use different symbol length for PC &LR */
	char pc_symbol[AEE_SZ_SYMBOL_S];
	char lr_symbol[AEE_SZ_SYMBOL_L];
};

/* aee_process_info struct should strictly small than ipanic_buffer, now 4KB */
struct aee_process_info {
	char process_path[AEE_PROCESS_NAME_LENGTH];
	char backtrace[AEE_BACKTRACE_LENGTH];
	struct aee_bt_frame ke_frame;
};

struct aee_process_bt {
	__u32 pid;
	__u32 nr_entries;
	struct aee_bt_frame *entries;
};


struct aee_thread_reg {
	pid_t tid;
	struct pt_regs regs;
};

struct aee_user_thread_stack {
	pid_t tid;
	int StackLength;
	/*8k stack ,define to char only for match 64bit/32bit*/
	unsigned char *Userthread_Stack;
};

struct aee_user_thread_maps {
	pid_t tid;
	int Userthread_mapsLength;
	/*8k stack ,define to char only for match 64bit/32bit*/
	unsigned char *Userthread_maps;
};

struct unwind_info_stack {
	pid_t tid __packed __aligned(8);
#ifdef __aarch64__
	__u64 sp;
#else
	long sp __packed __aligned(8);
#endif
	int StackLength __packed __aligned(8);
	unsigned char *Userthread_Stack __packed __aligned(8);
};

struct unwind_info_rms {
	pid_t tid __packed __aligned(8);
	struct pt_regs *regs __packed __aligned(8);
	int StackLength __packed __aligned(8);
	unsigned char *Userthread_Stack __packed __aligned(8);
	int Userthread_mapsLength __packed __aligned(8);
	unsigned char *Userthread_maps __packed __aligned(8);
};

#ifdef CONFIG_CONSOLE_LOCK_DURATION_DETECT
extern char *mtk8250_uart_dump(void);
#endif

#define AEE_MTK_CPU_NUMS	16
/* powerkey press,modules use bits */
#define AE_WDT_Powerkey_DEVICE_PATH		"/dev/kick_powerkey"
#define WDT_SETBY_DEFAULT			(0)
#define WDT_SETBY_Backlight			(1<<0)
#define WDT_SETBY_Display			(1<<1)
#define WDT_SETBY_SF				(1<<2)
#define WDT_SETBY_PM				(1<<3)
#define WDT_SETBY_WMS_DISABLE_PWK_MONITOR	(0xAEEAEE00)
#define WDT_SETBY_WMS_ENABLE_PWK_MONITOR	(0xAEEAEE01)
#define WDT_PWK_HANG_FORCE_HWT			(0xAEE0FFFF)

/* QHQ RT Monitor */
#define AEEIOCTL_RT_MON_Kick _IOR('p', 0x0A, int)
#define AE_WDT_DEVICE_PATH      "/dev/RT_Monitor"
/* QHQ RT Monitor    end */

/* DB dump option bits, set relative bit to 1 to include related file in db */
#define DB_OPT_DEFAULT				(0)
#define DB_OPT_FTRACE				(1<<0)
#define DB_OPT_PRINTK_TOO_MUCH			(1<<1)
#define DB_OPT_NE_JBT_TRACES			(1<<2)
#define DB_OPT_SWT_JBT_TRACES			(1<<3)
#define DB_OPT_VM_TRACES			(1<<4)
#define DB_OPT_DUMPSYS_ACTIVITY			(1<<5)
#define DB_OPT_DUMPSYS_WINDOW			(1<<6)
#define DB_OPT_DUMPSYS_GFXINFO			(1<<7)
#define DB_OPT_DUMPSYS_SURFACEFLINGER		(1<<8)
#define DB_OPT_DISPLAY_HANG_DUMP		(1<<9)
#define DB_OPT_LOW_MEMORY_KILLER		(1<<10)
#define DB_OPT_PROC_MEM				(1<<11)
#define DB_OPT_FS_IO_LOG			(1<<12)
#define DB_OPT_PROCESS_COREDUMP			(1<<13)
#define DB_OPT_VM_HPROF				(1<<14)
#define DB_OPT_PROCMEM				(1<<15)
#define DB_OPT_DUMPSYS_INPUT			(1<<16)
#define DB_OPT_MMPROFILE_BUFFER			(1<<17)
#define DB_OPT_BINDER_INFO			(1<<18)
#define DB_OPT_WCN_ISSUE_INFO			(1<<19)
#define DB_OPT_DUMMY_DUMP			(1<<20)
#define DB_OPT_PID_MEMORY_INFO			(1<<21)
#define DB_OPT_VM_OOME_HPROF			(1<<22)
#define DB_OPT_PID_SMAPS			(1<<23)
#define DB_OPT_PROC_CMDQ_INFO			(1<<24)
#define DB_OPT_PROC_USKTRK			(1<<25)
#define DB_OPT_SF_RTT_DUMP			(1<<26)
#define DB_OPT_PAGETYPE_INFO			(1<<27)
#define DB_OPT_DUMPSYS_PROCSTATS		(1<<28)
#define DB_OPT_DUMP_DISPLAY			(1<<29)
#define DB_OPT_NATIVE_BACKTRACE			(1<<30)
#define DB_OPT_AARCH64				(1<<31)

#define AEE_API_CALL_INTERVAL   (120 * HZ)
#define AEE_API_CALL_BURST      2

#if defined(MODULE) || IS_BUILTIN(CONFIG_MTK_AEE_AED)
#define aee_kernel_exception(module, msg...)		\
({							\
	static DEFINE_RATELIMIT_STATE(__func__##_rs,	\
			AEE_API_CALL_INTERVAL,		\
			AEE_API_CALL_BURST);		\
							\
	if (__ratelimit(&(__func__##_rs)))		\
		aee_kernel_exception_api_func(__FILE__, __LINE__,	\
			DB_OPT_DEFAULT, module, msg);	\
})
#define aee_kernel_warning(module, msg...)		\
({							\
	static DEFINE_RATELIMIT_STATE(__func__##_rs,	\
			AEE_API_CALL_INTERVAL,		\
			AEE_API_CALL_BURST);		\
							\
	if (__ratelimit(&(__func__##_rs)))		\
		aee_kernel_warning_api_func(__FILE__, __LINE__,	\
			DB_OPT_DEFAULT, module, msg);		\
})

#define aee_kernel_exception_api(file, line, db_opt, module, msg...)	\
({									\
	static DEFINE_RATELIMIT_STATE(__func__##_rs,			\
			AEE_API_CALL_INTERVAL,				\
			AEE_API_CALL_BURST);				\
	if (__ratelimit(&(__func__##_rs)))				\
		aee_kernel_exception_api_func(__FILE__, __LINE__,	\
			db_opt, module, msg);				\
})

#define aee_kernel_warning_api(file, line, db_opt, module, msg...)	\
({									\
	static DEFINE_RATELIMIT_STATE(__func__##_rs,			\
			AEE_API_CALL_INTERVAL,				\
			AEE_API_CALL_BURST);				\
	if (aee_is_printk_too_much(module))				\
		aee_kernel_warning_api_func(__FILE__, __LINE__, db_opt,	\
				module, msg);				\
	else if (__ratelimit(&(__func__##_rs)))				\
		aee_kernel_warning_api_func(__FILE__, __LINE__, db_opt,	\
				module, msg);				\
})
#else
#undef aee_kernel_warning
#define aee_kernel_warning(module, msg...) WARN(1, msg)

#undef aee_kernel_warning_api
#define aee_kernel_warning_api(file, line, db_opt, module, msg...) \
	WARN(1, msg)

#undef aee_kernel_exception
#define aee_kernel_exception(module, msg...) WARN(1, msg)

#undef aee_kernel_exception_api
#define aee_kernel_exception_api(file, line, db_opt, module, msg...) \
	WARN(1, msg)
#endif

#define aee_kernel_reminding(module, msg...)	\
	aee_kernel_reminding_api(__FILE__, __LINE__, DB_OPT_DEFAULT,	\
			module, msg)

#define aed_md_exception(log, log_size, phy, phy_size, detail)	\
	aed_md_exception_api(log, log_size, phy, phy_size, detail,	\
			DB_OPT_DEFAULT)
#define aed_md32_exception(log, log_size, phy, phy_size, detail)	\
	aed_md32_exception_api(log, log_size, phy, phy_size, detail,	\
			DB_OPT_DEFAULT)
#define aed_scp_exception(log, log_size, phy, phy_size, detail)	\
	aed_scp_exception_api(log, log_size, phy, phy_size, detail,	\
			DB_OPT_DEFAULT)
#define aed_combo_exception(log, log_size, phy, phy_size, detail)	\
	aed_combo_exception_api(log, log_size, phy, phy_size, detail,	\
			DB_OPT_DEFAULT)
#define aed_common_exception(assert_type, log, log_size, phy, phy_size,	\
			detail)	\
	aed_common_exception_api(assert_type, log, log_size, phy,	\
			phy_size, detail, DB_OPT_DEFAULT)

void aee_kernel_exception_api_func(const char *file, const int line,
		const int db_opt, const char *module, const char *msg, ...);
void aee_kernel_warning_api_func(const char *file, const int line,
		const int db_opt, const char *module, const char *msg, ...);
void aee_kernel_reminding_api(const char *file, const int line,
		const int db_opt, const char *module, const char *msg, ...);

void aed_md_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt);
void aed_md32_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt);
void aed_scp_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt);
void aed_combo_exception_api(const int *log, int log_size, const int *phy,
			int phy_size, const char *detail, const int db_opt);
void aed_common_exception_api(const char *assert_type, const int *log, int
			log_size, const int *phy, int phy_size, const char
			*detail, const int db_opt);

int aed_get_status(void);
int aee_is_printk_too_much(const char *module);
void aee_sram_printk(const char *fmt, ...);
int aee_is_enable(void);
#endif/* __AEE_H__ */
