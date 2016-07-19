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
#define PLUG_PROCI_VERSION 0x0007
/*----------------------------------------------------------------
0.7
1 add error handle in wakeup/process

0.6
1 add request lock and error reset
0.5
1 change T92/T93 report algorithm
2 change set and restore order
0.4
1 drift 20  /  allow mutiltouch /
0.3
1 add oppi direction in restore
0.2
fixed some bugs
0.1
1 first version of pi plugin
*/

#include "plug.h"
#include <linux/delay.h>

#define PI_FLAG_RESUME					(1<<0)
#define PI_FLAG_RESET					(1<<1)
#define PI_FLAG_CAL						(1<<2)

#define PI_FLAG_GLOVE				   (1<<12)
#define PI_FLAG_STYLUS				  (1<<13)
#define PI_FLAG_WAKEUP				  (1<<14)



#define PI_FLAG_WORKAROUND_HALT			(1<<31)

#define PI_FLAG_MASK_LOW			(0x000f)
#define PI_FLAG_MASK_NORMAL			(0xfff0)
#define PI_FLAG_MASK				(-1)

#define MAKEWORD(a, b)  ((unsigned short)(((unsigned char)(a)) \
	| ((unsigned short)((unsigned char)(b))) << 8))

enum{
	PI_GLOVE = 0,
	PI_STYLUS,
	PI_DWAKE,
	PI_LIST_NUM,
};



enum{
	P_COMMON = 0,
	P_AREA,

	DWK_DCLICK,
	DWK_GESTURE,
	GL_ENABLE,
	STY_ENABLE,

	DIR_OPPISTE,

	OP_SET,
	OP_CLR
};


static char *pi_cfg_name[PI_LIST_NUM] = {
	"GLOVE",
	"STYLUS",
	"DWAKE",
};

struct pi_observer{
	unsigned long flag;
	struct reg_config *set[PI_LIST_NUM];
	struct reg_config *stack[PI_LIST_NUM];


	struct mutex access_mutex;
	void *mem;
};

struct pi_config{
	struct reg_config *reg_cfg[PI_LIST_NUM];
	int num_reg_cfg[PI_LIST_NUM];
};

static void plugin_proci_pi_hook_t6(struct plugin_proci *p, u8 status)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;

	if (status & (MXT_T6_STATUS_RESET|MXT_T6_STATUS_CAL)) {
		dev_dbg2(dev, "PI hook T6 0x%x\n", status);

		if (status & MXT_T6_STATUS_CAL) {
			set_and_clr_flag(PI_FLAG_CAL,
				0, &obs->flag);
		}

		if (status & MXT_T6_STATUS_RESET) {
			set_and_clr_flag(PI_FLAG_RESET,
				PI_FLAG_MASK_NORMAL, &obs->flag);
		}
	} else {
		if (test_flag(PI_FLAG_CAL|PI_FLAG_RESET, &obs->flag))
			dev_dbg2(dev, "PI hook T6 end\n");
	}

	dev_info2(dev, "mxt pi flag=0x%lx\n",
		 obs->flag);
}

static int plugin_proci_pi_hook_t24(struct plugin_proci *p, u8 *msg, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;


	int state = msg[1] & 0xF;
	int idx = -EINVAL;

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP_DCLICK, &pl_flag))
		return -EINVAL;

	dev_info2(dev, "mxt hook pi t24 0x%x\n",
		state);

	if (state == 0x4)
		idx = 0;

	if (idx >= 0) {
#if defined(CONFIG_MXT_WAKEUP_KEY_T24_INDEX)
		idx  += CONFIG_MXT_WAKEUP_KEY_T24_INDEX;
#endif
		dev_info(dev, "T24 key index %d\n", idx);
	}
	return idx;
}

static int plugin_proci_pi_hook_t61(struct plugin_proci *p, int id, u8 status, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	int idx = -EINVAL;

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP, &pl_flag))
		return -EINVAL;

	dev_info(dev, "T61 timer %d status 0x%x %d\n", id, status, (status & MXT_T61_ELAPSED));

	if (id >= 0) {
		if (status & MXT_T61_ELAPSED) {
			idx = CONFIG_MXT_WAKEUP_KEY_T61_INDEX + id;

			dev_info(dev, "T92 key index %d\n", idx);
		}
	}

	return idx;
}

