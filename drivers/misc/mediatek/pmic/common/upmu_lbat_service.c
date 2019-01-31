/*
 * Copyright (C) 2016 MediaTek Inc.

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/dcache.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/pm_wakeup.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/workqueue.h>

#include <mt-plat/upmu_common.h>
#include "pmic_lbat_service.h"

#define VOLT_TO_RAW(volt)	(((volt) << 12) / 5400)
#define RAW_TO_VOLT(thd)	(((thd) * 5400) >> 12)
#define USER_SIZE	16

#define DEF_H_DEB	150	/* 150ms */
#define DEF_L_DEB	0	/* no de-bounce */
#define LBAT_PRD	15	/* 15ms */

#define LBAT_SERVICE_DBG 0

static DEFINE_MUTEX(lbat_mutex);
static struct list_head lbat_hv_list = LIST_HEAD_INIT(lbat_hv_list);
static struct list_head lbat_lv_list = LIST_HEAD_INIT(lbat_lv_list);

/* workqueue for SW de-bounce */
static struct workqueue_struct *lbat_wq;

static unsigned int user_count;

enum lbat_thd_type {
	LBAT_HV,
	LBAT_LV,
};

struct lbat_thd_t {
	unsigned int thd_volt;
	struct lbat_user *user;
	struct list_head list;
};

static struct lbat_thd_t *cur_hv_ptr;
static struct lbat_thd_t *cur_lv_ptr;
static struct lbat_user *lbat_user_table[USER_SIZE];

static void lbat_max_en_setting(int en_val)
{
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MAX, en_val);
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX, en_val);
	pmic_enable_interrupt(INT_BAT_H, en_val, "lbat_service");
}

static void lbat_min_en_setting(int en_val)
{
	pmic_set_register_value(PMIC_AUXADC_LBAT_EN_MIN, en_val);
	pmic_set_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN, en_val);
	pmic_enable_interrupt(INT_BAT_L, en_val, "lbat_service");
}

static void lbat_irq_enable(void)
{
	if (cur_hv_ptr != NULL)
		lbat_max_en_setting(1);
	if (cur_lv_ptr != NULL)
		lbat_min_en_setting(1);
}

static void lbat_irq_disable(void)
{
	lbat_max_en_setting(0);
	lbat_min_en_setting(0);
}

static int hv_list_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct lbat_thd_t *thd_a, *thd_b;

	thd_a = list_entry(a, struct lbat_thd_t, list);
	thd_b = list_entry(b, struct lbat_thd_t, list);

	return thd_a->thd_volt - thd_b->thd_volt;
}

static int lv_list_cmp(void *priv, struct list_head *a, struct list_head *b)
{
	struct lbat_thd_t *thd_a, *thd_b;

	thd_a = list_entry(a, struct lbat_thd_t, list);
	thd_b = list_entry(b, struct lbat_thd_t, list);

	return thd_b->thd_volt - thd_a->thd_volt;
}

static void modify_lbat_list(enum lbat_thd_type type,
	struct lbat_thd_t *thd)
{
	switch (type) {
	case LBAT_HV:
		list_add(&thd->list, &lbat_hv_list);
		list_sort(NULL, &lbat_hv_list, hv_list_cmp);
		thd = list_first_entry(&lbat_hv_list,
				struct lbat_thd_t, list);
		if (cur_hv_ptr != thd) {
			cur_hv_ptr = thd;
			pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MAX,
				VOLT_TO_RAW(cur_hv_ptr->thd_volt));
		}
		break;
	case LBAT_LV:
		list_add(&thd->list, &lbat_lv_list);
		list_sort(NULL, &lbat_lv_list, lv_list_cmp);
		thd = list_first_entry(&lbat_lv_list,
				struct lbat_thd_t, list);
		if (cur_lv_ptr != thd) {
			cur_lv_ptr = thd;
			pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN,
				VOLT_TO_RAW(cur_lv_ptr->thd_volt));
		}
		break;
	}
}

/*
 * After execute lbat_user's callback, set next thd node to wait event
 */
