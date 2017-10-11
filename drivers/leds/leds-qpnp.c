/* Copyright (c) 2012-2015, 2017, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/spmi.h>
#include <linux/platform_device.h>
#include <linux/qpnp/pwm.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>

#define WLED_MOD_EN_REG(base, n)	(base + 0x60 + n*0x10)
#define WLED_IDAC_DLY_REG(base, n)	(WLED_MOD_EN_REG(base, n) + 0x01)
#define WLED_FULL_SCALE_REG(base, n)	(WLED_IDAC_DLY_REG(base, n) + 0x01)
#define WLED_MOD_SRC_SEL_REG(base, n)	(WLED_FULL_SCALE_REG(base, n) + 0x01)

/* wled control registers */
#define WLED_OVP_INT_STATUS(base)		(base + 0x10)
#define WLED_BRIGHTNESS_CNTL_LSB(base, n)	(base + 0x40 + 2*n)
#define WLED_BRIGHTNESS_CNTL_MSB(base, n)	(base + 0x41 + 2*n)
#define WLED_MOD_CTRL_REG(base)			(base + 0x46)
#define WLED_SYNC_REG(base)			(base + 0x47)
#define WLED_FDBCK_CTRL_REG(base)		(base + 0x48)
#define WLED_SWITCHING_FREQ_REG(base)		(base + 0x4C)
#define WLED_OVP_CFG_REG(base)			(base + 0x4D)
#define WLED_BOOST_LIMIT_REG(base)		(base + 0x4E)
#define WLED_CURR_SINK_REG(base)		(base + 0x4F)
#define WLED_HIGH_POLE_CAP_REG(base)		(base + 0x58)
#define WLED_CURR_SINK_MASK		0xE0
#define WLED_CURR_SINK_SHFT		0x05
#define WLED_DISABLE_ALL_SINKS		0x00
#define WLED_DISABLE_1_2_SINKS		0x80
#define WLED_SWITCH_FREQ_MASK		0x0F
#define WLED_OVP_VAL_MASK		0x03
#define WLED_OVP_INT_MASK		0x02
#define WLED_OVP_VAL_BIT_SHFT		0x00
#define WLED_BOOST_LIMIT_MASK		0x07
#define WLED_BOOST_LIMIT_BIT_SHFT	0x00
#define WLED_BOOST_ON			0x80
#define WLED_BOOST_OFF			0x00
#define WLED_EN_MASK			0x80
#define WLED_NO_MASK			0x00
#define WLED_CP_SELECT_MAX		0x03
#define WLED_CP_SELECT_MASK		0x02
#define WLED_USE_EXT_GEN_MOD_SRC	0x01
#define WLED_CTL_DLY_STEP		200
#define WLED_CTL_DLY_MAX		1400
#define WLED_MAX_CURR			25
#define WLED_NO_CURRENT			0x00
#define WLED_OVP_DELAY			1000
#define WLED_OVP_DELAY_INT		200
#define WLED_OVP_DELAY_LOOP		100
#define WLED_MSB_MASK			0x0F
#define WLED_MAX_CURR_MASK		0x1F
#define WLED_OP_FDBCK_MASK		0x07
#define WLED_OP_FDBCK_BIT_SHFT		0x00
#define WLED_OP_FDBCK_DEFAULT		0x00

#define WLED_SET_ILIM_CODE		0x01

#define WLED_MAX_LEVEL			4095
#define WLED_8_BIT_MASK			0xFF
#define WLED_4_BIT_MASK			0x0F
#define WLED_8_BIT_SHFT			0x08
#define WLED_MAX_DUTY_CYCLE		0xFFF

#define WLED_SYNC_VAL			0x07
#define WLED_SYNC_RESET_VAL		0x00

#define PMIC_VER_8026			0x04
#define PMIC_VER_8941			0x01
#define PMIC_VERSION_REG		0x0105

#define WLED_DEFAULT_STRINGS		0x01
#define WLED_THREE_STRINGS		0x03
#define WLED_MAX_TRIES			5
#define WLED_DEFAULT_OVP_VAL		0x02
#define WLED_BOOST_LIM_DEFAULT		0x03
#define WLED_CP_SEL_DEFAULT		0x00
#define WLED_CTRL_DLY_DEFAULT		0x00
#define WLED_SWITCH_FREQ_DEFAULT	0x0B

#define FLASH_SAFETY_TIMER(base)	(base + 0x40)
#define FLASH_MAX_CURR(base)		(base + 0x41)
#define FLASH_LED_0_CURR(base)		(base + 0x42)
#define FLASH_LED_1_CURR(base)		(base + 0x43)
#define FLASH_CLAMP_CURR(base)		(base + 0x44)
#define FLASH_LED_TMR_CTRL(base)	(base + 0x48)
#define FLASH_HEADROOM(base)		(base + 0x4A)
#define FLASH_STARTUP_DELAY(base)	(base + 0x4B)
#define FLASH_MASK_ENABLE(base)		(base + 0x4C)
#define FLASH_VREG_OK_FORCE(base)	(base + 0x4F)
#define FLASH_ENABLE_CONTROL(base)	(base + 0x46)
#define FLASH_LED_STROBE_CTRL(base)	(base + 0x47)
#define FLASH_WATCHDOG_TMR(base)	(base + 0x49)
#define FLASH_FAULT_DETECT(base)	(base + 0x51)
#define FLASH_PERIPHERAL_SUBTYPE(base)	(base + 0x05)
#define FLASH_CURRENT_RAMP(base)	(base + 0x54)

#define FLASH_MAX_LEVEL			0x4F
#define TORCH_MAX_LEVEL			0x0F
#define	FLASH_NO_MASK			0x00

#define FLASH_MASK_1			0x20
#define FLASH_MASK_REG_MASK		0xE0
#define FLASH_HEADROOM_MASK		0x03
#define FLASH_SAFETY_TIMER_MASK		0x7F
#define FLASH_CURRENT_MASK		0xFF
#define FLASH_MAX_CURRENT_MASK		0x7F
#define FLASH_TMR_MASK			0x03
#define FLASH_TMR_WATCHDOG		0x03
#define FLASH_TMR_SAFETY		0x00
#define FLASH_FAULT_DETECT_MASK		0X80
#define FLASH_HW_VREG_OK		0x40
#define FLASH_SW_VREG_OK                0x80
#define FLASH_VREG_MASK			0xC0
#define FLASH_STARTUP_DLY_MASK		0x02
#define FLASH_CURRENT_RAMP_MASK		0xBF

#define FLASH_ENABLE_ALL		0xE0
#define FLASH_ENABLE_MODULE		0x80
#define FLASH_ENABLE_MODULE_MASK	0x80
#define FLASH_DISABLE_ALL		0x00
#define FLASH_ENABLE_MASK		0xE0
#define FLASH_ENABLE_LED_0		0xC0
#define FLASH_ENABLE_LED_1		0xA0
#define FLASH_INIT_MASK			0xE0
#define	FLASH_SELFCHECK_ENABLE		0x80
#define FLASH_WATCHDOG_MASK		0x1F
#define FLASH_RAMP_STEP_27US		0xBF

#define FLASH_HW_SW_STROBE_SEL_MASK	0x04
#define FLASH_STROBE_MASK		0xC7
#define FLASH_LED_0_OUTPUT		0x80
#define FLASH_LED_1_OUTPUT		0x40
#define FLASH_TORCH_OUTPUT		0xC0

#define FLASH_CURRENT_PRGM_MIN		1
#define FLASH_CURRENT_PRGM_SHIFT	1
#define FLASH_CURRENT_MAX		0x4F
#define FLASH_CURRENT_TORCH		0x07

#define FLASH_DURATION_200ms		0x13
#define TORCH_DURATION_12s		0x0A
#define FLASH_CLAMP_200mA		0x0F

#define FLASH_SUBTYPE_DUAL		0x01
#define FLASH_SUBTYPE_SINGLE		0x02

#define FLASH_RAMP_UP_DELAY_US		1000
#define FLASH_RAMP_DN_DELAY_US		2160

#define LED_TRIGGER_DEFAULT		"none"

#define RGB_LED_SRC_SEL(base)		(base + 0x45)
#define RGB_LED_EN_CTL(base)		(base + 0x46)
#define RGB_LED_ATC_CTL(base)		(base + 0x47)

#define RGB_MAX_LEVEL			LED_FULL
#define RGB_LED_ENABLE_RED		0x80
#define RGB_LED_ENABLE_GREEN		0x40
#define RGB_LED_ENABLE_BLUE		0x20
#define RGB_LED_SOURCE_VPH_PWR		0x01
#define RGB_LED_ENABLE_MASK		0xE0
#define RGB_LED_SRC_MASK		0x03
#define QPNP_LED_PWM_FLAGS	(PM_PWM_LUT_LOOP | PM_PWM_LUT_RAMP_UP)
#define QPNP_LUT_RAMP_STEP_DEFAULT	255
#define	PWM_LUT_MAX_SIZE		63
#define	PWM_GPLED_LUT_MAX_SIZE		31
#define RGB_LED_DISABLE			0x00

#define MPP_MAX_LEVEL			LED_FULL
#define LED_MPP_MODE_CTRL(base)		(base + 0x40)
#define LED_MPP_VIN_CTRL(base)		(base + 0x41)
#define LED_MPP_EN_CTRL(base)		(base + 0x46)
#define LED_MPP_SINK_CTRL(base)		(base + 0x4C)

#define LED_MPP_CURRENT_MIN		5
#define LED_MPP_CURRENT_MAX		40
#define LED_MPP_VIN_CTRL_DEFAULT	0
#define LED_MPP_CURRENT_PER_SETTING	5
#define LED_MPP_SOURCE_SEL_DEFAULT	LED_MPP_MODE_ENABLE

#define LED_MPP_SINK_MASK		0x07
#define LED_MPP_MODE_MASK		0x7F
#define LED_MPP_VIN_MASK		0x03
#define LED_MPP_EN_MASK			0x80
#define LED_MPP_SRC_MASK		0x0F
#define LED_MPP_MODE_CTRL_MASK		0x70

#define LED_MPP_MODE_SINK		(0x06 << 4)
#define LED_MPP_MODE_ENABLE		0x01
#define LED_MPP_MODE_OUTPUT		0x10
#define LED_MPP_MODE_DISABLE		0x00
#define LED_MPP_EN_ENABLE		0x80
#define LED_MPP_EN_DISABLE		0x00

#define MPP_SOURCE_DTEST1		0x08

#define GPIO_MAX_LEVEL			LED_FULL
#define LED_GPIO_MODE_CTRL(base)	(base + 0x40)
#define LED_GPIO_VIN_CTRL(base)		(base + 0x41)
#define LED_GPIO_EN_CTRL(base)		(base + 0x46)

#define LED_GPIO_VIN_CTRL_DEFAULT	0
#define LED_GPIO_SOURCE_SEL_DEFAULT	LED_GPIO_MODE_ENABLE

#define LED_GPIO_MODE_MASK		0x3F
#define LED_GPIO_VIN_MASK		0x0F
#define LED_GPIO_EN_MASK		0x80
#define LED_GPIO_SRC_MASK		0x0F
#define LED_GPIO_MODE_CTRL_MASK		0x30

#define LED_GPIO_MODE_ENABLE	0x01
#define LED_GPIO_MODE_DISABLE	0x00
#define LED_GPIO_MODE_OUTPUT		0x10
#define LED_GPIO_EN_ENABLE		0x80
#define LED_GPIO_EN_DISABLE		0x00

#define KPDBL_MAX_LEVEL			LED_FULL
#define KPDBL_ROW_SRC_SEL(base)		(base + 0x40)
#define KPDBL_ENABLE(base)		(base + 0x46)
#define KPDBL_ROW_SRC(base)		(base + 0xE5)

#define KPDBL_ROW_SRC_SEL_VAL_MASK	0x0F
#define KPDBL_ROW_SCAN_EN_MASK		0x80
#define KPDBL_ROW_SCAN_VAL_MASK		0x0F
#define KPDBL_ROW_SCAN_EN_SHIFT		7
#define KPDBL_MODULE_EN			0x80
#define KPDBL_MODULE_DIS		0x00
#define KPDBL_MODULE_EN_MASK		0x80
#define NUM_KPDBL_LEDS			4
#define KPDBL_MASTER_BIT_INDEX		0

/**
 * enum qpnp_leds - QPNP supported led ids
 * @QPNP_ID_WLED - White led backlight
 */
enum qpnp_leds {
	QPNP_ID_WLED = 0,
	QPNP_ID_FLASH1_LED0,
	QPNP_ID_FLASH1_LED1,
	QPNP_ID_RGB_RED,
	QPNP_ID_RGB_GREEN,
	QPNP_ID_RGB_BLUE,
	QPNP_ID_LED_MPP,
	QPNP_ID_KPDBL,
	QPNP_ID_LED_GPIO,
	QPNP_ID_MAX,
};

/* current boost limit */
enum wled_current_boost_limit {
	WLED_CURR_LIMIT_105mA,
	WLED_CURR_LIMIT_385mA,
	WLED_CURR_LIMIT_525mA,
	WLED_CURR_LIMIT_805mA,
	WLED_CURR_LIMIT_980mA,
	WLED_CURR_LIMIT_1260mA,
	WLED_CURR_LIMIT_1400mA,
	WLED_CURR_LIMIT_1680mA,
};

/* over voltage protection threshold */
enum wled_ovp_threshold {
	WLED_OVP_35V,
	WLED_OVP_32V,
	WLED_OVP_29V,
	WLED_OVP_27V,
};

enum flash_headroom {
	HEADROOM_250mV = 0,
	HEADROOM_300mV,
	HEADROOM_400mV,
	HEADROOM_500mV,
};

enum flash_startup_dly {
	DELAY_10us = 0,
	DELAY_32us,
	DELAY_64us,
	DELAY_128us,
};

enum led_mode {
	PWM_MODE = 0,
	LPG_MODE,
	MANUAL_MODE,
};

static u8 wled_debug_regs[] = {
	/* brightness registers */
	0x40, 0x41, 0x42, 0x43, 0x44, 0x45,
	/* common registers */
	0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f,
	0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59,
	/* LED1 */
	0x60, 0x61, 0x62, 0x63, 0x66,
	/* LED2 */
	0x70, 0x71, 0x72, 0x73, 0x76,
	/* LED3 */
	0x80, 0x81, 0x82, 0x83, 0x86,
};

static u8 flash_debug_regs[] = {
	0x40, 0x41, 0x42, 0x43, 0x44, 0x48, 0x49, 0x4b, 0x4c,
	0x4f, 0x46, 0x47,
};

static u8 rgb_pwm_debug_regs[] = {
	0x45, 0x46, 0x47,
};

static u8 mpp_debug_regs[] = {
	0x40, 0x41, 0x42, 0x45, 0x46, 0x4c,
};

static u8 kpdbl_debug_regs[] = {
	0x40, 0x46, 0xb1, 0xb3, 0xb4, 0xe5,
};

static u8 gpio_debug_regs[] = {
	0x40, 0x41, 0x42, 0x45, 0x46,
};

/**
 *  pwm_config_data - pwm configuration data
 *  @lut_params - lut parameters to be used by pwm driver
 *  @pwm_device - pwm device
 *  @pwm_period_us - period for pwm, in us
 *  @mode - mode the led operates in
 *  @old_duty_pcts - storage for duty pcts that may need to be reused
 *  @default_mode - default mode of LED as set in device tree
 *  @use_blink - use blink sysfs entry
 *  @blinking - device is currently blinking w/LPG mode
 */
struct pwm_config_data {
	struct lut_params	lut_params;
	struct pwm_device	*pwm_dev;
	u32			pwm_period_us;
	struct pwm_duty_cycles	*duty_cycles;
	int	*old_duty_pcts;
	u8	mode;
	u8	default_mode;
	bool	pwm_enabled;
	bool use_blink;
	bool blinking;
};

/**
 *  wled_config_data - wled configuration data
 *  @num_strings - number of wled strings to be configured
 *  @num_physical_strings - physical number of strings supported
 *  @ovp_val - over voltage protection threshold
 *  @boost_curr_lim - boot current limit
 *  @cp_select - high pole capacitance
 *  @ctrl_delay_us - delay in activation of led
 *  @dig_mod_gen_en - digital module generator
 *  @cs_out_en - current sink output enable
 *  @op_fdbck - selection of output as feedback for the boost
 */
struct wled_config_data {
	u8	num_strings;
	u8	num_physical_strings;
	u8	ovp_val;
	u8	boost_curr_lim;
	u8	cp_select;
	u8	ctrl_delay_us;
	u8	switch_freq;
	u8	op_fdbck;
	u8	pmic_version;
	bool	dig_mod_gen_en;
	bool	cs_out_en;
};

/**
 *  mpp_config_data - mpp configuration data
 *  @pwm_cfg - device pwm configuration
 *  @current_setting - current setting, 5ma-40ma in 5ma increments
 *  @source_sel - source selection
 *  @mode_ctrl - mode control
 *  @vin_ctrl - input control
 *  @min_brightness - minimum brightness supported
 *  @pwm_mode - pwm mode in use
 *  @max_uV - maximum regulator voltage
 *  @min_uV - minimum regulator voltage
 *  @mpp_reg - regulator to power mpp based LED
 *  @enable - flag indicating LED on or off
 */
