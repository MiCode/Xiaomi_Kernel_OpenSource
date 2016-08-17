/*
 * arch/arm/mach-tegra/board-dalmore-sensors.c
 *
 * Copyright (c) 2012-2013 NVIDIA CORPORATION, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * Redistributions of source code must retain the above copyright notice,
 * this list of conditions and the following disclaimer.
 *
 * Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * Neither the name of NVIDIA CORPORATION nor the names of its contributors
 * may be used to endorse or promote products derived from this software
 * without specific prior written permission.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/mpu.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/therm_est.h>
#include <linux/nct1008.h>
#include <mach/edp.h>
#include <linux/edp.h>
#include <mach/gpio-tegra.h>
#include <mach/pinmux-t11.h>
#include <mach/pinmux.h>
#include <media/imx091.h>
#include <media/ov9772.h>
#include <media/as364x.h>
#include <media/ad5816.h>
#include <media/ov5693.h>
#include <media/ad5823.h>
#include <media/imx135.h>
#include <media/imx132.h>
#include <media/ar0833.h>
#include <media/dw9718.h>
#include <media/max77387.h>
#include <generated/mach-types.h>
#include <linux/power/sbs-battery.h>

#include "gpio-names.h"
#include "board.h"
#include "board-common.h"
#include "board-dalmore.h"
#include "cpu-tegra.h"
#include "devices.h"
#include "tegra-board-id.h"
#include "dvfs.h"

static struct nvc_gpio_pdata imx091_gpio_pdata[] = {
	{IMX091_GPIO_RESET, CAM_RSTN, true, false},
	{IMX091_GPIO_PWDN, CAM1_POWER_DWN_GPIO, true, false},
	{IMX091_GPIO_GP1, CAM_GPIO1, true, false}
};

static struct board_info board_info;

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

static int __init dalmore_throttle_init(void)
{
	if (machine_is_dalmore())
		balanced_throttle_register(&tj_throttle, "tegra-balanced");
	return 0;
}
module_init(dalmore_throttle_init);

static struct nct1008_platform_data dalmore_nct1008_pdata = {
	.supported_hwrev = true,
	.ext_range = true,
	.conv_rate = 0x06, /* 4Hz conversion rate */
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
};

