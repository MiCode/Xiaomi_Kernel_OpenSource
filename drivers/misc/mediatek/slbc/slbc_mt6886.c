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
#include <linux/slab.h>

#include <slbc.h>
#include <slbc_ops.h>
#include <slbc_ipi.h>
#include <mtk_slbc_sram.h>

/* #define CREATE_TRACE_POINTS */
/* #include <slbc_events.h> */
#define trace_slbc_api(f, id)
#define trace_slbc_data(f, data)

#include <linux/kthread.h>
#include <linux/pm_qos.h>
#include <linux/cpuidle.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <linux/delay.h>

static struct pm_qos_request slbc_qos_request;

#define SLBC_WAY_SIZE			0x80000

#if IS_ENABLED(CONFIG_MTK_L3C_PART)
#include <l3c_part.h>
#endif /* CONFIG_MTK_L3C_PART */

/* #define SLBC_TRACE */
#define ENABLE_SLBC
#define SLBC_CB
#define SLBC_CB_THREAD
/* #define SLBC_CB_SLEEP */
/* #define SLBC_CB_TEST */

#define SLBC_WAY_A_BASE			0x0f000000
#define SLBC_WAY_B_BASE			0x680000000
#define SLBC_PADDR_MASK			0x00ffffff
#define SLBC_UID_VALID(uid)		(((uid) > UID_ZERO) && ((uid) < UID_MAX))
#define SLBC_SID_VALID(sid)		(((sid) >= 0) && ((sid) < ARRAY_SIZE(p_config)))
#define SLBC_GET_RIGHTMOST(x)		((x) & (~(x) + 1))
#define SLBC_SID_SRAM_IN_USED(sid)	(slbc_sram_read(SLBC_SLOT_USED) & p_config[(sid)].res_slot)

static struct mtk_slbc *slbc;

#define _BIT_(_bit_)		((uint32_t)(1 << (_bit_)))
#define SLBC_CHECK_TIME		msecs_to_jiffies(1000)
#define SLBC_CHECK_TIMEOUT	msecs_to_jiffies(5000)
#define SLBC_TIMEOUT_LIMIT	500

static int slb_disable;
static int slc_disable;
static int slbc_sram_enable;
static u32 slbc_force;
static int buffer_ref;
static u32 acp_ref;
static u32 slbc_ref;
static u32 slbc_sta;
static u32 slbc_ack_c;
static u32 slbc_ack_g;
static u32 cpuqos_mode;
static u32 slbc_sram_con;
static u32 slbc_cache_used;
static u32 slbc_pmu_0;
static u32 slbc_pmu_1;
static u32 slbc_pmu_2;
static u32 slbc_pmu_3;
static u32 slbc_pmu_4;
static u32 slbc_pmu_5;
static u32 slbc_pmu_6;
static int debug_level;
static int uid_ref[UID_MAX];
static int slbc_mic_num = 3;
static int slbc_inner = 5;
static int slbc_outer = 5;

static u64 req_val_count;
static u64 rel_val_count;
static u64 req_val_min;
static u64 req_val_max;
static u64 req_val_total;
static u64 rel_val_min;
static u64 rel_val_max;
static u64 rel_val_total;

static struct slbc_data test_d;

static struct timer_list slbc_deactivate_timer;
static LIST_HEAD(slbc_ops_list);
static DEFINE_MUTEX(slbc_ops_lock);
static DEFINE_MUTEX(slbc_req_lock);
static DEFINE_MUTEX(slbc_rel_lock);
DECLARE_WAIT_QUEUE_HEAD(slbc_wq);

/* 1 in bit is from request done to relase done */
static unsigned long slbc_uid_used;
/* 1 in bit is for mask */
static unsigned long slbc_sid_mask;
/* 1 in bit is under request flow */
static unsigned long slbc_sid_req_q;
/* 1 in bit is under release flow */
static unsigned long slbc_sid_rel_q;
/* 1 in bit is for slot ussage */
static unsigned long slbc_slot_used;
/* 1 in bit is for timeout uid */
static unsigned long slbc_uid_timeout;
/* 1 in bit is for CB fail sid */
static unsigned long slbc_sid_cb_fail;

#ifdef SLBC_CB_TEST
int user_activate(struct slbc_data *d)
{
	pr_info("%s: %s", __func__,  slbc_uid_str[d->uid]);
	slbc_request(d);
	return CB_OK;
}

int user_deactivate(struct slbc_data *d)
{
	pr_info("%s: %s", __func__, slbc_uid_str[d->uid]);
	mdelay(50);
	slbc_release(d);
	return CB_OK;
}

void user_cb_register(void)
{
	struct slbc_data venc_d = {.uid = UID_MM_VENC, .type = TP_BUFFER};
	struct slbc_data mml_d = {.uid = UID_MML, .type = TP_BUFFER};
	struct slbc_data disp_d = {.uid = UID_DISP, .type = TP_BUFFER};
	/* struct slbc_data ainr_d = {.uid = UID_AINR, .type = TP_BUFFER}; */

	struct slbc_ops venc_op = {.data = &venc_d,
			.activate = user_activate, .deactivate = user_deactivate};
	struct slbc_ops mml_op = {.data = &mml_d,
			.activate = user_activate, .deactivate = user_deactivate};
	struct slbc_ops disp_op = {.data = &disp_d,
			.activate = user_activate, .deactivate = user_deactivate};
	/* struct slbc_ops ainr_op = {.data = &ainr_d, */
	/* .activate = user_activate, .deactivate = user_deactivate}; */

	slbc_register_activate_ops(&venc_op);
	slbc_register_activate_ops(&mml_op);
	slbc_register_activate_ops(&disp_op);
	/* slbc_register_activate_ops(&ainr_op); */
}
#endif /* SLBC_CB_TEST */

static struct slbc_config p_config[] = {
	/* SLBC_ENTRY(id, sid, max, fix, p, extra, res, cache) */
	SLBC_ENTRY(UID_MM_VENC, 0, 0, 0, 1, 0x0, 0x3, 0),
	SLBC_ENTRY(UID_MML, 1, 0, 0, 1, 0x0, 0x3, 0),
	SLBC_ENTRY(UID_DISP, 2, 0, 0, 1, 0x0, 0x3, 0),
	SLBC_ENTRY(UID_AINR, 3, 0, 0, 0, 0x0, 0x3, 0),
};

#ifdef SLBC_CB_THREAD
static struct task_struct *slbc_activate_task[ARRAY_SIZE(p_config)];
static struct task_struct *slbc_deactivate_task[ARRAY_SIZE(p_config)];
#endif /* SLBC_CB_THREAD */

u32 slbc_sram_read(u32 offset)
{
	if (!slbc_enable)
		return 0;

	if (!slbc_sram_enable)
		return 0;

	if (!slbc->sram_vaddr || offset >= slbc->regsize)
		return 0;

	/* pr_info("#@# %s(%d) regs 0x%x 0%x\n", __func__, __LINE__, slbc->regs, offset); */

	return readl(slbc->sram_vaddr + offset);
}

