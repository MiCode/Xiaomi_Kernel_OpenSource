/*
 * arch/arm/mach-tegra/board-pluto-sensors.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.

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
#include <linux/power/max17042_battery.h>
#include <linux/power/power_supply_extcon.h>
#include <linux/nct1008.h>
#include <linux/io.h>
#include <linux/interrupt.h>
#include <mach/edp.h>
#include <mach/gpio-tegra.h>
#include <mach/pinmux-t11.h>
#include <mach/pinmux.h>
#include <media/max77665-flash.h>
#include <media/imx091.h>
#include <media/imx132.h>
#include <media/ad5816.h>
#include <media/ov5693.h>
#include <media/ad5823.h>
#include <media/ov5640.h>
#include <media/imx135.h>
#include <media/ar0833.h>
#include <media/dw9718.h>
#include <media/max77387.h>
#include <asm/mach-types.h>

#include "gpio-names.h"
#include "board.h"
#include "board-common.h"
#include "board-pluto.h"
#include "cpu-tegra.h"
#include "devices.h"
#include "tegra-board-id.h"
#include "dvfs.h"
#include "pm.h"
#include "battery-ini-model-data.h"

static struct nvc_gpio_pdata imx091_gpio_pdata[] = {
	{IMX091_GPIO_RESET, CAM_RSTN, true, false},
	{IMX091_GPIO_PWDN, CAM1_POWER_DWN_GPIO, true, false},
	{IMX091_GPIO_GP1, CAM_GPIO1, true, false}
};

static struct board_info board_info;

static struct max17042_platform_data max17042_pdata = {
	.config_data = &pluto_yoku_2000mA_max17042_battery,
	.init_data  = NULL,
	.num_init_data = 0,
	.enable_por_init = 1, /* Use POR init from Maxim appnote */
	.enable_current_sense = 1,
	.r_sns = 0,
	.is_battery_present = false, /* False as default */
};

static struct i2c_board_info max17042_device[] = {
	{
		I2C_BOARD_INFO("max17042", 0x36),
		.platform_data = &max17042_pdata,
	},
};

static struct nvc_torch_lumi_level_v1 pluto_max77665_lumi_tbl[] = {
	{0, 100000},
	{1, 201690},
	{2, 298080},
	{3, 387700},
	{4, 479050},
	{5, 562000},
	{6, 652560},
	{7, 732150},
	{8, 816050},
	{9, 896710},
	{10, 976890},
	{11, 1070160},
	{12, 1151000},
	{13, 1227790},
	{14, 1287690},
	{15, 1375060},
};

static unsigned max77665_f_estates[] = { 3500, 2375, 560, 456, 0 };

