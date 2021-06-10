/*
 * Copyright (c) 2015-2019, MICROTRUST Incorporated
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/cpu.h>
#include <imsg_log.h>
#include "switch_queue.h"
#include "backward_driver.h"

static struct list_head g_teei_task_link;
static struct list_head g_bdrv_task_link;
static struct mutex task_link_mutex;
static struct mutex bdrv_link_mutex;

int teei_init_task_link(void)
{
	mutex_init(&task_link_mutex);
	INIT_LIST_HEAD(&g_teei_task_link);

	mutex_init(&bdrv_link_mutex);
	INIT_LIST_HEAD(&g_bdrv_task_link);

	return 0;
}

int teei_add_to_task_link(struct list_head *entry)
{

	mutex_lock(&task_link_mutex);
	list_add_tail(entry, &g_teei_task_link);
	mutex_unlock(&task_link_mutex);

	return 0;
}

int is_teei_task_link_empty(void)
{
	int retVal = 0;

	mutex_lock(&task_link_mutex);
	retVal = list_empty(&g_teei_task_link);
	mutex_unlock(&task_link_mutex);

	return retVal;
}

struct task_entry_struct *teei_get_task_from_link(void)
{
	struct task_entry_struct *entry = NULL;
	int retVal = 0;

	retVal = is_teei_task_link_empty();
	if (retVal == 1)
		return NULL;

	mutex_lock(&task_link_mutex);
	entry = list_first_entry(&g_teei_task_link,
			struct task_entry_struct, c_link);

	list_del(&(entry->c_link));
	mutex_unlock(&task_link_mutex);

	return entry;
}

int teei_add_to_bdrv_link(struct list_head *entry)
{

	mutex_lock(&bdrv_link_mutex);
	list_add_tail(entry, &g_bdrv_task_link);
	mutex_unlock(&bdrv_link_mutex);

	return 0;
}

struct bdrv_work_struct *teei_get_bdrv_from_link(void)
{
	struct bdrv_work_struct *entry = NULL;
	int retVal = 0;

	retVal = list_empty(&g_bdrv_task_link);
	if (retVal == 1)
		return NULL;

	mutex_lock(&bdrv_link_mutex);
	entry = list_first_entry(&g_bdrv_task_link,
			struct bdrv_work_struct, c_link);

	list_del(&(entry->c_link));
	mutex_unlock(&bdrv_link_mutex);

	return entry;
}
