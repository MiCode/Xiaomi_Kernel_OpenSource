/*
 * Atmel maXTouch Touchscreen driver Plug in
 *
 * Copyright (C) 2013 Atmel Co.Ltd
 * Author: Pitter Liao <pitter.liao@atmel.com>
 * Copyright (C) 2015 XiaoMi, Inc.
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
#define PLUG_AC_T72_VERSION 0x0025
/*----------------------------------------------------------------
0.23
1 add dualx message support
0.222
1 fixed bug in hook t72
0.221
1 testing hook t72
0.22
1 change t72 control t9/t8/t55 only after t37 workaround is end
2 t72 report state to global when noise change
3 delete t61 timer control
0.21
1 add t9/t100, t55 set
0.2
1 process t72 noise message to set t8 in noise contion
0.11
1 version for simple workaround without debug message
0.1
1 first version of t72 plugin
*/

#include "plug.h"

#define T72_FLAG_RESUME			(1<<0)
#define T72_FLAG_RESET			(1<<1)
#define T72_FLAG_CAL			(1<<2)

#define T72_FLAG_START_T61		(1<<4)

#define T72_FLAG_STOP_T8		(1<<8)

#define T72_FLAG_STABLE			(1<<16)
#define T72_FLAG_NOISE			(1<<17)
#define T72_FLAG_VERY_NOISE		(1<<18)
#define T72_FLAG_STATE_CHANGE		(1<<19)

#define IRAD_FLAG_PROXIMITY_DETECTED	(1<<20)
#define IRAD_FLAG_PROXIMITY_REMOVED	(1<<21)
#define IRAD_FLAG_STATE_HANDLED		(1<<23)

#define T72_FLAG_WORKAROUND_HALT	(1<<31)

#define T72_FLAG_MASK_LOW		(0x00000f)
#define T72_FLAG_MASK_NORMAL		(0x000ff0)
#define T72_NOISE_MASK			(0x0f0000)
#define T72_PROXIMITY_MASK		(0xf00000)
#define T72_FLAG_MASK			(-1)

struct t72_observer{
	unsigned long flag;

	unsigned long time_next_st;
};

struct t72_config{
	unsigned long interval_fast;
	unsigned long interval_proximity_check;
};

static void proximity_enable(bool en){
	//enable your proximity here
}

static int get_proximity_data(void){
	//get proximity data:  0 proximity -EINVAL not
	return -EINVAL;
}

long status_interval(struct plugin_ac *p, unsigned long flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t72_config *cfg = p->cfg;
	unsigned long interval;

	if (test_flag(IRAD_FLAG_PROXIMITY_DETECTED, &flag))
		interval = cfg->interval_proximity_check;
	else
		interval = MAX_SCHEDULE_TIMEOUT;

	dev_dbg2(dev, "mxt interval %lu\n",interval);
	return (long)interval;
}

static void plugin_cal_t72_hook_t6(struct plugin_ac *p, u8 status)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t72_observer *obs = p->obs;

	if (status & (MXT_T6_STATUS_RESET|MXT_T6_STATUS_CAL)) {
		dev_info2(dev, "T72 hook T6 0x%x\n", status);

		if (status & MXT_T6_STATUS_CAL) {
			set_and_clr_flag(T72_FLAG_CAL,
					T72_FLAG_MASK_NORMAL,&obs->flag);
		}
		if (status & MXT_T6_STATUS_RESET) {
			set_and_clr_flag(T72_FLAG_RESET,
					T72_FLAG_MASK,&obs->flag);
		}
	} else {
		if (test_flag(T72_FLAG_CAL|T72_FLAG_RESET,&obs->flag)) {
			dev_info2(dev, "T72 hook T6 end\n");
			set_and_clr_flag(T72_FLAG_START_T61, 0, &obs->flag);
		}
	}

	dev_info2(dev, "mxt t72 flag=0x%lx\n",
			obs->flag);
}

static void plugin_cal_t72_hook_t61(struct plugin_ac *p, int id, u8 status)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t72_observer *obs = p->obs;

	dev_info(dev, "T61 timer %d status 0x%x\n", id, status);

	if (id == T72_TIME_ID) {
		if (status & MXT_T61_ELAPSED) {
			set_flag(T72_FLAG_STOP_T8, &obs->flag);
		}
	}
}