static struct max77665_f_platform_data pluto_max77665_flash_pdata = {
	.config		= {
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
		.max_peak_current_mA	= 600,
		.max_flash_threshold_mV	= 3400,
		.max_flash_hysteresis_mV = 200,
		.max_flash_lbdly_f_uS	= 256,
		.max_flash_lbdly_r_uS	= 256,
		.led_config[0] = {
			.flash_torch_ratio = 18100,
			.granularity = 1000,
			.flash_levels = ARRAY_SIZE(pluto_max77665_lumi_tbl),
			.lumi_levels = pluto_max77665_lumi_tbl,
			},
		.led_config[1] = {
			.flash_torch_ratio = 18100,
			.granularity = 1000,
			.flash_levels = ARRAY_SIZE(pluto_max77665_lumi_tbl),
			.lumi_levels = pluto_max77665_lumi_tbl,
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
	.edp_states = { 360, 0 },
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

static struct regulator_consumer_supply max77665_charger_supply[] = {
	REGULATOR_SUPPLY("usb_bat_chg", "tegra-udc.0"),
};

static struct max77665_charger_plat_data max77665_charger = {
	.fast_chg_cc = 2000, /* fast charger current*/
	.term_volt = 4200, /* charger termination voltage */
	.curr_lim = 2000, /* input current limit */
	.oc_alert = OC_3A25, /* generate OC alert at 3.25A */
	.num_cables = MAX_CABLES,
	.cables = maxim_cable,
	.extcon_name = "tegra-udc",
	.update_status = max17042_update_status,
	.is_battery_present = false, /* false as default */
	.consumer_supplies = max77665_charger_supply,
	.num_consumer_supplies = ARRAY_SIZE(max77665_charger_supply),
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
		.irq = (TEGRA_SOC_OC_IRQ_BASE + TEGRA_SOC_OC_IRQ_4),
	},
};

static struct power_supply_extcon_plat_data psy_extcon_pdata = {
	.extcon_name = "tegra-udc",
};

static struct platform_device psy_extcon_device = {
	.name = "power-supply-extcon",
	.id = -1,
	.dev = {
		.platform_data = &psy_extcon_pdata,
	},
};


/* isl29029 support is provided by isl29028*/
static struct i2c_board_info pluto_i2c1_isl_board_info[] = {
	{
		I2C_BOARD_INFO("isl29028", 0x44),
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
	if (machine_is_tegra_pluto())
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

static int pluto_dw9718_power_on(struct dw9718_power_rail *pw)
{
	int err;
	pr_debug("%s ++\n", __func__);

	if (unlikely(!pw || !pw->vdd || !pw->vdd_i2c))
		return -EFAULT;

	err = regulator_enable(pw->vdd);
	if (unlikely(err))
		goto dw9718_vdd_fail;

	err = regulator_enable(pw->vdd_i2c);
	if (unlikely(err))
		goto dw9718_i2c_fail;

	usleep_range(1000, 1020);

	/* return 1 to skip the in-driver power_on sequence */
	pr_debug("%s --\n", __func__);
	return 1;

dw9718_i2c_fail:
	regulator_disable(pw->vdd);

dw9718_vdd_fail:
	pr_err("%s FAILED\n", __func__);
	return -ENODEV;
}

static int pluto_dw9718_power_off(struct dw9718_power_rail *pw)
{
	if (unlikely(!pw || !pw->vdd || !pw->vdd_i2c))
		return -EFAULT;

	regulator_disable(pw->vdd);
	regulator_disable(pw->vdd_i2c);

	return 1;
}

static u16 dw9718_devid;
static int pluto_dw9718_detect(void *buf, size_t size)
{
	dw9718_devid = 0x9718;
	return 0;
}

static struct nvc_focus_cap dw9718_cap = {
	.settle_time = 30,
	.slew_rate = 0x3A200C,
	.focus_macro = 700,
	.focus_infinity = 360,
	.focus_hyper = 360,
};

static struct dw9718_platform_data pluto_dw9718_pdata = {
	.cfg = NVC_CFG_NODEV,
	.num = 1,
	.sync = 0,
	.dev_name = "focuser",
	.cap = &dw9718_cap,
	.power_on = pluto_dw9718_power_on,
	.power_off = pluto_dw9718_power_off,
	.detect = pluto_dw9718_detect,
};

static int pluto_focuser_power_on(struct ad5816_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->vdd || !pw->vdd_i2c)))
		return -EFAULT;

	err = regulator_enable(pw->vdd_i2c);
	if (unlikely(err))
		goto ad5816_vdd_i2c_fail;

	err = regulator_enable(pw->vdd);
	if (unlikely(err))
		goto ad5816_vdd_fail;

	return 0;

ad5816_vdd_fail:
	regulator_disable(pw->vdd_i2c);

ad5816_vdd_i2c_fail:
	pr_err("%s FAILED\n", __func__);

	return -ENODEV;
}

static int pluto_focuser_power_off(struct ad5816_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->vdd || !pw->vdd_i2c)))
		return -EFAULT;

	regulator_disable(pw->vdd);
	regulator_disable(pw->vdd_i2c);

	return 0;
}

static u16 ad5816_devid;
static u16 max77387_devid;
static int pluto_focuser_detect(void *buf, size_t size)
{
	if (!buf)
		return -EFAULT;
	if (size > sizeof(ad5816_devid))
		return -EINVAL;

	ad5816_devid = *(u16 *)buf;

	return 0;
}
static int pluto_flash_detect(void *buf, size_t size)
{
       if (!buf)
               return -EFAULT;
       if (size > sizeof(max77387_devid))
               return -EINVAL;

       max77387_devid = *(u16 *)buf;

       return 0;
}

static unsigned max77387_estates[] = {1000, 800, 600, 400, 200, 100, 0};

static struct max77387_platform_data pluto_max77387_pdata = {
	.config		= {
		.led_mask		= 3,
		.flash_trigger_mode	= 1,
		/* use ONE-SHOOT flash mode - flash triggered at the
		 * raising edge of strobe or strobe signal.
		*/
		.flash_mode		= 1,
		.def_ftimer		= 0x24,
		.max_total_current_mA	= 1000,
		.max_peak_current_mA	= 600,
		.led_config[0]	= {
			.flash_torch_ratio	= 18100,
			.granularity		= 1000,
			.flash_levels		= 0,
			.lumi_levels	= NULL,
			},
		.led_config[1]	= {
			.flash_torch_ratio	= 18100,
			.granularity		= 1000,
			.flash_levels		= 0,
			.lumi_levels		= NULL,
			},
		},
	.cfg		= NVC_CFG_NODEV,
	.dev_name	= "torch",
	.num		= 1,
	.gpio_strobe	= CAM_FLASH_STROBE,
	.edpc_config	= {
		.states		= max77387_estates,
		.num_states	= ARRAY_SIZE(max77387_estates),
		.e0_index	= 3,
		.priority	= EDP_MAX_PRIO + 2,
		},
	.detect = pluto_flash_detect,
};

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
 * pluto_vcmvdd: this is a workaround due to the focuser device(AD5816) will
 *               hook up the i2c bus if it is not powered up.
 * pluto_i2cvdd: by default, the power supply on the i2c bus is OFF. So it
 *               should be turned on every time any sensor device is activated.
