/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef DURASPEED_IND_H
#define DURASPEED_IND_H

#if IS_ENABLED(CONFIG_MTK_LOAD_TRACKER)

extern int init_uload_ind(struct proc_dir_entry *parent);

#else

static inline int init_uload_ind(struct proc_dir_entry *parent)
{ return -EINVAL; }

#endif

#endif