void slbc_sram_write(u32 offset, u32 val)
{
	if (!slbc_enable)
		return;

	if (!slbc_sram_enable)
		return;

	if (!slbc->sram_vaddr || offset >= slbc->regsize)
		return;

	/* pr_info("#@# %s(%d) regs 0x%x 0%x\n", __func__, __LINE__, slbc->regs, offset); */

	writel(val, slbc->sram_vaddr + offset);
}

void slbc_sram_init(struct mtk_slbc *slbc)
{
	int i;

	pr_info("slbc_sram addr:0x%lx len:%d\n",
			(unsigned long)slbc->regs, slbc->regsize);

	/* print_hex_dump(KERN_INFO, "SLBC: ", DUMP_PREFIX_OFFSET, */
	/* 16, 4, slbc->sram_vaddr, slbc->regsize, 1); */

	for (i = 0; i < slbc->regsize; i += 4)
		writel(0x0, slbc->sram_vaddr + i);
}

static void slbc_set_sram_data(struct slbc_data *d)
{
	pr_info("slbc: set pa:%lx va:%lx\n",
			(unsigned long)d->paddr, (unsigned long)d->vaddr);
}

static void slbc_clr_sram_data(struct slbc_data *d)
{
	pr_info("slbc: clr pa:%lx va:%lx\n",
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

static u32 slbc_read_debug_sram(int sid)
{
	if (sid < 0 || sid >= ARRAY_SIZE(p_config) || sid >= 16)
		return SID_NOT_FOUND;

	if (sid < 8)
		return slbc_sram_read(SLBC_DEBUG_0 + sid * 4);
	else
		return slbc_sram_read(SLBC_DEBUG_8 + (sid - 8) * 4);
}

void slbc_force_cmd(unsigned int force)
{
	if ((force & 0xffff) < FR_MAX) {
		slbc_force = force;
		slbc_force_scmi_cmd(force);
	}
}

static unsigned int slbc_get_dc_uid(struct slbc_data *d)
{
	unsigned int uid = d->uid;
	unsigned int uid_dc = 0;
	unsigned int i;

	if (BIT_IN_MM_BITS_3(_BIT_(uid)))
		uid_dc = UID_MM_BITS_3 & ~_BIT_(uid);

	if (uid_dc) {
		i = 0;
		while ((uid_dc >> i) != 1)
			i++;
		return i;
	}

	return 0;
}

static unsigned int slbc_find_high_in_used(struct slbc_data *d)
{
	unsigned int i;
	unsigned int sid = slbc_get_sid_by_uid((enum slbc_uid)d->uid);

	if (sid == SID_NOT_FOUND)
		return SID_NOT_FOUND;

	slbc_uid_used = slbc_sram_read(SLBC_UID_USED);
	for (i = 0; i < ARRAY_SIZE(p_config); i++) {
		if (test_bit(p_config[i].uid, &slbc_uid_used) &&
				(p_config[i].uid != d->uid) &&
				(p_config[i].priority <= p_config[sid].priority) &&
				(p_config[i].res_slot & p_config[sid].res_slot)) {
			return p_config[i].uid;
		}
	}
	return 0;
}

int slbc_activate_status(struct slbc_data *d)
{
	unsigned int sid;
	unsigned int dc_uid;

	if (!d)
		return 0;

	sid = slbc_get_sid_by_uid(d->uid);
	dc_uid = slbc_get_dc_uid(d);

	if (sid == SID_NOT_FOUND)
		return SID_NOT_FOUND;

	slbc_uid_used = slbc_sram_read(SLBC_UID_USED);
	if (test_bit(d->uid, &slbc_uid_used))
		return 0;

	if (dc_uid && test_bit(dc_uid, &slbc_uid_used))
		return 0;

	if (!SLBC_SID_SRAM_IN_USED(sid))
		return 0;

	if (slbc_find_high_in_used(d))
		return -ENOT_AVAILABLE;
	else
		return -EWAIT_RELEASE;
}

#ifdef SLBC_CB
static int find_slbc_slot_by_data(struct slbc_data *d)
{
	unsigned int uid = d->uid;
	int sid = slbc_get_sid_by_uid((enum slbc_uid)uid);

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */

	/* slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]); */

	if (sid == SID_NOT_FOUND)
		return SLOT_NOT_FOUND;

	/* slbc_debug_log("%s: slbc_slot_used %lx", __func__, slbc_sram_read(SLBC_SLOT_USED)); */

	if (!SLBC_SID_SRAM_IN_USED(sid))
		return SLOT_AVAILABLE;

	return SLOT_USED;
}

static int slbc_request_check(struct slbc_data *d)
{
	unsigned int uid = d->uid;
	int ret = 0;

	if (BIT_IN_MM_BITS_3(_BIT_(uid)) &&
		BIT_IN_MM_BITS_3(slbc_sram_read(SLBC_UID_USED))) {
		ret = UID_MM_BITS_3 & ~_BIT_(uid);
	}

	return ret;
}

static int slbc_release_check(struct slbc_data *d)
{
	unsigned int uid = d->uid;
	int ret = 0;

	if (BIT_IN_MM_BITS_3(_BIT_(uid)) &&
		BIT_IN_MM_BITS_3(slbc_sram_read(SLBC_UID_USED) & ~_BIT_(uid))) {
		ret = UID_MM_BITS_3 & ~_BIT_(uid);
	}

	return ret;
}

static void slbc_deactivate_timer_fn(struct timer_list *timer)
{
	int ref = 0;
	unsigned long expires;
	unsigned int sid;
	unsigned int uid;

	slbc_debug_log("%s: slbc_sid_rel_q %lx, slbc_uid_timeout %lx",
			__func__, slbc_sid_rel_q, slbc_uid_timeout);


	if (slbc_sid_rel_q) {
		for (sid = 0; sid < ARRAY_SIZE(p_config); sid++) {
			uid = p_config[sid].uid;
			if (test_bit(sid, &slbc_sid_rel_q)) {
#ifdef SLBC_TRACE
				trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
				slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

				slbc_uid_used = slbc_sram_read(SLBC_UID_USED);
				if (test_bit(uid, &slbc_uid_used)) {
					ref++;

					slbc_debug_log("%s(%d) %s not released!",
							__func__, __LINE__, slbc_uid_str[uid]);
				} else {
					clear_bit(sid, &slbc_sid_rel_q);
					slbc_sram_write(SLBC_SID_REL_Q, slbc_sid_rel_q);
					slbc_debug_log("%s: slbc_sid_rel_q %lx",
							__func__, slbc_sid_rel_q);

					slbc_debug_log("%s: %s released!",
							__func__, slbc_uid_str[uid]);
				}
			}
		}
	}

	if (slbc_uid_timeout) {
		for (sid = 0; sid < ARRAY_SIZE(p_config); sid++) {
			uid = p_config[sid].uid;
			if (test_bit(uid, &slbc_uid_timeout))
				slbc_debug_log("%s(%d) %s slb req timeout!",
						__func__, __LINE__, slbc_uid_str[uid]);
		}
	}

	if (ref) {
		expires = jiffies + SLBC_CHECK_TIME;
		mod_timer(&slbc_deactivate_timer, expires);
	} else if (slbc_uid_timeout) {
		expires = jiffies + SLBC_CHECK_TIMEOUT;
		mod_timer(&slbc_deactivate_timer, expires);
	}
}

#ifdef SLBC_CB_THREAD
static int slbc_activate_thread(void *arg)
{
	struct slbc_ops *ops = arg;
	struct slbc_data *d;
	unsigned int uid;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (!ops)
		return -EFAULT;
	d = ops->data;

	if (d->uid <= UID_ZERO || d->uid >= UID_MAX)
		return -EINVAL;
	uid = d->uid;


#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	if (ops->activate) {
		slbc_debug_log("%s: %s activate CB run!",
				__func__, slbc_uid_str[uid]);
		if (ops->activate(d) == CB_DONE) {
			slbc_debug_log("%s(%d) %s activate CB fail!",
					__func__, __LINE__, slbc_uid_str[uid]);
		} else {
#ifdef SLBC_TRACE
			trace_slbc_data((void *)__func__, d);
#endif /* SLBC_TRACE */
			slbc_debug_log("%s: %s activate CB done!",
					__func__, slbc_uid_str[uid]);
		}
		return 0;
	}

	slbc_debug_log("%s(%d) %s activate CB not found!",
			__func__, __LINE__, slbc_uid_str[uid]);

	return -EFAULT;
}

static int slbc_deactivate_thread(void *arg)
{
	struct slbc_ops *ops = arg;
	struct slbc_data *d;
	unsigned int uid;
	unsigned int sid;
	unsigned long expires;

	if (slbc_enable == 0)
		return -EDISABLED;

	if (!ops)
		return -EFAULT;
	d = ops->data;

	if (d->uid <= UID_ZERO || d->uid >= UID_MAX)
		return -EINVAL;
	uid = d->uid;
	sid = slbc_get_sid_by_uid((enum slbc_uid)uid);

#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s", __func__, slbc_uid_str[uid]);

	if (ops->deactivate) {
		slbc_debug_log("%s: %s deactivate CB run!",
				__func__, slbc_uid_str[uid]);
		if (ops->deactivate(d) == CB_DONE) {
			slbc_debug_log("%s(%d) %s deactivate CB fail!",
					__func__, __LINE__, slbc_uid_str[uid]);

			/* add user to cb_fail */
			set_bit(sid, &slbc_sid_cb_fail);
			/* slbc_debug_log("%s: sid %d CB fail, add to slbc_sid_cb_fail %lx", */
					/* __func__, sid, slbc_sid_cb_fail); */

#ifdef SLBC_CB_SLEEP
			/* trigger wait queue wakup */
			wake_up_all(&slbc_wq);
#endif /* SLBC_CB_SLEEP */
		} else {
#ifdef SLBC_TRACE
			trace_slbc_data((void *)__func__, d);
#endif /* SLBC_TRACE */
			slbc_debug_log("%s: %s deactivate CB done!",
					__func__, slbc_uid_str[uid]);

			/* add user to req_q */
			set_bit(sid, &slbc_sid_req_q);
			slbc_sram_write(SLBC_SID_REQ_Q, slbc_sid_req_q);
			/* slbc_debug_log("%s: sid %d be deactivate, add to slbc_sid_req_q %lx", */
					/* __func__, sid, slbc_sid_req_q); */

			/* add user to rel_q */
			set_bit(sid, &slbc_sid_rel_q);
			slbc_sram_write(SLBC_SID_REL_Q, slbc_sid_rel_q);
			/* slbc_debug_log("%s: wait sid %d release, add to slbc_sid_rel_q %lx", */
					/* __func__, sid, slbc_sid_rel_q); */

			/* clr user from cb_fail */
			clear_bit(sid, &slbc_sid_cb_fail);
			/* slbc_debug_log("%s: sid %d CB success, clr from slbc_sid_cb_fail %lx", */
					/* __func__, sid, slbc_sid_cb_fail); */

			/* timer */
			expires = jiffies + SLBC_CHECK_TIME;
			mod_timer(&slbc_deactivate_timer, expires);
		}
		return 0;
	}

	slbc_debug_log("%s(%d) %s deactivate CB not found!",
			__func__, __LINE__, slbc_uid_str[uid]);

	return -EFAULT;
}
#endif /* SLBC_CB_THREAD */

static unsigned long slbc_find_next_low_used(struct slbc_data *d_old)
{
	unsigned long ret = 0;
	struct slbc_ops *ops;
	struct slbc_config *config_old = &p_config[slbc_get_sid_by_uid(d_old->uid)];
	unsigned int uid_old = d_old->uid;
	unsigned int p_old = config_old->priority;
	unsigned int res_old = config_old->res_slot;
	unsigned int uid_check = slbc_find_high_in_used(d_old);

	slbc_uid_used = slbc_sram_read(SLBC_UID_USED);
	slbc_debug_log("%s: slbc_uid_used %lx", __func__, slbc_uid_used);

	/* check p_old have higher priority thans all user that have conflict sram */
	if (uid_check) {
		slbc_debug_log("%s: %s block by high priority user %s",
				__func__, slbc_uid_str[uid_old], slbc_uid_str[uid_check]);
		return 0;
	}

	list_for_each_entry(ops, &slbc_ops_list, node) {
		struct slbc_data *d = ops->data;
		struct slbc_config *config = d->config;
		unsigned int uid = d->uid;
		unsigned int sid = slbc_get_sid_by_uid(d->uid);
		unsigned int p = config->priority;
		unsigned int res = config->res_slot;

		if (test_bit(uid, &slbc_uid_used) &&
				!test_bit(sid, &slbc_sid_rel_q) &&
				(p > p_old) &&
				(res & res_old)) {
#ifdef SLBC_TRACE
			trace_slbc_data((void *)__func__, ops);
#endif /* SLBC_TRACE */
			slbc_debug_log("%s: %s find low priority user %s",
					__func__, slbc_uid_str[uid_old], slbc_uid_str[uid]);
			if (ops->deactivate) {
#ifdef SLBC_CB_THREAD
				slbc_deactivate_task[sid] = kthread_run(slbc_deactivate_thread,
						ops, "slbc_deactivate_thread");
#endif /* SLBC_CB_THREAD */
				set_bit(sid, &ret);
			} else {
				slbc_debug_log("%s: %s block by no CB user %s",
						__func__, slbc_uid_str[uid_old], slbc_uid_str[uid]);
				return 0;
			}
		}
	}
	return ret;
}

static struct slbc_ops *slbc_find_next_high_req(struct slbc_data *d_old,
						unsigned long slbc_sid_req_list)
{
	struct slbc_ops *ops;
	unsigned int p_min = UID_MAX;
	struct slbc_config *config_old = &p_config[slbc_get_sid_by_uid(d_old->uid)];
	unsigned int uid_old = d_old->uid;
	unsigned int res_old = config_old->res_slot;
	unsigned int dc_old = slbc_get_dc_uid(d_old);
	struct slbc_ops *res_ops = NULL;

	slbc_debug_log("%s: slbc_sid_req_list %lx", __func__, slbc_sid_req_list);

	list_for_each_entry(ops, &slbc_ops_list, node) {
		struct slbc_data *d = ops->data;
		struct slbc_config *config = d->config;
		unsigned int uid = d->uid;
		unsigned int sid = slbc_get_sid_by_uid((enum slbc_uid)uid);
		unsigned int p = config->priority;
		unsigned int res = config->res_slot;

		if (test_bit(sid, &slbc_sid_req_list) &&
				(p < p_min) &&
				(uid != uid_old) &&
				(uid != dc_old) &&
				(res & res_old)) {
			p_min = p;
			res_ops = ops;
		}
	}

	if (!res_ops) {
		slbc_debug_log("%s: %s no user found", __func__, slbc_uid_str[uid_old]);
		return NULL;
	}

#ifdef SLBC_TRACE
	trace_slbc_data((void *)__func__, res_ops);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: %s find user %s",
			__func__, slbc_uid_str[uid_old], slbc_uid_str[res_ops->data->uid]);

	return res_ops;
}