static struct i2c_board_info dalmore_i2c4_nct1008_board_info[] = {
	{
		I2C_BOARD_INFO("nct1008", 0x4C),
		.platform_data = &dalmore_nct1008_pdata,
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

static int dalmore_focuser_power_on(struct ad5816_power_rail *pw)
{
	int err;

	pr_info("%s\n", __func__);
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

static int dalmore_focuser_power_off(struct ad5816_power_rail *pw)
{
	pr_info("%s\n", __func__);
	if (unlikely(WARN_ON(!pw || !pw->vdd || !pw->vdd_i2c)))
		return -EFAULT;

	regulator_disable(pw->vdd);
	regulator_disable(pw->vdd_i2c);

	return 0;
}

static u16 ad5816_devid;
static u16 max77387_devid;
static struct ad5816_platform_data dalmore_imx091_ad5816_pdata;
static struct ad5816_platform_data dalmore_imx135_ad5816_pdata;

static int dalmore_focuser_detect(void *buf, size_t size)
{
	if (!buf)
		return -EFAULT;
	if (size > sizeof(ad5816_devid))
		return -EINVAL;

	ad5816_devid = *(u16 *)buf;
	/* overwrite platform data if module is imx135 */
	if ((ad5816_devid & 0xff) >= 0x6)
		memcpy(&dalmore_imx091_ad5816_pdata,
			&dalmore_imx135_ad5816_pdata,
			sizeof(dalmore_imx091_ad5816_pdata));

	return 0;
}

static int dalmore_flash_detect(void *buf, size_t size)
{
	if (!buf)
		return -EFAULT;
	if (size > sizeof(max77387_devid))
		return -EINVAL;

	max77387_devid = *(u16 *)buf;

	return 0;
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
 * As a workaround, dalmore_vcmvdd need to be allocated to activate the
 * sensor devices. This is due to the focuser device(AD5816) will hook up
 * the i2c bus if it is not powered up.
*/
static struct regulator *dalmore_vcmvdd;

static int dalmore_get_vcmvdd(void)
{
	if (!dalmore_vcmvdd) {
		dalmore_vcmvdd = regulator_get(NULL, "vdd_af_cam1");
		if (unlikely(WARN_ON(IS_ERR(dalmore_vcmvdd)))) {
			pr_err("%s: can't get regulator vcmvdd: %ld\n",
				__func__, PTR_ERR(dalmore_vcmvdd));
			dalmore_vcmvdd = NULL;
			return -ENODEV;
		}
	}
	return 0;
}

static int dalmore_ar0833_power_on(struct ar0833_power_rail *pw)
{
	int err;
	pr_debug("%s ++\n", __func__);

	if (unlikely(!pw || !pw->avdd || !pw->iovdd))
		return -EFAULT;

	if (dalmore_get_vcmvdd())
		goto ar0833_get_vcmvdd_fail;

	gpio_set_value(CAM_RSTN, 0);
	usleep_range(1000, 1020);

	err = regulator_enable(pw->iovdd);
	if (unlikely(err))
		goto ar0833_iovdd_fail;
	usleep_range(300, 320);

	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto ar0833_avdd_fail;

	usleep_range(1000, 1020);
	gpio_set_value(CAM_RSTN, 1);

	tegra_pinmux_config_table(&mclk_enable, 1);
	usleep_range(200, 220);

	err = regulator_enable(dalmore_vcmvdd);
	if (unlikely(err))
		goto ar0833_vcmvdd_fail;

	/* return 1 to skip the in-driver power_on sequence */
	pr_debug("%s --\n", __func__);
	return 1;

ar0833_vcmvdd_fail:
	regulator_disable(pw->avdd);

ar0833_avdd_fail:
	regulator_disable(pw->iovdd);

ar0833_iovdd_fail:
	gpio_set_value(CAM_RSTN, 0);

ar0833_get_vcmvdd_fail:
	pr_err("%s FAILED\n", __func__);
	return -ENODEV;
}

static int dalmore_ar0833_power_off(struct ar0833_power_rail *pw)
{
	if (unlikely(!pw || !pw->avdd || !pw->iovdd))
		return -EFAULT;

	usleep_range(100, 120);
	tegra_pinmux_config_table(&mclk_disable, 1);
	usleep_range(100, 120);
	gpio_set_value(CAM_RSTN, 0);
	regulator_disable(dalmore_vcmvdd);
	usleep_range(100, 120);
	regulator_disable(pw->avdd);
	usleep_range(100, 120);
	regulator_disable(pw->iovdd);

	return 1;
}

struct ar0833_platform_data dalmore_ar0833_pdata = {
	.power_on = dalmore_ar0833_power_on,
	.power_off = dalmore_ar0833_power_off,
};

static int dalmore_imx091_power_on(struct nvc_regulator *vreg)
{
	int err;

	if (unlikely(WARN_ON(!vreg)))
		return -EFAULT;

	if (dalmore_get_vcmvdd())
		goto imx091_poweron_fail;

	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);
	usleep_range(10, 20);

	err = regulator_enable(vreg[IMX091_VREG_AVDD].vreg);
	if (err)
		goto imx091_avdd_fail;

	err = regulator_enable(vreg[IMX091_VREG_IOVDD].vreg);
	if (err)
		goto imx091_iovdd_fail;

	udelay(2);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 1);

	err = regulator_enable(dalmore_vcmvdd);
	if (unlikely(err))
		goto imx091_vcmvdd_fail;

	tegra_pinmux_config_table(&mclk_enable, 1);
	usleep_range(300, 310);

	return 1;

imx091_vcmvdd_fail:
	regulator_disable(vreg[IMX091_VREG_IOVDD].vreg);

imx091_iovdd_fail:
	regulator_disable(vreg[IMX091_VREG_AVDD].vreg);

imx091_avdd_fail:
	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);

imx091_poweron_fail:
	pr_err("%s FAILED\n", __func__);
	return -ENODEV;
}

static int dalmore_imx091_power_off(struct nvc_regulator *vreg)
{
	if (unlikely(WARN_ON(!vreg)))
		return -EFAULT;

	udelay(2);
	tegra_pinmux_config_table(&mclk_disable, 1);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);
	udelay(2);

