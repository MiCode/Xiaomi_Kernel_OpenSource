// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/device.h>
#include <linux/kconfig.h>
#include <linux/kthread.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pm_wakeup.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>

#include <slbc.h>
#include <slbc_ops.h>

/* #define CREATE_TRACE_POINTS */
/* #include <slbc_events.h> */
#define trace_slbc_api(f, id)
#define trace_slbc_data(f, data)

#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <linux/cpuidle.h>

static struct pm_qos_request slbc_qos_request;

#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
#include <mmsram.h>

static struct mmsram_data mmsram;
#endif /* CONFIG_MTK_SLBC_MMSRAM */

#if IS_ENABLED(CONFIG_MTK_L3C_PART)
#include <l3c_part.h>
#endif /* CONFIG_MTK_L3C_PART */

/* #define SLBC_THREAD */
/* #define SLBC_TRACE */

static struct mtk_slbc *slbc;

#ifdef SLBC_THREAD
static struct task_struct *slbc_request_task;
static struct task_struct *slbc_release_task;
#endif /* SLBC_THREAD */

static int buffer_ref;
static int acp_ref;
static int slbc_ref;
static int debug_level;
static int uid_ref[UID_MAX];
static struct slbc_data test_d;
static struct slbc_data slbc_pd[UID_MAX];

static LIST_HEAD(slbc_ops_list);
static DEFINE_MUTEX(slbc_ops_lock);

/* 1 in bit is from request done to relase done */
static unsigned long slbc_uid_used;
/* 1 in bit is for mask */
static unsigned long slbc_sid_mask;
/* 1 in bit is under request flow */
static unsigned long slbc_sid_req_q;
/* 1 int bit is under release flow */
static unsigned long slbc_sid_rel_q;
/* 1 int bit is for slot ussage */
static unsigned long slbc_slot_used;

#define SLBC_CHECK_TIME msecs_to_jiffies(1000)
static struct timer_list slbc_deactivate_timer;

static struct slbc_config p_config[] = {
	/* SLBC_ENTRY(id, sid, max, fix, p, extra, res, cache) */
	SLBC_ENTRY(UID_MM_VENC, 0, 0, 1408, 0, 0x0, 0x1, 0),
	SLBC_ENTRY(UID_MM_DISP, 1, 0, 1383, 0, 0x0, 0x1, 0),
	SLBC_ENTRY(UID_MM_MDP, 2, 0, 1383, 0, 0x0, 0x1, 0),
	SLBC_ENTRY(UID_AI_MDLA, 3, 0, 1408, 1, 0x0, 0x1, 0),
	SLBC_ENTRY(UID_AI_ISP, 4, 0, 1408, 1, 0x0, 0x1, 0),
	SLBC_ENTRY(UID_TEST_BUFFER, 5, 0, 1408, 1, 0x0, 0x1, 0),
	SLBC_ENTRY(UID_TEST_CACHE, 6, 0, 0, 1, 0x0, 0x0, 0),
	SLBC_ENTRY(UID_TEST_ACP, 7, 0, 0, 1, 0x0, 0x0, 0),
};

#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
static int slbc_check_mmsram(void)
{
	if (!mmsram.size) {
		mmsram_get_info(&mmsram);
		if (!mmsram.size) {
			pr_info("#@# %s(%d) mmsram is wrong!\n",
					__func__, __LINE__);

			mmsram.paddr = 0;
			mmsram.vaddr = 0;

			return -EINVAL;
		}
	}

	pr_info("%s: pa:%p va:%p size:%#lx\n",
			__func__, mmsram.paddr, mmsram.vaddr, mmsram.size);

	return 0;
}
#endif /* CONFIG_MTK_SLBC_MMSRAM */

static void slbc_set_sram_data(struct slbc_data *d)
{
#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
	WARN_ON(slbc_check_mmsram());

	d->paddr = mmsram.paddr;
	d->vaddr = mmsram.vaddr;
	pr_info("slbc: pa:%lx va:%lx\n",
			(unsigned long)d->paddr, (unsigned long)d->vaddr);
#endif /* CONFIG_MTK_SLBC_MMSRAM */
}

static void slbc_clr_sram_data(struct slbc_data *d)
{
	d->paddr = 0;
	d->vaddr = 0;
	pr_info("slbc: pa:%lx va:%lx\n",
			(unsigned long)d->paddr, (unsigned long)d->vaddr);
}

static void slbc_debug_log(const char *fmt, ...)
{
#ifdef SLBC_DEBUG
	static char buf[1024];
	va_list va;
	int len;

	va_start(va, fmt);
	len = vsnprintf(buf, sizeof(buf), fmt, va);
	va_end(va);

	if (len)
		pr_info("#@# %s\n", buf);
#endif /* SLBC_DEBUG */
}

static int slbc_get_sid_by_uid(enum slbc_uid uid)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(p_config); i++) {
		if (p_config[i].uid == uid)
			return p_config[i].slot_id;
	}

	return SID_NOT_FOUND;
}

