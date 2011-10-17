/* Copyright (c) 2009-2011, Code Aurora Forum. All rights reserved.
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
/*
 * Qualcomm PMIC8058 driver header file
 *
 */

#include <linux/irq.h>
#include <linux/mfd/core.h>

#define PM8058_GPIOS		40
#define PM8058_MPPS		12

#define PM8058_IRQ_BLOCK_BIT(block, bit) ((block) * 8 + (bit))

/* MPPs and GPIOs [0,N) */
#define PM8058_MPP_IRQ(base, mpp)	((base) + \
					PM8058_IRQ_BLOCK_BIT(16, (mpp)))
#define PM8058_GPIO_IRQ(base, gpio)	((base) + \
					PM8058_IRQ_BLOCK_BIT(24, (gpio)))

#define PM8058_KEYPAD_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(9, 2))
#define PM8058_KEYSTUCK_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(9, 3))

#define PM8058_VCP_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(1, 0))
#define PM8058_CHGILIM_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(1, 3))
#define PM8058_VBATDET_LOW_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(1, 4))
#define PM8058_BATT_REPLACE_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(1, 5))
#define PM8058_CHGINVAL_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(1, 6))
#define PM8058_CHGVAL_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(1, 7))
#define PM8058_CHG_END_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(2, 0))
#define PM8058_FASTCHG_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(2, 1))
#define PM8058_CHGSTATE_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(2, 3))
#define PM8058_AUTO_CHGFAIL_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(2, 4))
#define PM8058_AUTO_CHGDONE_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(2, 5))
#define PM8058_ATCFAIL_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(2, 6))
#define PM8058_ATC_DONE_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(2, 7))
#define PM8058_OVP_OK_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(3, 0))
#define PM8058_COARSE_DET_OVP_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(3, 1))
#define PM8058_VCPMAJOR_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(3, 2))
#define PM8058_CHG_GONE_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(3, 3))
#define PM8058_CHGTLIMIT_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(3, 4))
#define PM8058_CHGHOT_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(3, 5))
#define PM8058_BATTTEMP_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(3, 6))
#define PM8058_BATTCONNECT_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(3, 7))
#define PM8058_BATFET_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(5, 4))
#define PM8058_VBATDET_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(5, 5))
#define PM8058_VBAT_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(5, 6))

#define PM8058_CBLPWR_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(4, 3))

#define PM8058_PWRKEY_REL_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(6, 2))
#define PM8058_PWRKEY_PRESS_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(6, 3))
#define PM8058_SW_0_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(7, 1))
#define PM8058_IR_0_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(7, 0))
#define PM8058_SW_1_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(7, 3))
#define PM8058_IR_1_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(7, 2))
#define PM8058_SW_2_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(7, 5))
#define PM8058_IR_2_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(7, 4))
#define PM8058_RTC_IRQ(base) 		((base) + PM8058_IRQ_BLOCK_BIT(6, 5))
#define PM8058_RTC_ALARM_IRQ(base) 	((base) + PM8058_IRQ_BLOCK_BIT(4, 7))
#define PM8058_ADC_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(9, 4))
#define PM8058_TEMP_ALARM_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(6, 7))
#define PM8058_OSCHALT_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(4, 6))
#define PM8058_BATT_ALARM_IRQ(base)	((base) + PM8058_IRQ_BLOCK_BIT(5, 6))
#define PM8058_RESOUT_IRQ(base)		((base) + PM8058_IRQ_BLOCK_BIT(6, 4))

struct pm8058_chip;

struct pm8058_platform_data {
	/* This table is only needed for misc interrupts. */
	int		irq_base;
	int		irq;
	int 		(*init)(struct pm8058_chip *pm_chip);

	int		num_subdevs;
	struct mfd_cell *sub_devices;
	int		irq_trigger_flags;
	struct mfd_cell *charger_sub_device;
};

struct pm8058_gpio_platform_data {
	int	gpio_base;
	int	irq_base;
	int	(*init)(void);
};

/* GPIO parameters */
/* direction */
#define	PM_GPIO_DIR_OUT			0x01
#define	PM_GPIO_DIR_IN			0x02
#define	PM_GPIO_DIR_BOTH		(PM_GPIO_DIR_OUT | PM_GPIO_DIR_IN)

/* output_buffer */
#define	PM_GPIO_OUT_BUF_OPEN_DRAIN	1
#define	PM_GPIO_OUT_BUF_CMOS		0

/* pull */
#define	PM_GPIO_PULL_UP_30		0
#define	PM_GPIO_PULL_UP_1P5		1
#define	PM_GPIO_PULL_UP_31P5		2
#define	PM_GPIO_PULL_UP_1P5_30		3
#define	PM_GPIO_PULL_DN			4
#define	PM_GPIO_PULL_NO			5

