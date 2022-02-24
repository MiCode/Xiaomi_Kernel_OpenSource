// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/list.h>
#include <linux/list_sort.h>
#include <linux/interrupt.h>
#include <linux/mfd/mt6357/registers.h>
#include <linux/mfd/mt6358/registers.h>
#include <linux/mfd/mt6359p/registers.h>
#include <linux/mfd/mt6397/core.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/pm_wakeup.h>
#include <linux/regmap.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/timer.h>
#include <linux/workqueue.h>

#include "pmic_lbat_service.h"

#define USER_NAME_MAXLEN	30
#define USER_SIZE		16
#define THD_VOLT_MAX		5400
#define THD_VOLT_MIN		2650
#define VOLT_FULL		1800
#define LBAT_RES		12

#define DEF_R_RATIO_0		7
#define DEF_R_RATIO_1		2

#define LBAT_SERVICE_DBG 0

enum lbat_thd_type {
	LBAT_HV,
	LBAT_LV,
};

struct lbat_thd_t {
	unsigned int thd_volt;
	struct lbat_user *user;
	struct list_head list;
};

struct lbat_user {
	char name[USER_NAME_MAXLEN];
	struct lbat_thd_t *hv_thd;
	struct lbat_thd_t *lv1_thd;
	struct lbat_thd_t *lv2_thd;
	void (*callback)(unsigned int thd_volt);
	unsigned int deb_cnt;
	struct lbat_thd_t *deb_thd_ptr;
	unsigned int hv_deb_prd;
	unsigned int hv_deb_times;
	unsigned int lv_deb_prd;
	unsigned int lv_deb_times;
	struct timer_list deb_timer;
	struct work_struct deb_work;
	struct list_head thd_list;
};

struct reg_t {
	unsigned int addr;
	unsigned int mask;
	size_t size;
};

struct lbat_regs_t {
	const char *regmap_source;
	const char *r_ratio_node_name;
	struct reg_t en;
	struct reg_t debt_max;
	struct reg_t debt_min;
	struct reg_t det_prd_h;
	struct reg_t det_prd_l;
	struct reg_t det_prd;
	struct reg_t max_en;
	struct reg_t volt_max;
	struct reg_t min_en;
	struct reg_t volt_min;
	struct reg_t adc_out;
	int volt_full;
};

struct lbat_regs_t mt6357_lbat_regs = {
	.regmap_source = "parent_drvdata",
	.en = {0},
	.debt_max = {MT6357_AUXADC_LBAT0, 0xFF, 1},
	.debt_min = {MT6357_AUXADC_LBAT0, 0xFF00, 1},
	.det_prd_l = {MT6357_AUXADC_LBAT1, 0xFFFF, 1},
	.det_prd_h = {MT6357_AUXADC_LBAT2, 0xF, 1},
	.max_en = {MT6357_AUXADC_LBAT3, 0x3000, 1},
	.volt_max = {MT6357_AUXADC_LBAT3, 0xFFF, 1},
	.min_en = {MT6357_AUXADC_LBAT4, 0x3000, 1},
	.volt_min = {MT6357_AUXADC_LBAT4, 0xFFF, 1},
	.adc_out = {MT6357_AUXADC_ADC14, 0xFFF, 1},
	.volt_full = 1800,
};

struct lbat_regs_t mt6358_lbat_regs = {
	.regmap_source = "parent_drvdata",
	.en = {0},
	.debt_max = {MT6358_AUXADC_LBAT0, 0xFF, 1},
	.debt_min = {MT6358_AUXADC_LBAT0, 0xFF00, 1},
	.det_prd_l = {MT6358_AUXADC_LBAT1, 0xFFFF, 1},
	.det_prd_h = {MT6358_AUXADC_LBAT2, 0xF, 1},
	.max_en = {MT6358_AUXADC_LBAT3, 0x3000, 1},
	.volt_max = {MT6358_AUXADC_LBAT3, 0xFFF, 1},
	.min_en = {MT6358_AUXADC_LBAT4, 0x3000, 1},
	.volt_min = {MT6358_AUXADC_LBAT4, 0xFFF, 1},
	.adc_out = {MT6358_AUXADC_ADC13, 0xFFF, 1},
	.volt_full = 1800,
};

