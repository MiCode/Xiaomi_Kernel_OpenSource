/*
 * Atmel maXTouch Touchscreen driver Plug in
 *
 * Copyright (C) 2013 Atmel Co.Ltd
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * Author: Pitter Liao <pitter.liao@atmel.com>
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 *
 */

/****************************************************************
	Pitter Liao add for macro for the global platform
		email:  pitter.liao@atmel.com
		mobile: 13244776877
-----------------------------------------------------------------*/
#define PLUG_WDG_VERSION 0x0011
/*----------------------------------------------------------------
fixed some bugs
0.11
1 improve reset algorithm
0.1
1 first version of wd plugin: WDG support
*/

#include "plug.h"
#include <linux/delay.h>

#define WD_FLAG_RESETING				(1<<0)
#define WD_FLAG_CALING					(1<<1)

#define WD_FLAG_RESET					(1<<4)
#define WD_FLAG_CAL						(1<<5)
#define WD_FLAG_RESUME					(1<<6)

#define WD_FLAG_REG_OVERFLOW			(1<<8)
#define WD_FLAG_TIMER_OVERFLOW			(1<<9)
#define WD_FLAG_REG_TICKS			(1<<12)
#define WD_FLAG_TIMER_TICKS			(1<<13)

#define WD_FLAG_FUNC_REGISTER			(1<<16)
#define WD_FLAG_FUNC_TIMER				(1<<17)

#define WD_FLAG_CL_MASK_SHIFT			16

#define WD_FLAG_WORKAROUND_HALT			(1<<31)

#define WD_FLAG_MASK_LOW			(0x000f0)
#define WD_FLAG_MASK_NORMAL		(0x00f00)
#define WD_FLAG_MASK_FUNC			(0xf0000)
#define WD_FLAG_MASK				(-1)

struct timer_observer {
	unsigned long flag;

	unsigned long time_wd_check_point;
	int count;
	int failed;
	int retry;
};

struct register_observer {
	unsigned long flag;

	int failed;
	int curr;

	int retry;
};

struct wd_observer{
	unsigned long flag;

	struct timer_observer timer;
	struct register_observer reg;
};

struct timer_config {
	unsigned long interval_wd_check;
	int min_check_count;
	int failed_reset_retry;
};

struct register_config {
	int failed_reset_count;
	int failed_reset_retry;
};

struct wd_config{
	struct timer_config timer;
	struct register_config reg;

	unsigned long interval_wdc_recheck;
};

static int wdg_ticks_enable(struct plugin_wdg *p, bool enable)
{
	struct reg_config t61_ticks = {.reg = MXT_SPT_TIMER_T61, .instance = T61_TIMER_ID_WDG,
			.offset = 1, .buf = {0}, .len = 1, .mask = 0, .flag = 0, .sleep = 0};

	if (enable)
		t61_ticks.buf[0] = 0x1;
	else
		t61_ticks.buf[0] = 0x2;
	return  p->set_obj_cfg(p->dev, &t61_ticks, NULL, 0);
}

static int wdg_handle_re_event(struct plugin_wdg *p, unsigned long flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct wd_config *cfg = p->cfg;
	struct wd_observer *obs = p->obs;
	struct register_config *re_cfg = &cfg->reg;
	struct register_observer *re_obs = &obs->reg;
	int ret = 0;

	if (test_flag(WD_FLAG_REG_OVERFLOW, &flag)) {
		re_obs->failed++;
		re_obs->curr++;

		if (re_obs->curr >= re_cfg->failed_reset_count) {
			if (re_obs->retry < re_cfg->failed_reset_retry) {
				dev_info(dev, "mxt WD reg set reset count %d(%d) retry %d(%d)\n",
					re_obs->curr, re_cfg->failed_reset_count, re_obs->retry, re_cfg->failed_reset_retry);
				if (p->reset)
					p->reset(p->dev, re_obs->retry);
				re_obs->retry++;

				ret = -EACCES;
			}
		}
	} else if (test_flag(WD_FLAG_REG_TICKS, &flag)) {
		re_obs->curr = 0;
		re_obs->retry = 0;
	}

	return ret;
}