	regulator_disable(dalmore_vcmvdd);
	regulator_disable(vreg[IMX091_VREG_IOVDD].vreg);
	regulator_disable(vreg[IMX091_VREG_AVDD].vreg);

	return 1;
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
	.power_on		= dalmore_imx091_power_on,
	.power_off		= dalmore_imx091_power_off,
};

struct sbs_platform_data sbs_pdata = {
	.poll_retry_count = 100,
	.i2c_retry_count = 2,
};

static int dalmore_imx135_power_on(struct imx135_power_rail *pw)
{
	int err;

	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd)))
		return -EFAULT;

	gpio_set_value(CAM_RSTN, 0);
	gpio_set_value(CAM_AF_PWDN, 1);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);
	usleep_range(10, 20);

	err = regulator_enable(pw->avdd);
	if (err)
		goto imx135_avdd_fail;

	err = regulator_enable(pw->iovdd);
	if (err)
		goto imx135_iovdd_fail;

	udelay(2);
	gpio_set_value(CAM_RSTN, 1);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 1);

	tegra_pinmux_config_table(&mclk_enable, 1);
	usleep_range(300, 310);

	return 1;

imx135_iovdd_fail:
	regulator_disable(pw->avdd);

imx135_avdd_fail:
	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);

	return -ENODEV;
}

static int dalmore_imx135_power_off(struct imx135_power_rail *pw)
{
	if (unlikely(WARN_ON(!pw || !pw->iovdd || !pw->avdd)))
		return -EFAULT;

	udelay(2);
	tegra_pinmux_config_table(&mclk_disable, 1);
	gpio_set_value(CAM1_POWER_DWN_GPIO, 0);
	udelay(2);

	regulator_disable(pw->iovdd);
	regulator_disable(pw->avdd);
	return 0;
}

struct imx135_platform_data dalmore_imx135_data = {
	.flash_cap = {
		.enable = 1,
		.edge_trig_en = 1,
		.start_edge = 0,
		.repeat = 1,
		.delay_frm = 0,
	},
	.power_on = dalmore_imx135_power_on,
	.power_off = dalmore_imx135_power_off,
};

static int dalmore_ov9772_power_on(struct ov9772_power_rail *pw)
{
	int err;

	if (unlikely(!pw || !pw->avdd || !pw->dovdd))
		return -EFAULT;

	if (dalmore_get_vcmvdd())
		goto ov9772_get_vcmvdd_fail;

	gpio_set_value(CAM2_POWER_DWN_GPIO, 0);
	gpio_set_value(CAM_RSTN, 0);

	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto ov9772_avdd_fail;

	err = regulator_enable(pw->dovdd);
	if (unlikely(err))
		goto ov9772_dovdd_fail;

	gpio_set_value(CAM_RSTN, 1);
	gpio_set_value(CAM2_POWER_DWN_GPIO, 1);

	err = regulator_enable(dalmore_vcmvdd);
	if (unlikely(err))
		goto ov9772_vcmvdd_fail;

	tegra_pinmux_config_table(&pbb0_enable, 1);
	usleep_range(340, 380);

	/* return 1 to skip the in-driver power_on sequence */
	return 1;

ov9772_vcmvdd_fail:
	regulator_disable(pw->dovdd);

ov9772_dovdd_fail:
	regulator_disable(pw->avdd);

ov9772_avdd_fail:
	gpio_set_value(CAM_RSTN, 0);
	gpio_set_value(CAM2_POWER_DWN_GPIO, 0);

ov9772_get_vcmvdd_fail:
	pr_err("%s FAILED\n", __func__);
	return -ENODEV;
}

