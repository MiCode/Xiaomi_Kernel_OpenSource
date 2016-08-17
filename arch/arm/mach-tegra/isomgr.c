/*
 * arch/arm/mach-tegra/isomgr.c
 *
 * Copyright (c) 2012, NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#define pr_fmt(fmt)	"%s(): " fmt, __func__

#include <linux/delay.h>
#include <linux/types.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/completion.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/printk.h>
#include <linux/clk.h>
#include <asm/processor.h>
#include <mach/hardware.h>
#include <mach/isomgr.h>
#include <mach/mc.h>

#define ISOMGR_SYSFS_VERSION 0	/* increment on change */

struct isoclient_info {
	enum tegra_iso_client client;
	char *name;
};

static struct isoclient_info *isoclient_info;
static bool client_valid[TEGRA_ISO_CLIENT_COUNT];

static struct isoclient_info tegra_null_isoclients[] = {
	/* This must be last entry*/
	{
		.client = TEGRA_ISO_CLIENT_COUNT,
		.name = NULL,
	},
};

static struct isoclient_info tegra11x_isoclients[] = {
	{
		.client = TEGRA_ISO_CLIENT_DISP_0,
		.name = "disp_0",
	},
	{
		.client = TEGRA_ISO_CLIENT_DISP_1,
		.name = "disp_1",
	},
	{
		.client = TEGRA_ISO_CLIENT_VI_0,
		.name = "vi_0",
	},
	/* This must be last entry*/
	{
		.client = TEGRA_ISO_CLIENT_COUNT,
		.name = NULL,
	},
};

static struct isoclient_info *get_iso_client_info(void)
{
	enum tegra_chipid cid;
	struct isoclient_info *cinfo;

	cid = tegra_get_chipid();
	switch (cid) {
	case TEGRA_CHIPID_TEGRA11:
		cinfo = tegra11x_isoclients;
		break;
	default:
		cinfo = tegra_null_isoclients;
		break;
	}
	return cinfo;
}

static struct isomgr_client {
	bool busy;		/* already registered */
	s32 dedi_bw;		/* BW dedicated to this client	(KB/sec) */
	s32 rsvd_bw;		/* BW reserved for this client	(KB/sec) */
	s32 real_bw;		/* BW realized for this client	(KB/sec) */
	s32 lti;		/* Client spec'd Latency Tolerance (usec) */
	s32 lto;		/* MC calculated Latency Tolerance (usec) */
	s32 rsvd_mf;		/* reserved minimum freq in support of LT */
	s32 real_mf;		/* realized minimum freq in support of LT */
	tegra_isomgr_renegotiate renegotiate;	/* ask client to renegotiate */
	bool realize;		/* bw realization in progress */
	void *priv;		/* client driver's private data */
	struct completion cmpl;	/* so we can sleep waiting for delta BW */
#ifdef CONFIG_TEGRA_ISOMGR_SYSFS
	struct kobject *client_kobj;
	struct isomgr_client_attrs {
		struct kobj_attribute busy;
		struct kobj_attribute dedi_bw;
		struct kobj_attribute rsvd_bw;
		struct kobj_attribute real_bw;
		struct kobj_attribute need_bw;
		struct kobj_attribute lti;
		struct kobj_attribute lto;
		struct kobj_attribute rsvd_mf;
		struct kobj_attribute real_mf;
	} client_attrs;
#ifdef CONFIG_TEGRA_ISOMGR_DEBUG
	u32 __arg0;		/* args to inject stimulus */
	u32 __arg1;		/* args to inject stimulus */
	u32 __arg2;		/* args to inject stimulus */
	u32 reneg_seqnum;	/* renegotiation() callback count */
	u32 dvfs_latency;	/* retained value (in usec) */
	struct kobject *debug_kobj;
	struct isomgr_debug_attrs {
		struct kobj_attribute __arg0;
		struct kobj_attribute __arg1;
		struct kobj_attribute __arg2;
		struct kobj_attribute _register;
		struct kobj_attribute _unregister;
		struct kobj_attribute _reserve;
		struct kobj_attribute _realize;
		struct kobj_attribute reneg_seqnum;
		struct kobj_attribute dvfs_latency;
	} debug_attrs;
#endif /* CONFIG_TEGRA_ISOMGR_DEBUG */
#endif /* CONFIG_TEGRA_ISOMGR_SYSFS */
} isomgr_clients[TEGRA_ISO_CLIENT_COUNT];

