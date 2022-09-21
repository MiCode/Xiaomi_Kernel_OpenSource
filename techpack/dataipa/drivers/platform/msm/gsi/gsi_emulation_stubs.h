/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */

#if !defined(_GSI_EMULATION_STUBS_H_)
# define _GSI_EMULATION_STUBS_H_

# include <asm/barrier.h>
# define __iowmb()       wmb() /* used in gsi.h */

#endif /* #if !defined(_GSI_EMULATION_STUBS_H_) */
