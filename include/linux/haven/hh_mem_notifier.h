/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#ifndef __HH_MEM_NOTIFIER_H
#define __HH_MEM_NOTIFIER_H

#include <linux/err.h>
#include <linux/errno.h>
#include <linux/haven/hh_rm_drv.h>
#include <linux/types.h>

enum hh_mem_notifier_tag {
	HH_MEM_NOTIFIER_TAG_DISPLAY,
	HH_MEM_NOTIFIER_TAG_TOUCH,
	HH_MEM_NOTIFIER_TAG_MAX
};

typedef void (*hh_mem_notifier_handler)(enum hh_mem_notifier_tag tag,
					unsigned long notif_type,
					void *entry_data, void *notif_msg);

#if IS_ENABLED(CONFIG_HH_MEM_NOTIFIER)
void *hh_mem_notifier_register(enum hh_mem_notifier_tag tag,
			       hh_mem_notifier_handler notif_handler,
			       void *data);
void hh_mem_notifier_unregister(void *cookie);
#else
static void *hh_mem_notifier_register(enum hh_mem_notifier_tag tag,
				      hh_mem_notifier_handler notif_handler,
				      void *data)
{
	return ERR_PTR(-ENOTSUPP);
}

static void hh_mem_notifier_unregister(void *cookie)
{
}
#endif
#endif