static struct {
	struct mutex lock;		/* to lock ALL isomgr state */
	struct task_struct *task;	/* check reentrant/mismatched locks */
	struct clk *emc_clk;		/* emc clock handle */
	s32 bw_mf;			/* min freq to support aggregate bw */
	s32 lt_mf;			/* min freq to support worst LT */
	s32 iso_mf;			/* min freq to support ISO clients */
	s32 avail_bw;			/* globally available MC BW */
	s32 dedi_bw;			/* total BW 'dedicated' to clients */
	u32 max_iso_bw;			/* max ISO BW MC can accomodate */
	struct kobject *kobj;		/* for sysfs linkage */
} isomgr = {
	.max_iso_bw = CONFIG_TEGRA_ISOMGR_POOL_KB_PER_SEC,
	.avail_bw = CONFIG_TEGRA_ISOMGR_POOL_KB_PER_SEC,
};

/* get minimum MC frequency for client that can support this BW and LT */
static inline u32 mc_min_freq(u32 ubw, u32 ult) /* in KB/sec and usec */
{
	unsigned int min_freq = 1;

	/* ult==0 means ignore LT (effectively infinite) */
	if (ubw == 0)
		goto out;
	min_freq = tegra_emc_bw_to_freq_req(ubw);

	/* ISO clients can only expect 35% efficiency. */
	min_freq = (min_freq * 100  + 35 - 1) / 35;
out:
	return min_freq; /* return value in KHz*/
}

/* get dvfs switch latency for client requiring this frequency */
static inline u32 mc_dvfs_latency(u32 ufreq)
{
	return tegra_emc_dvfs_latency(ufreq); /* return value in usec */
}

static inline void isomgr_lock(void)
{
	BUG_ON(isomgr.task == current); /* disallow rentrance, avoid deadlock */
	mutex_lock(&isomgr.lock);
	isomgr.task = current;
}

static inline void isomgr_unlock(void)
{
	BUG_ON(isomgr.task != current); /* detact mismatched calls */
	isomgr.task = 0;
	mutex_unlock(&isomgr.lock);
}

static inline void isomgr_scavenge(int client)
{
	struct isomgr_client *cp;
	int i;

	/* ask flexible clients above dedicated BW levels to pitch in */
	for (i = 0; i < TEGRA_ISO_CLIENT_COUNT; ++i) {
		if (i == client)
			continue;
		cp = &isomgr_clients[i];
		if (cp->busy && cp->renegotiate)
			if (cp->real_bw > cp->dedi_bw)
				cp->renegotiate(cp->priv);
	}
}

static inline void isomgr_scatter(int client)
{
	struct isomgr_client *cp;
	int i;

	for (i = 0; i < TEGRA_ISO_CLIENT_COUNT; ++i) {
		if (i == client)
			continue;
		cp = &isomgr_clients[i];
		if (cp->busy && cp->renegotiate)
			cp->renegotiate(cp->priv); /* poke flexibles */
	}
}

/* register an ISO BW client */
tegra_isomgr_handle tegra_isomgr_register(enum tegra_iso_client client,
					  u32 udedi_bw,
					  tegra_isomgr_renegotiate renegotiate,
					  void *priv)
{
	s32 dedi_bw = udedi_bw;
	struct isomgr_client *cp = NULL;

	if (unlikely(client < 0 || client >= TEGRA_ISO_CLIENT_COUNT ||
		     !client_valid[client])) {
		pr_err("invalid client (%d)\n", client);
		goto fail;
	}

	isomgr_lock();

	cp = &isomgr_clients[client];
	if (unlikely(dedi_bw > isomgr.max_iso_bw - isomgr.dedi_bw)) {
		pr_err("invalid bandwidth (%u), client (%d)\n",
			dedi_bw, client);
		goto fail_unlock;
	}

	if (unlikely(cp->busy)) {
		pr_err("client (%d) already registered", client);
		goto fail_unlock;
	}
	cp->busy = true;

	cp->dedi_bw = dedi_bw;
	cp->renegotiate = renegotiate;
	cp->priv = priv;
	isomgr.dedi_bw += dedi_bw;

	isomgr_unlock();
	return (tegra_isomgr_handle)cp;

fail_unlock:
	isomgr_unlock();
fail:
	return NULL;
}
EXPORT_SYMBOL(tegra_isomgr_register);

