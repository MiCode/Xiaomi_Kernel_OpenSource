/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/types.h>
#include <linux/errno.h>

#include "resource_mgt.h"
#include "sched_normal.h"
#include "scheduler.h"

int normal_queue_init(int type)
{
	int i = 0;
	struct apusys_res_table *tab = NULL;

	/* get resource table */
	tab = res_get_table(type);
	if (tab == NULL)
		return -ENODEV;

	/* init all priority linked list */
	for (i = 0; i < APUSYS_PRIORITY_MAX; i++)
		INIT_LIST_HEAD(&tab->normal_q.prio[i]);

	mdw_drv_debug("normal queue(%d) init done\n", type);

	return 0;
}

void normal_queue_destroy(int type)
{
	mdw_drv_debug("normal queue(%d) destroy done\n", type);
}

int normal_task_empty(int type)
{
	struct apusys_res_table *tab = NULL;

	/* get resource table */
	tab = res_get_table(type);
	if (tab == NULL)
		return false;

	/* check priority queue sc exist */
	return bitmap_empty(tab->normal_q.node_exist, APUSYS_PRIORITY_MAX);
}

int normal_task_insert(struct apusys_subcmd *sc)
{
	int prio = 0;
	struct apusys_res_table *tab = NULL;

	/* check argument */
	if (sc == NULL)
		return -EINVAL;

	/* get resource table */
	tab = res_get_table(sc->type);
	if (tab == NULL)
		return -ENODEV;

	/* add tail to normal queue */
	prio = sc->par_cmd->hdr->priority;
	list_add_tail(&sc->q_list, &tab->normal_q.prio[prio]);

	/* setup node exist */
	bitmap_set(tab->normal_q.node_exist, prio, 1);

	return 0;
}

int normal_task_remove(struct apusys_subcmd *sc)
{
	struct list_head *tmp = NULL, *list_ptr = NULL;
	struct apusys_subcmd *sc_node = NULL;
	struct apusys_res_table *tab = NULL;
	int i = 0;

	/* check argument */
	if (sc == NULL)
		return -EINVAL;

	/* get resource table */
	tab = res_get_table(sc->type);
	if (tab == NULL)
		return -ENODEV;

	/* query list to find mem in apusys user */
	for (i = 0; i < APUSYS_PRIORITY_MAX; i++) {
		list_for_each_safe(list_ptr, tmp, &tab->normal_q.prio[i]) {
			sc_node = list_entry(list_ptr,
				struct apusys_subcmd, q_list);
			if (sc_node != sc)
				continue;

			mdw_drv_debug("delete 0x%llx-#%d from q(%d/%d)\n",
				sc->par_cmd->cmd_id,
				sc->idx,
				sc->type,
				i);

			list_del(&sc->q_list);
			if (list_empty(&tab->normal_q.prio[i]))
				bitmap_clear(tab->normal_q.node_exist, i, 1);
			return 0;
		}
	}

	return -ENODATA;
}

struct apusys_subcmd *normal_task_pop(int type)
{
	int prio = 0;
	struct apusys_res_table *tab = NULL;
	struct apusys_subcmd *sc = NULL;

	/* get resource table */
	tab = res_get_table(type);
	if (tab == NULL)
		return NULL;

	/* check highest priority subcmd exist */
	prio = find_last_bit(tab->normal_q.node_exist,
			APUSYS_PRIORITY_MAX);
	if (prio >= APUSYS_PRIORITY_MAX) {
		mdw_drv_err("can't find cmd in type(%d) priority queue\n",
			type);
		return NULL;
	}

	/* get subcmd from priority queue */
	sc = list_first_entry(&tab->normal_q.prio[prio],
		struct apusys_subcmd, q_list);
	if (sc == NULL) {
		mdw_drv_err("get sc from nq(%d/%d) fail\n", type, prio);
		return NULL;
	}
	list_del(&sc->q_list);

	/* check priority queue empty */
	if (list_empty(&tab->normal_q.prio[prio]))
		bitmap_clear(tab->normal_q.node_exist, prio, 1);

	return sc;
}

int normal_task_start(struct apusys_subcmd *sc)
{
	return 0;
}

int normal_task_end(struct apusys_subcmd *sc)
{
	return 0;
}

