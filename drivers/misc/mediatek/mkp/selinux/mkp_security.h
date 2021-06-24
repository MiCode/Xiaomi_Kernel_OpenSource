/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2003 Red Hat, Inc., James Morris <jmorris@redhat.com>
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef _SELINUX_MKP_SECURITY_H_
#define _SELINUX_MKP_SECURITY_H_

#include <linux/compiler.h>
#include <linux/dcache.h>
#include <linux/magic.h>
#include <linux/types.h>
#include <linux/rcupdate.h>
#include <linux/refcount.h>
#include <linux/workqueue.h>
#include "mkp_policycap.h"


struct selinux_avc;
struct selinux_policy;

struct selinux_state {
#ifdef CONFIG_SECURITY_SELINUX_DISABLE
	bool disabled;
#endif
#ifdef CONFIG_SECURITY_SELINUX_DEVELOP
	bool enforcing;
#endif
	bool checkreqprot;
	bool initialized;
	bool policycap[__POLICYDB_CAPABILITY_MAX];
	bool android_netlink_route;

	struct page *status_page;
	struct mutex status_lock;

	struct selinux_avc *avc;
	struct selinux_policy __rcu *policy;
	struct mutex policy_mutex;
} __randomize_layout;

struct av_decision {
	u32 allowed;
	u32 auditallow;
	u32 auditdeny;
	u32 seqno;
	u32 flags;
};

struct extended_perms_data {
	u32 p[8];
};

struct extended_perms_decision {
	u8 used;
	u8 driver;
	struct extended_perms_data *allowed;
	struct extended_perms_data *auditallow;
	struct extended_perms_data *dontaudit;
};

struct extended_perms {
	u16 len;	/* length associated decision chain */
	struct extended_perms_data drivers; /* flag drivers that are used */
};

struct avc_xperms_node {
	struct extended_perms xp;
	struct list_head xpd_head;
};
struct avc_entry {
	u32 ssid;
	u32 tsid;
	u16 tclass;
	struct av_decision avd;
	struct avc_xperms_node *xp_node;
};

struct mkp_avc_node {
	struct avc_entry ae;
	struct hlist_node list;
	struct rcu_head rhead;
};
#endif /* _SELINUX_MKP_SECURITY_H_ */