/* unregister an ISO BW client */
void tegra_isomgr_unregister(tegra_isomgr_handle handle)
{
	struct isomgr_client *cp = (struct isomgr_client *)handle;
	int client = cp - &isomgr_clients[0];

	if (unlikely(!handle || client < 0 ||
		     client >= TEGRA_ISO_CLIENT_COUNT ||
		     !client_valid[client] || !cp->busy)) {
		pr_err("bad handle (%p)\n", handle);
		return;
	}

	/* relinquish lingering client resources */
	if (cp->rsvd_bw || cp->real_bw) {
		pr_debug("n c=%d, cp->rsvd_bw=%d, cp->real_bw=%d, avail_bw=%d",
			 client, cp->rsvd_bw, cp->real_bw, isomgr.avail_bw);
		tegra_isomgr_reserve(handle, 0, 0);
		tegra_isomgr_realize(handle);
	}

	/* unregister client and relinquish dedicated BW */
	isomgr_lock();
	isomgr.dedi_bw -= cp->dedi_bw;
	cp->dedi_bw = 0;
	cp->busy = false;
	cp->renegotiate = 0;
	cp->priv = 0;
	BUG_ON(cp->rsvd_bw);
	BUG_ON(cp->real_bw);
	pr_debug("x c=%d, cp->rsvd_bw=%d, cp->real_bw=%d, avail_bw=%d",
		 client, cp->rsvd_bw, cp->real_bw, isomgr.avail_bw);
	isomgr_unlock();
	isomgr_scatter(client); /* share the excess */
}
EXPORT_SYMBOL(tegra_isomgr_unregister);

/* reserve ISO BW for an ISO client.
 * returns dvfs latency thresh in usec.
 * return 0 indicates that reserve failed.
 */
u32 tegra_isomgr_reserve(tegra_isomgr_handle handle,
			 u32 ubw, u32 ult)
{
	s32 bw = ubw;
	u32 mf, dvfs_latency = 0;
	struct isomgr_client *cp = (struct isomgr_client *) handle;
	int client = cp - &isomgr_clients[0];

	if (unlikely(!handle || client < 0 ||
		     client >= TEGRA_ISO_CLIENT_COUNT ||
		     !client_valid[client] || !cp->busy)) {
		pr_err("bad handle (%p)\n", handle);
		return dvfs_latency;
	}

	isomgr_lock();
	pr_debug("n c=%d, cp->rsvd_bw=%d, cp->real_bw=%d, avail_bw=%d, bw=%d",
		 client, cp->rsvd_bw, cp->real_bw, isomgr.avail_bw, bw);
	if (unlikely(ubw >
		     (isomgr.max_iso_bw - isomgr.dedi_bw + cp->dedi_bw))) {
		pr_err("invalid BW (%u Kb/sec), client=%d\n", bw, client);
		goto out;
	}

	/* Look up MC's min freq that could satisfy requested BW and LT */
	mf = mc_min_freq(ubw, ult);
	if (unlikely(!mf)) {
		pr_err("invalid LT (%u usec), client=%d\n", ult, client);
		goto out;
	}
	/* Look up MC's dvfs latency at min freq */
	dvfs_latency = mc_dvfs_latency(mf);

	cp->lti = ult;		/* remember client spec'd LT (usec) */
	cp->lto = dvfs_latency;	/* remember MC calculated LT (usec) */
	cp->rsvd_mf = mf;	/* remember associated min freq */
	cp->rsvd_bw = bw;
out:
	pr_debug("x c=%d, cp->rsvd_bw=%d, cp->real_bw=%d, avail_bw=%d, bw=%d\n",
		client, cp->rsvd_bw, cp->real_bw, isomgr.avail_bw, bw);
	isomgr_unlock();
	return dvfs_latency;
}
EXPORT_SYMBOL(tegra_isomgr_reserve);