static void lbat_set_next_thd(struct lbat_user *user, struct lbat_thd_t *thd)
{
	if (thd == user->hv_thd) {
		modify_lbat_list(LBAT_LV, user->lv1_thd);
		if (!list_empty(&user->lv2_thd->list))
			list_del_init(&user->lv2_thd->list);
	} else if (thd == user->lv1_thd) {
		modify_lbat_list(LBAT_HV, user->hv_thd);
		if (list_empty(&user->lv2_thd->list))
			modify_lbat_list(LBAT_LV, user->lv2_thd);
	}
}

/*
 * Execute user's callback and set its next threshold if reach deb_times,
 * otherwise ignore this event and reset lbat_list
 */
static void lbat_deb_handler(struct work_struct *work)
{
	enum lbat_thd_type type;
	unsigned int deb_times;
	struct lbat_user *user =
		container_of(work, struct lbat_user, deb_work);

	mutex_lock(&lbat_mutex);
	if (user->deb_thd_ptr == user->hv_thd) {
		type = LBAT_HV;
		deb_times = user->hv_deb_times;
	} else {
		type = LBAT_LV;
		deb_times = user->lv_deb_times;
	}
	if (user->deb_cnt >= deb_times) {
		/* execute user's callback after de-bounce */
		user->callback(user->deb_thd_ptr->thd_volt);
		lbat_set_next_thd(user, user->deb_thd_ptr);
	} else {
		/* ignore this event and reset lbat_list */
		modify_lbat_list(type, user->deb_thd_ptr);
	}
	/* de-bounce done, reset deb_cnt and deb_thd_ptr */
	user->deb_cnt = 0;
	user->deb_thd_ptr = NULL;
	lbat_irq_disable();
	udelay(200);
	lbat_irq_enable();
	mutex_unlock(&lbat_mutex);
}

static void lbat_timer_func(unsigned long data)
{
	unsigned int deb_prd = 0;
	unsigned int deb_times = 0;
	struct lbat_user *user = (struct lbat_user *)data;

	if (user->deb_thd_ptr == user->hv_thd) {
		/* LBAT user HV de-bounce */
		if (lbat_read_volt() < user->deb_thd_ptr->thd_volt) {
			/* queue deb_work to reset lbat_list */
			goto wq_handler;
		}
		deb_prd = user->hv_deb_prd;
		deb_times = user->hv_deb_times;
	} else if (user->deb_thd_ptr == user->lv1_thd ||
		   user->deb_thd_ptr == user->lv2_thd) {
		/* LBAT user LV de-bounce */
		if (lbat_read_volt() > user->deb_thd_ptr->thd_volt) {
			/* queue deb_work to reset lbat_list */
			goto wq_handler;
		}
		deb_prd = user->lv_deb_prd;
		deb_times = user->lv_deb_times;
	} else {
		pr_notice("[%s] LBAT de-bounce threshold not match\n",
			__func__);
		return;
	}
	user->deb_cnt++;
#if LBAT_SERVICE_DBG
	pr_info("[%s] name:%s, thd_volt:%d, de-bounce times:%d\n",
		__func__, user->name,
		user->deb_thd_ptr->thd_volt, user->deb_cnt);
#endif
	if (user->deb_cnt < deb_times) {
		mod_timer(&user->deb_timer,
			jiffies + msecs_to_jiffies(deb_prd));
		return;
	}
wq_handler:
	/* queue deb_work to execute user's callback or reset lbat_list */
	queue_work(lbat_wq, &user->deb_work);
}

static void lbat_user_init_timer(struct lbat_user *user)
{
	user->deb_cnt = 0;
	user->hv_deb_prd = 0;
	user->hv_deb_times = 0;
	user->lv_deb_prd = 0;
	user->lv_deb_times = 0;
	init_timer(&user->deb_timer);
	user->deb_timer.data = (unsigned long)user;
	user->deb_timer.expires = 0;
	user->deb_timer.function = lbat_timer_func;
}

