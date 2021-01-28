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
#include "mali_kbase.h"

/* CSHW counter block offsets */
#define GPU_ACTIVE (4)

/* MEMSYS counter block offsets */
#define MEMSYS_L2_ANY_LOOKUP (25)

/* SC counter block offsets */
#define SC_EXEC_INSTR_FMA          (27)
#define SC_EXEC_INSTR_MSG          (30)
#define SC_TEX_FILT_NUM_OPERATIONS (39)

/**
 * get_active_cycles() - return the GPU_ACTIVE counter
 * @model_data:          Pointer to GPU model data.
 *
 * Return: the number of cycles the GPU was active during the counter sampling
 * period.
 */
static u32 kbase_csf_get_active_cycles(
	struct kbase_ipa_counter_model_data *model_data)
{
	size_t i;

	for (i = 0; i < model_data->counters_def_num; ++i) {
		const struct kbase_ipa_counter *counter =
			&model_data->counters_def[i];

		if (!strcmp(counter->name, "gpu_active"))
			return model_data->counter_values[i];
	}

	WARN_ON_ONCE(1);

	return 0;
}

/** Table of description of HW counters used by IPA counter model.
 *
 * This table provides a description of each performance counter
 * used by the IPA counter model for energy estimation.
 */
static const struct kbase_ipa_counter ipa_counters_def_todx[] = {
	{
		.name = "l2_access",
		.coeff_default_value = 599800,
		.counter_block_offset = MEMSYS_L2_ANY_LOOKUP,
		.counter_block_type = KBASE_IPA_CORE_TYPE_MEMSYS,
	},
	{
		.name = "exec_instr_msg",
		.coeff_default_value = 1830200,
		.counter_block_offset = SC_EXEC_INSTR_MSG,
		.counter_block_type = KBASE_IPA_CORE_TYPE_SHADER,
	},
	{
		.name = "exec_instr_fma",
		.coeff_default_value = 407300,
		.counter_block_offset = SC_EXEC_INSTR_FMA,
		.counter_block_type = KBASE_IPA_CORE_TYPE_SHADER,
	},
	{
		.name = "tex_filt_num_operations",
		.coeff_default_value = 224500,
		.counter_block_offset = SC_TEX_FILT_NUM_OPERATIONS,
		.counter_block_type = KBASE_IPA_CORE_TYPE_SHADER,
	},
	{
		.name = "gpu_active",
		.coeff_default_value = 153800,
		.counter_block_offset = GPU_ACTIVE,
		.counter_block_type = KBASE_IPA_CORE_TYPE_CSHW,
	},
};

#define IPA_POWER_MODEL_OPS(gpu, init_token) \
	const struct kbase_ipa_model_ops kbase_ ## gpu ## _ipa_model_ops = { \
		.name = "mali-" #gpu "-power-model", \
		.init = kbase_ ## init_token ## _power_model_init, \
		.term = kbase_ipa_counter_common_model_term, \
		.get_dynamic_coeff = kbase_ipa_counter_dynamic_coeff, \
	}; \
	KBASE_EXPORT_TEST_API(kbase_ ## gpu ## _ipa_model_ops)

#define STANDARD_POWER_MODEL(gpu, reference_voltage) \
	static int kbase_ ## gpu ## _power_model_init(\
			struct kbase_ipa_model *model) \
	{ \
		BUILD_BUG_ON(ARRAY_SIZE(ipa_counters_def_ ## gpu) > \
				KBASE_IPA_MAX_COUNTER_DEF_NUM); \
		return kbase_ipa_counter_common_model_init(model, \
				ipa_counters_def_ ## gpu, \
				ARRAY_SIZE(ipa_counters_def_ ## gpu), \
				kbase_csf_get_active_cycles, \
				(reference_voltage)); \
	} \
	IPA_POWER_MODEL_OPS(gpu, gpu)


#define ALIAS_POWER_MODEL(gpu, as_gpu) \
	IPA_POWER_MODEL_OPS(gpu, as_gpu)

/* Currently tBEx energy model is being used, for which reference voltage
 * value is 1000 mV.
 */
STANDARD_POWER_MODEL(todx, 1000);

/* Assuming LODX is an alias of TODX for IPA */
ALIAS_POWER_MODEL(lodx, todx);

static const struct kbase_ipa_model_ops *ipa_counter_model_ops[] = {
	&kbase_todx_ipa_model_ops,
	&kbase_lodx_ipa_model_ops
};

const struct kbase_ipa_model_ops *kbase_ipa_counter_model_ops_find(
		struct kbase_device *kbdev, const char *name)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(ipa_counter_model_ops); ++i) {
		const struct kbase_ipa_model_ops *ops =
			ipa_counter_model_ops[i];

		if (!strcmp(ops->name, name))
			return ops;
	}

	dev_err(kbdev->dev, "power model \'%s\' not found\n", name);

	return NULL;
}

const char *kbase_ipa_counter_model_name_from_id(u32 gpu_id)
{
	const u32 prod_id = (gpu_id & GPU_ID_VERSION_PRODUCT_ID) >>
			GPU_ID_VERSION_PRODUCT_ID_SHIFT;

	switch (GPU_ID2_MODEL_MATCH_VALUE(prod_id)) {
	case GPU_ID2_PRODUCT_TODX:
		return "mali-todx-power-model";
	case GPU_ID2_PRODUCT_LODX:
		return "mali-lodx-power-model";
	default:
		return NULL;
	}
}