struct mpp_config_data {
	struct pwm_config_data	*pwm_cfg;
	u8	current_setting;
	u8	source_sel;
	u8	mode_ctrl;
	u8	vin_ctrl;
	u8	min_brightness;
	u8 pwm_mode;
	u32	max_uV;
	u32	min_uV;
	struct regulator *mpp_reg;
	bool	enable;
};

/**
 *  flash_config_data - flash configuration data
 *  @current_prgm - current to be programmed, scaled by max level
 *  @clamp_curr - clamp current to use
 *  @headroom - headroom value to use
 *  @duration - duration of the flash
 *  @enable_module - enable address for particular flash
 *  @trigger_flash - trigger flash
 *  @startup_dly - startup delay for flash
 *  @strobe_type - select between sw and hw strobe
 *  @peripheral_subtype - module peripheral subtype
 *  @current_addr - address to write for current
 *  @second_addr - address of secondary flash to be written
 *  @safety_timer - enable safety timer or watchdog timer
 *  @torch_enable - enable flash LED torch mode
 *  @flash_reg_get - flash regulator attached or not
 *  @flash_wa_reg_get - workaround regulator attached or not
 *  @flash_on - flash status, on or off
 *  @torch_on - torch status, on or off
 *  @vreg_ok - specifies strobe type, sw or hw
 *  @no_smbb_support - specifies if smbb boost is not required and there is a
    single regulator for both flash and torch
 *  @flash_boost_reg - boost regulator for flash
 *  @torch_boost_reg - boost regulator for torch
 *  @flash_wa_reg - flash regulator for wa
 */
struct flash_config_data {
	u8	current_prgm;
	u8	clamp_curr;
	u8	headroom;
	u8	duration;
	u8	enable_module;
	u8	trigger_flash;
	u8	startup_dly;
	u8	strobe_type;
	u8	peripheral_subtype;
	u16	current_addr;
	u16	second_addr;
	bool	safety_timer;
	bool	torch_enable;
	bool	flash_reg_get;
	bool    flash_wa_reg_get;
	bool	flash_on;
	bool	torch_on;
	bool	vreg_ok;
	bool    no_smbb_support;
	struct regulator *flash_boost_reg;
	struct regulator *torch_boost_reg;
	struct regulator *flash_wa_reg;
};

/**
 *  kpdbl_config_data - kpdbl configuration data
 *  @pwm_cfg - device pwm configuration
 *  @mode - running mode: pwm or lut
 *  @row_id - row id of the led
 *  @row_src_vbst - 0 for vph_pwr and 1 for vbst
 *  @row_src_en - enable row source
 *  @always_on - always on row
 *  @lut_params - lut parameters to be used by pwm driver
 *  @duty_cycles - duty cycles for lut
 *  @pwm_mode - pwm mode in use
 */
struct kpdbl_config_data {
	struct pwm_config_data	*pwm_cfg;
	u32	row_id;
	bool	row_src_vbst;
	bool	row_src_en;
	bool	always_on;
	struct pwm_duty_cycles  *duty_cycles;
	struct lut_params	lut_params;
	u8	pwm_mode;
};

/**
 *  rgb_config_data - rgb configuration data
 *  @pwm_cfg - device pwm configuration
 *  @enable - bits to enable led
 */
struct rgb_config_data {
	struct pwm_config_data	*pwm_cfg;
	u8	enable;
};

/**
 *  gpio_config_data - gpio configuration data
 *  @source_sel - source selection
 *  @mode_ctrl - mode control
 *  @vin_ctrl - input control
 *  @enable - flag indicating LED on or off
 */
struct gpio_config_data {
	u8	source_sel;
	u8	mode_ctrl;
	u8	vin_ctrl;
	bool	enable;
};

/**
 * struct qpnp_led_data - internal led data structure
 * @led_classdev - led class device
 * @delayed_work - delayed work for turning off the LED
 * @workqueue - dedicated workqueue to handle concurrency
 * @work - workqueue for led
 * @id - led index
 * @base_reg - base register given in device tree
 * @lock - to protect the transactions
 * @reg - cached value of led register
 * @num_leds - number of leds in the module
 * @max_current - maximum current supported by LED
 * @default_on - true: default state max, false, default state 0
 * @turn_off_delay_ms - number of msec before turning off the LED
 */
struct qpnp_led_data {
	struct led_classdev		cdev;
	struct platform_device		*pdev;
	struct regmap			*regmap;
	struct delayed_work		dwork;
	struct workqueue_struct		*workqueue;
	struct work_struct		work;
	int				id;
	u16				base;
	u8				reg;
	u8				num_leds;
	struct mutex			lock;
	struct wled_config_data		*wled_cfg;
	struct flash_config_data	*flash_cfg;
	struct kpdbl_config_data	*kpdbl_cfg;
	struct rgb_config_data		*rgb_cfg;
	struct mpp_config_data		*mpp_cfg;
	struct gpio_config_data		*gpio_cfg;
	int				max_current;
	bool				default_on;
	bool				in_order_command_processing;
	int				turn_off_delay_ms;
};

static DEFINE_MUTEX(flash_lock);
static struct pwm_device *kpdbl_master;
static u32 kpdbl_master_period_us;
DECLARE_BITMAP(kpdbl_leds_in_use, NUM_KPDBL_LEDS);
static bool is_kpdbl_master_turn_on;

static int
qpnp_led_masked_write(struct qpnp_led_data *led, u16 addr, u8 mask, u8 val)
{
	int rc;

	rc = regmap_update_bits(led->regmap, addr, mask, val);
	if (rc)
		dev_err(&led->pdev->dev,
			"Unable to regmap_update_bits to addr=%x, rc(%d)\n",
			addr, rc);
	return rc;
}

static void qpnp_dump_regs(struct qpnp_led_data *led, u8 regs[], u8 array_size)
{
	int i;
	u8 val;

	pr_debug("===== %s LED register dump start =====\n", led->cdev.name);
	for (i = 0; i < array_size; i++) {
		regmap_bulk_read(led->regmap, led->base + regs[i], &val,
				 sizeof(val));
		pr_debug("%s: 0x%x = 0x%x\n", led->cdev.name,
					led->base + regs[i], val);
	}
	pr_debug("===== %s LED register dump end =====\n", led->cdev.name);
}

static int qpnp_wled_sync(struct qpnp_led_data *led)
{
	int rc;
	u8 val;

	/* sync */
	val = WLED_SYNC_VAL;
	rc = regmap_write(led->regmap, WLED_SYNC_REG(led->base), val);
	if (rc) {
		dev_err(&led->pdev->dev,
				"WLED set sync reg failed(%d)\n", rc);
		return rc;
	}

	val = WLED_SYNC_RESET_VAL;
	rc = regmap_write(led->regmap, WLED_SYNC_REG(led->base), val);
	if (rc) {
		dev_err(&led->pdev->dev,
				"WLED reset sync reg failed(%d)\n", rc);
		return rc;
	}
	return 0;
}

static int qpnp_wled_set(struct qpnp_led_data *led)
{
	int rc, duty, level, tries = 0;
	u8 val, i, num_wled_strings;
	uint sink_val, ilim_val, ovp_val;

	num_wled_strings = led->wled_cfg->num_strings;

	level = led->cdev.brightness;

	if (level > WLED_MAX_LEVEL)
		level = WLED_MAX_LEVEL;
	if (level == 0) {
		for (i = 0; i < num_wled_strings; i++) {
			rc = qpnp_led_masked_write(led,
				WLED_FULL_SCALE_REG(led->base, i),
				WLED_MAX_CURR_MASK, WLED_NO_CURRENT);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Write max current failure (%d)\n",
					rc);
				return rc;
			}
		}

		rc = qpnp_wled_sync(led);
		if (rc) {
			dev_err(&led->pdev->dev,
				"WLED sync failed(%d)\n", rc);
			return rc;
		}

		rc = regmap_read(led->regmap, WLED_CURR_SINK_REG(led->base),
				 &sink_val);
		if (rc) {
			dev_err(&led->pdev->dev,
				"WLED read sink reg failed(%d)\n", rc);
			return rc;
		}

		if (led->wled_cfg->pmic_version == PMIC_VER_8026) {
			val = WLED_DISABLE_ALL_SINKS;
			rc = regmap_write(led->regmap,
					  WLED_CURR_SINK_REG(led->base), val);
			if (rc) {
				dev_err(&led->pdev->dev,
					"WLED write sink reg failed(%d)\n", rc);
				return rc;
			}

			usleep_range(WLED_OVP_DELAY, WLED_OVP_DELAY + 10);
		} else if (led->wled_cfg->pmic_version == PMIC_VER_8941) {
			if (led->wled_cfg->num_physical_strings <=
					WLED_THREE_STRINGS) {
				val = WLED_DISABLE_1_2_SINKS;
				rc = regmap_write(led->regmap,
						  WLED_CURR_SINK_REG(led->base),
						  val);
				if (rc) {
					dev_err(&led->pdev->dev,
						"WLED write sink reg failed");
					return rc;
				}

				rc = regmap_read(led->regmap,
					 WLED_BOOST_LIMIT_REG(led->base),
					 &ilim_val);
				if (rc) {
					dev_err(&led->pdev->dev,
						"Unable to read boost reg");
				}
				val = WLED_SET_ILIM_CODE;
				rc = regmap_write(led->regmap,
					  WLED_BOOST_LIMIT_REG(led->base),
					  val);
				if (rc) {
					dev_err(&led->pdev->dev,
						"WLED write sink reg failed");
					return rc;
				}
				usleep_range(WLED_OVP_DELAY,
					     WLED_OVP_DELAY + 10);
			} else {
				val = WLED_DISABLE_ALL_SINKS;
				rc = regmap_write(led->regmap,
						  WLED_CURR_SINK_REG(led->base),
						  val);
				if (rc) {
					dev_err(&led->pdev->dev,
						"WLED write sink reg failed");
					return rc;
				}

				msleep(WLED_OVP_DELAY_INT);
				while (tries < WLED_MAX_TRIES) {
					rc = regmap_read(led->regmap,
						 WLED_OVP_INT_STATUS(led->base),
						 &ovp_val);
					if (rc) {
						dev_err(&led->pdev->dev,
						"Unable to read boost reg");
					}

					if (ovp_val & WLED_OVP_INT_MASK)
						break;

					msleep(WLED_OVP_DELAY_LOOP);
					tries++;
				}
				usleep_range(WLED_OVP_DELAY,
					     WLED_OVP_DELAY + 10);
			}
		}

		val = WLED_BOOST_OFF;
		rc = regmap_write(led->regmap, WLED_MOD_CTRL_REG(led->base),
				  val);
		if (rc) {
			dev_err(&led->pdev->dev,
				"WLED write ctrl reg failed(%d)\n", rc);
			return rc;
		}

		for (i = 0; i < num_wled_strings; i++) {
			rc = qpnp_led_masked_write(led,
				WLED_FULL_SCALE_REG(led->base, i),
				WLED_MAX_CURR_MASK, (u8)led->max_current);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Write max current failure (%d)\n",
					rc);
				return rc;
			}
		}

		rc = qpnp_wled_sync(led);
		if (rc) {
			dev_err(&led->pdev->dev,
				"WLED sync failed(%d)\n", rc);
			return rc;
		}

		if (led->wled_cfg->pmic_version == PMIC_VER_8941) {
			if (led->wled_cfg->num_physical_strings <=
					WLED_THREE_STRINGS) {
				rc = regmap_write(led->regmap,
					  WLED_BOOST_LIMIT_REG(led->base),
					  ilim_val);
				if (rc) {
					dev_err(&led->pdev->dev,
						"WLED write sink reg failed");
					return rc;
				}
			} else {
				/* restore OVP to original value */
				rc = regmap_write(led->regmap,
						  WLED_OVP_CFG_REG(led->base),
						  *&led->wled_cfg->ovp_val);
				if (rc) {
					dev_err(&led->pdev->dev,
						"WLED write sink reg failed");
					return rc;
				}
			}
		}

		/* re-enable all sinks */
		rc = regmap_write(led->regmap, WLED_CURR_SINK_REG(led->base),
				  sink_val);
		if (rc) {
			dev_err(&led->pdev->dev,
				"WLED write sink reg failed(%d)\n", rc);
			return rc;
		}

	} else {
		val = WLED_BOOST_ON;
		rc = regmap_write(led->regmap, WLED_MOD_CTRL_REG(led->base),
				  val);
		if (rc) {
			dev_err(&led->pdev->dev,
				"WLED write ctrl reg failed(%d)\n", rc);
			return rc;
		}
	}

	duty = (WLED_MAX_DUTY_CYCLE * level) / WLED_MAX_LEVEL;

	/* program brightness control registers */
	for (i = 0; i < num_wled_strings; i++) {
		rc = qpnp_led_masked_write(led,
			WLED_BRIGHTNESS_CNTL_MSB(led->base, i), WLED_MSB_MASK,
			(duty >> WLED_8_BIT_SHFT) & WLED_4_BIT_MASK);
		if (rc) {
			dev_err(&led->pdev->dev,
				"WLED set brightness MSB failed(%d)\n", rc);
			return rc;
		}
		val = duty & WLED_8_BIT_MASK;
		rc = regmap_write(led->regmap,
				  WLED_BRIGHTNESS_CNTL_LSB(led->base, i), val);
		if (rc) {
			dev_err(&led->pdev->dev,
				"WLED set brightness LSB failed(%d)\n", rc);
			return rc;
		}
	}

	rc = qpnp_wled_sync(led);
	if (rc) {
		dev_err(&led->pdev->dev, "WLED sync failed(%d)\n", rc);
		return rc;
	}
	return 0;
}

static int qpnp_mpp_set(struct qpnp_led_data *led)
{
	int rc;
	u8 val;
	int duty_us, duty_ns, period_us;

	if (led->cdev.brightness) {
		if (led->mpp_cfg->mpp_reg && !led->mpp_cfg->enable) {
			rc = regulator_set_voltage(led->mpp_cfg->mpp_reg,
					led->mpp_cfg->min_uV,
					led->mpp_cfg->max_uV);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Regulator voltage set failed rc=%d\n",
									rc);
				return rc;
			}

			rc = regulator_enable(led->mpp_cfg->mpp_reg);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Regulator enable failed(%d)\n", rc);
				goto err_reg_enable;
			}
		}

		led->mpp_cfg->enable = true;

		if (led->cdev.brightness < led->mpp_cfg->min_brightness) {
			dev_warn(&led->pdev->dev, "brightness is less than supported, set to minimum supported\n");
			led->cdev.brightness = led->mpp_cfg->min_brightness;
		}

		if (led->mpp_cfg->pwm_mode != MANUAL_MODE) {
			if (!led->mpp_cfg->pwm_cfg->blinking) {
				led->mpp_cfg->pwm_cfg->mode =
					led->mpp_cfg->pwm_cfg->default_mode;
				led->mpp_cfg->pwm_mode =
					led->mpp_cfg->pwm_cfg->default_mode;
			}
		}
		if (led->mpp_cfg->pwm_mode == PWM_MODE) {
			/*config pwm for brightness scaling*/
			rc = pwm_change_mode(led->mpp_cfg->pwm_cfg->pwm_dev,
					PM_PWM_MODE_PWM);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"Failed to set PWM mode, rc = %d\n",
					rc);
				return rc;
			}
			period_us = led->mpp_cfg->pwm_cfg->pwm_period_us;
			if (period_us > INT_MAX / NSEC_PER_USEC) {
				duty_us = (period_us * led->cdev.brightness) /
					LED_FULL;
				rc = pwm_config_us(
					led->mpp_cfg->pwm_cfg->pwm_dev,
					duty_us,
					period_us);
			} else {
				duty_ns = ((period_us * NSEC_PER_USEC) /
					LED_FULL) * led->cdev.brightness;
				rc = pwm_config(
					led->mpp_cfg->pwm_cfg->pwm_dev,
					duty_ns,
					period_us * NSEC_PER_USEC);
			}
			if (rc < 0) {
				dev_err(&led->pdev->dev, "Failed to configure pwm for new values\n");
				goto err_mpp_reg_write;
			}
		}

		if (led->mpp_cfg->pwm_mode != MANUAL_MODE) {
			pwm_enable(led->mpp_cfg->pwm_cfg->pwm_dev);
			led->mpp_cfg->pwm_cfg->pwm_enabled = 1;
		} else {
			if (led->cdev.brightness < LED_MPP_CURRENT_MIN)
				led->cdev.brightness = LED_MPP_CURRENT_MIN;
			else {
				/*
				 * PMIC supports LED intensity from 5mA - 40mA
				 * in steps of 5mA. Brightness is rounded to
				 * 5mA or nearest lower supported values
				 */
				led->cdev.brightness /= LED_MPP_CURRENT_MIN;
				led->cdev.brightness *= LED_MPP_CURRENT_MIN;
			}

			val = (led->cdev.brightness / LED_MPP_CURRENT_MIN) - 1;

			rc = qpnp_led_masked_write(led,
					LED_MPP_SINK_CTRL(led->base),
					LED_MPP_SINK_MASK, val);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Failed to write sink control reg\n");
				goto err_mpp_reg_write;
			}
		}

		val = (led->mpp_cfg->source_sel & LED_MPP_SRC_MASK) |
			(led->mpp_cfg->mode_ctrl & LED_MPP_MODE_CTRL_MASK);

		rc = qpnp_led_masked_write(led,
			LED_MPP_MODE_CTRL(led->base), LED_MPP_MODE_MASK,
			val);
		if (rc) {
			dev_err(&led->pdev->dev,
					"Failed to write led mode reg\n");
			goto err_mpp_reg_write;
		}

		rc = qpnp_led_masked_write(led,
				LED_MPP_EN_CTRL(led->base), LED_MPP_EN_MASK,
				LED_MPP_EN_ENABLE);
		if (rc) {
			dev_err(&led->pdev->dev, "Failed to write led enable reg\n");
			goto err_mpp_reg_write;
		}
	} else {
		if (led->mpp_cfg->pwm_mode != MANUAL_MODE) {
			led->mpp_cfg->pwm_cfg->mode =
				led->mpp_cfg->pwm_cfg->default_mode;
			led->mpp_cfg->pwm_mode =
				led->mpp_cfg->pwm_cfg->default_mode;
			pwm_disable(led->mpp_cfg->pwm_cfg->pwm_dev);
			led->mpp_cfg->pwm_cfg->pwm_enabled = 0;
		}
		rc = qpnp_led_masked_write(led,
					LED_MPP_MODE_CTRL(led->base),
					LED_MPP_MODE_MASK,
					LED_MPP_MODE_DISABLE);
		if (rc) {
			dev_err(&led->pdev->dev,
					"Failed to write led mode reg\n");
			goto err_mpp_reg_write;
		}

		rc = qpnp_led_masked_write(led,
					LED_MPP_EN_CTRL(led->base),
					LED_MPP_EN_MASK,
					LED_MPP_EN_DISABLE);
		if (rc) {
			dev_err(&led->pdev->dev,
					"Failed to write led enable reg\n");
			goto err_mpp_reg_write;
		}

		if (led->mpp_cfg->mpp_reg && led->mpp_cfg->enable) {
			rc = regulator_disable(led->mpp_cfg->mpp_reg);
			if (rc) {
				dev_err(&led->pdev->dev,
					"MPP regulator disable failed(%d)\n",
					rc);
				return rc;
			}

			rc = regulator_set_voltage(led->mpp_cfg->mpp_reg,
						0, led->mpp_cfg->max_uV);
			if (rc) {
				dev_err(&led->pdev->dev,
					"MPP regulator voltage set failed(%d)\n",
					rc);
				return rc;
			}
		}

		led->mpp_cfg->enable = false;
	}

	if (led->mpp_cfg->pwm_mode != MANUAL_MODE)
		led->mpp_cfg->pwm_cfg->blinking = false;
	qpnp_dump_regs(led, mpp_debug_regs, ARRAY_SIZE(mpp_debug_regs));

	return 0;

