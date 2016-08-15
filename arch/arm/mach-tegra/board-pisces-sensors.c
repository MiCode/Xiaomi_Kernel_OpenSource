/*
 * arch/arm/mach-tegra/board-pisces-sensors.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 * Copyright (C) 2016 XiaoMi, Inc.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/mpu.h>
#include <linux/max77665-charger.h>
#include <linux/mfd/max77665.h>
#include <linux/input/max77665-haptic.h>
#include <linux/input/isl29028.h>
#include <linux/power/max17042_battery.h>
#include <linux/nct1008.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/akm8963.h>
#include <linux/thermal.h>
#include <mach/edp.h>
#include <mach/gpio-tegra.h>
#include <mach/pinmux-t11.h>
#include <mach/pinmux.h>
#include <media/max77665-flash.h>
#include <media/imx135.h>
#include <media/imx132.h>
#include <media/ad5823.h>
#include <asm/mach-types.h>

#include "gpio-names.h"
#include "board.h"
#include "board-common.h"
#include "board-pisces.h"
#include "cpu-tegra.h"
#include "devices.h"
#include "tegra-board-id.h"
#include "dvfs.h"
#include "pm.h"

#include "battery-ini-model-data.h"

#define NTC_10K_TGAIN   0xE6A2
#define NTC_10K_TOFF    0x2694
#define NTC_68K_TGAIN   0xE665
#define NTC_68K_TOFF    0x26C1

static struct board_info board_info;
/* SDI by default */

static struct max17042_platform_data max17042_pdata = {
	.config_data = &pisces_conf_data_samsung, /* samsung by default */
	.init_data  = NULL,
	.num_init_data = 0,
	.enable_por_init = 1, /* Use POR init from Maxim appnote */
	.enable_current_sense = 1,
	.r_sns = 0,
	.is_battery_present = true,
};

static int pluto_focuser_power_on(struct ad5823_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->vdd || !pw->vdd_i2c)))
		return -EFAULT;

	gpio_set_value(CAM_AF_PWDN, 1);
	err = regulator_enable(pw->vdd_i2c);
	if (unlikely(err))
		goto ad5823_vdd_i2c_fail;

	err = regulator_enable(pw->vdd);
	if (unlikely(err))
		goto ad5823_vdd_fail;

	return 0;

ad5823_vdd_fail:
	regulator_disable(pw->vdd_i2c);

ad5823_vdd_i2c_fail:
	pr_err("%s FAILED\n", __func__);

	return -ENODEV;
}

static int pluto_focuser_power_off(struct ad5823_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->vdd || !pw->vdd_i2c)))
		return -EFAULT;

	gpio_set_value(CAM_AF_PWDN, 0);
	regulator_disable(pw->vdd);
	regulator_disable(pw->vdd_i2c);

	return 0;
}

static struct i2c_board_info max17042_device[] = {
	{
		I2C_BOARD_INFO("max17042", 0x36),
		.platform_data = &max17042_pdata,
	},
};

static unsigned max77665_f_estates[] = { 3500, 2375, 560, 456, 0 };

static struct max77665_f_platform_data pluto_max77665_flash_pdata = {
	.config = {
		.led_mask		= 3,
		/* set to true only when using the torch strobe input
		 * to trigger the flash.
		 */
		.flash_on_torch         = true,
		/* use ONE-SHOOT flash mode - flash triggered at the
		 * raising edge of strobe or strobe signal.
		 */
		.flash_mode		= 1,
		/* .flash_on_torch         = true, */
		.max_total_current_mA	= 1000,
		.max_peak_current_mA	= 425,
		.max_torch_current_mA	= 150,
		.max_flash_threshold_mV	= 3400,
		.max_flash_hysteresis_mV = 200,
		.max_flash_lbdly_f_uS	= 256,
		.max_flash_lbdly_r_uS	= 256,
		.led_config[0] = {
			.flash_torch_ratio = 18100,
			.granularity = 1000,
			},
		.led_config[1] = {
			.flash_torch_ratio = 18100,
			.granularity = 1000,
			},
		},
	.pinstate	= {
		.mask	= 1 << (CAM_FLASH_STROBE - TEGRA_GPIO_PBB0),
		.values	= 1 << (CAM_FLASH_STROBE - TEGRA_GPIO_PBB0),
		},
	.dev_name	= "torch",
	.gpio_strobe	= CAM_FLASH_STROBE,
	.edpc_config	= {
		.states = max77665_f_estates,
		.num_states = ARRAY_SIZE(max77665_f_estates),
		.e0_index = ARRAY_SIZE(max77665_f_estates) - 1,
		.priority = EDP_MAX_PRIO + 2,
		},
};

