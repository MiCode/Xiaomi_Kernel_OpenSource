/*
 * arch/arm/mach-tegra/isomgr.c
 *
 * Copyright (c) 2012-2014, NVIDIA CORPORATION. All rights reserved.
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

#define pr_fmt(fmt)	"%s %s(): " fmt, current->comm, __func__

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
#include <linux/err.h>
#include <linux/kref.h>
#include <linux/sched.h>
#include <linux/tegra-soc.h>
#include <asm/processor.h>
#include <asm/current.h>
#include <mach/isomgr.h>
#include <mach/mc.h>

#define CREATE_TRACE_POINTS
#include <trace/events/isomgr.h>

#define ISOMGR_SYSFS_VERSION 0	/* increment on change */
#define ISOMGR_DEBUG 1

#if ISOMGR_DEBUG
#define SANITY_CHECK_AVAIL_BW() { \
	int t = 0; \
	int idx = 0; \
	for (idx = 0; idx < TEGRA_ISO_CLIENT_COUNT; idx++) { \
		if (isomgr_clients[idx].real_bw >= \
		    isomgr_clients[idx].margin_bw) \
			t += isomgr_clients[idx].real_bw; \
		else \
			t += isomgr_clients[idx].margin_bw; \
	} \
	if (t + isomgr.avail_bw != isomgr.max_iso_bw) { \
		pr_err("bw mismatch, line=%d", __LINE__); \
		pr_err("t+isomgr.avail_bw=%d, isomgr.max_iso_bw=%d", \
			t + isomgr.avail_bw, isomgr.max_iso_bw); \
		BUG(); \
	} \
}
#else
#define SANITY_CHECK_AVAIL_BW()
#endif

#define VALIDATE_HANDLE() \
do { \
	if (unlikely(!cp || !is_client_valid(client) || \
		     cp->magic != ISOMGR_MAGIC)) { \
		pr_err("bad handle %p", handle); \
		goto validation_fail; \
	} \
} while (0)

#define VALIDATE_CLIENT() \
do { \
	if (unlikely(!is_client_valid(client))) { \
		pr_err("invalid client %d", client); \
		goto validation_fail; \
	} \
} while (0)

/* To allow test code take over control */
static bool test_mode;

static char *cname[] = {
	"disp_0",
	"disp_1",
	"vi_0",
	"vi_1",
	"isp_a",
	"isp_b",
	"bbc_0",
	"unknown"
};

struct isoclient_info {
	enum tegra_iso_client client;
	char *name;
	char *dev_name;
	char *emc_clk_name;
};

static struct isoclient_info *isoclient_info;
static bool client_valid[TEGRA_ISO_CLIENT_COUNT];
static int iso_bw_percentage = 100;

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
		.dev_name = "tegradc.0",
		.emc_clk_name = "emc",
	},
	{
		.client = TEGRA_ISO_CLIENT_DISP_1,
		.name = "disp_1",
		.dev_name = "tegradc.1",
		.emc_clk_name = "emc",
	},
	{
		.client = TEGRA_ISO_CLIENT_VI_0,
		.name = "vi_0",
		.dev_name = "vi",
		.emc_clk_name = "emc",
	},
	/* This must be last entry*/
	{
		.client = TEGRA_ISO_CLIENT_COUNT,
		.name = NULL,
	},
};

static struct isoclient_info tegra14x_isoclients[] = {
	{
		.client = TEGRA_ISO_CLIENT_DISP_0,
		.name = "disp_0",
		.dev_name = "tegradc.0",
		.emc_clk_name = "emc",
	},
	{
		.client = TEGRA_ISO_CLIENT_DISP_1,
		.name = "disp_1",
		.dev_name = "tegradc.1",
		.emc_clk_name = "emc",
	},
	{
		.client = TEGRA_ISO_CLIENT_VI_0,
		.name = "vi_0",
		.dev_name = "vi",
		.emc_clk_name = "emc",
	},
	{
		.client = TEGRA_ISO_CLIENT_BBC_0,
		.name = "bbc_0",
		.dev_name = "tegra_bb.0",
		.emc_clk_name = "emc_bw",
	},
	/* This must be last entry*/
	{
		.client = TEGRA_ISO_CLIENT_COUNT,
		.name = NULL,
	},
};

static struct isoclient_info tegra12x_isoclients[] = {
	{
		.client = TEGRA_ISO_CLIENT_DISP_0,
		.name = "disp_0",
		.dev_name = "tegradc.0",
		.emc_clk_name = "emc",
	},
	{
		.client = TEGRA_ISO_CLIENT_DISP_1,
		.name = "disp_1",
		.dev_name = "tegradc.1",
		.emc_clk_name = "emc",
	},
	{
		.client = TEGRA_ISO_CLIENT_VI_0,
		.name = "vi_0",
		.dev_name = "tegra_vi.0",
		.emc_clk_name = "emc",
	},
	{
		.client = TEGRA_ISO_CLIENT_VI_1,
		.name = "vi_1",
		.dev_name = "tegra_vi.1",
		.emc_clk_name = "emc",
	},
	{
		.client = TEGRA_ISO_CLIENT_ISP_A,
		.name = "isp_a",
		.dev_name = "tegra_isp.0",
		.emc_clk_name = "emc",
	},
	{
		.client = TEGRA_ISO_CLIENT_ISP_B,
		.name = "isp_b",
		.dev_name = "tegra_isp.1",
		.emc_clk_name = "emc",
	},
	/* This must be last entry*/
	{
		.client = TEGRA_ISO_CLIENT_COUNT,
		.name = NULL,
	},
};