static int slbc_activate_check(void *arg)
{
	struct slbc_data *d = arg;
	unsigned int uid = d->uid;
	unsigned int sid;
	unsigned int uid_dc;
	unsigned int sid_dc;

	struct slbc_ops *ops;
	struct slbc_ops *res_ops;
	struct slbc_config *res_config = NULL;
	unsigned long slbc_sid_req_list = slbc_sid_req_q;
	unsigned int slbc_act_slot = 0;


	if (uid <= UID_ZERO || uid >= UID_MAX) {
		slbc_debug_log("%s(%d) uid %x error!",
				__func__, __LINE__, uid);
		return 0;
	}
#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: trigger by %s release", __func__, slbc_uid_str[uid]);

	mutex_lock(&slbc_ops_lock);
	do {
		res_ops = slbc_find_next_high_req(d, slbc_sid_req_list);
		if (!res_ops)
			break;

		res_config = res_ops->data->config;
		if (!slbc_find_high_in_used(res_ops->data) &&
			!(slbc_act_slot & res_config->res_slot)) {
			slbc_debug_log("%s: slbc_act_slot %x, %s res_slot %x",
					__func__, slbc_act_slot, slbc_uid_str[res_ops->data->uid],
					res_config->res_slot);
			sid = slbc_get_sid_by_uid(res_ops->data->uid);
#ifdef SLBC_CB_THREAD
			slbc_activate_task[sid] = kthread_run(slbc_activate_thread,
					res_ops, "slbc_activate_thread");
#endif /* SLBC_CB_THREAD */
			slbc_act_slot |= res_config->res_slot;

			uid_dc = slbc_get_dc_uid(res_ops->data);
			sid_dc = slbc_get_sid_by_uid(uid_dc);
			if (sid_dc != SID_NOT_FOUND && test_bit(sid_dc, &slbc_sid_req_list)) {
				list_for_each_entry(ops, &slbc_ops_list, node) {
					if (uid_dc == ops->data->uid) {
#ifdef SLBC_CB_THREAD
						slbc_activate_task[sid_dc] =
								kthread_run(slbc_activate_thread,
								ops, "slbc_activate_thread");
#endif /* SLBC_CB_THREAD */
						break;
					}
				}
				clear_bit(sid_dc, &slbc_sid_req_list);
			}
		}
		clear_bit(slbc_get_sid_by_uid(res_ops->data->uid), &slbc_sid_req_list);
	} while (slbc_sid_req_list);
	mutex_unlock(&slbc_ops_lock);

	return 0;
}