static long wdg_handle_ti_event(struct plugin_wdg *p, unsigned long flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct wd_config *cfg = p->cfg;
	struct wd_observer *obs = p->obs;
	struct timer_config *ti_cfg = &cfg->timer;
	struct timer_observer *ti_obs = &obs->timer;
	int ret = 0;

	if (test_flag(WD_FLAG_RESET, &flag)) {
		   dev_dbg2(dev, "WD hook T61 reset\n");
		   ti_obs->time_wd_check_point = jiffies;
		   ti_obs->retry = 0;

		   wdg_ticks_enable(p, true);
	} else if (test_flag(WD_FLAG_TIMER_TICKS, &flag)) {
		dev_dbg2(dev, "WD hook T61 count %d\n", ti_obs->count);
		ti_obs->retry = 0;
		ti_obs->count++;
		if (ti_obs->count < 0)
			ti_obs->count = ti_cfg->min_check_count;
		ti_obs->time_wd_check_point = jiffies;
	} else if (test_flag(WD_FLAG_TIMER_OVERFLOW, &flag)) {
		dev_dbg2(dev, "WD hook T61 check overflow\n");
		if (ti_obs->count >= ti_cfg->min_check_count) {
			if (time_after_eq(jiffies, ti_obs->time_wd_check_point + ti_cfg->interval_wd_check)) {
				dev_info(dev, "mxt WD time out jiffise(%ld) check(%ld %ld) count %d(%d) retry %d(%d)\n",
					jiffies, ti_obs->time_wd_check_point, ti_cfg->interval_wd_check,
					ti_obs->count, ti_cfg->min_check_count, ti_obs->retry, ti_cfg->failed_reset_retry);
				ti_obs->failed++;
				if (ti_obs->retry < ti_cfg->failed_reset_retry) {
					if (p->reset)
						p->reset(p->dev, ti_obs->retry);
					ti_obs->retry++;
					ret = -EACCES;
				}
			}
		}
	}

	return ret;
}

static void plugin_wdg_hook_t6(struct plugin_wdg *p, u8 status)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct wd_observer *obs = p->obs;

	if (status & (MXT_T6_STATUS_RESET|MXT_T6_STATUS_CAL)) {
		dev_dbg2(dev, "WD hook T6 0x%x\n", status);

		if (status & MXT_T6_STATUS_CAL) {
			set_and_clr_flag(WD_FLAG_CALING,
				0, &obs->flag);
		}

		if (status & MXT_T6_STATUS_RESET) {
			set_and_clr_flag(WD_FLAG_RESETING,
				WD_FLAG_MASK_NORMAL, &obs->flag);
		}
	} else {
		if (test_flag(WD_FLAG_RESETING, &obs->flag))
			set_and_clr_flag(WD_FLAG_RESET,
				WD_FLAG_RESETING, &obs->flag);
		if (test_flag(WD_FLAG_CALING, &obs->flag))
			set_and_clr_flag(WD_FLAG_CAL,
				WD_FLAG_CALING, &obs->flag);

			dev_dbg2(dev, "WD hook T6 end\n");
	}

	dev_info2(dev, "mxt wd flag=0x%lx %x\n",
		 obs->flag, status);
}

static void plugin_wdg_hook_t61(struct plugin_wdg *p, int id, u8 state)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct wd_observer *obs = p->obs;

	if (!test_flag(WD_FLAG_FUNC_TIMER, &obs->flag))
		return;

	dev_dbg(dev, "mxt wd timer(%d) state %x\n",
		 id, state);

	if (id == T61_TIMER_ID_WDG) {
		if (state & MXT_T61_ELAPSED) {
			clear_flag(WD_FLAG_TIMER_OVERFLOW, &obs->flag);
			wdg_handle_ti_event(p, WD_FLAG_TIMER_TICKS);
		}
	}
}

