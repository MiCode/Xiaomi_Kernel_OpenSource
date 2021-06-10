/*
 *Copyright (C) 2018 MediaTek Inc.
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
#ifndef DURASPEED_IND_H
#define DURASPEED_IND_H

#ifdef CONFIG_MTK_LOAD_TRACKER

extern int init_uload_ind(struct proc_dir_entry *parent);

#else

static inline int init_uload_ind(struct proc_dir_entry *parent)
{ return -EINVAL; }

#endif

#endif