static int plugin_proci_pi_hook_t81(struct plugin_proci *p, u8 *msg, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	int idx = -EINVAL;
	int state = msg[1];

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP, &pl_flag))
		return -EINVAL;

	dev_info2(dev, "mxt hook pi t81 0x%x\n",
		state);

	state &= 0x1;
	if (state) {
		idx = CONFIG_MXT_WAKEUP_KEY_T81_INDEX;
		dev_info(dev, "T81 key index %d range %u %u\n", idx,
			MAKEWORD(msg[2], msg[1]), MAKEWORD(msg[4], msg[3]));
	}
	return idx;
}


static int plugin_proci_pi_hook_t92(struct plugin_proci *p, u8 *msg, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	int idx = -EINVAL;

	int state = msg[1];

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP_GESTURE, &pl_flag))
		return -EINVAL;

	dev_info2(dev, "mxt hook pi t92 0x%x\n",
		state);

	state &= 0x7F;
	if (state >= dcfg->t92.rptcode) {
		idx = state - dcfg->t92.rptcode;

		if (state & 0x80) {
#if defined(CONFIG_MXT_WAKEUP_KEY_T92_STROKE_INDEX)
			idx  += CONFIG_MXT_WAKEUP_KEY_T92_STROKE_INDEX;
#endif
		} else {
#if defined(CONFIG_MXT_WAKEUP_KEY_T92_SYMBOL_INDEX)
			idx  += CONFIG_MXT_WAKEUP_KEY_T92_SYMBOL_INDEX;
#endif
		}

		dev_info(dev, "T92 key index %d\n", idx);
	}

	return idx;
}

static int plugin_proci_pi_hook_t93(struct plugin_proci *p, u8 *msg, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;


	int state = msg[1] & 0x83;
	int idx = -EINVAL;

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP_DCLICK, &pl_flag))
		return -EINVAL;

	dev_info2(dev, "mxt hook pi t93 0x%x\n",
		state);

	if (state & 0x2/*0x1*/)
		idx = 0;

	if (idx >= 0) {
#if defined(CONFIG_MXT_WAKEUP_KEY_T93_INDEX)
		idx  += CONFIG_MXT_WAKEUP_KEY_T93_INDEX;
#endif
		dev_info(dev, "T93 key index %d\n", idx);
	}
	return idx;
}

static unsigned long get_pi_flag(int pi, bool enable, unsigned long pl_flag)
{
	unsigned long flag = 0;

	if (pi == PI_GLOVE)
		flag = BIT_MASK(GL_ENABLE);
	else if (pi == PI_STYLUS)
		flag = BIT_MASK(STY_ENABLE);
	else if (pi == PI_DWAKE) {
		if (test_flag(PL_FUNCTION_FLAG_WAKEUP_DCLICK, &pl_flag))
			flag |= BIT_MASK(DWK_DCLICK);
		if (test_flag(PL_FUNCTION_FLAG_WAKEUP_GESTURE, &pl_flag))
			flag |= BIT_MASK(DWK_GESTURE);
	}

	if (flag) {
		if (enable)
			flag |= BIT_MASK(OP_SET);
		else
			flag |= BIT_MASK(OP_CLR);

		flag |= BIT_MASK(P_COMMON) | BIT_MASK(P_AREA);
	}
	return flag;
}

static int pi_handle_request(struct plugin_proci *p, const struct reg_config *rcfg, int num_reg_cfg, struct reg_config *rset, struct reg_config *save, unsigned long flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = (struct pi_observer *)p->obs;

	int i, reg;
	u8 *stack_buf;
	int ret;

	for (i = 0; i < num_reg_cfg; i++) {

		if (test_flag(PI_FLAG_RESET, &obs->flag))
			break;

		if (test_flag(BIT_MASK(DIR_OPPISTE), &flag))
			reg = num_reg_cfg - i - 1;
		else
			reg = i;

		memcpy(&rset[reg], &rcfg[reg], sizeof(struct reg_config));
		if (save) {
			memcpy(&save[reg], &rcfg[reg], sizeof(struct reg_config));
			stack_buf = save[reg].buf;
		} else
			stack_buf = NULL;

		ret = p->set_obj_cfg(p->dev, &rset[reg], stack_buf, flag);
		if (ret == -EIO) {
			dev_err(dev, "pi request write reg %d off %d len %d failed\n",
				rset[reg].reg, rset[reg].offset, rset[reg].len);
			return ret;
		}
		if (rset[reg].sleep)
			msleep(rset[reg].sleep);
	}

	return 0;
}