static void isomgr_scatter(int client);

static struct isoclient_info *get_iso_client_info(void)
{
	enum tegra_chipid cid;
	struct isoclient_info *cinfo;

	cid = tegra_get_chipid();
	switch (cid) {
	case TEGRA_CHIPID_TEGRA11:
		cinfo = tegra11x_isoclients;
		iso_bw_percentage = 50;
		break;
	case TEGRA_CHIPID_TEGRA14:
		cinfo = tegra14x_isoclients;
		iso_bw_percentage = 50;
		break;
	case TEGRA_CHIPID_TEGRA12:
		cinfo = tegra12x_isoclients;
		iso_bw_percentage = 50;
		break;
	default:
		cinfo = tegra_null_isoclients;
		break;
	}
	return cinfo;
}

#define ISOMGR_MAGIC  0x150A1C
static struct isomgr_client {
	u32 magic;		/* magic to identify handle */
	struct kref kref;	/* ref counting */
	s32 dedi_bw;		/* BW dedicated to this client	(KB/sec) */
	s32 rsvd_bw;		/* BW reserved for this client	(KB/sec) */
	s32 real_bw;		/* BW realized for this client	(KB/sec) */
	s32 lti;		/* Client spec'd Latency Tolerance (usec) */
	s32 lto;		/* MC calculated Latency Tolerance (usec) */
	s32 rsvd_mf;		/* reserved minimum freq in support of LT */
	s32 real_mf;		/* realized minimum freq in support of LT */
	s32 real_mf_rq;		/* real_mf requested */
	tegra_isomgr_renegotiate renegotiate;	/* ask client to renegotiate */
	bool realize;		/* bw realization in progress */
	s32 sleep_bw;		/* sleeping for realize */
	s32 margin_bw;		/* BW set aside for this client	(KB/sec) */
	void *priv;		/* client driver's private data */
	struct completion cmpl;	/* so we can sleep waiting for delta BW */
	struct clk *emc_clk;	/* client emc clk for bw */
#ifdef CONFIG_TEGRA_ISOMGR_SYSFS
	struct kobject *client_kobj;
	struct isomgr_client_attrs {
		struct kobj_attribute dedi_bw;
		struct kobj_attribute rsvd_bw;
		struct kobj_attribute real_bw;
		struct kobj_attribute lti;
		struct kobj_attribute lto;
		struct kobj_attribute rsvd_mf;
		struct kobj_attribute real_mf;
		struct kobj_attribute sleep_bw;
		struct kobj_attribute margin_bw;
	} client_attrs;
#endif /* CONFIG_TEGRA_ISOMGR_SYSFS */
} isomgr_clients[TEGRA_ISO_CLIENT_COUNT];

static struct {
	struct mutex lock;		/* to lock ALL isomgr state */
	struct task_struct *task;	/* check reentrant/mismatched locks */
	struct clk *emc_clk;		/* isomgr emc clock for floor freq */
	s32 lt_mf;			/* min freq to support worst LT */
	s32 lt_mf_rq;			/* requested lt_mf */
	s32 avail_bw;			/* globally available MC BW */
	s32 dedi_bw;			/* total BW 'dedicated' to clients */
	s32 sleep_bw;			/* pending bw requirement */
	u32 max_iso_bw;			/* max ISO BW MC can accomodate */
	struct kobject *kobj;		/* for sysfs linkage */
} isomgr = {
	.max_iso_bw = CONFIG_TEGRA_ISOMGR_POOL_KB_PER_SEC,
	.avail_bw = CONFIG_TEGRA_ISOMGR_POOL_KB_PER_SEC,
};

/* get minimum MC frequency for client that can support this BW and LT */
static inline u32 mc_min_freq(u32 ubw, u32 ult) /* in KB/sec and usec */
{
	unsigned int min_freq = 0;

	/* ult==0 means ignore LT (effectively infinite) */
	if (ubw == 0)
		goto out;
	min_freq = tegra_emc_bw_to_freq_req(ubw);
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
	BUG_ON(isomgr.task != current); /* detect mismatched calls */
	isomgr.task = 0;
	mutex_unlock(&isomgr.lock);
}