err_mpp_reg_write:
	if (led->mpp_cfg->mpp_reg)
		regulator_disable(led->mpp_cfg->mpp_reg);
err_reg_enable:
	if (led->mpp_cfg->mpp_reg)
		regulator_set_voltage(led->mpp_cfg->mpp_reg, 0,
							led->mpp_cfg->max_uV);
	led->mpp_cfg->enable = false;

	return rc;
}

static int qpnp_gpio_set(struct qpnp_led_data *led)
{
	int rc, val;

	if (led->cdev.brightness) {
		val = (led->gpio_cfg->source_sel & LED_GPIO_SRC_MASK) |
			(led->gpio_cfg->mode_ctrl & LED_GPIO_MODE_CTRL_MASK);

		rc = qpnp_led_masked_write(led,
			 LED_GPIO_MODE_CTRL(led->base),
			 LED_GPIO_MODE_MASK,
			 val);
		if (rc) {
			dev_err(&led->pdev->dev,
					"Failed to write led mode reg\n");
			goto err_gpio_reg_write;
		}

		rc = qpnp_led_masked_write(led,
			 LED_GPIO_EN_CTRL(led->base),
			 LED_GPIO_EN_MASK,
			 LED_GPIO_EN_ENABLE);
		if (rc) {
			dev_err(&led->pdev->dev,
					"Failed to write led enable reg\n");
			goto err_gpio_reg_write;
		}

		led->gpio_cfg->enable = true;
	} else {
		rc = qpnp_led_masked_write(led,
				LED_GPIO_MODE_CTRL(led->base),
				LED_GPIO_MODE_MASK,
				LED_GPIO_MODE_DISABLE);
		if (rc) {
			dev_err(&led->pdev->dev,
					"Failed to write led mode reg\n");
			goto err_gpio_reg_write;
		}

		rc = qpnp_led_masked_write(led,
				LED_GPIO_EN_CTRL(led->base),
				LED_GPIO_EN_MASK,
				LED_GPIO_EN_DISABLE);
		if (rc) {
			dev_err(&led->pdev->dev,
					"Failed to write led enable reg\n");
			goto err_gpio_reg_write;
		}

		led->gpio_cfg->enable = false;
	}

	qpnp_dump_regs(led, gpio_debug_regs, ARRAY_SIZE(gpio_debug_regs));

	return 0;

err_gpio_reg_write:
	led->gpio_cfg->enable = false;

	return rc;
}

static int qpnp_flash_regulator_operate(struct qpnp_led_data *led, bool on)
{
	int rc, i;
	struct qpnp_led_data *led_array;
	bool regulator_on = false;

	led_array = dev_get_drvdata(&led->pdev->dev);
	if (!led_array) {
		dev_err(&led->pdev->dev, "Unable to get LED array\n");
		return -EINVAL;
	}

	for (i = 0; i < led->num_leds; i++)
		regulator_on |= led_array[i].flash_cfg->flash_on;

	if (!on)
		goto regulator_turn_off;

	if (!regulator_on && !led->flash_cfg->flash_on) {
		for (i = 0; i < led->num_leds; i++) {
			if (led_array[i].flash_cfg->flash_reg_get) {
				if (led_array[i].flash_cfg->flash_wa_reg_get) {
					rc = regulator_enable(
						led_array[i].flash_cfg->
							flash_wa_reg);
					if (rc) {
						dev_err(&led->pdev->dev, "Flash wa regulator enable failed(%d)\n",
							rc);
						return rc;
					}
				}

				rc = regulator_enable(
				       led_array[i].flash_cfg->flash_boost_reg);
				if (rc) {
					if (led_array[i].flash_cfg->
							flash_wa_reg_get)
						/*
						 * Disable flash wa regulator
						 * when flash boost regulator
						 * enable fails
						 */
						regulator_disable(
							led_array[i].flash_cfg->
								flash_wa_reg);
					dev_err(&led->pdev->dev, "Flash boost regulator enable failed(%d)\n",
						rc);
					return rc;
				}
				led->flash_cfg->flash_on = true;
			}
			break;
		}
	}

	return 0;

regulator_turn_off:
	if (regulator_on && led->flash_cfg->flash_on) {
		for (i = 0; i < led->num_leds; i++) {
			if (led_array[i].flash_cfg->flash_reg_get) {
				rc = qpnp_led_masked_write(led,
					FLASH_ENABLE_CONTROL(led->base),
					FLASH_ENABLE_MASK,
					FLASH_DISABLE_ALL);
				if (rc) {
					dev_err(&led->pdev->dev,
						"Enable reg write failed(%d)\n",
						rc);
				}

				rc = regulator_disable(
				       led_array[i].flash_cfg->flash_boost_reg);
				if (rc) {
					dev_err(&led->pdev->dev, "Flash boost regulator disable failed(%d)\n",
						rc);
					return rc;
				}
				if (led_array[i].flash_cfg->flash_wa_reg_get) {
					rc = regulator_disable(
						led_array[i].flash_cfg->
							flash_wa_reg);
					if (rc) {
						dev_err(&led->pdev->dev, "Flash_wa regulator disable failed(%d)\n",
							rc);
						return rc;
					}
				}
				led->flash_cfg->flash_on = false;
			}
			break;
		}
	}

	return 0;
}

static int qpnp_torch_regulator_operate(struct qpnp_led_data *led, bool on)
{
	int rc;

	if (!on)
		goto regulator_turn_off;

	if (!led->flash_cfg->torch_on) {
		rc = regulator_enable(led->flash_cfg->torch_boost_reg);
		if (rc) {
			dev_err(&led->pdev->dev,
				"Regulator enable failed(%d)\n", rc);
				return rc;
		}
		led->flash_cfg->torch_on = true;
	}
	return 0;

regulator_turn_off:
	if (led->flash_cfg->torch_on) {
		rc = qpnp_led_masked_write(led,	FLASH_ENABLE_CONTROL(led->base),
				FLASH_ENABLE_MODULE_MASK, FLASH_DISABLE_ALL);
		if (rc) {
			dev_err(&led->pdev->dev,
				"Enable reg write failed(%d)\n", rc);
		}

		rc = regulator_disable(led->flash_cfg->torch_boost_reg);
		if (rc) {
			dev_err(&led->pdev->dev,
				"Regulator disable failed(%d)\n", rc);
			return rc;
		}
		led->flash_cfg->torch_on = false;
	}
	return 0;
}

static int qpnp_flash_set(struct qpnp_led_data *led)
{
	int rc = 0, error;
	int val = led->cdev.brightness;

	if (led->flash_cfg->torch_enable)
		led->flash_cfg->current_prgm =
			(val * TORCH_MAX_LEVEL / led->max_current);
	else
		led->flash_cfg->current_prgm =
			(val * FLASH_MAX_LEVEL / led->max_current);

	/* Set led current */
	if (val > 0) {
		if (led->flash_cfg->torch_enable) {
			if (led->flash_cfg->peripheral_subtype ==
							FLASH_SUBTYPE_DUAL) {
				if (!led->flash_cfg->no_smbb_support)
					rc = qpnp_torch_regulator_operate(led,
									true);
				else
					rc = qpnp_flash_regulator_operate(led,
									true);
				if (rc) {
					dev_err(&led->pdev->dev,
					"Torch regulator operate failed(%d)\n",
					rc);
					return rc;
				}
			} else if (led->flash_cfg->peripheral_subtype ==
							FLASH_SUBTYPE_SINGLE) {
				rc = qpnp_flash_regulator_operate(led, true);
				if (rc) {
					dev_err(&led->pdev->dev,
					"Flash regulator operate failed(%d)\n",
					rc);
					goto error_flash_set;
				}
			}

			rc = qpnp_led_masked_write(led,
				FLASH_MAX_CURR(led->base),
				FLASH_CURRENT_MASK,
				TORCH_MAX_LEVEL);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Max current reg write failed(%d)\n",
					rc);
				goto error_reg_write;
			}

			rc = qpnp_led_masked_write(led,
				FLASH_LED_TMR_CTRL(led->base),
				FLASH_TMR_MASK,
				FLASH_TMR_WATCHDOG);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Timer control reg write failed(%d)\n",
					rc);
				goto error_reg_write;
			}

			rc = qpnp_led_masked_write(led,
				led->flash_cfg->current_addr,
				FLASH_CURRENT_MASK,
				led->flash_cfg->current_prgm);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Current reg write failed(%d)\n", rc);
				goto error_reg_write;
			}

			rc = qpnp_led_masked_write(led,
				led->flash_cfg->second_addr,
				FLASH_CURRENT_MASK,
				led->flash_cfg->current_prgm);
			if (rc) {
				dev_err(&led->pdev->dev,
					"2nd Current reg write failed(%d)\n",
					rc);
				goto error_reg_write;
			}

			rc = qpnp_led_masked_write(led,
				FLASH_WATCHDOG_TMR(led->base),
				FLASH_WATCHDOG_MASK,
				led->flash_cfg->duration);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Max current reg write failed(%d)\n",
					rc);
				goto error_reg_write;
			}

			rc = qpnp_led_masked_write(led,
				FLASH_ENABLE_CONTROL(led->base),
				FLASH_ENABLE_MASK,
				led->flash_cfg->enable_module);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Enable reg write failed(%d)\n",
					rc);
				goto error_reg_write;
			}

			if (!led->flash_cfg->strobe_type)
				led->flash_cfg->trigger_flash &=
						~FLASH_HW_SW_STROBE_SEL_MASK;
			else
				led->flash_cfg->trigger_flash |=
						FLASH_HW_SW_STROBE_SEL_MASK;

			rc = qpnp_led_masked_write(led,
				FLASH_LED_STROBE_CTRL(led->base),
				led->flash_cfg->trigger_flash,
				led->flash_cfg->trigger_flash);
			if (rc) {
				dev_err(&led->pdev->dev,
					"LED %d strobe reg write failed(%d)\n",
					led->id, rc);
				goto error_reg_write;
			}
		} else {
			rc = qpnp_flash_regulator_operate(led, true);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Flash regulator operate failed(%d)\n",
					rc);
				goto error_flash_set;
			}

			rc = qpnp_led_masked_write(led,
				FLASH_LED_TMR_CTRL(led->base),
				FLASH_TMR_MASK,
				FLASH_TMR_SAFETY);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Timer control reg write failed(%d)\n",
					rc);
				goto error_reg_write;
			}

			/* Set flash safety timer */
			rc = qpnp_led_masked_write(led,
				FLASH_SAFETY_TIMER(led->base),
				FLASH_SAFETY_TIMER_MASK,
				led->flash_cfg->duration);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Safety timer reg write failed(%d)\n",
					rc);
				goto error_flash_set;
			}

			/* Set max current */
			rc = qpnp_led_masked_write(led,
				FLASH_MAX_CURR(led->base), FLASH_CURRENT_MASK,
				FLASH_MAX_LEVEL);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Max current reg write failed(%d)\n",
					rc);
				goto error_flash_set;
			}

			/* Set clamp current */
			rc = qpnp_led_masked_write(led,
				FLASH_CLAMP_CURR(led->base),
				FLASH_CURRENT_MASK,
				led->flash_cfg->clamp_curr);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Clamp current reg write failed(%d)\n",
					rc);
				goto error_flash_set;
			}

			rc = qpnp_led_masked_write(led,
				led->flash_cfg->current_addr,
				FLASH_CURRENT_MASK,
				led->flash_cfg->current_prgm);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Current reg write failed(%d)\n", rc);
				goto error_flash_set;
			}

			rc = qpnp_led_masked_write(led,
				FLASH_ENABLE_CONTROL(led->base),
				led->flash_cfg->enable_module,
				led->flash_cfg->enable_module);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Enable reg write failed(%d)\n", rc);
				goto error_flash_set;
			}

			/*
			 * Add 1ms delay for bharger enter stable state
			 */
			usleep_range(FLASH_RAMP_UP_DELAY_US,
				     FLASH_RAMP_UP_DELAY_US + 10);

			if (!led->flash_cfg->strobe_type)
				led->flash_cfg->trigger_flash &=
						~FLASH_HW_SW_STROBE_SEL_MASK;
			else
				led->flash_cfg->trigger_flash |=
						FLASH_HW_SW_STROBE_SEL_MASK;

			rc = qpnp_led_masked_write(led,
				FLASH_LED_STROBE_CTRL(led->base),
				led->flash_cfg->trigger_flash,
				led->flash_cfg->trigger_flash);
			if (rc) {
				dev_err(&led->pdev->dev,
				"LED %d strobe reg write failed(%d)\n",
				led->id, rc);
				goto error_flash_set;
			}
		}
	} else {
		rc = qpnp_led_masked_write(led,
			FLASH_LED_STROBE_CTRL(led->base),
			led->flash_cfg->trigger_flash,
			FLASH_DISABLE_ALL);
		if (rc) {
			dev_err(&led->pdev->dev,
				"LED %d flash write failed(%d)\n", led->id, rc);
			if (led->flash_cfg->torch_enable)
				goto error_torch_set;
			else
				goto error_flash_set;
		}

		if (led->flash_cfg->torch_enable) {
			if (led->flash_cfg->peripheral_subtype ==
							FLASH_SUBTYPE_DUAL) {
				if (!led->flash_cfg->no_smbb_support)
					rc = qpnp_torch_regulator_operate(led,
									false);
				else
					rc = qpnp_flash_regulator_operate(led,
									false);
				if (rc) {
					dev_err(&led->pdev->dev,
						"Torch regulator operate failed(%d)\n",
						rc);
					return rc;
				}
			} else if (led->flash_cfg->peripheral_subtype ==
							FLASH_SUBTYPE_SINGLE) {
				rc = qpnp_flash_regulator_operate(led, false);
				if (rc) {
					dev_err(&led->pdev->dev,
						"Flash regulator operate failed(%d)\n",
						rc);
					return rc;
				}
			}
		} else {
			/*
			 * Disable module after ramp down complete for stable
			 * behavior
			 */
			usleep_range(FLASH_RAMP_UP_DELAY_US,
				     FLASH_RAMP_UP_DELAY_US + 10);

			rc = qpnp_led_masked_write(led,
				FLASH_ENABLE_CONTROL(led->base),
				led->flash_cfg->enable_module &
				~FLASH_ENABLE_MODULE_MASK,
				FLASH_DISABLE_ALL);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Enable reg write failed(%d)\n", rc);
				if (led->flash_cfg->torch_enable)
					goto error_torch_set;
				else
					goto error_flash_set;
			}

			rc = qpnp_flash_regulator_operate(led, false);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Flash regulator operate failed(%d)\n",
					rc);
				return rc;
			}
		}
	}

	qpnp_dump_regs(led, flash_debug_regs, ARRAY_SIZE(flash_debug_regs));

	return 0;