static void plugin_ac_hook_t72(struct plugin_ac *p, u8 *msg)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;

	struct t72_observer *obs = (struct t72_observer *)p->obs;
	int state,dualx;

	state = msg[2] & T72_NOISE_STATE_MASK;
	dualx = msg[2] & T72_NOISE_DUALX_MASK;

	dev_info2(dev, "mxt hook t72 %d(%d,%d,%d)\n",
			state,NOISE_STABLE,NOISE_NOISY,NOISE_VERY_NOISY);

	if (state == NOISE_STABLE) {
		if (!test_flag(T72_FLAG_STABLE, &obs->flag)) {
			set_and_clr_flag(T72_FLAG_STABLE | T72_FLAG_STATE_CHANGE, T72_NOISE_MASK, &obs->flag);
			p->set_and_clr_flag(p->dev, 0, PL_STATUS_FLAG_NOISE_MASK);
		}
	} else if (state == NOISE_NOISY) {
		if (!test_flag(T72_FLAG_NOISE, &obs->flag)) {
			set_and_clr_flag(T72_FLAG_NOISE | T72_FLAG_STATE_CHANGE, T72_NOISE_MASK, &obs->flag);
			p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_NOISE, PL_STATUS_FLAG_NOISE_MASK);
		}
	} else if (state == NOISE_VERY_NOISY) {
		if (!test_flag(T72_FLAG_VERY_NOISE | T72_FLAG_STATE_CHANGE, &obs->flag)) {
			set_and_clr_flag(T72_FLAG_VERY_NOISE, T72_NOISE_MASK, &obs->flag);
			p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_VERY_NOISE, PL_STATUS_FLAG_NOISE_MASK);
		}
	} else {
		dev_info(dev, "mxt hook t72 unknow status %d\n",state);
	}

	if (dualx) {
		dev_info(dev, "mxt hook t72 dualx %d\n",dualx);
		p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_DUALX, 0);
	} else {
		p->set_and_clr_flag(p->dev, 0, PL_STATUS_FLAG_DUALX);
	}
}

static int mxt_proc_noise_msg(struct plugin_ac *p,unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t72_observer *obs = p->obs;

	dev_info2(dev, "mxt t72 at mxt_proc_noise_msg flag 0x%lx pl_flag 0x%lx\n",obs->flag,pl_flag);

	//very noise
	if (test_flag(PL_STATUS_FLAG_CAL_END,&pl_flag)) {
		if (test_flag(T72_FLAG_VERY_NOISE,&obs->flag)) {
			dev_info(dev, "mxt t72 enter very noise state\n");
			p->set_t9_t100_cfg(p->dev, T9_T100_THLD_VERY_NOISE, BIT_MASK(MXT_T9_MRGTHR)|BIT_MASK(MXT_T9_MRGHYST));
			p->set_t55_adp_thld(p->dev, T55_DISABLE);
		//noise
		}else if (test_flag(T72_FLAG_NOISE,&obs->flag)) {
			dev_info(dev, "mxt t72 enter noise state\n");
			p->set_t9_t100_cfg(p->dev, T9_T100_THLD_NOISE, BIT_MASK(MXT_T9_MRGTHR)|BIT_MASK(MXT_T9_MRGHYST));
			p->set_t55_adp_thld(p->dev, T55_DISABLE);
		//stable
		}else{
			dev_info(dev, "mxt t72 enter very stable state\n");
			p->set_t9_t100_cfg(p->dev, T9_T100_NORMAL, BIT_MASK(MXT_T9_MRGTHR)|BIT_MASK(MXT_T9_MRGHYST));
			p->set_t55_adp_thld(p->dev, T55_NORMAL);
		}
	}

	clear_flag(T72_FLAG_STATE_CHANGE, &obs->flag);
	p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_NOISE_CHANGE, 0);
	return 0;
}