static unsigned long slbc_deactivate_check(void *arg)
{
	struct slbc_data *d = arg;
	unsigned int uid = d->uid;
	unsigned long ret;

	if (uid <= UID_ZERO || uid >= UID_MAX) {
		slbc_debug_log("%s(%d) uid %x error!",
				__func__, __LINE__, uid);
		return 0;
	}
#ifdef SLBC_TRACE
	trace_slbc_api((void *)__func__, slbc_uid_str[uid]);
#endif /* SLBC_TRACE */
	slbc_debug_log("%s: trigger by %s request", __func__, slbc_uid_str[uid]);

	mutex_lock(&slbc_ops_lock);
	ret = slbc_find_next_low_used(d);
	mutex_unlock(&slbc_ops_lock);

	return ret;
}
#endif /* SLBC_CB */

int slbc_register_activate_ops(struct slbc_ops *u_ops)
{
#ifdef SLBC_CB
	struct slbc_data *d;
	struct slbc_ops *ops;
	struct slbc_ops *check_ops;
	int sid;

	if (!u_ops)
		return -EFAULT;

	if (!u_ops->data)
		return -EFAULT;

	sid = slbc_get_sid_by_uid((enum slbc_uid)u_ops->data->uid);
	if (sid == SID_NOT_FOUND) {
		slbc_debug_log("%s(%d) register slbc ops fail: invalid uid %d!",
				__func__, __LINE__, u_ops->data->uid);
		return -EFAULT;
	}

	list_for_each_entry(check_ops, &slbc_ops_list, node) {
		if (u_ops->data->uid == check_ops->data->uid) {
			slbc_debug_log("%s: %s register slbc ops has been done",
					__func__, slbc_uid_str[u_ops->data->uid]);
			return 0;
		}
	}

	if (u_ops->data->type == TP_BUFFER) {
		ops = kmalloc(sizeof(struct slbc_ops), GFP_KERNEL);
		d = kmalloc(sizeof(struct slbc_data), GFP_KERNEL);
		if (!ops || !d) {
			slbc_debug_log("%s(%d) %s register slbc ops fail: kmalloc error!",
					__func__, __LINE__, slbc_uid_str[u_ops->data->uid]);
			kfree(ops);
			kfree(d);
			return -EFAULT;
		}

		*ops = *u_ops;

		d->uid = u_ops->data->uid;
		d->type = TP_BUFFER;
		d->config = &p_config[sid];
		d->user_cb_data = u_ops->data->user_cb_data;

		ops->data = d;

		mutex_lock(&slbc_ops_lock);
		list_add_tail(&(ops->node), &slbc_ops_list);
		mutex_unlock(&slbc_ops_lock);

		slbc_debug_log("%s: %s register slbc ops success",
				__func__, slbc_uid_str[u_ops->data->uid]);
	} else {
		slbc_debug_log("%s(%d) %s register slbc ops fail: type error!",
				__func__, __LINE__, slbc_uid_str[u_ops->data->uid]);
		return -EFAULT;
	}
#endif /* SLBC_CB */
	return 0;
}

