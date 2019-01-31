/*
 *
 * (C) COPYRIGHT 2016-2017 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * A copy of the licence is included with the program, and can also be obtained
 * from Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301, USA.
 *
 */


#include <linux/thermal.h>
#include <linux/devfreq_cooling.h>
#include <linux/of.h>
#include "mali_kbase.h"
#include "mali_kbase_ipa.h"
#include "mali_kbase_ipa_debugfs.h"

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 13, 0))
#include <linux/pm_opp.h>
#else
#include <linux/opp.h>
#define dev_pm_opp_find_freq_exact opp_find_freq_exact
#define dev_pm_opp_get_voltage opp_get_voltage
#define dev_pm_opp opp
#endif

#define KBASE_IPA_FALLBACK_MODEL_NAME "mali-simple-power-model"

int kbase_ipa_model_recalculate(struct kbase_ipa_model *model)
{
	int err = 0;

	lockdep_assert_held(&model->kbdev->ipa.lock);

	if (model->ops->recalculate) {
		err = model->ops->recalculate(model);
		if (err) {
			dev_err(model->kbdev->dev,
				"recalculation of power model %s returned error %d\n",
				model->ops->name, err);
		}
	}

	return err;
}

int kbase_ipa_model_ops_register(struct kbase_device *kbdev,
			     struct kbase_ipa_model_ops *new_model_ops)
{
	struct kbase_ipa_model *new_model;

	lockdep_assert_held(&kbdev->ipa.lock);

	new_model = kzalloc(sizeof(struct kbase_ipa_model), GFP_KERNEL);
	if (!new_model)
		return -ENOMEM;

	new_model->kbdev = kbdev;
	new_model->ops = new_model_ops;

	list_add(&new_model->link, &kbdev->ipa.power_models);

	return 0;
}

static int kbase_ipa_internal_models_append_list(struct kbase_device *kbdev)
{
	int err;

	INIT_LIST_HEAD(&kbdev->ipa.power_models);

	/* Always have the simple IPA model */
	err = kbase_ipa_model_ops_register(kbdev, &kbase_simple_ipa_model_ops);

	if (err)
		return err;

	return err;
}

struct kbase_ipa_model *kbase_ipa_get_model(struct kbase_device *kbdev,
					    const char *name)
{
	/* Search registered power models first */
	struct list_head *it;

	lockdep_assert_held(&kbdev->ipa.lock);

	list_for_each(it, &kbdev->ipa.power_models) {
		struct kbase_ipa_model *model =
				list_entry(it,
					   struct kbase_ipa_model,
					   link);
		if (strcmp(model->ops->name, name) == 0)
			return model;
	}

	return NULL;
}

void kbase_ipa_model_use_fallback_locked(struct kbase_device *kbdev)
{
	atomic_set(&kbdev->ipa_use_configured_model, false);
}


void kbase_ipa_model_use_configured_locked(struct kbase_device *kbdev)
{
	atomic_set(&kbdev->ipa_use_configured_model, true);
}

const char *kbase_ipa_model_name_from_id(u32 gpu_id)
{
	const u32 prod_id = (gpu_id & GPU_ID_VERSION_PRODUCT_ID) >>
			GPU_ID_VERSION_PRODUCT_ID_SHIFT;

	if (GPU_ID_IS_NEW_FORMAT(prod_id)) {
		switch (GPU_ID2_MODEL_MATCH_VALUE(prod_id)) {
		case GPU_ID2_PRODUCT_TMIX:
			return KBASE_IPA_FALLBACK_MODEL_NAME;
		default:
			return KBASE_IPA_FALLBACK_MODEL_NAME;
		}
	}

	return KBASE_IPA_FALLBACK_MODEL_NAME;
}

static struct device_node *get_model_dt_node(struct kbase_ipa_model *model)
{
	struct device_node *model_dt_node;
	char compat_string[64];

	snprintf(compat_string, sizeof(compat_string), "arm,%s",
		 model->ops->name);