*/
static struct regulator *pluto_vcmvdd;
static struct regulator *pluto_i2cvdd;

static int pluto_get_vcmvdd(void)
{
	if (!pluto_vcmvdd) {
		pluto_vcmvdd = regulator_get(NULL, "vdd_af_cam1");
		if (unlikely(WARN_ON(IS_ERR(pluto_vcmvdd)))) {
			pr_err("%s: can't get regulator vcmvdd: %ld\n",
				__func__, PTR_ERR(pluto_vcmvdd));
			pluto_vcmvdd = NULL;
			return -ENODEV;
		}
	}
	return 0;
}

static int pluto_ar0833_power_on(struct ar0833_power_rail *pw)
{
	int err;
	pr_debug("%s ++\n", __func__);

	if (unlikely(!pw || !pw->avdd || !pw->iovdd))
		return -EFAULT;

	if (pluto_get_vcmvdd())
		goto ar0833_get_vcmvdd_fail;

	gpio_set_value(CAM_RSTN, 0);
	usleep_range(1000, 1020);

	err = regulator_enable(pw->iovdd);
	if (unlikely(err))
		goto ar0833_iovdd_fail;
	usleep_range(300, 320);

	err = regulator_enable(pw->dvdd);
	if (unlikely(err))
		goto ar0833_dvdd_fail;
	usleep_range(300, 320);

	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto ar0833_avdd_fail;

	usleep_range(1000, 1020);
	gpio_set_value(CAM_RSTN, 1);

	tegra_pinmux_config_table(&mclk_enable, 1);
	usleep_range(200, 220);

	err = regulator_enable(pluto_vcmvdd);
	if (unlikely(err))
		goto ar0833_vcmvdd_fail;

	/* return 1 to skip the in-driver power_on sequence */
	pr_debug("%s --\n", __func__);
	return 1;

ar0833_vcmvdd_fail:
	regulator_disable(pw->avdd);

ar0833_avdd_fail:
	regulator_disable(pw->dvdd);

ar0833_dvdd_fail:
	regulator_disable(pw->iovdd);

ar0833_iovdd_fail:
	gpio_set_value(CAM_RSTN, 0);

ar0833_get_vcmvdd_fail:
	pr_err("%s FAILED\n", __func__);
	return -ENODEV;
}

static int pluto_ar0833_power_off(struct ar0833_power_rail *pw)
{
	if (unlikely(!pw || !pw->avdd || !pw->iovdd))
		return -EFAULT;

	usleep_range(100, 120);
	tegra_pinmux_config_table(&mclk_disable, 1);
	usleep_range(100, 120);
	gpio_set_value(CAM_RSTN, 0);
	regulator_disable(pluto_vcmvdd);
	usleep_range(100, 120);
	regulator_disable(pw->avdd);
	usleep_range(100, 120);
	regulator_disable(pw->dvdd);
	usleep_range(100, 120);
	regulator_disable(pw->iovdd);

	return 1;
}

struct ar0833_platform_data pluto_ar0833_pdata = {
	.power_on		= pluto_ar0833_power_on,
	.power_off		= pluto_ar0833_power_off,
};

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
	usleep_range(10, 20);

	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto imx135_avdd_fail;

	err = regulator_enable(pw->dvdd);
	if (unlikely(err))
		goto imx135_dvdd_fail;

	err = regulator_enable(pw->iovdd);
	if (unlikely(err))
		goto imx135_iovdd_fail;

	udelay(2);
	gpio_set_value(CAM_RSTN, 1);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 1);

	tegra_pinmux_config_table(&mclk_enable, 1);
	err = regulator_enable(pluto_i2cvdd);
	if (unlikely(err))
		goto imx135_i2c_fail;

	err = regulator_enable(pluto_vcmvdd);
	if (unlikely(err))
		goto imx135_vcm_fail;
	usleep_range(300, 310);

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

	udelay(2);
	tegra_pinmux_config_table(&mclk_disable, 1);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);
	udelay(2);

	regulator_disable(pw->iovdd);
	regulator_disable(pw->dvdd);
	regulator_disable(pw->avdd);
	regulator_disable(pluto_i2cvdd);
	regulator_disable(pluto_vcmvdd);

	return 0;
}