static int slbc_request_cache(struct slbc_data *d)
{
	int ret = 0;

	/* slbc_debug_log("%s: TP_CACHE\n", __func__); */

	ret = _slbc_request_cache_scmi(d);

	return ret;
}

static int slbc_request_status(struct slbc_data *d)
{
	int ret = 0;
	int uid = d->uid;
	int sid;
	struct slbc_config *config;

	/* slbc_debug_log("%s: TP_BUFFER\n", __func__); */

	if (uid <= UID_ZERO || uid > UID_MAX)
		d->config = NULL;
	else {
		sid = slbc_get_sid_by_uid((enum slbc_uid)uid);
		if (sid != SID_NOT_FOUND) {
			d->sid = sid;
			d->config = &p_config[sid];
			config = (struct slbc_config *)d->config;
			ret = _slbc_buffer_status_scmi(d);
		}
	}

	return ret;
}

static int slbc_request_buffer(struct slbc_data *d)
{
	int ret = 0;
	int uid = d->uid;
	int sid = slbc_get_sid_by_uid((enum slbc_uid)uid);
	struct slbc_config *config;
#ifdef SLBC_CB
	unsigned long deact_sid_list;
#ifndef SLBC_CB_SLEEP
	int i;
#endif /* SLBC_CB_SLEEP */
#endif /* SLBC_CB */

	mutex_lock(&slbc_req_lock);
	slbc_debug_log("%s: TP_BUFFER", __func__);

	if (!SLBC_UID_VALID(uid))
		d->config = NULL;
	else if (SLBC_SID_VALID(sid)) {
		d->sid = sid;
		d->config = &p_config[sid];
		config = (struct slbc_config *)d->config;
		slbc_debug_log("%s: %s req slb", __func__, slbc_uid_str[uid]);
#ifdef SLBC_CB
		set_bit(sid, &slbc_sid_req_q);
		slbc_sram_write(SLBC_SID_REQ_Q, slbc_sid_req_q);
		/* slbc_debug_log("%s: %s set sid %d to slbc_sid_req_q %lx", __func__, */
				/* slbc_uid_str[uid], sid, slbc_sid_req_q); */
#endif /* SLBC_CB */
	}
#ifdef SLBC_CB
	slbc_uid_used = slbc_sram_read(SLBC_UID_USED);
	if (SLBC_UID_VALID(uid) && SLBC_SID_VALID(sid) &&
		find_slbc_slot_by_data(d) == SLOT_USED &&
		!slbc_request_check(d) && !test_bit(uid, &slbc_uid_used)) {

		deact_sid_list = slbc_deactivate_check(d);

		if (!d->timeout) {
			slbc_debug_log("%s(%d) %s need to wait slb release!",
						__func__, __LINE__, slbc_uid_str[uid]);
			ret = -EWAIT_RELEASE;
		} else if (d->timeout < SLBC_TIMEOUT_LIMIT) {
			if (deact_sid_list) {
				slbc_sid_cb_fail &= ~deact_sid_list;
				slbc_debug_log("%s: %s clr slbc_sid_cb_fail %lx on %s %lx",
						__func__, slbc_uid_str[uid],
						slbc_sid_cb_fail,
						"deact_sid_list", deact_sid_list);

				slbc_debug_log("%s: %s start to wait slb, timeout %dms",
						__func__, slbc_uid_str[uid], d->timeout);
#ifdef SLBC_CB_SLEEP
				ret = wait_event_timeout(slbc_wq,
						!SLBC_SID_SRAM_IN_USED(sid) ||
						(deact_sid_list & slbc_sid_cb_fail),
						msecs_to_jiffies(d->timeout));
#else /* SLBC_CB_SLEEP */
				ret = 0;
				for (i = 0; i < d->timeout; i++) {
					mdelay(1);
					if (!SLBC_SID_SRAM_IN_USED(sid) ||
							(deact_sid_list & slbc_sid_cb_fail)) {
						ret = 1;
						break;
					}
				}
#endif /* SLBC_CB_SLEEP */
				slbc_debug_log("%s: %s stop to wait slb, ret %d",
						__func__, slbc_uid_str[uid], ret);
				if (ret) {
					if (!SLBC_SID_SRAM_IN_USED(sid)) {
						slbc_debug_log("%s: %s wait success", __func__,
								slbc_uid_str[uid]);
						ret = _slbc_request_buffer_scmi(d);
					} else {
						slbc_debug_log("%s(%d) %s wait fail: %s %lx",
								__func__, __LINE__,
								slbc_uid_str[uid],
								"slbc_sid_cb_fail",
								slbc_sid_cb_fail);
						ret = -EREQ_WAIT_FAIL;
					}
				} else {
					slbc_debug_log("%s(%d) %s wait timeout!",
							__func__, __LINE__, slbc_uid_str[uid]);
					set_bit(uid, &slbc_uid_timeout);
					ret = -EREQ_TIMEOUT;
				}
			} else {
				slbc_debug_log("%s(%d) %s no need to wait slb release!",
						__func__, __LINE__, slbc_uid_str[uid]);
				ret = -ENOT_AVAILABLE;
			}
		} else {
			slbc_debug_log("%s(%d) %s need to wait slb release(invalid timeout:%u ms)!",
					__func__, __LINE__, slbc_uid_str[uid], d->timeout);
			ret = -ENOT_AVAILABLE;
		}
	} else {
		ret = _slbc_request_buffer_scmi(d);
	}
#else /* SLBC_CB */
	ret = _slbc_request_buffer_scmi(d);
#endif /* SLBC_CB */

	if (!ret) {
		slbc_set_sram_data(d);

#ifdef SLBC_CB
		if (SLBC_UID_VALID(uid))
			clear_bit(uid, &slbc_uid_timeout);

		if (SLBC_SID_VALID(sid)) {
			clear_bit(sid, &slbc_sid_req_q);
			slbc_sram_write(SLBC_SID_REQ_Q, slbc_sid_req_q);
			/* slbc_debug_log("%s: %s clr sid %d from slbc_sid_req_q %lx", */
			/* __func__, slbc_uid_str[uid], sid, slbc_sid_req_q); */
		}
#endif /* SLBC_CB */

#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
		buffer_ref = slbc_sram_read(SLBC_BUFFER_REF);
#else
		buffer_ref++;
#endif /* CONFIG_MTK_SLBC_IPI */
	}
	mutex_unlock(&slbc_req_lock);

	return ret;
}

static int slbc_request_acp(void *ptr)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_L3C_PART)
	struct slbc_data *d = ptr;

	/* slbc_debug_log("%s: TP_ACP\n", __func__); */

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

int slbc_status(struct slbc_data *d)
{
	int ret = 0;

	if ((d->type) == TP_BUFFER)
		ret = slbc_request_status(d);

	pr_info("#@# %s(%d) uid 0x%x ret %d\n",
			__func__, __LINE__, d->uid, ret);

	return ret;
}

