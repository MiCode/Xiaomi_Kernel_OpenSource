/* Copyright (c) 2010-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __ARCH_ARM_MACH_MSM_SPM_H
#define __ARCH_ARM_MACH_MSM_SPM_H

enum {
	MSM_SPM_MODE_DISABLED,
	MSM_SPM_MODE_CLOCK_GATING,
	MSM_SPM_MODE_RETENTION,
	MSM_SPM_MODE_GDHS,
	MSM_SPM_MODE_POWER_COLLAPSE,
	MSM_SPM_MODE_STANDALONE_POWER_COLLAPSE,
	MSM_SPM_MODE_FASTPC,
	MSM_SPM_MODE_NR
};

enum msm_spm_avs_irq {
	MSM_SPM_AVS_IRQ_MIN,
	MSM_SPM_AVS_IRQ_MAX,
};

struct msm_spm_device;
struct device_node;

#if defined(CONFIG_MSM_SPM)

int msm_spm_set_low_power_mode(unsigned int mode, bool notify_rpm);
int msm_spm_probe_done(void);
int msm_spm_set_vdd(unsigned int cpu, unsigned int vlevel);
int msm_spm_get_vdd(unsigned int cpu);
int msm_spm_turn_on_cpu_rail(struct device_node *l2ccc_node,
		unsigned int val, int cpu, int vctl_offset);
struct msm_spm_device *msm_spm_get_device_by_name(const char *name);
int msm_spm_config_low_power_mode(struct msm_spm_device *dev,
		unsigned int mode, bool notify_rpm);
int msm_spm_device_init(void);
bool msm_spm_is_mode_avail(unsigned int mode);
void msm_spm_dump_regs(unsigned int cpu);
int msm_spm_is_avs_enabled(unsigned int cpu);
int msm_spm_avs_enable(unsigned int cpu);
int msm_spm_avs_disable(unsigned int cpu);
int msm_spm_avs_set_limit(unsigned int cpu, uint32_t min_lvl,
		uint32_t max_lvl);
int msm_spm_avs_enable_irq(unsigned int cpu, enum msm_spm_avs_irq irq);
int msm_spm_avs_disable_irq(unsigned int cpu, enum msm_spm_avs_irq irq);
int msm_spm_avs_clear_irq(unsigned int cpu, enum msm_spm_avs_irq irq);

#if defined(CONFIG_MSM_L2_SPM)

/* Public functions */

int msm_spm_apcs_set_phase(int cpu, unsigned int phase_cnt);
int msm_spm_enable_fts_lpm(int cpu, uint32_t mode);

#else

static inline int msm_spm_apcs_set_phase(int cpu, unsigned int phase_cnt)
{
	return -ENOSYS;
}

static inline int msm_spm_enable_fts_lpm(int cpu, uint32_t mode)
{
	return -ENOSYS;
}
#endif /* defined(CONFIG_MSM_L2_SPM) */
#else /* defined(CONFIG_MSM_SPM) */
static inline int msm_spm_set_low_power_mode(unsigned int mode, bool notify_rpm)
{
	return -ENOSYS;
}

static inline int msm_spm_probe_done(void)
{
	return -ENOSYS;
}

static inline int msm_spm_set_vdd(unsigned int cpu, unsigned int vlevel)
{
	return -ENOSYS;
}

static inline int msm_spm_get_vdd(unsigned int cpu)
{
	return 0;
}

static inline int msm_spm_turn_on_cpu_rail(struct device_node *l2ccc_node,
		unsigned int val, int cpu, int vctl_offset)
{
	return -ENOSYS;
}

static inline int msm_spm_device_init(void)
{
	return -ENOSYS;
}

static inline void msm_spm_dump_regs(unsigned int cpu)
{
	return;
}

static inline int msm_spm_config_low_power_mode(struct msm_spm_device *dev,
		unsigned int mode, bool notify_rpm)
{
	return -ENODEV;
}
static inline struct msm_spm_device *msm_spm_get_device_by_name(const char *name)
{
	return NULL;
}

static inline bool msm_spm_is_mode_avail(unsigned int mode)
{
	return false;
}

static inline int msm_spm_avs_enable_irq(unsigned int cpu,
		enum msm_spm_avs_irq irq)
{
	return -ENOSYS;
}

static inline int msm_spm_avs_disable_irq(unsigned int cpu,
		enum msm_spm_avs_irq irq)
{
	return -ENOSYS;
}

static inline int msm_spm_avs_clear_irq(unsigned int cpu,
		enum msm_spm_avs_irq irq)
{
	return -ENOSYS;
}

#endif  /* defined (CONFIG_MSM_SPM) */
#endif  /* __ARCH_ARM_MACH_MSM_SPM_H */