static int lbat_user_update(struct lbat_user *user)
{
	/*
	 * add lv_thd to lbat_lv_list
	 * and assign first entry of lv_list to cur_lv_ptr
	 */
	modify_lbat_list(LBAT_LV, user->lv1_thd);
	if (user_count == 0)
		lbat_min_en_setting(1);
	lbat_user_table[user_count++] = user;

	return 0;
}

static struct lbat_thd_t *lbat_thd_init(unsigned int thd_volt,
	struct lbat_user *user)
{
	struct lbat_thd_t *thd;

	if (thd_volt == 0)
		return NULL;
	thd = kzalloc(sizeof(*thd), GFP_KERNEL);
	if (thd == NULL)
		return NULL;
	thd->thd_volt = thd_volt;
	thd->user = user;
	INIT_LIST_HEAD(&thd->list);
	return thd;
}

int lbat_user_register(struct lbat_user *user, const char *name,
	unsigned int hv_thd_volt,
	unsigned int lv1_thd_volt, unsigned int lv2_thd_volt,
	void (*callback)(unsigned int))
{
	int ret = 0;

	mutex_lock(&lbat_mutex);
	if (IS_ERR(user)) {
		ret = PTR_ERR(user);
		goto out;
	}
	strncpy(user->name, name, strlen(name));
	if (hv_thd_volt >= 5400 || lv1_thd_volt <= 2650) {
		ret = -11;
		goto out;
	} else if (hv_thd_volt < lv1_thd_volt ||
			   lv1_thd_volt < lv2_thd_volt) {
		ret = -12;
		goto out;
	} else if (callback == NULL) {
		ret = -13;
		goto out;
	}
	user->hv_thd = lbat_thd_init(hv_thd_volt, user);
	user->lv1_thd = lbat_thd_init(lv1_thd_volt, user);
	user->lv2_thd = lbat_thd_init(lv2_thd_volt, user);
	user->callback = callback;
	lbat_user_init_timer(user);
	INIT_WORK(&user->deb_work, lbat_deb_handler);
	pr_info("[%s] hv=%d, lv1=%d, lv2=%d\n",
		__func__, hv_thd_volt, lv1_thd_volt, lv2_thd_volt);
	ret = lbat_user_update(user);
	if (ret)
		pr_notice("[%s] error ret=%d\n", __func__, ret);
out:
	mutex_unlock(&lbat_mutex);
	return ret;
}
EXPORT_SYMBOL(lbat_user_register);

int lbat_user_set_debounce(struct lbat_user *user,
	unsigned int hv_deb_prd, unsigned int hv_deb_times,
	unsigned int lv_deb_prd, unsigned int lv_deb_times)
{
	if (IS_ERR(user))
		return PTR_ERR(user);
	user->hv_deb_prd = hv_deb_prd;
	user->hv_deb_times = hv_deb_times;
	user->lv_deb_prd = lv_deb_prd;
	user->lv_deb_times = lv_deb_times;
	return 0;
}
EXPORT_SYMBOL(lbat_user_set_debounce);

static void bat_h_int_handler(void)
{
	struct lbat_user *user;

	if (cur_hv_ptr == NULL) {
		lbat_max_en_setting(0);
		return;
	}
	mutex_lock(&lbat_mutex);
	pr_info("[%s] cur_thd_volt=%d\n", __func__, cur_hv_ptr->thd_volt);

	user = cur_hv_ptr->user;
	list_del_init(&cur_hv_ptr->list);
	if (user->hv_deb_times) {
		user->deb_cnt = 0;
		user->deb_thd_ptr = cur_hv_ptr;
		mod_timer(&user->deb_timer,
			jiffies + msecs_to_jiffies(user->hv_deb_prd));
	} else {
		user->callback(cur_hv_ptr->thd_volt);
		lbat_set_next_thd(user, cur_hv_ptr);
	}

	/* Since cur_hv_ptr is removed, assign new thd for cur_hv_ptr */
	if (list_empty(&lbat_hv_list)) {
		cur_hv_ptr = NULL;
		goto out;
	}
	cur_hv_ptr = list_first_entry(
			&lbat_hv_list, struct lbat_thd_t, list);
	pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MAX,
		VOLT_TO_RAW(cur_hv_ptr->thd_volt));
