// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/pm_opp.h>
#include <linux/of.h>
#include <linux/delay.h>
#include <linux/devfreq.h>
#include <linux/pm_runtime.h>

#include "apu_log.h"
#include "apu_regulator.h"
#include "apu_devfreq.h"
#include "apu_clk.h"
#include "apusys_power.h"
#include "apu_common.h"
#include "apu_dbg.h"

/* notify regulator consumers and downstream regulator consumers.
 * Note mutex must be held by caller.
 */
static int _apu_notifier_call_chain(struct apu_regulator *reg,
				  unsigned long event, void *data)
{
	/* call rdev chain first */
	return blocking_notifier_call_chain(&reg->nf_head, event, data);
}

static void _regulator_apu_settle_time(struct apu_regulator *reg,
				       int old_uV, int new_uV)
{
	unsigned int ramp_delay = 0;
	unsigned int settle_rate = 1;
	unsigned int latency = 0;

	/* check whehter kernel regulator frame work suggest delay time already */
	ramp_delay =
		regulator_set_voltage_time(reg->vdd, old_uV, new_uV);
	if (ramp_delay)
		goto delay;

	/* kernel regulator frame work not provide delay time */
	latency = reg->cstr.settling_time;
	if (reg->cstr.settling_time_up &&
		 (new_uV > old_uV))
		settle_rate = reg->cstr.settling_time_up;
	else if (reg->cstr.settling_time_down &&
		 (new_uV < old_uV))
		settle_rate = reg->cstr.settling_time_down;

	ramp_delay = DIV_ROUND_UP(abs(new_uV - old_uV), settle_rate);
	ramp_delay += reg->cstr.settling_time;
delay:
	argul_info(reg->dev, "[%s] wait %s %d->%d diff:%d slew %lu(uv/us) %dus\n",
		"APUSYS_SETTLE_TIME_TEST", reg->name, TOMV(old_uV), TOMV(new_uV),
		(new_uV - old_uV), settle_rate, ramp_delay);
	udelay(ramp_delay);
}

static int _apu_set_volt_with_wait(struct apu_regulator *reg, int c_volt, int min_uV, int max_uV)
{
	int ret;
	struct regulator *vdd = reg->vdd;
	struct pre_voltage_change_data data;

	if (reg->floor_volt > min_uV) {
		min_uV = reg->floor_volt;
		max_uV = reg->floor_volt;
	}

	data.old_uV = reg->cur_volt;
	data.min_uV = min_uV;
	data.max_uV = max_uV;
	ret = _apu_notifier_call_chain(reg, REGULATOR_EVENT_PRE_VOLTAGE_CHANGE,
				   &data);
	if (ret & NOTIFY_STOP_MASK)
		return -EINVAL;

	ret = regulator_set_voltage(vdd, min_uV, max_uV);
	if (ret) {
		argul_err(reg->dev, "[%s] set %s %dmV-->%dmV fail, ret = %d",
			  __func__, reg->name, TOMV(c_volt), TOMV(min_uV), ret);
		goto out;
	}

	_regulator_apu_settle_time(reg, c_volt, min_uV);
	reg->cur_volt = min_uV;

out:
	return ret;
}

static void apu_mdla_restore_default_opp(struct work_struct *work)
{
	struct apu_regulator *dst =
		container_of(work, struct apu_regulator, deffer_work);
	struct apu_dev *ad = dev_get_drvdata(dst->dev);
	struct apu_clk_ops *clk_ops = ad->aclk->ops;
	ulong rate = 0, volt = 0;

	/* try to restore default voltage for MDLA */
	apu_get_recommend_freq_volt(ad->dev, &rate, &volt, 0);
	mutex_lock_nested(&ad->df->lock, ((struct apu_gov_data *)(ad->df->data))->depth);
	if (round_Mhz(clk_ops->get_rate(ad->aclk), rate)) {
		regulator_set_voltage(dst->vdd, volt, volt);
		argul_info(ad->dev, "[%s] set voltage to %d\n", __func__, volt);
	}
	mutex_unlock(&ad->df->lock);
}