error_reg_write:
	if (led->flash_cfg->peripheral_subtype == FLASH_SUBTYPE_SINGLE)
		goto error_flash_set;

error_torch_set:
	if (!led->flash_cfg->no_smbb_support)
		error = qpnp_torch_regulator_operate(led, false);
	else
		error = qpnp_flash_regulator_operate(led, false);
	if (error) {
		dev_err(&led->pdev->dev,
			"Torch regulator operate failed(%d)\n", rc);
		return error;
	}
	return rc;

error_flash_set:
	error = qpnp_flash_regulator_operate(led, false);
	if (error) {
		dev_err(&led->pdev->dev,
			"Flash regulator operate failed(%d)\n", rc);
		return error;
	}
	return rc;
}

static int qpnp_kpdbl_set(struct qpnp_led_data *led)
{
	int rc;
	int duty_us, duty_ns, period_us;

	if (led->cdev.brightness) {
		if (!led->kpdbl_cfg->pwm_cfg->blinking)
			led->kpdbl_cfg->pwm_cfg->mode =
				led->kpdbl_cfg->pwm_cfg->default_mode;

		if (bitmap_empty(kpdbl_leds_in_use, NUM_KPDBL_LEDS)) {
			rc = qpnp_led_masked_write(led, KPDBL_ENABLE(led->base),
					KPDBL_MODULE_EN_MASK, KPDBL_MODULE_EN);
			if (rc) {
				dev_err(&led->pdev->dev,
					"Enable reg write failed(%d)\n", rc);
				return rc;
			}
		}

		/* On some platforms, GPLED1 channel should always be enabled
		 * for the other GPLEDs 2/3/4 to glow. Before enabling GPLED
		 * 2/3/4, first check if GPLED1 is already enabled. If GPLED1
		 * channel is not enabled, then enable the GPLED1 channel but
		 * with a 0 brightness
		 */
		if (!led->kpdbl_cfg->always_on &&
			!test_bit(KPDBL_MASTER_BIT_INDEX, kpdbl_leds_in_use) &&
						kpdbl_master) {
			rc = pwm_config_us(kpdbl_master, 0,
					kpdbl_master_period_us);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"pwm config failed\n");
				return rc;
			}

			rc = pwm_enable(kpdbl_master);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"pwm enable failed\n");
				return rc;
			}
			set_bit(KPDBL_MASTER_BIT_INDEX,
						kpdbl_leds_in_use);
		}

		if (led->kpdbl_cfg->pwm_cfg->mode == PWM_MODE) {
			rc = pwm_change_mode(led->kpdbl_cfg->pwm_cfg->pwm_dev,
					PM_PWM_MODE_PWM);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"Failed to set PWM mode, rc = %d\n",
					rc);
				return rc;
			}
			period_us = led->kpdbl_cfg->pwm_cfg->pwm_period_us;
			if (period_us > INT_MAX / NSEC_PER_USEC) {
				duty_us = (period_us * led->cdev.brightness) /
					KPDBL_MAX_LEVEL;
				rc = pwm_config_us(
					led->kpdbl_cfg->pwm_cfg->pwm_dev,
					duty_us,
					period_us);
			} else {
				duty_ns = ((period_us * NSEC_PER_USEC) /
					KPDBL_MAX_LEVEL) * led->cdev.brightness;
				rc = pwm_config(
					led->kpdbl_cfg->pwm_cfg->pwm_dev,
					duty_ns,
					period_us * NSEC_PER_USEC);
			}
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"pwm config failed\n");
				return rc;
			}
		}

		rc = pwm_enable(led->kpdbl_cfg->pwm_cfg->pwm_dev);
		if (rc < 0) {
			dev_err(&led->pdev->dev, "pwm enable failed\n");
			return rc;
		}
		led->kpdbl_cfg->pwm_cfg->pwm_enabled = 1;
		set_bit(led->kpdbl_cfg->row_id, kpdbl_leds_in_use);

		/* is_kpdbl_master_turn_on will be set to true when GPLED1
		 * channel is enabled and has a valid brightness value
		 */
		if (led->kpdbl_cfg->always_on)
			is_kpdbl_master_turn_on = true;

	} else {
		led->kpdbl_cfg->pwm_cfg->mode =
			led->kpdbl_cfg->pwm_cfg->default_mode;

		/* Before disabling GPLED1, check if any other GPLED 2/3/4 is
		 * on. If any of the other GPLED 2/3/4 is on, then have the
		 * GPLED1 channel enabled with 0 brightness.
		 */
		if (led->kpdbl_cfg->always_on) {
			if (bitmap_weight(kpdbl_leds_in_use,
						NUM_KPDBL_LEDS) > 1) {
				rc = pwm_config_us(
					led->kpdbl_cfg->pwm_cfg->pwm_dev, 0,
					led->kpdbl_cfg->pwm_cfg->pwm_period_us);
				if (rc < 0) {
					dev_err(&led->pdev->dev,
						"pwm config failed\n");
					return rc;
				}

				rc = pwm_enable(led->kpdbl_cfg->pwm_cfg->
							pwm_dev);
				if (rc < 0) {
					dev_err(&led->pdev->dev,
						"pwm enable failed\n");
					return rc;
				}
				led->kpdbl_cfg->pwm_cfg->pwm_enabled = 1;
			} else {
				if (kpdbl_master) {
					pwm_disable(kpdbl_master);
					clear_bit(KPDBL_MASTER_BIT_INDEX,
						kpdbl_leds_in_use);
					rc = qpnp_led_masked_write(
						led, KPDBL_ENABLE(led->base),
						KPDBL_MODULE_EN_MASK,
						KPDBL_MODULE_DIS);
					if (rc) {
						dev_err(&led->pdev->dev, "Failed to write led enable reg\n");
						return rc;
					}
				}
			}
			is_kpdbl_master_turn_on = false;
		} else {
			pwm_disable(led->kpdbl_cfg->pwm_cfg->pwm_dev);
			led->kpdbl_cfg->pwm_cfg->pwm_enabled = 0;
			clear_bit(led->kpdbl_cfg->row_id, kpdbl_leds_in_use);
			if (bitmap_weight(kpdbl_leds_in_use,
				NUM_KPDBL_LEDS) == 1 && kpdbl_master &&
						!is_kpdbl_master_turn_on) {
				pwm_disable(kpdbl_master);
				clear_bit(KPDBL_MASTER_BIT_INDEX,
					kpdbl_leds_in_use);
				rc = qpnp_led_masked_write(
					led, KPDBL_ENABLE(led->base),
					KPDBL_MODULE_EN_MASK, KPDBL_MODULE_DIS);
				if (rc) {
					dev_err(&led->pdev->dev,
					"Failed to write led enable reg\n");
					return rc;
				}
				is_kpdbl_master_turn_on = false;
			}
		}
	}

	led->kpdbl_cfg->pwm_cfg->blinking = false;

	qpnp_dump_regs(led, kpdbl_debug_regs, ARRAY_SIZE(kpdbl_debug_regs));

	return 0;
}

static int qpnp_rgb_set(struct qpnp_led_data *led)
{
	int rc;
	int duty_us, duty_ns, period_us;

	if (led->cdev.brightness) {
		if (!led->rgb_cfg->pwm_cfg->blinking)
			led->rgb_cfg->pwm_cfg->mode =
				led->rgb_cfg->pwm_cfg->default_mode;
		if (led->rgb_cfg->pwm_cfg->mode == PWM_MODE) {
			rc = pwm_change_mode(led->rgb_cfg->pwm_cfg->pwm_dev,
					PM_PWM_MODE_PWM);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"Failed to set PWM mode, rc = %d\n",
					rc);
				return rc;
			}
			period_us = led->rgb_cfg->pwm_cfg->pwm_period_us;
			if (period_us > INT_MAX / NSEC_PER_USEC) {
				duty_us = (period_us * led->cdev.brightness) /
					LED_FULL;
				rc = pwm_config_us(
					led->rgb_cfg->pwm_cfg->pwm_dev,
					duty_us,
					period_us);
			} else {
				duty_ns = ((period_us * NSEC_PER_USEC) /
					LED_FULL) * led->cdev.brightness;
				rc = pwm_config(
					led->rgb_cfg->pwm_cfg->pwm_dev,
					duty_ns,
					period_us * NSEC_PER_USEC);
			}
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"pwm config failed\n");
				return rc;
			}
		}
		rc = qpnp_led_masked_write(led,
			RGB_LED_EN_CTL(led->base),
			led->rgb_cfg->enable, led->rgb_cfg->enable);
		if (rc) {
			dev_err(&led->pdev->dev,
				"Failed to write led enable reg\n");
			return rc;
		}
		if (!led->rgb_cfg->pwm_cfg->pwm_enabled) {
			pwm_enable(led->rgb_cfg->pwm_cfg->pwm_dev);
			led->rgb_cfg->pwm_cfg->pwm_enabled = 1;
		}
	} else {
		led->rgb_cfg->pwm_cfg->mode =
			led->rgb_cfg->pwm_cfg->default_mode;
		if (led->rgb_cfg->pwm_cfg->pwm_enabled) {
			pwm_disable(led->rgb_cfg->pwm_cfg->pwm_dev);
			led->rgb_cfg->pwm_cfg->pwm_enabled = 0;
		}
		rc = qpnp_led_masked_write(led,
			RGB_LED_EN_CTL(led->base),
			led->rgb_cfg->enable, RGB_LED_DISABLE);
		if (rc) {
			dev_err(&led->pdev->dev,
				"Failed to write led enable reg\n");
			return rc;
		}
	}

	led->rgb_cfg->pwm_cfg->blinking = false;
	qpnp_dump_regs(led, rgb_pwm_debug_regs, ARRAY_SIZE(rgb_pwm_debug_regs));

	return 0;
}

static void qpnp_led_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	struct qpnp_led_data *led;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);
	if (value < LED_OFF) {
		dev_err(&led->pdev->dev, "Invalid brightness value\n");
		return;
	}

	if (value > led->cdev.max_brightness)
		value = led->cdev.max_brightness;

	led->cdev.brightness = value;
	if (led->in_order_command_processing)
		queue_work(led->workqueue, &led->work);
	else
		schedule_work(&led->work);
}

static void __qpnp_led_work(struct qpnp_led_data *led,
				enum led_brightness value)
{
	int rc;

	if (led->id == QPNP_ID_FLASH1_LED0 || led->id == QPNP_ID_FLASH1_LED1)
		mutex_lock(&flash_lock);
	else
		mutex_lock(&led->lock);

	switch (led->id) {
	case QPNP_ID_WLED:
		rc = qpnp_wled_set(led);
		if (rc < 0)
			dev_err(&led->pdev->dev,
				"WLED set brightness failed (%d)\n", rc);
		break;
	case QPNP_ID_FLASH1_LED0:
	case QPNP_ID_FLASH1_LED1:
		rc = qpnp_flash_set(led);
		if (rc < 0)
			dev_err(&led->pdev->dev,
				"FLASH set brightness failed (%d)\n", rc);
		break;
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		rc = qpnp_rgb_set(led);
		if (rc < 0)
			dev_err(&led->pdev->dev,
				"RGB set brightness failed (%d)\n", rc);
		break;
	case QPNP_ID_LED_MPP:
		rc = qpnp_mpp_set(led);
		if (rc < 0)
			dev_err(&led->pdev->dev,
					"MPP set brightness failed (%d)\n", rc);
		break;
	case QPNP_ID_LED_GPIO:
		rc = qpnp_gpio_set(led);
		if (rc < 0)
			dev_err(&led->pdev->dev,
					"GPIO set brightness failed (%d)\n",
					rc);
		break;
	case QPNP_ID_KPDBL:
		rc = qpnp_kpdbl_set(led);
		if (rc < 0)
			dev_err(&led->pdev->dev,
				"KPDBL set brightness failed (%d)\n", rc);
		break;
	default:
		dev_err(&led->pdev->dev, "Invalid LED(%d)\n", led->id);
		break;
	}
	if (led->id == QPNP_ID_FLASH1_LED0 || led->id == QPNP_ID_FLASH1_LED1)
		mutex_unlock(&flash_lock);
	else
		mutex_unlock(&led->lock);

}

static void qpnp_led_work(struct work_struct *work)
{
	struct qpnp_led_data *led = container_of(work,
					struct qpnp_led_data, work);

	__qpnp_led_work(led, led->cdev.brightness);
}

static int qpnp_led_set_max_brightness(struct qpnp_led_data *led)
{
	switch (led->id) {
	case QPNP_ID_WLED:
		led->cdev.max_brightness = WLED_MAX_LEVEL;
		break;
	case QPNP_ID_FLASH1_LED0:
	case QPNP_ID_FLASH1_LED1:
		led->cdev.max_brightness = led->max_current;
		break;
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		led->cdev.max_brightness = RGB_MAX_LEVEL;
		break;
	case QPNP_ID_LED_MPP:
		if (led->mpp_cfg->pwm_mode == MANUAL_MODE)
			led->cdev.max_brightness = led->max_current;
		else
			led->cdev.max_brightness = MPP_MAX_LEVEL;
		break;
	case QPNP_ID_LED_GPIO:
			led->cdev.max_brightness = led->max_current;
		break;
	case QPNP_ID_KPDBL:
		led->cdev.max_brightness = KPDBL_MAX_LEVEL;
		break;
	default:
		dev_err(&led->pdev->dev, "Invalid LED(%d)\n", led->id);
		return -EINVAL;
	}

	return 0;
}

static enum led_brightness qpnp_led_get(struct led_classdev *led_cdev)
{
	struct qpnp_led_data *led;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	return led->cdev.brightness;
}

static void qpnp_led_turn_off_delayed(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct qpnp_led_data *led
		= container_of(dwork, struct qpnp_led_data, dwork);

	led->cdev.brightness = LED_OFF;
	qpnp_led_set(&led->cdev, led->cdev.brightness);
}

static void qpnp_led_turn_off(struct qpnp_led_data *led)
{
	INIT_DELAYED_WORK(&led->dwork, qpnp_led_turn_off_delayed);
	schedule_delayed_work(&led->dwork,
		msecs_to_jiffies(led->turn_off_delay_ms));
}

static int qpnp_wled_init(struct qpnp_led_data *led)
{
	int rc, i;
	u8 num_wled_strings, val = 0;

	num_wled_strings = led->wled_cfg->num_strings;

	/* verify ranges */
	if (led->wled_cfg->ovp_val > WLED_OVP_27V) {
		dev_err(&led->pdev->dev, "Invalid ovp value\n");
		return -EINVAL;
	}

	if (led->wled_cfg->boost_curr_lim > WLED_CURR_LIMIT_1680mA) {
		dev_err(&led->pdev->dev, "Invalid boost current limit\n");
		return -EINVAL;
	}

	if (led->wled_cfg->cp_select > WLED_CP_SELECT_MAX) {
		dev_err(&led->pdev->dev, "Invalid pole capacitance\n");
		return -EINVAL;
	}

	if (led->max_current > WLED_MAX_CURR) {
		dev_err(&led->pdev->dev, "Invalid max current\n");
		return -EINVAL;
	}

	if ((led->wled_cfg->ctrl_delay_us % WLED_CTL_DLY_STEP) ||
		(led->wled_cfg->ctrl_delay_us > WLED_CTL_DLY_MAX)) {
		dev_err(&led->pdev->dev, "Invalid control delay\n");
		return -EINVAL;
	}

	/* program over voltage protection threshold */
	rc = qpnp_led_masked_write(led, WLED_OVP_CFG_REG(led->base),
		WLED_OVP_VAL_MASK,
		(led->wled_cfg->ovp_val << WLED_OVP_VAL_BIT_SHFT));
	if (rc) {
		dev_err(&led->pdev->dev,
				"WLED OVP reg write failed(%d)\n", rc);
		return rc;
	}

	/* program current boost limit */
	rc = qpnp_led_masked_write(led, WLED_BOOST_LIMIT_REG(led->base),
		WLED_BOOST_LIMIT_MASK, led->wled_cfg->boost_curr_lim);
	if (rc) {
		dev_err(&led->pdev->dev,
				"WLED boost limit reg write failed(%d)\n", rc);
		return rc;
	}

	/* program output feedback */
	rc = qpnp_led_masked_write(led, WLED_FDBCK_CTRL_REG(led->base),
		WLED_OP_FDBCK_MASK,
		(led->wled_cfg->op_fdbck << WLED_OP_FDBCK_BIT_SHFT));
	if (rc) {
		dev_err(&led->pdev->dev,
				"WLED fdbck ctrl reg write failed(%d)\n", rc);
		return rc;
	}

	/* program switch frequency */
	rc = qpnp_led_masked_write(led,
		WLED_SWITCHING_FREQ_REG(led->base),
		WLED_SWITCH_FREQ_MASK, led->wled_cfg->switch_freq);
	if (rc) {
		dev_err(&led->pdev->dev,
			"WLED switch freq reg write failed(%d)\n", rc);
		return rc;
	}

	/* program current sink */
	if (led->wled_cfg->cs_out_en) {
		for (i = 0; i < led->wled_cfg->num_strings; i++)
			val |= 1 << i;
		rc = qpnp_led_masked_write(led, WLED_CURR_SINK_REG(led->base),
			WLED_CURR_SINK_MASK, (val << WLED_CURR_SINK_SHFT));
		if (rc) {
			dev_err(&led->pdev->dev,
				"WLED curr sink reg write failed(%d)\n", rc);
			return rc;
		}
	}

	/* program high pole capacitance */
	rc = qpnp_led_masked_write(led, WLED_HIGH_POLE_CAP_REG(led->base),
		WLED_CP_SELECT_MASK, led->wled_cfg->cp_select);
	if (rc) {
		dev_err(&led->pdev->dev,
				"WLED pole cap reg write failed(%d)\n", rc);
		return rc;
	}

	/* program modulator, current mod src and cabc */
	for (i = 0; i < num_wled_strings; i++) {
		rc = qpnp_led_masked_write(led, WLED_MOD_EN_REG(led->base, i),
			WLED_NO_MASK, WLED_EN_MASK);
		if (rc) {
			dev_err(&led->pdev->dev,
				"WLED mod enable reg write failed(%d)\n", rc);
			return rc;
		}

		if (led->wled_cfg->dig_mod_gen_en) {
			rc = qpnp_led_masked_write(led,
				WLED_MOD_SRC_SEL_REG(led->base, i),
				WLED_NO_MASK, WLED_USE_EXT_GEN_MOD_SRC);
			if (rc) {
				dev_err(&led->pdev->dev,
				"WLED dig mod en reg write failed(%d)\n", rc);
			}
		}

		rc = qpnp_led_masked_write(led,
			WLED_FULL_SCALE_REG(led->base, i), WLED_MAX_CURR_MASK,
			(u8)led->max_current);
		if (rc) {
			dev_err(&led->pdev->dev,
				"WLED max current reg write failed(%d)\n", rc);
			return rc;
		}

	}

	/* Reset WLED enable register */
	rc = qpnp_led_masked_write(led, WLED_MOD_CTRL_REG(led->base),
		WLED_8_BIT_MASK, WLED_BOOST_OFF);
	if (rc) {
		dev_err(&led->pdev->dev,
			"WLED write ctrl reg failed(%d)\n", rc);
		return rc;
	}

	/* dump wled registers */
	qpnp_dump_regs(led, wled_debug_regs, ARRAY_SIZE(wled_debug_regs));

	return 0;
}