out:
	lbat_irq_disable();
	udelay(200);
	lbat_irq_enable();
	mutex_unlock(&lbat_mutex);
}

static void bat_l_int_handler(void)
{
	struct lbat_user *user;

	if (cur_lv_ptr == NULL) {
		lbat_min_en_setting(0);
		return;
	}
	mutex_lock(&lbat_mutex);
	pr_info("[%s] cur_thd_volt=%d\n", __func__, cur_lv_ptr->thd_volt);

	user = cur_lv_ptr->user;
	list_del_init(&cur_lv_ptr->list);
	if (user->lv_deb_times) {
		user->deb_cnt = 0;
		user->deb_thd_ptr = cur_lv_ptr;
		mod_timer(&user->deb_timer,
			jiffies + msecs_to_jiffies(user->lv_deb_prd));
	} else {
		user->callback(cur_lv_ptr->thd_volt);
		lbat_set_next_thd(user, cur_lv_ptr);
	}

	/* Since cur_lv_ptr is removed, assign new thd for cur_lv_ptr */
	if (list_empty(&lbat_lv_list)) {
		cur_lv_ptr = NULL;
		goto out;
	}
	cur_lv_ptr = list_first_entry(
			&lbat_lv_list, struct lbat_thd_t, list);
	pmic_set_register_value(PMIC_AUXADC_LBAT_VOLT_MIN,
		VOLT_TO_RAW(cur_lv_ptr->thd_volt));
out:
	lbat_irq_disable();
	udelay(200);
	lbat_irq_enable();
	mutex_unlock(&lbat_mutex);
}

void lbat_suspend(void)
{
	lbat_irq_disable();
}

void lbat_resume(void)
{
	lbat_irq_enable();
}

int lbat_service_init(void)
{
	int ret = 0;

	pr_info("[%s]", __func__);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MAX,
		DEF_H_DEB / LBAT_PRD);
	pmic_set_register_value(PMIC_AUXADC_LBAT_DEBT_MIN,
		DEF_L_DEB / LBAT_PRD);
	pmic_set_register_value(
		PMIC_AUXADC_LBAT_DET_PRD_15_0,
		LBAT_PRD);
	pmic_set_register_value(
		PMIC_AUXADC_LBAT_DET_PRD_19_16,
		(LBAT_PRD & 0xF0000) >> 16);

	pmic_register_interrupt_callback(INT_BAT_L, bat_l_int_handler);
	pmic_register_interrupt_callback(INT_BAT_H, bat_h_int_handler);

	lbat_wq = create_singlethread_workqueue("lbat_service");

	return ret;
}

unsigned int lbat_read_raw(void)
{
	return pmic_get_register_value(PMIC_AUXADC_ADC_OUT_LBAT);
}

unsigned int lbat_read_volt(void)
{
	unsigned int raw_data = lbat_read_raw();

	return RAW_TO_VOLT(raw_data);
}

/*
 * Lbat service debug
 */
void lbat_dump_reg(void)
{
	pr_notice("AUXADC_LBAT_VOLT_MAX = 0x%x, AUXADC_LBAT_VOLT_MIN = 0x%x, RG_INT_EN_BAT_H = %d, RG_INT_EN_BAT_L = %d\n"
		, pmic_get_register_value(PMIC_AUXADC_LBAT_VOLT_MAX)
		, pmic_get_register_value(PMIC_AUXADC_LBAT_VOLT_MIN)
		, pmic_get_register_value(PMIC_RG_INT_EN_BAT_H)
		, pmic_get_register_value(PMIC_RG_INT_EN_BAT_L));
	pr_notice("AUXADC_LBAT_EN_MAX = %d, AUXADC_LBAT_IRQ_EN_MAX = %d, AUXADC_LBAT_EN_MIN = %d, AUXADC_LBAT_IRQ_EN_MIN = %d\n"
		, pmic_get_register_value(PMIC_AUXADC_LBAT_EN_MAX)
		, pmic_get_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX)
		, pmic_get_register_value(PMIC_AUXADC_LBAT_EN_MIN)
		, pmic_get_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN));
	pr_notice("AUXADC_LBAT_DEBT_MAX=%d, AUXADC_LBAT_DEBT_MIN=%d\n"
		, pmic_get_register_value(PMIC_AUXADC_LBAT_DEBT_MAX)
		, pmic_get_register_value(PMIC_AUXADC_LBAT_DEBT_MIN));
}

