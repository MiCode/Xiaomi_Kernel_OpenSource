/*
 * Copyright (C) 2004-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 */
#include <linux/resume-trace.h>
#include <linux/kernel.h>
#include <linux/swap.h>
#include <linux/syscalls.h>
#include <linux/bio.h>
#include <linux/root_dev.h>
#include <linux/freezer.h>
#include <linux/reboot.h>
#include <linux/writeback.h>
#include <linux/tty.h>
#include <linux/crypto.h>
#include <linux/cpu.h>
#include <linux/ctype.h>
#include "tuxonice_io.h"
#include "tuxonice.h"
#include "tuxonice_extent.h"
#include "tuxonice_netlink.h"
#include "tuxonice_prepare_image.h"
#include "tuxonice_ui.h"
#include "tuxonice_sysfs.h"
#include "tuxonice_pagedir.h"
#include "tuxonice_modules.h"
#include "tuxonice_builtin.h"
#include "tuxonice_power_off.h"
#include "tuxonice_alloc.h"

unsigned long toi_bootflags_mask;
EXPORT_SYMBOL_GPL(toi_bootflags_mask);

/*
 * Highmem related functions (x86 only).
 */

#ifdef CONFIG_HIGHMEM

/**
 * copyback_high: Restore highmem pages.
 *
 * Highmem data and pbe lists are/can be stored in highmem.
 * The format is slightly different to the lowmem pbe lists
 * used for the assembly code: the last pbe in each page is
 * a struct page * instead of struct pbe *, pointing to the
 * next page where pbes are stored (or NULL if happens to be
 * the end of the list). Since we don't want to generate
 * unnecessary deltas against swsusp code, we use a cast
 * instead of a union.
 **/

static void copyback_high(void)
{
	struct page *pbe_page = (struct page *)restore_highmem_pblist;
	struct pbe *this_pbe, *first_pbe;
	unsigned long *origpage, *copypage;
	int pbe_index = 1;

	if (!pbe_page)
		return;

	this_pbe = (struct pbe *)kmap_atomic(pbe_page);
	first_pbe = this_pbe;

	while (this_pbe) {
		int loop = (PAGE_SIZE / sizeof(unsigned long)) - 1;

		origpage = kmap_atomic(pfn_to_page((unsigned long)this_pbe->orig_address));
		copypage = kmap_atomic((struct page *)this_pbe->address);

		while (loop >= 0) {
			*(origpage + loop) = *(copypage + loop);
			loop--;
		}

		kunmap_atomic(origpage);
		kunmap_atomic(copypage);

		if (!this_pbe->next)
			break;

		if (pbe_index < PBES_PER_PAGE) {
			this_pbe++;
			pbe_index++;
		} else {
			pbe_page = (struct page *)this_pbe->next;
			kunmap_atomic(first_pbe);
			if (!pbe_page)
				return;
			this_pbe = (struct pbe *)kmap_atomic(pbe_page);
			first_pbe = this_pbe;
			pbe_index = 1;
		}
	}
	kunmap_atomic(first_pbe);
}

#else				/* CONFIG_HIGHMEM */
static void copyback_high(void)
{
}
#endif

char toi_wait_for_keypress_dev_console(int timeout)
{
	int fd, this_timeout = 255;
	char key = '\0';
	struct termios t, t_backup;

	/* We should be guaranteed /dev/console exists after populate_rootfs()
	 * in init/main.c.
	 */
	fd = sys_open("/dev/console", O_RDONLY, 0);
	if (fd < 0) {
		printk(KERN_INFO "Couldn't open /dev/console.\n");
		return key;
	}

	if (sys_ioctl(fd, TCGETS, (long)&t) < 0)
		goto out_close;

	memcpy(&t_backup, &t, sizeof(t));

	t.c_lflag &= ~(ISIG | ICANON | ECHO);
	t.c_cc[VMIN] = 0;

 new_timeout:
	if (timeout > 0) {
		this_timeout = timeout < 26 ? timeout : 25;
		timeout -= this_timeout;
		this_timeout *= 10;
	}

	t.c_cc[VTIME] = this_timeout;

	if (sys_ioctl(fd, TCSETS, (long)&t) < 0)
		goto out_restore;

	while (1) {
		if (sys_read(fd, &key, 1) <= 0) {
			if (timeout)
				goto new_timeout;
			key = '\0';
			break;
		}
		key = tolower(key);
		if (test_toi_state(TOI_SANITY_CHECK_PROMPT)) {
			if (key == 'c') {
				set_toi_state(TOI_CONTINUE_REQ);
				break;
			} else if (key == ' ')
				break;
		} else
			break;
	}

 out_restore:
	sys_ioctl(fd, TCSETS, (long)&t_backup);
 out_close:
	sys_close(fd);

	return key;
}
EXPORT_SYMBOL_GPL(toi_wait_for_keypress_dev_console);