	model_dt_node = of_find_compatible_node(model->kbdev->dev->of_node,
						NULL, compat_string);
	if (!model_dt_node) {
		dev_warn(model->kbdev->dev,
			 "Couldn't find power_model DT node matching \'%s\'\n",
			 compat_string);
	}

	return model_dt_node;
}

int kbase_ipa_model_add_param_u32_def(struct kbase_ipa_model *model,
				      const char *name, u32 *addr,
				      bool has_default, u32 default_value)
{
	int err;
	struct device_node *model_dt_node = get_model_dt_node(model);

	err = of_property_read_u32(model_dt_node, name, addr);

	if (err && !has_default) {
		dev_err(model->kbdev->dev,
			"No DT entry or default found for %s.%s, err = %d\n",
			model->ops->name, name, err);
		goto exit;
	} else if (err && has_default) {
		*addr = default_value;
		dev_dbg(model->kbdev->dev, "%s.%s = %u (default)\n",
			model->ops->name, name, *addr);
		err = 0;
	} else /* !err */ {
		dev_dbg(model->kbdev->dev, "%s.%s = %u (DT)\n",
			model->ops->name, name, *addr);
	}

	err = kbase_ipa_model_param_add(model, name, addr, sizeof(u32),
					PARAM_TYPE_U32);
exit:
	return err;
}

int kbase_ipa_model_add_param_s32_array(struct kbase_ipa_model *model,
					const char *name, s32 *addr,
					size_t num_elems)
{
	int err, i;
	struct device_node *model_dt_node = get_model_dt_node(model);

	err = of_property_read_u32_array(model_dt_node, name, addr, num_elems);

	if (err) {
		dev_err(model->kbdev->dev,
			"No DT entry found for %s.%s, err = %d\n",
			model->ops->name, name, err);
		goto exit;
	} else {
		for (i = 0; i < num_elems; ++i)
			dev_dbg(model->kbdev->dev, "%s.%s[%u] = %d (DT)\n",
				model->ops->name, name, i, *(addr + i));
	}

	/* Create a unique debugfs entry for each element */
	for (i = 0; i < num_elems; ++i) {
		char elem_name[32];

		snprintf(elem_name, sizeof(elem_name), "%s.%d", name, i);
		err = kbase_ipa_model_param_add(model, elem_name, &addr[i],
						sizeof(s32), PARAM_TYPE_S32);
		if (err)
			goto exit;
	}
exit:
	return err;
}

int kbase_ipa_model_add_param_string(struct kbase_ipa_model *model,
				     const char *name, char *addr,
				     size_t len)
{
	int err;
	struct device_node *model_dt_node = get_model_dt_node(model);
	const char *string_prop_value;

	err = of_property_read_string(model_dt_node, name,
				      &string_prop_value);
	if (err) {
		dev_err(model->kbdev->dev,
			"No DT entry found for %s.%s, err = %d\n",
			model->ops->name, name, err);
		goto exit;
	} else {
		strncpy(addr, string_prop_value, len);
		dev_dbg(model->kbdev->dev, "%s.%s = \'%s\' (DT)\n",
			model->ops->name, name, string_prop_value);
	}

	err = kbase_ipa_model_param_add(model, name, addr, len,
					PARAM_TYPE_STRING);
exit:
	return err;
}

static void term_model(struct kbase_ipa_model *model)
{
	if (!model)
		return;

	lockdep_assert_held(&model->kbdev->ipa.lock);

	if (model->ops->term)
		model->ops->term(model);

	kbase_ipa_model_param_free_all(model);
}

static struct kbase_ipa_model *init_model(struct kbase_device *kbdev,
					  const char *model_name)
{
	struct kbase_ipa_model *model;
	int err;

	lockdep_assert_held(&kbdev->ipa.lock);

	model = kbase_ipa_get_model(kbdev, model_name);
	if (!model) {
		dev_err(kbdev->dev, "power model \'%s\' not found\n",
			model_name);
		return NULL;
	}

	INIT_LIST_HEAD(&model->params);

	err = model->ops->init(model);
	if (err) {
		dev_err(kbdev->dev,
			"init of power model \'%s\' returned error %d\n",
			model_name, err);
		term_model(model);
		return NULL;
	}

	err = kbase_ipa_model_recalculate(model);
	if (err) {
		term_model(model);
		return NULL;
	}

	return model;
}

