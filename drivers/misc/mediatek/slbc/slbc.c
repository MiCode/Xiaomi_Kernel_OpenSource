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
/* #define CREATE_TRACE_POINTS */
/* #include <slbc_events.h> */
#define trace_slbc_api(f, id)
#define trace_slbc_data(f, data)

#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
#include <mmsram.h>

static struct mmsram_data mmsram;
#endif /* CONFIG_MTK_SLBC_MMSRAM */

#if IS_ENABLED(CONFIG_MTK_L3C_PART)
#include <l3c_part.h>
#endif /* CONFIG_MTK_L3C_PART */

/* #define SLBC_THREAD */
/* #define SLBC_TRACE */

#ifdef SLBC_THREAD
static struct task_struct *slbc_request_task;
static struct task_struct *slbc_release_task;
#endif /* SLBC_THREAD */

#ifdef CONFIG_PM_SLEEP
static struct wakeup_source *slbc_ws;
#endif /* CONFIG_PM_SLEEP */

static int slbc_enable = 1;
static int buffer_ref;
static int cache_ref;
static int slbc_ref;
static int uid_ref[UID_MAX];
static struct slbc_data test_d;

static LIST_HEAD(slbc_ops_list);
static DEFINE_MUTEX(slbc_ops_lock);
static unsigned long slbc_status;
static unsigned long slbc_mask_status;
static unsigned long slbc_req_status;
static unsigned long slbc_release_status;
static unsigned long slbc_slot_status;

#define SLBC_CHECK_TIME msecs_to_jiffies(1000)
static struct timer_list slbc_deactivate_timer;

static struct slbc_config p_config[] = {
	/* SLBC_ENTRY(id, sid, max, fix, p, extra, res, cache) */
	SLBC_ENTRY(UID_MM_VENC, 0, 0, 1408, 0, 0x0, 0x1, 0),
	SLBC_ENTRY(UID_MM_DISP, 1, 0, 1383, 0, 0x0, 0x1, 0),
	SLBC_ENTRY(UID_MM_MDP, 2, 0, 1383, 0, 0x0, 0x1, 0),
	SLBC_ENTRY(UID_MD_DPMAIF, 3, 0, 1408, 1, 0x0, 0x1, 0),
	SLBC_ENTRY(UID_AI_MDLA, 4, 0, 1408, 1, 0x0, 0x1, 0),
	SLBC_ENTRY(UID_AI_ISP, 5, 0, 1408, 1, 0x0, 0x1, 0),
	SLBC_ENTRY(UID_TEST, 6, 0, 1408, 1, 0x0, 0x1, 0),
};

char *slbc_uid_str[] = {
	"UID_ZERO",
	"UID_MM_VENC",
	"UID_MM_DISP",
	"UID_MM_MDP",
	"UID_MM_VDEC",
	"UID_MD_DPMAIF",
	"UID_AI_MDLA",
	"UID_AI_ISP",
	"UID_GPU",
	"UID_HIFI3",
	"UID_CPU",
	"UID_TEST",
	"UID_MAX",
};

#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
static int slbc_check_mmsram(void)
{
	if (!mmsram.size) {
		mmsram_get_info(&mmsram);
		if (!mmsram.size) {
			pr_info("#@# %s(%d) mmsram is wrong !!!\n",
					__func__, __LINE__);

			return -EINVAL;
		}
	}

	return 0;
}
#endif /* CONFIG_MTK_SLBC_MMSRAM */

static void slbc_set_mmsram_data(struct slbc_data *d)
{
#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
	if (slbc_check_mmsram()) {
		pr_info("#@# %s(%d) mmsram is wrong !!!\n",
				__func__, __LINE__);
	}

	d->paddr = mmsram.paddr;
	d->vaddr = mmsram.vaddr;
#endif /* CONFIG_MTK_SLBC_MMSRAM */
}

static void slbc_clr_mmsram_data(struct slbc_data *d)
{
#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
	d->paddr = 0;
	d->vaddr = 0;
#endif /* CONFIG_MTK_SLBC_MMSRAM */
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

static unsigned int get_slbc_sid_by_uid(enum slbc_uid uid)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(p_config); i++) {
		if (p_config[i].uid == uid)
			return p_config[i].slot_id;
	}

	return SID_NOT_FOUND;
}