/* call with isomgr_lock held. */
static void update_mc_clock(void)
{
	int i;

	BUG_ON(mutex_trylock(&isomgr.lock));
	/* determine worst case freq to satisfy LT */
	isomgr.lt_mf = 0;
	for (i = 0; i < TEGRA_ISO_CLIENT_COUNT; i++)
		isomgr.lt_mf = max(isomgr.lt_mf, isomgr_clients[i].real_mf);

	/* request the floor freq to satisfy LT */
	if (isomgr.lt_mf_rq != isomgr.lt_mf &&
	    !clk_set_rate(isomgr.emc_clk, isomgr.lt_mf * 1000)) {

		if (isomgr.lt_mf_rq == 0)
			clk_enable(isomgr.emc_clk);
		isomgr.lt_mf_rq = isomgr.lt_mf;
		if (isomgr.lt_mf_rq == 0)
			clk_disable(isomgr.emc_clk);
	}

	for (i = 0; i < TEGRA_ISO_CLIENT_COUNT; i++) {
		if (isomgr_clients[i].real_mf != isomgr_clients[i].real_mf_rq &&
		    !clk_set_rate(isomgr_clients[i].emc_clk,
			isomgr_clients[i].real_mf * 1000)) {

			if (isomgr_clients[i].real_mf_rq == 0)
				clk_enable(isomgr_clients[i].emc_clk);
			isomgr_clients[i].real_mf_rq = isomgr_clients[i].real_mf;
			if (isomgr_clients[i].real_mf_rq == 0)
				clk_disable(isomgr_clients[i].emc_clk);
		}
	}
}

static void purge_isomgr_client(struct isomgr_client *cp)
{
	cp->magic = 0;
	atomic_set(&cp->kref.refcount, 0);
	cp->dedi_bw = 0;
	cp->rsvd_bw = 0;
	cp->real_bw = 0;
	cp->rsvd_mf = 0;
	cp->real_mf = 0;
	cp->renegotiate = 0;
	cp->realize = false;
	cp->priv = NULL;
	cp->sleep_bw = 0;
	cp->margin_bw = 0;
}

static void unregister_iso_client(struct kref *kref)
{
	struct isomgr_client *cp = container_of(kref,
					struct isomgr_client, kref);
	int client = cp - &isomgr_clients[0];

	trace_tegra_isomgr_unregister_iso_client(cname[client], "enter");
	isomgr_lock();
	BUG_ON(cp->realize);

	if (cp->real_bw > cp->margin_bw)
		isomgr.avail_bw += cp->real_bw;
	else
		isomgr.avail_bw += cp->margin_bw;
	isomgr.dedi_bw -= cp->dedi_bw;
	purge_isomgr_client(cp);
	update_mc_clock();
	isomgr_unlock();

	isomgr_scatter(client); /* share the excess */
	trace_tegra_isomgr_unregister_iso_client(cname[client], "exit");
}

static void isomgr_scatter(int client)
{
	struct isomgr_client *cp;
	int i;

	trace_tegra_isomgr_scatter(client, 0, cname[client], "enter");
	for (i = 0; i < TEGRA_ISO_CLIENT_COUNT; ++i) {
		cp = &isomgr_clients[i];
		if (unlikely(!atomic_inc_not_zero(&cp->kref.refcount)))
			continue;
		if (cp->renegotiate && i != client) {
			int avail_bw = cp->real_bw + isomgr.avail_bw -
					isomgr.sleep_bw;
			if (avail_bw > cp->dedi_bw) {
				trace_tegra_isomgr_scatter(client, 0,
					cname[client], "scatter");
				/* poke flexibles */
				cp->renegotiate(cp->priv, avail_bw);
			}
		}
		kref_put(&cp->kref, unregister_iso_client);
	}
	trace_tegra_isomgr_scatter(client, 0, cname[client], "exit");
}

static void isomgr_scavenge(enum tegra_iso_client client)
{
	struct isomgr_client *cp;
	int i;

	trace_tegra_isomgr_scavenge(client, 0, cname[client], "enter");
	/* ask flexible clients above dedicated BW levels to pitch in */
	for (i = 0; i < TEGRA_ISO_CLIENT_COUNT; ++i) {
		cp = &isomgr_clients[i];
		if (unlikely(!atomic_inc_not_zero(&cp->kref.refcount)))
			continue;
		if (cp->renegotiate)
			if (cp->real_bw > cp->dedi_bw && i != client &&
			    !cp->realize) {
				int avail_bw = cp->real_bw + isomgr.avail_bw -
						isomgr.sleep_bw;
				avail_bw = avail_bw < cp->dedi_bw ?
					   cp->dedi_bw : avail_bw;
				trace_tegra_isomgr_scavenge(client, 0,
					cname[client], "renego");
				cp->renegotiate(cp->priv, avail_bw);
			}
		kref_put(&cp->kref, unregister_iso_client);
	}
	trace_tegra_isomgr_scavenge(client, 0, cname[client], "exit");
}

static bool is_client_valid(enum tegra_iso_client client)
{
	if (unlikely(client < 0 || client >= TEGRA_ISO_CLIENT_COUNT ||
		     !client_valid[client]))
		return false;
	return true;
}