static int apu_vsram_mdla_constrain(struct notifier_block *nb,
				  unsigned long event, void *data)
{
	int ret = NOTIFY_OK, diff = 0, cur_volt = 0;
	struct pre_voltage_change_data *pre_volt;
	struct apu_regulator *mdla_reg = to_regulator_apu(nb);

	if (event != REGULATOR_EVENT_PRE_VOLTAGE_CHANGE)
		goto out;

	if (IS_ERR_OR_NULL(mdla_reg->vdd))
		goto out;

	pre_volt = (struct pre_voltage_change_data *)(data);

	/* lock mdla first, then no one can change its voltage */
	mutex_lock_nested(&mdla_reg->reg_lock, SINGLE_DEPTH_NESTING);
	if (!regulator_is_enabled(mdla_reg->vdd)) {
		if (abs(pre_volt->min_uV - mdla_reg->def_volt) > mdla_reg->constrain_band)
			mdla_reg->floor_volt = mdla_reg->constrain_volt;
		else
			mdla_reg->floor_volt = 0;
		goto unlock;
	}

	cur_volt = mdla_reg->cur_volt;
	diff = pre_volt->min_uV - cur_volt;

	if (diff < 0) {
		/* the voltage of vsram ALWAYS bigger then vmdla */
		WARN_ONCE(1, "[%s] pre_min/cur %d/%d\n",
			mdla_reg->name, pre_volt->min_uV, cur_volt);
		ret = -EINVAL;
		goto unlock;
	} else if (diff < mdla_reg->constrain_band) {
		/* not meet gard band, need to vote mdla as 575mv */
		mdla_reg->floor_volt = 0;
		queue_pm_work(&mdla_reg->deffer_work);
		goto unlock;
	} else {
		/* touch the gard band, need to change mdla as constrain voltage */
		mdla_reg->floor_volt = mdla_reg->constrain_volt;
		ret = regulator_set_voltage(mdla_reg->vdd,
						mdla_reg->constrain_volt,
						mdla_reg->constrain_volt);
		if (ret)
			goto unlock;
		mdla_reg->cur_volt = mdla_reg->constrain_volt;
		_regulator_apu_settle_time(mdla_reg, cur_volt, mdla_reg->constrain_volt);
		argul_info(mdla_reg->dev, "[%s] cur %d floor_vol = %dmV\n",
			__func__, TOMV(mdla_reg->cur_volt), TOMV(mdla_reg->floor_volt));
	}

unlock:
	mutex_unlock(&mdla_reg->reg_lock);
out:
	return ret;
}


/**
 * regulator_apu_unregister_notifier - unregister regulator event notifier
 * @regulator: regulator source
 * @nb: notifier block
 *
 * Unregister regulator event notifier block.
 */
int regulator_apu_unregister_notifier(struct apu_regulator *reg,
				struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&reg->nf_head, nb);
}


/**
 * regulator_apu_register_notifier - register regulator event notifier
 * @regulator: regulator source
 * @nb: notifier block
 *
 * Register notifier block to receive regulator events.
 */
int regulator_apu_register_notifier(struct apu_regulator *reg,
			      struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&reg->nf_head, nb);
}


static int regulator_apu_get_voltage(struct apu_regulator_gp *rgul_gp)
{
	int cur_volt;

	mutex_lock(&rgul_gp->rgul->reg_lock);
	if (rgul_gp->rgul)
		cur_volt = regulator_get_voltage(rgul_gp->rgul->vdd);
	else
		cur_volt = -EINVAL;
	mutex_unlock(&rgul_gp->rgul->reg_lock);

	return cur_volt;
}


/**
 * regulator_apu_set_voltage - set regulator output voltage
 * @regulator: regulator source
 * @min_uV: Minimum required voltage in uV
 * @max_uV: Maximum acceptable voltage in uV
 *
 * Sets a voltage regulator to the desired output voltage. This can be set
 * during any regulator state. IOW, regulator can be disabled or enabled.
 *
 * If the regulator is enabled then the voltage will change to the new value
 * immediately otherwise if the regulator is disabled the regulator will
 * output at the new voltage when enabled.
 *
 * NOTE: If the regulator is shared between several devices then the lowest
 * request voltage that meets the system constraints will be used.
 * Regulator system constraints must be set for this regulator before
 * calling this function otherwise this call will fail.
 */