static int plugin_proci_pi_handle_request(struct plugin_proci *p, int pi, bool enable, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;
	struct pi_config *cfg = p->cfg;
	const struct reg_config *rcfg;
	struct reg_config *rstack, *rset;
	unsigned long flag;
	int ret;

	if (pi >= PI_LIST_NUM) {
		dev_err(dev, "Invalid pi request %d \n", pi);
		return -EINVAL;
	}

	mutex_lock(&obs->access_mutex);

	flag = get_pi_flag(pi, enable, pl_flag);

	if (enable) {
		rcfg = cfg->reg_cfg[pi];
		rset = obs->set[pi];
		rstack = obs->stack[pi];

		ret = pi_handle_request(p, rcfg, cfg->num_reg_cfg[pi], rset, rstack, flag);
		if (ret) {
			dev_err(dev, "pi handle request write reg %d offset %d len %d failed\n",
				rset->reg, rset->offset, rset->len);
			mutex_unlock(&obs->access_mutex);
			return ret;
		}
	} else {

		rset = obs->set[pi];
		rstack = obs->stack[pi];
		flag |= BIT_MASK(DIR_OPPISTE);

		ret = pi_handle_request(p, rstack, cfg->num_reg_cfg[pi], rset, /*rstack*/NULL, flag);
		if (ret) {
			dev_err(dev, "pi handle request write reg %d offset %d len %d failed\n",
				rset->reg, rset->offset, rset->len);
			mutex_unlock(&obs->access_mutex);
			return ret;
		}
	}

	dev_info(dev, "pi handle request result %d\n",
		ret);

	mutex_unlock(&obs->access_mutex);
	return ret;
}

static int plugin_proci_pi_wakeup_enable(struct plugin_proci *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;
	struct reg_config t7_power = {
		.reg = MXT_GEN_POWERCONFIG_T7,
		.offset = 0,
		.buf = {0x0},
		.len = 2,
		.mask = 0,
		.flag = BIT_MASK(P_COMMON)
	};

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP, &pl_flag))
		return 0;

	if (test_flag(PI_FLAG_WORKAROUND_HALT, &obs->flag))
		return 0;

	if (test_flag(PI_FLAG_WAKEUP, &obs->flag)) {
		dev_err(dev, "set wakeup enable loop, set reset\n");
		if (p->reset(p->dev) == 0)
			clear_flag(PI_FLAG_WAKEUP, &obs->flag);
		else
			p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_NEED_RESET, 0);
	}
	if (!test_flag(PI_FLAG_WAKEUP, &obs->flag)) {
		if (plugin_proci_pi_handle_request(p, PI_DWAKE, true, pl_flag) == 0) {
			set_flag(PI_FLAG_WAKEUP, &obs->flag);
		return -EBUSY;
		} else {
			dev_err(dev, "set wakeup enable failed, set reset\n");
			if (p->reset(p->dev) != 0)
				p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_NEED_RESET, 0);
			p->set_obj_cfg(p->dev, &t7_power, NULL, BIT_MASK(P_COMMON));
		}
	}

	return 0;
}

static int plugin_proci_pi_wakeup_disable(struct plugin_proci *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;
	unsigned long old_flag = obs->flag;

	if (!test_flag(PL_FUNCTION_FLAG_WAKEUP, &pl_flag))
		return 0;

	if (test_flag(PI_FLAG_WORKAROUND_HALT, &obs->flag))
		return 0;

	if (test_flag(PI_FLAG_WAKEUP, &obs->flag)) {
		if (plugin_proci_pi_handle_request(p, PI_DWAKE, false, pl_flag) == 0) {
			clear_flag(PI_FLAG_WAKEUP, &obs->flag);
		}
	}

	if (test_flag(PI_FLAG_WAKEUP, &obs->flag)) {
		dev_err(dev, "set wakeup disable failed, set reset\n");
		if (p->reset(p->dev) == 0)
			clear_flag(PI_FLAG_WAKEUP, &obs->flag);
		else
			p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_NEED_RESET, 0);
	}
	if (test_flag(PI_FLAG_WAKEUP, &old_flag))
		return -EBUSY;

	return 0;
}