static int mxt_proc_proximity_msg(struct plugin_ac *p,unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t72_observer *obs = p->obs;
	int ret;

	dev_info2(dev, "mxt t72 at mxt_proc_proximity_msg flag 0x%lx pl_flag 0x%lx\n",obs->flag,pl_flag);

	//proximity

	if (test_flag(IRAD_FLAG_PROXIMITY_DETECTED, &obs->flag)) {
		proximity_enable(true);
		ret = get_proximity_data();
		if (ret != 0) {
			dev_info(dev, "mxt t72 proximity removed\n");
			clear_flag(IRAD_FLAG_PROXIMITY_DETECTED, &obs->flag);
			p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_PROXIMITY_REMOVED, 0);
		}
		proximity_enable(false);
	}else if (test_flag(/*T72_FLAG_CAL*/T72_FLAG_RESUME, &obs->flag)) {//only check when proximity not detected
		proximity_enable(true);
		ret = get_proximity_data();
		if (ret == 0) {
			dev_info(dev, "mxt t72 proximity detected\n");
			set_flag(IRAD_FLAG_PROXIMITY_DETECTED, &obs->flag);
		}else {
			dev_info(dev, "mxt t72 proximity not detected\n");
			clear_flag(IRAD_FLAG_PROXIMITY_DETECTED, &obs->flag);
		}
		proximity_enable(false);
	}

	return 0;
}

static int mxt_proc_timer_msg(struct plugin_ac *p,unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t72_observer *obs = p->obs;

	dev_info2(dev, "mxt t72 at mxt_proc_timer_msg flag 0x%lx pl_flag 0x%lx\n",obs->flag,pl_flag);

	clear_flag(T72_FLAG_START_T61, &obs->flag);

	return 0;
}

static int mxt_proc_hw_msg(struct plugin_ac *p,unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t72_observer *obs = p->obs;

	dev_info2(dev, "mxt t72 at mxt_proc_hw_msg flag 0x%lx pl_flag 0x%lx\n",obs->flag,pl_flag);

	clear_flag(T72_FLAG_MASK_LOW, &obs->flag);

	obs->time_next_st = jiffies + 1;

	return 0;
}

static void plugin_cal_t72_start(struct plugin_ac *p, bool resume)
{
	struct t72_observer *obs = p->obs;

	clear_flag(T72_FLAG_WORKAROUND_HALT, &obs->flag);

	if (resume)
		set_flag(T72_FLAG_RESUME, &obs->flag);
}

static void plugin_cal_t72_stop(struct plugin_ac *p)
{
	struct t72_observer *obs = p->obs;

	set_and_clr_flag(T72_FLAG_WORKAROUND_HALT,T72_FLAG_RESUME, &obs->flag);
}

static long plugin_ac_t72_post_process_messages(struct plugin_ac *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t72_config *cfg = p->cfg;
	struct t72_observer *obs = p->obs;
	long next_interval = MAX_SCHEDULE_TIMEOUT;
	int ret = 0;

	if (test_flag(T72_FLAG_WORKAROUND_HALT,&obs->flag))
		return MAX_SCHEDULE_TIMEOUT;

	if (test_flag(T72_FLAG_STATE_CHANGE, &obs->flag)) {
		mxt_proc_noise_msg(p, pl_flag);
	}

	if (test_flag(/*T72_FLAG_CAL*/T72_FLAG_RESUME, &obs->flag)) {
		mxt_proc_proximity_msg(p, pl_flag);
	}

	if (test_flag(T72_FLAG_START_T61, &obs->flag)) {
		mxt_proc_timer_msg(p, pl_flag);
	}

	if (test_flag(T72_FLAG_MASK_LOW, &obs->flag)) {
		mxt_proc_hw_msg(p, pl_flag);
	}

	if (time_after_eq(jiffies,obs->time_next_st)) {
		if (test_flag(IRAD_FLAG_PROXIMITY_DETECTED, &obs->flag)) {
		mxt_proc_proximity_msg(p, pl_flag);
		}
	}else {
		dev_dbg2(dev, "mxt wait %lu\n", obs->time_next_st - jiffies);
		ret = -ETIME;
	}

	if (ret == -ETIME)
		next_interval = obs->time_next_st - jiffies + 1;
	if (ret == -EAGAIN)
		next_interval = cfg->interval_fast;
	else {
		next_interval = status_interval(p,obs->flag);
	}

	if (ret != -ETIME) {
		obs->time_next_st = jiffies;
		if (next_interval != MAX_SCHEDULE_TIMEOUT)
			obs->time_next_st = jiffies + next_interval;
		else
			obs->time_next_st = next_interval;
	}

	if (!next_interval)
		next_interval = MAX_SCHEDULE_TIMEOUT; //just wait interrupt if zero

	dev_dbg2(dev, "t72 mxt interval %lu wait %ld\n",next_interval,obs->time_next_st - jiffies);
	return next_interval;
}