static void kbase_ipa_term_locked(struct kbase_device *kbdev)
{
	lockdep_assert_held(&kbdev->ipa.lock);

	/* Clean up the models */
	if (kbdev->ipa.configured_model != kbdev->ipa.fallback_model)
		term_model(kbdev->ipa.configured_model);
	term_model(kbdev->ipa.fallback_model);

	/* Clean up the list */
	if (!list_empty(&kbdev->ipa.power_models)) {
		struct kbase_ipa_model *model_p, *model_n;

		list_for_each_entry_safe(model_p, model_n, &kbdev->ipa.power_models, link) {
			list_del(&model_p->link);
			kfree(model_p);
		}
	}
}

int kbase_ipa_init(struct kbase_device *kbdev)
{

	const char *model_name;
	struct kbase_ipa_model *default_model = NULL;
	int err;

	mutex_init(&kbdev->ipa.lock);
	/*
	 * Lock during init to avoid warnings from lockdep_assert_held (there
	 * shouldn't be any concurrent access yet).
	 */
	mutex_lock(&kbdev->ipa.lock);

	/* Add default ones to the list */
	err = kbase_ipa_internal_models_append_list(kbdev);

	/* The simple IPA model must *always* be present.*/
	default_model = init_model(kbdev, KBASE_IPA_FALLBACK_MODEL_NAME);
	if (!default_model) {
		err = -EINVAL;
		goto end;
	}

	kbdev->ipa.fallback_model = default_model;
	err = of_property_read_string(kbdev->dev->of_node,
				      "ipa-model",
				      &model_name);
	if (err) {
		/* Attempt to load a match from GPU-ID */
		u32 gpu_id;

		gpu_id = kbdev->gpu_props.props.raw_props.gpu_id;
		model_name = kbase_ipa_model_name_from_id(gpu_id);
	}

	if (strcmp(KBASE_IPA_FALLBACK_MODEL_NAME, model_name) != 0) {
		kbdev->ipa.configured_model = init_model(kbdev, model_name);
		if (!kbdev->ipa.configured_model) {
			err = -EINVAL;
			goto end;
		}
	} else {
		kbdev->ipa.configured_model = default_model;
		err = 0;
	}

	kbase_ipa_model_use_configured_locked(kbdev);

end:
	if (err)
		kbase_ipa_term_locked(kbdev);
	else
		dev_info(kbdev->dev,
			 "Using configured power model %s, and fallback %s\n",
			 kbdev->ipa.fallback_model->ops->name,
			 kbdev->ipa.configured_model->ops->name);

	mutex_unlock(&kbdev->ipa.lock);
	return err;
}

void kbase_ipa_term(struct kbase_device *kbdev)
{
	mutex_lock(&kbdev->ipa.lock);
	kbase_ipa_term_locked(kbdev);
	mutex_unlock(&kbdev->ipa.lock);
}

/**
 * kbase_scale_dynamic_power() - Scale a dynamic power coefficient to an OPP
 * @c:		Dynamic model coefficient, in pW/(Hz V^2). Should be in range
 *		0 < c < 2^26 to prevent overflow.
 * @freq:	Frequency, in Hz. Range: 2^23 < freq < 2^30 (~8MHz to ~1GHz)
 * @voltage:	Voltage, in mV. Range: 2^9 < voltage < 2^13 (~0.5V to ~8V)
 *
 * Keep a record of the approximate range of each value at every stage of the
 * calculation, to ensure we don't overflow. This makes heavy use of the
 * approximations 1000 = 2^10 and 1000000 = 2^20, but does the actual
 * calculations in decimal for increased accuracy.
 *
 * Return: Power consumption, in mW. Range: 0 < p < 2^13 (0W to ~8W)
 */
