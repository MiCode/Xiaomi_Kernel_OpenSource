/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>

#include "rs_base.h"
#include "rs_trace.h"
#include "rs_usage.h"
#include "rs_state.h"
#include "rs_index.h"

#define RS_SYSFS_DIR_NAME "resym"

struct kobject *rs_kobj;

static int __init rs_init(void)
{
	rs_kobj = kobject_create_and_add(RS_SYSFS_DIR_NAME, kernel_kobj);

	rs_index_init();
	rs_trace_init();
	rs_usage_init();
	rs_state_init();

	return 0;
}

static void __exit rs_exit(void)
{
	rs_usage_exit();
	rs_state_exit();

	kobject_put(rs_kobj);
	rs_kobj = NULL;
}

module_init(rs_init);
module_exit(rs_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Resource Symphony");
MODULE_AUTHOR("MediaTek Inc.");

