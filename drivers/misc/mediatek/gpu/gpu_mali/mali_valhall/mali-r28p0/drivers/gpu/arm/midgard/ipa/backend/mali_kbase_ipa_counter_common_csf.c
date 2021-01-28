/*
 *
 * (C) COPYRIGHT 2020 ARM Limited. All rights reserved.
 *
 * This program is free software and is provided to you under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation, and any use by you of this program is subject to the terms
 * of such GNU licence.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you can access it online at
 * http://www.gnu.org/licenses/gpl-2.0.html.
 *
 * SPDX-License-Identifier: GPL-2.0
 *
 */

#include "mali_kbase_ipa_counter_common_csf.h"
#include "ipa/mali_kbase_ipa_debugfs.h"

#define DEFAULT_SCALING_FACTOR 5

/* If the value of GPU_ACTIVE is below this, use the simple model
 * instead, to avoid extrapolating small amounts of counter data across
 * large sample periods.
 */
#define DEFAULT_MIN_SAMPLE_CYCLES 10000

static inline s64 kbase_ipa_add_saturate(s64 a, s64 b)
{
	s64 rtn;

	if (a > 0 && (S64_MAX - a) < b)
		rtn = S64_MAX;
	else if (a < 0 && (S64_MIN - a) > b)
		rtn = S64_MIN;
	else
		rtn = a + b;

	return rtn;
}

static s64 kbase_ipa_group_energy(s32 coeff, u64 counter_value)
{
	/* Range: 0 < counter_value < 2^27 */
	if (counter_value > U32_MAX)
		counter_value = U32_MAX;

	/* Range: -2^49 < ret < 2^49 */
	return counter_value * (s64)coeff;
}

/**
 * kbase_ipa_attach_ipa_control() - register with kbase_ipa_control
 * @model_data: Pointer to counter model data
 *
 * Register IPA counter model as a client of kbase_ipa_control, which
 * provides an interface to retreive the accumulated value of hardware
 * counters to calculate energy consumption.
 *
 * Return: 0 on success, or an error code.
 */
static int
kbase_ipa_attach_ipa_control(struct kbase_ipa_counter_model_data *model_data)
{
	struct kbase_device *kbdev = model_data->kbdev;
	struct kbase_ipa_control_perf_counter *perf_counters;
	size_t num_counters = model_data->counters_def_num;
	int err;
	size_t i;

	perf_counters =
		kcalloc(num_counters, sizeof(*perf_counters), GFP_KERNEL);

	if (!perf_counters) {
		dev_err(kbdev->dev,
			"Failed to allocate memory for perf_counters array");
		return -ENOMEM;
	}

	for (i = 0; i < num_counters; ++i) {
		const struct kbase_ipa_counter *counter =
			&model_data->counters_def[i];

		perf_counters[i].type = counter->counter_block_type;
		perf_counters[i].idx = counter->counter_block_offset;
		perf_counters[i].gpu_norm = false;
		perf_counters[i].scaling_factor = 1;
	}

	err = kbase_ipa_control_register(kbdev, perf_counters, num_counters,
					 &model_data->ipa_control_cli);
	if (err)
		dev_err(kbdev->dev,
			"Failed to register IPA with kbase_ipa_control");

	kfree(perf_counters);
	return err;
}

/**
 * kbase_ipa_detach_ipa_control() - De-register from kbase_ipa_control.
 * @model_data: Pointer to counter model data
 */
static void
kbase_ipa_detach_ipa_control(struct kbase_ipa_counter_model_data *model_data)
{
	if (model_data->ipa_control_cli) {
		kbase_ipa_control_unregister(model_data->kbdev,
					     model_data->ipa_control_cli);
		model_data->ipa_control_cli = NULL;
	}
}