static ssize_t led_mode_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	unsigned long state;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &state);
	if (ret)
		return ret;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	/* '1' to enable torch mode; '0' to switch to flash mode */
	if (state == 1)
		led->flash_cfg->torch_enable = true;
	else
		led->flash_cfg->torch_enable = false;

	return count;
}

static ssize_t led_strobe_type_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	unsigned long state;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &state);
	if (ret)
		return ret;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	/* '0' for sw strobe; '1' for hw strobe */
	if (state == 1)
		led->flash_cfg->strobe_type = 1;
	else
		led->flash_cfg->strobe_type = 0;

	return count;
}

static int qpnp_pwm_init(struct pwm_config_data *pwm_cfg,
					struct platform_device *pdev,
					const char *name)
{
	int rc, start_idx, idx_len, lut_max_size;

	if (pwm_cfg->pwm_dev) {
		if (pwm_cfg->mode == LPG_MODE) {
			start_idx =
			pwm_cfg->duty_cycles->start_idx;
			idx_len =
			pwm_cfg->duty_cycles->num_duty_pcts;

			if (strnstr(name, "kpdbl", sizeof("kpdbl")))
				lut_max_size = PWM_GPLED_LUT_MAX_SIZE;
			else
				lut_max_size = PWM_LUT_MAX_SIZE;

			if (idx_len >= lut_max_size && start_idx) {
				dev_err(&pdev->dev,
					"Wrong LUT size or index\n");
				return -EINVAL;
			}

			if ((start_idx + idx_len) > lut_max_size) {
				dev_err(&pdev->dev, "Exceed LUT limit\n");
				return -EINVAL;
			}
			rc = pwm_lut_config(pwm_cfg->pwm_dev,
				pwm_cfg->pwm_period_us,
				pwm_cfg->duty_cycles->duty_pcts,
				pwm_cfg->lut_params);
			if (rc < 0) {
				dev_err(&pdev->dev, "Failed to configure pwm LUT\n");
				return rc;
			}
			rc = pwm_change_mode(pwm_cfg->pwm_dev, PM_PWM_MODE_LPG);
			if (rc < 0) {
				dev_err(&pdev->dev, "Failed to set LPG mode\n");
				return rc;
			}
		}
	} else {
		dev_err(&pdev->dev, "Invalid PWM device\n");
		return -EINVAL;
	}

	return 0;
}

static ssize_t pwm_us_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	u32 pwm_us;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret;
	u32 previous_pwm_us;
	struct pwm_config_data *pwm_cfg;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	ret = kstrtou32(buf, 10, &pwm_us);
	if (ret)
		return ret;

	switch (led->id) {
	case QPNP_ID_LED_MPP:
		pwm_cfg = led->mpp_cfg->pwm_cfg;
		break;
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		break;
	case QPNP_ID_KPDBL:
		pwm_cfg = led->kpdbl_cfg->pwm_cfg;
		break;
	default:
		dev_err(&led->pdev->dev, "Invalid LED id type for pwm_us\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	previous_pwm_us = pwm_cfg->pwm_period_us;

	pwm_cfg->pwm_period_us = pwm_us;
	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret) {
		pwm_cfg->pwm_period_us = previous_pwm_us;
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		qpnp_led_set(&led->cdev, led->cdev.brightness);
		dev_err(&led->pdev->dev,
			"Failed to initialize pwm with new pwm_us value\n");
		return ret;
	}
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return count;
}

static ssize_t pause_lo_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	u32 pause_lo;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret;
	u32 previous_pause_lo;
	struct pwm_config_data *pwm_cfg;

	ret = kstrtou32(buf, 10, &pause_lo);
	if (ret)
		return ret;
	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	switch (led->id) {
	case QPNP_ID_LED_MPP:
		pwm_cfg = led->mpp_cfg->pwm_cfg;
		break;
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		break;
	case QPNP_ID_KPDBL:
		pwm_cfg = led->kpdbl_cfg->pwm_cfg;
		break;
	default:
		dev_err(&led->pdev->dev,
			"Invalid LED id type for pause lo\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	previous_pause_lo = pwm_cfg->lut_params.lut_pause_lo;

	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	pwm_cfg->lut_params.lut_pause_lo = pause_lo;
	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret) {
		pwm_cfg->lut_params.lut_pause_lo = previous_pause_lo;
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		qpnp_led_set(&led->cdev, led->cdev.brightness);
		dev_err(&led->pdev->dev,
			"Failed to initialize pwm with new pause lo value\n");
		return ret;
	}
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return count;
}

static ssize_t pause_hi_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	u32 pause_hi;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret;
	u32 previous_pause_hi;
	struct pwm_config_data *pwm_cfg;

	ret = kstrtou32(buf, 10, &pause_hi);
	if (ret)
		return ret;
	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	switch (led->id) {
	case QPNP_ID_LED_MPP:
		pwm_cfg = led->mpp_cfg->pwm_cfg;
		break;
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		break;
	case QPNP_ID_KPDBL:
		pwm_cfg = led->kpdbl_cfg->pwm_cfg;
		break;
	default:
		dev_err(&led->pdev->dev,
			"Invalid LED id type for pause hi\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	previous_pause_hi = pwm_cfg->lut_params.lut_pause_hi;

	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	pwm_cfg->lut_params.lut_pause_hi = pause_hi;
	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret) {
		pwm_cfg->lut_params.lut_pause_hi = previous_pause_hi;
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		qpnp_led_set(&led->cdev, led->cdev.brightness);
		dev_err(&led->pdev->dev,
			"Failed to initialize pwm with new pause hi value\n");
		return ret;
	}
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return count;
}

static ssize_t start_idx_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	u32 start_idx;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret;
	u32 previous_start_idx;
	struct pwm_config_data *pwm_cfg;

	ret = kstrtou32(buf, 10, &start_idx);
	if (ret)
		return ret;
	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	switch (led->id) {
	case QPNP_ID_LED_MPP:
		pwm_cfg = led->mpp_cfg->pwm_cfg;
		break;
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		break;
	case QPNP_ID_KPDBL:
		pwm_cfg = led->kpdbl_cfg->pwm_cfg;
		break;
	default:
		dev_err(&led->pdev->dev,
			"Invalid LED id type for start idx\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	previous_start_idx = pwm_cfg->duty_cycles->start_idx;
	pwm_cfg->duty_cycles->start_idx = start_idx;
	pwm_cfg->lut_params.start_idx = pwm_cfg->duty_cycles->start_idx;
	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret) {
		pwm_cfg->duty_cycles->start_idx = previous_start_idx;
		pwm_cfg->lut_params.start_idx = pwm_cfg->duty_cycles->start_idx;
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		qpnp_led_set(&led->cdev, led->cdev.brightness);
		dev_err(&led->pdev->dev,
			"Failed to initialize pwm with new start idx value\n");
		return ret;
	}
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return count;
}

static ssize_t ramp_step_ms_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	u32 ramp_step_ms;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret;
	u32 previous_ramp_step_ms;
	struct pwm_config_data *pwm_cfg;

	ret = kstrtou32(buf, 10, &ramp_step_ms);
	if (ret)
		return ret;
	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	switch (led->id) {
	case QPNP_ID_LED_MPP:
		pwm_cfg = led->mpp_cfg->pwm_cfg;
		break;
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		break;
	case QPNP_ID_KPDBL:
		pwm_cfg = led->kpdbl_cfg->pwm_cfg;
		break;
	default:
		dev_err(&led->pdev->dev,
			"Invalid LED id type for ramp step\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	previous_ramp_step_ms = pwm_cfg->lut_params.ramp_step_ms;

	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	pwm_cfg->lut_params.ramp_step_ms = ramp_step_ms;
	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret) {
		pwm_cfg->lut_params.ramp_step_ms = previous_ramp_step_ms;
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		qpnp_led_set(&led->cdev, led->cdev.brightness);
		dev_err(&led->pdev->dev,
			"Failed to initialize pwm with new ramp step value\n");
		return ret;
	}
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return count;
}

static ssize_t lut_flags_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	u32 lut_flags;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret;
	u32 previous_lut_flags;
	struct pwm_config_data *pwm_cfg;

	ret = kstrtou32(buf, 10, &lut_flags);
	if (ret)
		return ret;
	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	switch (led->id) {
	case QPNP_ID_LED_MPP:
		pwm_cfg = led->mpp_cfg->pwm_cfg;
		break;
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		break;
	case QPNP_ID_KPDBL:
		pwm_cfg = led->kpdbl_cfg->pwm_cfg;
		break;
	default:
		dev_err(&led->pdev->dev,
			"Invalid LED id type for lut flags\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	previous_lut_flags = pwm_cfg->lut_params.flags;

	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	pwm_cfg->lut_params.flags = lut_flags;
	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret) {
		pwm_cfg->lut_params.flags = previous_lut_flags;
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		qpnp_led_set(&led->cdev, led->cdev.brightness);
		dev_err(&led->pdev->dev,
			"Failed to initialize pwm with new lut flags value\n");
		return ret;
	}
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return count;
}

static ssize_t duty_pcts_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	int num_duty_pcts = 0;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	char *buffer;
	ssize_t ret;
	int i = 0;
	int max_duty_pcts;
	struct pwm_config_data *pwm_cfg;
	u32 previous_num_duty_pcts;
	int value;
	int *previous_duty_pcts;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	switch (led->id) {
	case QPNP_ID_LED_MPP:
		pwm_cfg = led->mpp_cfg->pwm_cfg;
		max_duty_pcts = PWM_LUT_MAX_SIZE;
		break;
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		pwm_cfg = led->rgb_cfg->pwm_cfg;
		max_duty_pcts = PWM_LUT_MAX_SIZE;
		break;
	case QPNP_ID_KPDBL:
		pwm_cfg = led->kpdbl_cfg->pwm_cfg;
		max_duty_pcts = PWM_GPLED_LUT_MAX_SIZE;
		break;
	default:
		dev_err(&led->pdev->dev,
			"Invalid LED id type for duty pcts\n");
		return -EINVAL;
	}

	if (pwm_cfg->mode == LPG_MODE)
		pwm_cfg->blinking = true;

	buffer = (char *)buf;

	for (i = 0; i < max_duty_pcts; i++) {
		if (buffer == NULL)
			break;
		ret = sscanf((const char *)buffer, "%u,%s", &value, buffer);
		pwm_cfg->old_duty_pcts[i] = value;
		num_duty_pcts++;
		if (ret <= 1)
			break;
	}

	if (num_duty_pcts >= max_duty_pcts) {
		dev_err(&led->pdev->dev,
			"Number of duty pcts given exceeds max (%d)\n",
			max_duty_pcts);
		return -EINVAL;
	}

	previous_num_duty_pcts = pwm_cfg->duty_cycles->num_duty_pcts;
	previous_duty_pcts = pwm_cfg->duty_cycles->duty_pcts;

	pwm_cfg->duty_cycles->num_duty_pcts = num_duty_pcts;
	pwm_cfg->duty_cycles->duty_pcts = pwm_cfg->old_duty_pcts;
	pwm_cfg->old_duty_pcts = previous_duty_pcts;
	pwm_cfg->lut_params.idx_len = pwm_cfg->duty_cycles->num_duty_pcts;

	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}

	ret = qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	if (ret)
		goto restore;

	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return count;

restore:
	dev_err(&led->pdev->dev,
		"Failed to initialize pwm with new duty pcts value\n");
	pwm_cfg->duty_cycles->num_duty_pcts = previous_num_duty_pcts;
	pwm_cfg->old_duty_pcts = pwm_cfg->duty_cycles->duty_pcts;
	pwm_cfg->duty_cycles->duty_pcts = previous_duty_pcts;
	pwm_cfg->lut_params.idx_len = pwm_cfg->duty_cycles->num_duty_pcts;
	if (pwm_cfg->pwm_enabled) {
		pwm_disable(pwm_cfg->pwm_dev);
		pwm_cfg->pwm_enabled = 0;
	}
	qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
	qpnp_led_set(&led->cdev, led->cdev.brightness);
	return ret;
}

static void led_blink(struct qpnp_led_data *led,
			struct pwm_config_data *pwm_cfg)
{
	int rc;

	flush_work(&led->work);
	mutex_lock(&led->lock);
	if (pwm_cfg->use_blink) {
		if (led->cdev.brightness) {
			pwm_cfg->blinking = true;
			if (led->id == QPNP_ID_LED_MPP)
				led->mpp_cfg->pwm_mode = LPG_MODE;
			else if (led->id == QPNP_ID_KPDBL)
				led->kpdbl_cfg->pwm_mode = LPG_MODE;
			pwm_cfg->mode = LPG_MODE;
		} else {
			pwm_cfg->blinking = false;
			pwm_cfg->mode = pwm_cfg->default_mode;
			if (led->id == QPNP_ID_LED_MPP)
				led->mpp_cfg->pwm_mode = pwm_cfg->default_mode;
			else if (led->id == QPNP_ID_KPDBL)
				led->kpdbl_cfg->pwm_mode =
						pwm_cfg->default_mode;
		}
		if (pwm_cfg->pwm_enabled) {
			pwm_disable(pwm_cfg->pwm_dev);
			pwm_cfg->pwm_enabled = 0;
		}
		qpnp_pwm_init(pwm_cfg, led->pdev, led->cdev.name);
		if (led->id == QPNP_ID_RGB_RED || led->id == QPNP_ID_RGB_GREEN
				|| led->id == QPNP_ID_RGB_BLUE) {
			rc = qpnp_rgb_set(led);
			if (rc < 0)
				dev_err(&led->pdev->dev,
				"RGB set brightness failed (%d)\n", rc);
		} else if (led->id == QPNP_ID_LED_MPP) {
			rc = qpnp_mpp_set(led);
			if (rc < 0)
				dev_err(&led->pdev->dev,
				"MPP set brightness failed (%d)\n", rc);
		} else if (led->id == QPNP_ID_KPDBL) {
			rc = qpnp_kpdbl_set(led);
			if (rc < 0)
				dev_err(&led->pdev->dev,
				"KPDBL set brightness failed (%d)\n", rc);
		}
	}
	mutex_unlock(&led->lock);
}

static ssize_t blink_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t count)
{
	struct qpnp_led_data *led;
	unsigned long blinking;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	ssize_t ret = -EINVAL;

	ret = kstrtoul(buf, 10, &blinking);
	if (ret)
		return ret;
	led = container_of(led_cdev, struct qpnp_led_data, cdev);
	led->cdev.brightness = blinking ? led->cdev.max_brightness : 0;

	switch (led->id) {
	case QPNP_ID_LED_MPP:
		led_blink(led, led->mpp_cfg->pwm_cfg);
		break;
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		led_blink(led, led->rgb_cfg->pwm_cfg);
		break;
	case QPNP_ID_KPDBL:
		led_blink(led, led->kpdbl_cfg->pwm_cfg);
		break;
	default:
		dev_err(&led->pdev->dev, "Invalid LED id type for blink\n");
		return -EINVAL;
	}
	return count;
}

