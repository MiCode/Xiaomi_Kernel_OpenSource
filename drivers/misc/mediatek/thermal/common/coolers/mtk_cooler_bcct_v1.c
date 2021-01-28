// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/uaccess.h>
#include <linux/err.h>
#include <linux/syscalls.h>
#include <linux/platform_device.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include <linux/uidgid.h>
#include <linux/notifier.h>
#include <linux/fb.h>
#include "mach/mtk_thermal.h"
#include <linux/power_supply.h>


static int cl_bcct_klog_on;

/* ************************************ */

#define mtk_cooler_bcct_dprintk_always(fmt, args...) \
	pr_notice("[Thermal/TC/bcct]" fmt, ##args)

#define mtk_cooler_bcct_dprintk(fmt, args...) \
	do { \
		if (cl_bcct_klog_on == 1) \
			pr_notice("[Thermal/TC/bcct]" fmt, ##args); \
	}  while (0)

#define MAX_NUM_INSTANCE_MTK_COOLER_BCCT  3

#define MTK_CL_BCCT_GET_LIMIT(limit, state) \
{(limit) = (short) (((unsigned long) (state))>>16); }

#define MTK_CL_BCCT_SET_LIMIT(limit, state) \
{(state) = ((((unsigned long) (state))&0xFFFF) | ((short) limit<<16)); }

#define MTK_CL_BCCT_GET_CURR_STATE(curr_state, state) \
{(curr_state) = (((unsigned long) (state))&0xFFFF); }

#define MTK_CL_BCCT_SET_CURR_STATE(curr_state, state) \
	do { \
		if (0 == (curr_state)) \
			state &= ~0x1; \
		else \
			state |= 0x1; \
	} while (0)

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

#define MIN(_a_, _b_) ((_a_) > (_b_) ? (_b_) : (_a_))
#define MAX(_a_, _b_) ((_a_) > (_b_) ? (_a_) : (_b_))

/* Battery & Charger Status*/
static int bat_info_uisoc; /* battery UI soc */
static int bat_info_vbat; /* battery voltage */
static int bat_info_ibat; /* charging current */
static int bat_info_mintchr; /* charger min temp */
static int bat_info_maxtchr; /* charger max temp */
static int bat_info_vbus; /* Vbus */
static int bat_info_aicr; /* input current */
static int bat_info_charging_type; /* type 0: none or normal,
				    * 1: pep1.0 2: pep2.0 3: pep3.0
				    */
/* Charger Limiter
 * Charger Limiter provides API to limit charger IC input current and
 * battery charging current. It arbitrates the limitation from users and sets
 * limitation to charger driver via two API functions:
 *	set_chr_input_current_limit()
 *	set_bat_charging_current_limit()
 */
static int chrlmt_chr_input_curr_limit = -1; /**< -1 is unlimit, unit is mA. */
static int chrlmt_bat_chr_curr_limit = -1; /**< -1 is unlimit, unit is mA. */
static bool chrlmt_is_lcmoff; /**0 is lcm on, 1 is lcm off */
static int chrlmt_lcmoff_policy_enable; /**0: No lcmoff abcct */

struct chrlmt_handle {
	int chr_input_curr_limit;
	int bat_chr_curr_limit;
};

static struct workqueue_struct *bcct_chrlmt_queue;
static struct work_struct      bcct_chrlmt_work;

static int get_battery_current(void)
{
	union power_supply_propval prop;
	struct power_supply *psy;
	int ret = 0;

	psy = power_supply_get_by_name("battery");
	if (psy == NULL)
		return -1;
	ret = power_supply_get_property(psy,
		POWER_SUPPLY_PROP_CURRENT_NOW, &prop);
	mtk_cooler_bcct_dprintk("%s %d\n",
		__func__, prop.intval);
	if (ret != 0)
		return -1;

	return prop.intval;

}

/* temp solution, use list instead */
#define CHR_LMT_MAX_USER_COUNT	(4)

static struct chrlmt_handle
		*chrlmt_registered_users[CHR_LMT_MAX_USER_COUNT] = { 0 };

static int chrlmt_register(struct chrlmt_handle *handle)
{
	int i;

	if (!handle)
		return -1;

	handle->chr_input_curr_limit = -1;
	handle->bat_chr_curr_limit = -1;
	/* find an empty entry */
	for (i = CHR_LMT_MAX_USER_COUNT; --i >= 0; )
		if (!chrlmt_registered_users[i]) {
			chrlmt_registered_users[i] = handle;
			return 0;
		}

	return -1;
}

static int chrlmt_unregister(struct chrlmt_handle *handle)
{
	return -1;
}

int clbcct_get_chr_curr_limit(void)
{
	return chrlmt_bat_chr_curr_limit;
}

int clbcct_get_input_curr_limit(void)
{
	return chrlmt_chr_input_curr_limit;
}

static void chrlmt_set_limit_handler(struct work_struct *work)
{
	union power_supply_propval prop;
	static struct power_supply *chg_psy;
	int ret;

	mtk_cooler_bcct_dprintk("%s %d %d\n", __func__,
					chrlmt_chr_input_curr_limit,
					chrlmt_bat_chr_curr_limit);

	if (chg_psy == NULL)
		chg_psy = power_supply_get_by_name("mtk-master-charger");
	if (chg_psy == NULL || IS_ERR(chg_psy)) {
		mtk_cooler_bcct_dprintk_always("%s Couldn't get chg_psy\n",
			__func__);
		return;
	}
	if (chrlmt_bat_chr_curr_limit != -1)
		prop.intval = chrlmt_bat_chr_curr_limit * 1000;
	else
		prop.intval = -1;
	ret = power_supply_set_property(chg_psy,
		POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT_MAX,
		&prop);
	if (ret != 0)
		pr_notice("%s bat curr fail\n", __func__);

	if (chrlmt_chr_input_curr_limit != -1)
		prop.intval = chrlmt_chr_input_curr_limit * 1000;
	else
		prop.intval = -1;
	ret = power_supply_set_property(chg_psy,
		POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &prop);
	if (ret != 0)
		pr_notice("%s input curr fail\n", __func__);

	/* High Voltage (Vbus) control*/
	/*Only master charger need to control Vbus*/
	/*prop.intval = 0, vbus 5V*/
	/*prop.intval = 1, vbus 9V*/
	if (chrlmt_bat_chr_curr_limit == 0) {
		prop.intval = 0;
		ret = power_supply_set_property(chg_psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &prop);
		if (ret != 0)
			pr_notice("%s vbus fail\n", __func__);
	}
	if (chrlmt_bat_chr_curr_limit == -1) {
		prop.intval = 1;
		ret = power_supply_set_property(chg_psy,
			POWER_SUPPLY_PROP_VOLTAGE_MAX, &prop);
		if (ret != 0)
			pr_notice("%s set vbus fail\n",
			__func__);
	}

	power_supply_changed(chg_psy);

}

static int chrlmt_set_limit(
struct chrlmt_handle *handle, int chr_input_curr_limit, int bat_char_curr_limit)
{
	int i;
	int min_char_input_curr_limit = 0xFFFFFF;
	int min_bat_char_curr_limit = 0xFFFFFF;
	if (!handle)
		return -1;

	handle->chr_input_curr_limit = chr_input_curr_limit;
	handle->bat_chr_curr_limit = bat_char_curr_limit;
	for (i = CHR_LMT_MAX_USER_COUNT; --i >= 0; )
		if (chrlmt_registered_users[i]) {
			if (chrlmt_registered_users[i]->chr_input_curr_limit
			> -1)
				min_char_input_curr_limit =
					MIN(chrlmt_registered_users[i]
					->chr_input_curr_limit
						, min_char_input_curr_limit);

			if (chrlmt_registered_users[i]->bat_chr_curr_limit > -1)
				min_bat_char_curr_limit =
					MIN(chrlmt_registered_users[i]
					->bat_chr_curr_limit
						, min_bat_char_curr_limit);
		}

	if (min_char_input_curr_limit == 0xFFFFFF)
		min_char_input_curr_limit = -1;
	if (min_bat_char_curr_limit == 0xFFFFFF)
		min_bat_char_curr_limit = -1;

	if ((min_char_input_curr_limit != chrlmt_chr_input_curr_limit)
	|| (min_bat_char_curr_limit != chrlmt_bat_chr_curr_limit)) {

		chrlmt_chr_input_curr_limit = min_char_input_curr_limit;
		chrlmt_bat_chr_curr_limit = min_bat_char_curr_limit;
		if (bcct_chrlmt_queue)
			queue_work(bcct_chrlmt_queue, &bcct_chrlmt_work);

		mtk_cooler_bcct_dprintk_always("%s %p %d %d\n", __func__
					, handle, chrlmt_chr_input_curr_limit
					, chrlmt_bat_chr_curr_limit);
	}

	return 0;
}


static struct thermal_cooling_device
			*cl_bcct_dev[MAX_NUM_INSTANCE_MTK_COOLER_BCCT] = { 0 };

static unsigned long cl_bcct_state[MAX_NUM_INSTANCE_MTK_COOLER_BCCT] = { 0 };
static struct chrlmt_handle cl_bcct_chrlmt_handle;

static int cl_bcct_cur_limit = 65535;

static void mtk_cl_bcct_set_bcct_limit(void)
{
	/* TODO: optimize */
	int i = 0;
	int min_limit = 65535;

	for (; i < MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i++) {
		unsigned long curr_state;

		MTK_CL_BCCT_GET_CURR_STATE(curr_state, cl_bcct_state[i]);
		if (curr_state == 1) {

			int limit;

			MTK_CL_BCCT_GET_LIMIT(limit, cl_bcct_state[i]);
			if ((min_limit > limit) && (limit > 0))
				min_limit = limit;
		}
	}

	if (min_limit != cl_bcct_cur_limit) {
		cl_bcct_cur_limit = min_limit;

		if (cl_bcct_cur_limit >= 65535) {
			chrlmt_set_limit(&cl_bcct_chrlmt_handle, -1, -1);
			mtk_cooler_bcct_dprintk("%s limit=-1\n", __func__);
		} else {
			chrlmt_set_limit(&cl_bcct_chrlmt_handle, -1,
							cl_bcct_cur_limit);

			mtk_cooler_bcct_dprintk("%s limit=%d\n", __func__
					, cl_bcct_cur_limit);
		}

		mtk_cooler_bcct_dprintk("%s real limit=%d\n", __func__
				, get_battery_current());

	}
}

static int mtk_cl_bcct_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_bcct_dprintk("%s %s %lu\n", __func__, cdev->type, *state);
	return 0;
}

static int mtk_cl_bcct_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	MTK_CL_BCCT_GET_CURR_STATE(*state, *((unsigned long *)cdev->devdata));
	mtk_cooler_bcct_dprintk("%s %s %lu\n", __func__, cdev->type, *state);
	mtk_cooler_bcct_dprintk("%s %s limit=%d\n", __func__, cdev->type,
			get_battery_current());
	return 0;
}