static void slbc_backup_private_data(int uid, struct slbc_data *d)
{
	struct slbc_data *pd = &slbc_pd[uid];

	memcpy(pd, d, offsetof(struct slbc_data, private));
	pd->private = d;
	d->private = pd;
}

static void slbc_restore_private_data(int uid, struct slbc_data *d)
{
	struct slbc_data *pd = &slbc_pd[uid];

	memcpy(pd, d, offsetof(struct slbc_data, sid));
	memcpy(&d->sid, &pd->sid, sizeof(struct slbc_data) -
			offsetof(struct slbc_data, sid));
	pd->private = d;
	d->private = pd;
}

static void slbc_deactivate_timer_fn(struct timer_list *timer)
{
	struct slbc_ops *ops;
	int ref = 0;

	slbc_debug_log("%s: slbc_uid_used %lx", __func__, slbc_uid_used);
	slbc_debug_log("%s: slbc_sid_rel_q %lx", __func__,
			slbc_sid_rel_q);

	mutex_lock(&slbc_ops_lock);
	list_for_each_entry(ops, &slbc_ops_list, node) {
		struct slbc_data *d = ops->data;
		unsigned int uid = d->uid;

		if (test_bit(uid, &slbc_sid_rel_q)) {
#ifdef SLBC_TRACE
			trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
			slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

			if (test_bit(uid, &slbc_uid_used)) {
				ref++;

				pr_info("#@# %s(%d) %s not released!\n",
						__func__, __LINE__,
						slbc_uid_str[uid]);
			} else {
				clear_bit(uid, &slbc_sid_rel_q);
				slbc_debug_log("%s: slbc_sid_rel_q %lx",
						__func__, slbc_sid_rel_q);

				pr_info("#@# %s(%d) %s released!\n",
						__func__, __LINE__,
						slbc_uid_str[uid]);
			}
		}
	}
	mutex_unlock(&slbc_ops_lock);

	if (ref) {
		unsigned long expires;

		expires = jiffies + SLBC_CHECK_TIME;
		mod_timer(&slbc_deactivate_timer, expires);
	}
}

int slbc_activate(struct slbc_data *d)
{
	struct slbc_ops *ops;
	unsigned int uid = d->uid;
	int ret = 0;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (d->uid <= 0)
		return -EINVAL;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	ops = container_of(&d, struct slbc_ops, data);
	if (ops && ops->activate) {
		ret = slbc_request(d);
		if (ret) {
			pr_info("#@# %s(%d) %s request fail!\n",
					__func__, __LINE__, slbc_uid_str[uid]);

			return ret;
		}

		if (ops->activate(d) == CB_DONE) {
			pr_info("#@# %s(%d) %s activate fail!\n",
					__func__, __LINE__, slbc_uid_str[uid]);

			ret = slbc_release(d);
			if (ret) {
				pr_info("#@# %s(%d) %s release fail!\n",
						__func__, __LINE__,
						slbc_uid_str[uid]);

				return ret;
			}
		} else {
#ifdef SLBC_TRACE
			trace_slbc_data((void *)__func__, d);
#endif /* SLBC_TRACE */
			slbc_debug_log("%s: %s %s", __func__, slbc_uid_str[uid],
					"done");
		}

		return 0;
	}

	pr_info("#@# %s(%d) %s data not found!\n",
			__func__, __LINE__, slbc_uid_str[uid]);

	return -EFAULT;
}

int slbc_deactivate(struct slbc_data *d)
{
	struct slbc_ops *ops;
	unsigned int uid = d->uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (d->uid <= 0)
		return -EINVAL;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	ops = container_of(&d, struct slbc_ops, data);
	if (ops && ops->deactivate) {
		unsigned long expires;

		ops->deactivate(d);

#ifdef SLBC_TRACE
		trace_slbc_data((void *)__func__, d);
#endif /* SLBC_TRACE */
		slbc_debug_log("%s: %s %s", __func__, slbc_uid_str[uid],
				"done");

		if (test_bit(uid, &slbc_sid_rel_q)) {
			slbc_debug_log("%s: %s already in release_q!",
					__func__, slbc_uid_str[uid],
					slbc_sid_rel_q);
		}

		set_bit(uid, &slbc_sid_rel_q);
		slbc_debug_log("%s: slbc_sid_rel_q %lx", __func__,
				slbc_sid_rel_q);
		expires = jiffies + SLBC_CHECK_TIME;
		mod_timer(&slbc_deactivate_timer, expires);

		return 0;
	}

	pr_info("#@# %s(%d) %s data not found!\n",
			__func__, __LINE__, slbc_uid_str[uid]);

	return -EFAULT;
}

#ifdef SLBC_THREAD
static struct slbc_data *slbc_find_next_low_used(struct slbc_data *d_old)
{
	struct slbc_ops *ops;
	struct slbc_config *config_old = d_old->config;
	unsigned int p_old = config_old->priority;
	unsigned int res_old = config_old->res_slot;
	struct slbc_data *d_used = NULL;

