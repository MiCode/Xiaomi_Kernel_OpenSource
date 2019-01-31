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



#ifndef _KBASE_IPA_H_
#define _KBASE_IPA_H_

struct kbase_ipa_model {
	struct list_head link;
	struct kbase_device *kbdev;
	void *model_data;
	struct kbase_ipa_model_ops *ops;
	struct list_head params;
};

int kbase_ipa_model_add_param_u32_def(struct kbase_ipa_model *model,
				  const char *name, u32 *addr,
				  bool has_default, u32 default_value);

int kbase_ipa_model_add_param_s32_array(struct kbase_ipa_model *model,
					const char *name, s32 *addr,
					size_t num_elems);

int kbase_ipa_model_add_param_string(struct kbase_ipa_model *model,
				     const char *name, char *addr,
				     size_t len);

#define kbase_ipa_model_add_param_u32(MODEL, NAME, ADDR) \
		kbase_ipa_model_add_param_u32_def((MODEL), (NAME), (ADDR), \
						  false, 0)

struct kbase_ipa_model_ops {
	char *name;
	/* The init, recalculate and term ops on the default model are always
	 * called.  However, all the other models are only invoked if the model
	 * is selected in the device tree. Otherwise they are never
	 * initialized. Additional resources can be acquired by models in
	 * init(), however they must be terminated in the term().
	 */
	int (*init)(struct kbase_ipa_model *model);
	/* Called immediately after init(), or when a parameter is changed, so
	 * that any coefficients derived from model parameters can be
	 * recalculated. */
	int (*recalculate)(struct kbase_ipa_model *model);
	void (*term)(struct kbase_ipa_model *model);
	/* get_dynamic_power() - return a coefficient with units pW/(Hz V^2),
	 * which is scaled by the IPA framework according to the current OPP's
	 * frequency and voltage. */
	unsigned long (*get_dynamic_power)(struct kbase_ipa_model *model);
	/* get_static_power() - return a coefficient with units uW/(V^3),
	 * which is scaled by the IPA framework according to the current OPP's
	 * voltage. */
	unsigned long (*get_static_power)(struct kbase_ipa_model *model);
};

/* Models can be registered only in the platform's platform_init_func call */
int kbase_ipa_model_ops_register(struct kbase_device *kbdev,
			     struct kbase_ipa_model_ops *new_model_ops);

int kbase_ipa_init(struct kbase_device *kbdev);
void kbase_ipa_term(struct kbase_device *kbdev);
void kbase_ipa_model_use_fallback_locked(struct kbase_device *kbdev);
void kbase_ipa_model_use_configured_locked(struct kbase_device *kbdev);
int kbase_ipa_model_recalculate(struct kbase_ipa_model *model);

extern struct kbase_ipa_model_ops kbase_simple_ipa_model_ops;

/**
 * kbase_ipa_dynamic_power() - Calculate dynamic power component
 * @kbdev:      Pointer to kbase device.
 * @freq:       Frequency, in Hz.
 * @voltage:    Voltage, in mV.
 *
 * Return:      Dynamic power consumption, in mW.
 */
unsigned long kbase_ipa_dynamic_power(struct kbase_device *kbdev,
				      unsigned long freq,
				      unsigned long voltage);

/**
 * kbase_ipa_static_power() - Calculate static power component
 * @kbdev:      Pointer to kbase device.
 * @voltage:    Voltage, in mV.
 *
 * Return:      Static power consumption, in mW.
 */
unsigned long kbase_ipa_static_power(struct kbase_device *kbdev,
				     unsigned long voltage);

/**
 * kbase_power_to_state() - Find the OPP which consumes target_power
 * @df:                 Pointer to devfreq device.
 * @target_power:       Maximum power consumption, in mW.
 *
 * Return: The index of the most performant OPP whose power consumption is less
 *         than target_power.
 */
unsigned long kbase_power_to_state(struct devfreq *df, u32 target_power);

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 4, 0)
extern struct devfreq_cooling_ops power_model_ops;
#else
extern struct devfreq_cooling_power power_model_ops;
#endif

#endif