static int mtk_cl_bcct_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	/*Only active while lcm not off */
	if (chrlmt_is_lcmoff)
		state = 0;

	mtk_cooler_bcct_dprintk("%s %s %lu\n", __func__, cdev->type, state);
	MTK_CL_BCCT_SET_CURR_STATE(state, *((unsigned long *)cdev->devdata));
	mtk_cl_bcct_set_bcct_limit();
	mtk_cooler_bcct_dprintk("%s %s limit=%d\n", __func__, cdev->type,
			get_battery_current());

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_bcct_ops = {
	.get_max_state = mtk_cl_bcct_get_max_state,
	.get_cur_state = mtk_cl_bcct_get_cur_state,
	.set_cur_state = mtk_cl_bcct_set_cur_state,
};

static int mtk_cooler_bcct_register_ltf(void)
{
	int i;

	mtk_cooler_bcct_dprintk("%s\n", __func__);

	chrlmt_register(&cl_bcct_chrlmt_handle);

#if (MAX_NUM_INSTANCE_MTK_COOLER_BCCT == 3)
	MTK_CL_BCCT_SET_LIMIT(1000, cl_bcct_state[0]);
	MTK_CL_BCCT_SET_LIMIT(500, cl_bcct_state[1]);
	MTK_CL_BCCT_SET_LIMIT(0, cl_bcct_state[2]);
#endif

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i-- > 0;) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-bcct%02d", i);
		/* put bcct state to cooler devdata */
		cl_bcct_dev[i] = mtk_thermal_cooling_device_register(temp,
						(void *)&cl_bcct_state[i],
						&mtk_cl_bcct_ops);
	}

	return 0;
}