static struct imx135_platform_data imx135_pdata = {
	.power_on		= pluto_imx135_power_on,
	.power_off		= pluto_imx135_power_off,
};

static int pluto_imx091_power_on(struct nvc_regulator *vreg)
{
	int err;

	if (unlikely(WARN_ON(!vreg)))
		return -EFAULT;

	if (pluto_get_extra_regulators())
		goto imx091_poweron_fail;

	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);
	usleep_range(10, 20);

	err = regulator_enable(vreg[IMX091_VREG_AVDD].vreg);
	if (unlikely(err))
		goto imx091_avdd_fail;

	err = regulator_enable(vreg[IMX091_VREG_DVDD].vreg);
	if (unlikely(err))
		goto imx091_dvdd_fail;

	err = regulator_enable(vreg[IMX091_VREG_IOVDD].vreg);
	if (unlikely(err))
		goto imx091_iovdd_fail;

	udelay(2);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 1);

	tegra_pinmux_config_table(&mclk_enable, 1);
	err = regulator_enable(pluto_i2cvdd);
	if (unlikely(err))
		goto imx091_i2c_fail;

	err = regulator_enable(pluto_vcmvdd);
	if (unlikely(err))
		goto imx091_vcm_fail;
	usleep_range(300, 310);

	return 1;

imx091_vcm_fail:
	regulator_disable(pluto_i2cvdd);

imx091_i2c_fail:
	tegra_pinmux_config_table(&mclk_disable, 1);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);
	regulator_disable(vreg[IMX091_VREG_IOVDD].vreg);

imx091_iovdd_fail:
	regulator_disable(vreg[IMX091_VREG_DVDD].vreg);

imx091_dvdd_fail:
	regulator_disable(vreg[IMX091_VREG_AVDD].vreg);

imx091_avdd_fail:
imx091_poweron_fail:
	pr_err("%s FAILED\n", __func__);
	return -ENODEV;
}

static int pluto_imx091_power_off(struct nvc_regulator *vreg)
{
	if (unlikely(WARN_ON(!vreg)))
		return -EFAULT;

	udelay(2);
	tegra_pinmux_config_table(&mclk_disable, 1);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);
	udelay(2);

	regulator_disable(vreg[IMX091_VREG_IOVDD].vreg);
	regulator_disable(vreg[IMX091_VREG_DVDD].vreg);
	regulator_disable(vreg[IMX091_VREG_AVDD].vreg);
	if (pluto_i2cvdd)
		regulator_disable(pluto_i2cvdd);
	if (pluto_vcmvdd)
		regulator_disable(pluto_vcmvdd);

	return 0;
}

static struct nvc_imager_cap imx091_cap = {
	.identifier		= "IMX091",
	.sensor_nvc_interface	= 3,
	.pixel_types[0]		= 0x100,
	.orientation		= 0,
	.direction		= 0,
	.initial_clock_rate_khz	= 6000,
	.clock_profiles[0] = {
		.external_clock_khz	= 24000,
		.clock_multiplier	= 850000, /* value / 1,000,000 */
	},
	.clock_profiles[1] = {
		.external_clock_khz	= 0,
		.clock_multiplier	= 0,
	},
	.h_sync_edge		= 0,
	.v_sync_edge		= 0,
	.mclk_on_vgp0		= 0,
	.csi_port		= 0,
	.data_lanes		= 4,
	.virtual_channel_id	= 0,
	.discontinuous_clk_mode	= 1,
	.cil_threshold_settle	= 0x0,
	.min_blank_time_width	= 16,
	.min_blank_time_height	= 16,
	.preferred_mode_index	= 0,
	.focuser_guid		= NVC_FOCUS_GUID(0),
	.torch_guid		= NVC_TORCH_GUID(0),
	.cap_version		= NVC_IMAGER_CAPABILITIES_VERSION2,
};

static unsigned imx091_estates[] = { 876, 656, 220, 0 };

static struct imx091_platform_data imx091_pdata = {
	.num			= 0,
	.sync			= 0,
	.dev_name		= "camera",
	.gpio_count		= ARRAY_SIZE(imx091_gpio_pdata),
	.gpio			= imx091_gpio_pdata,
	.flash_cap		= {
		.sdo_trigger_enabled = 1,
		.adjustable_flash_timing = 1,
	},
	.cap			= &imx091_cap,
	.edpc_config	= {
		.states = imx091_estates,
		.num_states = ARRAY_SIZE(imx091_estates),
		.e0_index = ARRAY_SIZE(imx091_estates) - 1,
		.priority = EDP_MAX_PRIO + 1,
		},
	.power_on		= pluto_imx091_power_on,
	.power_off		= pluto_imx091_power_off,
};