static struct max77665_haptic_platform_data max77665_haptic_pdata = {
	.pwm_channel_id = 2,
	.pwm_period = 50,
	.type = MAX77665_HAPTIC_LRA,
	.mode = MAX77665_INTERNAL_MODE,
	.internal_mode_pattern = 0,
	.pattern_cycle = 10,
	.pattern_signal_period = 0xD0,
	.pwm_divisor = MAX77665_PWM_DIVISOR_128,
	.feedback_duty_cycle = 12,
	.invert = MAX77665_INVERT_OFF,
	.cont_mode = MAX77665_CONT_MODE,
	.motor_startup_val = 0,
	.scf_val = 2,
	.edp_states = { 300, 0 },
};

static struct max77665_charger_cable maxim_cable[] = {
	{
		.name           = "USB",
	},
	{
		.name           = "USB-Host",
	},
	{
		.name           = "TA",
	},
	{
		.name           = "Fast-charger",
	},
	{
		.name           = "Slow-charger",
	},
	{
		.name           = "Charge-downstream",
	},
};

static struct max77665_charger_plat_data max77665_charger = {
	.fast_chg_cc = 1490, /* fast charger current*/
	.term_volt = 4375, /* charger termination voltage */
	.curr_lim = 200,
	.oc_alert = OC_DISABLED, /* Instead INA231 is used for OCP */
	.cool_temp = 1,
	.warm_temp = 45,
	.cool_bat_chg_current = 800,
	.warm_bat_chg_current = 800,
	.cool_bat_voltage = 4000,
	.warm_bat_voltage = 4000,
	.num_cables = MAX_CABLES,
	.cables = maxim_cable,
	.extcon_name = "tegra-udc",
	.pmu_extcon_name = "palmas-extcon",
	.update_status = max17042_update_status,
	.is_battery_present = true,
};

static struct max77665_muic_platform_data max77665_muic = {
	.ext_conn_name = "MAX77665_MUIC_ID",
};

struct max77665_system_interrupt max77665_sys_int = {
	.enable_thermal_interrupt = true,
	.enable_low_sys_interrupt = true,
};

static struct max77665_platform_data pluto_max77665_pdata = {
	.irq_base = MAX77665_TEGRA_IRQ_BASE,
	.irq_flag = IRQF_ONESHOT | IRQF_TRIGGER_FALLING,
	.system_interrupt = &max77665_sys_int,
	.muic_platform_data = {
		.pdata = &max77665_muic,
		.size =	sizeof(max77665_muic),
		},
	.charger_platform_data = {
		.pdata = &max77665_charger,
		.size =	sizeof(max77665_charger),
		},
	.flash_platform_data = {
		.pdata = &pluto_max77665_flash_pdata,
		.size =	sizeof(pluto_max77665_flash_pdata),
		},
	.haptic_platform_data = {
		.pdata = &max77665_haptic_pdata,
		.size = sizeof(max77665_haptic_pdata),
		},
};

static struct i2c_board_info pluto_i2c_board_info_max77665[] = {
	{
		I2C_BOARD_INFO("max77665", 0x66),
		.platform_data = &pluto_max77665_pdata,
	},
};

/* isl29029 support is provided by isl29028*/
static struct isl29028_platform_data isl29028_pdata = {
	.als_factor	= 10,
	.als_highrange	= 2,
	.als_lowthres	= 0x0e0,
	.als_highthres	= 0xf80,
	.als_sensitive	= 25,

	.prox_period	= 100000000,
	.prox_null_value	= 8,
	.prox_lowthres_offset	= 60,  /* must > 30 */
	.prox_threswindow	= 30,
};

static struct i2c_board_info pluto_i2c1_isl_board_info[] = {
	{
		I2C_BOARD_INFO("isl29028", 0x44),
		.platform_data = &isl29028_pdata,
	}
};

static struct i2c_board_info pluto_i2c1_pressure_board_info[] = {
	{
		I2C_BOARD_INFO("bmp180", 0x77),
	}
};