static DEVICE_ATTR(led_mode, 0664, NULL, led_mode_store);
static DEVICE_ATTR(strobe, 0664, NULL, led_strobe_type_store);
static DEVICE_ATTR(pwm_us, 0664, NULL, pwm_us_store);
static DEVICE_ATTR(pause_lo, 0664, NULL, pause_lo_store);
static DEVICE_ATTR(pause_hi, 0664, NULL, pause_hi_store);
static DEVICE_ATTR(start_idx, 0664, NULL, start_idx_store);
static DEVICE_ATTR(ramp_step_ms, 0664, NULL, ramp_step_ms_store);
static DEVICE_ATTR(lut_flags, 0664, NULL, lut_flags_store);
static DEVICE_ATTR(duty_pcts, 0664, NULL, duty_pcts_store);
static DEVICE_ATTR(blink, 0664, NULL, blink_store);

static struct attribute *led_attrs[] = {
	&dev_attr_led_mode.attr,
	&dev_attr_strobe.attr,
	NULL
};

static const struct attribute_group led_attr_group = {
	.attrs = led_attrs,
};

static struct attribute *pwm_attrs[] = {
	&dev_attr_pwm_us.attr,
	NULL
};

static struct attribute *lpg_attrs[] = {
	&dev_attr_pause_lo.attr,
	&dev_attr_pause_hi.attr,
	&dev_attr_start_idx.attr,
	&dev_attr_ramp_step_ms.attr,
	&dev_attr_lut_flags.attr,
	&dev_attr_duty_pcts.attr,
	NULL
};

static struct attribute *blink_attrs[] = {
	&dev_attr_blink.attr,
	NULL
};

static const struct attribute_group pwm_attr_group = {
	.attrs = pwm_attrs,
};

static const struct attribute_group lpg_attr_group = {
	.attrs = lpg_attrs,
};

static const struct attribute_group blink_attr_group = {
	.attrs = blink_attrs,
};

static int qpnp_flash_init(struct qpnp_led_data *led)
{
	int rc;

	led->flash_cfg->flash_on = false;

	rc = qpnp_led_masked_write(led,
		FLASH_LED_STROBE_CTRL(led->base),
		FLASH_STROBE_MASK, FLASH_DISABLE_ALL);
	if (rc) {
		dev_err(&led->pdev->dev,
			"LED %d flash write failed(%d)\n", led->id, rc);
		return rc;
	}

	/* Disable flash LED module */
	rc = qpnp_led_masked_write(led, FLASH_ENABLE_CONTROL(led->base),
		FLASH_ENABLE_MASK, FLASH_DISABLE_ALL);
	if (rc) {
		dev_err(&led->pdev->dev, "Enable reg write failed(%d)\n", rc);
		return rc;
	}

	if (led->flash_cfg->torch_enable)
		return 0;

	/* Set headroom */
	rc = qpnp_led_masked_write(led, FLASH_HEADROOM(led->base),
		FLASH_HEADROOM_MASK, led->flash_cfg->headroom);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Headroom reg write failed(%d)\n", rc);
		return rc;
	}

	/* Set startup delay */
	rc = qpnp_led_masked_write(led,
		FLASH_STARTUP_DELAY(led->base), FLASH_STARTUP_DLY_MASK,
		led->flash_cfg->startup_dly);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Startup delay reg write failed(%d)\n", rc);
		return rc;
	}

	/* Set timer control - safety or watchdog */
	if (led->flash_cfg->safety_timer) {
		rc = qpnp_led_masked_write(led,
			FLASH_LED_TMR_CTRL(led->base),
			FLASH_TMR_MASK, FLASH_TMR_SAFETY);
		if (rc) {
			dev_err(&led->pdev->dev,
				"LED timer ctrl reg write failed(%d)\n",
				rc);
			return rc;
		}
	}

	/* Set Vreg force */
	if (led->flash_cfg->vreg_ok)
		rc = qpnp_led_masked_write(led,	FLASH_VREG_OK_FORCE(led->base),
			FLASH_VREG_MASK, FLASH_SW_VREG_OK);
	else
		rc = qpnp_led_masked_write(led, FLASH_VREG_OK_FORCE(led->base),
			FLASH_VREG_MASK, FLASH_HW_VREG_OK);

	if (rc) {
		dev_err(&led->pdev->dev,
			"Vreg OK reg write failed(%d)\n", rc);
		return rc;
	}

	/* Set self fault check */
	rc = qpnp_led_masked_write(led, FLASH_FAULT_DETECT(led->base),
		FLASH_FAULT_DETECT_MASK, FLASH_SELFCHECK_ENABLE);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Fault detect reg write failed(%d)\n", rc);
		return rc;
	}

	/* Set mask enable */
	rc = qpnp_led_masked_write(led, FLASH_MASK_ENABLE(led->base),
		FLASH_MASK_REG_MASK, FLASH_MASK_1);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Mask enable reg write failed(%d)\n", rc);
		return rc;
	}

	/* Set current ramp */
	rc = qpnp_led_masked_write(led, FLASH_CURRENT_RAMP(led->base),
		FLASH_CURRENT_RAMP_MASK, FLASH_RAMP_STEP_27US);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Current ramp reg write failed(%d)\n", rc);
		return rc;
	}

	led->flash_cfg->strobe_type = 0;

	/* dump flash registers */
	qpnp_dump_regs(led, flash_debug_regs, ARRAY_SIZE(flash_debug_regs));

	return 0;
}

static int qpnp_kpdbl_init(struct qpnp_led_data *led)
{
	int rc;
	uint val;

	/* select row source - vbst or vph */
	rc = regmap_read(led->regmap, KPDBL_ROW_SRC_SEL(led->base), &val);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Unable to read from addr=%x, rc(%d)\n",
			KPDBL_ROW_SRC_SEL(led->base), rc);
		return rc;
	}

	if (led->kpdbl_cfg->row_src_vbst)
		val |= 1 << led->kpdbl_cfg->row_id;
	else
		val &= ~(1 << led->kpdbl_cfg->row_id);

	rc = regmap_write(led->regmap, KPDBL_ROW_SRC_SEL(led->base), val);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Unable to read from addr=%x, rc(%d)\n",
			KPDBL_ROW_SRC_SEL(led->base), rc);
		return rc;
	}

	/* row source enable */
	rc = regmap_read(led->regmap, KPDBL_ROW_SRC(led->base), &val);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Unable to read from addr=%x, rc(%d)\n",
			KPDBL_ROW_SRC(led->base), rc);
		return rc;
	}

	if (led->kpdbl_cfg->row_src_en)
		val |= KPDBL_ROW_SCAN_EN_MASK | (1 << led->kpdbl_cfg->row_id);
	else
		val &= ~(1 << led->kpdbl_cfg->row_id);

	rc = regmap_write(led->regmap, KPDBL_ROW_SRC(led->base), val);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Unable to write to addr=%x, rc(%d)\n",
			KPDBL_ROW_SRC(led->base), rc);
		return rc;
	}

	/* enable module */
	rc = qpnp_led_masked_write(led, KPDBL_ENABLE(led->base),
		KPDBL_MODULE_EN_MASK, KPDBL_MODULE_EN);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Enable module write failed(%d)\n", rc);
		return rc;
	}

	rc = qpnp_pwm_init(led->kpdbl_cfg->pwm_cfg, led->pdev,
				led->cdev.name);
	if (rc) {
		dev_err(&led->pdev->dev, "Failed to initialize pwm\n");
		return rc;
	}

	if (led->kpdbl_cfg->always_on) {
		kpdbl_master = led->kpdbl_cfg->pwm_cfg->pwm_dev;
		kpdbl_master_period_us = led->kpdbl_cfg->pwm_cfg->pwm_period_us;
	}

	/* dump kpdbl registers */
	qpnp_dump_regs(led, kpdbl_debug_regs, ARRAY_SIZE(kpdbl_debug_regs));

	return 0;
}

static int qpnp_rgb_init(struct qpnp_led_data *led)
{
	int rc;

	rc = qpnp_led_masked_write(led, RGB_LED_SRC_SEL(led->base),
		RGB_LED_SRC_MASK, RGB_LED_SOURCE_VPH_PWR);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Failed to write led source select register\n");
		return rc;
	}

	rc = qpnp_pwm_init(led->rgb_cfg->pwm_cfg, led->pdev, led->cdev.name);
	if (rc) {
		dev_err(&led->pdev->dev, "Failed to initialize pwm\n");
		return rc;
	}
	/* Initialize led for use in auto trickle charging mode */
	rc = qpnp_led_masked_write(led, RGB_LED_ATC_CTL(led->base),
		led->rgb_cfg->enable, led->rgb_cfg->enable);

	return 0;
}

static int qpnp_mpp_init(struct qpnp_led_data *led)
{
	int rc;
	u8 val;


	if (led->max_current < LED_MPP_CURRENT_MIN ||
		led->max_current > LED_MPP_CURRENT_MAX) {
		dev_err(&led->pdev->dev,
			"max current for mpp is not valid\n");
		return -EINVAL;
	}

	val = (led->mpp_cfg->current_setting / LED_MPP_CURRENT_PER_SETTING) - 1;

	if (val < 0)
		val = 0;

	rc = qpnp_led_masked_write(led, LED_MPP_VIN_CTRL(led->base),
		LED_MPP_VIN_MASK, led->mpp_cfg->vin_ctrl);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Failed to write led vin control reg\n");
		return rc;
	}

	rc = qpnp_led_masked_write(led, LED_MPP_SINK_CTRL(led->base),
		LED_MPP_SINK_MASK, val);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Failed to write sink control reg\n");
		return rc;
	}

	if (led->mpp_cfg->pwm_mode != MANUAL_MODE) {
		rc = qpnp_pwm_init(led->mpp_cfg->pwm_cfg, led->pdev,
					led->cdev.name);
		if (rc) {
			dev_err(&led->pdev->dev,
				"Failed to initialize pwm\n");
			return rc;
		}
	}

	return 0;
}

static int qpnp_gpio_init(struct qpnp_led_data *led)
{
	int rc;

	rc = qpnp_led_masked_write(led, LED_GPIO_VIN_CTRL(led->base),
		LED_GPIO_VIN_MASK, led->gpio_cfg->vin_ctrl);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Failed to write led vin control reg\n");
		return rc;
	}

	return 0;
}

static int qpnp_led_initialize(struct qpnp_led_data *led)
{
	int rc = 0;

	switch (led->id) {
	case QPNP_ID_WLED:
		rc = qpnp_wled_init(led);
		if (rc)
			dev_err(&led->pdev->dev,
				"WLED initialize failed(%d)\n", rc);
		break;
	case QPNP_ID_FLASH1_LED0:
	case QPNP_ID_FLASH1_LED1:
		rc = qpnp_flash_init(led);
		if (rc)
			dev_err(&led->pdev->dev,
				"FLASH initialize failed(%d)\n", rc);
		break;
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		rc = qpnp_rgb_init(led);
		if (rc)
			dev_err(&led->pdev->dev,
				"RGB initialize failed(%d)\n", rc);
		break;
	case QPNP_ID_LED_MPP:
		rc = qpnp_mpp_init(led);
		if (rc)
			dev_err(&led->pdev->dev,
				"MPP initialize failed(%d)\n", rc);
		break;
	case QPNP_ID_LED_GPIO:
		rc = qpnp_gpio_init(led);
		if (rc)
			dev_err(&led->pdev->dev,
				"GPIO initialize failed(%d)\n", rc);
		break;
	case QPNP_ID_KPDBL:
		rc = qpnp_kpdbl_init(led);
		if (rc)
			dev_err(&led->pdev->dev,
				"KPDBL initialize failed(%d)\n", rc);
		break;
	default:
		dev_err(&led->pdev->dev, "Invalid LED(%d)\n", led->id);
		return -EINVAL;
	}

	return rc;
}

static int qpnp_get_common_configs(struct qpnp_led_data *led,
				struct device_node *node)
{
	int rc;
	u32 val;
	const char *temp_string;

	led->cdev.default_trigger = LED_TRIGGER_DEFAULT;
	rc = of_property_read_string(node, "linux,default-trigger",
		&temp_string);
	if (!rc)
		led->cdev.default_trigger = temp_string;
	else if (rc != -EINVAL)
		return rc;

	led->default_on = false;
	rc = of_property_read_string(node, "qcom,default-state",
		&temp_string);
	if (!rc) {
		if (strcmp(temp_string, "on") == 0)
			led->default_on = true;
	} else if (rc != -EINVAL)
		return rc;

	led->turn_off_delay_ms = 0;
	rc = of_property_read_u32(node, "qcom,turn-off-delay-ms", &val);
	if (!rc)
		led->turn_off_delay_ms = val;
	else if (rc != -EINVAL)
		return rc;

	return 0;
}

/*
 * Handlers for alternative sources of platform_data
 */
static int qpnp_get_config_wled(struct qpnp_led_data *led,
				struct device_node *node)
{
	u32 val;
	uint tmp;
	int rc;

	led->wled_cfg = devm_kzalloc(&led->pdev->dev,
				sizeof(struct wled_config_data), GFP_KERNEL);
	if (!led->wled_cfg)
		return -ENOMEM;

	rc = regmap_read(led->regmap, PMIC_VERSION_REG, &tmp);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Unable to read pmic ver, rc(%d)\n", rc);
	}
	led->wled_cfg->pmic_version = (u8)tmp;

	led->wled_cfg->num_strings = WLED_DEFAULT_STRINGS;
	rc = of_property_read_u32(node, "qcom,num-strings", &val);
	if (!rc)
		led->wled_cfg->num_strings = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->num_physical_strings = led->wled_cfg->num_strings;
	rc = of_property_read_u32(node, "qcom,num-physical-strings", &val);
	if (!rc)
		led->wled_cfg->num_physical_strings = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->ovp_val = WLED_DEFAULT_OVP_VAL;
	rc = of_property_read_u32(node, "qcom,ovp-val", &val);
	if (!rc)
		led->wled_cfg->ovp_val = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->boost_curr_lim = WLED_BOOST_LIM_DEFAULT;
	rc = of_property_read_u32(node, "qcom,boost-curr-lim", &val);
	if (!rc)
		led->wled_cfg->boost_curr_lim = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->cp_select = WLED_CP_SEL_DEFAULT;
	rc = of_property_read_u32(node, "qcom,cp-sel", &val);
	if (!rc)
		led->wled_cfg->cp_select = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->ctrl_delay_us = WLED_CTRL_DLY_DEFAULT;
	rc = of_property_read_u32(node, "qcom,ctrl-delay-us", &val);
	if (!rc)
		led->wled_cfg->ctrl_delay_us = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->op_fdbck = WLED_OP_FDBCK_DEFAULT;
	rc = of_property_read_u32(node, "qcom,op-fdbck", &val);
	if (!rc)
		led->wled_cfg->op_fdbck = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->switch_freq = WLED_SWITCH_FREQ_DEFAULT;
	rc = of_property_read_u32(node, "qcom,switch-freq", &val);
	if (!rc)
		led->wled_cfg->switch_freq = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->wled_cfg->dig_mod_gen_en =
		of_property_read_bool(node, "qcom,dig-mod-gen-en");

	led->wled_cfg->cs_out_en =
		of_property_read_bool(node, "qcom,cs-out-en");

	return 0;
}

static int qpnp_get_config_flash(struct qpnp_led_data *led,
				struct device_node *node, bool *reg_set)
{
	int rc;
	u32 val;
	uint tmp;

	led->flash_cfg = devm_kzalloc(&led->pdev->dev,
				sizeof(struct flash_config_data), GFP_KERNEL);
	if (!led->flash_cfg)
		return -ENOMEM;

	rc = regmap_read(led->regmap, FLASH_PERIPHERAL_SUBTYPE(led->base),
			&tmp);
	if (rc) {
		dev_err(&led->pdev->dev,
			"Unable to read from addr=%x, rc(%d)\n",
			FLASH_PERIPHERAL_SUBTYPE(led->base), rc);
	}
	led->flash_cfg->peripheral_subtype = (u8)tmp;

	led->flash_cfg->torch_enable =
		of_property_read_bool(node, "qcom,torch-enable");

	led->flash_cfg->no_smbb_support =
		of_property_read_bool(node, "qcom,no-smbb-support");

	if (of_find_property(of_get_parent(node), "flash-wa-supply",
					NULL) && (!*reg_set)) {
		led->flash_cfg->flash_wa_reg =
			devm_regulator_get(&led->pdev->dev, "flash-wa");
		if (IS_ERR_OR_NULL(led->flash_cfg->flash_wa_reg)) {
			rc = PTR_ERR(led->flash_cfg->flash_wa_reg);
			if (rc != EPROBE_DEFER) {
				dev_err(&led->pdev->dev,
					"Flash wa regulator get failed(%d)\n",
					rc);
			}
		} else {
			led->flash_cfg->flash_wa_reg_get = true;
		}
	}