struct lbat_regs_t mt6359p_lbat_regs = {
	.regmap_source = "parent_drvdata",
	.en = {MT6359P_AUXADC_LBAT0, 0x1, 1},
	.debt_max = {MT6359P_AUXADC_LBAT1, 0xC, 1},
	.debt_min = {MT6359P_AUXADC_LBAT1, 0x30, 1},
	.det_prd = {MT6359P_AUXADC_LBAT1, 0x3, 1},
	.max_en = {MT6359P_AUXADC_LBAT2, 0x3000, 1},
	.volt_max = {MT6359P_AUXADC_LBAT2, 0xFFF, 1},
	.min_en = {MT6359P_AUXADC_LBAT3, 0x3000, 1},
	.volt_min = {MT6359P_AUXADC_LBAT3, 0xFFF, 1},
	.adc_out = {MT6359P_AUXADC_LBAT7, 0xFFF, 1},
	.volt_full = 1800,
};

#define MT6375_AUXADC_LBAT0		0x4AD
#define MT6375_AUXADC_LBAT1		0x4AE
#define MT6375_AUXADC_LBAT2		0x4AF
#define MT6375_AUXADC_LBAT3		0x4B0
#define MT6375_AUXADC_LBAT5		0x4B2
#define MT6375_AUXADC_LBAT6		0x4B3
#define MT6375_AUXADC_ADC_OUT_LBAT	0x41E

static struct lbat_regs_t mt6375_lbat_regs = {
	.regmap_source = "dev_get_regmap",
	.en = { MT6375_AUXADC_LBAT0, BIT(0), 1 },
	.debt_max = { MT6375_AUXADC_LBAT1, GENMASK(3, 2), 1 },
	.debt_min = { MT6375_AUXADC_LBAT1, GENMASK(5, 4), 1 },
	.det_prd = { MT6375_AUXADC_LBAT1, GENMASK(1, 0), 1 },
	.max_en = { MT6375_AUXADC_LBAT2, GENMASK(1, 0), 1 },
	.volt_max = { MT6375_AUXADC_LBAT3, GENMASK(11, 0), 2 },
	.min_en = { MT6375_AUXADC_LBAT5, GENMASK(1, 0), 1 },
	.volt_min = { MT6375_AUXADC_LBAT6, GENMASK(11, 0), 2 },
	.adc_out = { MT6375_AUXADC_ADC_OUT_LBAT, GENMASK(11, 0), 2 },
	.r_ratio_node_name = "lbat_service",
	.volt_full = 1840,
};

static DEFINE_MUTEX(lbat_mutex);
static struct list_head lbat_hv_list = LIST_HEAD_INIT(lbat_hv_list);
static struct list_head lbat_lv_list = LIST_HEAD_INIT(lbat_lv_list);

/* workqueue for SW de-bounce */
static struct workqueue_struct *lbat_wq;

struct regmap *regmap;
const struct lbat_regs_t *lbat_regs;
static struct lbat_thd_t *cur_hv_ptr;
static struct lbat_thd_t *cur_lv_ptr;
static struct lbat_user *lbat_user_table[USER_SIZE];
static unsigned int user_count;
static unsigned int r_ratio[2];

static int __regmap_update_bits(struct regmap *regmap, const struct reg_t *reg,
				unsigned int val)
{
	if (reg->size == 1)
		return regmap_update_bits(regmap, reg->addr, reg->mask, val);
	/*
	 * here we assume those register addresses are continuous and
	 * there is one and only one function in them.
	 * please take care of the endian if it is necessary.
	 * this is not a good assumption but we do this here for compatiblity.
	 * please abstract the register control if there is a chance to refactor
	 * this file.
	 */
	val &= reg->mask;
	return regmap_bulk_write(regmap, reg->addr, &val, reg->size);
}

static int __regmap_write(struct regmap *regmap, const struct reg_t *reg,
			  unsigned int val)
{
	if (reg->size == 1)
		return regmap_write(regmap, reg->addr, val);
	return regmap_bulk_write(regmap, reg->addr, &val, reg->size);
}

static int __regmap_read(struct regmap *regmap, const struct reg_t *reg,
			 unsigned int *val)
{
	if (reg->size == 1)
		return regmap_read(regmap, reg->addr, val);
	return regmap_bulk_read(regmap, reg->addr, val, reg->size);
}

static unsigned int VOLT_TO_RAW(unsigned int volt)
{
	return (volt << LBAT_RES) / (lbat_regs->volt_full * r_ratio[0] / r_ratio[1]);
}

