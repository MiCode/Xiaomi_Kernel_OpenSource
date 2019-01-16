/*
 * kernel/power/tuxonice.h
 *
 * Copyright (C) 2004-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * It contains declarations used throughout swsusp.
 *
 */

#ifndef KERNEL_POWER_TOI_H
#define KERNEL_POWER_TOI_H

#include <linux/delay.h>
#include <linux/bootmem.h>
#include <linux/suspend.h>
#include <linux/fs.h>
#include <linux/module.h>
#include <asm/setup.h>
#include "tuxonice_pageflags.h"
#include "power.h"

#define TOI_CORE_VERSION "3.3"
#define	TOI_HEADER_VERSION 3
#define MY_BOOT_KERNEL_DATA_VERSION 4

#define HIB_TOI_DEBUG 1
#define _TAG_HIB_M "HIB/TOI"
#if (HIB_TOI_DEBUG)
#undef hib_log
#define hib_log(fmt, ...)   pr_warn("[%s] [%s()]" fmt, _TAG_HIB_M, __func__, ##__VA_ARGS__);
#else
#define hib_log(fmt, ...)
#endif
#undef hib_warn
#define hib_warn(fmt, ...)  pr_warn("[%s] [%s()]" fmt, _TAG_HIB_M, __func__, ##__VA_ARGS__);
#undef hib_err
#define hib_err(fmt, ...)   pr_err("[%s] [%s()]" fmt, _TAG_HIB_M, __func__, ##__VA_ARGS__);

#ifdef CONFIG_TOI_FIXUP
#define HIB_SHOW_MEMINFO()                                              \
    do {                                                                \
        hib_warn("%d MEMINFO ---\n", __LINE__);                         \
        show_mem(SHOW_MEM_FILTER_NODES|SHOW_MEM_FILTER_PAGE_COUNT);     \
    } while (0)
#endif

struct toi_boot_kernel_data {
	int version;
	int size;
	unsigned long toi_action;
	unsigned long toi_debug_state;
	u32 toi_default_console_level;
	int toi_io_time[2][2];
	char toi_nosave_commandline[COMMAND_LINE_SIZE];
	unsigned long pages_used[33];
	unsigned long incremental_bytes_in;
	unsigned long incremental_bytes_out;
	unsigned long compress_bytes_in;
	unsigned long compress_bytes_out;
	unsigned long pruned_pages;
};

extern struct toi_boot_kernel_data toi_bkd;

/* Location of book kernel data struct in kernel being resumed */
extern unsigned long boot_kernel_data_buffer;

/*		 == Action states ==		*/

enum {
	TOI_REBOOT,
	TOI_PAUSE,
	TOI_LOGALL,
	TOI_CAN_CANCEL,
	TOI_KEEP_IMAGE,
	TOI_FREEZER_TEST,
	TOI_SINGLESTEP,
	TOI_PAUSE_NEAR_PAGESET_END,
	TOI_TEST_FILTER_SPEED,
	TOI_TEST_BIO,
	TOI_NO_PAGESET2,
	TOI_IGNORE_ROOTFS,
	TOI_REPLACE_SWSUSP,
	TOI_PAGESET2_FULL,
	TOI_ABORT_ON_RESAVE_NEEDED,
	TOI_NO_MULTITHREADED_IO,
	TOI_NO_DIRECT_LOAD,	/* Obsolete */
	TOI_LATE_CPU_HOTPLUG,
	TOI_GET_MAX_MEM_ALLOCD,
	TOI_NO_FLUSHER_THREAD,
	TOI_NO_PS2_IF_UNNEEDED,
	TOI_POST_RESUME_BREAKPOINT,
	TOI_NO_READAHEAD,
};

extern unsigned long toi_bootflags_mask;

#define clear_action_state(bit) (test_and_clear_bit(bit, &toi_bkd.toi_action))

/*		 == Result states ==		*/