int kbase_ipa_counter_dynamic_coeff(struct kbase_ipa_model *model, u32 *coeffp)
{
	struct kbase_ipa_counter_model_data *model_data =
		(struct kbase_ipa_counter_model_data *)model->model_data;
	s64 energy = 0;
	size_t i;
	u64 coeff = 0, coeff_mul = 0;
	u32 active_cycles;
	u64 ret;

	/* The last argument is supposed to be a pointer to the location that
	 * will store the time for which GPU has been in protected mode since
	 * last query. This can be passed as NULL as counter model itself will
	 * not be used when GPU enters protected mode, as IPA is supposed to
	 * switch to the simple power model.
	 */
	ret = kbase_ipa_control_query(model->kbdev, model_data->ipa_control_cli,
				      model_data->counter_values,
				      model_data->counters_def_num, NULL);
	if (WARN_ON(ret))
		return ret;

	/* Range: 0 (GPU not used at all), to the max sampling interval, say
	 * 1s, * max GPU frequency (GPU 100% utilized).
	 * 0 <= active_cycles <= 1 * ~2GHz
	 * 0 <= active_cycles < 2^31
	 */
	active_cycles = model_data->get_active_cycles(model_data);

	/* If the value of the active_cycles is less than the threshold, then
	 * return an error so that IPA framework can approximate using the
	 * cached simple model results instead. This may be more accurate
	 * than extrapolating using a very small counter dump.
	 */
	if (active_cycles < (u32)max(model_data->min_sample_cycles, 0))
		return -ENODATA;

	/* Range: 1 <= active_cycles < 2^31 */
	active_cycles = max(1u, active_cycles);

	/* Range of 'energy' is +/- 2^54 * number of IPA groups (~8), so around
	 * -2^57 < energy < 2^57
	 */
	for (i = 0; i < model_data->counters_def_num; i++) {
		s32 coeff = model_data->counter_coeffs[i];
		u64 counter_value = model_data->counter_values[i];
		s64 group_energy = kbase_ipa_group_energy(coeff, counter_value);

		energy = kbase_ipa_add_saturate(energy, group_energy);
	}

	/* Range: 0 <= coeff < 2^57 */
	if (energy > 0)
		coeff = energy;

	/* Range: 0 <= coeff < 2^57 (because active_cycles >= 1). However, this
	 * can be constrained further: Counter values can only be increased by
	 * a theoretical maximum of about 64k per clock cycle. Beyond this,
	 * we'd have to sample every 1ms to avoid them overflowing at the
	 * lowest clock frequency (say 100MHz). Therefore, we can write the
	 * range of 'coeff' in terms of active_cycles:
	 *
	 * coeff = SUM(coeffN * counterN * num_cores_for_counterN)
	 * coeff <= SUM(coeffN * counterN) * max_num_cores
	 * coeff <= num_IPA_groups * max_coeff * max_counter * max_num_cores
	 *       (substitute max_counter = 2^16 * active_cycles)
	 * coeff <= num_IPA_groups * max_coeff * 2^16 * active_cycles * max_num_cores
	 * coeff <=    2^3         *    2^22   * 2^16 * active_cycles * 2^5
	 * coeff <= 2^46 * active_cycles
	 *
	 * So after the division: 0 <= coeff <= 2^46
	 */
	coeff = div_u64(coeff, active_cycles);

	/* Not all models were derived at the same reference voltage. Voltage
	 * scaling is done by multiplying by V^2, so we need to *divide* by
	 * Vref^2 here.
	 * Range: 0 <= coeff <= 2^49
	 */
	coeff = div_u64(coeff * 1000, max(model_data->reference_voltage, 1));
	/* Range: 0 <= coeff <= 2^52 */
	coeff = div_u64(coeff * 1000, max(model_data->reference_voltage, 1));

	/* Scale by user-specified integer factor.
	 * Range: 0 <= coeff_mul < 2^57
	 */
	coeff_mul = coeff * model_data->scaling_factor;

	/* The power models have results with units
	 * mW/(MHz V^2), i.e. nW/(Hz V^2). With precision of 1/1000000, this
	 * becomes fW/(Hz V^2), which are the units of coeff_mul. However,
	 * kbase_scale_dynamic_power() expects units of pW/(Hz V^2), so divide
	 * by 1000.
	 * Range: 0 <= coeff_mul < 2^47
	 */
	coeff_mul = div_u64(coeff_mul, 1000u);

	/* Clamp to a sensible range - 2^16 gives about 14W at 400MHz/750mV */
	*coeffp = clamp(coeff_mul, (u64)0, (u64)1 << 16);
	return 0;
}

int kbase_ipa_counter_common_model_init(
	struct kbase_ipa_model *model,
	const struct kbase_ipa_counter *ipa_counters_def,
	size_t ipa_num_counters,
	kbase_ipa_get_active_cycles_callback get_active_cycles,
	s32 reference_voltage)
{
	int err = 0;
	size_t i;
	struct kbase_ipa_counter_model_data *model_data;

	if (!model || !ipa_counters_def || !ipa_num_counters ||
	    !get_active_cycles)
		return -EINVAL;

	model_data = kzalloc(sizeof(*model_data), GFP_KERNEL);
	if (!model_data)
		return -ENOMEM;

	model_data->kbdev = model->kbdev;
	model_data->counters_def = ipa_counters_def;
	model_data->counters_def_num = ipa_num_counters;
	model_data->get_active_cycles = get_active_cycles;

	model->model_data = (void *)model_data;

	for (i = 0; i < model_data->counters_def_num; ++i) {
		const struct kbase_ipa_counter *counter =
			&model_data->counters_def[i];

		model_data->counter_coeffs[i] = counter->coeff_default_value;
		err = kbase_ipa_model_add_param_s32(
			model, counter->name, &model_data->counter_coeffs[i], 1,
			false);
		if (err)
			goto exit;
	}

	model_data->scaling_factor = DEFAULT_SCALING_FACTOR;
	err = kbase_ipa_model_add_param_s32(
		model, "scale", &model_data->scaling_factor, 1, false);
	if (err)
		goto exit;

	model_data->min_sample_cycles = DEFAULT_MIN_SAMPLE_CYCLES;
	err = kbase_ipa_model_add_param_s32(model, "min_sample_cycles",
					    &model_data->min_sample_cycles, 1,
					    false);
	if (err)
		goto exit;

	model_data->reference_voltage = reference_voltage;
	err = kbase_ipa_model_add_param_s32(model, "reference_voltage",
					    &model_data->reference_voltage, 1,
					    false);
	if (err)
		goto exit;

	err = kbase_ipa_attach_ipa_control(model_data);

exit:
	if (err) {
		kbase_ipa_model_param_free_all(model);
		kfree(model_data);
	}
	return err;
}

void kbase_ipa_counter_common_model_term(struct kbase_ipa_model *model)
{
	struct kbase_ipa_counter_model_data *model_data =
		(struct kbase_ipa_counter_model_data *)model->model_data;

	kbase_ipa_detach_ipa_control(model_data);
	kfree(model_data);
}
