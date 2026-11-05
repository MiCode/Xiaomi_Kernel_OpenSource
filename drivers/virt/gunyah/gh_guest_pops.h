/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2024 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#ifndef _GH_GUEST_POPS_H
#define _GH_GUEST_POPS_H

#if IS_ENABLED(CONFIG_GH_GUEST_POPS)
int gh_guest_pops_init(void);
void gh_guest_pops_remove(void);
#else
static inline int gh_guest_pops_init(void) { return -ENODEV; }
static inline void gh_guest_pops_remove(void) {}
#endif
#endif /* _GH_GUEST_POPS_H */
