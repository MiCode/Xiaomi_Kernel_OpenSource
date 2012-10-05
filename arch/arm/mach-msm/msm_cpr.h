/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_MSM_CPR_H
#define __ARCH_ARM_MACH_MSM_CPR_H

/* Register Offsets for RBCPR */

/* RBCPR Gate Count and Target Registers */
#define RBCPR_GCNT_TARGET(n)	(0x60 + 4 * n)

/* RBCPR Timer Control */
#define RBCPR_TIMER_INTERVAL	0x44
#define RBIF_TIMER_ADJUST	0x4C

/* RBCPR Config Register */
#define RBIF_LIMIT		0x48
#define RBCPR_STEP_QUOT		0X80
#define RBCPR_CTL		0x90
#define RBIF_SW_VLEVEL		0x94
#define RBIF_CONT_ACK_CMD	0x98
#define RBIF_CONT_NACK_CMD	0x9C

/* RBCPR Result status Register */
#define RBCPR_RESULT_0		0xA0
#define RBCPR_RESULT_1		0xA4
#define RBCPR_QUOT_AVG		0x118

/* RBCPR DEBUG Register */
#define RBCPR_DEBUG1		0x120

/* RBCPR Interrupt Control Register */
#define RBIF_IRQ_EN(n)		(0x100 + 4 * n)
#define RBIF_IRQ_CLEAR		0x110
#define RBIF_IRQ_STATUS		0x114

/* Bit Mask Values */
#define GCNT_M				0x003FF000
#define TARGET_M			0x00000FFF
#define SW_VLEVEL_M			0x0000003F
#define UP_FLAG_M			0x00000010
#define DOWN_FLAG_M			0x00000004
#define CEILING_M			0x00000FC0
#define FLOOR_M				0x0000003F
#define LOOP_EN_M			0x00000001
#define TIMER_M				0x00000008
#define SW_AUTO_CONT_ACK_EN_M		0x00000020
#define SW_AUTO_CONT_NACK_DN_EN_M	0x00000040
#define HW_TO_PMIC_EN_M			BIT(4)
#define BUSY_M				BIT(19)
#define QUOT_SLOW_M			0x00FFF000
#define UP_THRESHOLD_M			0x0F000000
#define DN_THRESHOLD_M			0xF0000000

/* Bit Values */
#define ENABLE_CPR		BIT(0)
#define DISABLE_CPR		0x0
#define ENABLE_TIMER		BIT(3)
#define DISABLE_TIMER		0x0
#define SW_MODE			0x0
#define SW_AUTO_CONT_ACK_EN	BIT(5)
#define SW_AUTO_CONT_NACK_DN_EN	BIT(6)

/* Shift Values */
#define RBIF_CONS_DN_SHIFT (0x4)

/* Test values for RBCPR RUMI Testing */
#define GNT_CNT			0xC0
#define TARGET			0xEFF

#define CEILING_V		0x30
#define FLOOR_V			0x15

#define SW_LEVEL		0x20

/* Interrupt Mask for All interrupt flags */
#define INT_MASK (MIN_INT | DOWN_INT | MID_INT | UP_INT | MAX_INT)

/* Number of oscilator in each sensor */
#define NUM_OSC 8

#define CPR_MODE 2

/**
 * enum cpr_mode - Modes in which cpr is used
 */
enum cpr_mode {
	NORMAL_MODE = 0,
	TURBO_MODE,
	SVS_MODE,
};

/**
 * enum cpr_action - Cpr actions to be taken
 */
enum cpr_action {
	DOWN = 0,
	UP,
};

/**
 * enum cpr_interrupt
 */
enum cpr_interrupt {
	DONE_INT	= BIT(0),
	MIN_INT		= BIT(1),
	DOWN_INT	= BIT(2),
	MID_INT		= BIT(3),
	UP_INT		= BIT(4),
	MAX_INT		= BIT(5),
};

/**
 * struct msm_vp_data - structure for VP configuration
 * @min_volt: minimum microvolt level for VP
 * @max_volt: maximum microvolt level for VP
 * @default_volt: default microvolt for VP
 * @step_size: step size of voltage in microvolt
 */