int slbc_request(struct slbc_data *d)
{
	int ret = 0;
	u64 begin, val;

	begin = ktime_get_ns();

	if ((d->type) == TP_BUFFER) {
		ret = slbc_request_buffer(d);
		d->size = SLBC_WAY_SIZE * popcount(d->slot_used);
		if (!d->paddr)
			ret = -1;
		else
			d->emi_paddr = (void __iomem *)((((unsigned long)d->paddr)
					& SLBC_PADDR_MASK) | SLBC_WAY_B_BASE);
	} else if ((d->type) == TP_CACHE)
		ret = slbc_request_cache(d);
	else if ((d->type) == TP_ACP)
		ret = slbc_request_acp(d);

	pr_info("#@# %s(%d) uid 0x%x ret %d d->ret %d pa 0x%lx emipa 0x%lx size 0x%lx\n",
			__func__, __LINE__, d->uid, ret, d->ret,
			(unsigned long)d->paddr, (unsigned long)d->emi_paddr, d->size);

	if (!ret) {
#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
		d->ref = slbc_read_debug_sram(d->sid);
		slbc_ref = slbc_sram_read(SLBC_REF);
#else
		slbc_ref++;
#endif /* CONFIG_MTK_SLBC_IPI */
	}

	val = (ktime_get_ns() - begin) / 1000000;
	req_val_count++;
	req_val_total += val;
	req_val_max = max(val, req_val_max);
	if (!req_val_min)
		req_val_min = val;
	else
		req_val_min = min(val, req_val_min);

	return ret;
}

static int slbc_release_cache(struct slbc_data *d)
{
	int ret = 0;

	/* slbc_debug_log("%s: TP_CACHE\n", __func__); */

	ret = _slbc_release_cache_scmi(d);

	return ret;
}

static int slbc_release_buffer(struct slbc_data *d)
{
	int ret = 0;
#ifdef SLBC_CB
	int sid;
#endif /* SLBC_CB */

	mutex_lock(&slbc_rel_lock);
	slbc_debug_log("%s: TP_BUFFER", __func__);
#ifdef SLBC_CB
	sid = slbc_get_sid_by_uid(d->uid);
	slbc_uid_used = slbc_sram_read(SLBC_UID_USED);
	if (!test_bit(d->uid, &slbc_uid_used) &&
			SLBC_SID_VALID(sid) &&
			SLBC_UID_VALID(d->uid) &&
			test_bit(sid, &slbc_sid_req_q)) {
		clear_bit(sid, &slbc_sid_req_q);
		slbc_sram_write(SLBC_SID_REQ_Q, slbc_sid_req_q);
		slbc_debug_log("%s: %s clr sid %d from slbc_sid_req_q %lx",
				__func__, slbc_uid_str[d->uid], sid, slbc_sid_req_q);
	}
#endif /* SLBC_CB */

	ret = _slbc_release_buffer_scmi(d);

	if (!ret) {
		slbc_clr_sram_data(d);

#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
		buffer_ref = slbc_sram_read(SLBC_BUFFER_REF);
#else
		buffer_ref--;
#endif /* CONFIG_MTK_SLBC_IPI */
		WARN_ON(buffer_ref < 0);
	}

#ifdef SLBC_CB
#ifdef SLBC_CB_SLEEP
	/* trigger wait queue wakup */
	wake_up_all(&slbc_wq);
#endif /* SLBC_CB_SLEEP */

	if (!ret && slbc_sid_req_q && !slbc_release_check(d)) {
		/* slbc_debug_log("%s: %s trigger act, slbc_sid_req_q %lx", */
				/* __func__, slbc_uid_str[d->uid], slbc_sid_req_q); */

		slbc_activate_check(d);
	}
#endif /* SLBC_CB */
	mutex_unlock(&slbc_rel_lock);

	return ret;
}

static int slbc_release_acp(void *ptr)
{
	int ret = 0;
#if IS_ENABLED(CONFIG_MTK_L3C_PART)
	struct slbc_data *d = ptr;

	/* slbc_debug_log("%s: TP_ACP\n", __func__); */

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
	int ret = 0;
	u64 begin, val;

	begin = ktime_get_ns();

	if ((d->type) == TP_BUFFER) {
		ret = slbc_release_buffer(d);
		d->size = 0;
	} else if ((d->type) == TP_CACHE)
		ret = slbc_release_cache(d);
	else if ((d->type) == TP_ACP)
		ret = slbc_release_acp(d);

	pr_info("#@# %s(%d) uid 0x%x ret %d d->ret %d pa 0x%lx size 0x%lx\n",
			__func__, __LINE__, d->uid, ret, d->ret,
			(unsigned long)d->paddr, d->size);

	if (!ret) {
#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
		d->ref = slbc_read_debug_sram(d->sid);
		slbc_ref = slbc_sram_read(SLBC_REF);
#else
		slbc_ref--;
#endif /* CONFIG_MTK_SLBC_IPI */
	}

	val = (ktime_get_ns() - begin) / 1000000;
	rel_val_count++;
	rel_val_total += val;
	rel_val_max = max(val, rel_val_max);
	if (!rel_val_min)
		rel_val_min = val;
	else
		rel_val_min = min(val, rel_val_min);

	return ret;
}

static void slbc_sync_mb(void *task)
{
	dsb(sy);
	isb();
	/* Pairs with smp_wmb() */
	smp_rmb();
}

static void slbc_mem_barrier(void)
{
	/*
	 * Ensure all data update before kicking the CPUs.
	 * Pairs with smp_rmb() in slbc_sync_mb().
	 */
	smp_wmb();
	dsb(sy);
	isb();

	smp_call_function(slbc_sync_mb, NULL, 1);
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

	return 0;
}

void slbc_update_inner(unsigned int inner)
{
	slbc_inner = inner;
	slbc_inner_cmd(inner);
}

void slbc_update_outer(unsigned int outer)
{
	slbc_outer = outer;
	slbc_outer_cmd(outer);
}