struct pi_descriptor{
	unsigned long flag_pl;
	unsigned long flag_pi;
	int pi;
};
#if 0
int wake_switch;
int gesture_switch;
#endif
static void plugin_proci_pi_pre_process_messages(struct plugin_proci *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;

	if (test_flag(PI_FLAG_WORKAROUND_HALT, &obs->flag))
		return;

	dev_dbg2(dev, "mxt plugin_proci_pi_pre_process_messages pl_flag=0x%lx flag=0x%lx\n",
		 pl_flag, obs->flag);


#if 0
	if (wake_switch)
		p->set_and_clr_flag(p->dev, PL_FUNCTION_FLAG_WAKEUP_DCLICK, 0);
	else
		p->set_and_clr_flag(p->dev, 0, PL_FUNCTION_FLAG_WAKEUP_DCLICK);
	if (gesture_switch)
		p->set_and_clr_flag(p->dev, PL_FUNCTION_FLAG_WAKEUP_GESTURE, 0);
	else

		p->set_and_clr_flag(p->dev, 0, PL_FUNCTION_FLAG_WAKEUP_GESTURE);
#endif


}

static long plugin_proci_pi_post_process_messages(struct plugin_proci *p, unsigned long pl_flag)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;
	bool enable;
	int i;
	int ret;

	struct pi_descriptor dpr[] = {
		{PL_FUNCTION_FLAG_GLOVE, PI_FLAG_GLOVE, PI_GLOVE},
		{PL_FUNCTION_FLAG_STYLUS, PI_FLAG_STYLUS, PI_STYLUS} };

	if (test_flag(PI_FLAG_WORKAROUND_HALT, &obs->flag))
		return 0;

	dev_dbg2(dev, "mxt pi pl_flag=0x%lx flag=0x%lx\n",
		 pl_flag, obs->flag);

	for (i = 0; i < ARRAY_SIZE(dpr); i++) {
		if (test_flag(dpr[i].flag_pl, &pl_flag) != test_flag(dpr[i].flag_pi, &obs->flag)) {
			if (test_flag(PI_FLAG_RESET, &obs->flag))
				break;

			enable = test_flag(dpr[i].flag_pl, &pl_flag);
			ret = plugin_proci_pi_handle_request(p, dpr[i].pi, enable, pl_flag);
			if (ret) {
				dev_err(dev, "set pi %d enable %d failed %d\n", dpr[i].pi, enable, ret);
				if (p->reset(p->dev) == 0)
					clear_flag(dpr[i].flag_pi, &obs->flag);
				else
					p->set_and_clr_flag(p->dev, PL_STATUS_FLAG_NEED_RESET, 0);
			} else {
				if (enable)
					set_flag(dpr[i].flag_pi, &obs->flag);
				else
					clear_flag(dpr[i].flag_pi, &obs->flag);
			}
		}
	}

	clear_flag(PI_FLAG_MASK_LOW, &obs->flag);

	return MAX_SCHEDULE_TIMEOUT;
}

static struct reg_config mxt_glove_cfg[] = {

	{.reg = MXT_PROCI_GLOVEDETECTION_T78,
		.offset = 0, .buf = {0x1}, .len = 1, .mask = 0x1, .flag = BIT_MASK(OP_SET)},


	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},

	{MXT_TOUCH_MULTITOUCHSCREEN_T100,
		9, {16}, 1, 0, BIT_MASK(OP_SET)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,
		13, {0x40, 0x02}, 2, 0, BIT_MASK(OP_SET)},

	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
};

static struct reg_config mxt_stylus_cfg[] = {

	{MXT_PROCI_STYLUS_T47,
		0, {0x1}, 1, 0x1, BIT_MASK(STY_ENABLE)},


	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
};

static struct reg_config mxt_dwakeup_cfg[] = {


	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},

