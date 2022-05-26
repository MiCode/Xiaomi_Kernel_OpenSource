// SPDX-License-Identifier: GPL-2.0
// Copyright (c) 2019 MediaTek Inc.

//#define DEBUG

#include <linux/delay.h>
#include <linux/module.h>
#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/gpio/consumer.h>
#include <linux/regulator/consumer.h>
#include <linux/pinctrl/consumer.h>

#include "kd_imgsensor_define_v4l2.h"
#include "adaptor.h"
#include "adaptor-hw.h"

#define INST_OPS(__ctx, __field, __idx, __hw_id, __set, __unset) do {\
	if (__ctx->__field[__idx]) { \
		__ctx->hw_ops[__hw_id].set = __set; \
		__ctx->hw_ops[__hw_id].unset = __unset; \
		__ctx->hw_ops[__hw_id].data = (void *)__idx; \
	} \
} while (0)

static const char * const clk_names[] = {
	ADAPTOR_CLK_NAMES
};

static const char * const reg_names[] = {
	ADAPTOR_REGULATOR_NAMES
};

static const char * const state_names[] = {
	ADAPTOR_STATE_NAMES
};

static struct clk *get_clk_by_freq(struct adaptor_ctx *ctx, int freq)
{
	switch (freq) {
	case 6:
		return ctx->clk[CLK_6M];
	case 12:
		return ctx->clk[CLK_12M];
	case 13:
		return ctx->clk[CLK_13M];
	case 19:
		return ctx->clk[CLK_19_2M];
	case 24:
		return ctx->clk[CLK_24M];
	case 26:
		return ctx->clk[CLK_26M];
	case 52:
		return ctx->clk[CLK_52M];
	}

	return NULL;
}

static int set_mclk(struct adaptor_ctx *ctx, void *data, int val)
{
	int ret;
	struct clk *mclk, *mclk_src;

	mclk = ctx->clk[CLK_MCLK];
	mclk_src = get_clk_by_freq(ctx, val);

	ret = clk_prepare_enable(mclk);
	if (ret) {
		dev_err(ctx->dev, "failed to enable mclk\n");
		return ret;
	}

	ret = clk_prepare_enable(mclk_src);
	if (ret) {
		dev_err(ctx->dev, "failed to enable mclk_src\n");
		return ret;
	}

	ret = clk_set_parent(mclk, mclk_src);
	if (ret) {
		dev_err(ctx->dev, "failed to set mclk's parent\n");
		return ret;
	}

	return 0;
}

static int unset_mclk(struct adaptor_ctx *ctx, void *data, int val)
{
	struct clk *mclk, *mclk_src;

	mclk = ctx->clk[CLK_MCLK];
	mclk_src = get_clk_by_freq(ctx, val);

	clk_disable_unprepare(mclk_src);
	clk_disable_unprepare(mclk);

	return 0;
}

static int set_reg(struct adaptor_ctx *ctx, void *data, int val)
{
	unsigned long long ret, idx;
	struct regulator *reg;

	idx = (unsigned long long)data;
	reg = ctx->regulator[idx];

	ret = regulator_set_voltage(reg, val, val);
	if (ret) {
		dev_dbg(ctx->dev, "failed to set voltage %s %d\n",
				reg_names[idx], val);
	}

	ret = regulator_enable(reg);
	if (ret) {
		dev_dbg(ctx->dev, "failed to enable %s\n",
				reg_names[idx]);
		return ret;
	}

	return 0;
}

static int unset_reg(struct adaptor_ctx *ctx, void *data, int val)
{
	unsigned long long ret, idx;
	struct regulator *reg;

	idx = (unsigned long long)data;
	reg = ctx->regulator[idx];

	ret = regulator_disable(reg);
	if (ret) {
		dev_dbg(ctx->dev, "failed to disable %s\n",
				reg_names[idx]);
		return ret;
	}

	return 0;
}

static int set_state(struct adaptor_ctx *ctx, void *data, int val)
{
	unsigned long long idx, x;
	int ret;

	idx = (unsigned long long)data;
	x = idx + val;

	ret = pinctrl_select_state(ctx->pinctrl, ctx->state[x]);
	if (ret < 0) {
		dev_err(ctx->dev, "fail to select %s\n", state_names[x]);
		return ret;
	}

	return 0;
}

static int unset_state(struct adaptor_ctx *ctx, void *data, int val)
{
	return set_state(ctx, data, 0);
}

static int set_state_div2(struct adaptor_ctx *ctx, void *data, int val)
{
	return set_state(ctx, data, val >> 1);
}

static int set_state_boolean(struct adaptor_ctx *ctx, void *data, int val)
{
	return set_state(ctx, data, !!val);
}

static int set_state_mipi_switch(struct adaptor_ctx *ctx, void *data, int val)
{
	return set_state(ctx, (void *)STATE_MIPI_SWITCH_ON, 0);
}