static void slbc_dump_data(struct seq_file *m, struct slbc_data *d)
{
	unsigned int uid = d->uid;

	if (uid >= sizeof(slbc_uid_used)) {
		pr_info("slbc: uid size % >= slbc_uid_used size %d\n",
				uid, sizeof(slbc_uid_used));
		return;
	}

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

#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
	slbc_uid_used = slbc_sram_read(SLBC_UID_USED);
	slbc_sid_mask = slbc_sram_read(SLBC_SID_MASK);
	slbc_sid_req_q = slbc_sram_read(SLBC_SID_REQ_Q);
	slbc_sid_rel_q = slbc_sram_read(SLBC_SID_REL_Q);
	slbc_slot_used = slbc_sram_read(SLBC_SLOT_USED);
	slbc_force = slbc_sram_read(SLBC_FORCE);
	buffer_ref = slbc_sram_read(SLBC_BUFFER_REF);
	slbc_ref = slbc_sram_read(SLBC_REF);
	slbc_sta = slbc_sram_read(SLBC_STA);
	slbc_ack_c = slbc_sram_read(SLBC_ACK_C);
	slbc_ack_g = slbc_sram_read(SLBC_ACK_G);
	cpuqos_mode = slbc_sram_read(CPUQOS_MODE);
	slbc_sram_con = slbc_sram_read(SLBC_SRAM_CON);
	slbc_cache_used = slbc_sram_read(SLBC_CACHE_USED);
	slbc_pmu_0 = slbc_sram_read(SLBC_PMU_0);
	slbc_pmu_1 = slbc_sram_read(SLBC_PMU_1);
	slbc_pmu_2 = slbc_sram_read(SLBC_PMU_2);
	slbc_pmu_3 = slbc_sram_read(SLBC_PMU_3);
	slbc_pmu_4 = slbc_sram_read(SLBC_PMU_4);
	slbc_pmu_5 = slbc_sram_read(SLBC_PMU_5);
	slbc_pmu_6 = slbc_sram_read(SLBC_PMU_6);

	for (i = 0; i < UID_MAX; i++) {
		sid = slbc_get_sid_by_uid(i);
		if (sid != SID_NOT_FOUND)
			uid_ref[i] = slbc_read_debug_sram(sid);
	}
#endif /* CONFIG_MTK_SLBC_IPI */

	seq_printf(m, "slbc_enable %x\n", slbc_enable);
	seq_printf(m, "slb_disable %x\n", slb_disable);
	seq_printf(m, "slc_disable %x\n", slc_disable);
	seq_printf(m, "slbc_sram_enable %x\n", slbc_sram_enable);
	seq_printf(m, "slbc_scmi_enable %x\n", slbc_get_scmi_enable());
	seq_printf(m, "slbc_uid_used 0x%lx\n", slbc_uid_used);
	seq_printf(m, "slbc_uid_timeout 0x%lx\n", slbc_uid_timeout);
	seq_printf(m, "slbc_sid_mask 0x%lx\n", slbc_sid_mask);
	seq_printf(m, "slbc_sid_req_q 0x%lx\n", slbc_sid_req_q);
	seq_printf(m, "slbc_sid_rel_q 0x%lx\n", slbc_sid_rel_q);
	seq_printf(m, "slbc_sid_cb_fail 0x%lx\n", slbc_sid_cb_fail);
	seq_printf(m, "slbc_slot_used 0x%lx\n", slbc_slot_used);
	seq_printf(m, "slbc_force 0x%x\n", slbc_force);
	seq_printf(m, "buffer_ref %x\n", buffer_ref);
	seq_printf(m, "acp_ref %x\n", acp_ref);
	seq_printf(m, "slbc_ref %x\n", slbc_ref);
	seq_printf(m, "debug_level %x\n", debug_level);
	seq_printf(m, "slbc_sta %x\n", slbc_sta);
	seq_printf(m, "slbc_ack_c %x\n", slbc_ack_c);
	seq_printf(m, "slbc_ack_g %x\n", slbc_ack_g);
	seq_printf(m, "cpuqos_mode %x\n", cpuqos_mode);
	seq_printf(m, "slbc_sram_con %x\n", slbc_sram_con);
	seq_printf(m, "slbc_cache_used %x\n", slbc_cache_used);
	seq_printf(m, "slbc_pmu_0 %x\n", slbc_pmu_0);
	seq_printf(m, "slbc_pmu_1 %x\n", slbc_pmu_1);
	seq_printf(m, "slbc_pmu_2 %x\n", slbc_pmu_2);
	seq_printf(m, "slbc_pmu_3 %x\n", slbc_pmu_3);
	seq_printf(m, "slbc_pmu_4 %x\n", slbc_pmu_4);
	seq_printf(m, "slbc_pmu_5 %x\n", slbc_pmu_5);
	seq_printf(m, "slbc_pmu_6 %x\n", slbc_pmu_6);
	seq_printf(m, "mic_num %x\n", slbc_mic_num);
	seq_printf(m, "inner %x\n", slbc_inner);
	seq_printf(m, "outer %x\n", slbc_outer);

	for (i = 0; i < UID_MAX; i++) {
		struct slbc_data d;

		d.uid = i;
		d.type  = TP_BUFFER;
		sid = slbc_get_sid_by_uid(i);
		if (sid != SID_NOT_FOUND)
			seq_printf(m, "uid_ref %s %x, slbc_activate_status %d\n",
					slbc_uid_str[i], uid_ref[i], slbc_activate_status(&d));
	}

	mutex_lock(&slbc_ops_lock);
	list_for_each_entry(ops, &slbc_ops_list, node) {
		struct slbc_data *d = ops->data;

		slbc_dump_data(m, d);
	}
	mutex_unlock(&slbc_ops_lock);

	if (req_val_count) {
		seq_printf(m, "stat req count:%lld min:%lld avg:%lld max:%lld\n",
				req_val_count, req_val_min,
				req_val_total / req_val_count, req_val_max);
		seq_printf(m, "stat rel count:%lld min:%lld avg:%lld max:%lld\n",
				rel_val_count, rel_val_min,
				rel_val_total / rel_val_count, rel_val_max);
	}

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

				if (uid >= sizeof(slbc_uid_used)) {
					pr_info("slbc: uid size % >= slbc_uid_used size %d\n",
							uid, sizeof(slbc_uid_used));
					return -EINVAL;
				}

				if (test_bit(uid, &slbc_uid_used))
					ops->deactivate(d);
			}
			mutex_unlock(&slbc_ops_lock);
		}
	} else if (!strcmp(cmd, "slb_disable")) {
		pr_info("slb disable %ld\n", val_1);
		slb_disable = val_1;
		slbc_sspm_slb_disable((int)!!val_1);
	} else if (!strcmp(cmd, "slc_disable")) {
		pr_info("slc disable %ld\n", val_1);
		slc_disable = val_1;
		slbc_sspm_slc_disable((int)!!val_1);
	} else if (!strcmp(cmd, "slbc_scmi_enable")) {
		pr_info("slbc scmi enable %ld\n", val_1);
		slbc_sspm_enable((int)!!val_1);
	} else if (!strcmp(cmd, "slbc_uid_used")) {
		slbc_uid_used = val_1;
		slbc_sram_write(SLBC_UID_USED, slbc_uid_used);
	} else if (!strcmp(cmd, "slbc_sid_mask")) {
		slbc_sid_mask = val_1;
		slbc_sram_write(SLBC_SID_MASK, slbc_sid_mask);
	} else if (!strcmp(cmd, "slbc_sid_req_q")) {
		slbc_sid_req_q = val_1;
		slbc_sram_write(SLBC_SID_REQ_Q, slbc_sid_req_q);
	} else if (!strcmp(cmd, "slbc_sid_rel_q")) {
		slbc_sid_rel_q = val_1;
		slbc_sram_write(SLBC_SID_REL_Q, slbc_sid_rel_q);
	} else if (!strcmp(cmd, "slbc_slot_used")) {
		slbc_slot_used = val_1;
		slbc_sram_write(SLBC_SLOT_USED, slbc_slot_used);
	} else if (!strcmp(cmd, "test_acp_request")) {
		test_d.uid = UID_TEST_ACP;
		test_d.type  = TP_ACP;
		test_d.flag = val_1;
		ret = slbc_request(&test_d);
	} else if (!strcmp(cmd, "test_acp_release")) {
		test_d.uid = UID_TEST_ACP;
		test_d.type  = TP_ACP;
		test_d.flag = val_1;
		ret = slbc_release(&test_d);
	} else if (!strcmp(cmd, "test_slb_request")) {
		test_d.uid = val_1;
		test_d.type  = TP_BUFFER;
#ifdef SLBC_CB
		test_d.timeout = val_2;
#endif /* SLBC_CB */
		ret = slbc_request(&test_d);
	} else if (!strcmp(cmd, "test_slb_release")) {
		test_d.uid = val_1;
		test_d.type  = TP_BUFFER;
		ret = slbc_release(&test_d);
	} else if (!strcmp(cmd, "slbc_force")) {
		slbc_force = val_1;
		slbc_force_cmd(slbc_force);
	} else if (!strcmp(cmd, "mic_num")) {
		slbc_update_mic_num(val_1);
	} else if (!strcmp(cmd, "inner")) {
		slbc_update_inner(val_1);
	} else if (!strcmp(cmd, "outer")) {
		slbc_update_outer(val_1);
	} else if (!strcmp(cmd, "debug_level")) {
		debug_level = val_1;
	} else if (!strcmp(cmd, "sram")) {
		pr_info("#@# %s(%d) slbc->regs 0x%lx slbc->regsize 0x%x\n",
				__func__, __LINE__,
				(unsigned long)slbc->regs, slbc->regsize);

		print_hex_dump(KERN_INFO, "SLBC: ", DUMP_PREFIX_OFFSET,
				16, 4, slbc->sram_vaddr, slbc->regsize, 1);
	} else {
		pr_info("#@# %s(%d) wrong cmd %s val %ld\n",
				__func__, __LINE__, cmd, val_1);
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
	.slbc_status = slbc_status,
	.slbc_request = slbc_request,
	.slbc_release = slbc_release,
	.slbc_power_on = slbc_power_on,
	.slbc_power_off = slbc_power_off,
	.slbc_secure_on = slbc_secure_on,
	.slbc_secure_off = slbc_secure_off,
	.slbc_register_activate_ops = slbc_register_activate_ops,
	.slbc_activate_status = slbc_activate_status,
	.slbc_sram_read = slbc_sram_read,
	.slbc_sram_write = slbc_sram_write,
};