static int plugin_ac_t72_show(struct plugin_ac *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t72_config * cfg = p->cfg;
	struct t72_observer * obs = p->obs;

	dev_info(dev, "[mxt]PLUG_AC_T72_VERSION: 0x%x\n",PLUG_AC_T72_VERSION);

	if (!p->init)
		return 0;

	dev_info(dev, "[mxt]T72 cfg :\n");
	dev_info(dev, "[mxt]interval: Fast=%lu, Prox=%lu\n",
		cfg->interval_fast,
		cfg->interval_proximity_check);
	dev_info(dev, "[mxt]\n");

	dev_info(dev, "[mxt]T72 obs :\n");
	dev_info(dev, "[mxt]status: Flag=0x%08lx\n",
		obs->flag);
	dev_info(dev, "[mxt]time: Tnext=%lu\n",
		obs->time_next_st);
	dev_info(dev, "[mxt]\n");

	return 0;
}

static int plugin_ac_t72_store(struct plugin_ac *p, const char *buf, size_t count)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct t72_config * cfg = p->cfg;
	struct t72_observer * obs = p->obs;

	dev_info(dev, "[mxt]t72 store:%s\n",buf);

	if (!p->init)
		return 0;

	if (sscanf(buf, "status: Flag=0x%lx\n",
		&obs->flag) > 0) {
		dev_info(dev, "[mxt] OK\n");
	} else if (sscanf(buf, "interval: Fast=%lu, Prox=%lu\n",
		&cfg->interval_fast,
		&cfg->interval_proximity_check) > 0) {
		dev_info(dev, "[mxt] OK\n");
	} else {
		dev_info(dev, "[mxt] BAD\n");
	}

	return 0;
}

static int init_t72_object(struct plugin_ac *p)
{
	struct t72_config *cfg = p->cfg;
	struct t72_observer * obs = p->obs;

	cfg->interval_fast = 1;
	cfg->interval_proximity_check = HZ / 10;

	obs->time_next_st = jiffies;

	return 0;
}

static int deinit_t72_object(struct plugin_ac *p)
{
	return 0;
}

static int plugin_ac_t72_init(struct plugin_ac *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;

	dev_info(dev, "%s: plugin ac t72 version 0x%x\n",
		__func__,PLUG_AC_T72_VERSION);

	p->obs = kzalloc(sizeof(struct t72_observer), GFP_KERNEL);
	if (!p->obs) {
		dev_err(dev, "Failed to allocate memory for t72 observer\n");
		return -ENOMEM;
	}

	p->cfg = kzalloc(sizeof(struct t72_config), GFP_KERNEL);
	if (!p->cfg) {
		dev_err(dev, "Failed to allocate memory for t72 cfg\n");
		kfree(p->obs);
		return -ENOMEM;
	}

	if (init_t72_object(p) != 0) {
		dev_err(dev, "Failed to allocate memory for t72 cfg\n");
		kfree(p->obs);
		kfree(p->cfg);
	}

	return 0;
}

static void plugin_ac_t72_deinit(struct plugin_ac *p)
{
	if (p->obs) {
		deinit_t72_object(p);
		kfree(p->obs);
	}
	if (p->cfg)
		kfree(p->cfg);
}

struct plugin_ac mxt_plugin_ac_t72 =
{
	.init = plugin_ac_t72_init,
	.deinit = plugin_ac_t72_deinit,
	.start = plugin_cal_t72_start,
	.stop = plugin_cal_t72_stop,
	.hook_t6 = plugin_cal_t72_hook_t6,
	.hook_t61 = plugin_cal_t72_hook_t61,
	.hook_t72 = plugin_ac_hook_t72,
	.post_process = plugin_ac_t72_post_process_messages,
	.show = plugin_ac_t72_show,
	.store = plugin_ac_t72_store,
};

int plugin_ac_init(struct plugin_ac *p)
{
	memcpy(p, &mxt_plugin_ac_t72, sizeof(struct plugin_ac));
	return 0;
}