static void mtk_cooler_bcct_unregister_ltf(void)
{
	int i;

	mtk_cooler_bcct_dprintk("%s\n", __func__);

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i-- > 0;) {
		if (cl_bcct_dev[i]) {
			mtk_thermal_cooling_device_unregister(cl_bcct_dev[i]);
			cl_bcct_dev[i] = NULL;
			cl_bcct_state[i] = 0;
		}
	}

	chrlmt_unregister(&cl_bcct_chrlmt_handle);
}

static struct thermal_cooling_device *cl_abcct_dev;
static unsigned long cl_abcct_state;
static struct chrlmt_handle abcct_chrlmt_handle;
static long abcct_prev_temp;
static long abcct_curr_temp;
static long abcct_target_temp = 48000;
static long abcct_kp = 1000;
static long abcct_ki = 3000;
static long abcct_kd = 10000;
static int abcct_max_bat_chr_curr_limit = 3000;
static int abcct_min_bat_chr_curr_limit = 200;
static int abcct_cur_bat_chr_curr_limit;
static int abcct_input_current_limit_on;
static int abcct_HW_thermal_solution = 3000;
static int abcct_max_chr_input_curr_limit = 3000;
static int abcct_min_chr_input_curr_limit = 200;
static int abcct_cur_chr_input_curr_limit;
static long abcct_iterm;
static int abcct_times_of_ts_polling_interval = 1;
static void bat_chg_info_update(void)
{
	union power_supply_propval prop;
	struct power_supply *psy;
	struct power_supply *chr_psy;
	int ret = 0;

	psy = power_supply_get_by_name("battery");
	if (psy == NULL) {
		pr_notice("%s Couldn't get psy\n", __func__);
	} else {
		ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
		if (ret == 0)
			bat_info_vbat = prop.intval;

		ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_CAPACITY, &prop);
		if (ret == 0)
			bat_info_uisoc = prop.intval;

		ret = power_supply_get_property(psy,
			POWER_SUPPLY_PROP_CURRENT_NOW, &prop);
		if (ret == 0)
			bat_info_ibat = prop.intval;
		mtk_cooler_bcct_dprintk("vbat:%d,uisoc:%d,ibat:%d\n",
				bat_info_vbat,
				bat_info_uisoc,
				bat_info_ibat);
	}


	chr_psy = power_supply_get_by_name("mtk-master-charger");
	if (chr_psy == NULL) {
		pr_notice("%s Couldn't get chr_psy\n", __func__);
	} else {
		ret = power_supply_get_property(chr_psy,
			POWER_SUPPLY_PROP_VOLTAGE_NOW, &prop);
		if (ret == 0)
			bat_info_vbus = prop.intval;
		mtk_cooler_bcct_dprintk("vbus %d\n",
					bat_info_vbus);
		ret = power_supply_get_property(chr_psy,
			POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT, &prop);
		if (ret == 0)
			bat_info_aicr = prop.intval;
		mtk_cooler_bcct_dprintk("aicr %d\n",
					bat_info_aicr);
	}


}

static int mtk_cl_abcct_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_bcct_dprintk("%s %s %lu\n", __func__, cdev->type, *state);
	return 0;
}

static int mtk_cl_abcct_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_abcct_state;
	mtk_cooler_bcct_dprintk("%s %s %lu\n", __func__, cdev->type, *state);
	return 0;
}

static int mtk_cl_abcct_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	static ktime_t lasttime;

	cl_abcct_state = state;
	/*Only active while lcm not off */
	if (chrlmt_is_lcmoff)
		cl_abcct_state = 0;

	if (ktime_to_ms(ktime_sub(ktime_get(), lasttime)) > 5000) {
		bat_chg_info_update();
		lasttime = ktime_get();
	}

	mtk_cooler_bcct_dprintk("%s %s %lu\n", __func__,
					cdev->type, cl_abcct_state);

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_abcct_ops = {
	.get_max_state = mtk_cl_abcct_get_max_state,
	.get_cur_state = mtk_cl_abcct_get_cur_state,
	.set_cur_state = mtk_cl_abcct_set_cur_state,
};

static int mtk_cl_abcct_set_cur_temp(
struct thermal_cooling_device *cdev, unsigned long temp)
{
	long delta, pterm, dterm;
	int limit;
	static int i;
	if (++i < abcct_times_of_ts_polling_interval)
		return 0;