#define ISOMGR_DEBUG 1
#if ISOMGR_DEBUG
#define SANITY_CHECK_AVAIL_BW() { \
	int t = 0; \
	int idx = 0; \
	for (idx = 0; idx < TEGRA_ISO_CLIENT_COUNT; idx++) \
		t += isomgr_clients[idx].real_bw; \
	if (t + isomgr.avail_bw != isomgr.max_iso_bw) { \
		pr_err("bw mismatch, line=%d", __LINE__); \
		BUG(); \
	} \
}
#else
#define SANITY_CHECK_AVAIL_BW()
#endif

u32 tegra_isomgr_realize(tegra_isomgr_handle handle)
{
	int i;
	u32 rval = 0;
	struct isomgr_client *cp = (struct isomgr_client *) handle;
	int client = cp - &isomgr_clients[0];

	if (unlikely(!handle || client < 0 ||
		     client >= TEGRA_ISO_CLIENT_COUNT ||
		     !client_valid[client] || !cp->busy)) {
		pr_err("bad handle (%p) clinet (%d)\n", handle, client);
		return rval;
	}

retry:
	isomgr_lock();

	pr_debug("n c=%d, cp->rsvd_bw=%d, cp->real_bw=%d, avail_bw=%d",
		client, cp->rsvd_bw, cp->real_bw, isomgr.avail_bw);
	if (cp->realize)
		pr_debug("waiting for realize to finish, c=%d", client);
	while (cp->realize) {
		/* This is not expected to happen often. It would be extremely
		 * rare case. Handle it in a better way.
		 */
		isomgr_unlock();
		msleep(20);
		cpu_relax();
		goto retry;
	}
	cp->realize = true;

	isomgr.avail_bw += cp->real_bw;
	pr_debug("after release, c=%d avail_bw=%d", client, isomgr.avail_bw);
	cp->real_bw = 0;
	BUG_ON(isomgr.avail_bw > isomgr.max_iso_bw);

	if (cp->rsvd_bw <= isomgr.avail_bw) {
		isomgr.avail_bw -= cp->rsvd_bw;
		pr_debug("after alloc, c=%d avail_bw=%d",
			 client, isomgr.avail_bw);
		cp->real_bw = cp->rsvd_bw; /* reservation has been realized */
		cp->real_mf = cp->rsvd_mf; /* minimum frequency realized */
		BUG_ON(isomgr.avail_bw < 0);
		SANITY_CHECK_AVAIL_BW();
	} else {

		SANITY_CHECK_AVAIL_BW();
		cp->realize = false;
		isomgr_unlock();
		isomgr_scavenge(client);
		goto retry;
	}

	rval = (u32)cp->lto;

	/* determine worst case freq to satisfy LT */
	isomgr.lt_mf = 0;
	for (i = 0; i < TEGRA_ISO_CLIENT_COUNT; ++i) {
		if (isomgr_clients[i].busy)
			isomgr.lt_mf = max(isomgr.lt_mf,
					   isomgr_clients[i].real_mf);
	}

	/* determine worst case freq to satisfy BW */
	isomgr.bw_mf = mc_min_freq(isomgr.max_iso_bw - isomgr.avail_bw, 0);

	/* combine them and set the MC clock */
	isomgr.iso_mf = max(isomgr.lt_mf, isomgr.bw_mf);
	clk_set_rate(isomgr.emc_clk, isomgr.iso_mf * 1000);
	pr_debug("iso req clk=%dKHz", isomgr.iso_mf);

	cp->realize = false;
	pr_debug("x c=%d, cp->rsvd_bw=%d, cp->real_bw=%d, avail_bw=%d",
		client, cp->rsvd_bw, cp->real_bw, isomgr.avail_bw);
	isomgr_unlock();
	return rval;
}
EXPORT_SYMBOL(tegra_isomgr_realize);

#ifdef CONFIG_TEGRA_ISOMGR_SYSFS
static ssize_t isomgr_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

static const struct kobj_attribute bw_mf_attr =
	__ATTR(bw_mf, 0444, isomgr_show, 0);
static const struct kobj_attribute lt_mf_attr =
	__ATTR(lt_mf, 0444, isomgr_show, 0);
static const struct kobj_attribute iso_mf_attr =
	__ATTR(iso_mf, 0444, isomgr_show, 0);
static const struct kobj_attribute avail_bw_attr =
	__ATTR(avail_bw, 0444, isomgr_show, 0);
static const struct kobj_attribute max_iso_bw_attr =
	__ATTR(max_iso_bw, 0444, isomgr_show, 0);