static struct throttle_table tj_throttle_table[] = {
	/* CPU_THROT_LOW cannot be used by other than CPU */
	/*      CPU,  C2BUS,  C3BUS,   SCLK,    EMC   */
	{ { 1810500, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1785000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1759500, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1734000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1708500, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1683000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1657500, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1632000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1606500, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1581000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1555500, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1530000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1504500, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1479000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1453500, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1428000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1402500, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1377000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1351500, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1326000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1300500, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1275000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1249500, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1224000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1198500, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1173000, 636000, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1147500, 636000, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1122000, 636000, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1096500, 636000, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1071000, 636000, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1045500, 636000, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1020000, 636000, NO_CAP, NO_CAP, NO_CAP } },
	{ {  994500, 636000, NO_CAP, NO_CAP, NO_CAP } },
	{ {  969000, 600000, NO_CAP, NO_CAP, NO_CAP } },
	{ {  943500, 600000, NO_CAP, NO_CAP, NO_CAP } },
	{ {  918000, 600000, NO_CAP, NO_CAP, NO_CAP } },
	{ {  892500, 600000, NO_CAP, NO_CAP, NO_CAP } },
	{ {  867000, 600000, NO_CAP, NO_CAP, NO_CAP } },
	{ {  841500, 564000, NO_CAP, NO_CAP, NO_CAP } },
	{ {  816000, 564000, NO_CAP, NO_CAP, 792000 } },
	{ {  790500, 564000, NO_CAP, 372000, 792000 } },
	{ {  765000, 564000, 468000, 372000, 792000 } },
	{ {  739500, 528000, 468000, 372000, 792000 } },
	{ {  714000, 528000, 468000, 336000, 792000 } },
	{ {  688500, 528000, 420000, 336000, 792000 } },
	{ {  663000, 492000, 420000, 336000, 792000 } },
	{ {  637500, 492000, 420000, 336000, 408000 } },
	{ {  612000, 492000, 420000, 300000, 408000 } },
	{ {  586500, 492000, 360000, 336000, 408000 } },
	{ {  561000, 420000, 420000, 300000, 408000 } },
	{ {  535500, 420000, 360000, 228000, 408000 } },
	{ {  510000, 420000, 288000, 228000, 408000 } },
	{ {  484500, 324000, 288000, 228000, 408000 } },
	{ {  459000, 324000, 288000, 228000, 408000 } },
	{ {  433500, 324000, 288000, 228000, 408000 } },
	{ {  408000, 324000, 288000, 228000, 408000 } },
};

static struct balanced_throttle tj_throttle = {
	.throt_tab_size = ARRAY_SIZE(tj_throttle_table),
	.throt_tab = tj_throttle_table,
};

static int __init pluto_throttle_init(void)
{
	if (machine_is_pisces())
		balanced_throttle_register(&tj_throttle, "tegra-balanced");
	return 0;
}

module_init(pluto_throttle_init);

static struct nct1008_platform_data pluto_nct1008_pdata = {
	.supported_hwrev = true,
	.ext_range = true,
	.conv_rate = 0x06, /* 4Hz conversion rate */
	.offset = 0,
	.shutdown_ext_limit = 105, /* C */
	.shutdown_local_limit = 120, /* C */

	.num_trips = 1,
	.trips = {
		{
			.cdev_type = "suspend_soctherm",
			.trip_temp = 50000,
			.trip_type = THERMAL_TRIP_ACTIVE,
			.upper = 1,
			.lower = 1,
			.hysteresis = 5000,
		},
	},

#ifdef CONFIG_TEGRA_LP1_LOW_COREVOLTAGE
	.suspend_ext_limit_hi = 25000,
	.suspend_ext_limit_lo = 20000,
	.suspend_with_wakeup = tegra_is_lp1_suspend_mode,
#endif
};

static struct i2c_board_info pluto_i2c4_nct1008_board_info[] = {
	{
		I2C_BOARD_INFO("nct1008", 0x4C),
		.platform_data = &pluto_nct1008_pdata,
		.irq = -1,
	}
};

#define VI_PINMUX(_pingroup, _mux, _pupd, _tri, _io, _lock, _ioreset) \
	{							\
		.pingroup	= TEGRA_PINGROUP_##_pingroup,	\
		.func		= TEGRA_MUX_##_mux,		\
		.pupd		= TEGRA_PUPD_##_pupd,		\
		.tristate	= TEGRA_TRI_##_tri,		\
		.io		= TEGRA_PIN_##_io,		\
		.lock		= TEGRA_PIN_LOCK_##_lock,	\
		.od		= TEGRA_PIN_OD_DEFAULT,		\
		.ioreset	= TEGRA_PIN_IO_RESET_##_ioreset	\
}