static int pluto_imx132_power_on(struct imx132_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->avdd || !pw->iovdd || !pw->dvdd)))
		return -EFAULT;

	if (pluto_get_extra_regulators())
		goto pluto_imx132_poweron_fail;

	gpio_set_value(CAM2_POWER_DWN_GPIO, 0);

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

	udelay(2);

	gpio_set_value(CAM2_POWER_DWN_GPIO, 1);

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

	udelay(2);

	regulator_disable(pw->iovdd);
	regulator_disable(pw->dvdd);
	regulator_disable(pw->avdd);

	tegra_pinmux_config_table(&pbb0_disable, 1);

	regulator_disable(pluto_vcmvdd);
	regulator_disable(pluto_i2cvdd);

	return 0;
}

struct imx132_platform_data pluto_imx132_data = {
	.power_on = pluto_imx132_power_on,
	.power_off = pluto_imx132_power_off,
};

static struct ad5816_platform_data pluto_ad5816_pdata = {
	.cfg		= NVC_CFG_NODEV,
	.num		= 0,
	.sync		= 0,
	.dev_name	= "focuser",
	.power_on	= pluto_focuser_power_on,
	.power_off	= pluto_focuser_power_off,
	.detect		= pluto_focuser_detect,
};

static struct regulator *dvdd_1v8;
static struct regulator *avdd_cam2;
static struct regulator *vdd_af1;
static int pluto_ov5640_power_on(struct device *dev)
{
	if (avdd_cam2 == NULL) {
		avdd_cam2 = regulator_get(dev, "avdd_cam2");
		if (WARN_ON(IS_ERR(avdd_cam2))) {
			pr_err("%s: couldn't get regulator avdd_cam2: %ld\n",
				__func__, PTR_ERR(avdd_cam2));
			avdd_cam2 = NULL;
			goto avdd_cam2_fail;
		}
	}
	if (dvdd_1v8 == NULL) {
		dvdd_1v8 = regulator_get(dev, "vdd_1v8_cam12");
		if (WARN_ON(IS_ERR(dvdd_1v8))) {
			pr_err("%s: couldn't get regulator vdd_1v8_cam: %ld\n",
				__func__, PTR_ERR(dvdd_1v8));
			dvdd_1v8 = NULL;
			goto dvdd_1v8_fail;
		}
	}

	if (vdd_af1 == NULL) {
		vdd_af1 = regulator_get(dev, "vdd_af_cam1");
		if (WARN_ON(IS_ERR(vdd_af1))) {
			pr_err("%s: couldn't get regulator vdd_af_cam1: %ld\n",
				__func__, PTR_ERR(vdd_af1));
			vdd_af1 = NULL;
			goto vdd_af1_fail;
		}
	}

	/* power up sequence */
	gpio_set_value(CAM2_POWER_DWN_GPIO, 1);
	gpio_set_value(CAM_RSTN, 0);
	mdelay(1);

	regulator_enable(vdd_af1);
	regulator_enable(dvdd_1v8);
	regulator_enable(avdd_cam2);

	tegra_pinmux_config_table(&pbb0_enable, 1);

	mdelay(5);
	gpio_set_value(CAM2_POWER_DWN_GPIO, 0);
	mdelay(1);
	gpio_set_value(CAM_RSTN, 1);

	mdelay(20);

	return 0;

vdd_af1_fail:
	regulator_disable(dvdd_1v8);
dvdd_1v8_fail:
	regulator_disable(avdd_cam2);
avdd_cam2_fail:
	return -ENODEV;
}

static int pluto_ov5640_power_off(struct device *dev)
{
	gpio_set_value(CAM_RSTN, 0);

	tegra_pinmux_config_table(&pbb0_disable, 1);

	if (avdd_cam2)
		regulator_disable(avdd_cam2);
	if (dvdd_1v8)
		regulator_disable(dvdd_1v8);
	if (vdd_af1)
		regulator_disable(vdd_af1);
	gpio_set_value(CAM2_POWER_DWN_GPIO, 1);

	return 0;
}

struct ov5640_platform_data pluto_ov5640_data = {
	.power_on = pluto_ov5640_power_on,
	.power_off = pluto_ov5640_power_off,
};

static struct i2c_board_info pluto_board_info_ov5640[] = {
	{
		I2C_BOARD_INFO("ov5640", 0x3c),
		.platform_data = &pluto_ov5640_data,
	}
};

/* Because ov9772 already allocated a 'avdd' regulator alias at
 * max77663_ldo8_supply, ov5693 cannot allocate another 'avdd'
 * alias at max77663_ldo7_supply, which will cause boot up hang.
 * as a workaround, we request the regulator using its common name.
 */