static void lbat_dump_thd_list(struct seq_file *s)
{
	unsigned int len = 0;
	char str[128] = "";
	struct lbat_thd_t *thd = NULL;

	if (list_empty(&lbat_hv_list) && list_empty(&lbat_lv_list)) {
		pr_notice("[%s] no entry in lbat list\n", __func__);
		seq_puts(s, "no entry in lbat list\n");
		return;
	}
	mutex_lock(&lbat_mutex);
	list_for_each_entry(thd, &lbat_hv_list, list) {
		len += snprintf(str + len, sizeof(str) - len,
			"%shv_list, thd_volt:%d, user:%s\n",
			(thd == cur_hv_ptr ||
			 thd == cur_lv_ptr) ? "->" : "  ",
			thd->thd_volt, thd->user->name);
		pr_notice("%s", str);
		seq_printf(s, "%s", str);
		strncpy(str, "", strlen(str));
		len = 0;
	}
	pr_notice("\n");
	seq_puts(s, "\n");

	list_for_each_entry(thd, &lbat_lv_list, list) {
		len += snprintf(str + len, sizeof(str) - len,
			"%slv_list, thd_volt:%d, user:%s\n",
			(thd == cur_hv_ptr ||
			 thd == cur_lv_ptr) ? "->" : "  ",
			thd->thd_volt, thd->user->name);
		pr_notice("%s", str);
		seq_printf(s, "%s", str);
		strncpy(str, "", strlen(str));
		len = 0;
	}
	pr_notice("\n");
	mutex_unlock(&lbat_mutex);
}

static void lbat_dbg_dump_reg(struct seq_file *s)
{
	lbat_dump_reg();
	seq_printf(s, "AUXADC_LBAT_VOLT_MAX = 0x%x\n",
		pmic_get_register_value(PMIC_AUXADC_LBAT_VOLT_MAX));
	seq_printf(s, "AUXADC_LBAT_VOLT_MIN = 0x%x\n",
		pmic_get_register_value(PMIC_AUXADC_LBAT_VOLT_MIN));
	seq_printf(s, "RG_INT_EN_BAT_H = 0x%x\n",
		pmic_get_register_value(PMIC_RG_INT_EN_BAT_H));
	seq_printf(s, "RG_INT_EN_BAT_L = 0x%x\n",
		pmic_get_register_value(PMIC_RG_INT_EN_BAT_L));
	seq_printf(s, "AUXADC_LBAT_EN_MAX = 0x%x\n",
		pmic_get_register_value(PMIC_AUXADC_LBAT_EN_MAX));
	seq_printf(s, "AUXADC_LBAT_IRQ_EN_MAX = 0x%x\n",
		pmic_get_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MAX));
	seq_printf(s, "AUXADC_LBAT_EN_MIN = 0x%x\n",
		pmic_get_register_value(PMIC_AUXADC_LBAT_EN_MIN));
	seq_printf(s, "AUXADC_LBAT_IRQ_EN_MIN = 0x%x\n",
		pmic_get_register_value(PMIC_AUXADC_LBAT_IRQ_EN_MIN));
	seq_printf(s, "AUXADC_LBAT_DEBT_MAX = 0x%x\n",
		pmic_get_register_value(PMIC_AUXADC_LBAT_DEBT_MAX));
	seq_printf(s, "AUXADC_LBAT_DEBT_MIN = 0x%x\n",
		pmic_get_register_value(PMIC_AUXADC_LBAT_DEBT_MIN));
	seq_printf(s, "AUXADC_ADC_RDY_LBAT = 0x%x\n",
		pmic_get_register_value(PMIC_AUXADC_ADC_RDY_LBAT));
	seq_printf(s, "AUXADC_ADC_OUT_LBAT = 0x%x\n",
		pmic_get_register_value(PMIC_AUXADC_ADC_OUT_LBAT));
}

