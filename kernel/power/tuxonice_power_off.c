/*
 * kernel/power/tuxonice_power_off.c
 *
 * Copyright (C) 2006-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * Support for powering down.
 */

#include <linux/device.h>
#include <linux/suspend.h>
#include <linux/mm.h>
#include <linux/pm.h>
#include <linux/reboot.h>
#include <linux/cpu.h>
#include <linux/console.h>
#include <linux/fs.h>
#include "tuxonice.h"
#include "tuxonice_ui.h"
#include "tuxonice_power_off.h"
#include "tuxonice_sysfs.h"
#include "tuxonice_modules.h"
#include "tuxonice_io.h"

unsigned long toi_poweroff_method;	/* 0 - Kernel power off */
EXPORT_SYMBOL_GPL(toi_poweroff_method);

static int wake_delay;
static char lid_state_file[256], wake_alarm_dir[256];
static struct file *lid_file, *alarm_file, *epoch_file;
static int post_wake_state = -1;

static int did_suspend_to_both;

int hybrid_sleep_mode(void)
{
	int retval = 0;
	if (toi_poweroff_method == 3 && did_suspend_to_both == 1)
		retval = 1;
	return retval;
}
EXPORT_SYMBOL_GPL(hybrid_sleep_mode);

/*
 * __toi_power_down
 * Functionality   : Powers down or reboots the computer once the image
 *                   has been written to disk.
 * Key Assumptions : Able to reboot/power down via code called or that
 *                   the warning emitted if the calls fail will be visible
 *                   to the user (ie printk resumes devices).
 */

static void __toi_power_down(int method)
{
	int error;

	toi_cond_pause(1, test_action_state(TOI_REBOOT) ? "Ready to reboot." : "Powering down.");

	if (test_result_state(TOI_ABORTED))
		goto out;

	if (test_action_state(TOI_REBOOT))
		kernel_restart(NULL);

	switch (method) {
	case 0:
		break;
	case 3:
		/*
		 * Re-read the overwritten part of pageset2 to make post-resume
		 * faster.
		 */
		if (read_pageset2(1))
			panic("Attempt to reload pagedir 2 failed. " "Try rebooting.");

		pm_prepare_console();

		error = pm_notifier_call_chain(PM_SUSPEND_PREPARE);
		if (!error) {
			pm_restore_gfp_mask();
			error = suspend_devices_and_enter(PM_SUSPEND_MEM);
			pm_restrict_gfp_mask();
			if (!error)
				did_suspend_to_both = 1;
		}
		pm_notifier_call_chain(PM_POST_SUSPEND);
		pm_restore_console();

		/* Success - we're now post-resume-from-ram */
		if (did_suspend_to_both)
			return;

		/* Failed to suspend to ram - do normal power off */
		break;
	case 4:
		/*
		 * If succeeds, doesn't return. If fails, do a simple
		 * powerdown.
		 */
		hibernation_platform_enter();
		break;
	case 5:
		/* Historic entry only now */
		break;
	}

	if (method && method != 5)
		toi_cond_pause(1, "Falling back to alternate power off method.");

	if (test_result_state(TOI_ABORTED))
		goto out;

	kernel_power_off();
	kernel_halt();
	toi_cond_pause(1, "Powerdown failed.");
	while (1)
		cpu_relax();

 out:
	if (read_pageset2(1))
		panic("Attempt to reload pagedir 2 failed. Try rebooting.");
	return;
}

#define CLOSE_FILE(file) \
	if (file) { \
		filp_close(file, NULL); file = NULL; \
	}

static void powerdown_cleanup(int toi_or_resume)
{
	if (!toi_or_resume)
		return;

	CLOSE_FILE(lid_file);
	CLOSE_FILE(alarm_file);
	CLOSE_FILE(epoch_file);
}

static void open_file(char *format, char *arg, struct file **var, int mode, char *desc)
{
	char buf[256];

	if (strlen(arg)) {
		sprintf(buf, format, arg);
		*var = filp_open(buf, mode, 0);
		if (IS_ERR(*var) || !*var) {
			printk(KERN_INFO "Failed to open %s file '%s' (%p).\n", desc, buf, *var);
			*var = NULL;
		}
	}
}