	slbc_debug_log("%s: slbc_uid_used %lx", __func__, slbc_uid_used);

	list_for_each_entry(ops, &slbc_ops_list, node) {
		struct slbc_data *d = ops->data;
		struct slbc_config *config = d->config;
		unsigned int uid = d->uid;
		unsigned int p = config->priority;

		if (test_bit(uid, &slbc_uid_used) &&
				(p > p_old) &&
				(d->slot_used & res_old)) {
			d_used = d;

			break;
		}
	}

	if (d_used) {
#ifdef SLBC_TRACE
		trace_slbc_data((void *)__func__, d_used);
#endif /* SLBC_TRACE */
		slbc_debug_log("%s: %s", __func__,
				slbc_uid_str[d_used->uid]);
	}

	return d_used;

}

static struct slbc_data *slbc_find_next_high_req(struct slbc_data *d_old)
{
	struct slbc_ops *ops;
	struct slbc_config *config_old = d_old->config;
	unsigned int p_old = config_old->priority;
	unsigned int res_old = config_old->res_slot;
	struct slbc_data *d_req = NULL;

	slbc_debug_log("%s: slbc_sid_req_q %lx", __func__, slbc_sid_req_q);

	list_for_each_entry(ops, &slbc_ops_list, node) {
		struct slbc_data *d = ops->data;
		struct slbc_config *config = d->config;
		unsigned int res = config->res_slot;
		unsigned int uid = d->uid;
		unsigned int p = config->priority;

		if (test_bit(uid, &slbc_sid_req_q) &&
				(p <= p_old) &&
				(res & res_old)) {
			p_old = p;
			d_req = d;

			if (!p)
				break;
		}
	}

	if (d_req) {
#ifdef SLBC_TRACE
		trace_slbc_data((void *)__func__, d_req);
#endif /* SLBC_TRACE */
		slbc_debug_log("%s: %s", __func__, slbc_uid_str[d_req->uid]);
	}

	return d_req;
}

static int slbc_activate_thread(void *arg)
{
	struct slbc_data *d = arg;
	unsigned int uid = d->uid;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	/* check return value */
	slbc_activate(d);

	return 0;
}

static int slbc_deactivate_thread(void *arg)
{
	struct slbc_data *d = arg;
	unsigned int uid = d->uid;
	struct slbc_data *d_used;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	while ((d_used = slbc_find_next_low_used(d))) {
		/* check return value */
		slbc_deactivate(d_used);
	};

	return 0;
}
#endif /* SLBC_THREAD */

static void check_slot_by_data(struct slbc_data *d)
{
	struct slbc_config *config = d->config;
	unsigned int res = config->res_slot;
	unsigned int uid = d->uid;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	/* check all need slot */
	if ((d->type) == TP_BUFFER)
		d->slot_used = res;
	else
		d->slot_used = 0;
}

static int find_slbc_slot_by_data(struct slbc_data *d)
{
	unsigned int uid = d->uid;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	slbc_debug_log("%s: slbc_slot_used %lx", __func__, slbc_slot_used);
	if (!(slbc_slot_used & d->slot_used))
		return SLOT_AVAILABLE;

	return SLOT_USED;
}

static void slbc_set_slot_by_data(struct slbc_data *d)
{
	slbc_slot_used |= d->slot_used;
	slbc_debug_log("%s: slbc_slot_used %lx", __func__, slbc_slot_used);
}

static void slbc_clr_slot_by_data(struct slbc_data *d)
{
	slbc_slot_used &= ~d->slot_used;
	slbc_debug_log("%s: slbc_slot_used %lx", __func__, slbc_slot_used);
}

static void slbc_debug_dump_data(struct slbc_data *d)
{
	unsigned int uid = d->uid;

	pr_info("ID %s\t", slbc_uid_str[uid]);

	if (test_bit(uid, &slbc_uid_used))
		pr_info(" activate\n");
	else
		pr_info(" deactivate\n");

	pr_info("uid: %d\n", uid);
	pr_info("type: 0x%x\n", d->type);
	pr_info("size: %ld\n", d->size);
	pr_info("paddr: %lx\n", (unsigned long)d->paddr);
	pr_info("vaddr: %lx\n", (unsigned long)d->vaddr);
	pr_info("sid: %d\n", d->sid);
	pr_info("slot_used: 0x%x\n", d->slot_used);
	pr_info("config: %p\n", d->config);
	pr_info("ref: %d\n", d->ref);
	pr_info("pwr_ref: %d\n", d->pwr_ref);
}