static inline unsigned long kbase_scale_dynamic_power(const unsigned long c,
						      const unsigned long freq,
						      const unsigned long voltage)
{
	/* Range: 2^8 < v2 < 2^16 m(V^2) */
	const unsigned long v2 = (voltage * voltage) / 1000;

	/* Range: 2^3 < f_MHz < 2^10 MHz */
	const unsigned long f_MHz = freq / 1000000;

	/* Range: 2^11 < v2f_big < 2^26 kHz V^2 */
	const unsigned long v2f_big = v2 * f_MHz;

	/* Range: 2^1 < v2f < 2^16 MHz V^2 */
	const unsigned long v2f = v2f_big / 1000;

	/* Range (working backwards from next line): 0 < v2fc < 2^23 uW.
	 * Must be < 2^42 to avoid overflowing the return value. */
	const u64 v2fc = (u64) c * (u64) v2f;

	/* Range: 0 < v2fc / 1000 < 2^13 mW */
	return v2fc / 1000;
}

/**
 * kbase_scale_static_power() - Scale a static power coefficient to an OPP
 * @c:		Static model coefficient, in uW/V^3. Should be in range
 *		0 < c < 2^32 to prevent overflow.
 * @voltage:	Voltage, in mV. Range: 2^9 < voltage < 2^13 (~0.5V to ~8V)
 *
 * Return: Power consumption, in mW. Range: 0 < p < 2^13 (0W to ~8W)
 */
unsigned long kbase_scale_static_power(const unsigned long c,
				       const unsigned long voltage)
{
	/* Range: 2^8 < v2 < 2^16 m(V^2) */
	const unsigned long v2 = (voltage * voltage) / 1000;

	/* Range: 2^17 < v3_big < 2^29 m(V^2) mV */
	const unsigned long v3_big = v2 * voltage;

	/* Range: 2^7 < v3 < 2^19 m(V^3) */
	const unsigned long v3 = v3_big / 1000;

	/*
	 * Range (working backwards from next line): 0 < v3c_big < 2^33 nW.
	 * The result should be < 2^52 to avoid overflowing the return value.
	 */
	const u64 v3c_big = (u64) c * (u64) v3;

	/* Range: 0 < v3c_big / 1000000 < 2^13 mW */
	return v3c_big / 1000000;
}

static struct kbase_ipa_model *get_current_model(struct kbase_device *kbdev)
{
	lockdep_assert_held(&kbdev->ipa.lock);

	if (atomic_read(&kbdev->ipa_use_configured_model))
		return kbdev->ipa.configured_model;
	else
		return kbdev->ipa.fallback_model;
}

#ifdef CONFIG_MALI_PWRSOFT_765
static unsigned long kbase_get_static_power(struct devfreq *df,
					    unsigned long voltage)
#else
static unsigned long kbase_get_static_power(unsigned long voltage)
#endif
{
	struct kbase_ipa_model *model;
	unsigned long power_coeff = 0, power = 0;
#ifdef CONFIG_MALI_PWRSOFT_765
	struct kbase_device *kbdev = dev_get_drvdata(&df->dev);
#else
	struct kbase_device *kbdev = kbase_find_device(-1);
#endif

	mutex_lock(&kbdev->ipa.lock);

	model = get_current_model(kbdev);

	if (model) {
		power_coeff = model->ops->get_static_power(model);
		power = kbase_scale_static_power(power_coeff, voltage);
	} else {
		dev_err(kbdev->dev, "%s: No current IPA model set", __func__);
	}

	kbdev->ipa.last_static_power_coeff = power_coeff;

	mutex_unlock(&kbdev->ipa.lock);

#ifndef CONFIG_MALI_PWRSOFT_765
	kbase_release_device(kbdev);
#endif

	return power;
}

#ifdef CONFIG_MALI_PWRSOFT_765
static unsigned long kbase_get_dynamic_power(struct devfreq *df,
					     unsigned long freq,
					     unsigned long voltage)
#else
static unsigned long kbase_get_dynamic_power(unsigned long freq,
					     unsigned long voltage)