static tegra_isomgr_handle __tegra_isomgr_register(
			enum tegra_iso_client client, u32 udedi_bw,
			tegra_isomgr_renegotiate renegotiate, void *priv)
{
	s32 dedi_bw = udedi_bw;
	struct isomgr_client *cp = NULL;

	VALIDATE_CLIENT();
	trace_tegra_isomgr_register(client, dedi_bw, renegotiate,
		priv, cname[client], "enter");

	if (unlikely(!udedi_bw && !renegotiate))
		goto validation_fail;

	isomgr_lock();
	cp = &isomgr_clients[client];

	if (unlikely(atomic_read(&cp->kref.refcount)))
		goto fail_unlock;

	if (unlikely(dedi_bw > isomgr.max_iso_bw - isomgr.dedi_bw)) {
#ifdef CONFIG_TEGRA_ISOMGR_MAX_ISO_BW_QUIRK
		int i;
		WARN(1, "max_iso_bw is relaxed to %dKB from %dKB",
			dedi_bw + isomgr.dedi_bw, isomgr.max_iso_bw);
		isomgr.avail_bw += dedi_bw + isomgr.dedi_bw -
				   isomgr.max_iso_bw;
		isomgr.max_iso_bw = dedi_bw + isomgr.dedi_bw;
		pr_info("ISO BW usage:");
		for (i = 0; i < TEGRA_ISO_CLIENT_COUNT; i++) {
			if (!client_valid[i])
				continue;
			pr_info("client=%s, iso dedi bw=%dKB",
				cname[i],
				(client == i) ? dedi_bw :
				isomgr_clients[i].dedi_bw);
		}
		pr_info("revisit BW usage of iso clients");
#else
		pr_err("iso bandwidth %uKB is not available, client %s\n",
			dedi_bw, cname[client]);
		goto fail_unlock;
#endif
	}

	purge_isomgr_client(cp);
	cp->magic = ISOMGR_MAGIC;
	kref_init(&cp->kref);
	cp->dedi_bw = dedi_bw;
	cp->renegotiate = renegotiate;
	cp->priv = priv;
	isomgr.dedi_bw += dedi_bw;

	isomgr_unlock();
	trace_tegra_isomgr_register(client, dedi_bw, renegotiate,
		priv, cname[client], "exit");
	return (tegra_isomgr_handle)cp;

fail_unlock:
	isomgr_unlock();
validation_fail:
	trace_tegra_isomgr_register(client, dedi_bw, renegotiate,
		priv, cname[client], "inv_args_exit");
	return ERR_PTR(-EINVAL);
}

/**
 * tegra_isomgr_register - register an ISO BW client.
 *
 * @client	client to register as an ISO client.
 * @udedi_bw	minimum bw client can work at. This bw is guarnteed to be
 *		available for client when ever client need it. Client can
 *		always request for more bw and client can get it based on
 *		availability of bw in the system. udedi_bw is specified in KB.
 * @renegotiate	callback function to be called to renegotiate for bw.
 *		client with no renegotiate callback provided can't allocate
 *		bw more than udedi_bw.
 *		Client with renegotiate callback can allocate more than
 *		udedi_bw and release it during renegotiate callback, when
 *		other clients in the system need their bw back.
 *		renegotiate callback is called in two cases. 1. The isomgr
 *		has excess bw, checking client to see if they need more bw.
 *		2. The isomgr is out of bw and other clients need their udedi_bw
 *		back. In this case, the client, which is using higher bw need to
 *		release the bw and fallback to low(udedi_bw) bw use case.
 * @priv	pointer to renegotiate callback function.
 *
 * @return	returns valid handle on successful registration.
 * @retval	-EINVAL invalid arguments passed.
 */
tegra_isomgr_handle tegra_isomgr_register(enum tegra_iso_client client,
					  u32 udedi_bw,
					  tegra_isomgr_renegotiate renegotiate,
					  void *priv)
{
	if (test_mode)
		return (tegra_isomgr_handle)1;
	return __tegra_isomgr_register(client, udedi_bw, renegotiate, priv);
}
EXPORT_SYMBOL(tegra_isomgr_register);

static void __tegra_isomgr_unregister(tegra_isomgr_handle handle)
{
	struct isomgr_client *cp = (struct isomgr_client *)handle;
	int client = cp - &isomgr_clients[0];

	VALIDATE_HANDLE();
	trace_tegra_isomgr_unregister(handle, cname[client]);
	kref_put(&cp->kref, unregister_iso_client);
validation_fail:
	return;
}

/**
 * tegra_isomgr_unregister - unregister an ISO BW client.
 *
 * @handle	handle acquired during tegra_isomgr_register.
 */
void tegra_isomgr_unregister(tegra_isomgr_handle handle)
{
	if (test_mode)
		return;
	__tegra_isomgr_unregister(handle);
}
EXPORT_SYMBOL(tegra_isomgr_unregister);

