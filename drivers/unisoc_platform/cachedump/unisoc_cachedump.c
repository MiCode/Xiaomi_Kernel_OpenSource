// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2013 Spreadtrum Communications Inc.
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

#define pr_fmt(fmt)  "cachedump: " fmt
#include <linux/kconfig.h>
#include <linux/kdebug.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/panic_notifier.h>
#include <linux/sprd_sip_svc.h>
#include "../drivers/unisoc_platform/sysdump/unisoc_sysdump.h"

#define SPRD_CACHEDUMP_NAME	"cachedump"
#define SPRD_CACHEDUMP_ADDR	0xbb200000
#define SPRD_CACHEDUMP_SIZE	0x251000

/*In order to avoid not using die notify*/
static int die_flag = 1;
static int is_panic = CONFIG_PANIC_ON_OOPS_VALUE;

static int cachedump_panic_event(struct notifier_block *self,
					unsigned long val,
					void *reason)
{
	int ret;
	uint8_t mesi = 0xe; /* bit0: I, bit1: S, bit2: E, bit3: M, eg:1110 MES*/
	uint8_t valid = 1; /* bit0: Invalid, bit1: Valid*/
	struct sprd_sip_svc_handle *svc_handle;

	if ((!is_panic) && (!in_interrupt()))
		return NOTIFY_DONE;

	if (die_flag)
		die_flag = 0;
	else
		return NOTIFY_DONE;

	pr_info("cachedump ------ in (%d)\n", smp_processor_id());
	svc_handle = sprd_sip_svc_get_handle();
	if (!svc_handle) {
		pr_err("%s: failed to get svc handle\n", __func__);
		return NOTIFY_DONE;
	}

	if (svc_handle->cachedump_ops.cachedump_func_api) {
		ret = svc_handle->cachedump_ops.cachedump_func_api(mesi, valid);
		if (ret)
			pr_err("Trigger cachedump_func_api fail\n");
	}
	return NOTIFY_DONE;
}

static struct notifier_block cachedump_panic_event_nb = {
	.notifier_call	= cachedump_panic_event,
	.priority	= INT_MAX,
};

static int __init cachedump_panic_event_init(void)
{

	register_die_notifier(&cachedump_panic_event_nb);
	atomic_notifier_chain_register(&panic_notifier_list,
						&cachedump_panic_event_nb);

	if (minidump_save_extend_information(SPRD_CACHEDUMP_NAME,
						SPRD_CACHEDUMP_ADDR,
						SPRD_CACHEDUMP_ADDR + SPRD_CACHEDUMP_SIZE))
		pr_err("%s minidump err\n", SPRD_CACHEDUMP_NAME);

	return 0;
}

static void __exit cachedump_panic_event_exit(void)
{
	minidump_change_extend_information(SPRD_CACHEDUMP_NAME, 0, 0);

	unregister_die_notifier(&cachedump_panic_event_nb);
	atomic_notifier_chain_unregister(&panic_notifier_list,
					&cachedump_panic_event_nb);
}

/* built-in for debug version */
module_init(cachedump_panic_event_init);
module_exit(cachedump_panic_event_exit);

MODULE_AUTHOR("Ruohan.Ma <ruohan.ma@unisoc.com>");
MODULE_DESCRIPTION("Trigger cachedump in panic notifier");
MODULE_LICENSE("GPL");
