/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/types.h>
#ifndef _IPT_NATTYPE_H_target
#define _IPT_NATTYPE_H_target

#define NATTYPE_TIMEOUT 300

enum nattype_mode {
	MODE_DNAT,
	MODE_FORWARD_IN,
	MODE_FORWARD_OUT
};

enum nattype_type {
	TYPE_PORT_ADDRESS_RESTRICTED,
	TYPE_ENDPOINT_INDEPENDENT,
	TYPE_ADDRESS_RESTRICTED
};


struct ipt_nattype_info {
	__u16 mode;
	__u16 type;
	__u32 padding;
};

#endif /*_IPT_NATTYPE_H_target*/