static void lbat_dump_user_table(struct seq_file *s)
{
	unsigned int i;
	struct lbat_user *user;

	mutex_lock(&lbat_mutex);
	for (i = 0; i < user_count; i++) {
		user = lbat_user_table[i];
		seq_printf(s, "%2d:%20s, %d, %d, %d, (%d,%d,%d,%d), %pf\n",
			i, user->name,
			user->hv_thd->thd_volt,
			user->lv1_thd->thd_volt, user->lv2_thd->thd_volt,
			user->hv_deb_prd, user->hv_deb_times,
			user->lv_deb_prd, user->lv_deb_times,
			user->callback);
	}
	mutex_unlock(&lbat_mutex);
}

struct lbat_dbg_st {
	unsigned int dbg_id;
};

enum {
	LBAT_DBG_DUMP_LIST,
	LBAT_DBG_DUMP_REG,
	LBAT_DBG_DUMP_TABLE,
	LBAT_DBG_MAX,
};

static struct lbat_dbg_st dbg_data[LBAT_DBG_MAX];

static int lbat_dbg_show(struct seq_file *s, void *unused)
{
	struct lbat_dbg_st *dbg_st = s->private;

	switch (dbg_st->dbg_id) {
	case LBAT_DBG_DUMP_LIST:
		lbat_dump_thd_list(s);
		break;
	case LBAT_DBG_DUMP_REG:
		lbat_dbg_dump_reg(s);
		break;
	case LBAT_DBG_DUMP_TABLE:
		lbat_dump_user_table(s);
		break;
	default:
		break;
	}
	return 0;
}

static int lbat_dbg_open(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		return single_open(file, lbat_dbg_show, inode->i_private);
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t lbat_dbg_write(struct file *file,
	const char __user *user_buffer, size_t count, loff_t *position)
{
	return count;
}

static int lbat_dbg_release(struct inode *inode, struct file *file)
{
	if (file->f_mode & FMODE_READ)
		return single_release(inode, file);
	return 0;
}

static const struct file_operations lbat_dbg_fops = {
	.open = lbat_dbg_open,
	.read = seq_read,
	.write = lbat_dbg_write,
	.llseek  = seq_lseek,
	.release = lbat_dbg_release,
};

int lbat_debug_init(struct dentry *debug_dir)
{
	struct dentry *lbat_dbg_dir;

	if (IS_ERR(debug_dir) || !debug_dir) {
		pr_notice("dir mtk_pmic does not exist\n");
		return -1;
	}
	lbat_dbg_dir = debugfs_create_dir("lbat_dbg", debug_dir);
	if (IS_ERR(lbat_dbg_dir) || !lbat_dbg_dir) {
		pr_notice("fail mkdir /sys/kernel/debug/mtk_pmic/lbat_dbg\n");
		return -1;
	}
	/* lbat service debug init */
	dbg_data[0].dbg_id = LBAT_DBG_DUMP_LIST;
	/* file type is regular file(S_IFREG), permission is read(444) */
	debugfs_create_file("lbat_dump_list", (S_IFREG | 0444),
		lbat_dbg_dir, (void *)&dbg_data[0], &lbat_dbg_fops);

	dbg_data[1].dbg_id = LBAT_DBG_DUMP_REG;
	debugfs_create_file("lbat_dump_reg", (S_IFREG | 0444),
		lbat_dbg_dir, (void *)&dbg_data[1], &lbat_dbg_fops);

	dbg_data[2].dbg_id = LBAT_DBG_DUMP_TABLE;
	debugfs_create_file("lbat_dump_table", (S_IFREG | 0444),
		lbat_dbg_dir, (void *)&dbg_data[2], &lbat_dbg_fops);

	return 0;
}
