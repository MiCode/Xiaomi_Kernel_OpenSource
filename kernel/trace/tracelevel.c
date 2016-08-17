/*
 * kernel/trace/tracelevel.c
 *
 * Copyright (c) 2011, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 *
 */

#include <linux/ftrace_event.h>
#include <linux/list.h>
#include <linux/moduleparam.h>
#include <linux/mutex.h>
#include <linux/tracelevel.h>
#include <linux/vmalloc.h>

#include "trace.h"

#define TAG KERN_ERR "tracelevel: "

struct tracelevel_record {
	struct list_head list;
	char *name;
	int level;
};

static LIST_HEAD(tracelevel_list);

static bool started;
static unsigned int tracelevel_level = TRACELEVEL_DEFAULT;

static DEFINE_MUTEX(tracelevel_record_lock);

/* tracelevel_set_event sets a single event if set = 1, or
 * clears an event if set = 0.
 */
static int tracelevel_set_event(struct tracelevel_record *evt, bool set)
{
	if (trace_set_clr_event(NULL, evt->name, set) < 0) {
		printk(TAG "failed to set event %s\n", evt->name);
		return -EINVAL;
	}
	return 0;
}

/* Registers an event. If possible, it also sets it.
 * If not, we'll set it in tracelevel_init.
 */
int __tracelevel_register(char *name, unsigned int level)
{
	struct tracelevel_record *evt = (struct tracelevel_record *)
		vmalloc(sizeof(struct tracelevel_record));
	if (!evt) {
		printk(TAG "failed to allocate tracelevel_record for %s\n",
			name);
		return -ENOMEM;
	}

	evt->name = name;
	evt->level = level;

	mutex_lock(&tracelevel_record_lock);
	list_add(&evt->list, &tracelevel_list);
	mutex_unlock(&tracelevel_record_lock);

	if (level >= tracelevel_level && started)
		tracelevel_set_event(evt, 1);
	return 0;
}

/* tracelevel_set_level sets the global level, clears events
 * lower than that level, and enables events greater or equal.
 */
int tracelevel_set_level(int level)
{
	struct tracelevel_record *evt = NULL;

	if (level < 0 || level > TRACELEVEL_MAX)
		return -EINVAL;
	tracelevel_level = level;

	mutex_lock(&tracelevel_record_lock);
	list_for_each_entry(evt, &tracelevel_list, list) {
		if (evt->level >= level)
			tracelevel_set_event(evt, 1);
		else
			tracelevel_set_event(evt, 0);
	}
	mutex_unlock(&tracelevel_record_lock);
	return 0;
}

static int param_set_level(const char *val, const struct kernel_param *kp)
{
	int level, ret;
	ret = strict_strtol(val, 0, &level);
	if (ret < 0)
		return ret;
	return tracelevel_set_level(level);
}

static int param_get_level(char *buffer, const struct kernel_param *kp)
{
	return param_get_int(buffer, kp);
}

static struct kernel_param_ops tracelevel_level_ops = {
	.set = param_set_level,
	.get = param_get_level
};

module_param_cb(level, &tracelevel_level_ops, &tracelevel_level, 0644);

/* Turn on the tracing that has been registered thus far. */
static int __init tracelevel_init(void)
{
	int ret;
	started = true;

	/* Ring buffer is initialize to 1 page until the user sets a tracer.
	 * Since we're doing this manually, we need to ask for expanded buffer.
	 */
	ret = tracing_update_buffers();
	if (ret < 0)
		return ret;

	return tracelevel_set_level(tracelevel_level);
}

/* Tracing mechanism is set up during fs_initcall. */
fs_initcall_sync(tracelevel_init);