static struct tegra_pingroup_config mclk_disable =
	VI_PINMUX(CAM_MCLK, VI, NORMAL, NORMAL, OUTPUT, DEFAULT, DEFAULT);

static struct tegra_pingroup_config mclk_enable =
	VI_PINMUX(CAM_MCLK, VI_ALT3, NORMAL, NORMAL, OUTPUT, DEFAULT, DEFAULT);

static struct tegra_pingroup_config pbb0_disable =
	VI_PINMUX(GPIO_PBB0, VI, NORMAL, NORMAL, OUTPUT, DEFAULT, DEFAULT);

static struct tegra_pingroup_config pbb0_enable =
	VI_PINMUX(GPIO_PBB0, VI_ALT3, NORMAL, NORMAL, OUTPUT, DEFAULT, DEFAULT);

/*
 * more regulators need to be allocated to activate the sensor devices.
 * pluto_vcmvdd: this is a workaround due to the focuser device(AD5823) will
 *               hook up the i2c bus if it is not powered up.
 * pluto_i2cvdd: by default, the power supply on the i2c bus is OFF. So it
 *               should be turned on every time any sensor device is activated.
*/
static struct regulator *pluto_vcmvdd;
static struct regulator *pluto_i2cvdd;

static int pluto_get_extra_regulators(void)
{
	if (!pluto_vcmvdd) {
		pluto_vcmvdd = regulator_get(NULL, "vdd_af_cam1");
		if (WARN_ON(IS_ERR(pluto_vcmvdd))) {
			pr_err("%s: can't get regulator vdd_af_cam1: %ld\n",
				__func__, PTR_ERR(pluto_vcmvdd));
			pluto_vcmvdd = NULL;
			return -ENODEV;
		}
	}

	if (!pluto_i2cvdd) {
		pluto_i2cvdd = regulator_get(NULL, "vddio_cam_mb");
		if (unlikely(WARN_ON(IS_ERR(pluto_i2cvdd)))) {
			pr_err("%s: can't get regulator vddio_cam_mb: %ld\n",
				__func__, PTR_ERR(pluto_i2cvdd));
			pluto_i2cvdd = NULL;
			return -ENODEV;
		}
	}

	return 0;
}

static int pluto_imx135_power_on(struct imx135_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->dvdd || !pw->iovdd || !pw->avdd)))
		return -EFAULT;

	if (pluto_get_extra_regulators())
		goto imx135_poweron_fail;

	gpio_set_value(CAM_RSTN, 0);
	gpio_set_value(CAM_AF_PWDN, 1);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);

	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto imx135_avdd_fail;

	err = regulator_enable(pw->dvdd);
	if (unlikely(err))
		goto imx135_dvdd_fail;

	err = regulator_enable(pw->iovdd);
	if (unlikely(err))
		goto imx135_iovdd_fail;

	usleep_range(2000, 2500);
	gpio_set_value(CAM_RSTN, 1);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 1);

	tegra_pinmux_config_table(&mclk_enable, 1);
	err = regulator_enable(pluto_i2cvdd);
	if (unlikely(err))
		goto imx135_i2c_fail;

	err = regulator_enable(pluto_vcmvdd);
	if (unlikely(err))
		goto imx135_vcm_fail;
	usleep_range(250, 300);

	return 0;

imx135_vcm_fail:
	regulator_disable(pluto_i2cvdd);

imx135_i2c_fail:
	tegra_pinmux_config_table(&mclk_disable, 1);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);
	regulator_disable(pw->iovdd);

imx135_iovdd_fail:
	regulator_disable(pw->dvdd);

imx135_dvdd_fail:
	regulator_disable(pw->avdd);

imx135_avdd_fail:
imx135_poweron_fail:
	pr_err("%s FAILED\n", __func__);
	return -ENODEV;
}

static int pluto_imx135_power_off(struct imx135_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pluto_i2cvdd || !pluto_vcmvdd ||
				!pw->dvdd || !pw->iovdd || !pw->avdd)))
		return -EFAULT;

	usleep_range(1, 2);
	tegra_pinmux_config_table(&mclk_disable, 1);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);
	gpio_set_value(CAM_RSTN, 0);
	gpio_set_value(CAM_AF_PWDN, 0);

	usleep_range(1, 2);

	regulator_disable(pw->iovdd);
	regulator_disable(pw->dvdd);
	regulator_disable(pw->avdd);
	regulator_disable(pluto_i2cvdd);
	regulator_disable(pluto_vcmvdd);

	return 0;
}

