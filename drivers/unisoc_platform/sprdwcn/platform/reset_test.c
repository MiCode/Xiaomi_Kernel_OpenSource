// SPDX-License-Identifier: GPL-2.0-only
/*
 * reset_test.c - Unisoc platform driver
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include <linux/notifier.h>

#include "wcn_glb.h"

static int wcn_reset(struct notifier_block *this, unsigned long ev, void *ptr)
{
	WCN_INFO("%s: reset callback coming\n", __func__);

	return NOTIFY_DONE;
}

static struct notifier_block wcn_reset_block = {
	.notifier_call = wcn_reset,
};

int reset_test_init(void)
{
	atomic_notifier_chain_register(&wcn_reset_notifier_list,
				       &wcn_reset_block);

	return 0;
}