/* vin_sel: Voltage Input Select */
#define	PM_GPIO_VIN_VPH			0
#define	PM_GPIO_VIN_BB			1
#define	PM_GPIO_VIN_S3			2
#define	PM_GPIO_VIN_L3			3
#define	PM_GPIO_VIN_L7			4
#define	PM_GPIO_VIN_L6			5
#define	PM_GPIO_VIN_L5			6
#define	PM_GPIO_VIN_L2			7

/* out_strength */
#define	PM_GPIO_STRENGTH_NO		0
#define	PM_GPIO_STRENGTH_HIGH		1
#define	PM_GPIO_STRENGTH_MED		2
#define	PM_GPIO_STRENGTH_LOW		3

/* function */
#define	PM_GPIO_FUNC_NORMAL		0
#define	PM_GPIO_FUNC_PAIRED		1
#define	PM_GPIO_FUNC_1			2
#define	PM_GPIO_FUNC_2			3
#define	PM_GPIO_DTEST1			4
#define	PM_GPIO_DTEST2			5
#define	PM_GPIO_DTEST3			6
#define	PM_GPIO_DTEST4			7

struct pm8058_gpio {
	int		direction;
	int		output_buffer;
	int		output_value;
	int		pull;
	int		vin_sel;	/* 0..7 */
	int		out_strength;
	int		function;
	int		inv_int_pol;	/* invert interrupt polarity */
	int		disable_pin;	/* disable pin and tri-state its pad */
};

struct pmic8058_charger_data {
	unsigned int max_source_current;
	int charger_type;
};

/* chip revision */
#define PM_8058_REV_1p0			0xE1
#define PM_8058_REV_2p0			0xE2
#define PM_8058_REV_2p1			0xE3

/* misc: control mask and flag */
#define	PM8058_UART_MUX_MASK		0x60

#define PM8058_UART_MUX_NO		0x0
#define PM8058_UART_MUX_1		0x20
#define PM8058_UART_MUX_2		0x40
#define PM8058_UART_MUX_3		0x60

enum pon_config{
	DISABLE_HARD_RESET = 0,
	SHUTDOWN_ON_HARD_RESET,
	RESTART_ON_HARD_RESET,
	MAX_PON_CONFIG,
};

enum pm8058_smpl_delay {
	PM8058_SMPL_DELAY_0p5,
	PM8058_SMPL_DELAY_1p0,
	PM8058_SMPL_DELAY_1p5,
	PM8058_SMPL_DELAY_2p0,
};

/* Note -do not call pm8058_read and pm8058_write in an atomic context */
int pm8058_read(struct pm8058_chip *pm_chip, u16 addr, u8 *values,
		unsigned int len);
int pm8058_write(struct pm8058_chip *pm_chip, u16 addr, u8 *values,
		 unsigned int len);

int pm8058_gpio_config(int gpio, struct pm8058_gpio *param);

int pm8058_rev(struct pm8058_chip *pm_chip);

int pm8058_irq_get_rt_status(struct pm8058_chip *pm_chip, int irq);

int pm8058_misc_control(struct pm8058_chip *pm_chip, int mask, int flag);

#ifdef CONFIG_PMIC8058
int pm8058_reset_pwr_off(int reset);
#else
static inline int pm8058_reset_pwr_off(int reset) { return 0; }
#endif


int pm8058_hard_reset_config(enum pon_config config);

/**
 * pm8058_smpl_control - enables/disables SMPL detection
 * @enable: 0 = shutdown PMIC on power loss, 1 = reset PMIC on power loss
 *
 * This function enables or disables the Sudden Momentary Power Loss detection
 * module.  If SMPL detection is enabled, then when a sufficiently long power
 * loss event occurs, the PMIC will automatically reset itself.  If SMPL
 * detection is disabled, then the PMIC will shutdown when power loss occurs.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_smpl_control(int enable);

/**
 * pm8058_smpl_set_delay - sets the SMPL detection time delay
 * @delay: enum value corresponding to delay time
 *
 * This function sets the time delay of the SMPL detection module.  If power
 * is reapplied within this interval, then the PMIC reset automatically.  The
 * SMPL detection module must be enabled for this delay time to take effect.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_smpl_set_delay(enum pm8058_smpl_delay delay);

/**
 * pm8058_watchdog_reset_control - enables/disables watchdog reset detection
 * @enable: 0 = shutdown when PS_HOLD goes low, 1 = reset when PS_HOLD goes low
 *
 * This function enables or disables the PMIC watchdog reset detection feature.
 * If watchdog reset detection is enabled, then the PMIC will reset itself
 * when PS_HOLD goes low.  If it is not enabled, then the PMIC will shutdown
 * when PS_HOLD goes low.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_watchdog_reset_control(int enable);

/**
 * pm8058_stay_on - enables stay_on feature
 *
 * PMIC stay-on feature allows PMIC to ignore MSM PS_HOLD=low
 * signal so that some special functions like debugging could be
 * performed.
 *
 * This feature should not be used in any product release.
 *
 * RETURNS: an appropriate -ERRNO error value on error, or zero for success.
 */
int pm8058_stay_on(void);
