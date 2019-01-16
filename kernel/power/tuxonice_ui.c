/*
 * kernel/power/tuxonice_ui.c
 *
 * Copyright (C) 1998-2001 Gabor Kuti <seasons@fornax.hu>
 * Copyright (C) 1998,2001,2002 Pavel Machek <pavel@suse.cz>
 * Copyright (C) 2002-2003 Florent Chabaud <fchabaud@free.fr>
 * Copyright (C) 2002-2010 Nigel Cunningham (nigel at tuxonice net)
 *
 * This file is released under the GPLv2.
 *
 * Routines for TuxOnIce's user interface.
 *
 * The user interface code talks to a userspace program via a
 * netlink socket.
 *
 * The kernel side:
 * - starts the userui program;
 * - sends text messages and progress bar status;
 *
 * The user space side:
 * - passes messages regarding user requests (abort, toggle reboot etc)
 *
 */

#define __KERNEL_SYSCALLS__

#include <linux/reboot.h>

#include "tuxonice_sysfs.h"
#include "tuxonice_modules.h"
#include "tuxonice.h"
#include "tuxonice_ui.h"
#include "tuxonice_netlink.h"
#include "tuxonice_power_off.h"
#include "tuxonice_builtin.h"

static char local_printf_buf[1024];	/* Same as printk - should be safe */
struct ui_ops *toi_current_ui;
EXPORT_SYMBOL_GPL(toi_current_ui);

/**
 * toi_wait_for_keypress - Wait for keypress via userui or /dev/console.
 *
 * @timeout: Maximum time to wait.
 *
 * Wait for a keypress, either from userui or /dev/console if userui isn't
 * available. The non-userui path is particularly for at boot-time, prior
 * to userui being started, when we have an important warning to give to
 * the user.
 */
#if defined(CONFIG_VT) || defined(CONFIG_SERIAL_CONSOLE)
static char toi_wait_for_keypress(int timeout)
{
	if (toi_current_ui && toi_current_ui->wait_for_key(timeout))
		return ' ';

	return toi_wait_for_keypress_dev_console(timeout);
}
#endif

/* toi_early_boot_message()
 * Description:	Handle errors early in the process of booting.
 *		The user may press C to continue booting, perhaps
 *		invalidating the image,  or space to reboot.
 *		This works from either the serial console or normally
 *		attached keyboard.
 *
 *		Note that we come in here from init, while the kernel is
 *		locked. If we want to get events from the serial console,
 *		we need to temporarily unlock the kernel.
 *
 *		toi_early_boot_message may also be called post-boot.
 *		In this case, it simply printks the message and returns.
 *
 * Arguments:	int	Whether we are able to erase the image.
 *		int	default_answer. What to do when we timeout. This
 *			will normally be continue, but the user might
 *			provide command line options (__setup) to override
 *			particular cases.
 *		Char *. Pointer to a string explaining why we're moaning.
 */

#define say(message, a...) printk(KERN_EMERG message, ##a)