static int slbc_debug_all(void)
{
	struct slbc_ops *ops;
	int i;
	int sid;

	pr_info("slbc_enable %x\n", slbc_enable);
	pr_info("slbc_uid_used %lx\n", slbc_uid_used);
	pr_info("slbc_sid_mask %lx\n", slbc_sid_mask);
	pr_info("slbc_sid_req_q %lx\n", slbc_sid_req_q);
	pr_info("slbc_sid_rel_q %lx\n", slbc_sid_rel_q);
	pr_info("slbc_slot_used %lx\n", slbc_slot_used);
	pr_info("buffer_ref %x\n", buffer_ref);
	pr_info("acp_ref %x\n", acp_ref);
	pr_info("slbc_ref %x\n", slbc_ref);

	for (i = 0; i < UID_MAX; i++) {
		sid = slbc_get_sid_by_uid(i);
		if (sid != SID_NOT_FOUND)
			pr_info("uid_ref %s %x\n", slbc_uid_str[i], uid_ref[i]);
	}

	mutex_lock(&slbc_ops_lock);
	list_for_each_entry(ops, &slbc_ops_list, node) {
		struct slbc_data *d = ops->data;

		slbc_debug_dump_data(d);
	}
	mutex_unlock(&slbc_ops_lock);

	pr_info("\n");

	return 0;
}

static int slbc_request_cache(struct slbc_data *d)
{
	int ret = 0;

	slbc_debug_log("%s: TP_CACHE\n", __func__);

	return ret;
}

static int slbc_request_buffer(struct slbc_data *d)
{
	int ret = 0;

	slbc_debug_log("%s: TP_BUFFER\n", __func__);
#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
	if (!buffer_ref) {
		d->pwr_ref++;
		enable_mmsram();
	}
#endif /* CONFIG_MTK_SLBC_MMSRAM */

	slbc_set_sram_data(d);

	buffer_ref++;

	return ret;
}

static int slbc_request_acp(void *ptr)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_L3C_PART)
	struct slbc_data *d = ptr;

	slbc_debug_log("%s: TP_ACP\n", __func__);

	if (!acp_ref) {
#if IS_ENABLED(CONFIG_PM_SLEEP)
		__pm_stay_awake(slbc->ws);
#endif /* CONFIG_PM_SLEEP */

		cpu_latency_qos_update_request(&slbc_qos_request,
				slbc->slbc_qos_latency);
	}

	if (acp_ref++ == 0) {
		const unsigned long ratio_bits = d->flag & FG_ACP_BITS;
		int ratio = find_first_bit(&ratio_bits, 32)
			- ACP_ONLY_BIT;

		slbc_debug_log("%s: before %s:0x%x, %s:0x%x\n",
				__func__,
				"MTK_L3C_PART_MCU",
				mtk_l3c_get_part(MTK_L3C_PART_MCU),
				"MTK_L3C_PART_ACP",
				mtk_l3c_get_part(MTK_L3C_PART_ACP));
		if (d->flag & FG_ACP_ONLY)
			mtk_l3c_set_mcu_part(4 - ratio);
		else
			mtk_l3c_set_mcu_part(4);
		mtk_l3c_set_acp_part(ratio);
		slbc_debug_log("%s: after %s:0x%x, %s:0x%x\n",
				__func__,
				"MTK_L3C_PART_MCU",
				mtk_l3c_get_part(MTK_L3C_PART_MCU),
				"MTK_L3C_PART_ACP",
				mtk_l3c_get_part(MTK_L3C_PART_ACP));
	}
#endif /* CONFIG_MTK_L3C_PART */

	return ret;
}