	i = 0;
	/* based on temp and state to do ATM */
	abcct_prev_temp = abcct_curr_temp;
	abcct_curr_temp = (long) temp;

	if (cl_abcct_state == 0) {
		abcct_iterm = 0;
		abcct_cur_bat_chr_curr_limit = abcct_max_bat_chr_curr_limit;
		abcct_cur_chr_input_curr_limit = -1;
		chrlmt_set_limit(&abcct_chrlmt_handle, -1, -1);
		return 0;
	}

	pterm = abcct_target_temp - abcct_curr_temp;

	abcct_iterm += pterm;
	if (((abcct_curr_temp < abcct_target_temp) && (abcct_iterm < 0))
	|| ((abcct_curr_temp > abcct_target_temp) && (abcct_iterm > 0)))
		abcct_iterm = 0;

	if (((abcct_curr_temp < abcct_target_temp)
		&& (abcct_curr_temp < abcct_prev_temp))
	|| ((abcct_curr_temp > abcct_target_temp)
		&& (abcct_curr_temp > abcct_prev_temp)))
		dterm = abcct_prev_temp - abcct_curr_temp;
	else
		dterm = 0;

	delta = pterm/abcct_kp + abcct_iterm/abcct_ki + dterm/abcct_kd;

	/* Align limit to 50mA to avoid redundant calls to chrlmt. */
	if (delta > 0 && delta < 50)
		delta = 50;
	else if (delta > -50 && delta < 0)
		delta = -50;
	if (abcct_cur_chr_input_curr_limit == -1) {
		limit = abcct_cur_bat_chr_curr_limit + (int) delta;
		/* Align limit to 50mA to avoid redundant calls to chrlmt. */
		limit = (limit / 50) * 50;
		limit = MIN(abcct_max_bat_chr_curr_limit, limit);
		limit = MAX(abcct_min_bat_chr_curr_limit, limit);

		abcct_cur_bat_chr_curr_limit = limit;

		if ((abcct_input_current_limit_on)
		&& (abcct_cur_bat_chr_curr_limit == 0)) {
			abcct_max_chr_input_curr_limit =
				abcct_HW_thermal_solution / 5; /* mA = mW/5V */
			abcct_cur_chr_input_curr_limit =
						abcct_max_chr_input_curr_limit;
		}
	} else {
		limit = abcct_cur_chr_input_curr_limit + (int) delta;
		/* Align limit to 50mA to avoid redundant calls to chrlmt. */
		limit = (limit / 50) * 50;
		limit = MIN(abcct_max_chr_input_curr_limit, limit);
		limit = MAX(abcct_min_chr_input_curr_limit, limit);
		abcct_cur_chr_input_curr_limit = limit;

		if (abcct_cur_chr_input_curr_limit
			== abcct_max_chr_input_curr_limit)
			abcct_cur_chr_input_curr_limit = -1;
	}

	mtk_cooler_bcct_dprintk("%s %ld %ld %ld %ld %ld %d %d\n"
			, __func__, abcct_curr_temp, pterm, abcct_iterm, dterm,
			delta, abcct_cur_chr_input_curr_limit,
			abcct_cur_bat_chr_curr_limit);
	chrlmt_set_limit(&abcct_chrlmt_handle, abcct_cur_chr_input_curr_limit,
					abcct_cur_bat_chr_curr_limit);

	return 0;
}

static struct thermal_cooling_device_ops_extra mtk_cl_abcct_ops_ext = {
	.set_cur_temp = mtk_cl_abcct_set_cur_temp
};

static struct thermal_cooling_device *cl_abcct_lcmoff_dev;
static unsigned long cl_abcct_lcmoff_state;
static struct chrlmt_handle abcct_lcmoff_chrlmt_handle;
static long abcct_lcmoff_prev_temp;
static long abcct_lcmoff_curr_temp;
static long abcct_lcmoff_target_temp = 48000;
static long abcct_lcmoff_kp = 1000;
static long abcct_lcmoff_ki = 3000;
static long abcct_lcmoff_kd = 10000;
static int abcct_lcmoff_max_bat_chr_curr_limit = 3000;
static int abcct_lcmoff_min_bat_chr_curr_limit = 200;
static int abcct_lcmoff_cur_bat_chr_curr_limit;
static long abcct_lcmoff_iterm;
static int mtk_cl_abcct_lcmoff_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	mtk_cooler_bcct_dprintk("%s %s %lu\n", __func__, cdev->type, *state);
	return 0;
}

static int mtk_cl_abcct_lcmoff_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_abcct_lcmoff_state;
	mtk_cooler_bcct_dprintk("%s %s %lu\n", __func__, cdev->type, *state);
	return 0;
}

static int mtk_cl_abcct_lcmoff_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_abcct_lcmoff_state = state;

	/*Only active while lcm off */
	if (!chrlmt_is_lcmoff)
		cl_abcct_lcmoff_state = 0;
	mtk_cooler_bcct_dprintk("%s %s %lu\n", __func__, cdev->type,
							cl_abcct_lcmoff_state);
	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_abcct_lcmoff_ops = {
	.get_max_state = mtk_cl_abcct_lcmoff_get_max_state,
	.get_cur_state = mtk_cl_abcct_lcmoff_get_cur_state,
	.set_cur_state = mtk_cl_abcct_lcmoff_set_cur_state,
};

static int mtk_cl_abcct_lcmoff_set_cur_temp(
struct thermal_cooling_device *cdev, unsigned long temp)
{
	long delta, pterm, dterm;
	int limit;

	/* based on temp and state to do ATM */
	abcct_lcmoff_prev_temp = abcct_lcmoff_curr_temp;
	abcct_lcmoff_curr_temp = (long) temp;