void toi_early_boot_message(int message_detail, int default_answer, char *warning_reason, ...)
{
	unsigned long orig_state = get_toi_state(), continue_req = 0;
#if defined(CONFIG_VT) || defined(CONFIG_SERIAL_CONSOLE)
	unsigned long orig_loglevel = console_loglevel;
	int can_ask = 1;
#else
	int can_ask = 0;
#endif

	va_list args;
	int printed_len;

	if (!toi_wait) {
		set_toi_state(TOI_CONTINUE_REQ);
		can_ask = 0;
	}

	if (warning_reason) {
		va_start(args, warning_reason);
		printed_len = vsnprintf(local_printf_buf,
					sizeof(local_printf_buf), warning_reason, args);
		va_end(args);
	}

	if (!test_toi_state(TOI_BOOT_TIME)) {
		printk("TuxOnIce: %s\n", local_printf_buf);
		return;
	}

	if (!can_ask) {
		continue_req = !!default_answer;
		goto post_ask;
	}
#if defined(CONFIG_VT) || defined(CONFIG_SERIAL_CONSOLE)
	console_loglevel = 7;

	say("=== TuxOnIce ===\n\n");
	if (warning_reason) {
		say("BIG FAT WARNING!! %s\n\n", local_printf_buf);
		switch (message_detail) {
		case 0:
			say("If you continue booting, note that any image WILL"
			    "NOT BE REMOVED.\nTuxOnIce is unable to do so "
			    "because the appropriate modules aren't\n"
			    "loaded. You should manually remove the image "
			    "to avoid any\npossibility of corrupting your "
			    "filesystem(s) later.\n");
			break;
		case 1:
			say("If you want to use the current TuxOnIce image, "
			    "reboot and try\nagain with the same kernel "
			    "that you hibernated from. If you want\n"
			    "to forget that image, continue and the image " "will be erased.\n");
			break;
		}
		say("Press SPACE to reboot or C to continue booting with " "this kernel\n\n");
		if (toi_wait > 0)
			say("Default action if you don't select one in %d "
			    "seconds is: %s.\n",
			    toi_wait,
			    default_answer == TOI_CONTINUE_REQ ? "continue booting" : "reboot");
	} else {
		say("BIG FAT WARNING!!\n\n"
		    "You have tried to resume from this image before.\n"
		    "If it failed once, it may well fail again.\n"
		    "Would you like to remove the image and boot "
		    "normally?\nThis will be equivalent to entering "
		    "noresume on the\nkernel command line.\n\n"
		    "Press SPACE to remove the image or C to continue " "resuming.\n\n");
		if (toi_wait > 0)
			say("Default action if you don't select one in %d "
			    "seconds is: %s.\n", toi_wait,
			    !!default_answer ? "continue resuming" : "remove the image");
	}
	console_loglevel = orig_loglevel;

	set_toi_state(TOI_SANITY_CHECK_PROMPT);
	clear_toi_state(TOI_CONTINUE_REQ);

	if (toi_wait_for_keypress(toi_wait) == 0)	/* We timed out */
		continue_req = !!default_answer;
	else
		continue_req = test_toi_state(TOI_CONTINUE_REQ);

#endif				/* CONFIG_VT or CONFIG_SERIAL_CONSOLE */

 post_ask:
	if ((warning_reason) && (!continue_req))
		kernel_restart(NULL);

	restore_toi_state(orig_state);
	if (continue_req)
		set_toi_state(TOI_CONTINUE_REQ);
}
EXPORT_SYMBOL_GPL(toi_early_boot_message);
#undef say

/*
 * User interface specific /sys/power/tuxonice entries.
 */

static struct toi_sysfs_data sysfs_params[] = {
#if defined(CONFIG_NET) && defined(CONFIG_SYSFS)
	SYSFS_INT("default_console_level", SYSFS_RW,
		  &toi_bkd.toi_default_console_level, 0, 7, 0, NULL),
	SYSFS_UL("debug_sections", SYSFS_RW, &toi_bkd.toi_debug_state, 0,
		 1 << 30, 0),
	SYSFS_BIT("log_everything", SYSFS_RW, &toi_bkd.toi_action, TOI_LOGALL,
		  0)
#endif
};

static struct toi_module_ops userui_ops = {
	.type = MISC_HIDDEN_MODULE,
	.name = "printk ui",
	.directory = "user_interface",
	.module = THIS_MODULE,
	.sysfs_data = sysfs_params,
	.num_sysfs_entries = sizeof(sysfs_params) / sizeof(struct toi_sysfs_data),
};

int toi_register_ui_ops(struct ui_ops *this_ui)
{
	if (toi_current_ui) {
		printk(KERN_INFO "Only one TuxOnIce user interface module can "
		       "be loaded at a time.");
		return -EBUSY;
	}

	toi_current_ui = this_ui;

	return 0;
}
EXPORT_SYMBOL_GPL(toi_register_ui_ops);

void toi_remove_ui_ops(struct ui_ops *this_ui)
{
	if (toi_current_ui != this_ui)
		return;

	toi_current_ui = NULL;
}
EXPORT_SYMBOL_GPL(toi_remove_ui_ops);

/* toi_console_sysfs_init
 * Description: Boot time initialisation for user interface.
 */

int toi_ui_init(void)
{
	return toi_register_module(&userui_ops);
}

void toi_ui_exit(void)
{
	toi_unregister_module(&userui_ops);
}