#if defined(CONFIG_CHIP_540S)
	{MXT_GEN_ACQUISITIONCONFIG_T8,
		2, {0x05, 0x05, 0x0, 0x0, 0x0a, 0x19, 0x00, 0x00, 0x01, 0x01, 0x01, 0, 0x1}, 13, 0, BIT_MASK(P_COMMON)},

	{MXT_PROCI_TOUCHSUPPRESSION_T42,
		0, {0x41}, 1, 0x41, BIT_MASK(OP_SET)},

	{MXT_SPT_CTECONFIG_T46,
		2, {8, 20}, 2, 0, BIT_MASK(P_COMMON)},
	{MXT_PROCI_SHIELDLESS_T56,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_PROCI_LENSBENDING_T65,
		0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},

	{MXT_PROCG_NOISESUPPRESSION_T72,
		0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},



	{MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80,
		0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},

	{MXT_TOUCH_MULTITOUCHSCREEN_T100,
		0, {0}, 1, 0x2, BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,
		6, {1}, 1, 0, BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,
		30, {70, 20, 40}, 3, 0, BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,
		39, {3, 1, 1}, 3, 0, BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,
		43, {20}, 1, 0, BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,
		53, {10}, 1, 0, BIT_MASK(P_COMMON)},

	{MXT_PROCI_AUXTOUCHCONFIG_T104,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},

	{MXT_PROCI_UNLOCKGESTURE_T81,
		0, {0x1}, 1, 0x1, BIT_MASK(DWK_GESTURE)},

	{MXT_PROCI_GESTURE_T92,
		0, {0x1}, 1, 0x1, BIT_MASK(DWK_GESTURE)},

	{MXT_PROCI_TOUCHSEQUENCELOGGER_T93,
		0, {0x1}, 1, 0x1, BIT_MASK(DWK_DCLICK)},

	{MXT_GEN_POWERCONFIG_T7,
		0, {0x3c, 0x0f, 0x04, 0x40, 0x00}, 5, 0, BIT_MASK(OP_SET)},

#elif defined(CONFIG_CHIP_336T_640T)

	{MXT_SPT_CTECONFIG_T46,
		 2, {8, 20}, 2, 0, BIT_MASK(P_COMMON)},

	{MXT_PROCI_UNLOCKGESTURE_T81,
		0, {0x1}, 1, 0x1, BIT_MASK(DWK_GESTURE)},

	{MXT_PROCI_GESTURE_T92,
		 0, {0x1}, 1, 0x1, BIT_MASK(DWK_GESTURE)},

	 {MXT_PROCI_TOUCHSEQUENCELOGGER_T93,
		 0, {0x1}, 1, 0x1, BIT_MASK(DWK_DCLICK)},


	 {MXT_PROCI_STYLUS_T47,
		 0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},

	 {MXT_PROCG_NOISESUPPRESSION_T72,
		 0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},

	 {MXT_PROCI_GLOVEDETECTION_T78,
		 0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},


	 {MXT_TOUCH_MULTITOUCHSCREEN_T100,
		 0, {0}, 1, 0x2, BIT_MASK(P_COMMON)},

	 {MXT_PROCG_NOISESUPSELFCAP_T108,
		 0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},


	 {MXT_GEN_POWERCONFIG_T7,
		 0, {0x64, 0x0f}, 2, 0, BIT_MASK(OP_SET)},

#else
	{MXT_GEN_ACQUISITIONCONFIG_T8,
		2, {0x05, 0x05, 0x0, 0x0, 0x0a, 0x19, 0x00, 0x00, 0x01, 0x01, 0x01, 0, 0x1}, /*13*/11, 0, BIT_MASK(P_COMMON)},

	{MXT_PROCI_GLOVEDETECTION_T78,
		0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},

	{MXT_PROCI_STYLUS_T47,
		0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},

	{MXT_PROCI_TOUCHSUPPRESSION_T42,
		0, {1}, 1, 0x1, BIT_MASK(P_COMMON)},

	{MXT_SPT_CTECONFIG_T46,
		2, {8, 20}, 2, 0, BIT_MASK(P_COMMON)},