enum {
	TOI_ABORTED,
	TOI_ABORT_REQUESTED,
	TOI_NOSTORAGE_AVAILABLE,
	TOI_INSUFFICIENT_STORAGE,
	TOI_FREEZING_FAILED,
	TOI_KEPT_IMAGE,
	TOI_WOULD_EAT_MEMORY,
	TOI_UNABLE_TO_FREE_ENOUGH_MEMORY,
	TOI_PM_SEM,
	TOI_DEVICE_REFUSED,
	TOI_SYSDEV_REFUSED,
	TOI_EXTRA_PAGES_ALLOW_TOO_SMALL,
	TOI_UNABLE_TO_PREPARE_IMAGE,
	TOI_FAILED_MODULE_INIT,
	TOI_FAILED_MODULE_CLEANUP,
	TOI_FAILED_IO,
	TOI_OUT_OF_MEMORY,
	TOI_IMAGE_ERROR,
	TOI_PLATFORM_PREP_FAILED,
	TOI_CPU_HOTPLUG_FAILED,
	TOI_ARCH_PREPARE_FAILED,	/* Removed Linux-3.0 */
	TOI_RESAVE_NEEDED,
	TOI_CANT_SUSPEND,
	TOI_NOTIFIERS_PREPARE_FAILED,
	TOI_PRE_SNAPSHOT_FAILED,
	TOI_PRE_RESTORE_FAILED,
	TOI_USERMODE_HELPERS_ERR,
	TOI_CANT_USE_ALT_RESUME,
	TOI_HEADER_TOO_BIG,
	TOI_WAKEUP_EVENT,
	TOI_SYSCORE_REFUSED,
	TOI_DPM_PREPARE_FAILED,
#ifdef CONFIG_TOI_FIXUP
	/* TOI_DPM_PREPARE_FAILED will exceed the # of bit when set_abort_result(TOI_DPM_SUSPEND_FAILED) is called!!! */
	TOI_DPM_SUSPEND_FAILED = TOI_DPM_PREPARE_FAILED,
#else
    TOI_DPM_SUSPEND_FAILED,
#endif
	TOI_NUM_RESULT_STATES	/* Used in printing debug info only */
};

extern unsigned long toi_result;

#define set_result_state(bit) (test_and_set_bit(bit, &toi_result))
#define set_abort_result(bit) (test_and_set_bit(TOI_ABORTED, &toi_result), \
				test_and_set_bit(bit, &toi_result))
#define clear_result_state(bit) (test_and_clear_bit(bit, &toi_result))
#define test_result_state(bit) (test_bit(bit, &toi_result))

/*	 == Debug sections and levels ==	*/

/* debugging levels. */
enum {
	TOI_STATUS = 0,
	TOI_ERROR = 2,
	TOI_LOW,
	TOI_MEDIUM,
	TOI_HIGH,
	TOI_VERBOSE,
};

enum {
	TOI_ANY_SECTION,
	TOI_EAT_MEMORY,
	TOI_IO,
	TOI_HEADER,
	TOI_WRITER,
	TOI_MEMORY,
	TOI_PAGEDIR,
	TOI_COMPRESS,
	TOI_BIO,
};

#define set_debug_state(bit) (test_and_set_bit(bit, &toi_bkd.toi_debug_state))
#define clear_debug_state(bit) \
	(test_and_clear_bit(bit, &toi_bkd.toi_debug_state))
#define test_debug_state(bit) (test_bit(bit, &toi_bkd.toi_debug_state))

/*		== Steps in hibernating ==	*/

enum {
	STEP_HIBERNATE_PREPARE_IMAGE,
	STEP_HIBERNATE_SAVE_IMAGE,
	STEP_HIBERNATE_POWERDOWN,
	STEP_RESUME_CAN_RESUME,
	STEP_RESUME_LOAD_PS1,
	STEP_RESUME_DO_RESTORE,
	STEP_RESUME_READ_PS2,
	STEP_RESUME_GO,
	STEP_RESUME_ALT_IMAGE,
	STEP_CLEANUP,
	STEP_QUIET_CLEANUP
};

/*		== TuxOnIce states ==
	(see also include/linux/suspend.h)	*/

#define get_toi_state()  (toi_state)
#define restore_toi_state(saved_state) \
	do { toi_state = saved_state; } while (0)

/*		== Module support ==		*/

struct toi_core_fns {
	int (*post_context_save) (void);
	unsigned long (*get_nonconflicting_page) (void);
	int (*try_hibernate) (void);
	void (*try_resume) (void);
};

extern struct toi_core_fns *toi_core_fns;

/*		== All else ==			*/
#undef KB
#undef MB
#define KB(x) ((x) << (PAGE_SHIFT - 10))
#define MB(x) ((x) >> (20 - PAGE_SHIFT))

extern int toi_start_anything(int toi_or_resume);
extern void toi_finish_anything(int toi_or_resume);

extern int save_image_part1(void);
extern int toi_atomic_restore(void);

extern int toi_try_hibernate(void);
extern void toi_try_resume(void);

extern int __toi_post_context_save(void);

extern unsigned int nr_hibernates;
extern char alt_resume_param[256];

extern void copyback_post(void);
extern int toi_hibernate(void);
extern unsigned long extra_pd1_pages_used;

#define SECTOR_SIZE 512

extern void toi_early_boot_message(int can_erase_image, int default_answer,
				   char *warning_reason, ...);

extern int do_check_can_resume(void);
extern int do_toi_step(int step);
extern int toi_launch_userspace_program(char *command, int channel_no, int wait, int debug);

extern char tuxonice_signature[9];

extern int toi_start_other_threads(void);
extern void toi_stop_other_threads(void);

#ifdef CONFIG_TOI_ENHANCE
extern int toi_ignore_late_initcall(void);
#endif /* CONFIG_TOI_ENHANCE */

#endif