#endif
{
	struct kbase_ipa_model *model;
	unsigned long power_coeff = 0, power = 0;
#ifdef CONFIG_MALI_PWRSOFT_765
	struct kbase_device *kbdev = dev_get_drvdata(&df->dev);
#else
	struct kbase_device *kbdev = kbase_find_device(-1);
#endif

	mutex_lock(&kbdev->ipa.lock);

	model = get_current_model(kbdev);

	if (model) {
		power_coeff = model->ops->get_dynamic_power(model);
		power = kbase_scale_dynamic_power(power_coeff, freq, voltage);
	} else {
		dev_err(kbdev->dev, "%s: No current IPA model set", __func__);
	}

	kbdev->ipa.last_model_dyn_power_coeff = power_coeff;

	mutex_unlock(&kbdev->ipa.lock);

#ifndef CONFIG_MALI_PWRSOFT_765
	kbase_release_device(kbdev);
#endif

	return power;
}

unsigned long kbase_power_to_state(struct devfreq *df, u32 target_power)
{
	struct kbase_device *kbdev = dev_get_drvdata(&df->dev);
	struct device *dev = df->dev.parent;
	unsigned long i, state = -1;
	unsigned long dyn_coeff, static_coeff;

	mutex_lock(&kbdev->ipa.lock);

	dyn_coeff = kbdev->ipa.last_model_dyn_power_coeff;
	static_coeff = kbdev->ipa.last_static_power_coeff;

	mutex_unlock(&kbdev->ipa.lock);

	/* OPPs are sorted from highest frequency to lowest */
	for (i = 0; i < df->profile->max_state - 1; i++) {
		struct dev_pm_opp *opp;
		unsigned int freq;
		unsigned long dyn_power, static_power, voltage;

		freq = df->profile->freq_table[i]; /* Hz */

		rcu_read_lock();
		opp = dev_pm_opp_find_freq_exact(dev, freq, true);
		/* Allow unavailable frequencies too in case we can enable a
		 * higher one. */
		if (PTR_ERR(opp) == -ERANGE)
			opp = dev_pm_opp_find_freq_exact(dev, freq, false);

		if (IS_ERR(opp)) {
			rcu_read_unlock();
			return PTR_ERR(opp);
		}

		voltage = dev_pm_opp_get_voltage(opp) / 1000; /* mV */
		rcu_read_unlock();

		dyn_power = kbase_scale_dynamic_power(dyn_coeff, freq, voltage);
		static_power = kbase_scale_static_power(static_coeff, voltage);

		if (target_power >= dyn_power + static_power)
			break;
	}
	state = i;

	return state;
}
KBASE_EXPORT_TEST_API(kbase_power_to_state);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
struct devfreq_cooling_ops power_model_ops = {
#else
struct devfreq_cooling_power power_model_ops = {
#endif
	.get_static_power = &kbase_get_static_power,
	.get_dynamic_power = &kbase_get_dynamic_power,
#ifdef CONFIG_MALI_PWRSOFT_765
	.power2state = &kbase_power_to_state,
#endif
};

unsigned long kbase_ipa_dynamic_power(struct kbase_device *kbdev,
					     unsigned long freq,
					     unsigned long voltage)
{
#ifdef CONFIG_MALI_PWRSOFT_765
	struct devfreq *df = kbdev->devfreq;

	return kbase_get_dynamic_power(df, freq, voltage);
#else
	return kbase_get_dynamic_power(freq, voltage);
#endif
}
KBASE_EXPORT_TEST_API(kbase_ipa_dynamic_power);

unsigned long kbase_ipa_static_power(struct kbase_device *kbdev,
				     unsigned long voltage)
{
#ifdef CONFIG_MALI_PWRSOFT_765
	struct devfreq *df = kbdev->devfreq;

	return kbase_get_static_power(df, voltage);
#else
	return kbase_get_static_power(voltage);
#endif
}
KBASE_EXPORT_TEST_API(kbase_ipa_static_power);
