/*
  Copyright (C) 2010-2014 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
#ifndef _VTSS_TASK_MAP_H_
#define _VTSS_TASK_MAP_H_

#include "vtss_autoconf.h"

#include <linux/list.h>         /* for struct hlist_node */
#include <asm/atomic.h>         /* for atomic_t */

struct _vtss_task_map_item_t;
typedef void (vtss_task_map_func_t) (struct _vtss_task_map_item_t* item, void* args);

typedef struct _vtss_task_map_item_t
{
    struct hlist_node     hlist;
    pid_t                 key;
    atomic_t              usage;
    int                   in_list;
    vtss_task_map_func_t* dtor;
    char                  data[0]; /* placeholder for data */
} vtss_task_map_item_t;

/** find item in list and return with incremented usage */
vtss_task_map_item_t* vtss_task_map_get_item(pid_t key);
/** just decrement usage and destroy if usage become zero */
int  vtss_task_map_put_item(vtss_task_map_item_t* item);
/** allocate item + data but not insert it into list, usage is 1 */
vtss_task_map_item_t* vtss_task_map_alloc(pid_t key, size_t size, vtss_task_map_func_t* dtor, gfp_t flags);
/** add item into list with incremented usage */
int  vtss_task_map_add_item(vtss_task_map_item_t* item);
/** del item from list, decrement usage and destroy if usage become zero */
int  vtss_task_map_del_item(vtss_task_map_item_t* item);

/** call func for each item in list */
int  vtss_task_map_foreach(vtss_task_map_func_t* func, void* args);

/** init/fini */
int  vtss_task_map_init(void);
void vtss_task_map_fini(void);

#endif /* _VTSS_TASK_MAP_H_ */