int slbc_request(struct slbc_data *d)
{
	struct slbc_data *pd;
	unsigned int uid;
	unsigned int sid;
	int ret = 0;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (d == 0)
		return -EINVAL;

	if (d->uid <= 0)
		return -EINVAL;

	mutex_lock(&slbc_ops_lock);

	uid = d->uid;

	sid = slbc_get_sid_by_uid(uid);
	if (sid != SID_NOT_FOUND) {
		memset(&d->sid, 0, sizeof(struct slbc_data) -
				offsetof(struct slbc_data, sid));
		d->sid = sid;
		d->config = &p_config[sid];
	} else {
		pr_info("#@# %s(%d) %s sid is wrong!\n",
				__func__, __LINE__, slbc_uid_str[uid]);

		ret = -EINVAL;
		goto error;
	}

	pd = &slbc_pd[uid];
	if (pd->private)
		slbc_restore_private_data(uid, d);

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	slbc_debug_log("%s: slbc_sid_mask %lx", __func__, slbc_sid_mask);
	if (test_bit(uid, &slbc_sid_mask)) {
		ret = -EREQ_MASKED;
		goto error;
	}

	if (test_bit(uid, &slbc_sid_req_q)) {
		slbc_debug_log("%s: %s already in request_q!",
				__func__, slbc_uid_str[uid],
				slbc_sid_req_q);
	}

	set_bit(uid, &slbc_sid_req_q);
	slbc_debug_log("%s: slbc_sid_req_q %lx", __func__, slbc_sid_req_q);

	slbc_debug_log("%s: slbc_uid_used %lx", __func__, slbc_uid_used);
	if (test_bit(uid, &slbc_uid_used)) {
		slbc_debug_log("%s: %s already requested!",
				__func__, slbc_uid_str[uid]);

		slbc_set_sram_data(d);

		goto request_done1;
	}

	if (BIT_IN_MM_BITS_1(BIT(uid)) &&
			!BIT_IN_MM_BITS_1(slbc_uid_used)) {
		slbc_debug_log("%s: %s use the same request!",
				__func__, slbc_uid_str[uid]);

		slbc_set_sram_data(d);

		goto request_done;
	}

	check_slot_by_data(d);

	if (((d->type) == TP_BUFFER) &&
			(find_slbc_slot_by_data(d) != SLOT_AVAILABLE)) {
#ifdef SLBC_THREAD
		struct slbc_data *d_used;

		d_used = slbc_find_next_low_used(d);
		if (d_used) {
			slbc_release_task = kthread_run(slbc_deactivate_thread,
					d, "slbc_deactivate_thread");

			ret = -EWAIT_RELEASE;
			goto error;
		}
#endif /* SLBC_THREAD */

		ret = -ENOT_AVAILABLE;
		goto error;
	}

	if ((d->type) == TP_BUFFER)
		slbc_request_buffer(d);

	if ((d->type) == TP_CACHE)
		slbc_request_cache(d);

	if ((d->type) == TP_ACP)
		slbc_request_acp(d);

	slbc_set_slot_by_data(d);

#ifdef SLBC_TRACE
	trace_slbc_data((void *)__func__, d);
#endif /* SLBC_TRACE */

request_done:
	set_bit(uid, &slbc_uid_used);
	slbc_debug_log("%s: slbc_uid_used %lx", __func__, slbc_uid_used);
	clear_bit(uid, &slbc_sid_req_q);
	slbc_debug_log("%s: slbc_sid_req_q %lx", __func__, slbc_sid_req_q);

request_done1:
	slbc_ref++;
	d->ref++;
	uid_ref[uid]++;

	slbc_debug_dump_data(d);

error:
	slbc_backup_private_data(uid, d);
	mutex_unlock(&slbc_ops_lock);

	return 0;
}

static int slbc_release_cache(struct slbc_data *d)
{
	int ret = 0;

	slbc_debug_log("%s: TP_CACHE\n", __func__);

	return ret;
}

static int slbc_release_buffer(struct slbc_data *d)
{
	int ret = 0;

	slbc_debug_log("%s: TP_BUFFER\n", __func__);

	buffer_ref--;
	WARN_ON(buffer_ref < 0);

	slbc_clr_sram_data(d);

#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
	if (!buffer_ref) {
		d->pwr_ref--;
		disable_mmsram();
	}
#endif /* CONFIG_MTK_SLBC_MMSRAM */

	return ret;
}

static int slbc_release_acp(void *ptr)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_L3C_PART)
	struct slbc_data *d = ptr;

	slbc_debug_log("%s: TP_ACP\n", __func__);

	if (--acp_ref == 0) {
		slbc_debug_log("%s: before %s:0x%x, %s:0x%x\n",
				__func__,
				"MTK_L3C_PART_MCU",
				mtk_l3c_get_part(MTK_L3C_PART_MCU),
				"MTK_L3C_PART_ACP",
				mtk_l3c_get_part(MTK_L3C_PART_ACP));
		mtk_l3c_set_mcu_part(4);
		mtk_l3c_set_acp_part(1);
		slbc_debug_log("%s: after %s:0x%x, %s:0x%x\n",
				__func__,
				"MTK_L3C_PART_MCU",
				mtk_l3c_get_part(MTK_L3C_PART_MCU),
				"MTK_L3C_PART_ACP",
				mtk_l3c_get_part(MTK_L3C_PART_ACP));
	}
	WARN_ON(acp_ref < 0);

	if (!acp_ref) {
#if IS_ENABLED(CONFIG_PM_SLEEP)
		__pm_relax(slbc->ws);
#endif /* CONFIG_PM_SLEEP */

		cpu_latency_qos_update_request(&slbc_qos_request,
				PM_QOS_DEFAULT_VALUE);
	}
#endif /* CONFIG_MTK_L3C_PART */

	return ret;
}