/**
 * register_slbc_ops - Register a set of slbc operations.
 * @ops: slbc operations to register.
 */
int register_slbc_ops(struct slbc_ops *ops)
{
	unsigned int uid;
	unsigned int sid;
	struct slbc_data *d;

#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
	if (slbc_check_mmsram()) {
		pr_info("#@# %s(%d) mmsram is wrong !!!\n",
				__func__, __LINE__);

		return -EINVAL;
	}
#endif /* CONFIG_MTK_SLBC_MMSRAM */

	if (ops && ops->data) {
		d = ops->data;
		uid = d->uid;

#ifdef SLBC_TRACE
		trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
		slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);
	} else {
		pr_info("#@# %s(%d) data is wrong !!!\n", __func__, __LINE__);

		return -EINVAL;
	}

	sid = get_slbc_sid_by_uid(uid);
	if (sid != SID_NOT_FOUND) {
		d->sid = sid;
		d->config = &p_config[sid];
		d->slot_used = 0;
		d->ref = 0;
		d->pwr_ref = 0;
	} else {
		pr_info("#@# %s(%d) slot is wrong !!!\n", __func__, __LINE__);

		return -EINVAL;
	}

	mutex_lock(&slbc_ops_lock);
	list_add_tail(&ops->node, &slbc_ops_list);
	mutex_unlock(&slbc_ops_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(register_slbc_ops);

/**
 * unregister_slbc_ops - Unregister a set of slbc operations.
 * @ops: slbc operations to unregister.
 */
int unregister_slbc_ops(struct slbc_ops *ops)
{
	unsigned int uid;
	struct slbc_data *d;

	if (ops && ops->data) {
		d = ops->data;
		uid = d->uid;

#ifdef SLBC_TRACE
		trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
		slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);
	} else {
		pr_info("#@# %s(%d) data is wrong !!!\n", __func__, __LINE__);

		return -EINVAL;
	}

	d->sid = 0;
	d->config = 0;
	d->slot_used = 0;
	d->ref = 0;
	d->pwr_ref = 0;

	mutex_lock(&slbc_ops_lock);
	list_del(&ops->node);
	mutex_unlock(&slbc_ops_lock);

	return 0;
}
EXPORT_SYMBOL_GPL(unregister_slbc_ops);