static int unset_state_mipi_switch(struct adaptor_ctx *ctx, void *data,
	int val)
{
	return set_state(ctx, (void *)STATE_MIPI_SWITCH_OFF, 0);
}

static int reinit_pinctrl(struct adaptor_ctx *ctx)
{
	int i;
	struct device *dev = ctx->dev;

	/* pinctrl */
	ctx->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ctx->pinctrl)) {
		dev_err(dev, "fail to get pinctrl\n");
		return PTR_ERR(ctx->pinctrl);
	}

	/* pinctrl states */
	for (i = 0; i < STATE_MAXCNT; i++) {
		ctx->state[i] = pinctrl_lookup_state(
				ctx->pinctrl, state_names[i]);
		if (IS_ERR(ctx->state[i])) {
			ctx->state[i] = NULL;
			dev_dbg(dev, "no state %s\n", state_names[i]);
		}
	}

	return 0;
}
int do_hw_power_on(struct adaptor_ctx *ctx)
{
	int i;
	const struct subdrv_pw_seq_entry *ent;
	struct adaptor_hw_ops *op;

	if (ctx->sensor_ws)
		__pm_stay_awake(ctx->sensor_ws);
	else
		dev_dbg(ctx->dev, "%s fail to __pm_stay_awake\n",
			__func__);

	/* may be released for mipi switch */
	if (!ctx->pinctrl)
		reinit_pinctrl(ctx);

	op = &ctx->hw_ops[HW_ID_MIPI_SWITCH];
	if (op->set)
		op->set(ctx, op->data, 0);

	for (i = 0; i < ctx->subdrv->pw_seq_cnt; i++) {
		ent = &ctx->subdrv->pw_seq[i];
		op = &ctx->hw_ops[ent->id];
		if (!op->set) {
			dev_dbg(ctx->dev, "cannot set comp %d val %d\n",
				ent->id, ent->val);
			continue;
		}
		op->set(ctx, op->data, ent->val);
		if (ent->delay)
			mdelay(ent->delay);
	}

	if (ctx->subdrv->ops->power_on)
		subdrv_call(ctx, power_on, NULL);

	//dev_dbg(ctx->dev, "%s\n", __func__);

	return 0;
}

int adaptor_hw_power_on(struct adaptor_ctx *ctx)
{

#ifndef IMGSENSOR_USE_PM_FRAMEWORK
	dev_dbg(ctx->dev, "%s power ref cnt = %d\n", __func__, ctx->power_refcnt);
	ctx->power_refcnt++;
	if (ctx->power_refcnt > 1) {
		dev_dbg(ctx->dev, "%s already powered, cnt = %d\n", __func__, ctx->power_refcnt);
		return 0;
	}
#endif
	return do_hw_power_on(ctx);
}

int do_hw_power_off(struct adaptor_ctx *ctx)
{
	int i;
	const struct subdrv_pw_seq_entry *ent;
	struct adaptor_hw_ops *op;

	/* call subdrv close function before pwr off */
	subdrv_call(ctx, close);

	if (ctx->subdrv->ops->power_off)
		subdrv_call(ctx, power_off, NULL);

	for (i = ctx->subdrv->pw_seq_cnt - 1; i >= 0; i--) {
		ent = &ctx->subdrv->pw_seq[i];
		op = &ctx->hw_ops[ent->id];
		if (!op->unset)
			continue;
		op->unset(ctx, op->data, ent->val);
		//msleep(ent->delay);
	}

	op = &ctx->hw_ops[HW_ID_MIPI_SWITCH];
	if (op->unset)
		op->unset(ctx, op->data, 0);

	/* the pins of mipi switch are shared. free it for another users */
	if (ctx->state[STATE_MIPI_SWITCH_ON] ||
		ctx->state[STATE_MIPI_SWITCH_OFF]) {
		devm_pinctrl_put(ctx->pinctrl);
		ctx->pinctrl = NULL;
	}

	if (ctx->sensor_ws)
		__pm_relax(ctx->sensor_ws);
	else
		dev_dbg(ctx->dev, "%s fail to __pm_relax\n",
			__func__);

	//dev_dbg(ctx->dev, "%s\n", __func__);
	return 0;

}
int adaptor_hw_power_off(struct adaptor_ctx *ctx)
{

#ifndef IMGSENSOR_USE_PM_FRAMEWORK

	if (!ctx->power_refcnt) {
		dev_dbg(ctx->dev, "%s power ref cnt = %d, skip due to not power on yet\n",
			__func__, ctx->power_refcnt);
		return 0;
	}
	dev_dbg(ctx->dev, "%s power ref cnt = %d\n", __func__, ctx->power_refcnt);
	ctx->power_refcnt--;
	if (ctx->power_refcnt > 0) {
		dev_dbg(ctx->dev, "%s skip due to cnt = %d\n", __func__, ctx->power_refcnt);
		return 0;
	}
	ctx->power_refcnt = 0;
	ctx->is_sensor_inited = 0;
	ctx->is_sensor_scenario_inited = 0;
#endif
	return do_hw_power_off(ctx);
}

