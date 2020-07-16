/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */
#ifndef __HH_HCALL_COMMON_H
#define __HH_HCALL_COMMON_H

#include <linux/types.h>

struct hh_hcall_args {
	unsigned long arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7;
};

struct hh_hcall_resp {
	unsigned long resp0, resp1, resp2, resp3, resp4, resp5, resp6, resp7;
};

typedef u16 hh_hcall_fnid_t;

#endif