static u32 __tegra_isomgr_reserve(tegra_isomgr_handle handle,
			 u32 ubw, u32 ult)
{
	s32 bw = ubw;
	u32 mf, dvfs_latency = 0;
	struct isomgr_client *cp = (struct isomgr_client *) handle;
	int client = cp - &isomgr_clients[0];

	VALIDATE_HANDLE();

	isomgr_lock();
	if (unlikely(!atomic_inc_not_zero(&cp->kref.refcount)))
		goto handle_unregistered;

	if (cp->rsvd_bw == ubw && cp->lti == ult) {
		kref_put(&cp->kref, unregister_iso_client);
		isomgr_unlock();
		return cp->lto;
	}

	trace_tegra_isomgr_reserve(handle, ubw, ult, cname[client], "enter");

	if (unlikely(cp->realize))
		goto out;

	if (bw <= cp->margin_bw)
		goto skip_bw_check;

	if (unlikely(!cp->renegotiate && bw > cp->dedi_bw))
		goto out;

	if (bw > cp->dedi_bw &&
	    bw > isomgr.avail_bw + cp->real_bw - isomgr.sleep_bw)
		goto out;

skip_bw_check:
	/* Look up MC's min freq that could satisfy requested BW and LT */
	mf = mc_min_freq(ubw, ult);
	/* Look up MC's dvfs latency at min freq */
	dvfs_latency = mc_dvfs_latency(mf);

	cp->lti = ult;		/* remember client spec'd LT (usec) */
	cp->lto = dvfs_latency;	/* remember MC calculated LT (usec) */
	cp->rsvd_mf = mf;	/* remember associated min freq */
	cp->rsvd_bw = bw;
out:
	kref_put(&cp->kref, unregister_iso_client);
	isomgr_unlock();
	trace_tegra_isomgr_reserve(handle, ubw, ult, cname[client],
		dvfs_latency ? "exit" : "rsrv_fail_exit");
	return dvfs_latency;
handle_unregistered:
	isomgr_unlock();
	trace_tegra_isomgr_reserve(handle, ubw, ult,
		cname[client], "inv_handle_exit");
	return dvfs_latency;
validation_fail:
	trace_tegra_isomgr_reserve(handle, ubw, ult, "unk", "inv_handle_exit");
	return dvfs_latency;
}

/**
 * tegra_isomgr_reserve - reserve bw for the ISO client.
 *
 * @handle	handle acquired during tegra_isomgr_register.
 * @ubw		bandwidth in KBps.
 * @ult		latency that can be tolerated by client in usec.
 *
 * returns dvfs latency thresh in usec.
 * return 0 indicates that reserve failed.
 */
u32 tegra_isomgr_reserve(tegra_isomgr_handle handle,
			 u32 ubw, u32 ult)
{
	if (test_mode)
		return 1;
	return __tegra_isomgr_reserve(handle, ubw, ult);
}
EXPORT_SYMBOL(tegra_isomgr_reserve);

static u32 __tegra_isomgr_realize(tegra_isomgr_handle handle)
{
	bool retry = false;
	u32 dvfs_latency = 0;
	s32 delta_bw = 0;
	struct isomgr_client *cp = (struct isomgr_client *) handle;
	int client = cp - &isomgr_clients[0];

	VALIDATE_HANDLE();

retry:
	isomgr_lock();
	if (unlikely(!atomic_inc_not_zero(&cp->kref.refcount)))
		goto handle_unregistered;

	if (cp->rsvd_bw == cp->real_bw && cp->rsvd_mf == cp->real_mf) {
		kref_put(&cp->kref, unregister_iso_client);
		isomgr_unlock();
		return cp->lto;
	}

	if (!retry)
		trace_tegra_isomgr_realize(handle, cname[client], "enter");
	if (cp->margin_bw < cp->real_bw)
		isomgr.avail_bw += cp->real_bw - cp->margin_bw;
	cp->real_bw = 0;
	cp->realize = true;
	BUG_ON(isomgr.avail_bw > isomgr.max_iso_bw);

	if (cp->rsvd_bw <= cp->margin_bw) {
		BUG_ON(cp->sleep_bw);
		cp->real_bw = cp->rsvd_bw; /* reservation has been realized */
		cp->real_mf = cp->rsvd_mf; /* minimum frequency realized */
	} else if (cp->rsvd_bw <= isomgr.avail_bw + cp->margin_bw) {
		delta_bw = cp->rsvd_bw - cp->margin_bw;
		isomgr.avail_bw -= delta_bw;
		cp->real_bw = cp->rsvd_bw; /* reservation has been realized */
		cp->real_mf = cp->rsvd_mf; /* minimum frequency realized */
		if (cp->sleep_bw) {
			isomgr.sleep_bw -= delta_bw;
			cp->sleep_bw -= delta_bw;
			BUG_ON(cp->sleep_bw);
		}
		BUG_ON(isomgr.avail_bw < 0);
		SANITY_CHECK_AVAIL_BW();
	} else {
		/* Protection from first scavenge failure */
		if (!cp->sleep_bw) {
			delta_bw = cp->rsvd_bw - cp->margin_bw;
			SANITY_CHECK_AVAIL_BW();
			isomgr.sleep_bw += delta_bw;
			BUG_ON(cp->sleep_bw);
			cp->sleep_bw += delta_bw;
		}
		kref_put(&cp->kref, unregister_iso_client);
		isomgr_unlock();
		isomgr_scavenge(client);
		retry = true;
		goto retry;
	}

	dvfs_latency = (u32)cp->lto;
	cp->realize = false;
	update_mc_clock();

	kref_put(&cp->kref, unregister_iso_client);
	isomgr_unlock();
	trace_tegra_isomgr_realize(handle, cname[client],
		dvfs_latency ? "exit" : "real_fail_exit");
	return dvfs_latency;
handle_unregistered:
	isomgr_unlock();
	trace_tegra_isomgr_realize(handle, cname[client], "inv_handle_exit");
	return dvfs_latency;
validation_fail:
	trace_tegra_isomgr_realize(handle, "unk", "inv_handle_exit");
	return dvfs_latency;
}