static void slbc_deactivate_timer_fn(struct timer_list *timer)
{
	struct slbc_ops *ops;
	int ref = 0;

	slbc_debug_log("%s: slbc_status %lx", __func__, slbc_status);
	slbc_debug_log("%s: slbc_release_status %lx", __func__,
			slbc_release_status);

	list_for_each_entry(ops, &slbc_ops_list, node) {
		struct slbc_data *d = ops->data;
		unsigned int uid = d->uid;

		if (test_bit(uid, &slbc_release_status)) {
#ifdef SLBC_TRACE
			trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
			slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

			if (test_bit(uid, &slbc_status)) {
				ref++;

				pr_info("#@# %s(%d) %s not released !!!\n",
						__func__, __LINE__,
						slbc_uid_str[uid]);
			} else {
				clear_bit(uid, &slbc_release_status);
				slbc_debug_log("%s: slbc_release_status %lx",
						__func__, slbc_release_status);

				pr_info("#@# %s(%d) %s released !!!\n",
						__func__, __LINE__,
						slbc_uid_str[uid]);
			}
		}
	}

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
	int ret;

	if (slbc_enable == 0)
		return -EDISABLED;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	ops = container_of(&d, struct slbc_ops, data);
	if (ops && ops->activate) {
		ret = slbc_request(d);
		if (ret) {
			pr_info("#@# %s(%d) %s request fail !!!\n",
					__func__, __LINE__, slbc_uid_str[uid]);

			return ret;
		}

		if (ops->activate(d) == CB_DONE) {
			pr_info("#@# %s(%d) %s activate fail !!!\n",
					__func__, __LINE__, slbc_uid_str[uid]);

			ret = slbc_release(d);
			if (ret) {
				pr_info("#@# %s(%d) %s release fail !!!\n",
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

	pr_info("#@# %s(%d) %s data not found !!!\n",
			__func__, __LINE__, slbc_uid_str[uid]);

	return -EFAULT;
}
EXPORT_SYMBOL_GPL(slbc_activate);

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

		set_bit(uid, &slbc_release_status);
		slbc_debug_log("%s: slbc_release_status %lx", __func__,
				slbc_release_status);
		expires = jiffies + SLBC_CHECK_TIME;
		mod_timer(&slbc_deactivate_timer, expires);

		return 0;
	}

	pr_info("#@# %s(%d) %s data not found !!!\n",
			__func__, __LINE__, slbc_uid_str[uid]);

	return -EFAULT;
}
EXPORT_SYMBOL_GPL(slbc_deactivate);

#ifdef SLBC_THREAD
static struct slbc_data *slbc_find_next_low_used(struct slbc_data *d_old)
{
	struct slbc_ops *ops;
	struct slbc_config *config_old = d_old->config;
	unsigned int p_old = config_old->priority;
	unsigned int res_old = config_old->res_slot;
	struct slbc_data *d_used = NULL;

	slbc_debug_log("%s: slbc_status %lx", __func__, slbc_status);

	list_for_each_entry(ops, &slbc_ops_list, node) {
		struct slbc_data *d = ops->data;
		struct slbc_config *config = d->config;
		unsigned int uid = d->uid;
		unsigned int p = config->priority;

		if (test_bit(uid, &slbc_status) && (p > p_old) &&
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

	slbc_debug_log("%s: slbc_req_status %lx", __func__, slbc_req_status);

	list_for_each_entry(ops, &slbc_ops_list, node) {
		struct slbc_data *d = ops->data;
		struct slbc_config *config = d->config;
		unsigned int res = config->res_slot;
		unsigned int uid = d->uid;
		unsigned int p = config->priority;

		if (test_bit(uid, &slbc_req_status) && (p <= p_old) &&
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

	/* FIXME: */
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
		/* FIXME: */
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

	/* FIXME: */
	/* check all need slot */
	d->slot_used = res;
}

static int find_slbc_slot_by_data(struct slbc_data *d)
{
	unsigned int uid = d->uid;

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	slbc_debug_log("%s: slbc_slot_status %lx", __func__, slbc_slot_status);
	if (!(slbc_slot_status & d->slot_used))
		return SLOT_AVAILABLE;

	return SLOT_USED;
}

static void set_slbc_slot_by_data(struct slbc_data *d)
{
	slbc_slot_status |= d->slot_used;
	slbc_debug_log("%s: slbc_slot_status %lx", __func__, slbc_slot_status);
}

static void clr_slbc_slot_by_data(struct slbc_data *d)
{
	slbc_slot_status &= ~d->slot_used;
	slbc_debug_log("%s: slbc_slot_status %lx", __func__, slbc_slot_status);
}

int slbc_request(struct slbc_data *d)
{
	int ret = 0;
	unsigned int uid;
	unsigned int sid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (d == 0)
		return -EINVAL;

	if (d->uid <= 0)
		return -EINVAL;

	uid = d->uid;

	sid = get_slbc_sid_by_uid(uid);
	if (sid != SID_NOT_FOUND) {
		d->sid = sid;
		d->config = &p_config[sid];
		d->slot_used = 0;
		d->ref = 0;
	} else {
		pr_info("#@# %s(%d) slot is wrong !!!\n", __func__, __LINE__);

		return -EINVAL;
	}

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	slbc_debug_log("%s: slbc_mask_status %lx", __func__, slbc_mask_status);
	ret = test_bit(uid, &slbc_mask_status);
	if (ret == 1)
		return -EREQ_MASKED;

	set_bit(uid, &slbc_req_status);
	slbc_debug_log("%s: slbc_req_status %lx", __func__, slbc_req_status);

	slbc_debug_log("%s: slbc_status %lx", __func__, slbc_status);
	ret = test_bit(uid, &slbc_status);
	if (ret == 1) {
		slbc_set_mmsram_data(d);

		goto request_done1;
	}

	if (BIT_IN_MM_BITS_1(BIT(uid)) &&
			!BIT_IN_MM_BITS_1(slbc_status)) {
		slbc_set_mmsram_data(d);

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

			return -EWAIT_RELEASE;
		}
#endif /* SLBC_THREAD */

		return -ENOT_AVAILABLE;
	}

	mutex_lock(&slbc_ops_lock);

	if ((d->type) == TP_BUFFER) {
		slbc_debug_log("%s: TP_BUFFER\n", __func__);
#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
		if (!buffer_ref) {
			d->pwr_ref++;
			enable_mmsram();
		}
#endif /* CONFIG_MTK_SLBC_MMSRAM */

		slbc_set_mmsram_data(d);

		buffer_ref++;
	}

#if IS_ENABLED(CONFIG_MTK_L3C_PART)
	if ((d->type) == TP_CACHE) {
		slbc_debug_log("%s: TP_CACHE\n", __func__);
		if (cache_ref++ == 0) {
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
	}
#endif /* CONFIG_MTK_L3C_PART */

	mutex_unlock(&slbc_ops_lock);

	set_slbc_slot_by_data(d);

#ifdef SLBC_TRACE
	trace_slbc_data((void *)__func__, d);
#endif /* SLBC_TRACE */

request_done:
	set_bit(uid, &slbc_status);
	slbc_debug_log("%s: slbc_status %lx", __func__, slbc_status);
	clear_bit(uid, &slbc_req_status);
	slbc_debug_log("%s: slbc_req_status %lx", __func__, slbc_req_status);

request_done1:
	slbc_ref++;
	d->ref++;
	uid_ref[uid]++;

	return 0;
}
EXPORT_SYMBOL_GPL(slbc_request);

static void slbc_debug_dump_data(struct slbc_data *d)
{
	unsigned int uid = d->uid;

	pr_info("\nID %s", slbc_uid_str[uid]);

	if (test_bit(uid, &slbc_status))
		pr_info(" activate\n");
	else
		pr_info(" deactivate\n");

	pr_info("\t%d\t", uid);
	pr_info("%x\t", d->type);
	pr_info("%ld\n", d->size);
	pr_info("%p\t", d->paddr);
	pr_info("%p\t", d->vaddr);
	pr_info("%d\t", d->sid);
	pr_info("%x\n", d->slot_used);
	pr_info("%p\n", d->config);
	pr_info("%d\n", d->ref);
	pr_info("%d\n", d->pwr_ref);
}

static int slbc_debug_all(void)
{
	struct slbc_ops *ops;
	int i;

	pr_info("slbc_enable %x\n", slbc_enable);
	pr_info("slbc_status %lx\n", slbc_status);
	pr_info("slbc_mask_status %lx\n", slbc_mask_status);
	pr_info("slbc_req_status %lx\n", slbc_req_status);
	pr_info("slbc_release_status %lx\n", slbc_release_status);
	pr_info("slbc_slot_status %lx\n", slbc_slot_status);
	pr_info("buffer_ref %x\n", buffer_ref);
	pr_info("cache_ref %x\n", cache_ref);
	pr_info("slbc_ref %x\n", slbc_ref);
	for (i = 0; i < UID_MAX; i++)
		pr_info("uid_ref %s %x\n", slbc_uid_str[i], uid_ref[i]);

	mutex_lock(&slbc_ops_lock);
	list_for_each_entry(ops, &slbc_ops_list, node) {
		struct slbc_data *d = ops->data;

		slbc_debug_dump_data(d);
	}
	mutex_unlock(&slbc_ops_lock);

	pr_info("\n");

	return 0;
}

int slbc_release(struct slbc_data *d)
{
	int ret = 0;
#ifdef SLBC_THREAD
	struct slbc_data *d_req;
#endif /* SLBC_THREAD */
	unsigned int uid;
	unsigned int sid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (d == 0)
		return -EINVAL;

	if (d->uid <= 0)
		return -EINVAL;

	uid = d->uid;

	sid = get_slbc_sid_by_uid(uid);
	if (sid != SID_NOT_FOUND) {
		d->sid = sid;
		d->config = &p_config[sid];
	} else {
		pr_info("#@# %s(%d) slot is wrong !!!\n", __func__, __LINE__);

		return -EINVAL;
	}

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	slbc_debug_log("%s: slbc_mask_status %lx", __func__, slbc_mask_status);
	ret = test_bit(uid, &slbc_mask_status);
	if (ret == 1)
		return -EREQ_MASKED;

	slbc_debug_log("%s: slbc_status %lx", __func__, slbc_status);
	ret = test_bit(uid, &slbc_status);
	if (ret == 0) {
		slbc_clr_mmsram_data(d);

		goto release_done1;
	}

	if (uid_ref[uid] > 1) {
		slbc_clr_mmsram_data(d);

		goto release_done1;
	}

	if (BIT_IN_MM_BITS_1(BIT(uid)) &&
			!BIT_IN_MM_BITS_1(slbc_status & ~BIT(uid))) {
		slbc_clr_mmsram_data(d);

		goto release_done;
	}

	mutex_lock(&slbc_ops_lock);

	if ((d->type) == TP_BUFFER) {
		slbc_debug_log("%s: TP_BUFFER\n", __func__);

		buffer_ref--;
		WARN_ON(buffer_ref < 0);

		slbc_clr_mmsram_data(d);

#if IS_ENABLED(CONFIG_MTK_SLBC_MMSRAM)
		if (!buffer_ref) {
			d->pwr_ref--;
			disable_mmsram();
		}
#endif /* CONFIG_MTK_SLBC_MMSRAM */
	}

#if IS_ENABLED(CONFIG_MTK_L3C_PART)
	if ((d->type) == TP_CACHE) {
		slbc_debug_log("%s: TP_CACHE\n", __func__);
		if (--cache_ref == 0) {
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
		WARN_ON(cache_ref < 0);
	}
#endif /* CONFIG_MTK_L3C_PART */

	mutex_unlock(&slbc_ops_lock);

	clr_slbc_slot_by_data(d);

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
	clear_bit(uid, &slbc_status);
	slbc_debug_log("%s: slbc_status %lx", __func__, slbc_status);

release_done1:
	--slbc_ref;
	d->ref--;
	uid_ref[uid]--;
	WARN((d->ref < 0) || (uid_ref[uid] < 0),
			"%s: release %s fail !!! %d %d\n",
			__func__, slbc_uid_str[uid], d->ref, uid_ref[uid]);

	if ((d->ref < 0) || (uid_ref[uid] < 0))
		slbc_debug_all();

	return 0;
}
EXPORT_SYMBOL_GPL(slbc_release);

int slbc_power_on(struct slbc_data *d)
{
	unsigned int uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (d == 0)
		return -EINVAL;

	if (d->uid <= 0)
		return -EINVAL;

	uid = d->uid;

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
EXPORT_SYMBOL_GPL(slbc_power_on);

int slbc_power_off(struct slbc_data *d)
{
	unsigned int uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (d == 0)
		return -EINVAL;

	if (d->uid <= 0)
		return -EINVAL;

	uid = d->uid;

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
EXPORT_SYMBOL_GPL(slbc_power_off);

int slbc_secure_on(struct slbc_data *d)
{
	unsigned int uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (d == 0)
		return -EINVAL;

	if (d->uid <= 0)
		return -EINVAL;

	uid = d->uid;

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
EXPORT_SYMBOL_GPL(slbc_secure_on);

int slbc_secure_off(struct slbc_data *d)
{
	unsigned int uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (d == 0)
		return -EINVAL;

	if (d->uid <= 0)
		return -EINVAL;

	uid = d->uid;

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
EXPORT_SYMBOL_GPL(slbc_secure_off);

static void slbc_dump_data(struct seq_file *m, struct slbc_data *d)
{
	unsigned int uid = d->uid;

	seq_printf(m, "\nID %s", slbc_uid_str[uid]);

	if (test_bit(uid, &slbc_status))
		seq_puts(m, " activate\n");
	else
		seq_puts(m, " deactivate\n");

	seq_printf(m, "\t%d\t", uid);
	seq_printf(m, "%x\t", d->type);
	seq_printf(m, "%ld\n", d->size);
	seq_printf(m, "%p\t", d->paddr);
	seq_printf(m, "%p\t", d->vaddr);
	seq_printf(m, "%d\t", d->sid);
	seq_printf(m, "%x\n", d->slot_used);
	seq_printf(m, "%p\n", d->config);
	seq_printf(m, "%d\n", d->ref);
	seq_printf(m, "%d\n", d->pwr_ref);
}

static int dbg_slbc_proc_show(struct seq_file *m, void *v)
{
	struct slbc_ops *ops;
	int i;

	seq_printf(m, "slbc_enable %x\n", slbc_enable);
	seq_printf(m, "slbc_status %lx\n", slbc_status);
	seq_printf(m, "slbc_mask_status %lx\n", slbc_mask_status);
	seq_printf(m, "slbc_req_status %lx\n", slbc_req_status);
	seq_printf(m, "slbc_release_status %lx\n", slbc_release_status);
	seq_printf(m, "slbc_slot_status %lx\n", slbc_slot_status);
	seq_printf(m, "buffer_ref %x\n", buffer_ref);
	seq_printf(m, "cache_ref %x\n", cache_ref);
	seq_printf(m, "slbc_ref %x\n", slbc_ref);
	for (i = 0; i < UID_MAX; i++)
		seq_printf(m, "uid_ref %s %x\n", slbc_uid_str[i], uid_ref[i]);

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
	int ret;
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
		slbc_enable = val_1;
		if (slbc_enable == 0) {
			struct slbc_ops *ops;

			mutex_lock(&slbc_ops_lock);
			list_for_each_entry(ops, &slbc_ops_list, node) {
				struct slbc_data *d = ops->data;
				unsigned int uid = d->uid;

				if (test_bit(uid, &slbc_status))
					ops->deactivate(d);
			}
			mutex_unlock(&slbc_ops_lock);
		}
	} else if (!strcmp(cmd, "slbc_status"))
		slbc_status = val_1;
	else if (!strcmp(cmd, "slbc_mask_status"))
		slbc_mask_status = val_1;
	else if (!strcmp(cmd, "slbc_req_status"))
		slbc_req_status = val_1;
	else if (!strcmp(cmd, "slbc_release_status"))
		slbc_release_status = val_1;
	else if (!strcmp(cmd, "slbc_slot_status"))
		slbc_slot_status = val_1;
	else if (!strcmp(cmd, "test_request")) {
		test_d.uid = UID_TEST;
		test_d.type  = TP_CACHE;
		test_d.flag = val_1;
		slbc_request(&test_d);
	} else if (!strcmp(cmd, "test_release")) {
		test_d.uid = UID_TEST;
		test_d.type  = TP_CACHE;
		test_d.flag = val_1;
		slbc_release(&test_d);
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
		const struct file_operations *fops;
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

int __init slbc_module_init(void)
{
	struct device_node *node;
	int ret;
	const char *buf;

	node = of_find_compatible_node(NULL, NULL,
			"mediatek,slbc");
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

#ifdef CONFIG_PM_SLEEP
	slbc_ws = wakeup_source_register(NULL, "slbc");
	if (!slbc_ws)
		pr_debug("slbc wakelock register fail!\n");
#endif /* CONFIG_PM_SLEEP */

	ret = slbc_create_debug_fs();
	if (ret) {
		pr_info("FAILED TO CREATE DEBUG FILESYSTEM (%d)\n", ret);

		return ret;
	}

	timer_setup(&slbc_deactivate_timer, slbc_deactivate_timer_fn,
			TIMER_DEFERRABLE);

	return 0;
}

late_initcall(slbc_module_init);

MODULE_DESCRIPTION("SLBC Driver v0.1");