static void lbat_max_en_setting(bool en)
{
	unsigned int val;

	val = en ? lbat_regs->max_en.mask : 0;
	__regmap_update_bits(regmap, &lbat_regs->max_en, val);
}

static void lbat_min_en_setting(bool en)
{
	unsigned int val;

	val = en ? lbat_regs->min_en.mask : 0;
	__regmap_update_bits(regmap, &lbat_regs->min_en, val);
}

static void lbat_irq_enable(void)
{
	if (cur_hv_ptr != NULL)
		lbat_max_en_setting(true);
	if (cur_lv_ptr != NULL)
		lbat_min_en_setting(true);
	if (lbat_regs->en.addr)
		__regmap_write(regmap, &lbat_regs->en, 1);
}

static void lbat_irq_disable(void)
{
	if (lbat_regs->en.addr)
		__regmap_write(regmap, &lbat_regs->en, 0);
	lbat_max_en_setting(false);
	lbat_min_en_setting(false);
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

static void modify_lbat_list(enum lbat_thd_type type, struct lbat_thd_t *thd)
{
	switch (type) {
	case LBAT_HV:
		list_move(&thd->list, &lbat_hv_list);
		list_sort(NULL, &lbat_hv_list, hv_list_cmp);
		thd = list_first_entry(&lbat_hv_list, struct lbat_thd_t, list);
		if (cur_hv_ptr != thd) {
			cur_hv_ptr = thd;
			__regmap_update_bits(regmap, &lbat_regs->volt_max,
					     VOLT_TO_RAW(cur_hv_ptr->thd_volt));
		}
		break;
	case LBAT_LV:
		list_move(&thd->list, &lbat_lv_list);
		list_sort(NULL, &lbat_lv_list, lv_list_cmp);
		thd = list_first_entry(&lbat_lv_list, struct lbat_thd_t, list);
		if (cur_lv_ptr != thd) {
			cur_lv_ptr = thd;
			__regmap_update_bits(regmap, &lbat_regs->volt_min,
					     VOLT_TO_RAW(cur_lv_ptr->thd_volt));
		}
		break;
	}
}

/*
 * After execute lbat_user's callback, set next thd node to wait event
 */
static void lbat_hv_set_next_thd(struct lbat_user *user, struct lbat_thd_t *thd)
{
	/* restore user->thd_list */
	list_move(&thd->list, &user->thd_list);
	list_sort(NULL, &user->thd_list, lv_list_cmp);
	/* HV is triggered */
	if (!list_is_first(&thd->list, &user->thd_list)) /* Not first */
		modify_lbat_list(LBAT_HV, list_prev_entry(thd, list));
	if (!list_is_last(&thd->list, &user->thd_list)) /* Not last */
		modify_lbat_list(LBAT_LV, list_next_entry(thd, list));
}

static void lbat_lv_set_next_thd(struct lbat_user *user, struct lbat_thd_t *thd)
{
	/* restore user->thd_list */
	list_move(&thd->list, &user->thd_list);
	list_sort(NULL, &user->thd_list, lv_list_cmp);
	/* LV is triggered */
	if (!list_is_first(&thd->list, &user->thd_list)) /* Not first */
		modify_lbat_list(LBAT_HV, list_prev_entry(thd, list));
	if (!list_is_last(&thd->list, &user->thd_list)) /* Not last */
		modify_lbat_list(LBAT_LV, list_next_entry(thd, list));
}

static void lbat_set_next_thd(struct lbat_user *user, struct lbat_thd_t *thd)
{
	if (thd == user->hv_thd) {
		modify_lbat_list(LBAT_LV, user->lv1_thd);
		if (user->lv2_thd && !list_empty(&user->lv2_thd->list))
			list_del_init(&user->lv2_thd->list);
	} else if (thd == user->lv1_thd) {
		modify_lbat_list(LBAT_HV, user->hv_thd);
		if (user->lv2_thd && list_empty(&user->lv2_thd->list))
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
	struct lbat_user *user = container_of(work, struct lbat_user, deb_work);

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

static void lbat_timer_func(struct timer_list *t)
{
	unsigned int deb_prd = 0;
	unsigned int deb_times = 0;
	struct lbat_user *user = from_timer(user, t, deb_timer);

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
		pr_notice("[%s] LBAT debounce threshold not match\n", __func__);
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
	timer_setup(&user->deb_timer, lbat_timer_func, 0);
}

static int lbat_user_update(struct lbat_user *user)
{
	struct lbat_thd_t *thd;
	/*
	 * add lv_thd to lbat_lv_list
	 * and assign first entry of lv_list to cur_lv_ptr
	 */
	if (list_empty(&user->thd_list))
		thd = user->lv1_thd;
	else {
		thd = list_first_entry(&user->thd_list,
				       struct lbat_thd_t, list);
		thd = list_next_entry(thd, list);
	}
	modify_lbat_list(LBAT_LV, thd);
	if (user_count == 0)
		lbat_irq_enable();
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

struct lbat_user *lbat_user_register_ext(const char *name, unsigned int *thd_volt_arr,
					 unsigned int thd_volt_size,
					 void (*callback)(unsigned int thd_volt))
{
	int i, ret;
	struct lbat_user *user;
	struct lbat_thd_t *thd;

	if (!regmap)
		return ERR_PTR(-EPROBE_DEFER);
	mutex_lock(&lbat_mutex);
	user = kzalloc(sizeof(*user), GFP_KERNEL);
	if (user == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	strncpy(user->name, name, USER_NAME_MAXLEN - 1);
	if (thd_volt_arr[0] >= 5400 || thd_volt_arr[thd_volt_size - 1] <= 2000) {
		ret = -EINVAL;
		goto out;
	} else if (callback == NULL) {
		ret = -EINVAL;
		goto out;
	}
	INIT_LIST_HEAD(&user->thd_list);
	thd = lbat_thd_init(thd_volt_arr[0], user);
	for (i = 0; i < thd_volt_size; i++) {
		thd = lbat_thd_init(thd_volt_arr[i], user);
		list_add_tail(&thd->list, &user->thd_list);
	}
	user->callback = callback;
	lbat_user_init_timer(user);
	INIT_WORK(&user->deb_work, lbat_deb_handler);
	pr_info("[%s] name=%s, thd_volt_max=%d, thd_volt_min=%d\n", __func__,
		user->name, thd_volt_arr[0], thd_volt_arr[thd_volt_size - 1]);
	ret = lbat_user_update(user);
out:
	if (ret) {
		pr_notice("[%s] error ret=%d\n", __func__, ret);
		if (ret == -EINVAL)
			kfree(user);
		return ERR_PTR(ret);
	}
	mutex_unlock(&lbat_mutex);
	return user;
}
EXPORT_SYMBOL(lbat_user_register_ext);

struct lbat_user *lbat_user_register(const char *name, unsigned int hv_thd_volt,
				     unsigned int lv1_thd_volt,
				     unsigned int lv2_thd_volt,
				     void (*callback)(unsigned int thd_volt))
{
	int ret = 0;
	struct lbat_user *user;

	if (!regmap)
		return ERR_PTR(-EPROBE_DEFER);
	mutex_lock(&lbat_mutex);
	user = kzalloc(sizeof(*user), GFP_KERNEL);
	if (user == NULL) {
		ret = -ENOMEM;
		goto out;
	}
	strncpy(user->name, name, USER_NAME_MAXLEN - 1);
	if (hv_thd_volt >= THD_VOLT_MAX || lv1_thd_volt <= THD_VOLT_MIN) {
		ret = -EINVAL;
		goto out;
	} else if (hv_thd_volt < lv1_thd_volt || lv1_thd_volt < lv2_thd_volt) {
		ret = -EINVAL;
		goto out;
	} else if (callback == NULL) {
		ret = -EINVAL;
		goto out;
	}
	INIT_LIST_HEAD(&user->thd_list);
	user->hv_thd = lbat_thd_init(hv_thd_volt, user);
	user->lv1_thd = lbat_thd_init(lv1_thd_volt, user);
	user->lv2_thd = lbat_thd_init(lv2_thd_volt, user);
	if (!user->hv_thd || !user->lv1_thd || !user->lv2_thd) {
		ret = -EINVAL;
		goto out;
	}
	user->callback = callback;
	lbat_user_init_timer(user);
	INIT_WORK(&user->deb_work, lbat_deb_handler);
	pr_info("[%s] name=%s, hv=%d, lv1=%d, lv2=%d\n",
		__func__, name, hv_thd_volt, lv1_thd_volt, lv2_thd_volt);
	ret = lbat_user_update(user);
out:
	if (ret) {
		pr_notice("[%s] error ret=%d\n", __func__, ret);
		if (ret == -EINVAL)
			kfree(user);
		return ERR_PTR(ret);
	}
	mutex_unlock(&lbat_mutex);
	return user;
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

unsigned int lbat_read_raw(void)
{
	unsigned int adc_out = 0;

	if (!regmap)
		return 0;
	__regmap_read(regmap, &lbat_regs->adc_out, &adc_out);
	adc_out &= lbat_regs->adc_out.mask;
	return adc_out;
}
EXPORT_SYMBOL(lbat_read_raw);

unsigned int lbat_read_volt(void)
{
	unsigned int raw_data = lbat_read_raw();

	return (raw_data * lbat_regs->volt_full * r_ratio[0] / r_ratio[1]) >> LBAT_RES;
}
EXPORT_SYMBOL(lbat_read_volt);

static irqreturn_t bat_h_int_handler(int irq, void *data)
{
	struct lbat_user *user;

	if (cur_hv_ptr == NULL) {
		lbat_max_en_setting(0);
		return IRQ_NONE;
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
		if (list_empty(&user->thd_list))
			lbat_set_next_thd(user, cur_hv_ptr);
		else
			lbat_hv_set_next_thd(user, cur_hv_ptr);
	}

	/* Since cur_hv_ptr is removed, assign new thd for cur_hv_ptr */
	if (list_empty(&lbat_hv_list)) {
		cur_hv_ptr = NULL;
		goto out;
	}
	cur_hv_ptr = list_first_entry(&lbat_hv_list, struct lbat_thd_t, list);
	__regmap_update_bits(regmap, &lbat_regs->volt_max,
			     VOLT_TO_RAW(cur_hv_ptr->thd_volt));
out:
	lbat_irq_disable();
	udelay(200);
	lbat_irq_enable();
	mutex_unlock(&lbat_mutex);
	return IRQ_HANDLED;
}

static irqreturn_t bat_l_int_handler(int irq, void *data)
{
	struct lbat_user *user;

	if (cur_lv_ptr == NULL) {
		lbat_min_en_setting(0);
		return IRQ_NONE;
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
		if (list_empty(&user->thd_list))
			lbat_set_next_thd(user, cur_lv_ptr);
		else
			lbat_lv_set_next_thd(user, cur_lv_ptr);
	}

	/* Since cur_lv_ptr is removed, assign new thd for cur_lv_ptr */
	if (list_empty(&lbat_lv_list)) {
		cur_lv_ptr = NULL;
		goto out;
	}
	cur_lv_ptr = list_first_entry(&lbat_lv_list, struct lbat_thd_t, list);
	__regmap_update_bits(regmap, &lbat_regs->volt_min,
			     VOLT_TO_RAW(cur_lv_ptr->thd_volt));
out:
	lbat_irq_disable();
	udelay(200);
	lbat_irq_enable();
	mutex_unlock(&lbat_mutex);
	return IRQ_HANDLED;
}

/* LBAT H/L debounce: H: 150 ms, L: no-debounce */
/* LBAT detion period (ms) */
#define DEF_H_DEB		150
#define DEF_L_DEB		0
#define	DEF_DET_PRD		15

static void mt6357_lbat_init_setting(void)
{
	/* Selects debounce as 10 */
	__regmap_update_bits(regmap, &lbat_regs->debt_max, DEF_H_DEB / DEF_DET_PRD);
	/* Selects debounce as 0 */
	__regmap_update_bits(regmap, &lbat_regs->debt_min, DEF_L_DEB / DEF_DET_PRD);
	/* Set LBAT_PRD as 15ms */
	__regmap_update_bits(regmap, &lbat_regs->det_prd_l, DEF_DET_PRD);
	__regmap_update_bits(regmap, &lbat_regs->det_prd_h, (DEF_DET_PRD & 0xF0000) >> 16);
}

/* DEBT_SEL: {0: 1, 1: 2, 2: 4, 3: 8}*/
/* DET_PRD_SEL: {0: 15, 1: 30, 2: 45, 3: 60}*/
#define DEF_DEBT_MAX_SEL	3
#define DEF_DEBT_MIN_SEL	0
#define DEF_DET_PRD_SEL		0

static void mt6359p_lbat_init_setting(void)
{
	unsigned int val;

	/* Selects debounce as 8 */
	val = DEF_DEBT_MAX_SEL << (ffs(lbat_regs->debt_max.mask) - 1);
	__regmap_update_bits(regmap, &lbat_regs->debt_max, val);
	/* Selects debounce as 1 */
	val = DEF_DEBT_MIN_SEL << (ffs(lbat_regs->debt_min.mask) - 1);
	__regmap_update_bits(regmap, &lbat_regs->debt_min, val);
	/* Set LBAT_PRD as 15ms */
	val = DEF_DET_PRD_SEL << (ffs(lbat_regs->det_prd.mask) - 1);
	__regmap_update_bits(regmap, &lbat_regs->det_prd, val);
}

static int pmic_lbat_service_probe(struct platform_device *pdev)
{
	int ret, irq;
	struct device_node *np;
	struct mt6397_chip *chip;
	const char *r_ratio_node_name;

	lbat_regs = of_device_get_match_data(&pdev->dev);
	if (!strcmp(lbat_regs->regmap_source, "parent_drvdata")) {
		chip = dev_get_drvdata(pdev->dev.parent);
		regmap = chip->regmap;

		switch (chip->chip_id) {
		case MT6357_CHIP_ID:
		case MT6358_CHIP_ID:
		case MT6366_CHIP_ID:
			mt6357_lbat_init_setting();
			break;
		default:
			mt6359p_lbat_init_setting();
			break;
		}
	} else {
		regmap = dev_get_regmap(pdev->dev.parent, NULL);
		mt6359p_lbat_init_setting();
	}

	irq = platform_get_irq_byname(pdev, "bat_h");
	if (irq < 0) {
		dev_notice(&pdev->dev, "failed to get bat_h irq, ret=%d\n",
			   irq);
		return irq;
	}
	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
					bat_h_int_handler, IRQF_ONESHOT,
					"bat_h", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request bat_h irq fail\n");
	irq = platform_get_irq_byname(pdev, "bat_l");
	if (irq < 0) {
		dev_notice(&pdev->dev, "failed to get bat_l irq, ret=%d\n",
			   irq);
		return irq;
	}
	ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
					bat_l_int_handler, IRQF_ONESHOT,
					"bat_l", NULL);
	if (ret < 0)
		dev_notice(&pdev->dev, "request bat_l irq fail\n");

	lbat_wq = create_singlethread_workqueue("lbat_service");

	/* get LBAT r_ratio */
	r_ratio_node_name = lbat_regs->r_ratio_node_name ? lbat_regs->r_ratio_node_name : "batadc";
	np = of_find_node_by_name(pdev->dev.parent->of_node, r_ratio_node_name);
	if (!np) {
		dev_notice(&pdev->dev, "get %s node fail\n", r_ratio_node_name);
		r_ratio[0] = DEF_R_RATIO_0;
		r_ratio[1] = DEF_R_RATIO_1;
		return 0;
	}
	ret = of_property_read_u32_array(np, "resistance-ratio", r_ratio, 2);
	dev_info(&pdev->dev, "r_ratio = %d/%d\n", r_ratio[0], r_ratio[1]);

	return ret;
}

static int __maybe_unused lbat_service_suspend(struct device *d)
{
	lbat_irq_disable();
	return 0;
}

static int __maybe_unused lbat_service_resume(struct device *d)
{
	lbat_irq_enable();
	return 0;
}

static SIMPLE_DEV_PM_OPS(lbat_service_pm_ops, lbat_service_suspend,
			 lbat_service_resume);

static const struct of_device_id lbat_service_of_match[] = {
	{
		.compatible = "mediatek,mt6357-lbat_service",
		.data = &mt6357_lbat_regs,
	}, {
		.compatible = "mediatek,mt6358-lbat_service",
		.data = &mt6358_lbat_regs,
	}, {
		.compatible = "mediatek,mt6359p-lbat_service",
		.data = &mt6359p_lbat_regs,
	}, {
		.compatible = "mediatek,mt6375-lbat-service",
		.data = &mt6375_lbat_regs,
	}, {
		/* sentinel */
	}
};
MODULE_DEVICE_TABLE(of, lbat_service_of_match);

static struct platform_driver pmic_lbat_service_driver = {
	.driver = {
		.name = "pmic_lbat_service",
		.of_match_table = lbat_service_of_match,
		.pm = &lbat_service_pm_ops,
	},
	.probe	= pmic_lbat_service_probe,
};
module_platform_driver(pmic_lbat_service_driver);

MODULE_AUTHOR("Jeter Chen <Jeter.Chen@mediatek.com>");
MODULE_DESCRIPTION("MTK lbat driver");
MODULE_LICENSE("GPL");