struct msm_cpr_vp_data {
	int min_volt;
	int max_volt;
	int default_volt;
	int step_size;
};

/**
 * struct msm_cpr_osc -  Data for CPR ring oscillator
 * @gcnt: gate count value for the oscillator
 * @quot: target value for ring oscillator
 */
struct msm_cpr_osc {
	int gcnt;
	uint32_t quot;
};

/**
 * struct msm_cpr_mode -  Data for CPR modes of operation
 * @msm_cpr_osc: structure for oscillator data
 * @ring_osc: ring oscillator of the sensor
 * @tgt_volt_offset: inital voltage offset from default value
 * @step_quot: step Quot for CPR calcuation
 */
struct msm_cpr_mode {
	struct msm_cpr_osc ring_osc_data[NUM_OSC];
	int ring_osc;
	int32_t tgt_volt_offset;
	uint32_t step_quot;
	uint32_t turbo_Vmax;
	uint32_t turbo_Vmin;
	uint32_t nom_Vmax;
	uint32_t nom_Vmin;
	uint32_t calibrated_uV;
};

/**
 * struct msm_cpr_config -  Platform data for CPR configuration
 * @ref_clk_khz: clock value of CPR in KHz
 * @delay_us: timer delay in micro second
 * @irq_line: irq line to be use (0 or 1 or 2)
 * @msm_cpr_mode: structure for CPR mode data
 */
struct msm_cpr_config {
	unsigned long ref_clk_khz;
	unsigned long delay_us;
	int irq_line;
	struct msm_cpr_mode *cpr_mode_data;
	int min_down_step;
	uint32_t tgt_count_div_N; /* Target Cnt(Nom) = Target Cnt(Turbo) / N */
	uint32_t floor;
	uint32_t ceiling;
	uint32_t sw_vlevel;
	uint32_t up_threshold;
	uint32_t dn_threshold;
	uint32_t up_margin;
	uint32_t dn_margin;
	uint32_t max_nom_freq;
	uint32_t max_freq;
	uint32_t max_quot;
	struct msm_cpr_vp_data *vp_data;
	uint32_t (*get_quot)(uint32_t max_quot, uint32_t max_freq,
				uint32_t new_freq);
	void (*clk_enable)(void);
};

/**
* struct msm_cpr_config -  CPR Registers
*/
struct msm_cpr_reg {
	uint32_t rbif_timer_interval;
	uint32_t rbif_int_en;
	uint32_t rbif_limit;
	uint32_t rbif_timer_adjust;
	uint32_t rbcpr_gcnt_target;
	uint32_t rbcpr_step_quot;
	uint32_t rbif_sw_level;
	uint32_t rbcpr_ctl;
};

#if defined(CONFIG_MSM_CPR) || defined(CONFIG_MSM_CPR_MODULE)
/* msm_cpr_pm_resume: Used by Power Manager for Idle Power Collapse */
void msm_cpr_pm_resume(void);
/* msm_cpr_pm_suspend: Used by Power Manager for Idle Power Collapse */
void msm_cpr_pm_suspend(void);
/* msm_cpr_enable: Used by Power Manager for GDFS */
void msm_cpr_enable(void);
/* msm_cpr_disable: Used by Power Manager for GDFS */
void msm_cpr_disable(void);
#else
/* msm_cpr_pm_resume: Used by Power Manager for Idle Power Collapse */
void msm_cpr_pm_resume(void) { }
/* msm_cpr_pm_suspend: Used by Power Manager for Idle Power Collapse */
void msm_cpr_pm_suspend(void) { }
/* msm_cpr_enable: Used by Power Manager for GDFS */
void msm_cpr_enable(void) { }
/* msm_cpr_disable: Used by Power Manager for GDFS */
void msm_cpr_disable(void) { }
#endif

#ifdef CONFIG_DEBUG_FS
int msm_cpr_debug_init(void *);
#else
static inline int msm_cpr_debug_init(void *) { return 0; }
#endif
#endif /* __ARCH_ARM_MACH_MSM_CPR_H */