static struct slbc_ipi_ops ipi_ops = {
	.slbc_request_acp = slbc_request_acp,
	.slbc_release_acp = slbc_release_acp,
	.slbc_mem_barrier = slbc_mem_barrier,
};

static int slbc_probe(struct platform_device *pdev)
{
	struct device_node *node = pdev->dev.of_node;
	struct device *dev = &pdev->dev;
	int ret = 0;
	struct cpuidle_driver *drv = cpuidle_get_driver();
	/* struct resource *res; */
	uint32_t reg[4] = {0, 0, 0, 0};

#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
	ret = slbc_scmi_init();
	if (ret < 0)
		return ret;
#endif /* CONFIG_MTK_SLBC_IPI */

	slbc = devm_kzalloc(dev, sizeof(struct mtk_slbc), GFP_KERNEL);
	if (!slbc)
		return -ENOMEM;

	slbc->dev = dev;

#ifdef ENABLE_SLBC
	if (node) {
		ret = of_property_read_u32(node,
				"slbc-enable", &slbc_enable);
		if (ret)
			pr_info("failed to get slbc_enable from dts\n");
		else
			pr_info("#@# %s(%d) slbc_enable %d\n", __func__, __LINE__,
					slbc_enable);
	} else
		pr_info("find slbc node failed\n");
#else
	slbc_enable = 0;

	pr_info("#@# %s(%d) slbc_enable %d\n", __func__, __LINE__,
			slbc_enable);
#endif /* ENABLE_SLBC */

	ret = of_property_read_u32_array(node, "reg", reg, 4);
	if (ret < 0) {
		slbc_sram_enable = 0;

		pr_info("slbc of_property_read_u32_array ERR : %d\n", ret);
	} else {

		slbc->regs = (void *)(long)reg[1];
		slbc->regsize = reg[3];
		slbc->sram_vaddr = (void __iomem *) devm_memremap(dev,
				(resource_size_t)reg[1], slbc->regsize,
				MEMREMAP_WT);
		if (IS_ERR(slbc->sram_vaddr)) {
			slbc_sram_enable = 0;

			dev_notice(dev, "slbc could not ioremap resource for memory\n");
		} else {
			slbc_sram_enable = 1;

			pr_info("#@# %s(%d) slbc->regs 0x%lx slbc->regsize 0x%x\n",
					__func__, __LINE__,
					(unsigned long)slbc->regs, slbc->regsize);
			slbc_sram_init(slbc);
		}
	}

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

	slbc_register_ipi_ops(&ipi_ops);
	slbc_register_common_ops(&common_ops);

#ifdef SLBC_CB
	timer_setup(&slbc_deactivate_timer, slbc_deactivate_timer_fn,
			TIMER_DEFERRABLE);
#endif /* SLBC_CB */
	if (slbc_enable)
		slbc_sspm_enable(slbc_enable);

#ifdef SLBC_CB_TEST
	user_cb_register();
#endif /* SLBC_CB_TEST */

	return 0;
}

static int slbc_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;

	slbc_unregister_ipi_ops(&ipi_ops);
	slbc_unregister_common_ops(&common_ops);
	devm_kfree(dev, slbc);

	return 0;
}

static int slbc_suspend(struct platform_device *pdev, pm_message_t state)
{
#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
	slbc_suspend_resume_notify(1);
#endif /* CONFIG_MTK_SLBC_IPI */
	return 0;
}

static int slbc_resume(struct platform_device *pdev)
{
#if IS_ENABLED(CONFIG_MTK_SLBC_IPI)
	slbc_suspend_resume_notify(0);
#endif /* CONFIG_MTK_SLBC_IPI */
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

static int __init slbc_module_init(void)
{
	return platform_driver_register(&slbc_pdrv);
}
module_init(slbc_module_init);

static void __exit slbc_module_exit(void)
{
	platform_driver_unregister(&slbc_pdrv);
}
module_exit(slbc_module_exit);

MODULE_SOFTDEP("pre: tinysys-scmi.ko");
MODULE_DESCRIPTION("SLBC Driver mt6886 v0.1");
MODULE_LICENSE("GPL");