#if defined(CONFIG_DISABLE_ALL_OBJECT)
	{MXT_PROCI_SHIELDLESS_T56,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
#endif

	{MXT_PROCI_LENSBENDING_T65,
		0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},

	{MXT_PROCG_NOISESUPPRESSION_T72,
		0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},
	{MXT_PROCI_RETRANSMISSIONCOMPENSATION_T80,
		0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},

	{MXT_TOUCH_MULTITOUCHSCREEN_T100,
		0, {0}, 1, 0x2, BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,
		6, {1}, 1, 0, BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,
		30, {85, 20, 40}, 3, 0, BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,
		39, {3, 1, 1}, 3, 0, BIT_MASK(P_COMMON)},
	{MXT_TOUCH_MULTITOUCHSCREEN_T100,
		43, {20}, 1, 0, BIT_MASK(P_COMMON)},

#if defined(CONFIG_DISABLE_ALL_OBJECT)
	{MXT_SPT_TOUCHSCREENHOVER_T101,
		0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},

	{MXT_PROCI_AUXTOUCHCONFIG_T104,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},

	{MXT_PROCG_NOISESUPSELFCAP_T108,
		0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},

	{MXT_SPT_SELFCAPGLOBALCONFIG_T109,
		0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},

	{MXT_SPT_SELFCAPTUNINGPARAMS_T110,
		0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},

	{MXT_SPT_SELFCAPCONFIG_T111,
		0, {0}, 1, 0x1, BIT_MASK(P_COMMON)},

	{MXT_SPT_TIMER_T61,
		1, {0}, 1, 0x1, BIT_MASK(DWK_DCLICK)},
#endif

	{MXT_PROCI_ONETOUCHGESTUREPROCESSOR_T24,
		0, {0x1}, 1, 0x1, BIT_MASK(DWK_DCLICK)},

	{MXT_PROCI_UNLOCKGESTURE_T81,
		0, {0x1}, 1, 0x1, BIT_MASK(DWK_GESTURE)},

	{MXT_PROCI_GESTURE_T92,
		0, {0x1}, 1, 0x1, BIT_MASK(DWK_GESTURE)},

	{MXT_PROCI_TOUCHSEQUENCELOGGER_T93,
		0, {0x1}, 1, 0x1, BIT_MASK(DWK_DCLICK)},

	{MXT_GEN_POWERCONFIG_T7,
		0, {0x3c, 0x0f, 0x04, 0x40, 0x00}, 5, 0, BIT_MASK(OP_SET)},
#endif

	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
	{MXT_SPT_USERDATA_T38,
		0, {0}, 0, 0x1, BIT_MASK(P_COMMON)},
};

static int plugin_proci_pi_show(struct plugin_proci *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;
	struct pi_config *cfg = p->cfg;
	const struct reg_config *r, *rcfg, *rstack, *rset;
	int i, j;

	dev_info(dev, "[mxt]PLUG_PROCI_VERSION: 0x%x\n", PLUG_PROCI_VERSION);

	if (!p->init)
		return 0;

	dev_info(dev, "[mxt]status: Flag=0x%08lx\n",
		obs->flag);

	for (i = 0; i < PI_LIST_NUM; i++) {
		rcfg = cfg->reg_cfg[i];
		rstack = obs->stack[i];
		rset = obs->set[i];

		dev_info(dev, "[mxt]PI config %d '%s':\n", i, pi_cfg_name[i]);

		dev_info(dev, "[mxt] rcfg %d\n", cfg->num_reg_cfg[i]);
		r = rcfg;
		for (j = 0; j < cfg->num_reg_cfg[i]; j++) {
			dev_info(dev, "[mxt]config %d-%d: T%d offset %d len %d(%lx, %lx, %hhd): ", i, j, r[j].reg, r[j].offset, r[j].len, r[j].mask, r[j].flag, r[j].sleep);
			print_hex_dump(KERN_INFO, "[mxt]", DUMP_PREFIX_NONE, 16, 1,
					r[j].buf, r[j].len, false);
		}

		dev_info(dev, "[mxt] rset\n");
		r = rset;
		for (j = 0; j < cfg->num_reg_cfg[i]; j++) {
			dev_info(dev, "[mxt]config %d-%d: T%d offset %d len %d(%lx, %lx): ", i, j, r[j].reg, r[j].offset, r[j].len, r[j].mask, r[j].flag);
			print_hex_dump(KERN_INFO, "[mxt]", DUMP_PREFIX_NONE, 16, 1,
					r[j].buf, r[j].len, false);
		}

		dev_info(dev, "[mxt] rstack\n");
		r = rstack;
		for (j = 0; j < cfg->num_reg_cfg[i]; j++) {
			dev_info(dev, "[mxt]config %d-%d: T%d offset %d len %d(%lx, %lx): ", i, j, r[j].reg, r[j].offset, r[j].len, r[j].mask, r[j].flag);
			print_hex_dump(KERN_INFO, "[mxt]", DUMP_PREFIX_NONE, 16, 1,
					r[j].buf, r[j].len, false);
		}
	}


	dev_info(dev, "[mxt]\n");

	return 0;
}