static int plugin_wdg_hook_reg_access(struct plugin_wdg *p, u16 addr, u16 reg, u16 len, const void *val, unsigned long flag, int result, bool is_w)
{
	struct wd_observer *obs = p->obs;
	int ret = result;

	if (!test_flag(WD_FLAG_FUNC_REGISTER, &obs->flag))
		return ret;

	if (result)
		set_flag(WD_FLAG_REG_OVERFLOW, &obs->flag);
	else {
		clear_flag(WD_FLAG_REG_OVERFLOW, &obs->flag);
		wdg_handle_re_event(p, WD_FLAG_REG_TICKS);
	}

	return ret;
}

static void plugin_wdg_pre_process_messages(struct plugin_wdg *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct wd_observer *obs = p->obs;

	if (test_flag(WD_FLAG_WORKAROUND_HALT, &obs->flag))
		return;

	dev_dbg2(dev, "mxt plugin_wdg_pre_process_messages pl_flag=0x%lx flag=0x%lx\n",
		 pl_flag, obs->flag);
}

static long plugin_wdg_post_process_messages(struct plugin_wdg *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct wd_config *cfg = p->cfg;
	struct wd_observer *obs = p->obs;
	struct timer_config *ti_cfg = &cfg->timer;
	struct timer_observer *ti_obs = &obs->timer;
	long interval = MAX_SCHEDULE_TIMEOUT;
	int ret[2];

	if (test_flag(WD_FLAG_WORKAROUND_HALT, &obs->flag))
		return interval;

	if (test_flag(WD_FLAG_RESETING, &obs->flag))
		return interval;

	dev_dbg(dev, "mxt wd pl_flag=0x%lx flag=0x%lx\n",
		 pl_flag, obs->flag);

	ret[0] = ret[1] = 0;
	if (test_flag(WD_FLAG_FUNC_REGISTER, &obs->flag))
		ret[0] = wdg_handle_re_event(p, obs->flag);

	if (test_flag(WD_FLAG_FUNC_TIMER, &obs->flag))
		ret[1] = wdg_handle_ti_event(p, test_flag(WD_FLAG_RESET | WD_FLAG_CAL, &obs->flag) ?
										WD_FLAG_RESET : WD_FLAG_TIMER_OVERFLOW);

	if (ret[0] || ret[1])
		interval = cfg->interval_wdc_recheck;
	else {
		if (ti_obs->count >= ti_cfg->min_check_count)
			interval = ti_cfg->interval_wd_check + 1;
	}

	clear_flag(WD_FLAG_MASK_LOW, &obs->flag);

	return interval;
}

static void plugin_wdg_start(struct plugin_wdg *p, bool resume)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct wd_observer *obs = p->obs;
	int ret = 0;

	if (test_flag(WD_FLAG_TIMER_OVERFLOW | WD_FLAG_REG_OVERFLOW, &obs->flag)) {
		dev_err(dev, "wdg start reset flag 0x%lx\n", obs->flag);
		if (p->reset)
			ret = p->reset(p->dev, 1);
		if (ret == 0)
			clear_flag(WD_FLAG_TIMER_OVERFLOW | WD_FLAG_REG_OVERFLOW, &obs->flag);
	} else
		wdg_handle_ti_event(p, WD_FLAG_TIMER_TICKS);
}

static void plugin_wdg_stop(struct plugin_wdg *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct wd_observer *obs = p->obs;
	int ret = 0;

	if (test_flag(WD_FLAG_TIMER_OVERFLOW | WD_FLAG_REG_OVERFLOW, &obs->flag)) {
		dev_err(dev, "wdg stop reset flag 0x%lx\n", obs->flag);
		if (p->reset)
			ret = p->reset(p->dev, 1);
		if (ret == 0)
			clear_flag(WD_FLAG_TIMER_OVERFLOW | WD_FLAG_REG_OVERFLOW, &obs->flag);
	}
}