static int dalmore_ov9772_power_off(struct ov9772_power_rail *pw)
{
	if (unlikely(!pw || !dalmore_vcmvdd || !pw->avdd || !pw->dovdd))
		return -EFAULT;

	usleep_range(21, 25);
	tegra_pinmux_config_table(&pbb0_disable, 1);

	gpio_set_value(CAM2_POWER_DWN_GPIO, 0);
	gpio_set_value(CAM_RSTN, 0);

	regulator_disable(dalmore_vcmvdd);
	regulator_disable(pw->dovdd);
	regulator_disable(pw->avdd);

	/* return 1 to skip the in-driver power_off sequence */
	return 1;
}

static struct nvc_gpio_pdata ov9772_gpio_pdata[] = {
	{ OV9772_GPIO_TYPE_SHTDN, CAM2_POWER_DWN_GPIO, true, 0, },
	{ OV9772_GPIO_TYPE_PWRDN, CAM_RSTN, true, 0, },
};

static struct ov9772_platform_data dalmore_ov9772_pdata = {
	.num		= 1,
	.dev_name	= "camera",
	.gpio_count	= ARRAY_SIZE(ov9772_gpio_pdata),
	.gpio		= ov9772_gpio_pdata,
	.power_on	= dalmore_ov9772_power_on,
	.power_off	= dalmore_ov9772_power_off,
};

static int dalmore_as3648_power_on(struct as364x_power_rail *pw)
{
	int err = dalmore_get_vcmvdd();

	if (err)
		return err;

	return regulator_enable(dalmore_vcmvdd);
}

static int dalmore_as3648_power_off(struct as364x_power_rail *pw)
{
	if (!dalmore_vcmvdd)
		return -ENODEV;

	return regulator_disable(dalmore_vcmvdd);
}

static struct as364x_platform_data dalmore_as3648_pdata = {
	.config		= {
		.led_mask = 3,
		.max_total_current_mA = 1000,
		.max_peak_current_mA = 600,
		.max_torch_current_mA = 150,
		.vin_low_v_run_mV = 3070,
		.strobe_type = 1,
		},
	.pinstate	= {
		.mask	= 1 << (CAM_FLASH_STROBE - TEGRA_GPIO_PBB0),
		.values	= 1 << (CAM_FLASH_STROBE - TEGRA_GPIO_PBB0)
		},
	.dev_name	= "torch",
	.type		= AS3648,
	.gpio_strobe	= CAM_FLASH_STROBE,

	.power_on_callback = dalmore_as3648_power_on,
	.power_off_callback = dalmore_as3648_power_off,
};

static unsigned max77387_estates[] = {1000, 800, 600, 400, 200, 100, 0};

static struct max77387_platform_data dalmore_max77387_pdata = {
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
	.gpio_strobe	= CAM_FLASH_STROBE,
	.edpc_config	= {
		.states		= max77387_estates,
		.num_states	= ARRAY_SIZE(max77387_estates),
		.e0_index	= 3,
		.priority	= EDP_MAX_PRIO + 2,
		},
	.detect = dalmore_flash_detect,
};

static struct nvc_focus_cap dalmore_imx091_ad5816_cap = {
	.version = NVC_FOCUS_CAP_VER2,
	.settle_time = 30,
	.focus_macro = 620,
	.focus_infinity = 70,
	.focus_hyper = 70,
};