static unsigned imx135_estates[] = {876, 656, 220, 0};

static struct imx135_platform_data imx135_pdata = {
	.edpc_config = {
		.states = imx135_estates,
		.num_states = ARRAY_SIZE(imx135_estates),
		.e0_index = ARRAY_SIZE(imx135_estates) - 1,
		.priority = EDP_MAX_PRIO + 1,
	},
	.power_on               = pluto_imx135_power_on,
	.power_off              = pluto_imx135_power_off,
};

static int pluto_imx132_power_on(struct imx132_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd || !pw->dvdd)))
		return -EFAULT;

	if (pluto_get_extra_regulators())
		goto pluto_imx132_poweron_fail;

	gpio_set_value(CAM2_POWER_DWN_GPIO, 0);
	gpio_set_value(CAM_GPIO1, 0);

	tegra_pinmux_config_table(&pbb0_enable, 1);

	err = regulator_enable(pluto_i2cvdd);
	if (unlikely(err))
		goto imx132_i2c_fail;

	err = regulator_enable(pluto_vcmvdd);
	if (unlikely(err))
		goto imx132_vcm_fail;

	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto imx132_avdd_fail;

	err = regulator_enable(pw->dvdd);
	if (unlikely(err))
		goto imx132_dvdd_fail;

	err = regulator_enable(pw->iovdd);
	if (unlikely(err))
		goto imx132_iovdd_fail;

	usleep_range(1, 2);

	gpio_set_value(CAM2_POWER_DWN_GPIO, 1);
	gpio_set_value(CAM_GPIO1, 1);

	return 0;

imx132_iovdd_fail:
	regulator_disable(pw->dvdd);

imx132_dvdd_fail:
	regulator_disable(pw->avdd);

imx132_avdd_fail:
	regulator_disable(pluto_vcmvdd);

imx132_vcm_fail:
	regulator_disable(pluto_i2cvdd);

imx132_i2c_fail:
	tegra_pinmux_config_table(&pbb0_disable, 1);

pluto_imx132_poweron_fail:
	pr_err("%s failed.\n", __func__);
	return -ENODEV;
}

static int pluto_imx132_power_off(struct imx132_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd || !pw->dvdd ||
			!pluto_i2cvdd || !pluto_vcmvdd)))
		return -EFAULT;

	gpio_set_value(CAM2_POWER_DWN_GPIO, 0);
	gpio_set_value(CAM_GPIO1, 0);

	usleep_range(1, 2);

	regulator_disable(pw->iovdd);
	regulator_disable(pw->dvdd);
	regulator_disable(pw->avdd);

	tegra_pinmux_config_table(&pbb0_disable, 1);

	regulator_disable(pluto_vcmvdd);
	regulator_disable(pluto_i2cvdd);

	return 0;
}

static unsigned imx132_estates[] = { 220, 0 };

struct imx132_platform_data pluto_imx132_data = {
	.edpc_config = {
		.states = imx132_estates,
		.num_states = ARRAY_SIZE(imx132_estates),
		.e0_index = ARRAY_SIZE(imx132_estates) - 1,
		.priority = EDP_MAX_PRIO + 1,
	},
	.power_on = pluto_imx132_power_on,
	.power_off = pluto_imx132_power_off,
};

static struct nvc_gpio_pdata ad5823_gpio_pdata[] = {
	{ AD5823_GPIO_CAM_AF_PWDN, CAM_AF_PWDN, true, 0,},
};

static struct ad5823_platform_data pluto_ad5823_pdata = {
	.cfg		= 0,
	.num		= 0,
	.sync		= 0,
	.dev_name	= "focuser",
	.gpio_count     = ARRAY_SIZE(ad5823_gpio_pdata),
	.gpio           = ad5823_gpio_pdata,
	.power_on	= pluto_focuser_power_on,
	.power_off	= pluto_focuser_power_off,
};

static struct i2c_board_info pluto_i2c_board_info_e1625[] = {
	{
		I2C_BOARD_INFO("imx135", 0x10),
		.platform_data = &imx135_pdata,
	},
	{
		I2C_BOARD_INFO("imx132", 0x36),
		.platform_data = &pluto_imx132_data,
	},
	{
		I2C_BOARD_INFO("ad5823", 0x0C),
		.platform_data = &pluto_ad5823_pdata,
	},
};