	if (cl_abcct_lcmoff_state == 0) {
		abcct_lcmoff_iterm = 0;
		abcct_lcmoff_cur_bat_chr_curr_limit =
					abcct_lcmoff_max_bat_chr_curr_limit;
		chrlmt_set_limit(&abcct_lcmoff_chrlmt_handle, -1, -1);
		return 0;
	}

	pterm = abcct_lcmoff_target_temp - abcct_lcmoff_curr_temp;

	abcct_lcmoff_iterm += pterm;
	if (((abcct_lcmoff_curr_temp < abcct_target_temp)
		&& (abcct_lcmoff_iterm < 0))
	|| ((abcct_lcmoff_curr_temp > abcct_target_temp)
		&& (abcct_lcmoff_iterm > 0)))
		abcct_lcmoff_iterm = 0;

	if (((abcct_lcmoff_curr_temp < abcct_target_temp)
		&& (abcct_lcmoff_curr_temp < abcct_lcmoff_prev_temp))
	|| ((abcct_lcmoff_curr_temp > abcct_target_temp)
		&& (abcct_lcmoff_curr_temp > abcct_lcmoff_prev_temp)))
		dterm = abcct_lcmoff_prev_temp - abcct_lcmoff_curr_temp;
	else
		dterm = 0;

	delta = pterm/abcct_lcmoff_kp + abcct_lcmoff_iterm/abcct_lcmoff_ki
		+ dterm/abcct_lcmoff_kd;

	/* Align limit to 50mA to avoid redundant calls to chrlmt. */
	if (delta > 0 && delta < 50)
		delta = 50;
	else if (delta > -50 && delta < 0)
		delta = -50;
	limit = abcct_lcmoff_cur_bat_chr_curr_limit + (int) delta;
	/* Align limit to 50mA to avoid redundant calls to chrlmt. */
	limit = (limit / 50) * 50;
	limit = MIN(abcct_lcmoff_max_bat_chr_curr_limit, limit);
	limit = MAX(abcct_lcmoff_min_bat_chr_curr_limit, limit);
	abcct_lcmoff_cur_bat_chr_curr_limit = limit;

	mtk_cooler_bcct_dprintk("%s %ld %ld %ld %ld %ld %d\n"
				, __func__, abcct_lcmoff_curr_temp, pterm,
				abcct_lcmoff_iterm, dterm, delta, limit);
	chrlmt_set_limit(&abcct_lcmoff_chrlmt_handle, -1, limit);

	return 0;
}

static struct thermal_cooling_device_ops_extra mtk_cl_abcct_lcmoff_ops_ext = {
	.set_cur_temp = mtk_cl_abcct_lcmoff_set_cur_temp
};

static int mtk_cooler_abcct_register_ltf(void)
{
	mtk_cooler_bcct_dprintk("%s\n", __func__);

	chrlmt_register(&abcct_chrlmt_handle);

	if (!cl_abcct_dev)
		cl_abcct_dev =
			mtk_thermal_cooling_device_register_wrapper_extra(
							"abcct", (void *)NULL,
							&mtk_cl_abcct_ops,
							&mtk_cl_abcct_ops_ext);

	return 0;
}

static void mtk_cooler_abcct_unregister_ltf(void)
{
	mtk_cooler_bcct_dprintk("%s\n", __func__);

	if (cl_abcct_dev) {
		mtk_thermal_cooling_device_unregister(cl_abcct_dev);
		cl_abcct_dev = NULL;
		cl_abcct_state = 0;
	}

	chrlmt_unregister(&abcct_chrlmt_handle);
}

static int mtk_cooler_abcct_lcmoff_register_ltf(void)
{
	mtk_cooler_bcct_dprintk("%s\n", __func__);

	chrlmt_register(&abcct_lcmoff_chrlmt_handle);

	if (!cl_abcct_lcmoff_dev)
		cl_abcct_lcmoff_dev =
			mtk_thermal_cooling_device_register_wrapper_extra(
						"abcct_lcmoff", (void *)NULL,
						&mtk_cl_abcct_lcmoff_ops,
						&mtk_cl_abcct_lcmoff_ops_ext);

	return 0;
}

static void mtk_cooler_abcct_lcmoff_unregister_ltf(void)
{
	mtk_cooler_bcct_dprintk("%s\n", __func__);

	if (cl_abcct_lcmoff_dev) {
		mtk_thermal_cooling_device_unregister(cl_abcct_lcmoff_dev);
		cl_abcct_lcmoff_dev = NULL;
		cl_abcct_lcmoff_state = 0;
	}

	chrlmt_unregister(&abcct_lcmoff_chrlmt_handle);
}

static ssize_t _cl_bcct_write(
struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	/* int ret = 0; */
	char tmp[128] = { 0 };
	int klog_on, limit0, limit1, limit2;

	len = (len < (128 - 1)) ? len : (128 - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	/**
	 * sscanf format <klog_on> <mtk-cl-bcct00 limit> <mtk-cl-bcct01 limit>
	 * <klog_on> can only be 0 or 1
	 * <mtk-cl-bcct00 limit> can only be positive integer
	 * or -1 to denote no limit
	 */

	if (data == NULL) {
		mtk_cooler_bcct_dprintk("%s null data\n", __func__);
		return -EINVAL;
	}
	/* WARNING: Modify here if
	 * MTK_THERMAL_MONITOR_COOLER_MAX_EXTRA_CONDITIONS
	 * is changed to other than 3
	 */
#if (MAX_NUM_INSTANCE_MTK_COOLER_BCCT == 3)
	MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[0]);
	MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[1]);
	MTK_CL_BCCT_SET_LIMIT(-1, cl_bcct_state[2]);

	if (sscanf(
		tmp, "%d %d %d %d", &klog_on, &limit0, &limit1, &limit2) >= 1) {
		if (klog_on == 0 || klog_on == 1)
			cl_bcct_klog_on = klog_on;

		if (limit0 >= -1)
			MTK_CL_BCCT_SET_LIMIT(limit0, cl_bcct_state[0]);
		if (limit1 >= -1)
			MTK_CL_BCCT_SET_LIMIT(limit1, cl_bcct_state[1]);
		if (limit2 >= -1)
			MTK_CL_BCCT_SET_LIMIT(limit2, cl_bcct_state[2]);

		return len;
	}