static struct regulator *ov5693_avdd;
static int pluto_ov5693_power_on(struct ov5693_power_rail *pw)
{
	int err;

	if (unlikely(!pw || !pw->dovdd))
		return -EFAULT;

	if (!ov5693_avdd) {
		ov5693_avdd = regulator_get(NULL, "avdd_cam1");
		if (unlikely(WARN_ON(IS_ERR(ov5693_avdd)))) {
			pr_err("%s: can't get regulator avdd: %ld\n",
				__func__, PTR_ERR(ov5693_avdd));
			ov5693_avdd = NULL;
			return -ENODEV;
		}
	}

	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);
	usleep_range(10, 20);

	err = regulator_enable(ov5693_avdd);
	if (err)
		goto ov5693_avdd_fail;

	err = regulator_enable(pw->dovdd);
	if (err)
		goto ov5693_iovdd_fail;

	udelay(2);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 1);

	tegra_pinmux_config_table(&mclk_enable, 1);
	usleep_range(300, 310);

	return 0;

ov5693_iovdd_fail:
	regulator_disable(ov5693_avdd);

ov5693_avdd_fail:
	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);

	pr_err("%s FAILED\n", __func__);
	return -ENODEV;
}

static int pluto_ov5693_power_off(struct ov5693_power_rail *pw)
{
	if (unlikely(!pw || !pw->dovdd || !ov5693_avdd))
		return -EFAULT;

	usleep_range(21, 25);
	tegra_pinmux_config_table(&mclk_disable, 1);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);
	udelay(2);

	regulator_disable(pw->dovdd);
	regulator_disable(ov5693_avdd);

	return 0;
}

static struct nvc_gpio_pdata ov5693_gpio_pdata[] = {
	{ OV5693_GPIO_TYPE_PWRDN, CAM_RSTN, true, 0, },
};
static struct ov5693_platform_data pluto_ov5693_pdata = {
	.num		= 5693,
	.dev_name	= "camera",
	.gpio_count	= ARRAY_SIZE(ov5693_gpio_pdata),
	.gpio		= ov5693_gpio_pdata,
	.power_on	= pluto_ov5693_power_on,
	.power_off	= pluto_ov5693_power_off,
};

static int pluto_ad5823_power_on(struct ad5823_platform_data *pdata)
{
	pr_info("%s\n", __func__);
	gpio_set_value_cansleep(pdata->gpio, 1);

	return 0;
}

static int pluto_ad5823_power_off(struct ad5823_platform_data *pdata)
{
	pr_info("%s\n", __func__);
	gpio_set_value_cansleep(pdata->gpio, 0);

	return 0;
}


static struct ad5823_platform_data pluto_ad5823_pdata = {
	.gpio = CAM_AF_PWDN,
	.power_on	= pluto_ad5823_power_on,
	.power_off	= pluto_ad5823_power_off,
};

static struct i2c_board_info pluto_i2c_board_info_ov5693 = {
		I2C_BOARD_INFO("ov5693", 0x10),
		.platform_data = &pluto_ov5693_pdata,
};

static struct i2c_board_info pluto_i2c_board_info_ad5823 = {
		I2C_BOARD_INFO("ad5823", 0x0c),
		.platform_data = &pluto_ad5823_pdata,
};

static struct i2c_board_info pluto_i2c_board_info_dw9718 = {
		I2C_BOARD_INFO("dw9718", 0x0c),
		.platform_data = &pluto_dw9718_pdata,
};

static struct i2c_board_info pluto_i2c_board_info_imx091 = {
	I2C_BOARD_INFO("imx091", 0x10),
	.platform_data = &imx091_pdata,
};

static struct i2c_board_info pluto_i2c_board_info_imx135 = {
	I2C_BOARD_INFO("imx135", 0x10),
	.platform_data = &imx135_pdata,
};

static struct i2c_board_info pluto_i2c_board_info_ar0833 = {
	I2C_BOARD_INFO("ar0833", 0x36),
	.platform_data = &pluto_ar0833_pdata,
};

static struct i2c_board_info pluto_i2c_board_info_imx132 = {
	I2C_BOARD_INFO("imx132", 0x36),
	.platform_data = &pluto_imx132_data,
};

static struct i2c_board_info pluto_i2c_board_info_e1625[] = {
	{
		I2C_BOARD_INFO("ad5816", 0x0E),
		.platform_data = &pluto_ad5816_pdata,
	},
	{
		I2C_BOARD_INFO("dw9718", 0x0c),
		.platform_data = &pluto_dw9718_pdata,
	},
	{
		I2C_BOARD_INFO("max77387", 0x4A),
		.platform_data = &pluto_max77387_pdata,
	},
};