static int pluto_camera_init(void)
{
	pr_debug("%s: ++\n", __func__);

	tegra_pinmux_config_table(&mclk_disable, 1);
	tegra_pinmux_config_table(&pbb0_disable, 1);

	i2c_register_board_info(2, pluto_i2c_board_info_e1625,
		ARRAY_SIZE(pluto_i2c_board_info_e1625));

	return 0;
}

static struct akm8963_platform_data akm_board_data = {
	.layout	= 2,
	.gpio_RSTN = MPU_COMPASS_RESET_GPIO,
};

static struct i2c_board_info inv_akm_board_info[] = {
	{
		I2C_BOARD_INFO(AKM_I2C_NAME, MPU_COMPASS_ADDR),
		.platform_data = &akm_board_data,
	},
};

/* MPU board file definition */
static struct mpu_platform_data mpu_gyro_data = {
	.int_config	= 0x00,
	.level_shifter	= 0,
	.orientation	= MPU_GYRO_ORIENTATION,
	.sec_slave_type	= SECONDARY_SLAVE_TYPE_NONE,
	.key		= {0xA3, 0x0F, 0x46, 0x36, 0x9C, 0x6B, 0x39, 0xBE,
			   0x1A, 0xEE, 0x49, 0xB7, 0x81, 0x07, 0xD6, 0xFA},
};


static struct i2c_board_info __initdata inv_mpu_i2c0_board_info[] = {
	{
		I2C_BOARD_INFO(MPU_GYRO_NAME, MPU_GYRO_ADDR),
		.platform_data = &mpu_gyro_data,
	},
};

static void mpuirq_init(void)
{
	int ret = 0;
	int i = 0;

	pr_info("*** MPU START *** mpuirq_init...\n");

	/* MPU-IRQ assignment */
	ret = gpio_request(MPU_GYRO_IRQ_GPIO, MPU_GYRO_NAME);
	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return;
	}

	ret = gpio_direction_input(MPU_GYRO_IRQ_GPIO);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, ret);
		gpio_free(MPU_GYRO_IRQ_GPIO);
		return;
	}
	pr_info("*** MPU END *** mpuirq_init...\n");

	inv_mpu_i2c0_board_info[i++].irq = gpio_to_irq(MPU_GYRO_IRQ_GPIO);

	i2c_register_board_info(MPU_GYRO_BUS_NUM, inv_mpu_i2c0_board_info,
		ARRAY_SIZE(inv_mpu_i2c0_board_info));
}

static void compass_init(void)
{
	int err;

	/* release compass reset pin */
	err = gpio_request(MPU_COMPASS_RESET_GPIO, MPU_COMPASS_NAME);
	if (err < 0)
		pr_err("%s: gpio_request failed %d\n", __func__, err);

	err = gpio_direction_output(MPU_COMPASS_RESET_GPIO, 1);
	if (err < 0)
		pr_err("%s: gpio_direction_output failed %d\n", __func__, err);

	gpio_request(MPU_COMPASS_IRQ_GPIO, MPU_COMPASS_NAME "-irq");
	gpio_direction_input(MPU_COMPASS_IRQ_GPIO);
	inv_akm_board_info[0].irq = gpio_to_irq(MPU_COMPASS_IRQ_GPIO);
	err = i2c_register_board_info(0, inv_akm_board_info,
				ARRAY_SIZE(inv_akm_board_info));
	if (err)
		pr_err("%s: akm board register failed.\n", __func__);
}

static int pluto_nct1008_init(void)
{
	int nct1008_port;
	int ret = 0;

	if (board_info.board_id == BOARD_E1580 ||
	    board_info.board_id == BOARD_E1575 ||
	    board_info.board_id == BOARD_E1577) {
		nct1008_port = TEGRA_GPIO_PX6;
	} else {
		nct1008_port = TEGRA_GPIO_PX6;
		pr_err("Warning: nct alert port assumed TEGRA_GPIO_PX6 for unknown pluto board id E%d\n",
			board_info.board_id);
	}

	tegra_add_cdev_trips(pluto_nct1008_pdata.trips,
				&pluto_nct1008_pdata.num_trips);

	pluto_i2c4_nct1008_board_info[0].irq = gpio_to_irq(nct1008_port);
	pr_info("%s: pluto nct1008 irq %d",
		__func__, pluto_i2c4_nct1008_board_info[0].irq);

	ret = gpio_request(nct1008_port, "temp_alert");
	if (ret < 0)
		return ret;

	ret = gpio_direction_input(nct1008_port);
	if (ret < 0) {
		pr_info("%s: calling gpio_free(nct1008_port)", __func__);
		gpio_free(nct1008_port);
	}

	/* pluto has thermal sensor on GEN1-I2C i.e. instance 0 */
	i2c_register_board_info(0, pluto_i2c4_nct1008_board_info,
				ARRAY_SIZE(pluto_i2c4_nct1008_board_info));

	return ret;
}