int slbc_release(struct slbc_data *d)
{
	struct slbc_data *pd;
#ifdef SLBC_THREAD
	struct slbc_data *d_req;
#endif /* SLBC_THREAD */
	unsigned int uid;
	unsigned int sid;
	int ret = 0;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (d == 0)
		return -EINVAL;

	if (d->uid <= 0)
		return -EINVAL;

	mutex_lock(&slbc_ops_lock);

	uid = d->uid;

	sid = slbc_get_sid_by_uid(uid);
	if (sid != SID_NOT_FOUND) {
		d->sid = sid;
		d->config = &p_config[sid];
	} else {
		pr_info("#@# %s(%d) %s sid is wrong!\n",
				__func__, __LINE__, slbc_uid_str[uid]);

		ret = -EINVAL;
		goto error;
	}

	if (!uid_ref[uid]) {
		pr_info("#@# %s(%d) %s uid_ref[%d] is zero!\n",
				__func__, __LINE__, slbc_uid_str[uid], uid);

		ret = -EINVAL;
		goto error;
	}

	pd = &slbc_pd[uid];
	if (pd->private)
		slbc_restore_private_data(uid, d);

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	slbc_debug_log("%s: slbc_sid_mask %lx", __func__, slbc_sid_mask);
	if (test_bit(uid, &slbc_sid_mask)) {
		ret = -EREQ_MASKED;
		goto error;
	}

	slbc_debug_log("%s: slbc_uid_used %lx", __func__, slbc_uid_used);
	if (!test_bit(uid, &slbc_uid_used)) {
		slbc_debug_log("%s: %s already released!",
				__func__, slbc_uid_str[uid]);

		slbc_clr_sram_data(d);

		goto release_done1;
	}

	if (uid_ref[uid] > 1) {
		slbc_debug_log("%s: %s already released!",
				__func__, slbc_uid_str[uid]);

		slbc_clr_sram_data(d);

		goto release_done1;
	}

	if (BIT_IN_MM_BITS_1(BIT(uid)) &&
			!BIT_IN_MM_BITS_1(slbc_uid_used & ~BIT(uid))) {
		slbc_debug_log("%s: %s use the same release!",
				__func__, slbc_uid_str[uid]);

		slbc_clr_sram_data(d);

		goto release_done;
	}

	if ((d->type) == TP_BUFFER)
		slbc_release_buffer(d);

	if ((d->type) == TP_CACHE)
		slbc_release_cache(d);

	if ((d->type) == TP_ACP)
		slbc_release_acp(d);

	slbc_clr_slot_by_data(d);

#ifdef SLBC_TRACE
	trace_slbc_data((void *)__func__, d);
#endif /* SLBC_TRACE */

#ifdef SLBC_THREAD
	d_req = slbc_find_next_high_req(d);
	if (d_req) {
		slbc_request_task = kthread_run(slbc_activate_thread,
				d_req, "slbc_activate_thread");
	}
#endif /* SLBC_THREAD */

release_done:
	clear_bit(uid, &slbc_uid_used);
	slbc_debug_log("%s: slbc_uid_used %lx", __func__, slbc_uid_used);

release_done1:
	slbc_ref--;
	d->ref--;
	uid_ref[uid]--;

	if ((d->ref < 0) || (uid_ref[uid] < 0)) {
		slbc_debug_dump_data(d);
		slbc_debug_all();
	}

	WARN((d->ref < 0) || (uid_ref[uid] < 0),
			"%s: release %s fail! %d %d\n",
			__func__, slbc_uid_str[uid], d->ref, uid_ref[uid]);

error:
	slbc_backup_private_data(uid, d);
	mutex_unlock(&slbc_ops_lock);

	return 0;
}

int slbc_power_on(struct slbc_data *d)
{
	unsigned int uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (!d)
		return -EINVAL;

	uid = d->uid;
	if (uid <= UID_ZERO || uid >= UID_MAX)
		return -EINVAL;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	/* slbc_debug_log("%s: %s flag %x", __func__, */
	/* slbc_uid_str[uid], d->flag); */

#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
	if (IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM) &&
			(SLBC_TRY_FLAG_BIT(d, FG_POWER) ||
			 d->ref)) {
		d->pwr_ref++;
		return mmsram_power_on();
	}
#endif /* CONFIG_MTK_SLBC_MMSRAM */

	return 0;
}

int slbc_power_off(struct slbc_data *d)
{
	unsigned int uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (!d)
		return -EINVAL;

	uid = d->uid;
	if (uid <= UID_ZERO || uid >= UID_MAX)
		return -EINVAL;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	/* slbc_debug_log("%s: %s flag %x", __func__, */
	/* slbc_uid_str[uid], d->flag); */

#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
	if (IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM) &&
			(SLBC_TRY_FLAG_BIT(d, FG_POWER) ||
			 d->ref)) {
		d->pwr_ref--;
		mmsram_power_off();
	}
#endif /* CONFIG_MTK_SLBC_MMSRAM */

	return 0;
}

int slbc_secure_on(struct slbc_data *d)
{
	unsigned int uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (!d)
		return -EINVAL;

	uid = d->uid;
	if (uid <= UID_ZERO || uid >= UID_MAX)
		return -EINVAL;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s flag %x", __func__, slbc_uid_str[uid], d->flag);

#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
	if (IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM) &&
			SLBC_TRY_FLAG_BIT(d, FG_SECURE)) {
		mmsram_set_secure(true);
	}
#endif /* CONFIG_MTK_SLBC_MMSRAM */

	return 0;
}