static const struct kobj_attribute version_attr =
	__ATTR(version, 0444, isomgr_show, 0);

static const struct attribute *isomgr_attrs[] = {
	&bw_mf_attr.attr,
	&lt_mf_attr.attr,
	&iso_mf_attr.attr,
	&avail_bw_attr.attr,
	&max_iso_bw_attr.attr,
	&version_attr.attr,
	NULL
};

static ssize_t isomgr_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	ssize_t rval = 0;

	if (attr == &bw_mf_attr)
		rval = sprintf(buf, "%dKHz\n", isomgr.bw_mf);
	else if (attr == &lt_mf_attr)
		rval = sprintf(buf, "%dKHz\n", isomgr.lt_mf);
	else if (attr == &iso_mf_attr)
		rval = sprintf(buf, "%dKHz\n", isomgr.iso_mf);
	else if (attr == &avail_bw_attr)
		rval = sprintf(buf, "%dKB\n", isomgr.avail_bw);
	else if (attr == &max_iso_bw_attr)
		rval = sprintf(buf, "%dKB\n", isomgr.max_iso_bw);
	else if (attr == &version_attr)
		rval = sprintf(buf, "%d\n", ISOMGR_SYSFS_VERSION);
	return rval;
}

#ifdef CONFIG_TEGRA_ISOMGR_DEBUG
static void isomgr_client_reneg(void *priv)
{
	struct isomgr_client *cp = (struct isomgr_client *)priv;

	++cp->reneg_seqnum;
}

static ssize_t isomgr_debug_store(struct kobject *kobj,
	struct kobj_attribute *attr, const char *buf, size_t size)
{
	int client = ((char *)attr - (char *)isomgr_clients) /
			sizeof(struct isomgr_client);
	struct isomgr_client *cp =
		(struct isomgr_client *)&isomgr_clients[client];

	if (attr == &cp->debug_attrs.__arg0)
		sscanf(buf, "%d\n", &cp->__arg0);
	else if (attr == &cp->debug_attrs.__arg1)
		sscanf(buf, "%d\n", &cp->__arg1);
	else if (attr == &cp->debug_attrs.__arg2)
		sscanf(buf, "%d\n", &cp->__arg2);
	return size;
}

static ssize_t isomgr_debug_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int client = ((char *)attr - (char *)isomgr_clients) /
			sizeof(struct isomgr_client);
	struct isomgr_client *cp =
			(struct isomgr_client *)&isomgr_clients[client];
	ssize_t rval = 0;
	bool b;
	tegra_isomgr_handle h;

	if (attr == &cp->debug_attrs.__arg0)
		rval = sprintf(buf, "%d\n", cp->__arg0);
	else if (attr == &cp->debug_attrs.__arg1)
		rval = sprintf(buf, "%d\n", cp->__arg1);
	else if (attr == &cp->debug_attrs.__arg2)
		rval = sprintf(buf, "%d\n", cp->__arg2);
	else if (attr == &cp->debug_attrs._register) {
		h = tegra_isomgr_register(client,
					  cp->__arg0, /* dedi_bw */
					  cp->__arg1 ? isomgr_client_reneg : 0,
					  &isomgr_clients[client]);
		rval = sprintf(buf, "%p\n", h);
	} else if (attr == &cp->debug_attrs._unregister) {
		tegra_isomgr_unregister((tegra_isomgr_handle)cp);
		rval = sprintf(buf, "\n");
	} else if (attr == &cp->debug_attrs._reserve) {
		b = tegra_isomgr_reserve((tegra_isomgr_handle)cp,
					 (u32)cp->__arg0,
					 (u32)cp->__arg1);
		rval = sprintf(buf, "%d\n", b ? 1 : 0);
	} else if (attr == &cp->debug_attrs._realize) {
		tegra_isomgr_realize((tegra_isomgr_handle)cp);
		rval = sprintf(buf, "\n");
	} else if (attr == &cp->debug_attrs.reneg_seqnum)
		rval = sprintf(buf, "%d\n", cp->reneg_seqnum);
	else if (attr == &cp->debug_attrs.dvfs_latency)
		rval = sprintf(buf, "%d\n", cp->dvfs_latency);
	return rval;
}