/**
 * tegra_isomgr_realize - realize the bw reserved by client.
 *
 * @handle	handle acquired during tegra_isomgr_register.
 *
 * returns dvfs latency thresh in usec.
 * return 0 indicates that realize failed.
 */
u32 tegra_isomgr_realize(tegra_isomgr_handle handle)
{
	if (test_mode)
		return 1;
	return __tegra_isomgr_realize(handle);
}
EXPORT_SYMBOL(tegra_isomgr_realize);

static int __tegra_isomgr_set_margin(enum tegra_iso_client client,
					u32 bw, bool wait)
{
	int ret = -EINVAL;
	s32 high_bw;
	struct isomgr_client *cp = NULL;

	trace_tegra_isomgr_set_margin(client, bw, wait, "enter");
	VALIDATE_CLIENT();

retry:
	isomgr_lock();
	cp = &isomgr_clients[client];
	if (unlikely(!atomic_inc_not_zero(&cp->kref.refcount)))
		goto handle_unregistered;

	if (bw > cp->dedi_bw)
		goto out;

	if (bw <= cp->real_bw) {
		if (cp->margin_bw > cp->real_bw)
			isomgr.avail_bw += cp->margin_bw - cp->real_bw;
		cp->margin_bw = bw;
	} else if (bw <= cp->margin_bw) {
		BUG_ON(cp->margin_bw > cp->real_bw);
		isomgr.avail_bw += cp->margin_bw - bw;
		cp->margin_bw = bw;
		BUG_ON(cp->margin_bw > cp->real_bw);
	} else if (bw > cp->margin_bw) {
		high_bw = (cp->margin_bw > cp->real_bw) ?
				cp->margin_bw : cp->real_bw;
		if (bw - high_bw <= isomgr.avail_bw - isomgr.sleep_bw) {
			isomgr.avail_bw -= bw - high_bw;
			cp->margin_bw = bw;
		} else {
			if (wait) {
				kref_put(&cp->kref, unregister_iso_client);
				isomgr_unlock();
				wait = false;
				isomgr_scavenge(client);
				goto retry;
			}
			ret = -ENOMEM;
			goto out;
		}
	}
	ret = 0;
out:
	kref_put(&cp->kref, unregister_iso_client);
	isomgr_unlock();
	trace_tegra_isomgr_set_margin(client, bw, wait,
					ret ? "fail_exit" : "exit");
	return ret;
handle_unregistered:
	isomgr_unlock();
validation_fail:
	trace_tegra_isomgr_set_margin(client, bw, wait, "inv_arg_fail");
	return ret;
}

/**
 * This sets bw aside for the client specified.
 * This bw can never be used for other clients needs.
 * margin bw, if not zero, should always be greater than equal to
 * reserved/realized bw.
 *
 * @client client
 * @bw bw margin KB.
 * @wait if true and bw is not available, it would wait till bw is available.
 *	  if false, it would return immediately with success or failure.
 *
 * @retval  0 operation is successful.
 * @retval -ENOMEM Iso Bw requested is not avialable.
 * @retval -EINVAL Invalid arguments, bw is less than reserved/realized bw.
 */
int tegra_isomgr_set_margin(enum tegra_iso_client client, u32 bw, bool wait)
{
	if (test_mode)
		return 0;
	return __tegra_isomgr_set_margin(client, bw, wait);
}
EXPORT_SYMBOL(tegra_isomgr_set_margin);

static int __tegra_isomgr_get_imp_time(enum tegra_iso_client client, u32 bw)
{
	int ret = -EINVAL;

	if (unlikely(!is_client_valid(client)))
		return ret;

	/* FIXME: get this from renegotiable clients(display driver). */
	ret = 100;
	if (isomgr.avail_bw >= bw)
		ret = 0;
	trace_tegra_isomgr_get_imp_time(client, bw, ret, cname[client]);
	return ret;
}

/**
 * Returns the imp time required to realize the bw request.
 * The time returned is an approximate. It is possible that imp
 * time is returned as zero and still realize would be blocked for
 * non-zero time in realize call.
 *
 * @client	client id
 * @bw		bw in KB/sec
 *
 * @returns	time in milliseconds.
 * @retval	-EINVAL, client id is invalid.
 */