#else
#error	\
"Change correspondent part when changing MAX_NUM_INSTANCE_MTK_COOLER_BCCT!"
#endif
	mtk_cooler_bcct_dprintk("%s bad argument\n", __func__);
	return -EINVAL;
}

static int _cl_bcct_read(struct seq_file *m, void *v)
{
	/**
	 * The format to print out:
	 *  kernel_log <0 or 1>
	 *  <mtk-cl-bcct<ID>> <bcc limit>
	 *  ..
	 */

	mtk_cooler_bcct_dprintk("%s\n", __func__);

	{
		int i = 0;

		seq_printf(m, "%d\n", cl_bcct_cur_limit);
		seq_printf(m, "klog %d\n", cl_bcct_klog_on);
		seq_printf(m, "curr_limit %d\n", cl_bcct_cur_limit);

		for (; i < MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i++) {
			int limit;
			unsigned int curr_state;

			MTK_CL_BCCT_GET_LIMIT(limit, cl_bcct_state[i]);
			MTK_CL_BCCT_GET_CURR_STATE(
						curr_state, cl_bcct_state[i]);

			seq_printf(m, "mtk-cl-bcct%02d %d mA, state %d\n", i,
							limit, curr_state);
		}
	}

	return 0;
}

static int _cl_bcct_open(struct inode *inode, struct file *file)
{
	return single_open(file, _cl_bcct_read, PDE_DATA(inode));
}

static const struct file_operations _cl_bcct_fops = {
	.owner = THIS_MODULE,
	.open = _cl_bcct_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _cl_bcct_write,
	.release = single_release,
};

