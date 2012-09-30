/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
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
	MSM_SPM_MODE_POWER_RETENTION,
	MSM_SPM_MODE_POWER_COLLAPSE,
	MSM_SPM_MODE_NR
};

enum {
	MSM_SPM_L2_MODE_DISABLED = MSM_SPM_MODE_DISABLED,
	MSM_SPM_L2_MODE_RETENTION,
	MSM_SPM_L2_MODE_GDHS,
	MSM_SPM_L2_MODE_POWER_COLLAPSE,
};

#if defined(CONFIG_MSM_SPM_V1)

enum {
	MSM_SPM_REG_SAW_AVS_CTL,
	MSM_SPM_REG_SAW_CFG,
	MSM_SPM_REG_SAW_SPM_CTL,
	MSM_SPM_REG_SAW_SPM_SLP_TMR_DLY,
	MSM_SPM_REG_SAW_SPM_WAKE_TMR_DLY,
	MSM_SPM_REG_SAW_SLP_CLK_EN,
	MSM_SPM_REG_SAW_SLP_HSFS_PRECLMP_EN,
	MSM_SPM_REG_SAW_SLP_HSFS_POSTCLMP_EN,
	MSM_SPM_REG_SAW_SLP_CLMP_EN,
	MSM_SPM_REG_SAW_SLP_RST_EN,
	MSM_SPM_REG_SAW_SPM_MPM_CFG,
	MSM_SPM_REG_NR_INITIALIZE,

	MSM_SPM_REG_SAW_VCTL = MSM_SPM_REG_NR_INITIALIZE,
	MSM_SPM_REG_SAW_STS,
	MSM_SPM_REG_SAW_SPM_PMIC_CTL,
	MSM_SPM_REG_NR
};

struct msm_spm_platform_data {
	void __iomem *reg_base_addr;
	uint32_t reg_init_values[MSM_SPM_REG_NR_INITIALIZE];

	uint8_t awake_vlevel;
	uint8_t retention_vlevel;
	uint8_t collapse_vlevel;
	uint8_t retention_mid_vlevel;
	uint8_t collapse_mid_vlevel;

	uint32_t vctl_timeout_us;
};

#elif defined(CONFIG_MSM_SPM_V2)

enum {
	MSM_SPM_REG_SAW2_CFG,
	MSM_SPM_REG_SAW2_AVS_CTL,
	MSM_SPM_REG_SAW2_AVS_HYSTERESIS,
	MSM_SPM_REG_SAW2_SPM_CTL,
	MSM_SPM_REG_SAW2_PMIC_DLY,
	MSM_SPM_REG_SAW2_AVS_LIMIT,
	MSM_SPM_REG_SAW2_AVS_DLY,
	MSM_SPM_REG_SAW2_SPM_DLY,
	MSM_SPM_REG_SAW2_PMIC_DATA_0,
	MSM_SPM_REG_SAW2_PMIC_DATA_1,
	MSM_SPM_REG_SAW2_PMIC_DATA_2,
	MSM_SPM_REG_SAW2_PMIC_DATA_3,
	MSM_SPM_REG_SAW2_PMIC_DATA_4,
	MSM_SPM_REG_SAW2_PMIC_DATA_5,
	MSM_SPM_REG_SAW2_PMIC_DATA_6,
	MSM_SPM_REG_SAW2_PMIC_DATA_7,
	MSM_SPM_REG_SAW2_RST,

	MSM_SPM_REG_NR_INITIALIZE = MSM_SPM_REG_SAW2_RST,

	MSM_SPM_REG_SAW2_ID,
	MSM_SPM_REG_SAW2_SECURE,
	MSM_SPM_REG_SAW2_STS0,
	MSM_SPM_REG_SAW2_STS1,
	MSM_SPM_REG_SAW2_VCTL,
	MSM_SPM_REG_SAW2_SEQ_ENTRY,
	MSM_SPM_REG_SAW2_SPM_STS,
	MSM_SPM_REG_SAW2_AVS_STS,
	MSM_SPM_REG_SAW2_PMIC_STS,
	MSM_SPM_REG_SAW2_VERSION,

	MSM_SPM_REG_NR,
};

struct msm_spm_seq_entry {
	uint32_t mode;
	uint8_t *cmd;
	bool  notify_rpm;
};

struct msm_spm_platform_data {
	void __iomem *reg_base_addr;
	uint32_t reg_init_values[MSM_SPM_REG_NR_INITIALIZE];

	uint32_t ver_reg;
	uint32_t vctl_port;
	uint32_t phase_port;