static struct ad5816_platform_data dalmore_imx091_ad5816_pdata = {
	.cfg = NVC_CFG_NODEV,
	.num = 0,
	.sync = 0,
	.dev_name = "focuser",
	.cap = &dalmore_imx091_ad5816_cap,
	.arc_mode = 3,
	.lens_freq = 920,
	.power_on = dalmore_focuser_power_on,
	.power_off = dalmore_focuser_power_off,
	.detect = dalmore_focuser_detect,
};

static struct nvc_focus_cap dalmore_imx135_ad5816_cap = {
	.version = NVC_FOCUS_CAP_VER2,
	.settle_time = 10,
	.focus_macro = 590,
	.focus_infinity = 230,
	.focus_hyper = 230,
};

static struct ad5816_platform_data dalmore_imx135_ad5816_pdata = {
	.cfg = NVC_CFG_NODEV,
	.num = 0,
	.sync = 0,
	.dev_name = "focuser",
	.cap = &dalmore_imx135_ad5816_cap,
	.arc_mode = 1,
	.lens_freq = 1050,
	.power_on = dalmore_focuser_power_on,
	.power_off = dalmore_focuser_power_off,
	.detect = dalmore_focuser_detect,
};

static int dalmore_dw9718_power_on(struct dw9718_power_rail *pw)
{
	int err;
	pr_info("%s\n", __func__);

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

static int dalmore_dw9718_power_off(struct dw9718_power_rail *pw)
{
	pr_info("%s\n", __func__);

	if (unlikely(!pw || !pw->vdd || !pw->vdd_i2c))
		return -EFAULT;

	regulator_disable(pw->vdd);
	regulator_disable(pw->vdd_i2c);

	return 1;
}

static u16 dw9718_devid;
static int dalmore_dw9718_detect(void *buf, size_t size)
{
	dw9718_devid = 0x9718;
	return 0;
}

static struct nvc_focus_cap dw9718_cap = {
	.settle_time = 30,
	.slew_rate = 0x3A200C,
	.focus_macro = 450,
	.focus_infinity = 200,
	.focus_hyper = 200,
};

static struct dw9718_platform_data dalmore_dw9718_pdata = {
	.cfg = NVC_CFG_NODEV,
	.num = 1,
	.sync = 0,
	.dev_name = "focuser",
	.cap = &dw9718_cap,
	.power_on = dalmore_dw9718_power_on,
	.power_off = dalmore_dw9718_power_off,
	.detect = dalmore_dw9718_detect,
};

/* Because ov9772 already allocated a 'avdd' regulator alias at
 * max77663_ldo8_supply, ov5693 cannot allocate another 'avdd'
 * alias at max77663_ldo7_supply, which will cause boot up hang.
 * as a workaround, we request the regulator using its common name.
 */
static struct regulator *ov5693_avdd;
static int dalmore_ov5693_power_on(struct ov5693_power_rail *pw)
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

static int dalmore_ov5693_power_off(struct ov5693_power_rail *pw)
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
static struct ov5693_platform_data dalmore_ov5693_pdata = {
	.num		= 5693,
	.dev_name	= "camera",
	.gpio_count	= ARRAY_SIZE(ov5693_gpio_pdata),
	.gpio		= ov5693_gpio_pdata,
	.power_on	= dalmore_ov5693_power_on,
	.power_off	= dalmore_ov5693_power_off,
};

static int dalmore_imx132_power_on(struct imx132_power_rail *pw)
{
	int err;

	if (unlikely(!pw || !pw->avdd || !pw->iovdd))
		return -EFAULT;

	gpio_set_value(CAM2_POWER_DWN_GPIO, 0);

	tegra_pinmux_config_table(&pbb0_enable, 1);

	err = regulator_enable(pw->avdd);
	if (unlikely(err))
		goto imx132_avdd_fail;

	err = regulator_enable(pw->iovdd);
	if (unlikely(err))
		goto imx132_iovdd_fail;

	udelay(2);

	gpio_set_value(CAM2_POWER_DWN_GPIO, 1);

	/* return 1 to skip the in-driver power_on sequence */
	return 1;

imx132_iovdd_fail:
	regulator_disable(pw->avdd);

imx132_avdd_fail:
	pr_err("%s FAILED\n", __func__);
	return -ENODEV;
}

static int dalmore_imx132_power_off(struct imx132_power_rail *pw)
{
	if (unlikely(!pw || !pw->avdd || !pw->iovdd))
		return -EFAULT;

	udelay(2);

	gpio_set_value(CAM2_POWER_DWN_GPIO, 0);

	regulator_disable(pw->iovdd);
	regulator_disable(pw->avdd);

	tegra_pinmux_config_table(&pbb0_disable, 1);

	/* return 1 to skip the in-driver power_off sequence */
	return 1;
}

struct imx132_platform_data dalmore_imx132_data = {
	.power_on = dalmore_imx132_power_on,
	.power_off = dalmore_imx132_power_off,
};

static int dalmore_ad5823_power_on(struct ad5823_platform_data *pdata)
{
	int err = 0;

	pr_info("%s\n", __func__);
	gpio_set_value_cansleep(pdata->gpio, 1);

	return err;
}

static int dalmore_ad5823_power_off(struct ad5823_platform_data *pdata)
{
	pr_info("%s\n", __func__);
	gpio_set_value_cansleep(pdata->gpio, 0);

	return 0;
}


static struct ad5823_platform_data dalmore_ad5823_pdata = {
	.gpio = CAM_AF_PWDN,
	.power_on	= dalmore_ad5823_power_on,
	.power_off	= dalmore_ad5823_power_off,
};

static struct i2c_board_info dalmore_i2c_board_info_ov5693 = {
		I2C_BOARD_INFO("ov5693", 0x10),
		.platform_data = &dalmore_ov5693_pdata,
};

static struct i2c_board_info dalmore_i2c_board_info_ad5823 = {
		I2C_BOARD_INFO("ad5823", 0x0c),
		.platform_data = &dalmore_ad5823_pdata,
};

static struct i2c_board_info dalmore_i2c_board_info_e1625[] = {
	{
		I2C_BOARD_INFO("ad5816", 0x0E),
		.platform_data = &dalmore_imx091_ad5816_pdata,
	},
	{
		I2C_BOARD_INFO("dw9718", 0x0c),
		.platform_data = &dalmore_dw9718_pdata,
	},
	{
		I2C_BOARD_INFO("max77387", 0x4A),
		.platform_data = &dalmore_max77387_pdata,
	},
};

static struct i2c_board_info dalmore_i2c_board_info_imx091 = {
	I2C_BOARD_INFO("imx091", 0x36),
	.platform_data = &imx091_pdata,
};

static struct i2c_board_info dalmore_i2c_board_info_imx135 = {
	I2C_BOARD_INFO("imx135", 0x10),
	.platform_data = &dalmore_imx135_data,
};

static struct i2c_board_info dalmore_i2c_board_info_ar0833 = {
		I2C_BOARD_INFO("ar0833", 0x36),
		.platform_data = &dalmore_ar0833_pdata,
};

static struct i2c_board_info dalmore_i2c_board_info_ov9772 = {
	I2C_BOARD_INFO("ov9772", 0x10),
	.platform_data = &dalmore_ov9772_pdata,
};

static struct i2c_board_info dalmore_i2c_board_info_imx132 = {
	I2C_BOARD_INFO("imx132", 0x36),
	.platform_data = &dalmore_imx132_data,
};

static struct i2c_board_info dalmore_i2c_board_info_as3648 = {
	I2C_BOARD_INFO("as3648", 0x30),
	.platform_data = &dalmore_as3648_pdata,
};

static int dalmore_camera_init(void)
{
	tegra_pinmux_config_table(&mclk_disable, 1);
	tegra_pinmux_config_table(&pbb0_disable, 1);

	i2c_register_board_info(2, dalmore_i2c_board_info_e1625,
		ARRAY_SIZE(dalmore_i2c_board_info_e1625));
	return 0;
}

#define TEGRA_CAMERA_GPIO(_gpio, _label, _value)		\
	{							\
		.gpio = _gpio,					\
		.label = _label,				\
		.value = _value,				\
	}

static struct i2c_board_info dalmore_i2c_board_info_cm3218[] = {
	{
		I2C_BOARD_INFO("cm3218", 0x48),
	},
};

/* MPU board file definition	*/
static struct mpu_platform_data mpu9150_gyro_data = {
	.int_config	= 0x10,
	.level_shifter	= 0,
	/* Located in board_[platformname].h */
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

static struct i2c_board_info __initdata inv_mpu9150_i2c2_board_info[] = {
	{
		I2C_BOARD_INFO(MPU_GYRO_NAME, MPU_GYRO_ADDR),
		.platform_data = &mpu9150_gyro_data,
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
	unsigned gyro_irq_gpio = MPU_GYRO_IRQ_GPIO;
	unsigned gyro_bus_num = MPU_GYRO_BUS_NUM;
	char *gyro_name = MPU_GYRO_NAME;

	pr_info("*** MPU START *** mpuirq_init...\n");

	ret = gpio_request(gyro_irq_gpio, gyro_name);

	if (ret < 0) {
		pr_err("%s: gpio_request failed %d\n", __func__, ret);
		return;
	}

	ret = gpio_direction_input(gyro_irq_gpio);
	if (ret < 0) {
		pr_err("%s: gpio_direction_input failed %d\n", __func__, ret);
		gpio_free(gyro_irq_gpio);
		return;
	}
	pr_info("*** MPU END *** mpuirq_init...\n");

	inv_mpu9150_i2c2_board_info[0].irq = gpio_to_irq(MPU_GYRO_IRQ_GPIO);
	i2c_register_board_info(gyro_bus_num, inv_mpu9150_i2c2_board_info,
		ARRAY_SIZE(inv_mpu9150_i2c2_board_info));
}

static int __init dalmore_nct1008_init(void)
{
	int nct1008_port;
	int ret = 0;

	if ((board_info.board_id == BOARD_E1612) ||
	    (board_info.board_id == BOARD_E1641) ||
	    (board_info.board_id == BOARD_E1613) ||
	    (board_info.board_id == BOARD_P2454))
	{
		/* per email from Matt 9/10/2012 */
		nct1008_port = TEGRA_GPIO_PX6;
	} else if (board_info.board_id == BOARD_E1611) {
		if (board_info.fab == 0x04)
			nct1008_port = TEGRA_GPIO_PO4;
		else
			nct1008_port = TEGRA_GPIO_PX6;
	} else {
		nct1008_port = TEGRA_GPIO_PX6;
		pr_err("Warning: nct alert_port assumed TEGRA_GPIO_PX6"
			" for unknown dalmore board id E%d\n",
			board_info.board_id);
	}

	tegra_add_cdev_trips(dalmore_nct1008_pdata.trips,
				&dalmore_nct1008_pdata.num_trips);

	dalmore_i2c4_nct1008_board_info[0].irq = gpio_to_irq(nct1008_port);
	pr_info("%s: dalmore nct1008 irq %d",
			__func__, dalmore_i2c4_nct1008_board_info[0].irq);

	ret = gpio_request(nct1008_port, "temp_alert");
	if (ret < 0)
		return ret;

	ret = gpio_direction_input(nct1008_port);
	if (ret < 0) {
		pr_info("%s: calling gpio_free(nct1008_port)", __func__);
		gpio_free(nct1008_port);
	}

	/* dalmore has thermal sensor on GEN1-I2C i.e. instance 0 */
	i2c_register_board_info(0, dalmore_i2c4_nct1008_board_info,
		ARRAY_SIZE(dalmore_i2c4_nct1008_board_info));

	return ret;
}

static struct i2c_board_info __initdata bq20z45_pdata[] = {
	{
		I2C_BOARD_INFO("sbs-battery", 0x0B),
		.platform_data = &sbs_pdata,
	},
};

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

static int __init dalmore_skin_init(void)
{
	if (machine_is_dalmore()) {
		balanced_throttle_register(&skin_throttle, "skin-balanced");
		tegra_skin_therm_est_device.dev.platform_data = &skin_data;
		platform_device_register(&tegra_skin_therm_est_device);
	}

	return 0;
}
late_initcall(dalmore_skin_init);
#endif

int __init dalmore_sensors_init(void)
{
	int err;

	tegra_get_board_info(&board_info);

	err = dalmore_nct1008_init();
	if (err)
		return err;

	dalmore_camera_init();
	mpuirq_init();

	i2c_register_board_info(0, dalmore_i2c_board_info_cm3218,
		ARRAY_SIZE(dalmore_i2c_board_info_cm3218));

	i2c_register_board_info(0, bq20z45_pdata,
		ARRAY_SIZE(bq20z45_pdata));

	return 0;
}

static int dalmore_chk_conflict(struct device *dev, void *addrp)
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

int camera_auto_detect(void)
{
	struct i2c_adapter *adap = i2c_get_adapter(2);

	pr_info("%s ++ %04x - %04x - %04x\n", __func__,
		ad5816_devid, dw9718_devid, max77387_devid);

	if ((ad5816_devid & 0xff00) == 0x2400) {
		if ((max77387_devid & 0xff) == 0x91) {
			/* IMX135 found */
			i2c_new_device(adap, &dalmore_i2c_board_info_imx135);
			i2c_new_device(adap, &dalmore_i2c_board_info_imx132);
		} else {
			/* remove max77387 from system */
			device_for_each_child(&adap->dev,
				&dalmore_i2c_board_info_e1625[2].addr,
				dalmore_chk_conflict);
			/* IMX091 found*/
			i2c_new_device(adap, &dalmore_i2c_board_info_imx091);
			i2c_new_device(adap, &dalmore_i2c_board_info_ov9772);
			i2c_new_device(adap, &dalmore_i2c_board_info_as3648);
		}
	} else {
		/* remove ad5816 from system */
		device_for_each_child(&adap->dev,
			&dalmore_i2c_board_info_e1625[0].addr,
			dalmore_chk_conflict);
		/* remove max77387 from system */
		device_for_each_child(&adap->dev,
			&dalmore_i2c_board_info_e1625[2].addr,
			dalmore_chk_conflict);
		if (dw9718_devid) {
			/* AR0833 found */
			i2c_new_device(adap, &dalmore_i2c_board_info_ar0833);
			i2c_new_device(adap, &dalmore_i2c_board_info_as3648);
			i2c_new_device(adap, &dalmore_i2c_board_info_ov9772);
		} else { /* default using ov5693 + ad5823 */
			device_for_each_child(&adap->dev,
				&dalmore_i2c_board_info_e1625[1].addr,
				dalmore_chk_conflict);
			i2c_new_device(adap, &dalmore_i2c_board_info_ov5693);
			i2c_new_device(adap, &dalmore_i2c_board_info_ad5823);
			i2c_new_device(adap, &dalmore_i2c_board_info_as3648);
		}
	}

	pr_info("%s --\n", __func__);
	return 0;
}

int __init dalmore_camera_late_init(void)
{
	if (board_info.board_id != BOARD_E1611) {
		pr_err("%s: Dalmore not found!\n", __func__);
		return 0;
	}

	camera_auto_detect();

	return 0;
}

late_initcall(dalmore_camera_late_init);