static ssize_t _cl_abcct_write(
struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	/* int ret = 0; */
	char tmp[128] = { 0 };
	long _abcct_target_temp, _abcct_kp, _abcct_ki, _abcct_kd;
	int _max_cur, _min_cur, _input_current_limit_on, _HW_thermal_sol,
				_min_input, _times_of_ts_polling_inteval;

	int _pep30_max_input, _pep30_min_input;
	int scan_count = 0;

	len = (len < (128 - 1)) ? len : (128 - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	if (data == NULL)  {
		mtk_cooler_bcct_dprintk("%s null data\n", __func__);
		return -EINVAL;
	}

	scan_count = sscanf(tmp, "%ld %ld %ld %ld %d %d %d %d %d %d %d %d",
				&_abcct_target_temp, &_abcct_kp, &_abcct_ki,
				&_abcct_kd, &_max_cur, &_min_cur,
				&_input_current_limit_on, &_HW_thermal_sol,
				&_min_input, &_times_of_ts_polling_inteval,
				&_pep30_max_input, &_pep30_min_input);

	if (scan_count >= 6) {
		abcct_target_temp = _abcct_target_temp;
		abcct_kp = _abcct_kp;
		abcct_ki = _abcct_ki;
		abcct_kd = _abcct_kd;
		abcct_max_bat_chr_curr_limit = _max_cur;
		abcct_min_bat_chr_curr_limit = _min_cur;

		if (scan_count > 6) {
			abcct_input_current_limit_on = _input_current_limit_on;
			abcct_HW_thermal_solution = _HW_thermal_sol;
			abcct_min_chr_input_curr_limit = _min_input;
			abcct_times_of_ts_polling_interval =
						_times_of_ts_polling_inteval;
		}
		abcct_cur_chr_input_curr_limit = -1;
		abcct_cur_bat_chr_curr_limit = abcct_max_bat_chr_curr_limit;
		abcct_iterm = 0;

		return len;
	}

	mtk_cooler_bcct_dprintk("%s bad argument\n", __func__);
	return -EINVAL;
}

static int _cl_abcct_read(struct seq_file *m, void *v)
{
	mtk_cooler_bcct_dprintk("%s\n", __func__);

	seq_printf(m, "%d %ld %ld %ld %ld %d %d\n",
				abcct_cur_bat_chr_curr_limit, abcct_target_temp,
				abcct_kp, abcct_ki, abcct_kd,
				abcct_max_bat_chr_curr_limit,
				abcct_min_bat_chr_curr_limit);

	seq_printf(m, "abcct_cur_bat_chr_curr_limit %d\n",
						abcct_cur_bat_chr_curr_limit);

	seq_printf(m, "abcct_cur_chr_input_curr_limit %d\n",
						abcct_cur_chr_input_curr_limit);
	seq_printf(m, "abcct_target_temp %ld\n", abcct_target_temp);
	seq_printf(m, "abcct_kp %ld\n", abcct_kp);
	seq_printf(m, "abcct_ki %ld\n", abcct_ki);
	seq_printf(m, "abcct_kd %ld\n", abcct_kd);
	seq_printf(m, "abcct_max_bat_chr_curr_limit %d\n",
						abcct_max_bat_chr_curr_limit);

	seq_printf(m, "abcct_min_bat_chr_curr_limit %d\n",
						abcct_min_bat_chr_curr_limit);

	seq_printf(m, "abcct_input_current_limit_on %d\n",
						abcct_input_current_limit_on);

	seq_printf(m, "abcct_HW_thermal_solution %d\n",
						abcct_HW_thermal_solution);

	seq_printf(m, "abcct_min_chr_input_curr_limit %d\n",
						abcct_min_chr_input_curr_limit);

	seq_printf(m, "abcct_times_of_ts_polling_interval %d\n",
					abcct_times_of_ts_polling_interval);
	return 0;
}

static int _cl_abcct_open(struct inode *inode, struct file *file)
{
	return single_open(file, _cl_abcct_read, PDE_DATA(inode));
}

static const struct file_operations _cl_abcct_fops = {
	.owner = THIS_MODULE,
	.open = _cl_abcct_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _cl_abcct_write,
	.release = single_release,
};

static ssize_t _cl_abcct_lcmoff_write(
struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	/* int ret = 0; */
	char tmp[128] = { 0 };
	int _lcmoff_policy_enable;
	long _abcct_lcmoff_target_temp, _abcct_lcmoff_kp,
			_abcct_lcmoff_ki, _abcct_lcmoff_kd;

	int _max_cur, _min_cur, _pep30_max_input, _pep30_min_input;
	int scan_count = 0;

	len = (len < (128 - 1)) ? len : (128 - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	if (data == NULL) {
		mtk_cooler_bcct_dprintk("%s null data\n", __func__);
		return -EINVAL;
	}

	scan_count =  sscanf(tmp, "%d %ld %ld %ld %ld %d %d %d %d"
				, &_lcmoff_policy_enable
				, &_abcct_lcmoff_target_temp, &_abcct_lcmoff_kp
				, &_abcct_lcmoff_ki, &_abcct_lcmoff_kd
				, &_max_cur, &_min_cur, &_pep30_max_input
				, &_pep30_min_input);

	if (scan_count >= 7) {
		chrlmt_lcmoff_policy_enable = _lcmoff_policy_enable;
		abcct_lcmoff_target_temp = _abcct_lcmoff_target_temp;
		abcct_lcmoff_kp = _abcct_lcmoff_kp;
		abcct_lcmoff_ki = _abcct_lcmoff_ki;
		abcct_lcmoff_kd = _abcct_lcmoff_kd;
		abcct_lcmoff_max_bat_chr_curr_limit = _max_cur;
		abcct_lcmoff_min_bat_chr_curr_limit = _min_cur;
		abcct_lcmoff_cur_bat_chr_curr_limit =
					abcct_lcmoff_max_bat_chr_curr_limit;

		abcct_lcmoff_iterm = 0;
		return len;
	}

	mtk_cooler_bcct_dprintk("%s bad argument\n", __func__);
	return -EINVAL;
}

static int _cl_abcct_lcmoff_read(struct seq_file *m, void *v)
{
	mtk_cooler_bcct_dprintk("%s\n", __func__);

	seq_printf(m, "chrlmt_lcmoff_policy_enable %d\n",
						chrlmt_lcmoff_policy_enable);

	seq_printf(m, "%d\n", abcct_lcmoff_cur_bat_chr_curr_limit);
	seq_printf(m, "abcct_lcmoff_cur_bat_chr_curr_limit %d\n",
					abcct_lcmoff_cur_bat_chr_curr_limit);
	seq_printf(m, "abcct_lcmoff_target_temp %ld\n",
						abcct_lcmoff_target_temp);

	seq_printf(m, "abcct_lcmoff_kp %ld\n", abcct_lcmoff_kp);
	seq_printf(m, "abcct_lcmoff_ki %ld\n", abcct_lcmoff_ki);
	seq_printf(m, "abcct_lcmoff_kd %ld\n", abcct_lcmoff_kd);
	seq_printf(m, "abcct_lcmoff_max_bat_chr_curr_limit %d\n",
					abcct_lcmoff_max_bat_chr_curr_limit);

	seq_printf(m, "abcct_lcmoff_min_bat_chr_curr_limit %d\n",
					abcct_lcmoff_min_bat_chr_curr_limit);
	return 0;
}

static int _cl_abcct_lcmoff_open(struct inode *inode, struct file *file)
{
	return single_open(file, _cl_abcct_lcmoff_read, PDE_DATA(inode));
}

static const struct file_operations _cl_abcct_lcmoff_fops = {
	.owner = THIS_MODULE,
	.open = _cl_abcct_lcmoff_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _cl_abcct_lcmoff_write,
	.release = single_release,
};

static void bcct_lcmoff_switch(int onoff)
{
	mtk_cooler_bcct_dprintk("%s: onoff = %d\n", __func__, onoff);

	/* onoff = 0: LCM OFF */
	/* others: LCM ON */
	if (onoff) {
		/* deactivate lcmoff policy */
		chrlmt_is_lcmoff = 0;
	} else {
		/* activate lcmoff policy */
		chrlmt_is_lcmoff = 1;
	}
}

static int bcct_lcmoff_fb_notifier_callback(
struct notifier_block *self, unsigned long event, void *data)
{
	struct fb_event *evdata = data;
	int blank;

	/* skip if it's not a blank event */
	if ((event != FB_EVENT_BLANK) || (data == NULL))
		return 0;

	/* skip if policy is not enable */
	if (!chrlmt_lcmoff_policy_enable)
		return 0;

	blank = *(int *)evdata->data;
	mtk_cooler_bcct_dprintk("%s: blank = %d, event = %lu\n",
						__func__, blank, event);

	switch (blank) {
	/* LCM ON */
	case FB_BLANK_UNBLANK:
		bcct_lcmoff_switch(1);
		break;
		/* LCM OFF */
	case FB_BLANK_POWERDOWN:
		bcct_lcmoff_switch(0);
		break;
	default:
		break;
	}

	return 0;
}

static struct notifier_block bcct_lcmoff_fb_notifier = {
	.notifier_call = bcct_lcmoff_fb_notifier_callback,
};

static int _cl_chrlmt_read(struct seq_file *m, void *v)
{
	mtk_cooler_bcct_dprintk("%s\n", __func__);

	seq_printf(m, "%d,%d\n", chrlmt_chr_input_curr_limit,
						chrlmt_bat_chr_curr_limit);
	seq_printf(m, "chrlmt_chr_input_curr_limit %d\n",
						chrlmt_chr_input_curr_limit);

	seq_printf(m, "chrlmt_bat_chr_curr_limit %d\n",
						chrlmt_bat_chr_curr_limit);

	seq_printf(m, "abcct_cur_bat_chr_curr_limit %d\n",
						abcct_cur_bat_chr_curr_limit);

	seq_printf(m, "cl_bcct_cur_limit %d\n", cl_bcct_cur_limit);

	return 0;
}

static int _cl_chrlmt_open(struct inode *inode, struct file *file)
{
	return single_open(file, _cl_chrlmt_read, PDE_DATA(inode));
}

static const struct file_operations _cl_chrlmt_fops = {
	.owner = THIS_MODULE,
	.open = _cl_chrlmt_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int _cl_battery_status_read(struct seq_file *m, void *v)
{
	mtk_cooler_bcct_dprintk("%s\n", __func__);

	seq_printf(m, "%d,%d,%d,%d,%d,%d,%d,%d\n",
			bat_info_uisoc, bat_info_vbat,
			bat_info_ibat, bat_info_mintchr, bat_info_maxtchr,
			bat_info_vbus, bat_info_aicr, bat_info_charging_type);

	return 0;
}

static int _cl_battery_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, _cl_battery_status_read, PDE_DATA(inode));
}

static const struct file_operations _cl_battery_status_fops = {
	.owner = THIS_MODULE,
	.open = _cl_battery_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

int mtk_cooler_is_abcct_unlimit(void)
{
	return (cl_abcct_state == 0 &&  cl_abcct_lcmoff_state == 0) ? 1 : 0;
}
EXPORT_SYMBOL(mtk_cooler_is_abcct_unlimit);



static int __init mtk_cooler_bcct_init(void)
{
	int err = 0;
	int i;

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_BCCT; i-- > 0;) {
		cl_bcct_dev[i] = NULL;
		cl_bcct_state[i] = 0;
	}

	/* cl_bcct_dev = NULL; */

	mtk_cooler_bcct_dprintk("%s\n", __func__);

	err = mtk_cooler_bcct_register_ltf();
	if (err)
		goto err_unreg;

	err = mtk_cooler_abcct_register_ltf();
	if (err)
		goto err_unreg;

	err = mtk_cooler_abcct_lcmoff_register_ltf();
	if (err)
		goto err_unreg;

	if (fb_register_client(&bcct_lcmoff_fb_notifier)) {
		mtk_cooler_bcct_dprintk_always(
					"%s: register FB client failed!\n",
					__func__);
		err = -EINVAL;
		goto err_unreg;
	}

	/* create a proc file */
	{
		struct proc_dir_entry *entry = NULL;
		struct proc_dir_entry *dir_entry = NULL;

		dir_entry = mtk_thermal_get_proc_drv_therm_dir_entry();
		if (!dir_entry) {
			mtk_cooler_bcct_dprintk(
				"[%s]: mkdir /proc/driver/thermal failed\n",
				__func__);
		}

		entry = proc_create("clbcct", 0664, dir_entry, &_cl_bcct_fops);

		if (!entry)
			mtk_cooler_bcct_dprintk_always(
				"%s clbcct creation failed\n", __func__);
		else
			proc_set_user(entry, uid, gid);

		entry = proc_create("clabcct", 0664,
					dir_entry, &_cl_abcct_fops);

		if (!entry)
			mtk_cooler_bcct_dprintk_always(
				"%s clabcct creation failed\n", __func__);
		else
			proc_set_user(entry, uid, gid);

		entry = proc_create("clabcct_lcmoff", 0664,
					dir_entry, &_cl_abcct_lcmoff_fops);

		if (!entry)
			mtk_cooler_bcct_dprintk_always(
					"%s clabcct_lcmoff creation failed\n",
					__func__);
		else
			proc_set_user(entry, uid, gid);

		entry = proc_create("bcctlmt", 0444, NULL, &_cl_chrlmt_fops);

		entry = proc_create("battery_status", 0444, NULL,
						&_cl_battery_status_fops);
	}

	bcct_chrlmt_queue = alloc_workqueue("bcct_chrlmt_work",
			WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	INIT_WORK(&bcct_chrlmt_work, chrlmt_set_limit_handler);

	return 0;

err_unreg:
	mtk_cooler_bcct_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_bcct_exit(void)
{
	mtk_cooler_bcct_dprintk("%s\n", __func__);

	if (bcct_chrlmt_queue) {
		cancel_work_sync(&bcct_chrlmt_work);
		flush_workqueue(bcct_chrlmt_queue);
		destroy_workqueue(bcct_chrlmt_queue);
		bcct_chrlmt_queue = NULL;
	}

	/* remove the proc file */
	remove_proc_entry("driver/thermal/clbcct", NULL);
	remove_proc_entry("driver/thermal/clabcct", NULL);
	remove_proc_entry("driver/thermal/clabcct_lcmoff", NULL);

	mtk_cooler_bcct_unregister_ltf();
	mtk_cooler_abcct_unregister_ltf();
	mtk_cooler_abcct_lcmoff_unregister_ltf();

	fb_unregister_client(&bcct_lcmoff_fb_notifier);

}

module_init(mtk_cooler_bcct_init);
module_exit(mtk_cooler_bcct_exit);