#ifdef CONFIG_TEGRA_SKIN_THROTTLE
static struct thermal_trip_info skin_trips[] = {
	{
		.cdev_type = "skin-balanced",
		.trip_temp = 40000,
		.trip_type = THERMAL_TRIP_PASSIVE,
		.upper = THERMAL_NO_LIMIT,
		.lower = THERMAL_NO_LIMIT,
		.hysteresis = 0,
	},
	{
		/*camera cooling device trip point*/
		.cdev_type = "camera-throttle",
		.trip_temp = 42000,
		.trip_type = THERMAL_TRIP_PASSIVE,
		.upper = THERMAL_NO_LIMIT,
		.lower = THERMAL_NO_LIMIT,
	},
};

static struct therm_est_subdevice skin_devs[] = {
	{
		.dev_data = "Tdiode",
		.coeffs = {
			-3, -2, 0, 0,
			-1, -1, -1, -1,
			0, 0, 0, 0,
			0, 0, 0, 0,
			0, -1, -1, -2
		},
	},
	{
		.dev_data = "Tboard",
		.coeffs = {
			6, 6, 6, 6,
			6, 5, 5, 5,
			5, 4, 4, 5,
			5, 5, 5, 5,
			4, 5, 5, 5
		},
	},
};

static struct thermal_zone_params skin_tzp = {
	.governor_name = "pid_thermal_gov",
};

static struct therm_est_data skin_data = {
	.num_trips = ARRAY_SIZE(skin_trips),
	.trips = skin_trips,
	.tzp = &skin_tzp,
	.toffset = 2959,
	.polling_period = 1100,
	.passive_delay = 7000,
	.tc1 = 10,
	.tc2 = 1,
	.ndevs = ARRAY_SIZE(skin_devs),
	.devs = skin_devs,
};

static struct throttle_table skin_throttle_table[] = {
	/* CPU_THROT_LOW cannot be used by other than CPU */
	/*      CPU,  C2BUS,  C3BUS,   SCLK,    EMC   */
	{ { 1605000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1605000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1428000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1428000, NO_CAP, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1428000, 750000, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1224000, 750000, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1224000, 636000, NO_CAP, NO_CAP, NO_CAP } },
	{ { 1224000, 636000, NO_CAP, NO_CAP, 624000 } },
	{ { 1224000, 600000, NO_CAP, NO_CAP, 624000 } },
	{ { 1224000, 600000, NO_CAP, NO_CAP, 624000 } },
	{ { 1122000, 564000, NO_CAP, NO_CAP, 528000 } },
	{ { 1122000, 564000, NO_CAP, NO_CAP, 528000 } },
	{ { 1122000, 564000, NO_CAP, NO_CAP, 528000 } },
	{ { 1122000, 528000, NO_CAP, NO_CAP, 528000 } },
	{ { 1122000, 528000, NO_CAP, NO_CAP, 528000 } },
	{ { 1122000, 528000, NO_CAP, NO_CAP, 528000 } },
	{ { 1122000, 528000, NO_CAP, NO_CAP, 528000 } },
	{ { 1122000, 528000, NO_CAP, NO_CAP, 528000 } },
	{ { 1122000, 468000, 456000, NO_CAP, 528000 } },
	{ { 1020000, 468000, 456000, NO_CAP, 408000 } },
	{ { 1020000, 468000, 456000, NO_CAP, 408000 } },
	{ { 1020000, 468000, 456000, NO_CAP, 408000 } },
	{ { 1020000, 468000, 456000, NO_CAP, 408000 } },
	{ { 1020000, 468000, 456000, NO_CAP, 408000 } },
	{ { 1020000, 384000, 408000, NO_CAP, 408000 } },
	{ { 1020000, 384000, 408000, 208000, 408000 } },
	{ {  918000, 384000, 408000, 208000, 408000 } },
	{ {  918000, 384000, 408000, 208000, 408000 } },
	{ {  918000, 384000, 324000, 204000, 408000 } },
	{ {  918000, 384000, 324000, 204000, 408000 } },
	{ {  918000, 384000, 324000, 204000, 408000 } },
	{ {  918000, 384000, 324000, 136000, 408000 } },
	{ {  918000, 384000, 324000, 136000, 408000 } },
	{ {  918000, 384000, 252000, 136000, 408000 } },
	{ {  816000, 300000, 252000, 102000, 312000 } },
	{ {  816000, 300000, 252000, 102000, 312000 } },
	{ {  816000, 300000, 204000, 102000, 312000 } },
	{ {  816000, 300000, 204000, 102000, 312000 } },
	{ {  816000, 300000, 204000,  81600, 312000 } },
};