	if (led->id == QPNP_ID_FLASH1_LED0) {
		led->flash_cfg->enable_module = FLASH_ENABLE_LED_0;
		led->flash_cfg->current_addr = FLASH_LED_0_CURR(led->base);
		led->flash_cfg->trigger_flash = FLASH_LED_0_OUTPUT;
		if (!*reg_set) {
			led->flash_cfg->flash_boost_reg =
				regulator_get(&led->pdev->dev,
							"flash-boost");
			if (IS_ERR(led->flash_cfg->flash_boost_reg)) {
				rc = PTR_ERR(led->flash_cfg->flash_boost_reg);
				dev_err(&led->pdev->dev,
					"Regulator get failed(%d)\n", rc);
				goto error_get_flash_reg;
			}
			led->flash_cfg->flash_reg_get = true;
			*reg_set = true;
		} else
			led->flash_cfg->flash_reg_get = false;

		if (led->flash_cfg->torch_enable) {
			led->flash_cfg->second_addr =
						FLASH_LED_1_CURR(led->base);
		}
	} else if (led->id == QPNP_ID_FLASH1_LED1) {
		led->flash_cfg->enable_module = FLASH_ENABLE_LED_1;
		led->flash_cfg->current_addr = FLASH_LED_1_CURR(led->base);
		led->flash_cfg->trigger_flash = FLASH_LED_1_OUTPUT;
		if (!*reg_set) {
			led->flash_cfg->flash_boost_reg =
					regulator_get(&led->pdev->dev,
								"flash-boost");
			if (IS_ERR(led->flash_cfg->flash_boost_reg)) {
				rc = PTR_ERR(led->flash_cfg->flash_boost_reg);
				dev_err(&led->pdev->dev,
					"Regulator get failed(%d)\n", rc);
				goto error_get_flash_reg;
			}
			led->flash_cfg->flash_reg_get = true;
			*reg_set = true;
		} else
			led->flash_cfg->flash_reg_get = false;

		if (led->flash_cfg->torch_enable) {
			led->flash_cfg->second_addr =
						FLASH_LED_0_CURR(led->base);
		}
	} else {
		dev_err(&led->pdev->dev, "Unknown flash LED name given\n");
		return -EINVAL;
	}

	if (led->flash_cfg->torch_enable) {
		if (of_find_property(of_get_parent(node), "torch-boost-supply",
									NULL)) {
			if (!led->flash_cfg->no_smbb_support) {
				led->flash_cfg->torch_boost_reg =
					regulator_get(&led->pdev->dev,
								"torch-boost");
				if (IS_ERR(led->flash_cfg->torch_boost_reg)) {
					rc = PTR_ERR(led->flash_cfg->
							torch_boost_reg);
					dev_err(&led->pdev->dev,
					"Torch regulator get failed(%d)\n", rc);
					goto error_get_torch_reg;
				}
			}
			led->flash_cfg->enable_module = FLASH_ENABLE_MODULE;
		} else
			led->flash_cfg->enable_module = FLASH_ENABLE_ALL;
		led->flash_cfg->trigger_flash = FLASH_TORCH_OUTPUT;

		rc = of_property_read_u32(node, "qcom,duration", &val);
		if (!rc)
			led->flash_cfg->duration = ((u8) val) - 2;
		else if (rc == -EINVAL)
			led->flash_cfg->duration = TORCH_DURATION_12s;
		else {
			if (led->flash_cfg->peripheral_subtype ==
							FLASH_SUBTYPE_SINGLE)
				goto error_get_flash_reg;
			else if (led->flash_cfg->peripheral_subtype ==
							FLASH_SUBTYPE_DUAL)
				goto error_get_torch_reg;
		}

		rc = of_property_read_u32(node, "qcom,current", &val);
		if (!rc)
			led->flash_cfg->current_prgm = (val *
				TORCH_MAX_LEVEL / led->max_current);
		else {
			if (led->flash_cfg->peripheral_subtype ==
							FLASH_SUBTYPE_SINGLE)
				goto error_get_flash_reg;
			else if (led->flash_cfg->peripheral_subtype ==
							FLASH_SUBTYPE_DUAL)
				goto error_get_torch_reg;
			goto error_get_torch_reg;
		}

		return 0;
	}

	rc = of_property_read_u32(node, "qcom,duration", &val);
	if (!rc)
		led->flash_cfg->duration = (u8)((val - 10) / 10);
	else if (rc == -EINVAL)
		led->flash_cfg->duration = FLASH_DURATION_200ms;
	else
		goto error_get_flash_reg;

	rc = of_property_read_u32(node, "qcom,current", &val);
	if (!rc)
		led->flash_cfg->current_prgm = val * FLASH_MAX_LEVEL
						/ led->max_current;
	else
		goto error_get_flash_reg;

	rc = of_property_read_u32(node, "qcom,headroom", &val);
	if (!rc)
		led->flash_cfg->headroom = (u8) val;
	else if (rc == -EINVAL)
		led->flash_cfg->headroom = HEADROOM_500mV;
	else
		goto error_get_flash_reg;

	rc = of_property_read_u32(node, "qcom,clamp-curr", &val);
	if (!rc)
		led->flash_cfg->clamp_curr = (val *
				FLASH_MAX_LEVEL / led->max_current);
	else if (rc == -EINVAL)
		led->flash_cfg->clamp_curr = FLASH_CLAMP_200mA;
	else
		goto error_get_flash_reg;

	rc = of_property_read_u32(node, "qcom,startup-dly", &val);
	if (!rc)
		led->flash_cfg->startup_dly = (u8) val;
	else if (rc == -EINVAL)
		led->flash_cfg->startup_dly = DELAY_128us;
	else
		goto error_get_flash_reg;

	led->flash_cfg->safety_timer =
		of_property_read_bool(node, "qcom,safety-timer");

	led->flash_cfg->vreg_ok =
		of_property_read_bool(node, "qcom,sw_vreg_ok");

	return 0;

error_get_torch_reg:
	if (led->flash_cfg->no_smbb_support)
		regulator_put(led->flash_cfg->flash_boost_reg);
	else
		regulator_put(led->flash_cfg->torch_boost_reg);

error_get_flash_reg:
	regulator_put(led->flash_cfg->flash_boost_reg);
	return rc;

}

static int qpnp_get_config_pwm(struct pwm_config_data *pwm_cfg,
				struct platform_device *pdev,
				struct device_node *node)
{
	struct property *prop;
	int rc, i, lut_max_size;
	u32 val;
	u8 *temp_cfg;
	const char *led_label;

	pwm_cfg->pwm_dev = of_pwm_get(node, NULL);

	if (IS_ERR(pwm_cfg->pwm_dev)) {
		rc = PTR_ERR(pwm_cfg->pwm_dev);
		dev_err(&pdev->dev, "Cannot get PWM device rc:(%d)\n", rc);
		pwm_cfg->pwm_dev = NULL;
		return rc;
	}

	if (pwm_cfg->mode != MANUAL_MODE) {
		rc = of_property_read_u32(node, "qcom,pwm-us", &val);
		if (!rc)
			pwm_cfg->pwm_period_us = val;
		else
			return rc;
	}

	pwm_cfg->use_blink =
		of_property_read_bool(node, "qcom,use-blink");

	if (pwm_cfg->mode == LPG_MODE || pwm_cfg->use_blink) {
		pwm_cfg->duty_cycles =
			devm_kzalloc(&pdev->dev,
			sizeof(struct pwm_duty_cycles), GFP_KERNEL);
		if (!pwm_cfg->duty_cycles) {
			dev_err(&pdev->dev, "Unable to allocate memory\n");
			rc = -ENOMEM;
			goto bad_lpg_params;
		}

		prop = of_find_property(node, "qcom,duty-pcts",
			&pwm_cfg->duty_cycles->num_duty_pcts);
		if (!prop) {
			dev_err(&pdev->dev, "Looking up property node qcom,duty-pcts failed\n");
			rc =  -ENODEV;
			goto bad_lpg_params;
		} else if (!pwm_cfg->duty_cycles->num_duty_pcts) {
			dev_err(&pdev->dev, "Invalid length of duty pcts\n");
			rc =  -EINVAL;
			goto bad_lpg_params;
		}

		rc = of_property_read_string(node, "label", &led_label);

		if (rc < 0) {
			dev_err(&pdev->dev,
				"Failure reading label, rc = %d\n", rc);
			return rc;
		}

		if (strcmp(led_label, "kpdbl") == 0)
			lut_max_size = PWM_GPLED_LUT_MAX_SIZE;
		else
			lut_max_size = PWM_LUT_MAX_SIZE;

		pwm_cfg->duty_cycles->duty_pcts =
			devm_kzalloc(&pdev->dev,
			sizeof(int) * lut_max_size,
			GFP_KERNEL);
		if (!pwm_cfg->duty_cycles->duty_pcts) {
			dev_err(&pdev->dev, "Unable to allocate memory\n");
			rc = -ENOMEM;
			goto bad_lpg_params;
		}

		pwm_cfg->old_duty_pcts =
			devm_kzalloc(&pdev->dev,
			sizeof(int) * lut_max_size,
			GFP_KERNEL);
		if (!pwm_cfg->old_duty_pcts) {
			dev_err(&pdev->dev, "Unable to allocate memory\n");
			rc = -ENOMEM;
			goto bad_lpg_params;
		}

		temp_cfg = devm_kzalloc(&pdev->dev,
				pwm_cfg->duty_cycles->num_duty_pcts *
				sizeof(u8), GFP_KERNEL);
		if (!temp_cfg) {
			dev_err(&pdev->dev, "Failed to allocate memory for duty pcts\n");
			rc = -ENOMEM;
			goto bad_lpg_params;
		}

		memcpy(temp_cfg, prop->value,
			pwm_cfg->duty_cycles->num_duty_pcts);

		for (i = 0; i < pwm_cfg->duty_cycles->num_duty_pcts; i++)
			pwm_cfg->duty_cycles->duty_pcts[i] =
				(int) temp_cfg[i];

		rc = of_property_read_u32(node, "qcom,start-idx", &val);
		if (!rc) {
			pwm_cfg->lut_params.start_idx = val;
			pwm_cfg->duty_cycles->start_idx = val;
		} else
			goto bad_lpg_params;

		pwm_cfg->lut_params.lut_pause_hi = 0;
		rc = of_property_read_u32(node, "qcom,pause-hi", &val);
		if (!rc)
			pwm_cfg->lut_params.lut_pause_hi = val;
		else if (rc != -EINVAL)
			goto bad_lpg_params;

		pwm_cfg->lut_params.lut_pause_lo = 0;
		rc = of_property_read_u32(node, "qcom,pause-lo", &val);
		if (!rc)
			pwm_cfg->lut_params.lut_pause_lo = val;
		else if (rc != -EINVAL)
			goto bad_lpg_params;

		pwm_cfg->lut_params.ramp_step_ms =
				QPNP_LUT_RAMP_STEP_DEFAULT;
		rc = of_property_read_u32(node, "qcom,ramp-step-ms", &val);
		if (!rc)
			pwm_cfg->lut_params.ramp_step_ms = val;
		else if (rc != -EINVAL)
			goto bad_lpg_params;

		pwm_cfg->lut_params.flags = QPNP_LED_PWM_FLAGS;
		rc = of_property_read_u32(node, "qcom,lut-flags", &val);
		if (!rc)
			pwm_cfg->lut_params.flags = (u8) val;
		else if (rc != -EINVAL)
			goto bad_lpg_params;

		pwm_cfg->lut_params.idx_len =
			pwm_cfg->duty_cycles->num_duty_pcts;
	}
	return 0;

bad_lpg_params:
	pwm_cfg->use_blink = false;
	if (pwm_cfg->mode == PWM_MODE) {
		dev_err(&pdev->dev, "LPG parameters not set for blink mode, defaulting to PWM mode\n");
		return 0;
	}
	return rc;
};

static int qpnp_led_get_mode(const char *mode)
{
	if (strcmp(mode, "manual") == 0)
		return MANUAL_MODE;
	else if (strcmp(mode, "pwm") == 0)
		return PWM_MODE;
	else if (strcmp(mode, "lpg") == 0)
		return LPG_MODE;
	else
		return -EINVAL;
};

static int qpnp_get_config_kpdbl(struct qpnp_led_data *led,
				struct device_node *node)
{
	int rc;
	u32 val;
	u8 led_mode;
	const char *mode;

	led->kpdbl_cfg = devm_kzalloc(&led->pdev->dev,
				sizeof(struct kpdbl_config_data), GFP_KERNEL);
	if (!led->kpdbl_cfg)
		return -ENOMEM;

	rc = of_property_read_string(node, "qcom,mode", &mode);
	if (!rc) {
		led_mode = qpnp_led_get_mode(mode);
		if ((led_mode == MANUAL_MODE) || (led_mode == -EINVAL)) {
			dev_err(&led->pdev->dev, "Selected mode not supported for kpdbl.\n");
			return -EINVAL;
		}
		led->kpdbl_cfg->pwm_cfg = devm_kzalloc(&led->pdev->dev,
					sizeof(struct pwm_config_data),
					GFP_KERNEL);
		if (!led->kpdbl_cfg->pwm_cfg)
			return -ENOMEM;

		led->kpdbl_cfg->pwm_cfg->mode = led_mode;
		led->kpdbl_cfg->pwm_cfg->default_mode = led_mode;
	} else {
		return rc;
	}

	rc = qpnp_get_config_pwm(led->kpdbl_cfg->pwm_cfg, led->pdev,  node);
	if (rc < 0) {
		if (led->kpdbl_cfg->pwm_cfg->pwm_dev)
			pwm_put(led->kpdbl_cfg->pwm_cfg->pwm_dev);
		return rc;
	}

	rc = of_property_read_u32(node, "qcom,row-id", &val);
	if (!rc)
		led->kpdbl_cfg->row_id = val;
	else
		return rc;

	led->kpdbl_cfg->row_src_vbst =
			of_property_read_bool(node, "qcom,row-src-vbst");

	led->kpdbl_cfg->row_src_en =
			of_property_read_bool(node, "qcom,row-src-en");

	led->kpdbl_cfg->always_on =
			of_property_read_bool(node, "qcom,always-on");

	return 0;
}

static int qpnp_get_config_rgb(struct qpnp_led_data *led,
				struct device_node *node)
{
	int rc;
	u8 led_mode;
	const char *mode;

	led->rgb_cfg = devm_kzalloc(&led->pdev->dev,
				sizeof(struct rgb_config_data), GFP_KERNEL);
	if (!led->rgb_cfg)
		return -ENOMEM;

	if (led->id == QPNP_ID_RGB_RED)
		led->rgb_cfg->enable = RGB_LED_ENABLE_RED;
	else if (led->id == QPNP_ID_RGB_GREEN)
		led->rgb_cfg->enable = RGB_LED_ENABLE_GREEN;
	else if (led->id == QPNP_ID_RGB_BLUE)
		led->rgb_cfg->enable = RGB_LED_ENABLE_BLUE;
	else
		return -EINVAL;

	rc = of_property_read_string(node, "qcom,mode", &mode);
	if (!rc) {
		led_mode = qpnp_led_get_mode(mode);
		if ((led_mode == MANUAL_MODE) || (led_mode == -EINVAL)) {
			dev_err(&led->pdev->dev, "Selected mode not supported for rgb\n");
			return -EINVAL;
		}
		led->rgb_cfg->pwm_cfg = devm_kzalloc(&led->pdev->dev,
					sizeof(struct pwm_config_data),
					GFP_KERNEL);
		if (!led->rgb_cfg->pwm_cfg) {
			dev_err(&led->pdev->dev,
				"Unable to allocate memory\n");
			return -ENOMEM;
		}
		led->rgb_cfg->pwm_cfg->mode = led_mode;
		led->rgb_cfg->pwm_cfg->default_mode = led_mode;
	} else {
		return rc;
	}

	rc = qpnp_get_config_pwm(led->rgb_cfg->pwm_cfg, led->pdev, node);
	if (rc < 0) {
		if (led->rgb_cfg->pwm_cfg->pwm_dev)
			pwm_put(led->rgb_cfg->pwm_cfg->pwm_dev);
		return rc;
	}

	return 0;
}

static int qpnp_get_config_mpp(struct qpnp_led_data *led,
		struct device_node *node)
{
	int rc;
	u32 val;
	u8 led_mode;
	const char *mode;

	led->mpp_cfg = devm_kzalloc(&led->pdev->dev,
			sizeof(struct mpp_config_data), GFP_KERNEL);
	if (!led->mpp_cfg)
		return -ENOMEM;

	if (of_find_property(of_get_parent(node), "mpp-power-supply", NULL)) {
		led->mpp_cfg->mpp_reg =
				regulator_get(&led->pdev->dev,
							"mpp-power");
		if (IS_ERR(led->mpp_cfg->mpp_reg)) {
			rc = PTR_ERR(led->mpp_cfg->mpp_reg);
			dev_err(&led->pdev->dev,
				"MPP regulator get failed(%d)\n", rc);
			return rc;
		}
	}

	if (led->mpp_cfg->mpp_reg) {
		rc = of_property_read_u32(of_get_parent(node),
					"qcom,mpp-power-max-voltage", &val);
		if (!rc)
			led->mpp_cfg->max_uV = val;
		else
			goto err_config_mpp;

		rc = of_property_read_u32(of_get_parent(node),
					"qcom,mpp-power-min-voltage", &val);
		if (!rc)
			led->mpp_cfg->min_uV = val;
		else
			goto err_config_mpp;
	} else {
		rc = of_property_read_u32(of_get_parent(node),
					"qcom,mpp-power-max-voltage", &val);
		if (!rc)
			dev_warn(&led->pdev->dev, "No regulator specified\n");

		rc = of_property_read_u32(of_get_parent(node),
					"qcom,mpp-power-min-voltage", &val);
		if (!rc)
			dev_warn(&led->pdev->dev, "No regulator specified\n");
	}