/* Detect ov5640 adapter by toggling the CAM_GPIO1 and read it back
 * from CAM_GPIO2.
 * On the ov5640 adapter board (E1633) for pluto, pin 5 & 6 of connector J9
 * should be shorted.
 */
static int ov5640_installed(void)
{
	int val;
	int ret;

	ret = gpio_request(CAM_GPIO1, "camera_gpio1");
	if (ret < 0) {
		pr_err("%s, gpio_request failed for CAM_GPIO1\n", __func__);
		return 0;
	}

	ret = gpio_request(CAM_GPIO2, "camera_gpio2");
	if (ret < 0) {
		pr_err("%s, gpio_request failed for CAM_GPIO2\n", __func__);
		gpio_free(CAM_GPIO1);
		return 0;
	}

	gpio_direction_output(CAM_GPIO1, 1);
	gpio_direction_input(CAM_GPIO2);

	val = gpio_get_value(CAM_GPIO1);
	ret = gpio_get_value(CAM_GPIO2);
	pr_info("%s round 1: %d vs %d\n", __func__, val, ret);
	if (ret != val) {
		gpio_free(CAM_GPIO1);
		gpio_free(CAM_GPIO2);
		return 0;
	}

	/* toggle CAM_GPIO1 and read back from detect pin */
	val ^= 1;
	gpio_set_value(CAM_GPIO1, val & 1);
	ret = gpio_get_value(CAM_GPIO2);
	/* resume CAM_GPIO1 state */
	gpio_set_value(CAM_GPIO1, (~val) & 1);

	gpio_free(CAM_GPIO2);
	gpio_free(CAM_GPIO1);

	pr_info("%s round 2: %d vs %d\n", __func__, val, ret);
	if (ret != val)
		return 0;

	return 1;
}

static int pluto_camera_init(void)
{
	pr_debug("%s: ++\n", __func__);

	tegra_pinmux_config_table(&mclk_disable, 1);
	tegra_pinmux_config_table(&pbb0_disable, 1);

	if (ov5640_installed()) {
		pr_info("%s ov5640 installed.\n", __func__);
		i2c_register_board_info(2, pluto_board_info_ov5640,
			ARRAY_SIZE(pluto_board_info_ov5640));
	} else
		i2c_register_board_info(2, pluto_i2c_board_info_e1625,
			ARRAY_SIZE(pluto_i2c_board_info_e1625));

	return 0;
}

/* MPU board file definition */
static struct mpu_platform_data mpu_gyro_data = {
	.int_config	= 0x00,
	.level_shifter	= 0,
	.orientation	= MPU_GYRO_ORIENTATION,
	.sec_slave_type	= SECONDARY_SLAVE_TYPE_NONE,
	.key		= {0x4E, 0xCC, 0x7E, 0xEB, 0xF6, 0x1E, 0x35, 0x22,
			   0x00, 0x34, 0x0D, 0x65, 0x32, 0xE9, 0x94, 0x89},
};

static struct mpu_platform_data mpu_compass_data = {
	.orientation	= MPU_COMPASS_ORIENTATION,
	.config		= NVI_CONFIG_BOOT_MPU,
};

static struct mpu_platform_data bmp180_pdata = {
	.config		= NVI_CONFIG_BOOT_MPU,
};

static struct i2c_board_info __initdata inv_mpu_i2c0_board_info[] = {
	{
		I2C_BOARD_INFO(MPU_GYRO_NAME, MPU_GYRO_ADDR),
		.platform_data = &mpu_gyro_data,
	},
	{
		/* The actual BMP180 address is 0x77 but because this conflicts
		 * with another device, this address is hacked so Linux will
		 * call the driver.  The conflict is technically okay since the
		 * BMP180 is behind the MPU.  Also, the BMP180 driver uses a
		 * hard-coded address of 0x77 since it can't be changed anyway.
		 */
		I2C_BOARD_INFO("bmp180", 0x78),
		.platform_data = &bmp180_pdata,
	},
	{
		I2C_BOARD_INFO(MPU_COMPASS_NAME, MPU_COMPASS_ADDR),
		.platform_data = &mpu_compass_data,
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
#if MPU_COMPASS_IRQ_GPIO
	inv_mpu_i2c0_board_info[i++].irq = gpio_to_irq(MPU_COMPASS_IRQ_GPIO);
#endif
	i2c_register_board_info(MPU_GYRO_BUS_NUM, inv_mpu_i2c0_board_info,
		ARRAY_SIZE(inv_mpu_i2c0_board_info));
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

	pluto_i2c4_nct1008_board_info[0].irq =
		gpio_to_irq(nct1008_port);
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
		.trip_temp = 45000,
		.trip_type = THERMAL_TRIP_PASSIVE,
		.upper = THERMAL_NO_LIMIT,
		.lower = THERMAL_NO_LIMIT,
		.hysteresis = 0,
	},
};