int plugin_wd_mxt_show(struct plugin_wdg *p, char *buf, size_t count)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct wd_config *cfg = p->cfg;
	struct wd_observer *obs = p->obs;
	struct timer_config *ti_cfg = &cfg->timer;
	struct timer_observer *ti_obs = &obs->timer;
	struct register_config *re_cfg = &cfg->reg;
	struct register_observer *re_obs = &obs->reg;

	int offset = 0;

	if (!p->init)
		return 0;

	dev_info(dev, "[mxt]wdg status: Flag=0x%08lx\n",
		obs->flag);

	dev_info(dev, "[mxt]wdg ti: interval %lx count %d retry %d\n",
		ti_cfg->interval_wd_check, ti_cfg->min_check_count, ti_cfg->failed_reset_retry);

	dev_info(dev, "[mxt]wdg re: count %d retry %d\n",
		re_cfg->failed_reset_count, re_cfg->failed_reset_retry);

	dev_info(dev, "[mxt]wdg ti: st %lx count %d failed %d retry %d flag %lx\n",
		ti_obs->time_wd_check_point, ti_obs->count, ti_obs->failed, ti_obs->retry, re_obs->flag);

	dev_info(dev, "[mxt]wdg re: failed %d count %d retry %d flag %lx\n",
		re_obs->failed, re_obs->curr, re_obs->retry, re_obs->flag);

	if (count > 0) {
		offset += scnprintf(buf + offset, count - offset, "Func %lx\n",
			(obs->flag & WD_FLAG_MASK_FUNC) >> WD_FLAG_CL_MASK_SHIFT);
	}

	return offset;
}


size_t plugin_wd_mxt_store(struct plugin_wdg *p, const char *buf, size_t count)
{
	printk(KERN_ERR "[mxt]plugin_wdg_store: ------------------------- \n");

	if (!p->init)
		return 0;

	return count;
}


int plugin_wd_debug_show(struct plugin_wdg *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct wd_observer *obs = p->obs;

	dev_info(dev, "[mxt]PLUG_WDG_VERSION: 0x%x\n", PLUG_WDG_VERSION);

	if (!p->init)
		return 0;

	dev_info(dev, "[mxt]wdg status: Flag=0x%08lx\n",
		obs->flag);

	plugin_wd_mxt_show(p, NULL, 0);

	dev_info(dev, "[mxt]\n");

	return 0;
}

static int plugin_wd_debug_store(struct plugin_wdg *p, const char *buf, size_t count)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct wd_config *cfg = p->cfg;
	struct wd_observer *obs = p->obs;
	struct timer_config *ti_cfg = &cfg->timer;
	struct register_config *re_cfg = &cfg->reg;
	bool enable = false;
	int offset, ret;
	char name[255];
	int config[10];

	dev_info(dev, "[mxt]wd store:%s\n", buf);

	if (!p->init)
		return 0;

	if (count <= 0)
		return 0;

	if (sscanf(buf, "status: Flag=0x%lx\n",
		&obs->flag) > 0) {
		dev_info(dev, "[mxt] OK\n");
	} else if (sscanf(buf, "enable %x\n",
		&config[0]) > 0) {
		config[0] <<= WD_FLAG_CL_MASK_SHIFT;
		config[0]  &= WD_FLAG_MASK_FUNC;
		set_and_clr_flag(config[0], WD_FLAG_MASK_FUNC, &obs->flag);
		dev_info(dev, "[mxt] set func %x\n", config[0]);

		if (test_flag(WD_FLAG_FUNC_TIMER, &obs->flag))
			enable = true;
		wdg_ticks_enable(p, enable);
	} else {
		if (count > 4 && count < sizeof(name)) {
			ret = sscanf(buf, "%s: %n", name, &offset);
			dev_info2(dev, "name %s, offset %d, ret %d\n", name, offset, ret);
			if (ret == 1) {
				if (strncmp(name, "ti", 2) == 0) {
					if (sscanf(buf + offset, "interval %lx count %d retry %d\n",
						&ti_cfg->interval_wd_check, &ti_cfg->min_check_count, &ti_cfg->failed_reset_retry) == 3) {
					} else {
						dev_err(dev, "Unknow wd ti command: %s\n", buf + offset);
					}
				}
				if (strncmp(name, "re", 2) == 0) {
					if (sscanf(buf + offset, "re: count %d retry %d\n",
						&re_cfg->failed_reset_count, &re_cfg->failed_reset_retry) == 2) {
					} else {
						dev_err(dev, "Unknow wd ti command: %s\n", buf + offset);
					}
				} else {
					dev_err(dev, "Unknow wd command: %s\n", buf);
					return -EINVAL;
				}
			} else {
				dev_err(dev, "Unknow parameter, ret %d\n", ret);
			}
		}
	}

	return 0;
}