int slbc_secure_off(struct slbc_data *d)
{
	unsigned int uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (!d)
		return -EINVAL;

	uid = d->uid;
	if (uid <= UID_ZERO || uid >= UID_MAX)
		return -EINVAL;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s flag %x", __func__, slbc_uid_str[uid], d->flag);

#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
	if (IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM) &&
			SLBC_TRY_FLAG_BIT(d, FG_SECURE)) {
		mmsram_set_secure(false);
	}
#endif /* CONFIG_MTK_SLBC_MMSRAM */

	return 0;
}

static void slbc_dump_data(struct seq_file *m, struct slbc_data *d)
{
	unsigned int uid = d->uid;

	seq_printf(m, "ID %s\t", slbc_uid_str[uid]);

	if (test_bit(uid, &slbc_uid_used))
		seq_puts(m, " activate\n");
	else
		seq_puts(m, " deactivate\n");

	seq_printf(m, "uid: %d\n", uid);
	seq_printf(m, "type: 0x%x\n", d->type);
	seq_printf(m, "size: %ld\n", d->size);
	seq_printf(m, "paddr: %lx\n", (unsigned long)d->paddr);
	seq_printf(m, "vaddr: %lx\n", (unsigned long)d->vaddr);
	seq_printf(m, "sid: %d\n", d->sid);
	seq_printf(m, "slot_used: 0x%x\n", d->slot_used);
	seq_printf(m, "config: %p\n", d->config);
	seq_printf(m, "ref: %d\n", d->ref);
	seq_printf(m, "pwr_ref: %d\n", d->pwr_ref);
}

static int dbg_slbc_proc_show(struct seq_file *m, void *v)
{
	struct slbc_ops *ops;
	int i;
	int sid;

	seq_printf(m, "slbc_enable %x\n", slbc_enable);
	seq_printf(m, "slbc_uid_used 0x%lx\n", slbc_uid_used);
	seq_printf(m, "slbc_sid_mask 0x%lx\n", slbc_sid_mask);
	seq_printf(m, "slbc_sid_req_q 0x%lx\n", slbc_sid_req_q);
	seq_printf(m, "slbc_sid_rel_q 0x%lx\n", slbc_sid_rel_q);
	seq_printf(m, "slbc_slot_used 0x%lx\n", slbc_slot_used);
	seq_printf(m, "buffer_ref %x\n", buffer_ref);
	seq_printf(m, "acp_ref %x\n", acp_ref);
	seq_printf(m, "slbc_ref %x\n", slbc_ref);
	seq_printf(m, "debug_level %x\n", debug_level);

	for (i = 0; i < UID_MAX; i++) {
		sid = slbc_get_sid_by_uid(i);
		if (sid != SID_NOT_FOUND)
			seq_printf(m, "uid_ref %s %x\n",
					slbc_uid_str[i], uid_ref[i]);
	}

	mutex_lock(&slbc_ops_lock);
	list_for_each_entry(ops, &slbc_ops_list, node) {
		struct slbc_data *d = ops->data;

		slbc_dump_data(m, d);
	}
	mutex_unlock(&slbc_ops_lock);

	seq_puts(m, "\n");

	return 0;
}

static ssize_t dbg_slbc_proc_write(struct file *file,
		const char __user *buffer, size_t count, loff_t *pos)
{
	int ret = 0;
	char *buf = (char *) __get_free_page(GFP_USER);
	char cmd[64];
	unsigned long val_1;
	unsigned long val_2;

	if (!buf)
		return -ENOMEM;

	ret = -EINVAL;

	if (count >= PAGE_SIZE)
		goto out;

	ret = -EFAULT;

	if (copy_from_user(buf, buffer, count))
		goto out;

	buf[count] = '\0';

	ret = sscanf(buf, "%63s %lx %lx", cmd, &val_1, &val_2);
	if (ret < 1) {
		ret = -EPERM;
		goto out;
	}

	if (!strcmp(cmd, "slbc_enable")) {
		slbc_enable = !!val_1;
		if (slbc_enable == 0) {
			struct slbc_ops *ops;

			mutex_lock(&slbc_ops_lock);
			list_for_each_entry(ops, &slbc_ops_list, node) {
				struct slbc_data *d = ops->data;
				unsigned int uid = d->uid;

				if (test_bit(uid, &slbc_uid_used))
					ops->deactivate(d);
			}
			mutex_unlock(&slbc_ops_lock);
		}
	} else if (!strcmp(cmd, "slbc_uid_used"))
		slbc_uid_used = val_1;
	else if (!strcmp(cmd, "slbc_sid_mask"))
		slbc_sid_mask = val_1;
	else if (!strcmp(cmd, "slbc_sid_req_q"))
		slbc_sid_req_q = val_1;
	else if (!strcmp(cmd, "slbc_sid_rel_q"))
		slbc_sid_rel_q = val_1;
	else if (!strcmp(cmd, "slbc_slot_used"))
		slbc_slot_used = val_1;
	else if (!strcmp(cmd, "test_acp_request")) {
		test_d.uid = UID_TEST_ACP;
		test_d.type  = TP_ACP;
		test_d.flag = val_1;
		slbc_request(&test_d);
	} else if (!strcmp(cmd, "test_acp_release")) {
		test_d.uid = UID_TEST_ACP;
		test_d.type  = TP_ACP;
		test_d.flag = val_1;
		slbc_release(&test_d);
	} else if (!strcmp(cmd, "debug_level")) {
		debug_level = val_1;
	}

out:
	free_page((unsigned long)buf);

	if (ret < 0)
		return ret;

	return count;
}