int regulator_apu_set_voltage(struct apu_regulator_gp *rgul_gp, int min_uV, int max_uV)
{
	int ret = 0;
	struct apu_regulator *sup_reg = NULL, *reg = NULL;
	int c_volt = 0, t_volt = 0, nt_volt = 0, s_volt;

	sup_reg = rgul_gp->rgul_sup;
	reg = rgul_gp->rgul;

	mutex_lock(&reg->reg_lock);
	if (!IS_ERR_OR_NULL(sup_reg))
		mutex_lock(&sup_reg->reg_lock);

	c_volt = reg->cur_volt;
	if (IS_ERR_OR_NULL(sup_reg))
		goto bypass_sup;
	t_volt = sup_reg->supply_trans_uV;
	nt_volt = sup_reg->supply_trans_next_uV;
	s_volt = sup_reg->cur_volt;

	argul_info(rgul_gp->dev, "[%s] min/c/s/t/floor %dmV/%dmV/%dmV/%dmV/%dmV\n",
		   __func__, TOMV(min_uV), TOMV(c_volt), TOMV(s_volt),
		   TOMV(t_volt), TOMV(reg->floor_volt));

	if (min_uV > t_volt && c_volt > t_volt) {
		if (s_volt < nt_volt) {
			t_volt = s_volt;
			goto set_next_trant;
		}
		goto bypass_sup;
	} else if (min_uV <= t_volt && c_volt <= t_volt) {
		if (s_volt >= nt_volt) {
			nt_volt = s_volt;
			goto set_trant;
		}
		goto bypass_sup;
	}

	/* target_vol > trans_volt */
	if (min_uV > t_volt) {
		/* if cur_volt < trans_volt, raise regulator to trans_volt */
		if (c_volt < t_volt) {
			ret = _apu_set_volt_with_wait(reg, c_volt, t_volt, t_volt);
			if (ret)
				goto out;
			/* update finish, set curren volt as trans volt */
			c_volt = t_volt;
		}
set_next_trant:
		/* change suplier to next trans volt */
		ret = _apu_set_volt_with_wait(sup_reg, t_volt, nt_volt, nt_volt);
		if (ret)
			goto out;
		argul_info(rgul_gp->dev, "[%s] \"%s\" final %dmV",
			   __func__, rgul_gp->rgul_sup->name, TOMV(nt_volt));

	} else {
		if (c_volt > t_volt) {
			ret = _apu_set_volt_with_wait(reg, c_volt, t_volt, t_volt);
			if (ret)
				goto out;
			c_volt = t_volt;
		}
set_trant:
		/* change suplier to trans volt */
		ret = _apu_set_volt_with_wait(sup_reg, nt_volt, t_volt, t_volt);
		if (ret)
			goto out;
		argul_info(rgul_gp->dev, "[%s] \"%s\" final %dmV",
			   __func__, rgul_gp->rgul_sup->name, TOMV(t_volt));

	}

	/* no need to change Vsram voltage or Vsram setting finished*/
bypass_sup:
	if (!IS_ERR_OR_NULL(reg)) {
		ret = _apu_set_volt_with_wait(reg, c_volt, min_uV, max_uV);
		if (ret)
			goto out;
	}

	argul_info(rgul_gp->dev, "[%s] \"%s\" final %dmV",
				__func__, reg->name, TOMV(min_uV));
out:
	if (!IS_ERR_OR_NULL(sup_reg))
		mutex_unlock(&sup_reg->reg_lock);
	mutex_unlock(&reg->reg_lock);

	apu_get_power_info(0);
	return ret;
}