static struct balanced_throttle skin_throttle = {
	.throt_tab_size = ARRAY_SIZE(skin_throttle_table),
	.throt_tab = skin_throttle_table,
};

static int __init pluto_skin_init(void)
{
	if (machine_is_pisces()) {
		balanced_throttle_register(&skin_throttle, "skin-balanced");
		tegra_skin_therm_est_device.dev.platform_data = &skin_data;
		platform_device_register(&tegra_skin_therm_est_device);
	}

	return 0;
}

late_initcall(pluto_skin_init);
#endif

void __init max77665_init(void)
{
	int err;

	/* For battery presence into charger driver */
	if (get_power_supply_type() == POWER_SUPPLY_TYPE_BATTERY)
		max77665_charger.is_battery_present = true;

	/* Request GPIO for MAX77665's INTB */
	err = gpio_request(MAX77665_INTB_GPIO, "MAX77665_INTB");
	if (err < 0) {
		pr_err("%s: gpio request failed %d\n", __func__, err);
		goto fail_init_irq;
	}

	err = gpio_direction_input(MAX77665_INTB_GPIO);
	if (err < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, err);
		gpio_free(MAX77665_INTB_GPIO);
		goto fail_init_irq;
	}

	pluto_i2c_board_info_max77665[0].irq = gpio_to_irq(MAX77665_INTB_GPIO);

fail_init_irq:
	err = i2c_register_board_info(4, pluto_i2c_board_info_max77665,
		ARRAY_SIZE(pluto_i2c_board_info_max77665));
	if (err)
		pr_err("%s: max77665 device register failed.\n", __func__);

	return;
}

int __init pluto_sensors_init(void)
{
	int err;

	tegra_get_board_info(&board_info);

	pr_debug("%s: ++\n", __func__);
	pluto_camera_init();

	err = pluto_nct1008_init();
	if (err)
		return err;

	gpio_request(PL_SENSOR_IRQ_GPIO, PL_SENSOR_NAME);
	gpio_direction_input(PL_SENSOR_IRQ_GPIO);
	pluto_i2c1_isl_board_info[0].irq = gpio_to_irq(PL_SENSOR_IRQ_GPIO);
	err = i2c_register_board_info(0, pluto_i2c1_isl_board_info,
				ARRAY_SIZE(pluto_i2c1_isl_board_info));
	if (err)
		pr_err("%s: isl board register failed.\n", __func__);

	err = i2c_register_board_info(0, pluto_i2c1_pressure_board_info,
				ARRAY_SIZE(pluto_i2c1_pressure_board_info));
	if (err)
		pr_err("%s: pressure board register failed.\n", __func__);

	compass_init();
	mpuirq_init();
	max77665_init();

	if (get_power_supply_type() == POWER_SUPPLY_TYPE_BATTERY)
		max17042_pdata.is_battery_present = true;

	err = gpio_request(MAX17042_INTB_GPIO, "MAX17042_INTB");
	if (err < 0)
		pr_err("%s: gpio request failed %d\n", __func__, err);

	err = gpio_direction_input(MAX17042_INTB_GPIO);
	if (err < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, err);
		gpio_free(MAX17042_INTB_GPIO);
	}

	max17042_device[0].irq = gpio_to_irq(MAX17042_INTB_GPIO);
	err = i2c_register_board_info(0, max17042_device,
				ARRAY_SIZE(max17042_device));
	if (err)
		pr_err("%s: max17042 device register failed.\n", __func__);

	/* turn on TPS22913B for VDD_1V8_SNSR */
	gpio_request(TEGRA_GPIO_PG7, "SNSR_1V8_EN");
	gpio_direction_output(TEGRA_GPIO_PG7, 1);

	return 0;
}
