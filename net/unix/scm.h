/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef NET_UNIX_SCM_H
#define NET_UNIX_SCM_H

extern struct list_head gc_inflight_list;
extern spinlock_t unix_gc_lock;

int unix_attach_fds(struct scm_cookie *scm, struct sk_buff *skb);
void unix_detach_fds(struct scm_cookie *scm, struct sk_buff *skb);

#endif
