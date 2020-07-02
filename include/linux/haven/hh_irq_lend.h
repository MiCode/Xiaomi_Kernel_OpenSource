/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 *
 */

#ifndef __HH_IRQ_LEND_H
#define __HH_IRQ_LEND_H

#include <linux/types.h>

#include "hh_common.h"
#include "hh_rm_drv.h"

enum hh_irq_label {
	HH_IRQ_LABEL_SDE,
	HH_IRQ_LABEL_MAX
};

typedef void (*hh_irq_handle_fn)(void *req, enum hh_irq_label label);

int hh_irq_lend(enum hh_irq_label label, enum hh_vm_names name,
		int hw_irq, hh_irq_handle_fn on_release, void *data);
int hh_irq_reclaim(enum hh_irq_label label);

int hh_irq_wait_for_lend(enum hh_irq_label label, enum hh_vm_names name,
			 hh_irq_handle_fn on_lend, void *data);
int hh_irq_accept(enum hh_irq_label label, int hw_irq);
int hh_irq_release(enum hh_irq_label label);

#endif