static int powerdown_init(int toi_or_resume)
{
	if (!toi_or_resume)
		return 0;

	did_suspend_to_both = 0;

	open_file("/proc/acpi/button/%s/state", lid_state_file, &lid_file, O_RDONLY, "lid");

	if (strlen(wake_alarm_dir)) {
		open_file("/sys/class/rtc/%s/wakealarm", wake_alarm_dir,
			  &alarm_file, O_WRONLY, "alarm");

		open_file("/sys/class/rtc/%s/since_epoch", wake_alarm_dir,
			  &epoch_file, O_RDONLY, "epoch");
	}

	return 0;
}

static int lid_closed(void)
{
	char array[25];
	ssize_t size;
	loff_t pos = 0;

	if (!lid_file)
		return 0;

	size = vfs_read(lid_file, (char __user *)array, 25, &pos);
	if ((int)size < 1) {
		printk(KERN_INFO "Failed to read lid state file (%d).\n", (int)size);
		return 0;
	}

	if (!strcmp(array, "state:      closed\n"))
		return 1;

	return 0;
}

static void write_alarm_file(int value)
{
	ssize_t size;
	char buf[40];
	loff_t pos = 0;

	if (!alarm_file)
		return;

	sprintf(buf, "%d\n", value);

	size = vfs_write(alarm_file, (char __user *)buf, strlen(buf), &pos);

	if (size < 0)
		printk(KERN_INFO "Error %d writing alarm value %s.\n", (int)size, buf);
}

/**
 * toi_check_resleep: See whether to powerdown again after waking.
 *
 * After waking, check whether we should powerdown again in a (usually
 * different) way. We only do this if the lid switch is still closed.
 */
void toi_check_resleep(void)
{
	/* We only return if we suspended to ram and woke. */
	if (lid_closed() && post_wake_state >= 0)
		__toi_power_down(post_wake_state);
}

void toi_power_down(void)
{
	if (alarm_file && wake_delay) {
		char array[25];
		loff_t pos = 0;
		size_t size = vfs_read(epoch_file, (char __user *)array, 25,
				       &pos);

		if (((int)size) < 1)
			printk(KERN_INFO "Failed to read epoch file (%d).\n", (int)size);
		else {
			unsigned long since_epoch;
			if (!strict_strtoul(array, 0, &since_epoch)) {
				/* Clear any wakeup time. */
				write_alarm_file(0);

				/* Set new wakeup time. */
				write_alarm_file(since_epoch + wake_delay);
			}
		}
	}

	__toi_power_down(toi_poweroff_method);

	toi_check_resleep();
}
EXPORT_SYMBOL_GPL(toi_power_down);

static struct toi_sysfs_data sysfs_params[] = {
#if defined(CONFIG_ACPI)
	SYSFS_STRING("lid_file", SYSFS_RW, lid_state_file, 256, 0, NULL),
	SYSFS_INT("wake_delay", SYSFS_RW, &wake_delay, 0, INT_MAX, 0, NULL),
	SYSFS_STRING("wake_alarm_dir", SYSFS_RW, wake_alarm_dir, 256, 0, NULL),
	SYSFS_INT("post_wake_state", SYSFS_RW, &post_wake_state, -1, 5, 0,
		  NULL),
#endif
	SYSFS_UL("powerdown_method", SYSFS_RW, &toi_poweroff_method, 0, 5, 0),
	SYSFS_INT("did_suspend_to_both", SYSFS_READONLY, &did_suspend_to_both,
		  0, 0, 0, NULL)
};

static struct toi_module_ops powerdown_ops = {
	.type = MISC_HIDDEN_MODULE,
	.name = "poweroff",
	.initialise = powerdown_init,
	.cleanup = powerdown_cleanup,
	.directory = "[ROOT]",
	.module = THIS_MODULE,
	.sysfs_data = sysfs_params,
	.num_sysfs_entries = sizeof(sysfs_params) / sizeof(struct toi_sysfs_data),
};

int toi_poweroff_init(void)
{
	return toi_register_module(&powerdown_ops);
}

void toi_poweroff_exit(void)
{
	toi_unregister_module(&powerdown_ops);
}