struct toi_boot_kernel_data toi_bkd __nosavedata __attribute__ ((aligned(PAGE_SIZE))) = {
	MY_BOOT_KERNEL_DATA_VERSION, 0,
#ifdef CONFIG_TOI_REPLACE_SWSUSP
	    (1 << TOI_REPLACE_SWSUSP) |
#endif
	    (1 << TOI_NO_FLUSHER_THREAD) |
/* (1 << TOI_NO_MULTITHREADED_IO) | */
(1 << TOI_LATE_CPU_HOTPLUG) | (1 << TOI_PAGESET2_FULL),};

EXPORT_SYMBOL_GPL(toi_bkd);

struct block_device *toi_open_by_devnum(dev_t dev)
{
	struct block_device *bdev = bdget(dev);
	int err = -ENOMEM;
	if (bdev)
		err = blkdev_get(bdev, FMODE_READ | FMODE_NDELAY, NULL);
	return err ? ERR_PTR(err) : bdev;
}
EXPORT_SYMBOL_GPL(toi_open_by_devnum);

/**
 * toi_close_bdev: Close a swap bdev.
 *
 * int: The swap entry number to close.
 */
void toi_close_bdev(struct block_device *bdev)
{
	blkdev_put(bdev, FMODE_READ | FMODE_NDELAY);
}
EXPORT_SYMBOL_GPL(toi_close_bdev);

int toi_wait = CONFIG_TOI_DEFAULT_WAIT;
EXPORT_SYMBOL_GPL(toi_wait);

struct toi_core_fns *toi_core_fns;
EXPORT_SYMBOL_GPL(toi_core_fns);

unsigned long toi_result;
EXPORT_SYMBOL_GPL(toi_result);

struct pagedir pagedir1 = { 1 };

EXPORT_SYMBOL_GPL(pagedir1);

unsigned long toi_get_nonconflicting_page(void)
{
	return toi_core_fns->get_nonconflicting_page();
}

int toi_post_context_save(void)
{
	return toi_core_fns->post_context_save();
}

int try_tuxonice_hibernate(void)
{
	if (!toi_core_fns)
		return -ENODEV;

	return toi_core_fns->try_hibernate();
}

static int num_resume_calls;
#ifdef CONFIG_TOI_IGNORE_LATE_INITCALL
static int ignore_late_initcall = 1;
#else
static int ignore_late_initcall;
#endif

#ifdef CONFIG_TOI_ENHANCE
int toi_ignore_late_initcall(void)
{
	return ignore_late_initcall;
}
EXPORT_SYMBOL_GPL(toi_ignore_late_initcall);
#endif

int toi_translate_err_default = TOI_CONTINUE_REQ;
EXPORT_SYMBOL_GPL(toi_translate_err_default);

void try_tuxonice_resume(void)
{
	/* Don't let it wrap around eventually */
	if (num_resume_calls < 2)
		num_resume_calls++;

	if (num_resume_calls == 1 && ignore_late_initcall) {
		printk(KERN_INFO "TuxOnIce: Ignoring late initcall, as requested.\n");
		return;
	}

	if (toi_core_fns)
		toi_core_fns->try_resume();
	else
		printk(KERN_INFO "TuxOnIce core not loaded yet.\n");
}

int toi_lowlevel_builtin(void)
{
	int error = 0;

	save_processor_state();
	error = swsusp_arch_suspend();
	if (error)
		printk(KERN_ERR "Error %d hibernating\n", error);

#ifdef CONFIG_TOI_ENHANCE
	if (test_result_state(TOI_ARCH_PREPARE_FAILED)) {
		hib_err("CAUTION: error(%d/0x%08x)\n", error, (unsigned int)toi_result);
	}
#endif

	/* Restore control flow appears here */
	if (!toi_in_hibernate) {
		copyback_high();
		set_toi_state(TOI_NOW_RESUMING);
	}

	restore_processor_state();
	return error;
}
EXPORT_SYMBOL_GPL(toi_lowlevel_builtin);

unsigned long toi_compress_bytes_in;
EXPORT_SYMBOL_GPL(toi_compress_bytes_in);

unsigned long toi_compress_bytes_out;
EXPORT_SYMBOL_GPL(toi_compress_bytes_out);

int toi_in_suspend(void)
{
	return in_suspend;
}
EXPORT_SYMBOL_GPL(toi_in_suspend);

unsigned long toi_state = ((1 << TOI_BOOT_TIME) |
			   (1 << TOI_IGNORE_LOGLEVEL) | (1 << TOI_IO_STOPPED));
EXPORT_SYMBOL_GPL(toi_state);

