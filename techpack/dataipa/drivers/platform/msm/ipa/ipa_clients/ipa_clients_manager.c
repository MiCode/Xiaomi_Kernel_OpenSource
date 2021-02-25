// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/module.h>
#include "ipa_clients_i.h"
#include "ipa_i.h"

static int __init ipa_clients_manager_init(void)
{
	pr_info("IPA clients manager init\n");

	ipa3_usb_init();

	ipa_wdi3_register();

	ipa_gsb_register();

	ipa_uc_offload_register();

	ipa_mhi_register();

	ipa_wigig_register();

	ipa3_notify_clients_registered();

	return 0;
}
subsys_initcall(ipa_clients_manager_init);

static void __exit ipa_clients_manager_exit(void)
{
	pr_debug("IPA clients manger exit\n");

	ipa3_usb_exit();
}
module_exit(ipa_clients_manager_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA HW clients manager");