int tegra_isomgr_get_imp_time(enum tegra_iso_client client, u32 bw)
{
	if (test_mode)
		return 0;
	return __tegra_isomgr_get_imp_time(client, bw);
}
EXPORT_SYMBOL(tegra_isomgr_get_imp_time);

static u32 __tegra_isomgr_get_available_iso_bw(void)
{
	trace_tegra_isomgr_get_available_iso_bw(isomgr.avail_bw);
	return isomgr.avail_bw;
}

/**
 * Returns available iso bw at the time of calling this API.
 *
 * @returns	available bw in KB/sec.
 */
u32 tegra_isomgr_get_available_iso_bw(void)
{
	if (test_mode)
		return UINT_MAX;
	return __tegra_isomgr_get_available_iso_bw();
}
EXPORT_SYMBOL(tegra_isomgr_get_available_iso_bw);

static u32 __tegra_isomgr_get_total_iso_bw(void)
{
	trace_tegra_isomgr_get_total_iso_bw(isomgr.max_iso_bw);
	return isomgr.max_iso_bw;
}

/**
 * Returns total iso bw in the system.
 *
 * @returns	total bw in KB/sec.
 */
u32 tegra_isomgr_get_total_iso_bw(void)
{
	if (test_mode)
		return UINT_MAX;
	return __tegra_isomgr_get_total_iso_bw();
}
EXPORT_SYMBOL(tegra_isomgr_get_total_iso_bw);

#ifdef CONFIG_TEGRA_ISOMGR_SYSFS
static ssize_t isomgr_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf);

static const struct kobj_attribute lt_mf_attr =
	__ATTR(lt_mf, 0444, isomgr_show, 0);
static const struct kobj_attribute avail_bw_attr =
	__ATTR(avail_bw, 0444, isomgr_show, 0);
static const struct kobj_attribute max_iso_bw_attr =
	__ATTR(max_iso_bw, 0444, isomgr_show, 0);
static const struct kobj_attribute version_attr =
	__ATTR(version, 0444, isomgr_show, 0);

static const struct attribute *isomgr_attrs[] = {
	&lt_mf_attr.attr,
	&avail_bw_attr.attr,
	&max_iso_bw_attr.attr,
	&version_attr.attr,
	NULL
};

static ssize_t isomgr_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	ssize_t rval = 0;

	if (attr == &lt_mf_attr)
		rval = sprintf(buf, "%dKHz\n", isomgr.lt_mf);
	else if (attr == &avail_bw_attr)
		rval = sprintf(buf, "%dKB\n", isomgr.avail_bw);
	else if (attr == &max_iso_bw_attr)
		rval = sprintf(buf, "%dKB\n", isomgr.max_iso_bw);
	else if (attr == &version_attr)
		rval = sprintf(buf, "%d\n", ISOMGR_SYSFS_VERSION);
	return rval;
}

static ssize_t isomgr_client_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	int client = ((char *)attr - (char *)isomgr_clients) /
			sizeof(struct isomgr_client);
	struct isomgr_client *cp =
			(struct isomgr_client *)&isomgr_clients[client];
	ssize_t rval = 0;

	if (attr == &cp->client_attrs.dedi_bw)
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
	else if (attr == &cp->client_attrs.sleep_bw)
		rval = sprintf(buf, "%dKB\n", cp->sleep_bw);
	else if (attr == &cp->client_attrs.margin_bw)
		rval = sprintf(buf, "%dKB\n", cp->margin_bw);
	return rval;
}

static const struct isomgr_client_attrs client_attrs = {
	__ATTR(dedi_bw, 0444, isomgr_client_show, 0),
	__ATTR(rsvd_bw, 0444, isomgr_client_show, 0),
	__ATTR(real_bw, 0444, isomgr_client_show, 0),
	__ATTR(lti,     0444, isomgr_client_show, 0),
	__ATTR(lto,     0444, isomgr_client_show, 0),
	__ATTR(rsvd_mf, 0444, isomgr_client_show, 0),
	__ATTR(real_mf, 0444, isomgr_client_show, 0),
	__ATTR(sleep_bw, 0444, isomgr_client_show, 0),
	__ATTR(margin_bw, 0444, isomgr_client_show, 0),
};

#define NCATTRS (sizeof(client_attrs) / sizeof(struct kobj_attribute))
static const struct attribute *client_attr_list[][NCATTRS+1] = {
#define CLIENT_ATTR(i)\
	{\
		&isomgr_clients[i].client_attrs.dedi_bw.attr,\
		&isomgr_clients[i].client_attrs.rsvd_bw.attr,\
		&isomgr_clients[i].client_attrs.real_bw.attr,\
		&isomgr_clients[i].client_attrs.lti.attr,\
		&isomgr_clients[i].client_attrs.lto.attr,\
		&isomgr_clients[i].client_attrs.rsvd_mf.attr,\
		&isomgr_clients[i].client_attrs.real_mf.attr,\
		&isomgr_clients[i].client_attrs.sleep_bw.attr,\
		&isomgr_clients[i].client_attrs.margin_bw.attr,\
		NULL\
	},
	CLIENT_ATTR(0)
	CLIENT_ATTR(1)
	CLIENT_ATTR(2)
	CLIENT_ATTR(3)
	CLIENT_ATTR(4)
	CLIENT_ATTR(5)
	CLIENT_ATTR(6)
};