/* The number of hibernates we have started (some may have been cancelled) */
unsigned int nr_hibernates;
EXPORT_SYMBOL_GPL(nr_hibernates);

int toi_running;
EXPORT_SYMBOL_GPL(toi_running);

__nosavedata int toi_in_hibernate;
EXPORT_SYMBOL_GPL(toi_in_hibernate);

__nosavedata struct pbe *restore_highmem_pblist;
EXPORT_SYMBOL_GPL(restore_highmem_pblist);

int toi_trace_allocs;
EXPORT_SYMBOL_GPL(toi_trace_allocs);

void toi_read_lock_tasklist(void)
{
	read_lock(&tasklist_lock);
}
EXPORT_SYMBOL_GPL(toi_read_lock_tasklist);

void toi_read_unlock_tasklist(void)
{
	read_unlock(&tasklist_lock);
}
EXPORT_SYMBOL_GPL(toi_read_unlock_tasklist);

#ifdef CONFIG_TOI_ZRAM_SUPPORT
int (*toi_flag_zram_disks) (void);
EXPORT_SYMBOL_GPL(toi_flag_zram_disks);

int toi_do_flag_zram_disks(void)
{
	return toi_flag_zram_disks ? (*toi_flag_zram_disks) () : 0;
}
EXPORT_SYMBOL_GPL(toi_do_flag_zram_disks);
#endif

static int __init toi_wait_setup(char *str)
{
	int value;

	if (sscanf(str, "=%d", &value)) {
		if (value < -1 || value > 255)
			printk(KERN_INFO "TuxOnIce_wait outside range -1 to " "255.\n");
		else
			toi_wait = value;
	}

	return 1;
}

__setup("toi_wait", toi_wait_setup);

static int __init toi_translate_retry_setup(char *str)
{
	toi_translate_err_default = 0;
	return 1;
}

__setup("toi_translate_retry", toi_translate_retry_setup);

static int __init toi_debug_setup(char *str)
{
	toi_bkd.toi_action |= (1 << TOI_LOGALL);
	toi_bootflags_mask |= (1 << TOI_LOGALL);
	toi_bkd.toi_debug_state = 255;
	toi_bkd.toi_default_console_level = 7;
	return 1;
}

__setup("toi_debug_setup", toi_debug_setup);

static int __init toi_pause_setup(char *str)
{
	toi_bkd.toi_action |= (1 << TOI_PAUSE);
	toi_bootflags_mask |= (1 << TOI_PAUSE);
	return 1;
}

__setup("toi_pause", toi_pause_setup);

#ifdef CONFIG_PM_DEBUG
static int __init toi_trace_allocs_setup(char *str)
{
	int value;

	if (sscanf(str, "=%d", &value))
		toi_trace_allocs = value;

	return 1;
}

__setup("toi_trace_allocs", toi_trace_allocs_setup);
#endif

static int __init toi_ignore_late_initcall_setup(char *str)
{
	int value;

	if (sscanf(str, "=%d", &value))
		ignore_late_initcall = value;

	return 1;
}

__setup("toi_initramfs_resume_only", toi_ignore_late_initcall_setup);

static int __init toi_force_no_multithreaded_setup(char *str)
{
	int value;

	toi_bkd.toi_action &= ~(1 << TOI_NO_MULTITHREADED_IO);
	toi_bootflags_mask |= (1 << TOI_NO_MULTITHREADED_IO);

	if (sscanf(str, "=%d", &value) && value)
		toi_bkd.toi_action |= (1 << TOI_NO_MULTITHREADED_IO);

	return 1;
}

__setup("toi_no_multithreaded", toi_force_no_multithreaded_setup);

#ifdef CONFIG_KGDB
static int __init toi_post_resume_breakpoint_setup(char *str)
{
	int value;

	toi_bkd.toi_action &= ~(1 << TOI_POST_RESUME_BREAKPOINT);
	toi_bootflags_mask |= (1 << TOI_POST_RESUME_BREAKPOINT);
	if (sscanf(str, "=%d", &value) && value)
		toi_bkd.toi_action |= (1 << TOI_POST_RESUME_BREAKPOINT);

	return 1;
}

__setup("toi_post_resume_break", toi_post_resume_breakpoint_setup);
#endif

static int __init toi_disable_readahead_setup(char *str)
{
	int value;

	toi_bkd.toi_action &= ~(1 << TOI_NO_READAHEAD);
	toi_bootflags_mask |= (1 << TOI_NO_READAHEAD);
	if (sscanf(str, "=%d", &value) && value)
		toi_bkd.toi_action |= (1 << TOI_NO_READAHEAD);

	return 1;
}

__setup("toi_no_readahead", toi_disable_readahead_setup);