static int regulator_apu_enable(struct apu_regulator_gp *rgul_gp)
{
	int ret = 0;
	struct apu_regulator *dst = NULL;
	int n_volt;

	if (!IS_ERR_OR_NULL(rgul_gp->rgul_sup)) {
		dst = rgul_gp->rgul_sup;
		if (!dst->enabled) {
			ret = regulator_enable(dst->vdd);
			if (ret) {
				argul_err(rgul_gp->dev, "[%s] %s enable fail, ret = %d\n",
					dst->name, ret);
				goto out;
			}
			dst->enabled = 1;
		}
		dst->cur_volt = regulator_get_voltage(dst->vdd);
		if (dst->cur_volt < 0)
			goto out;
	}

	if (rgul_gp->rgul) {
		dst = rgul_gp->rgul;
		if (!dst->enabled) {
			ret = regulator_enable(dst->vdd);
			if (ret) {
				argul_err(rgul_gp->dev, "[%s] %s enable fail, ret = %d\n",
					dst->name, ret);
				goto out;
			}
			dst->enabled = 1;
		}
		dst->cur_volt = regulator_get_voltage(dst->vdd);
		if (dst->cur_volt < 0)
			goto out;
	}

	/* let regulator to be floor or default voltage */
	n_volt = dst->floor_volt ? dst->floor_volt : dst->def_volt;
	ret = regulator_apu_set_voltage(rgul_gp, n_volt, n_volt);
out:
	apu_get_power_info(0);
	return ret;
}

static int regulator_apu_disable(struct apu_regulator_gp *rgul_gp)
{
	int ret = 0;
	struct apu_regulator *dst = NULL;

	ret = regulator_apu_set_voltage(rgul_gp,
				rgul_gp->rgul->shut_volt,
				rgul_gp->rgul->shut_volt);
	if (ret)
		goto out;

	if (!IS_ERR_OR_NULL(rgul_gp->rgul)) {
		dst = rgul_gp->rgul;
		if (dst->cstr.always_on)
			goto out;
		ret = regulator_disable(dst->vdd);
		if (ret) {
			argul_err(rgul_gp->dev, "[%s] %s disable fail, ret = %d\n",
				__func__, rgul_gp->rgul->name, ret);
			goto out;
		}
		dst->enabled = 0;
	}

	if (!IS_ERR_OR_NULL(rgul_gp->rgul_sup)) {
		dst = rgul_gp->rgul_sup;
		if (dst->cstr.always_on)
			goto out;
		ret = regulator_disable(dst->vdd);
		if (ret) {
			argul_err(rgul_gp->dev, "[%s] %s disable fail, ret = %d\n",
				__func__, dst->name, ret);
			goto out;
		}
		dst->enabled = 0;
	}

	apu_get_power_info(0);
out:
	return ret;
}

static struct apu_regulator_ops apu_rgul_gp_ops = {
	.enable = regulator_apu_enable,
	.disable = regulator_apu_disable,
	.set_voltage = regulator_apu_set_voltage,
	.get_voltage = regulator_apu_get_voltage,
};


static struct apu_regulator mt6873vcore = {
	.name = "vcore",
};

static struct apu_regulator mt6873vsram = {
	.name = "vsram",
	.cstr = {
		.settling_time = 8,
		.settling_time_up = 10000,
		.settling_time_down = 5000,
		.always_on = 1,
	},
	.def_volt = 750000,
	.shut_volt = 750000,
	.supply_trans_uV = 750000,
	.supply_trans_next_uV = 825000,
};

static struct apu_regulator mt6853vsram = {
	.name = "vsram",
	.cstr = {
		.settling_time = 8,
		.settling_time_up = 11250,
		.settling_time_down = 4500,
		.always_on = 1,
	},
	.def_volt = 750000,
	.shut_volt = 750000,
	.supply_trans_uV = 750000,
	.supply_trans_next_uV = 800000,
};

static struct apu_regulator mt688xvsram = {
	.name = "vsram",
	.cstr = {
		.settling_time = 8,
		.settling_time_up = 10000,
		.settling_time_down = 5000,
	},
	.def_volt = 750000,
	.shut_volt = 750000,
	.supply_trans_uV = 750000,
	.supply_trans_next_uV = 825000,
};