int adaptor_hw_init(struct adaptor_ctx *ctx)
{
	int i;
	struct device *dev = ctx->dev;

	/* clocks */
	for (i = 0; i < CLK_MAXCNT; i++) {
		ctx->clk[i] = devm_clk_get(dev, clk_names[i]);
		if (IS_ERR(ctx->clk[i])) {
			ctx->clk[i] = NULL;
			dev_dbg(dev, "no clk %s\n", clk_names[i]);
		}
	}

	/* supplies */
	for (i = 0; i < REGULATOR_MAXCNT; i++) {
		ctx->regulator[i] = devm_regulator_get_optional(
				dev, reg_names[i]);
		if (IS_ERR(ctx->regulator[i])) {
			ctx->regulator[i] = NULL;
			dev_dbg(dev, "no reg %s\n", reg_names[i]);
		}
	}

	/* pinctrl */
	ctx->pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR(ctx->pinctrl)) {
		dev_err(dev, "fail to get pinctrl\n");
		return PTR_ERR(ctx->pinctrl);
	}

	/* pinctrl states */
	for (i = 0; i < STATE_MAXCNT; i++) {
		ctx->state[i] = pinctrl_lookup_state(
				ctx->pinctrl, state_names[i]);
		if (IS_ERR(ctx->state[i])) {
			ctx->state[i] = NULL;
			dev_dbg(dev, "no state %s\n", state_names[i]);
		}
	}

	/* install operations */

	INST_OPS(ctx, clk, CLK_MCLK, HW_ID_MCLK, set_mclk, unset_mclk);

	INST_OPS(ctx, regulator, REGULATOR_AVDD, HW_ID_AVDD,
			set_reg, unset_reg);

	INST_OPS(ctx, regulator, REGULATOR_DVDD, HW_ID_DVDD,
			set_reg, unset_reg);

	INST_OPS(ctx, regulator, REGULATOR_DOVDD, HW_ID_DOVDD,
			set_reg, unset_reg);

	INST_OPS(ctx, regulator, REGULATOR_AFVDD, HW_ID_AFVDD,
			set_reg, unset_reg);

	INST_OPS(ctx, regulator, REGULATOR_AVDD1, HW_ID_AVDD1,
			set_reg, unset_reg);

	if (ctx->state[STATE_MIPI_SWITCH_ON])
		ctx->hw_ops[HW_ID_MIPI_SWITCH].set = set_state_mipi_switch;

	if (ctx->state[STATE_MIPI_SWITCH_OFF])
		ctx->hw_ops[HW_ID_MIPI_SWITCH].unset = unset_state_mipi_switch;

	INST_OPS(ctx, state, STATE_MCLK_OFF, HW_ID_MCLK_DRIVING_CURRENT,
			set_state_div2, unset_state);

	INST_OPS(ctx, state, STATE_RST_LOW, HW_ID_RST,
			set_state, unset_state);

	INST_OPS(ctx, state, STATE_PDN_LOW, HW_ID_PDN,
			set_state, unset_state);

	INST_OPS(ctx, state, STATE_AVDD_OFF, HW_ID_AVDD,
			set_state_boolean, unset_state);

	INST_OPS(ctx, state, STATE_DVDD_OFF, HW_ID_DVDD,
			set_state_boolean, unset_state);

	INST_OPS(ctx, state, STATE_DOVDD_OFF, HW_ID_DOVDD,
			set_state_boolean, unset_state);

	INST_OPS(ctx, state, STATE_AFVDD_OFF, HW_ID_AFVDD,
			set_state_boolean, unset_state);

	INST_OPS(ctx, state, STATE_AVDD1_OFF, HW_ID_AVDD1,
			set_state_boolean, unset_state);

	/* the pins of mipi switch are shared. free it for another users */
	if (ctx->state[STATE_MIPI_SWITCH_ON] ||
		ctx->state[STATE_MIPI_SWITCH_OFF]) {
		devm_pinctrl_put(ctx->pinctrl);
		ctx->pinctrl = NULL;
	}

	return 0;
}

int adaptor_hw_sensor_reset(struct adaptor_ctx *ctx)
{
	dev_info(ctx->dev, "%s %d|%d|%d\n",
		__func__,
		ctx->is_streaming,
		ctx->is_sensor_inited,
		ctx->power_refcnt);

	if (ctx->is_streaming == 1 &&
		ctx->is_sensor_inited == 1 &&
		ctx->power_refcnt > 0) {

		do_hw_power_off(ctx);
		do_hw_power_on(ctx);

		return 0;
	}
	dev_info(ctx->dev, "%s skip to reset due to either integration or else\n",
		__func__);

	return -1;
}