static int init_ti(struct plugin_wdg *p)
{
	struct wd_config *cfg = p->cfg;
	struct timer_config *ti_cfg = &cfg->timer;

	ti_cfg->interval_wd_check = 2 * HZ + (HZ/10);
	ti_cfg->min_check_count = 2;
	ti_cfg->failed_reset_retry = 3;

	return 0;
}

static void deinit_ti(struct plugin_wdg *p)
{

}

static int init_re(struct plugin_wdg *p)
{
	struct wd_config *cfg = p->cfg;
	struct register_config *re_cfg = &cfg->reg;

	re_cfg->failed_reset_count = 1;
	re_cfg->failed_reset_retry = 5;

	return 0;
}

static void deinit_re(struct plugin_wdg *p)
{

}

static int init_wd_object(struct plugin_wdg *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct wd_config *cfg = p->cfg;
	struct wd_observer *obs = p->obs;
	int ret;

	ret = init_ti(p);
	if (ret) {
		dev_err(dev, "Failed to init_ti %s\n", __func__);
		return -ENOMEM;
	}

	ret = init_re(p);
	if (ret) {
		dev_err(dev, "Failed to init_re %s\n", __func__);
		return -ENOMEM;
	}

	cfg->interval_wdc_recheck = HZ / 8;

	set_flag(/*WD_FLAG_FUNC_TIMER|WD_FLAG_FUNC_REGISTER*/0, &obs->flag);

	return ret;
}

static int deinit_wd_object(struct plugin_wdg *p)
{
	deinit_re(p);
	deinit_ti(p);

	return 0;
}

static int plugin_wdg_init(struct plugin_wdg *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;

	dev_info(dev, "%s: plugin wdg wd version 0x%x\n",
			__func__, PLUG_WDG_VERSION);

	p->obs = kzalloc(sizeof(struct wd_observer), GFP_KERNEL);
	if (!p->obs) {
		dev_err(dev, "Failed to allocate memory for wd observer\n");
		return -ENOMEM;
	}

	p->cfg = kzalloc(sizeof(struct wd_config), GFP_KERNEL);
	if (!p->cfg) {
		dev_err(dev, "Failed to allocate memory for wd cfg\n");
		kfree(p->obs);
		p->obs = NULL;
		return -ENOMEM;
	}

	return init_wd_object(p);
}

static void plugin_wdg_deinit(struct plugin_wdg *p)
{
	deinit_wd_object(p);

	if (p->obs) {
		kfree(p->obs);
		p->obs = NULL;
	}
	if (p->cfg) {
		kfree(p->cfg);
		p->cfg = NULL;
	}
}

static struct plugin_wdg mxt_plugin_wdg_if = {
	.init = plugin_wdg_init,
	.deinit = plugin_wdg_deinit,
	.start = plugin_wdg_start,
	.stop = plugin_wdg_stop,
	.hook_t6 = plugin_wdg_hook_t6,
	.hook_t61 = plugin_wdg_hook_t61,
	.hook_reg_access = plugin_wdg_hook_reg_access,
	.pre_process = plugin_wdg_pre_process_messages,
	.post_process = plugin_wdg_post_process_messages,
	.show = plugin_wd_debug_show,
	.store = plugin_wd_debug_store,
};

void plugin_interface_wdg_init(struct plugin_wdg *p)
{
	memcpy(p, &mxt_plugin_wdg_if, sizeof(struct plugin_wdg));
}