	led->mpp_cfg->current_setting = LED_MPP_CURRENT_MIN;
	rc = of_property_read_u32(node, "qcom,current-setting", &val);
	if (!rc) {
		if (led->mpp_cfg->current_setting < LED_MPP_CURRENT_MIN)
			led->mpp_cfg->current_setting = LED_MPP_CURRENT_MIN;
		else if (led->mpp_cfg->current_setting > LED_MPP_CURRENT_MAX)
			led->mpp_cfg->current_setting = LED_MPP_CURRENT_MAX;
		else
			led->mpp_cfg->current_setting = (u8) val;
	} else if (rc != -EINVAL)
		goto err_config_mpp;

	led->mpp_cfg->source_sel = LED_MPP_SOURCE_SEL_DEFAULT;
	rc = of_property_read_u32(node, "qcom,source-sel", &val);
	if (!rc)
		led->mpp_cfg->source_sel = (u8) val;
	else if (rc != -EINVAL)
		goto err_config_mpp;

	led->mpp_cfg->mode_ctrl = LED_MPP_MODE_SINK;
	rc = of_property_read_u32(node, "qcom,mode-ctrl", &val);
	if (!rc)
		led->mpp_cfg->mode_ctrl = (u8) val;
	else if (rc != -EINVAL)
		goto err_config_mpp;

	led->mpp_cfg->vin_ctrl = LED_MPP_VIN_CTRL_DEFAULT;
	rc = of_property_read_u32(node, "qcom,vin-ctrl", &val);
	if (!rc)
		led->mpp_cfg->vin_ctrl = (u8) val;
	else if (rc != -EINVAL)
		goto err_config_mpp;

	led->mpp_cfg->min_brightness = 0;
	rc = of_property_read_u32(node, "qcom,min-brightness", &val);
	if (!rc)
		led->mpp_cfg->min_brightness = (u8) val;
	else if (rc != -EINVAL)
		goto err_config_mpp;

	rc = of_property_read_string(node, "qcom,mode", &mode);
	if (!rc) {
		led_mode = qpnp_led_get_mode(mode);
		led->mpp_cfg->pwm_mode = led_mode;
		if (led_mode == MANUAL_MODE)
			return MANUAL_MODE;
		else if (led_mode == -EINVAL) {
			dev_err(&led->pdev->dev, "Selected mode not supported for mpp\n");
			rc = -EINVAL;
			goto err_config_mpp;
		}
		led->mpp_cfg->pwm_cfg = devm_kzalloc(&led->pdev->dev,
					sizeof(struct pwm_config_data),
					GFP_KERNEL);
		if (!led->mpp_cfg->pwm_cfg) {
			dev_err(&led->pdev->dev,
				"Unable to allocate memory\n");
			rc = -ENOMEM;
			goto err_config_mpp;
		}
		led->mpp_cfg->pwm_cfg->mode = led_mode;
		led->mpp_cfg->pwm_cfg->default_mode = led_mode;
	} else {
		return rc;
	}

	rc = qpnp_get_config_pwm(led->mpp_cfg->pwm_cfg, led->pdev, node);
	if (rc < 0) {
		if (led->mpp_cfg->pwm_cfg && led->mpp_cfg->pwm_cfg->pwm_dev)
			pwm_put(led->mpp_cfg->pwm_cfg->pwm_dev);
		goto err_config_mpp;
	}

	return 0;

err_config_mpp:
	if (led->mpp_cfg->mpp_reg)
		regulator_put(led->mpp_cfg->mpp_reg);
	return rc;
}

static int qpnp_get_config_gpio(struct qpnp_led_data *led,
		struct device_node *node)
{
	int rc;
	u32 val;

	led->gpio_cfg = devm_kzalloc(&led->pdev->dev,
			sizeof(struct gpio_config_data), GFP_KERNEL);
	if (!led->gpio_cfg)
		return -ENOMEM;

	led->gpio_cfg->source_sel = LED_GPIO_SOURCE_SEL_DEFAULT;
	rc = of_property_read_u32(node, "qcom,source-sel", &val);
	if (!rc)
		led->gpio_cfg->source_sel = (u8) val;
	else if (rc != -EINVAL)
		goto err_config_gpio;

	led->gpio_cfg->mode_ctrl = LED_GPIO_MODE_OUTPUT;
	rc = of_property_read_u32(node, "qcom,mode-ctrl", &val);
	if (!rc)
		led->gpio_cfg->mode_ctrl = (u8) val;
	else if (rc != -EINVAL)
		goto err_config_gpio;

	led->gpio_cfg->vin_ctrl = LED_GPIO_VIN_CTRL_DEFAULT;
	rc = of_property_read_u32(node, "qcom,vin-ctrl", &val);
	if (!rc)
		led->gpio_cfg->vin_ctrl = (u8) val;
	else if (rc != -EINVAL)
		goto err_config_gpio;

	return 0;

err_config_gpio:
	return rc;
}

static int qpnp_leds_probe(struct platform_device *pdev)
{
	struct qpnp_led_data *led, *led_array;
	unsigned int base;
	struct device_node *node, *temp;
	int rc, i, num_leds = 0, parsed_leds = 0;
	const char *led_label;
	bool regulator_probe = false;

	node = pdev->dev.of_node;
	if (node == NULL)
		return -ENODEV;

	temp = NULL;
	while ((temp = of_get_next_child(node, temp)))
		num_leds++;

	if (!num_leds)
		return -ECHILD;

	led_array = devm_kcalloc(&pdev->dev, num_leds, sizeof(*led_array),
				GFP_KERNEL);
	if (!led_array)
		return -ENOMEM;

	for_each_child_of_node(node, temp) {
		led = &led_array[parsed_leds];
		led->num_leds = num_leds;
		led->regmap = dev_get_regmap(pdev->dev.parent, NULL);
		if (!led->regmap) {
			dev_err(&pdev->dev, "Couldn't get parent's regmap\n");
			return -EINVAL;
		}
		led->pdev = pdev;

		rc = of_property_read_u32(pdev->dev.of_node, "reg", &base);
		if (rc < 0) {
			dev_err(&pdev->dev,
				"Couldn't find reg in node = %s rc = %d\n",
				pdev->dev.of_node->full_name, rc);
			goto fail_id_check;
		}
		led->base = base;

		rc = of_property_read_string(temp, "label", &led_label);
		if (rc < 0) {
			dev_err(&led->pdev->dev,
				"Failure reading label, rc = %d\n", rc);
			goto fail_id_check;
		}

		rc = of_property_read_string(temp, "linux,name",
			&led->cdev.name);
		if (rc < 0) {
			dev_err(&led->pdev->dev,
				"Failure reading led name, rc = %d\n", rc);
			goto fail_id_check;
		}

		rc = of_property_read_u32(temp, "qcom,max-current",
			&led->max_current);
		if (rc < 0) {
			dev_err(&led->pdev->dev,
				"Failure reading max_current, rc =  %d\n", rc);
			goto fail_id_check;
		}

		rc = of_property_read_u32(temp, "qcom,id", &led->id);
		if (rc < 0) {
			dev_err(&led->pdev->dev,
				"Failure reading led id, rc =  %d\n", rc);
			goto fail_id_check;
		}

		rc = qpnp_get_common_configs(led, temp);
		if (rc) {
			dev_err(&led->pdev->dev, "Failure reading common led configuration, rc = %d\n",
				rc);
			goto fail_id_check;
		}

		led->cdev.brightness_set    = qpnp_led_set;
		led->cdev.brightness_get    = qpnp_led_get;

		if (strcmp(led_label, "wled") == 0) {
			rc = qpnp_get_config_wled(led, temp);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"Unable to read wled config data\n");
				goto fail_id_check;
			}
		} else if (strcmp(led_label, "flash") == 0) {
			if (!of_find_property(node, "flash-boost-supply", NULL))
				regulator_probe = true;
			rc = qpnp_get_config_flash(led, temp, &regulator_probe);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"Unable to read flash config data\n");
				goto fail_id_check;
			}
		} else if (strcmp(led_label, "rgb") == 0) {
			rc = qpnp_get_config_rgb(led, temp);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"Unable to read rgb config data\n");
				goto fail_id_check;
			}
		} else if (strcmp(led_label, "mpp") == 0) {
			rc = qpnp_get_config_mpp(led, temp);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
						"Unable to read mpp config data\n");
				goto fail_id_check;
			}
		} else if (strcmp(led_label, "gpio") == 0) {
			rc = qpnp_get_config_gpio(led, temp);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
						"Unable to read gpio config data\n");
				goto fail_id_check;
			}
		} else if (strcmp(led_label, "kpdbl") == 0) {
			bitmap_zero(kpdbl_leds_in_use, NUM_KPDBL_LEDS);
			is_kpdbl_master_turn_on = false;
			rc = qpnp_get_config_kpdbl(led, temp);
			if (rc < 0) {
				dev_err(&led->pdev->dev,
					"Unable to read kpdbl config data\n");
				goto fail_id_check;
			}
		} else {
			dev_err(&led->pdev->dev, "No LED matching label\n");
			rc = -EINVAL;
			goto fail_id_check;
		}

		if (led->id != QPNP_ID_FLASH1_LED0 &&
					led->id != QPNP_ID_FLASH1_LED1)
			mutex_init(&led->lock);

		led->in_order_command_processing = of_property_read_bool
				(temp, "qcom,in-order-command-processing");

		if (led->in_order_command_processing) {
			/*
			 * the command order from user space needs to be
			 * maintained use ordered workqueue to prevent
			 * concurrency
			 */
			led->workqueue = alloc_ordered_workqueue
							("led_workqueue", 0);
			if (!led->workqueue) {
				rc = -ENOMEM;
				goto fail_id_check;
			}
		}

		INIT_WORK(&led->work, qpnp_led_work);

		rc =  qpnp_led_initialize(led);
		if (rc < 0)
			goto fail_id_check;

		rc = qpnp_led_set_max_brightness(led);
		if (rc < 0)
			goto fail_id_check;

		rc = led_classdev_register(&pdev->dev, &led->cdev);
		if (rc) {
			dev_err(&pdev->dev,
				"unable to register led %d,rc=%d\n",
						 led->id, rc);
			goto fail_id_check;
		}

		if (led->id == QPNP_ID_FLASH1_LED0 ||
			led->id == QPNP_ID_FLASH1_LED1) {
			rc = sysfs_create_group(&led->cdev.dev->kobj,
							&led_attr_group);
			if (rc)
				goto fail_id_check;

		}

		if (led->id == QPNP_ID_LED_MPP) {
			if (!led->mpp_cfg->pwm_cfg)
				break;
			if (led->mpp_cfg->pwm_cfg->mode == PWM_MODE) {
				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&pwm_attr_group);
				if (rc)
					goto fail_id_check;
			}
			if (led->mpp_cfg->pwm_cfg->use_blink) {
				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&blink_attr_group);
				if (rc)
					goto fail_id_check;

				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&lpg_attr_group);
				if (rc)
					goto fail_id_check;
			} else if (led->mpp_cfg->pwm_cfg->mode == LPG_MODE) {
				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&lpg_attr_group);
				if (rc)
					goto fail_id_check;
			}
		} else if ((led->id == QPNP_ID_RGB_RED) ||
			(led->id == QPNP_ID_RGB_GREEN) ||
			(led->id == QPNP_ID_RGB_BLUE)) {
			if (led->rgb_cfg->pwm_cfg->mode == PWM_MODE) {
				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&pwm_attr_group);
				if (rc)
					goto fail_id_check;
			}
			if (led->rgb_cfg->pwm_cfg->use_blink) {
				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&blink_attr_group);
				if (rc)
					goto fail_id_check;

				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&lpg_attr_group);
				if (rc)
					goto fail_id_check;
			} else if (led->rgb_cfg->pwm_cfg->mode == LPG_MODE) {
				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&lpg_attr_group);
				if (rc)
					goto fail_id_check;
			}
		} else if (led->id == QPNP_ID_KPDBL) {
			if (led->kpdbl_cfg->pwm_cfg->mode == PWM_MODE) {
				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&pwm_attr_group);
				if (rc)
					goto fail_id_check;
			}
			if (led->kpdbl_cfg->pwm_cfg->use_blink) {
				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&blink_attr_group);
				if (rc)
					goto fail_id_check;

				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&lpg_attr_group);
				if (rc)
					goto fail_id_check;
			} else if (led->kpdbl_cfg->pwm_cfg->mode == LPG_MODE) {
				rc = sysfs_create_group(&led->cdev.dev->kobj,
					&lpg_attr_group);
				if (rc)
					goto fail_id_check;
			}
		}

		/* configure default state */
		if (led->default_on) {
			led->cdev.brightness = led->cdev.max_brightness;
			__qpnp_led_work(led, led->cdev.brightness);
			if (led->turn_off_delay_ms > 0)
				qpnp_led_turn_off(led);
		} else
			led->cdev.brightness = LED_OFF;

		parsed_leds++;
	}
	dev_set_drvdata(&pdev->dev, led_array);
	return 0;

fail_id_check:
	for (i = 0; i < parsed_leds; i++) {
		if (led_array[i].id != QPNP_ID_FLASH1_LED0 &&
				led_array[i].id != QPNP_ID_FLASH1_LED1)
			mutex_destroy(&led_array[i].lock);
		if (led_array[i].in_order_command_processing)
			destroy_workqueue(led_array[i].workqueue);
		led_classdev_unregister(&led_array[i].cdev);
	}

	return rc;
}

static int qpnp_leds_remove(struct platform_device *pdev)
{
	struct qpnp_led_data *led_array  = dev_get_drvdata(&pdev->dev);
	int i, parsed_leds = led_array->num_leds;

	for (i = 0; i < parsed_leds; i++) {
		cancel_work_sync(&led_array[i].work);
		if (led_array[i].id != QPNP_ID_FLASH1_LED0 &&
				led_array[i].id != QPNP_ID_FLASH1_LED1)
			mutex_destroy(&led_array[i].lock);

		if (led_array[i].in_order_command_processing)
			destroy_workqueue(led_array[i].workqueue);
		led_classdev_unregister(&led_array[i].cdev);
		switch (led_array[i].id) {
		case QPNP_ID_WLED:
			break;
		case QPNP_ID_FLASH1_LED0:
		case QPNP_ID_FLASH1_LED1:
			if (led_array[i].flash_cfg->flash_reg_get)
				regulator_put(
				       led_array[i].flash_cfg->flash_boost_reg);
			if (led_array[i].flash_cfg->torch_enable)
				if (!led_array[i].flash_cfg->no_smbb_support)
					regulator_put(led_array[i].
					flash_cfg->torch_boost_reg);
			sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&led_attr_group);
			break;
		case QPNP_ID_RGB_RED:
		case QPNP_ID_RGB_GREEN:
		case QPNP_ID_RGB_BLUE:
			if (led_array[i].rgb_cfg->pwm_cfg->mode == PWM_MODE)
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&pwm_attr_group);
			if (led_array[i].rgb_cfg->pwm_cfg->use_blink) {
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&blink_attr_group);
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&lpg_attr_group);
			} else if (led_array[i].rgb_cfg->pwm_cfg->mode
				   == LPG_MODE)
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&lpg_attr_group);
			break;
		case QPNP_ID_LED_MPP:
			if (!led_array[i].mpp_cfg->pwm_cfg)
				break;
			if (led_array[i].mpp_cfg->pwm_cfg->mode == PWM_MODE)
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&pwm_attr_group);
			if (led_array[i].mpp_cfg->pwm_cfg->use_blink) {
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&blink_attr_group);
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&lpg_attr_group);
			} else if (led_array[i].mpp_cfg->pwm_cfg->mode
				   == LPG_MODE)
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&lpg_attr_group);
			if (led_array[i].mpp_cfg->mpp_reg)
				regulator_put(led_array[i].mpp_cfg->mpp_reg);
			break;
		case QPNP_ID_KPDBL:
			if (led_array[i].kpdbl_cfg->pwm_cfg->mode == PWM_MODE)
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&pwm_attr_group);
			if (led_array[i].kpdbl_cfg->pwm_cfg->use_blink) {
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&blink_attr_group);
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&lpg_attr_group);
			} else if (led_array[i].kpdbl_cfg->pwm_cfg->mode
				   == LPG_MODE)
				sysfs_remove_group(&led_array[i].cdev.dev->kobj,
							&lpg_attr_group);
			break;
		default:
			dev_err(&led_array->pdev->dev,
					"Invalid LED(%d)\n",
					led_array[i].id);
			return -EINVAL;
		}
	}

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id spmi_match_table[] = {
	{ .compatible = "qcom,leds-qpnp",},
	{ },
};
#else
#define spmi_match_table NULL
#endif

static struct platform_driver qpnp_leds_driver = {
	.driver		= {
		.name		= "qcom,leds-qpnp",
		.of_match_table	= spmi_match_table,
	},
	.probe		= qpnp_leds_probe,
	.remove		= qpnp_leds_remove,
};

static int __init qpnp_led_init(void)
{
	return platform_driver_register(&qpnp_leds_driver);
}
module_init(qpnp_led_init);

static void __exit qpnp_led_exit(void)
{
	platform_driver_unregister(&qpnp_leds_driver);
}
module_exit(qpnp_led_exit);

MODULE_DESCRIPTION("QPNP LEDs driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-qpnp");