static struct therm_est_subdevice skin_devs[] = {
	{
		.dev_data = "Tdiode",
		.coeffs = {
			2, 1, 1, 1,
			1, 1, 1, 1,
			1, 1, 1, 0,
			1, 1, 0, 0,
			0, 0, -1, -7
		},
	},
	{
		.dev_data = "Tboard",
		.coeffs = {
			-11, -7, -5, -3,
			-3, -2, -1, 0,
			0, 0, 1, 1,
			1, 2, 2, 3,
			4, 6, 11, 18
		},
	},
};

static struct therm_est_data skin_data = {
	.num_trips = ARRAY_SIZE(skin_trips),
	.trips = skin_trips,
	.toffset = 9793,
	.polling_period = 1100,
	.passive_delay = 15000,
	.tc1 = 10,
	.tc2 = 1,
	.ndevs = ARRAY_SIZE(skin_devs),
	.devs = skin_devs,
};

static struct throttle_table skin_throttle_table[] = {
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

static struct balanced_throttle skin_throttle = {
	.throt_tab_size = ARRAY_SIZE(skin_throttle_table),
	.throt_tab = skin_throttle_table,
};

static int __init pluto_skin_init(void)
{
	if (machine_is_tegra_pluto()) {
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

	err = i2c_register_board_info(4, pluto_i2c_board_info_max77665,
		ARRAY_SIZE(pluto_i2c_board_info_max77665));
	if (err)
		pr_err("%s: max77665 device register failed.\n", __func__);

	platform_device_register(&psy_extcon_device);

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

	err = i2c_register_board_info(0, pluto_i2c1_isl_board_info,
				ARRAY_SIZE(pluto_i2c1_isl_board_info));
	if (err)
		pr_err("%s: isl board register failed.\n", __func__);

	mpuirq_init();
	max77665_init();

	if (get_power_supply_type() == POWER_SUPPLY_TYPE_BATTERY)
		max17042_pdata.is_battery_present = true;

	err = i2c_register_board_info(0, max17042_device,
				ARRAY_SIZE(max17042_device));
	if (err)
		pr_err("%s: max17042 device register failed.\n", __func__);

	return 0;
}

static int pluto_chk_conflict(struct device *dev, void *addrp)
{
	struct i2c_client	*client = i2c_verify_client(dev);
	unsigned short		addr = *(unsigned short *)addrp;

	if (!client)
		return 0;

	pr_info("%s: %s %x - %x\n", __func__, client->name, client->addr, addr);
	if (client->addr == addr)
		i2c_unregister_device(client);
	return 0;
}

static int camera_auto_detect(void)
{
	struct i2c_adapter *adap = i2c_get_adapter(2);

	pr_info("%s ++ %04x - %04x - %04x\n",
		__func__, ad5816_devid, dw9718_devid, max77387_devid);

	if ((ad5816_devid & 0xff00) == 0x2400) {
		if ((max77387_devid & 0xff) == 0x91) {
			/* IMX135 found */
			i2c_new_device(adap, &pluto_i2c_board_info_imx135);
		} else {
			/* IMX091 found*/
			i2c_new_device(adap, &pluto_i2c_board_info_imx091);
		}
	} else if (dw9718_devid) {
		if (!max77387_devid) {
			/* board e1823, IMX135 found */
			i2c_new_device(adap, &pluto_i2c_board_info_imx135);
			/* remove current dw9718 */
			device_for_each_child(&adap->dev,
				&pluto_i2c_board_info_dw9718.addr,
				pluto_chk_conflict);
			/* reinstall with new device node */
			pluto_dw9718_pdata.num = 0;
			i2c_new_device(adap, &pluto_i2c_board_info_dw9718);
		} else {
			/* AR0833 found */
			i2c_new_device(adap, &pluto_i2c_board_info_ar0833);
		}
	} else { /* default using ov5693 + ad5823 */
		device_for_each_child(&adap->dev,
			&pluto_i2c_board_info_ad5823.addr,
			pluto_chk_conflict);
		i2c_new_device(adap, &pluto_i2c_board_info_ov5693);
		i2c_new_device(adap, &pluto_i2c_board_info_ad5823);
	}
	i2c_new_device(adap, &pluto_i2c_board_info_imx132);

	pr_info("%s --\n", __func__);
	return 0;
}

int __init pluto_camera_late_init(void)
{
	if (board_info.board_id != BOARD_E1580)	{
		pr_debug("%s: Pluto not found!\n", __func__);
		return 0;
	}

	camera_auto_detect();

	return 0;
}


late_initcall(pluto_camera_late_init);