static int plugin_proci_pi_store(struct plugin_proci *p, const char *buf, size_t count)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer * obs = p->obs;
	struct pi_config *cfg = p->cfg;
	int offset, ofs, i, j, k, ret, val;
	struct reg_config rc, *r;

	dev_info(dev, "[mxt]pi store:%s\n", buf);

	if (!p->init)
		return 0;

	if (sscanf(buf, "status: Flag=0x%lx\n",
		&obs->flag) > 0) {
		dev_info(dev, "[mxt] OK\n");
	} else {
		ret = sscanf(buf, "config %d-%d: %n", &i, &j, &offset);
		dev_info(dev, "config (%d, %d), offset %d ret %d^\n", i, j, offset, ret);
		if (ret == 2) {
			if (i >= 0 && i < PI_LIST_NUM) {
				if (j >= 0 && j < cfg->num_reg_cfg[i]) {
					r = cfg->reg_cfg[i];
					memcpy(&rc, &r[j], sizeof(struct reg_config));

					ofs = 0;
					ret = sscanf(buf + offset, "T%hd offset %hd len %hd(%lx, %lx, %hhd): %n",
						&rc.reg, &rc.offset, &rc.len, &rc.mask, &rc.flag, &rc.sleep, &ofs);
					if (ret > 0) {
						dev_info(dev, "%s\n", buf + offset);
						dev_info(dev, "T%hd offset %hd len %hd(%lx, %lx):(ret %d ofs %d)^", rc.reg, rc.offset, rc.len, rc.mask, rc.flag, ret, ofs);
						if (rc.len > MAX_REG_DATA_LEN)
							rc.len = MAX_REG_DATA_LEN;

						for (k = 0; k < rc.len; k++) {
							offset += ofs;
							if (offset < count) {
								dev_info(dev, "%s\n", buf + offset);
								ret = sscanf(buf + offset, "%x %n",
									&val, &ofs);
								if (ret == 1) {
									rc.buf[k] = (u8)val;
									dev_info(dev, "%x", rc.buf[k]);
								} else
									break;
							} else
								break;
						}
						if (k && ret > 0) {
							print_trunk(rc.buf, 0, k);
							print_hex_dump(KERN_INFO, "[mxt]", DUMP_PREFIX_NONE, 16, 1,
								rc.buf, k, false);
							dev_info(dev, "set buf data %d\n", k);
						}

						memcpy(&r[j], &rc, sizeof(struct reg_config));
					} else
						dev_info(dev, "invalid string: %s\n", buf + offset);

				}
			}
		}
	}

	return 0;
}