PROC_FOPS_RW(dbg_slbc);

static int slbc_create_debug_fs(void)
{
	int i;
	struct proc_dir_entry *dir = NULL;

	struct pentry {
		const char *name;
		const struct proc_ops *fops;
		void *data;
	};

	const struct pentry entries[] = {
		PROC_ENTRY(dbg_slbc),
	};

	/* create /proc/slbc */
	dir = proc_mkdir("slbc", NULL);
	if (!dir) {
		pr_info("fail to create /proc/slbc @ %s()\n", __func__);

		return -ENOMEM;
	}

	for (i = 0; i < ARRAY_SIZE(entries); i++) {
		if (!proc_create_data(entries[i].name, 0660,
					dir, entries[i].fops, entries[i].data))
			pr_info("%s(), create /proc/slbc/%s failed\n",
					__func__, entries[i].name);
	}

	return 0;
}

static struct slbc_common_ops common_ops = {
	.slbc_request = slbc_request,
	.slbc_release = slbc_release,
	.slbc_power_on = slbc_power_on,
	.slbc_power_off = slbc_power_off,
	.slbc_secure_on = slbc_secure_on,
	.slbc_secure_off = slbc_secure_off,
};

static int slbc_probe(struct platform_device *pdev)
{
	struct device_node *node;
	struct device *dev = &pdev->dev;
	int ret = 0;
	const char *buf;
	struct cpuidle_driver *drv = cpuidle_get_driver();

	slbc = devm_kzalloc(dev, sizeof(struct mtk_slbc), GFP_KERNEL);
	if (!slbc)
		return -ENOMEM;

	slbc->dev = dev;

	node = of_find_compatible_node(NULL, NULL,
			"mediatek,mtk-slbc");
	if (node) {
		ret = of_property_read_string(node,
				"status", (const char **)&buf);

		if (ret == 0) {
			if (!strcmp(buf, "enable"))
				slbc_enable = 1;
			else
				slbc_enable = 0;
		}
		pr_info("#@# %s(%d) slbc_enable %d\n", __func__, __LINE__,
				slbc_enable);
	} else
		pr_info("find slbc node failed\n");

#if IS_ENABLED(CONFIG_PM_SLEEP)
	slbc->ws = wakeup_source_register(NULL, "slbc");
	if (!slbc->ws)
		pr_debug("slbc wakelock register fail!\n");
#endif /* CONFIG_PM_SLEEP */

	ret = slbc_create_debug_fs();
	if (ret) {
		pr_info("SLBC FAILED TO CREATE DEBUG FILESYSTEM (%d)\n", ret);

		return ret;
	}

	if (drv)
		slbc->slbc_qos_latency = drv->states[2].exit_latency - 10;
	else
		slbc->slbc_qos_latency = 300;
	pr_info("slbc_qos_latency %dus\n", slbc->slbc_qos_latency);

	cpu_latency_qos_add_request(&slbc_qos_request,
			PM_QOS_DEFAULT_VALUE);

	timer_setup(&slbc_deactivate_timer, slbc_deactivate_timer_fn,
			TIMER_DEFERRABLE);

	slbc_register_common_ops(&common_ops);

	return 0;
}

static int slbc_remove(struct platform_device *pdev)
{
	slbc_unregister_common_ops(&common_ops);

	return 0;
}

static int slbc_suspend(struct platform_device *pdev, pm_message_t state)
{
	return 0;
}

static int slbc_resume(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id slbc_of_match[] = {
	{ .compatible = "mediatek,mtk-slbc", },
	{}
};

static struct platform_driver slbc_pdrv = {
	.probe = slbc_probe,
	.remove = slbc_remove,
	.suspend = slbc_suspend,
	.resume = slbc_resume,
	.driver = {
		.name = "slbc",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(slbc_of_match),
	},
};

struct platform_device slbc_pdev = {
	.name = "slbc",
	.id = -1,
};

int __init slbc_module_init(void)
{
	int r;

	/* register platform device/driver */
	r = platform_device_register(&slbc_pdev);
	if (r) {
		pr_info("Failed to register platform device.\n");
		return -EINVAL;
	}

	r = platform_driver_register(&slbc_pdrv);
	if (r) {
		pr_info("fail to register slbc driver @ %s()\n", __func__);
		return -EINVAL;
	}

	return 0;
}

late_initcall(slbc_module_init);

MODULE_DESCRIPTION("SLBC Driver mt6893 v0.1");
MODULE_LICENSE("GPL");