static void isomgr_create_client(int client, const char *name)
{
	struct isomgr_client *cp = &isomgr_clients[client];

	/* If this error hits, more CLIENT_ATTR(x) need to be added
	 * in the above array client_attr_list.
	 */
	BUILD_BUG_ON(TEGRA_ISO_CLIENT_COUNT > 7);
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

int __init isomgr_init(void)
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
	if (IS_ERR_OR_NULL(isomgr.emc_clk)) {
		pr_err("couldn't find iso emc clock. disabling isomgr.");
		test_mode = 1;
		return 0;
	}

	if (!isomgr.max_iso_bw) {
		max_emc_clk = clk_round_rate(isomgr.emc_clk, ULONG_MAX) / 1000;
		pr_info("iso emc max clk=%dKHz", max_emc_clk);
		max_emc_bw = tegra_emc_freq_req_to_bw(max_emc_clk);
		/* ISO clients can use iso_bw_percentage of max emc bw. */
		isomgr.max_iso_bw = max_emc_bw * iso_bw_percentage / 100;
		pr_info("max_iso_bw=%dKB", isomgr.max_iso_bw);
		isomgr.avail_bw = isomgr.max_iso_bw;
	}

	for (i = 0; i < TEGRA_ISO_CLIENT_COUNT; i++) {
		atomic_set(&isomgr_clients[i].kref.refcount, 0);
		init_completion(&isomgr_clients[i].cmpl);
		if (client_valid[i]) {
			isomgr_clients[i].emc_clk = clk_get_sys(
					isoclient_info[i].dev_name,
					isoclient_info[i].emc_clk_name);
			if (IS_ERR_OR_NULL(isomgr_clients[i].emc_clk)) {
				pr_err("couldn't find %s %s clock",
					isoclient_info[i].dev_name,
					isoclient_info[i].emc_clk_name);
				pr_err("disabling iso mgr");
				test_mode = 1;
				return 0;
			}
		}
	}
	isomgr_create_sysfs();
	return 0;
}

int tegra_isomgr_enable_test_mode(void)
{
	int i;
	struct isomgr_client *cp = NULL;

	isomgr_lock();
	test_mode = 1;
	isomgr_unlock();
	for (i = 0; i < TEGRA_ISO_CLIENT_COUNT; i++) {
		if (!client_valid[i])
			continue;
		cp = &isomgr_clients[i];
retry:
		__tegra_isomgr_unregister(cp);
		if (atomic_read(&cp->kref.refcount))
			goto retry;
	}
	pr_info("done");
	return 0;
}
EXPORT_SYMBOL(tegra_isomgr_enable_test_mode);

tegra_isomgr_handle test_tegra_isomgr_register(enum tegra_iso_client client,
					  u32 dedicated_bw,	/* KB/sec */
					  tegra_isomgr_renegotiate renegotiate,
					  void *priv)
{
	return __tegra_isomgr_register(client, dedicated_bw, renegotiate, priv);
}
EXPORT_SYMBOL(test_tegra_isomgr_register);

void test_tegra_isomgr_unregister(tegra_isomgr_handle handle)
{
	return __tegra_isomgr_unregister(handle);
}
EXPORT_SYMBOL(test_tegra_isomgr_unregister);

u32 test_tegra_isomgr_reserve(tegra_isomgr_handle handle,
			 u32 bw,	/* KB/sec */
			 u32 lt)	/* usec */
{
	return __tegra_isomgr_reserve(handle, bw, lt);
}
EXPORT_SYMBOL(test_tegra_isomgr_reserve);

u32 test_tegra_isomgr_realize(tegra_isomgr_handle handle)
{
	return __tegra_isomgr_realize(handle);
}
EXPORT_SYMBOL(test_tegra_isomgr_realize);

int test_tegra_isomgr_set_margin(enum tegra_iso_client client,
				u32 bw, bool wait)
{
	return __tegra_isomgr_set_margin(client, bw, wait);
}
EXPORT_SYMBOL(test_tegra_isomgr_set_margin);

int test_tegra_isomgr_get_imp_time(enum tegra_iso_client client, u32 bw)
{
	return __tegra_isomgr_get_imp_time(client, bw);
}
EXPORT_SYMBOL(test_tegra_isomgr_get_imp_time);

u32 test_tegra_isomgr_get_available_iso_bw(void)
{
	return __tegra_isomgr_get_available_iso_bw();
}
EXPORT_SYMBOL(test_tegra_isomgr_get_available_iso_bw);

u32 test_tegra_isomgr_get_total_iso_bw(void)
{
	return __tegra_isomgr_get_total_iso_bw();
}
EXPORT_SYMBOL(test_tegra_isomgr_get_total_iso_bw);