static const struct isomgr_debug_attrs debug_attrs = {
	__ATTR(__arg0, 0644, isomgr_debug_show, isomgr_debug_store),
	__ATTR(__arg1, 0644, isomgr_debug_show, isomgr_debug_store),
	__ATTR(__arg2, 0644, isomgr_debug_show, isomgr_debug_store),
	__ATTR(_register, 0400, isomgr_debug_show, 0),
	__ATTR(_unregister, 0400, isomgr_debug_show, 0),
	__ATTR(_reserve, 0400, isomgr_debug_show, 0),
	__ATTR(_realize, 0400, isomgr_debug_show, 0),
	__ATTR(reneg_seqnum, 0444, isomgr_debug_show, 0),
	__ATTR(dvfs_latency, 0444, isomgr_debug_show, 0),
};

#define NDATTRS (sizeof(debug_attrs) / sizeof(struct kobj_attribute))
static const struct attribute *debug_attr_list[][NDATTRS+1] = {
#define DEBUG_ATTR(i)\
	{\
		&isomgr_clients[i].debug_attrs.__arg0.attr,\
		&isomgr_clients[i].debug_attrs.__arg1.attr,\
		&isomgr_clients[i].debug_attrs.__arg2.attr,\
		&isomgr_clients[i].debug_attrs._register.attr,\
		&isomgr_clients[i].debug_attrs._unregister.attr,\
		&isomgr_clients[i].debug_attrs._reserve.attr,\
		&isomgr_clients[i].debug_attrs._realize.attr,\
		&isomgr_clients[i].debug_attrs.reneg_seqnum.attr,\
		&isomgr_clients[i].debug_attrs.dvfs_latency.attr,\
		NULL\
	},
	DEBUG_ATTR(0)
	DEBUG_ATTR(1)
	DEBUG_ATTR(2)
	DEBUG_ATTR(3)
	DEBUG_ATTR(4)
	DEBUG_ATTR(5)
};

static void isomgr_create_debug(int client)
{
	struct isomgr_client *cp = &isomgr_clients[client];

	BUG_ON(!cp->client_kobj);
	BUG_ON(cp->debug_kobj);
	cp->debug_kobj = kobject_create_and_add("debug", cp->client_kobj);
	if (!cp->debug_kobj) {
		pr_err("failed to create sysfs debug client dir");
		return;
	}
	cp->debug_attrs = debug_attrs;
	if (sysfs_create_files(cp->debug_kobj, &debug_attr_list[client][0])) {
		pr_err("failed to create sysfs debug files");
		kobject_del(cp->debug_kobj);
		return;
	}
}
#else
static inline void isomgr_create_debug(int client) {}
#endif

static ssize_t isomgr_client_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int client = ((char *)attr - (char *)isomgr_clients) /
			sizeof(struct isomgr_client);
	struct isomgr_client *cp =
			(struct isomgr_client *)&isomgr_clients[client];
	ssize_t rval = 0;

	if (attr == &cp->client_attrs.busy)
		rval = sprintf(buf, "%d\n", cp->busy ? 1 : 0);
	else if (attr == &cp->client_attrs.dedi_bw)
		rval = sprintf(buf, "%dKB\n", cp->dedi_bw);
	else if (attr == &cp->client_attrs.rsvd_bw)
		rval = sprintf(buf, "%dKB\n", cp->rsvd_bw);
	else if (attr == &cp->client_attrs.real_bw)
		rval = sprintf(buf, "%dKB\n", cp->real_bw);
	else if (attr == &cp->client_attrs.lti)
		rval = sprintf(buf, "%dus\n", cp->lti);
	else if (attr == &cp->client_attrs.lto)
		rval = sprintf(buf, "%dus\n", cp->lto);
	else if (attr == &cp->client_attrs.rsvd_mf)
		rval = sprintf(buf, "%dKHz\n", cp->rsvd_mf);
	else if (attr == &cp->client_attrs.real_mf)
		rval = sprintf(buf, "%dKHz\n", cp->real_mf);
	return rval;
}

static const struct isomgr_client_attrs client_attrs = {
	__ATTR(busy,    0444, isomgr_client_show, 0),
	__ATTR(dedi_bw, 0444, isomgr_client_show, 0),
	__ATTR(rsvd_bw, 0444, isomgr_client_show, 0),
	__ATTR(real_bw, 0444, isomgr_client_show, 0),
	__ATTR(need_bw, 0444, isomgr_client_show, 0),
	__ATTR(lti,     0444, isomgr_client_show, 0),
	__ATTR(lto,     0444, isomgr_client_show, 0),
	__ATTR(rsvd_mf, 0444, isomgr_client_show, 0),
	__ATTR(real_mf, 0444, isomgr_client_show, 0),
};