static int init_pi_object(struct plugin_proci *p)
{
	const struct mxt_config *dcfg = p->dcfg;
	struct device *dev = dcfg->dev;
	struct pi_observer *obs = p->obs;
	struct pi_config *cfg = p->cfg;
	const struct reg_config *rcfg;
	struct reg_config *rstack, *rset;
	int mem_size;

	int i, j;

	cfg->reg_cfg[PI_GLOVE] = mxt_glove_cfg;
	cfg->num_reg_cfg[PI_GLOVE] = ARRAY_SIZE(mxt_glove_cfg);
	cfg->reg_cfg[PI_STYLUS] = mxt_stylus_cfg;
	cfg->num_reg_cfg[PI_STYLUS] = ARRAY_SIZE(mxt_stylus_cfg);
	cfg->reg_cfg[PI_DWAKE] = mxt_dwakeup_cfg;
	cfg->num_reg_cfg[PI_DWAKE] = ARRAY_SIZE(mxt_dwakeup_cfg);

	for (i = 0 , mem_size = 0; i < PI_LIST_NUM; i++)
		mem_size += cfg->num_reg_cfg[i] * sizeof(struct reg_config);

	mem_size <<= 1;
	dev_info(dev, "%s: alloc mem %d, each %d\n",
			__func__, mem_size, sizeof(struct reg_config));

	obs->mem = kzalloc(mem_size, GFP_KERNEL);
	if (!obs->mem)
		dev_err(dev, "Failed to allocate memory for pi observer reg mem\n");
		return -ENOMEM;

	for (i = 0, mem_size = 0; i < PI_LIST_NUM; i++) {
		obs->stack[i] = obs->mem + mem_size;
		mem_size += cfg->num_reg_cfg[i] * sizeof(struct reg_config);
		obs->set[i] = obs->mem + mem_size;
		mem_size += cfg->num_reg_cfg[i] * sizeof(struct reg_config);

		rcfg = cfg->reg_cfg[i];
		rstack = obs->stack[i];
		rset = obs->set[i];

		for (j = 0; j < cfg->num_reg_cfg[i]; j++) {

			memcpy(&rstack[j], &rcfg[j], sizeof(struct reg_config));
		}
	}

	for (i = 0; i < PI_LIST_NUM; i++) {
		rcfg = cfg->reg_cfg[i];
		rstack = obs->stack[i];
		rset = obs->set[i];
	}

	mutex_init(&obs->access_mutex);

	return 0;
}

static int deinit_pi_object(struct plugin_proci *p)
{


	struct pi_observer *obs = p->obs;
	struct pi_config *cfg = p->cfg;

	memset(cfg->reg_cfg, 0, sizeof(cfg->reg_cfg));

	if (obs->mem)
		kfree(obs->mem);
	return 0;
}


static int plugin_proci_pi_init(struct plugin_proci *p)
{
	const struct mxt_config *dcfg = p->dcfg;

	struct device *dev = dcfg->dev;

	dev_info(dev, "%s: plugin proci pi version 0x%x\n",
			__func__, PLUG_PROCI_VERSION);

	p->obs = kzalloc(sizeof(struct pi_observer), GFP_KERNEL);
	if (!p->obs) {
		dev_err(dev, "Failed to allocate memory for pi observer\n");
		return -ENOMEM;
	}

	p->cfg = kzalloc(sizeof(struct pi_config), GFP_KERNEL);
	if (!p->cfg) {
		dev_err(dev, "Failed to allocate memory for pi cfg\n");
		kfree(p->obs);
		return -ENOMEM;
	}

	if (init_pi_object(p) != 0) {
		dev_err(dev, "Failed to allocate memory for pi cfg\n");
		kfree(p->obs);
		kfree(p->cfg);
	}

	return  0;
}

static void plugin_proci_pi_deinit(struct plugin_proci *p)
{
	if (p->obs) {
		deinit_pi_object(p);
		kfree(p->obs);
	}
	if (p->cfg)
		kfree(p->cfg);
}

struct plugin_proci mxt_plugin_proci_pi = {
	.init = plugin_proci_pi_init,
	.deinit = plugin_proci_pi_deinit,
	.start = NULL,
	.stop = NULL,
	.hook_t6 = plugin_proci_pi_hook_t6,
	.hook_t24 = plugin_proci_pi_hook_t24,
	.hook_t61 = plugin_proci_pi_hook_t61,
	.hook_t81 = plugin_proci_pi_hook_t81,
	.hook_t92 = plugin_proci_pi_hook_t92,
	.hook_t93 = plugin_proci_pi_hook_t93,
	.wake_enable = plugin_proci_pi_wakeup_enable,
	.wake_disable = plugin_proci_pi_wakeup_disable,
	.pre_process = plugin_proci_pi_pre_process_messages,
	.post_process = plugin_proci_pi_post_process_messages,
	.show = plugin_proci_pi_show,
	.store = plugin_proci_pi_store,
};

int plugin_proci_init(struct plugin_proci *p)
{
	memcpy(p, &mxt_plugin_proci_pi, sizeof(struct plugin_proci));

	return 0;
}