static struct apu_regulator mt6873vvpu = {
	.name = "vvpu",
	.cstr = {
		.settling_time = 8,
		.settling_time_up = 10000,
		.settling_time_down = 5000,
		.always_on = 1,
	},
};

static struct apu_regulator mt6853vvpu = {
	.name = "vvpu",
	.cstr = {
		.settling_time = 8,
		.settling_time_up = 11250,
		.settling_time_down = 4500,
		.always_on = 1,
	},
	.shut_volt = 550000,
};

static struct apu_regulator mt688xvvpu = {
	.name = "vvpu",
	.cstr = {
		.settling_time = 8,
		.settling_time_up = 10000,
		.settling_time_down = 5000,
	},
};

static struct apu_regulator mt6873vmdla = {
	.name = "vmdla",
	.cstr = {
		.settling_time = 8,
		.settling_time_up = 10000,
		.settling_time_down = 5000,
		.always_on = 0
	},
	.notify_reg = &mt6873vsram,
	.notify_func = apu_vsram_mdla_constrain,
	.constrain_band = (800000 - 575000), /* gard band */
	.deffer_func = apu_mdla_restore_default_opp,
};

static struct apu_regulator mt688xvmdla = {
	.name = "vmdla",
	.cstr = {
		.settling_time = 8,
		.settling_time_up = 10000,
		.settling_time_down = 5000,
	},
	.notify_reg = &mt688xvsram,
	.notify_func = apu_vsram_mdla_constrain,
	.constrain_band = (800000 - 575000), /* gard band */
	.deffer_func = apu_mdla_restore_default_opp,
};


static struct apu_regulator_gp mt6873_core_rgul_gp = {
	.rgul = &mt6873vcore,
	.ops = &apu_rgul_gp_ops,
};

static struct apu_regulator_gp mt6873_conn_rgul_gp = {
	.rgul_sup = &mt6873vsram,
	.rgul = &mt6873vvpu,
	.ops = &apu_rgul_gp_ops,
};

static struct apu_regulator_gp mt6853_conn_rgul_gp = {
	.rgul_sup = &mt6853vsram,
	.rgul = &mt6853vvpu,
	.ops = &apu_rgul_gp_ops,
};

static struct apu_regulator_gp mt688x_conn_rgul_gp = {
	.rgul_sup = &mt688xvsram,
	.rgul = &mt688xvvpu,
	.ops = &apu_rgul_gp_ops,
};

static struct apu_regulator_gp mt6873_mdla_rgul_gp = {
	.rgul = &mt6873vmdla,
	.ops = &apu_rgul_gp_ops,
};

static struct apu_regulator_gp mt688x_mdla_rgul_gp = {
	.rgul = &mt688xvmdla,
	.ops = &apu_rgul_gp_ops,
};

static const struct apu_regulator_array apu_rgul_gps[] = {
	{ .name = "mt6873_core", .argul_gp = &mt6873_core_rgul_gp },
	{ .name = "mt6873_conn", .argul_gp = &mt6873_conn_rgul_gp },
	{ .name = "mt6853_conn", .argul_gp = &mt6853_conn_rgul_gp },
	{ .name = "mt688x_conn", .argul_gp = &mt688x_conn_rgul_gp },
	{ .name = "mt6873_mdla", .argul_gp = &mt6873_mdla_rgul_gp },
	{ .name = "mt688x_mdla", .argul_gp = &mt688x_mdla_rgul_gp },
};

struct apu_regulator_gp *regulator_apu_gp_get(struct apu_dev *ad, const char *name)
{
	int i = 0;

	if (!name)
		goto out;

	for (i = 0; i < ARRAY_SIZE(apu_rgul_gps); i++) {
		if (strcmp(name, apu_rgul_gps[i].name) == 0)
			return apu_rgul_gps[i].argul_gp;
	}

	aprobe_err(ad->dev, "%s cannot find regulator \"%s\"\n", __func__, name);
out:
	return ERR_PTR(-ENOENT);
}