#define NCATTRS (sizeof(client_attrs) / sizeof(struct kobj_attribute))
static const struct attribute *client_attr_list[][NCATTRS+1] = {
#define CLIENT_ATTR(i)\
	{\
		&isomgr_clients[i].client_attrs.busy.attr,\
		&isomgr_clients[i].client_attrs.dedi_bw.attr,\
		&isomgr_clients[i].client_attrs.rsvd_bw.attr,\
		&isomgr_clients[i].client_attrs.real_bw.attr,\
		&isomgr_clients[i].client_attrs.need_bw.attr,\
		&isomgr_clients[i].client_attrs.lti.attr,\
		&isomgr_clients[i].client_attrs.lto.attr,\
		&isomgr_clients[i].client_attrs.rsvd_mf.attr,\
		&isomgr_clients[i].client_attrs.real_mf.attr,\
		NULL\
	},
	CLIENT_ATTR(0)
	CLIENT_ATTR(1)
	CLIENT_ATTR(2)
	CLIENT_ATTR(3)
	CLIENT_ATTR(4)
	CLIENT_ATTR(5)
};

static void isomgr_create_client(int client, const char *name)
{
	struct isomgr_client *cp = &isomgr_clients[client];

	BUG_ON(!isomgr.kobj);
	BUG_ON(cp->client_kobj);
	cp->client_kobj = kobject_create_and_add(name, isomgr.kobj);
	if (!cp->client_kobj) {
		pr_err("failed to create sysfs client dir");
		return;
	}
	cp->client_attrs = client_attrs;
	if (sysfs_create_files(cp->client_kobj, &client_attr_list[client][0])) {
		pr_err("failed to create sysfs client files");
		kobject_del(cp->client_kobj);
		return;
	}
	isomgr_create_debug(client);
}

static void isomgr_create_sysfs(void)
{
	int i;

	BUG_ON(isomgr.kobj);
	isomgr.kobj = kobject_create_and_add("isomgr", kernel_kobj);
	if (!isomgr.kobj) {
		pr_err("failed to create kobject");
		return;
	}
	if (sysfs_create_files(isomgr.kobj, isomgr_attrs)) {
		pr_err("failed to create sysfs files");
		kobject_del(isomgr.kobj);
		isomgr.kobj = 0;
		return;
	}

	for (i = 0; i < TEGRA_ISO_CLIENT_COUNT; i++) {
		if (client_valid[i])
			isomgr_create_client(isoclient_info[i].client,
					     isoclient_info[i].name);
	}
}
#else
static inline void isomgr_create_sysfs(void) {};
#endif /* CONFIG_TEGRA_ISOMGR_SYSFS */

static int __init isomgr_init(void)
{
	int i;
	unsigned int max_emc_clk;
	unsigned int max_emc_bw;

	mutex_init(&isomgr.lock);
	isoclient_info = get_iso_client_info();

	for (i = 0; ; i++) {
		if (isoclient_info[i].name)
			client_valid[isoclient_info[i].client] = true;
		else
			break;
	}

	isomgr.emc_clk = clk_get_sys("iso", "emc");
	if (!isomgr.max_iso_bw) {
		max_emc_clk = clk_round_rate(isomgr.emc_clk, ULONG_MAX) / 1000;
		pr_debug("iso emc max clk=%dKHz", max_emc_clk);
		max_emc_bw = tegra_emc_freq_req_to_bw(max_emc_clk);
		/* ISO clients can use 35% of max emc bw. */
		isomgr.max_iso_bw = max_emc_bw * 35 / 100;
		pr_debug("max_iso_bw=%dKB", isomgr.max_iso_bw);
		isomgr.avail_bw = isomgr.max_iso_bw;
	}
	for (i = 0; i < TEGRA_ISO_CLIENT_COUNT; ++i)
		init_completion(&isomgr_clients[i].cmpl);
	isomgr_create_sysfs();
	return 0;
}
subsys_initcall(isomgr_init);