	uint8_t awake_vlevel;
	uint32_t vctl_timeout_us;
	uint32_t avs_timeout_us;

	uint32_t num_modes;
	struct msm_spm_seq_entry *modes;
};
#endif

#if defined(CONFIG_MSM_SPM_V1) || defined(CONFIG_MSM_SPM_V2)

/* Public functions */

/**
 * msm_spm_set_low_power_mode() - Configure SPM start address for low power mode
 * @mode: SPM LPM mode to enter
 * @notify_rpm: Notify RPM in this mode
 */
int msm_spm_set_low_power_mode(unsigned int mode, bool notify_rpm);

/**
 * msm_spm_set_vdd(): Set core voltage
 * @cpu: core id
 * @vlevel: Encoded PMIC data.
 */
int msm_spm_set_vdd(unsigned int cpu, unsigned int vlevel);

/**
 * msm_spm_get_vdd(): Get core voltage
 * @cpu: core id
 * @return: Returns encoded PMIC data.
 */
unsigned int msm_spm_get_vdd(unsigned int cpu);

/**
 * msm_spm_turn_on_cpu_rail(): Power on cpu rail before turning on core
 * @cpu: core id
 */
int msm_spm_turn_on_cpu_rail(unsigned int cpu);

/* Internal low power management specific functions */

/**
 * msm_spm_reinit(): Reinitialize SPM registers
 */
void msm_spm_reinit(void);

/**
 * msm_spm_init(): Board initalization function
 * @data: platform specific SPM register configuration data
 * @nr_devs: Number of SPM devices being initialized
 */
int msm_spm_init(struct msm_spm_platform_data *data, int nr_devs);

/**
 * msm_spm_device_init(): Device tree initialization function
 */
int msm_spm_device_init(void);

#if defined(CONFIG_MSM_L2_SPM)

/* Public functions */

/**
 * msm_spm_l2_set_low_power_mode(): Configure L2 SPM start address
 *                                  for low power mode
 * @mode: SPM LPM mode to enter
 * @notify_rpm: Notify RPM in this mode
 */
int msm_spm_l2_set_low_power_mode(unsigned int mode, bool notify_rpm);

/**
 * msm_spm_apcs_set_vdd(): Set Apps processor core sub-system voltage
 * @vlevel: Encoded PMIC data.
 */
int msm_spm_apcs_set_vdd(unsigned int vlevel);

/**
 * msm_spm_apcs_set_phase(): Set number of SMPS phases.
 * phase_cnt: Number of phases to be set active
 */
int msm_spm_apcs_set_phase(unsigned int phase_cnt);

/* Internal low power management specific functions */

/**
 * msm_spm_l2_init(): Board initialization function
 * @data: SPM target specific register configuration
 */
int msm_spm_l2_init(struct msm_spm_platform_data *data);

/**
 * msm_spm_l2_reinit(): Reinitialize L2 SPM registers
 */
void msm_spm_l2_reinit(void);

#else

static inline int msm_spm_l2_set_low_power_mode(unsigned int mode,
		bool notify_rpm)
{
	return -ENOSYS;
}
static inline int msm_spm_l2_init(struct msm_spm_platform_data *data)
{
	return -ENOSYS;
}
static inline void msm_spm_l2_reinit(void)
{
	/* empty */
}

static inline int msm_spm_apcs_set_vdd(unsigned int vlevel)
{
	return -ENOSYS;
}

static inline int msm_spm_apcs_set_phase(unsigned int phase_cnt)
{
	return -ENOSYS;
}
#endif /* defined(CONFIG_MSM_L2_SPM) */
#else /* defined(CONFIG_MSM_SPM_V1) || defined(CONFIG_MSM_SPM_V2) */
static inline int msm_spm_set_low_power_mode(unsigned int mode, bool notify_rpm)
{
	return -ENOSYS;
}

static inline int msm_spm_set_vdd(unsigned int cpu, unsigned int vlevel)
{
	return -ENOSYS;
}

static inline unsigned int msm_spm_get_vdd(unsigned int cpu)
{
	return 0;
}

static inline void msm_spm_reinit(void)
{
	/* empty */
}

static inline int msm_spm_turn_on_cpu_rail(unsigned int cpu)
{
	return -ENOSYS;
}

static inline int msm_spm_device_init(void)
{
	return -ENOSYS;
}

#endif  /*defined(CONFIG_MSM_SPM_V1) || defined (CONFIG_MSM_SPM_V2) */
#endif  /* __ARCH_ARM_MACH_MSM_SPM_H */
