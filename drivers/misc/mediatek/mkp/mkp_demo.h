/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef __MKP_DEMO_H
#define __MKP_DEMO_H

#include "debug.h"
struct avc_sbuf_content {
	unsigned long avc_node;
	u32 ssid __aligned(8);
	u32 tsid __aligned(8);
	u16 tclass __aligned(8);
	u32 ae_allowed __aligned(8);
} __aligned(8);

struct _cred_sbuf_content {
	kuid_t uid;
	kgid_t gid;
	kuid_t euid;
	kgid_t egid;
	kuid_t fsuid;
	kgid_t fsgid;
	void *security;
};

struct cred_sbuf_content {
	union {
		struct _cred_sbuf_content csc;
		unsigned long args[4];
	};
};

extern struct rb_root mkp_rbtree;
extern rwlock_t mkp_rbtree_rwlock;
int __init mkp_demo_init(void);
#endif
