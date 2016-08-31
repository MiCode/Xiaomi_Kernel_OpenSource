/*
 * imx091.c - imx091 sensor driver
 *
 * Copyright (c) 2012-2014, NVIDIA Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/list.h>
#include <media/imx091.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/tegra-soc.h>

#include "nvc_utilities.h"

#ifdef CONFIG_DEBUG_FS
#include <media/nvc_debugfs.h>
#endif

#undef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))


#define IMX091_ID			0x0091
#define IMX091_ID_ADDRESS   0x0000
#define IMX091_STREAM_CONTROL_REG 0x0100
#define IMX091_STREAM_ENABLE 0x01
#define IMX091_STREAM_DISABLE 0x00
#define IMX091_SENSOR_TYPE		NVC_IMAGER_TYPE_RAW
#define IMX091_STARTUP_DELAY_MS		50
#define IMX091_RES_CHG_WAIT_TIME_MS	100
#define IMX091_SIZEOF_I2C_BUF		16
#define IMX091_TABLE_WAIT_MS		0
#define IMX091_TABLE_END		1
#define IMX091_NUM_MODES_MAX	MAX(ARRAY_SIZE(imx091_mode_table_non_fpga)\
					, ARRAY_SIZE(imx091_mode_table_fpga))
/*#define IMX091_MODE_UNKNOWN		(IMX091_NUM_MODES + 1)*/
#define IMX091_LENS_MAX_APERTURE	0 /* / _INT2FLOAT_DIVISOR */
#define IMX091_LENS_FNUMBER		0 /* / _INT2FLOAT_DIVISOR */
#define IMX091_LENS_FOCAL_LENGTH	4760 /* / _INT2FLOAT_DIVISOR */
#define IMX091_LENS_VIEW_ANGLE_H	60400 /* / _INT2FLOAT_DIVISOR */
#define IMX091_LENS_VIEW_ANGLE_V	60400 /* / _INT2FLOAT_DIVISOR */
#define IMX091_WAIT_MS 3
#define IMX091_I2C_TABLE_MAX_ENTRIES	400
#define IMX091_FUSE_ID_SIZE		8

static u16 imx091_ids[] = {
	0x0091,
};

static struct nvc_gpio_init imx091_gpios[] = {
	{IMX091_GPIO_RESET, GPIOF_OUT_INIT_LOW, "reset", false, true},
	{IMX091_GPIO_PWDN, GPIOF_OUT_INIT_LOW, "pwdn", false, true},
	{IMX091_GPIO_GP1, 0, "gp1", false, false},
};

static struct nvc_regulator_init imx091_vregs[] = {
	{ IMX091_VREG_DVDD, "vdig", },
	{ IMX091_VREG_AVDD, "vana", },
	{ IMX091_VREG_IOVDD, "vif", },
};

struct imx091_info {
	u16 dev_id;
	atomic_t in_use;
	struct i2c_client *i2c_client;
	struct imx091_platform_data *pdata;
	struct nvc_imager_cap *cap;
	struct miscdevice miscdev;
	struct list_head list;
	struct clk *mclk;
	struct nvc_gpio gpio[ARRAY_SIZE(imx091_gpios)];
	struct nvc_regulator vreg[ARRAY_SIZE(imx091_vregs)];
	int pwr_api;
	int pwr_dev;
	u8 s_mode;
	struct imx091_info *s_info;
	u32 mode_index;
	bool mode_valid;
	bool mode_enable;
	bool reset_flag;
	unsigned test_pattern;
	struct nvc_imager_static_nvc sdata;
	struct regulator *imx091_ext_reg_vcm_vdd;
	struct regulator *imx091_ext_reg_i2c_vdd;
	u8 i2c_buf[IMX091_SIZEOF_I2C_BUF];
	u8 bin_en;
#ifdef CONFIG_DEBUG_FS
	struct nvc_debugfs_info debugfs_info;
#endif
	struct nvc_fuseid fuse_id;
	char devname[16];
};

struct imx091_reg {
	u16 addr;
	u16 val;
};

struct imx091_mode_data {
	struct nvc_imager_mode sensor_mode;
	struct nvc_imager_dynamic_nvc sensor_dnvc;
	struct imx091_reg *p_mode_i2c;
};

static struct nvc_imager_cap imx091_dflt_cap = {
	.identifier		= "IMX091",
	.sensor_nvc_interface	= 3,
	.pixel_types[0]		= 0x100,
	.orientation		= 0,
	.direction		= 0,
	.initial_clock_rate_khz	= 6000,
	.clock_profiles[0] = {
		.external_clock_khz	= 24000,
		.clock_multiplier	= 8500000, /* value / 1,000,000 */
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
	.preferred_mode_index	= 1,
	.focuser_guid	= NVC_FOCUS_GUID(0),
	.torch_guid		= NVC_TORCH_GUID(0),
	.cap_version		= NVC_IMAGER_CAPABILITIES_VERSION2,
};

static struct imx091_platform_data imx091_dflt_pdata = {
	.cfg			= 0,
	.num			= 0,
	.sync			= 0,
	.dev_name		= "camera",
	.cap			= &imx091_dflt_cap,
};

	/* NOTE: static vs dynamic
	 * If a member in the nvc_imager_static_nvc structure is not actually
	 * static data, then leave blank and add the parameter to the parameter
	 * read function that dynamically reads the data.  The NVC user driver
	 * will call the parameter read for the data if the member data is 0.
	 * If the dynamic data becomes static during probe (a one time read
	 * such as device ID) then add the dynamic read to the _sdata_init
	 * function.
	 */
static struct nvc_imager_static_nvc imx091_dflt_sdata = {
	.api_version		= NVC_IMAGER_API_STATIC_VER,
	.sensor_type		= IMX091_SENSOR_TYPE,
	.bits_per_pixel		= 10,
	.sensor_id		= IMX091_ID,
	.sensor_id_minor	= 0,
	.focal_len		= IMX091_LENS_FOCAL_LENGTH,
	.max_aperture		= IMX091_LENS_MAX_APERTURE,
	.fnumber		= IMX091_LENS_FNUMBER,
	.view_angle_h		= IMX091_LENS_VIEW_ANGLE_H,
	.view_angle_v		= IMX091_LENS_VIEW_ANGLE_V,
	.res_chg_wait_time	= IMX091_RES_CHG_WAIT_TIME_MS,
};

static LIST_HEAD(imx091_info_list);
static DEFINE_SPINLOCK(imx091_spinlock);


static struct imx091_reg tp_none_seq[] = {
	{IMX091_TABLE_END, 0x0000}
};

static struct imx091_reg tp_cbars_seq[] = {
	{IMX091_TABLE_END, 0x0000}
};

static struct imx091_reg tp_checker_seq[] = {
	{IMX091_TABLE_END, 0x0000}
};

static struct imx091_reg *test_patterns[] = {
	tp_none_seq,
	tp_cbars_seq,
	tp_checker_seq,
};

static int imx091_power_on(struct nvc_regulator *vreg);
static int imx091_power_off(struct nvc_regulator *vreg);

#define IMX091_WAIT_1000_MS 1000
/* The setting is for 1-lane mode.
 * THS time is different as well. They are set
 * in the VI programming layer
 */
static struct imx091_reg imx091_FPGA_4160x3120_i2c[] = {
	{0x0103, 0x01},
	{IMX091_TABLE_WAIT_MS, IMX091_WAIT_MS},

	{0x3087, 0x53},
	{0x309D, 0x94},
	{0x30A1, 0x08},
	{0x30C7, 0x00},
	{0x3115, 0x0E},
	{0x3118, 0x42},
	{0x311D, 0x34},
	{0x3121, 0x0D},
	{0x3212, 0xF2},
	{0x3213, 0x0F},
	{0x3215, 0x0F},
	{0x3217, 0x0B},
	{0x3219, 0x0B},
	{0x321B, 0x0D},
	{0x321D, 0x0D},
	{0x0305, 0x01},
	{0x0307, 0x18},
	{0x30A4, 0x01},
	{0x303C, 0x28},
	{0x3032, 0x00},
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0340, 0x0C},
	{0x0341, 0x58},
	{0x0342, 0x12},
	{0x0343, 0x0C},
	{0x0344, 0x00},
	{0x0345, 0x08},
	{0x0346, 0x00},
	{0x0347, 0x30},
	{0x0348, 0x10},
	{0x0349, 0x77},
	{0x034A, 0x0C},
	{0x034B, 0x5F},
	{0x034C, 0x10},
	{0x034D, 0x40},
	{0x034E, 0x0C},
	{0x034F, 0x30},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x3033, 0x00},
	{0x303D, 0x10},
	{0x303E, 0xD0},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x00},
	{0x304C, 0x7F},
	{0x304D, 0x04},
	{0x3064, 0x12},
	{0x309B, 0x20},
	{0x309E, 0x00},
	{0x30D5, 0x00},
	{0x30D6, 0x85},
	{0x30D7, 0x2A},
	{0x30D8, 0x64},
	{0x30D9, 0x89},
	{0x30DA, 0x00},
	{0x30DB, 0x00},
	{0x30DC, 0x00},
	{0x30DD, 0x00},
	{0x30DE, 0x00},
	{0x3102, 0x10},
	{0x3103, 0x44},
	{0x3104, 0x40},
	{0x3105, 0x00},
	{0x3106, 0x0D},
	{0x3107, 0x01},
	{0x310A, 0x0A},
	{0x315C, 0x99},
	{0x315D, 0x98},
	{0x316E, 0x9A},
	{0x316F, 0x99},
	{0x3301, 0x01},
	{0x3304, 0x05},
	{0x3305, 0x04},
	{0x3306, 0x12},
	{0x3307, 0x03},
	{0x3308, 0x0D},
	{0x3309, 0x05},
	{0x330A, 0x09},
	{0x330B, 0x04},
	{0x330C, 0x08},
	{0x330D, 0x05},
	{0x330E, 0x03},
	{0x3318, 0x65},
	{0x3322, 0x02},
	{0x3342, 0x0F},
	{0x3348, 0xE0},
	{0x0600, 0x00},
	{0x0601, 0x00},

	{IMX091_TABLE_WAIT_MS, 5000},
	{IMX091_TABLE_END, 0x00}
};

static struct imx091_reg imx091_FPGA_1052x1560_i2c[] = {
	/* Stand by */
	{0x0100, 0x00},
	{IMX091_TABLE_WAIT_MS, IMX091_WAIT_MS},

	/* global settings */
	{0x3087, 0x53},
	{0x309D, 0x94},
	{0x30A1, 0x08},
	{0x30C7, 0x00},
	{0x3115, 0x0E},
	{0x3118, 0x42},
	{0x311D, 0x34},
	{0x3121, 0x0D},
	{0x3212, 0xF2},
	{0x3213, 0x0F},
	{0x3215, 0x0F},
	{0x3217, 0x0B},
	{0x3219, 0x0B},
	{0x321B, 0x0D},
	{0x321D, 0x0D},

	/* black level */
	{0x3032, 0x00},

	/*
	 * PLL (MCLK) = 13MHz
	 * Previously, this was calibrated for a 24MHz system, which has
	 * PREPLLCLK_DIV of 2 (1/2), RGPLTD of 2 (1/1), and PLL_MPY of 47.
	 * In the standard setting, this is set to 564MHz.
	 * In the 13MHz (FPGA) setting, set for a PREPLLCLK_DIV of 1 (1/1),
	 * a PLL_MPY of 43, and a RGPLTD of 2 (1/1), for an output PLL frequency
	 * of 559MHz.
	 */
	{0x0305, 0x01}, /* PREPLLCLK_DIV */
	{0x0307, 0x18}, /* PLL_MPY */
	{0x30A4, 0x01}, /* RGPLTD */
	{0x303C, 0x28}, /* PLSTATIM */

	/* Mode Settings */
	{0x0112, 0x0A}, /* RAW 10 */
	{0x0113, 0x0A},
	{0x0340, 0x07}, /* FrameLength 0x700 = 1792 */
	{0x0341, 0x00},
	{0x0342, 0x12}, /* LineLength = 0x120C = 4620 */
	{0x0343, 0x0C},

	/* Imaging Area Determination */
	{0x0344, 0x00}, /* ReadOut start horizontal position = 8 */
	{0x0345, 0x08},
	{0x0346, 0x00}, /* ReadOut start vertical position = 0x30 = 48 */
	{0x0347, 0x30},
	{0x0348, 0x10}, /* ReadOut end horizontal position = 0x1077 */
	{0x0349, 0x77},
	{0x034A, 0x0C}, /* ReadOut end vertical position = 0xc5f */
	{0x034B, 0x5F},
	{0x034C, 0x04}, /* ReadOut size horizontal position = 0x041C = 1052 */
	{0x034D, 0x1C},
	{0x034E, 0x06}, /* ReadOut size vertical position = 0x0618 = 1560 */
	{0x034F, 0x18},
	{0x0381, 0x05},
	{0x0383, 0x03},
	{0x0385, 0x01},
	{0x0387, 0x03},
	{0x3033, 0x00}, /* HD mode off */
	{0x303D, 0x10}, /* standby end immediate */
	{0x303E, 0xD0},
	{0x3040, 0x08}, /* OPB start and end address vert */
	{0x3041, 0x97},
	{0x3048, 0x01}, /* vertical addition enabled */
	{0x304C, 0x7F}, /* readout pixels set to 0x04_78 */
	{0x304D, 0x04},
	{0x3064, 0x12},
	{0x309B, 0x28},
	{0x309E, 0x00},
	{0x30D5, 0x09},
	{0x30D6, 0x00},
	{0x30D7, 0x00},
	{0x30D8, 0x00},
	{0x30D9, 0x00},
	{0x30DA, 0x00},
	{0x30DB, 0x00},
	{0x30DC, 0x00},
	{0x30DD, 0x00},
	{0x30DE, 0x04},
	{0x3102, 0x10},
	{0x3103, 0x44},
	{0x3104, 0x40},
	{0x3105, 0x00},
	{0x3106, 0x0D},
	{0x3107, 0x01},
	{0x310A, 0x0A},
	{0x315C, 0x99},
	{0x315D, 0x98},
	{0x316E, 0x9A},
	{0x316F, 0x99},
	{0x3301, 0x03}, /* 4 Lane */
	{0x3304, 0x05},
	{0x3305, 0x04},
	{0x3306, 0x12},
	{0x3307, 0x03},
	{0x3308, 0x0D},
	{0x3309, 0x05},
	{0x330A, 0x09},
	{0x330B, 0x04},
	{0x330C, 0x08},
	{0x330D, 0x05},
	{0x330E, 0x03},
	{0x3318, 0x73},
	{0x3322, 0x02},
	{0x3342, 0x0F},
	{0x3348, 0xE0},
	{0x0600, 0x00}, /* colorbar fade-to-gray */
	{0x0601, 0x00},
	{0x0101, 0x00}, /* image orientation */
	{0x0100, 0x01},

	{IMX091_TABLE_WAIT_MS, IMX091_WAIT_MS},
	{IMX091_TABLE_END, 0x00}
};

static struct imx091_reg imx091_4208x3120_i2c[] = {
	/* Reset */
	{0x0103, 0x01},
	{IMX091_TABLE_WAIT_MS, IMX091_WAIT_MS},

	/* global settings */
	{0x3087, 0x53},
	{0x309D, 0x94},
	{0x30A1, 0x08},
	{0x30C7, 0x00},
	{0x3115, 0x0E},
	{0x3118, 0x42},
	{0x311D, 0x34},
	{0x3121, 0x0D},
	{0x3212, 0xF2},
	{0x3213, 0x0F},
	{0x3215, 0x0F},
	{0x3217, 0x0B},
	{0x3219, 0x0B},
	{0x321B, 0x0D},
	{0x321D, 0x0D},

	/* black level setting */
	{0x3032, 0x40},

	/* PLL */
	{0x0305, 0x02},
	{0x0307, 0x2A}, /* PLL Multipiler = 42 */
	{0x30A4, 0x02},
	{0x303C, 0x4B},

	/* Mode Settings */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0340, 0x0C},
	{0x0341, 0x58},
	{0x0342, 0x12},
	{0x0343, 0x0C},
	{0x0344, 0x00},
	{0x0345, 0x08},
	{0x0346, 0x00},
	{0x0347, 0x30},
	{0x0348, 0x10},
	{0x0349, 0x77},
	{0x034A, 0x0C},
	{0x034B, 0x5F},
	{0x034C, 0x10},
	{0x034D, 0x70},
	{0x034E, 0x0C},
	{0x034F, 0x30},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x01},
	{0x3033, 0x00},
	{0x303D, 0x10},
	{0x303E, 0xD0},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x00},
	{0x304C, 0x7F},
	{0x304D, 0x04},
	{0x3064, 0x12},
	{0x309B, 0x20},
	{0x309E, 0x00},
	{0x30D5, 0x00},
	{0x30D6, 0x85},
	{0x30D7, 0x2A},
	{0x30D8, 0x64},
	{0x30D9, 0x89},
	{0x30DE, 0x00},
	{0x3102, 0x10},
	{0x3103, 0x44},
	{0x3104, 0x40},
	{0x3105, 0x00},
	{0x3106, 0x0D},
	{0x3107, 0x01},
	{0x310A, 0x0A},
	{0x315C, 0x99},
	{0x315D, 0x98},
	{0x316E, 0x9A},
	{0x316F, 0x99},
	{0x3301, 0x03},
	{0x3304, 0x05},
	{0x3305, 0x04},
	{0x3306, 0x12},
	{0x3307, 0x03},
	{0x3308, 0x0D},
	{0x3309, 0x05},
	{0x330A, 0x09},
	{0x330B, 0x04},
	{0x330C, 0x08},
	{0x330D, 0x05},
	{0x330E, 0x03},
	{0x3318, 0x65},
	{0x3322, 0x02},
	{0x3342, 0x0F},
	{0x3348, 0xE0},

	{0x0202, 0x0B},
	{0x0203, 0xB8},
	{0x0205, 0x00},

	{IMX091_TABLE_END, 0x00}
};

static struct imx091_reg imx091_1948x1096_i2c[] = {
	/* Reset */
	{0x0103, 0x01},
	{IMX091_TABLE_WAIT_MS, IMX091_WAIT_MS},

	/* global settings */
	{0x3087, 0x53},
	{0x309D, 0x94},
	{0x30A1, 0x08},
	{0x30C7, 0x00},
	{0x3115, 0x0E},
	{0x3118, 0x42},
	{0x311D, 0x34},
	{0x3121, 0x0D},
	{0x3212, 0xF2},
	{0x3213, 0x0F},
	{0x3215, 0x0F},
	{0x3217, 0x0B},
	{0x3219, 0x0B},
	{0x321B, 0x0D},
	{0x321D, 0x0D},

	/* black level setting */
	{0x3032, 0x40},

	/* PLL */
	{0x0305, 0x02},
	{0x0307, 0x20},
	{0x30A4, 0x02},
	{0x303C, 0x4B},

	/* Mode Settings */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0340, 0x08},
	{0x0341, 0xA6},
	{0x0342, 0x09},
	{0x0343, 0x06},
	{0x0344, 0x00},
	{0x0345, 0xA4},
	{0x0346, 0x02},
	{0x0347, 0x00},
	{0x0348, 0x0F},
	{0x0349, 0xDB},
	{0x034A, 0x0A},
	{0x034B, 0x8F},
	{0x034C, 0x07},
	{0x034D, 0x9C},
	{0x034E, 0x04},
	{0x034F, 0x48},
	{0x0381, 0x01},
	{0x0383, 0x01},
	{0x0385, 0x01},
	{0x0387, 0x03},
	{0x3033, 0x84},
	{0x303D, 0x10},
	{0x303E, 0xD0},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x01},
	{0x304C, 0x3F},
	{0x304D, 0x02},
	{0x3064, 0x12},
	{0x309B, 0x48},
	{0x309E, 0x04},
	{0x30D5, 0x04},
	{0x30D6, 0x85},
	{0x30D7, 0x2A},
	{0x30D8, 0x64},
	{0x30D9, 0x89},
	{0x30DE, 0x00},
	{0x3102, 0x09},
	{0x3103, 0x23},
	{0x3104, 0x24},
	{0x3105, 0x00},
	{0x3106, 0x8B},
	{0x3107, 0x00},
	{0x310A, 0x0A},
	{0x315C, 0x4A},
	{0x315D, 0x49},
	{0x316E, 0x4B},
	{0x316F, 0x4A},
	{0x3301, 0x03},
	{0x3304, 0x05},
	{0x3305, 0x04},
	{0x3306, 0x12},
	{0x3307, 0x03},
	{0x3308, 0x0D},
	{0x3309, 0x05},
	{0x330A, 0x09},
	{0x330B, 0x04},
	{0x330C, 0x08},
	{0x330D, 0x05},
	{0x330E, 0x03},
	{0x3318, 0x67},
	{0x3322, 0x02},
	{0x3342, 0x0F},
	{0x3348, 0xE0},

	{0x0202, 0x00},
	{0x0203, 0x00},
	{0x0205, 0x00},

	{IMX091_TABLE_END, 0x00}
};

static struct imx091_reg imx091_1308x736_i2c[] = {
	/* Reset */
	{0x0103, 0x01},
	{IMX091_TABLE_WAIT_MS, IMX091_WAIT_MS},

	/* global settings */
	{0x3087, 0x53},
	{0x309D, 0x94},
	{0x30A1, 0x08},
	{0x30C7, 0x00},
	{0x3115, 0x0E},
	{0x3118, 0x42},
	{0x311D, 0x34},
	{0x3121, 0x0D},
	{0x3212, 0xF2},
	{0x3213, 0x0F},
	{0x3215, 0x0F},
	{0x3217, 0x0B},
	{0x3219, 0x0B},
	{0x321B, 0x0D},
	{0x321D, 0x0D},

	/* black level setting */
	{0x3032, 0x40},

	/* PLL */
	{0x0305, 0x02},
	{0x0307, 0x20},
	{0x30A4, 0x02},
	{0x303C, 0x4B},

	/* Mode Settings */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0340, 0x04},
	{0x0341, 0x4E},
	{0x0342, 0x12},
	{0x0343, 0x0C},
	{0x0344, 0x00},
	{0x0345, 0x96},
	{0x0346, 0x01},
	{0x0347, 0xF8},
	{0x0348, 0x0F},
	{0x0349, 0xE9},
	{0x034A, 0x0A},
	{0x034B, 0x97},
	{0x034C, 0x05},
	{0x034D, 0x1C},
	{0x034E, 0x02},
	{0x034F, 0xE0},
	{0x0381, 0x03},
	{0x0383, 0x03},
	{0x0385, 0x03},
	{0x0387, 0x03},
	{0x3033, 0x00},
	{0x303D, 0x10},
	{0x303E, 0xD0},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x22},
	{0x304C, 0x7F},
	{0x304D, 0x04},
	{0x3064, 0x12},
	{0x309B, 0x60},
	{0x309E, 0x04},
	{0x30D5, 0x09},
	{0x30D6, 0x00},
	{0x30D7, 0x00},
	{0x30D8, 0x00},
	{0x30D9, 0x89},
	{0x30DE, 0x03},
	{0x3102, 0x09},
	{0x3103, 0x23},
	{0x3104, 0x24},
	{0x3105, 0x00},
	{0x3106, 0x8B},
	{0x3107, 0x00},
	{0x310A, 0x0A},
	{0x315C, 0x4A},
	{0x315D, 0x49},
	{0x316E, 0x4B},
	{0x316F, 0x4A},
	{0x3301, 0x03},
	{0x3304, 0x05},
	{0x3305, 0x04},
	{0x3306, 0x12},
	{0x3307, 0x03},
	{0x3308, 0x0D},
	{0x3309, 0x05},
	{0x330A, 0x09},
	{0x330B, 0x04},
	{0x330C, 0x08},
	{0x330D, 0x05},
	{0x330E, 0x03},
	{0x3318, 0x6C},
	{0x3322, 0x02},
	{0x3342, 0x0F},
	{0x3348, 0xE0},

	{0x0202, 0x00},
	{0x0203, 0x00},
	{0x0205, 0x00},

	{IMX091_TABLE_END, 0x00}
};

static struct imx091_reg imx091_2104x1560_i2c[] = {
	/* Software reset */
	{0x0103, 0x01},
	{IMX091_TABLE_WAIT_MS, IMX091_WAIT_MS},

	/* global settings */
	{0x3087, 0x53},
	{0x309D, 0x94},
	{0x30A1, 0x08},
	{0x30C7, 0x00},
	{0x3115, 0x0E},
	{0x3118, 0x42},
	{0x311D, 0x34},
	{0x3121, 0x0D},
	{0x3212, 0xF2},
	{0x3213, 0x0F},
	{0x3215, 0x0F},
	{0x3217, 0x0B},
	{0x3219, 0x0B},
	{0x321B, 0x0D},
	{0x321D, 0x0D},

	/* black level setting */
	{0x3032, 0x40},

	/* PLL */
	{0x0305, 0x02},
	{0x0307, 0x2F},
	{0x30A4, 0x02},
	{0x303C, 0x4B},

	/* Mode Settings */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0340, 0x06},
	{0x0341, 0x58},
	{0x0342, 0x12},
	{0x0343, 0x0C},
	{0x0344, 0x00},
	{0x0345, 0x08},
	{0x0346, 0x00},
	{0x0347, 0x30},
	{0x0348, 0x10},
	{0x0349, 0x77},

	{0x034A, 0x0C},
	{0x034B, 0x5F},
	{0x034C, 0x08},
	{0x034D, 0x38},
	{0x034E, 0x06},
	{0x034F, 0x18},
	{0x0381, 0x01},
	{0x0383, 0x03},
	{0x0385, 0x01},
	{0x0387, 0x03},
	{0x3033, 0x00},
	{0x303D, 0x10},
	{0x303E, 0xD0},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x01},
	{0x304C, 0x7F},
	{0x304D, 0x04},
	{0x3064, 0x12},
	{0x309B, 0x28},
	{0x309E, 0x00},
	{0x30D5, 0x09},
	{0x30D6, 0x01},
	{0x30D7, 0x01},
	{0x30D8, 0x64},
	{0x30D9, 0x89},
	{0x30DE, 0x02},
	{0x3102, 0x10},
	{0x3103, 0x44},
	{0x3104, 0x40},
	{0x3105, 0x00},
	{0x3106, 0x0D},
	{0x3107, 0x01},
	{0x310A, 0x0A},
	{0x315C, 0x99},
	{0x315D, 0x98},
	{0x316E, 0x9A},
	{0x316F, 0x99},
	{0x3301, 0x03},
	{0x3304, 0x05},
	{0x3305, 0x04},
	{0x3306, 0x12},
	{0x3307, 0x03},
	{0x3308, 0x0D},
	{0x3309, 0x05},
	{0x330A, 0x09},
	{0x330B, 0x04},
	{0x330C, 0x08},
	{0x330D, 0x05},
	{0x330E, 0x03},
	{0x3318, 0x73},
	{0x3322, 0x02},
	{0x3342, 0x0F},
	{0x3348, 0xE0},

	{0x0202, 0x06},
	{0x0203, 0x00},
	{0x0205, 0x00},

	{IMX091_TABLE_WAIT_MS, IMX091_WAIT_MS},
	{IMX091_TABLE_END, 0x00}
};

static struct imx091_reg imx091_524X390_i2c[] = {
	/* Reset */
	{0x0103, 0x01},
	{IMX091_TABLE_WAIT_MS, IMX091_WAIT_MS},

	/* global settings */
	{0x3087, 0x53},
	{0x309D, 0x94},
	{0x30A1, 0x08},
	{0x30C7, 0x00},
	{0x3115, 0x0E},
	{0x3118, 0x42},
	{0x311D, 0x34},
	{0x3121, 0x0D},
	{0x3212, 0xF2},
	{0x3213, 0x0F},
	{0x3215, 0x0F},
	{0x3217, 0x0B},
	{0x3219, 0x0B},
	{0x321B, 0x0D},
	{0x321D, 0x0D},

	/* black level setting */
	{0x3032, 0x40},

	/* PLL */
	{0x0305, 0x02},
	{0x0307, 0x2F},
	{0x30A4, 0x02},
	{0x303C, 0x4B},

	/* Mode Settings */
	{0x0112, 0x0A},
	{0x0113, 0x0A},
	{0x0340, 0x01},
	{0x0341, 0x96},
	{0x0342, 0x12},
	{0x0343, 0x0C},
	{0x0344, 0x00},
	{0x0345, 0x10},
	{0x0346, 0x00},
	{0x0347, 0x30},
	{0x0348, 0x10},
	{0x0349, 0x6F},
	{0x034A, 0x0C},
	{0x034B, 0x5F},
	{0x034C, 0x02},
	{0x034D, 0x0C},
	{0x034E, 0x01},
	{0x034F, 0x86},
	{0x0381, 0x09},
	{0x0383, 0x07},
	{0x0385, 0x09},
	{0x0387, 0x07},
	{0x3033, 0x00},
	{0x303D, 0x10},
	{0x303E, 0xD0},
	{0x3040, 0x08},
	{0x3041, 0x97},
	{0x3048, 0x01},
	{0x304C, 0x7F},
	{0x304D, 0x04},
	{0x3064, 0x12},
	{0x309B, 0x28},
	{0x309E, 0x00},
	{0x30D5, 0x09},
	{0x30D6, 0x00},
	{0x30D7, 0x00},
	{0x30D8, 0x00},
	{0x30D9, 0x00},
	{0x30DE, 0x08},
	{0x3102, 0x10},
	{0x3103, 0x44},
	{0x3104, 0x40},
	{0x3105, 0x00},
	{0x3106, 0x0D},
	{0x3107, 0x01},
	{0x310A, 0x0A},
	{0x315C, 0x99},
	{0x315D, 0x98},
	{0x316E, 0x9A},
	{0x316F, 0x99},
	{0x3301, 0x03},
	{0x3304, 0x03},
	{0x3305, 0x02},
	{0x3306, 0x09},
	{0x3307, 0x06},
	{0x3308, 0x1E},
	{0x3309, 0x05},
	{0x330A, 0x05},
	{0x330B, 0x04},
	{0x330C, 0x07},
	{0x330D, 0x06},
	{0x330E, 0x01},
	{0x3318, 0x44},
	{0x3322, 0x0E},
	{0x3342, 0x00},
	{0x3348, 0xE0},

	{0x0202, 0x00},
	{0x0203, 0x00},
	{0x0205, 0x00},

	{IMX091_TABLE_END, 0x00}
};

/* Each resolution requires the below data table setup and the corresponding
 * I2C data table.
 * If more NVC data is needed for the NVC driver, be sure and modify the
 * nvc_imager_nvc structure in nvc_imager.h
 * If more data sets are needed per resolution, they can be added to the
 * table format below with the imx091_mode_data structure.  New data sets
 * should conform to an already defined NVC structure.  If it's data for the
 * NVC driver, then it should be added to the nvc_imager_nvc structure.
 * Steps to add a resolution:
 * 1. Add I2C data table
 * 2. Add imx091_mode_data table
 * 3. Add entry to the imx091_mode_table_non_fpga and/or imx091_mode_table_fpga
 */
static struct imx091_mode_data imx091_FPGA_4160x3120 = {
	.sensor_mode = {
		.res_x			= 4160,
		.res_y			= 3120,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000,  /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 5000,  /* / _INT2FLOAT_DIVISOR */
		.crop_mode		= NVC_IMAGER_CROPMODE_NONE,
	},
	.sensor_dnvc = {
		.api_version		= NVC_IMAGER_API_DYNAMIC_VER,
		.region_start_x		= 0,
		.region_start_y		= 0,
		.x_scale		= 1,
		.y_scale		= 1,
		.bracket_caps		= 1,
		.flush_count		= 2,
		.init_intra_frame_skip	= 0,
		.ss_intra_frame_skip	= 2,
		.ss_frame_number	= 3,
		.coarse_time		= 0x06FB,
		.max_coarse_diff	= 5,
		.min_exposure_course	= 2,
		.max_exposure_course	= 0xFFFC,
		.diff_integration_time	= 110, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x120C,
		.frame_length		= 0x0c58,
		.min_frame_length	= 0x0c58,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 16000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 0x20,
		.pll_div		= 0x2,
	},
	.p_mode_i2c			= imx091_FPGA_4160x3120_i2c,
};

static struct imx091_mode_data imx091_FPGA_1052x1560 = {
	.sensor_mode = {
		.res_x			= 1032,
		.res_y			= 1540,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000,  /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 5000,  /* / _INT2FLOAT_DIVISOR */
		.crop_mode		= NVC_IMAGER_CROPMODE_NONE,
	},
	.sensor_dnvc = {
		.api_version		= NVC_IMAGER_API_DYNAMIC_VER,
		.region_start_x		= 0,
		.region_start_y		= 0,
		.x_scale		= 1,
		.y_scale		= 1,
		.bracket_caps		= 1,
		.flush_count		= 2,
		.init_intra_frame_skip	= 0,
		.ss_intra_frame_skip	= 2,
		.ss_frame_number	= 3,
		.coarse_time		= 0x06FB,
		.max_coarse_diff	= 5,
		.min_exposure_course	= 2,
		.max_exposure_course	= 0xFFFC,
		.diff_integration_time	= 110, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x120C,
		.frame_length		= 0x0700,
		.min_frame_length	= 0x0700,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 16000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 0x20,
		.pll_div		= 0x2,
	},
	.p_mode_i2c			= imx091_FPGA_1052x1560_i2c,
};

static struct imx091_mode_data imx091_4208x3120 = {
	.sensor_mode = {
		.res_x			= 4096,
		.res_y			= 3072,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 15000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 10000, /* / _INT2FLOAT_DIVISOR */
		.crop_mode		= NVC_IMAGER_CROPMODE_NONE,
	},
	.sensor_dnvc = {
		.api_version		= NVC_IMAGER_API_DYNAMIC_VER,
		.region_start_x		= 0,
		.region_start_y		= 0,
		.x_scale		= 1,
		.y_scale		= 1,
		.bracket_caps		= 1,
		.flush_count		= 2,
		.init_intra_frame_skip	= 0,
		.ss_intra_frame_skip	= 2,
		.ss_frame_number	= 3,
		.coarse_time		= 0x0C53,
		.max_coarse_diff	= 5,
		.min_exposure_course	= 2,
		.max_exposure_course	= 0xFFFC,
		.diff_integration_time	= 110, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x120C,
		.frame_length		= 0x0C58,
		.min_frame_length	= 0x0C58,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 16000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 0x2A,
		.pll_div		= 0x2,
	},
	.p_mode_i2c			= imx091_4208x3120_i2c,
};

static struct imx091_mode_data imx091_1948x1096 = {
	.sensor_mode = {
		.res_x			= 1920,
		.res_y			= 1080,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 7000, /* / _INT2FLOAT_DIVISOR */
		.crop_mode		= NVC_IMAGER_CROPMODE_PARTIAL,
	},
	.sensor_dnvc = {
		.api_version		= NVC_IMAGER_API_DYNAMIC_VER,
		.region_start_x		= 0,
		.region_start_y		= 0,
		.x_scale		= 1,
		.y_scale		= 1,
		.bracket_caps		= 1,
		.flush_count		= 2,
		.init_intra_frame_skip	= 0,
		.ss_intra_frame_skip	= 2,
		.ss_frame_number	= 3,
	.coarse_time		= 0x08A1,
	.max_coarse_diff	= 5,
		.min_exposure_course	= 2,
		.max_exposure_course	= 0xFFFC,
		.diff_integration_time	= 110, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x0906,
		.frame_length		= 0x08A6,
		.min_frame_length	= 0x08A6,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 16000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 0x20,
		.pll_div		= 0x2,
	},
	.p_mode_i2c			= imx091_1948x1096_i2c,
};

static struct imx091_mode_data imx091_1308x736 = {
	.sensor_mode = {
		.res_x			= 1280,
		.res_y			= 720,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 5000, /* / _INT2FLOAT_DIVISOR */
		.crop_mode		= NVC_IMAGER_CROPMODE_PARTIAL,
	},
	.sensor_dnvc = {
		.api_version		= NVC_IMAGER_API_DYNAMIC_VER,
		.region_start_x		= 0,
		.region_start_y		= 0,
		.x_scale		= 1,
		.y_scale		= 1,
		.bracket_caps		= 1,
		.flush_count		= 2,
		.init_intra_frame_skip	= 0,
		.ss_intra_frame_skip	= 2,
		.ss_frame_number	= 3,
		.coarse_time		= 0x0448,
		.max_coarse_diff	= 5,
		.min_exposure_course	= 2,
		.max_exposure_course	= 0xFFFC,
		.diff_integration_time	= 110, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x120C,
		.frame_length		= 0x044e,
		.min_frame_length	= 0x044e,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 16000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 0x20,
		.pll_div		= 0x2,
	},
	.p_mode_i2c			= imx091_1308x736_i2c,
};

static struct imx091_mode_data imx091_2104x1560 = {
	.sensor_mode = {
		.res_x			= 2048,
		.res_y			= 1536,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 6000, /* / _INT2FLOAT_DIVISOR */
		.crop_mode		= NVC_IMAGER_CROPMODE_NONE,
	},
	.sensor_dnvc = {
		.api_version		= NVC_IMAGER_API_DYNAMIC_VER,
		.region_start_x		= 0,
		.region_start_y		= 0,
		.x_scale		= 1,
		.y_scale		= 1,
		.bracket_caps		= 1,
		.flush_count		= 2,
		.init_intra_frame_skip	= 0,
		.ss_intra_frame_skip	= 2,
		.ss_frame_number	= 3,
		.coarse_time		= 0x0653,
		.max_coarse_diff	= 5,
		.min_exposure_course	= 2,
		.max_exposure_course	= 0xFFFC,
		.diff_integration_time	= 110, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x120C,
		.frame_length		= 0x0658,
		.min_frame_length	= 0x0658,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 16000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 0x2F,
		.pll_div		= 0x2,
	},
	.p_mode_i2c			= imx091_2104x1560_i2c,
};

static struct imx091_mode_data imx091_524x390 = {
	.sensor_mode = {
		.res_x			= 524,
		.res_y			= 374,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 120000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 5000, /* / _INT2FLOAT_DIVISOR */
		.crop_mode		= NVC_IMAGER_CROPMODE_PARTIAL,
	},
	.sensor_dnvc = {
		.api_version		= NVC_IMAGER_API_DYNAMIC_VER,
		.region_start_x		= 0,
		.region_start_y		= 0,
		.x_scale		= 1,
		.y_scale		= 1,
		.bracket_caps		= 1,
		.flush_count		= 2,
		.init_intra_frame_skip	= 0,
		.ss_intra_frame_skip	= 2,
		.ss_frame_number	= 3,
		.coarse_time		= 0x0191,
		.max_coarse_diff	= 5,
		.min_exposure_course	= 2,
		.max_exposure_course	= 0xFFFC,
		.diff_integration_time	= 110, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x120C,
		.frame_length		= 0x0196,
		.min_frame_length	= 0x0196,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 16000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 0x2F,
		.pll_div		= 0x2,
	},
	.p_mode_i2c			= imx091_524X390_i2c,
};

static struct imx091_mode_data **imx091_mode_table;
static unsigned int imx091_num_modes;

static struct imx091_mode_data *imx091_mode_table_fpga[] = {
	&imx091_FPGA_1052x1560,
	&imx091_FPGA_4160x3120,
};

static struct imx091_mode_data *imx091_mode_table_non_fpga[] = {
	&imx091_4208x3120,
	&imx091_1948x1096,
	&imx091_1308x736,
	&imx091_2104x1560,
	&imx091_524x390,
};

static int imx091_i2c_rd8(struct i2c_client *client, u16 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[3];

	buf[0] = (reg >> 8);
	buf[1] = (reg & 0x00FF);
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = &buf[0];
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &buf[2];
	*val = 0;
	if (i2c_transfer(client->adapter, msg, 2) != 2)
		return -EIO;

	*val = buf[2];
	return 0;
}

static int imx091_i2c_rd16(struct i2c_client *client, u16 reg, u16 *val)
{
	struct i2c_msg msg[2];
	u8 buf[4];

	buf[0] = (reg >> 8);
	buf[1] = (reg & 0x00FF);
	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = &buf[0];
	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = &buf[2];
	*val = 0;
	if (i2c_transfer(client->adapter, msg, 2) != 2)
		return -EIO;

	*val = (((u16)buf[2] << 8) | (u16)buf[3]);
	return 0;
}

static int imx091_i2c_wr8(struct i2c_client *client, u16 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[3];

	buf[0] = (reg >> 8);
	buf[1] = (reg & 0x00FF);
	buf[2] = val;
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = &buf[0];
	if (i2c_transfer(client->adapter, &msg, 1) != 1)
		return -EIO;

	return 0;
}

static int imx091_i2c_wr16(struct i2c_client *client, u16 reg, u16 val)
{
	struct i2c_msg msg;
	u8 buf[4];

	buf[0] = (reg >> 8);
	buf[1] = (reg & 0x00FF);
	buf[2] = (val & 0x00FF);
	buf[3] = (val >> 8);
	msg.addr = client->addr;
	msg.flags = 0;
	msg.len = 4;
	msg.buf = &buf[0];
	if (i2c_transfer(client->adapter, &msg, 1) != 1)
		return -EIO;

	return 0;
}

static int imx091_i2c_rd_table(struct imx091_info *info,
			       struct imx091_reg table[])
{
	struct imx091_reg *p_table = table;
	u8 val;
	int err = 0;

	while (p_table->addr != IMX091_TABLE_END) {
		err = imx091_i2c_rd8(info->i2c_client, p_table->addr, &val);
		if (err)
			return err;

		p_table->val = (u16)val;
		p_table++;
	}

	return err;
}

static int imx091_i2c_wr_blk(struct imx091_info *info, u8 *buf, int len)
{
	struct i2c_msg msg;

	msg.addr = info->i2c_client->addr;
	msg.flags = 0;
	msg.len = len;
	msg.buf = buf;
	if (i2c_transfer(info->i2c_client->adapter, &msg, 1) != 1)
		return -EIO;

	return 0;
}

static int imx091_i2c_wr_table(struct imx091_info *info,
			       struct imx091_reg table[])
{
	int err;
	const struct imx091_reg *next;
	const struct imx091_reg *n_next;
	u8 *b_ptr = info->i2c_buf;
	u16 buf_count = 0;

	for (next = table; next->addr != IMX091_TABLE_END; next++) {
		if (next->addr == IMX091_TABLE_WAIT_MS) {
			msleep(next->val);
			continue;
		}

		if (!buf_count) {
			b_ptr = info->i2c_buf;
			*b_ptr++ = next->addr >> 8;
			*b_ptr++ = next->addr & 0xFF;
			buf_count = 2;
		}
		*b_ptr++ = next->val;
		buf_count++;
		n_next = next + 1;
		if (n_next->addr == next->addr + 1 &&
				n_next->addr != IMX091_TABLE_WAIT_MS &&
				buf_count < IMX091_SIZEOF_I2C_BUF &&
				n_next->addr != IMX091_TABLE_END)
			continue;

		err = imx091_i2c_wr_blk(info, info->i2c_buf, buf_count);
		if (err)
			return err;

		buf_count = 0;
	}

	return 0;
}

static inline void imx091_frame_length_reg(struct imx091_reg *regs,
					   u32 frame_length)
{
	regs->addr = 0x0340;
	regs->val = (frame_length >> 8) & 0xFF;
	(regs + 1)->addr = 0x0341;
	(regs + 1)->val = (frame_length) & 0xFF;
}

static inline void imx091_coarse_time_reg(struct imx091_reg *regs,
					  u32 coarse_time)
{
	regs->addr = 0x0202;
	regs->val = (coarse_time >> 8) & 0xFF;
	(regs + 1)->addr = 0x0203;
	(regs + 1)->val = (coarse_time) & 0xFF;
}

static inline void imx091_gain_reg(struct imx091_reg *regs, u32 gain)
{
	regs->addr = 0x0205;
	regs->val = gain & 0xFF;
}

static int imx091_bin_wr(struct imx091_info *info, u8 enable)
{
	int err = 0;

	if (enable == info->bin_en)
		return 0;

	if (!info->mode_valid || !imx091_mode_table[info->mode_index]->
				  sensor_dnvc.support_bin_control)
		return -EINVAL;

	if (!err)
		info->bin_en = enable;
	dev_dbg(&info->i2c_client->dev, "%s bin_en=%x err=%d\n",
		__func__, info->bin_en, err);
	return err;
}

static int imx091_exposure_wr(struct imx091_info *info,
			      struct nvc_imager_bayer *mode)
{
	struct imx091_reg reg_list[8];
	int err;

	reg_list[0].addr = 0x0104;
	reg_list[0].val = 0x01;
	imx091_frame_length_reg(reg_list+1, mode->frame_length);
	imx091_coarse_time_reg(reg_list + 3, mode->coarse_time);
	imx091_gain_reg(reg_list + 5, mode->gain);
	reg_list[6].addr = 0x0104;
	reg_list[6].val = 0x00;
	reg_list[7].addr = IMX091_TABLE_END;
	err = imx091_i2c_wr_table(info, reg_list);
	if (!err)
		err = imx091_bin_wr(info, mode->bin_en);
	return err;
}

static int imx091_gain_wr(struct imx091_info *info, u32 gain)
{
	int err;

	gain &= 0xFF;
	err = imx091_i2c_wr16(info->i2c_client, 0x0205, (u16)gain);
	return err;
}

static int imx091_gain_rd(struct imx091_info *info, u32 *gain)
{
	int err;

	*gain = 0;
	err = imx091_i2c_rd8(info->i2c_client, 0x0205, (u8 *)gain);
	return err;
}

static int imx091_group_hold_wr(struct imx091_info *info,
				struct nvc_imager_ae *ae)
{
	int err;
	bool groupHoldEnable;
	struct imx091_reg reg_list[6];
	int count = 0;

	groupHoldEnable = ae->gain_enable |
					ae->frame_length_enable |
					ae->coarse_time_enable;

	if (groupHoldEnable) {
		err = imx091_i2c_wr8(info->i2c_client, 0x104, 1);
		if (err) {
			dev_err(&info->i2c_client->dev,
				"Error: %s fail to enable grouphold\n",
				__func__);
			return err;
		}
	}

	if (ae->gain_enable) {
		imx091_gain_reg(reg_list + count, ae->gain);
		count += 1;
	}
	if (ae->coarse_time_enable) {
		imx091_coarse_time_reg(reg_list + count, ae->coarse_time);
		count += 2;
	}
	if (ae->frame_length_enable) {
		imx091_frame_length_reg(reg_list + count, ae->frame_length);
		count += 2;
	}
	reg_list[count].addr = IMX091_TABLE_END;
	err = imx091_i2c_wr_table(info, reg_list);
	if (err) {
		dev_err(&info->i2c_client->dev, "Error: %s i2c wr_table fail\n",
			__func__);
	}

	if (groupHoldEnable) {
		err = imx091_i2c_wr8(info->i2c_client, 0x104, 0);
		if (err) {
			dev_err(&info->i2c_client->dev,
				"Error: %s fail to release grouphold\n",
				__func__);
		}
	}
	return err;
}

static int imx091_test_pattern_wr(struct imx091_info *info, unsigned pattern)
{
	if (pattern >= ARRAY_SIZE(test_patterns))
		return -EINVAL;

	return imx091_i2c_wr_table(info, test_patterns[pattern]);
}

static int imx091_set_flash_output(struct imx091_info *info)
{
	struct imx091_flash_config *fcfg;
	u8 val = 0;
	int ret = 0;

	if (!info->pdata)
		return 0;

	fcfg = &info->pdata->flash_cap;
	if (fcfg->xvs_trigger_enabled)
		val |= 0x0c;
	if (fcfg->sdo_trigger_enabled)
		val |= 0x02;
	dev_dbg(&info->i2c_client->dev, "%s: %02x\n", __func__, val);
	/* disable all flash pulse output */
	ret = imx091_i2c_wr8(info->i2c_client, 0x304A, 0);
	/* config XVS/SDO pin output mode */
	ret |= imx091_i2c_wr8(info->i2c_client, 0x3240, val);
	/* set the control pulse width settings - Gain + Step
	 * Pulse width(sec) = 64 * 2^(Gain) * (Step + 1) / Logic Clk
	 * Logic Clk = ExtClk * PLL Multipiler / Pre_Div / Post_Div
	 * / Logic Clk Division Ratio
	 * Logic Clk Division Ratio = 5 @4lane, 10 @2lane, 20 @1lane
	 */
	ret |= imx091_i2c_wr8(info->i2c_client, 0x307C, 0x07);
	ret |= imx091_i2c_wr8(info->i2c_client, 0x307D, 0x3F);
	return ret;
}

static void imx091_get_flash_cap(struct imx091_info *info)
{
	struct nvc_imager_cap *fcap = info->cap;
	struct imx091_flash_config *fcfg;

	if (!info->pdata)
		return;

	fcfg = &info->pdata->flash_cap;
	fcap->flash_control_enabled =
		fcfg->xvs_trigger_enabled | fcfg->sdo_trigger_enabled;
	fcap->adjustable_flash_timing = fcfg->adjustable_flash_timing;
}

static int imx091_flash_control(
	struct imx091_info *info, union nvc_imager_flash_control *fm)
{
	int ret;
	u8 f_cntl;
	u8 f_tim;

	if (!info->pdata)
		return -EFAULT;

	ret = imx091_i2c_wr8(info->i2c_client, 0x304A, 0);
	f_tim = 0;
	f_cntl = 0;
	if (fm->settings.enable) {
		if (fm->settings.edge_trig_en) {
			f_cntl |= 0x10;
			if (fm->settings.start_edge)
				f_tim |= 0x08;
			if (fm->settings.repeat)
				f_tim |= 0x04;
			f_tim |= fm->settings.delay_frm & 0x03;
		} else
			f_cntl |= 0x20;
	}
	ret |= imx091_i2c_wr8(info->i2c_client, 0x307B, f_tim);
	ret |= imx091_i2c_wr8(info->i2c_client, 0x304A, f_cntl);

	dev_dbg(&info->i2c_client->dev,
		"%s: %04x %02x %02x\n", __func__, fm->mode, f_tim, f_cntl);
	return ret;
}

static int imx091_gpio_rd(struct imx091_info *info,
			  enum imx091_gpio i)
{
	int val = -EINVAL;

	if (info->gpio[i].flag) {
		val = gpio_get_value_cansleep(info->gpio[i].gpio);
		if (val)
			val = 1;
		dev_dbg(&info->i2c_client->dev, "%s %u %d\n",
			__func__, info->gpio[i].gpio, val);
		if (!info->gpio[i].active_high)
			val = !val;
		val &= 1;
	}
	return val; /* return read value or error */
}

static int imx091_gpio_wr(struct imx091_info *info,
			  enum imx091_gpio i,
			  int val) /* val: 0=deassert, 1=assert */
{
	int err = -EINVAL;

	if (info->gpio[i].flag) {
		if (val)
			val = 1;
		if (!info->gpio[i].active_high)
			val = !val;
		val &= 1;
		err = val;
		gpio_set_value_cansleep(info->gpio[i].gpio, val);
		dev_dbg(&info->i2c_client->dev, "%s %u %d\n",
			__func__, info->gpio[i].gpio, val);
	}
	return err; /* return value written or error */
}

static int imx091_gpio_pwrdn(struct imx091_info *info, int val)
{
	int prev_val;

	prev_val = imx091_gpio_rd(info, IMX091_GPIO_PWDN);
	if (prev_val < 0)
		return 1; /* assume PWRDN hardwired deasserted */

	if (val == prev_val)
		return 0; /* no change */

	imx091_gpio_wr(info, IMX091_GPIO_PWDN, val);
	return 1; /* return state change */
}

static int imx091_gpio_reset(struct imx091_info *info, int val)
{
	int err = 0;

	if (val) {
		if (!info->reset_flag) {
			info->reset_flag = true;
			err = imx091_gpio_wr(info, IMX091_GPIO_RESET, 1);
			if (err < 0)
				return 0; /* flag no reset */

			usleep_range(1000, 1500);
			imx091_gpio_wr(info, IMX091_GPIO_RESET, 0);
			msleep(IMX091_STARTUP_DELAY_MS); /* startup delay */
			err = 1; /* flag that a reset was done */
		}
	} else {
		info->reset_flag = false;
	}
	return err;
}

static void imx091_gpio_able(struct imx091_info *info, int val)
{
	if (val)
		imx091_gpio_wr(info, IMX091_GPIO_GP1, val);
	else
		imx091_gpio_wr(info, IMX091_GPIO_GP1, val);
}

static void imx091_gpio_exit(struct imx091_info *info)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(imx091_gpios); i++) {
		if (info->gpio[i].flag && info->gpio[i].own) {
			gpio_free(info->gpio[i].gpio);
			info->gpio[i].own = false;
		}
	}
}

static void imx091_gpio_init(struct imx091_info *info)
{
	char label[32];
	unsigned long flags;
	unsigned type;
	unsigned i;
	unsigned j;
	int err;

	for (i = 0; i < ARRAY_SIZE(imx091_gpios); i++)
		info->gpio[i].flag = false;
	if (!info->pdata->gpio_count || !info->pdata->gpio)
		return;

	for (i = 0; i < ARRAY_SIZE(imx091_gpios); i++) {
		type = imx091_gpios[i].gpio_type;
		for (j = 0; j < info->pdata->gpio_count; j++) {
			if (type == info->pdata->gpio[j].gpio_type)
				break;
		}
		if (j == info->pdata->gpio_count)
			continue;

		info->gpio[type].gpio = info->pdata->gpio[j].gpio;
		info->gpio[type].flag = true;
		if (imx091_gpios[i].use_flags) {
			flags = imx091_gpios[i].flags;
			info->gpio[type].active_high =
						   imx091_gpios[i].active_high;
		} else {
			info->gpio[type].active_high =
					      info->pdata->gpio[j].active_high;
			if (info->gpio[type].active_high)
				flags = GPIOF_OUT_INIT_LOW;
			else
				flags = GPIOF_OUT_INIT_HIGH;
		}
		if (!info->pdata->gpio[j].init_en)
			continue;

		snprintf(label, sizeof(label), "imx091_%u_%s",
			 info->pdata->num, imx091_gpios[i].label);
		err = gpio_request_one(info->gpio[type].gpio, flags, label);
		if (err) {
			dev_err(&info->i2c_client->dev, "%s ERR %s %u\n",
				__func__, label, info->gpio[type].gpio);
		} else {
			info->gpio[type].own = true;
			dev_dbg(&info->i2c_client->dev, "%s %s %u\n",
				__func__, label, info->gpio[type].gpio);
		}
	}
}

static void imx091_mclk_disable(struct imx091_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int imx091_mclk_enable(struct imx091_info *info)
{
	int err;
	unsigned long mclk_init_rate =
		nvc_imager_get_mclk(info->cap, &imx091_dflt_cap, 0);

	dev_dbg(&info->i2c_client->dev, "%s: enable MCLK with %lu Hz\n",
		__func__, mclk_init_rate);

	err = clk_set_rate(info->mclk, mclk_init_rate);
	if (!err)
		err = clk_prepare_enable(info->mclk);

	return err;
}

static int imx091_vreg_dis_all(struct imx091_info *info)
{
	int err;

	if (!info->pdata || !info->pdata->power_off)
		return -EFAULT;

	err = info->pdata->power_off(info->vreg);

	imx091_mclk_disable(info);

	return err;
}

static int imx091_vreg_en_all(struct imx091_info *info)
{
	int err;

	if (!info->pdata || !info->pdata->power_on)
		return -EFAULT;

	err = imx091_mclk_enable(info);
	if (err)
		return err;

	err = info->pdata->power_on(info->vreg);
	if (err < 0)
		imx091_mclk_disable(info);

	return err;
}

static void imx091_vreg_exit(struct imx091_info *info)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(imx091_vregs); i++) {
		if (info->vreg[i].vreg)
			regulator_put(info->vreg[i].vreg);
		info->vreg[i].vreg = NULL;
	}

	if (likely(info->imx091_ext_reg_vcm_vdd))
		regulator_put(info->imx091_ext_reg_vcm_vdd);
	info->imx091_ext_reg_vcm_vdd = NULL;

	if (likely(info->imx091_ext_reg_i2c_vdd))
		regulator_put(info->imx091_ext_reg_i2c_vdd);
	info->imx091_ext_reg_i2c_vdd = NULL;
}

static int imx091_vreg_init(struct imx091_info *info)
{
	unsigned i;
	unsigned j;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE(imx091_vregs); i++) {
		j = imx091_vregs[i].vreg_num;
		info->vreg[j].vreg_name = imx091_vregs[i].vreg_name;
		info->vreg[j].vreg_flag = false;
		info->vreg[j].vreg = regulator_get(&info->i2c_client->dev,
						   info->vreg[j].vreg_name);
		if (IS_ERR(info->vreg[j].vreg)) {
			dev_dbg(&info->i2c_client->dev, "%s %s ERR: %d\n",
				__func__, info->vreg[j].vreg_name,
				(int)info->vreg[j].vreg);
			err |= PTR_ERR(info->vreg[j].vreg);
			info->vreg[j].vreg = NULL;
		} else {
			dev_dbg(&info->i2c_client->dev, "%s: %s\n",
				__func__, info->vreg[j].vreg_name);
		}
	}
	return err;
}

static int imx091_pm_wr(struct imx091_info *info, int pwr)
{
	int ret;
	int err = 0;
	u16 val;

	if ((info->pdata->cfg & (NVC_CFG_OFF2STDBY | NVC_CFG_BOOT_INIT)) &&
			(pwr == NVC_PWR_OFF ||
			 pwr == NVC_PWR_STDBY_OFF))
		pwr = NVC_PWR_STDBY;
	if (pwr == info->pwr_dev)
		return 0;

	switch (pwr) {
	case NVC_PWR_OFF_FORCE:
	case NVC_PWR_OFF:
	case NVC_PWR_STDBY_OFF:
		imx091_gpio_pwrdn(info, 1);
		err = imx091_vreg_dis_all(info);
		imx091_gpio_able(info, 0);
		imx091_gpio_reset(info, 0);
		info->mode_valid = false;
		info->bin_en = 0;
		break;

	case NVC_PWR_STDBY:
		imx091_gpio_pwrdn(info, 1);
		err = imx091_vreg_en_all(info);
		imx091_gpio_able(info, 1);
		break;

	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		if (info->pwr_dev != NVC_PWR_ON &&
				info->pwr_dev != NVC_PWR_COMM)
			imx091_gpio_pwrdn(info, 1);
		err = imx091_vreg_en_all(info);
		imx091_gpio_able(info, 1);
		ret = imx091_gpio_pwrdn(info, 0);
		ret &= !imx091_gpio_reset(info, 1);
		if (ret) /* if no reset && pwrdn changed states then delay */
			msleep(IMX091_STARTUP_DELAY_MS);
		if (err > 0)
			err = imx091_i2c_rd16(info->i2c_client,
					      IMX091_ID_ADDRESS, &val);
		break;

	default:
		err = -EINVAL;
		break;
	}

	if (err < 0) {
		dev_err(&info->i2c_client->dev, "%s err %d\n", __func__, err);
		pwr = NVC_PWR_ERR;
	}
	info->pwr_dev = pwr;
	dev_dbg(&info->i2c_client->dev, "%s pwr_dev=%d\n",
		__func__, info->pwr_dev);
	if (err > 0)
		return 0;

	return err;
}

static int imx091_pm_wr_s(struct imx091_info *info, int pwr)
{
	int err1 = 0;
	int err2 = 0;

	if ((info->s_mode == NVC_SYNC_OFF) ||
			(info->s_mode == NVC_SYNC_MASTER) ||
			(info->s_mode == NVC_SYNC_STEREO))
		err1 = imx091_pm_wr(info, pwr);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
			(info->s_mode == NVC_SYNC_STEREO))
		err2 = imx091_pm_wr(info->s_info, pwr);
	return err1 | err2;
}

static int imx091_pm_api_wr(struct imx091_info *info, int pwr)
{
	int err = 0;

	if (!pwr || (pwr > NVC_PWR_ON))
		return 0;

	if (pwr > info->pwr_dev)
		err = imx091_pm_wr_s(info, pwr);
	if (!err)
		info->pwr_api = pwr;
	else
		info->pwr_api = NVC_PWR_ERR;
	if (info->pdata->cfg & NVC_CFG_NOERR)
		return 0;

	return err;
}

static int imx091_pm_dev_wr(struct imx091_info *info, int pwr)
{
	if (pwr < info->pwr_api)
		pwr = info->pwr_api;
	if (info->mode_enable)
		pwr = NVC_PWR_ON;
	return imx091_pm_wr(info, pwr);
}

static void imx091_pm_exit(struct imx091_info *info)
{
	imx091_pm_dev_wr(info, NVC_PWR_OFF_FORCE);
	imx091_vreg_exit(info);
	imx091_gpio_exit(info);
}

static void imx091_pm_init(struct imx091_info *info)
{
	imx091_gpio_init(info);
	imx091_vreg_init(info);
}

static int imx091_reset(struct imx091_info *info, u32 level)
{
	int err;

	if (level == NVC_RESET_SOFT) {
		err = imx091_pm_wr(info, NVC_PWR_COMM);
		/* SW reset */
		err |= imx091_i2c_wr8(info->i2c_client, 0x0103, 0x01);
	} else {
		err = imx091_pm_wr(info, NVC_PWR_OFF_FORCE);
	}
	err |= imx091_pm_wr(info, info->pwr_api);
	return err;
}

static int imx091_dev_id(struct imx091_info *info)
{
	u16 val = 0;
	unsigned i;
	int err;

	dev_dbg(&info->i2c_client->dev, "%s +++++\n",
			__func__);
	imx091_pm_dev_wr(info, NVC_PWR_COMM);
	dev_dbg(&info->i2c_client->dev, "DUCK:%s:%d\n",
			__func__, __LINE__);
	err = imx091_i2c_rd16(info->i2c_client, IMX091_ID_ADDRESS, &val);
	if (!err) {
		dev_dbg(&info->i2c_client->dev, "%s found devId: %x\n",
			__func__, val);
		info->sdata.sensor_id_minor = 0;
		for (i = 0; i < ARRAY_SIZE(imx091_ids); i++) {
			if (val == imx091_ids[i]) {
				info->sdata.sensor_id_minor = val;
				break;
			}
		}
		if (!info->sdata.sensor_id_minor) {
			err = -ENODEV;
			dev_dbg(&info->i2c_client->dev, "%s No devId match\n",
				__func__);
		}
	}
	imx091_pm_dev_wr(info, NVC_PWR_OFF);
	dev_dbg(&info->i2c_client->dev, "%s -----\n",
			__func__);
	return err;
}

static int imx091_mode_enable(struct imx091_info *info, bool mode_enable)
{
	u8 val;
	int err;

	if (mode_enable)
		val = IMX091_STREAM_ENABLE;
	else
		val = IMX091_STREAM_DISABLE;
	err = imx091_i2c_wr8(info->i2c_client, IMX091_STREAM_CONTROL_REG, val);
	if (!err) {
		info->mode_enable = mode_enable;
		dev_dbg(&info->i2c_client->dev, "%s streaming=%x\n",
			__func__, info->mode_enable);
		if (!mode_enable)
			imx091_pm_dev_wr(info, NVC_PWR_OFF);
	}
	msleep(IMX091_WAIT_MS);
	return err;
}

static int imx091_mode_rd(struct imx091_info *info,
			  s32 res_x,
			  s32 res_y,
			  u32 *index)
{
	int i;

	if (!res_x && !res_y) {
		*index = info->cap->preferred_mode_index;
		return 0;
	}

	for (i = 0; i < imx091_num_modes; i++) {
		if ((res_x == imx091_mode_table[i]->sensor_mode.res_x) &&
		    (res_y == imx091_mode_table[i]->sensor_mode.res_y)) {
			break;
		}
	}

	if (i == imx091_num_modes) {
		dev_err(&info->i2c_client->dev,
			"%s invalid resolution: %dx%d\n",
			__func__, res_x, res_y);
		return -EINVAL;
	}

	*index = i;
	return 0;
}

static int imx091_mode_wr_full(struct imx091_info *info, u32 mode_index)
{
	int err;

	imx091_pm_dev_wr(info, NVC_PWR_ON);
	imx091_bin_wr(info, 0);
	err = imx091_i2c_wr_table(info,
				  imx091_mode_table[mode_index]->p_mode_i2c);
	if (!err) {
		info->mode_index = mode_index;
		info->mode_valid = true;
	} else {
		info->mode_valid = false;
	}

	return err;
}

static int imx091_mode_wr(struct imx091_info *info,
			  struct nvc_imager_bayer *mode)
{
	u32 mode_index;
	int err;

	err = imx091_mode_rd(info, mode->res_x, mode->res_y, &mode_index);
	if (err < 0)
		return err;

	if (!mode->res_x && !mode->res_y) {
		if (mode->frame_length || mode->coarse_time || mode->gain) {
			/* write exposure only */
			err = imx091_exposure_wr(info, mode);
			return err;
		} else {
			/* turn off streaming */
			err = imx091_mode_enable(info, false);
			return err;
		}
	}

	if (!info->mode_valid || (info->mode_index != mode_index))
		err = imx091_mode_wr_full(info, mode_index);
	else
		dev_dbg(&info->i2c_client->dev, "%s short mode\n", __func__);
	err |= imx091_exposure_wr(info, mode);
	if (err < 0) {
		info->mode_valid = false;
		goto imx091_mode_wr_err;
	}

	err = imx091_set_flash_output(info);

	err |= imx091_mode_enable(info, true);
	if (err < 0)
		goto imx091_mode_wr_err;

	return 0;

imx091_mode_wr_err:
	if (!info->mode_enable)
		imx091_pm_dev_wr(info, NVC_PWR_STDBY);
	return err;
}


static int imx091_param_rd(struct imx091_info *info, unsigned long arg)
{
	struct nvc_param params;
	struct imx091_reg *p_i2c_table;
	const void *data_ptr;
	u32 data_size = 0;
	u32 u32val;
	int err;

	if (copy_from_user(&params,
			   (const void __user *)arg,
			   sizeof(struct nvc_param))) {
		dev_err(&info->i2c_client->dev,
			"%s copy_from_user err line %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (info->s_mode == NVC_SYNC_SLAVE)
		info = info->s_info;

	switch (params.param) {
	case NVC_PARAM_GAIN:
		imx091_pm_dev_wr(info, NVC_PWR_COMM);
		err = imx091_gain_rd(info, &u32val);
		imx091_pm_dev_wr(info, NVC_PWR_OFF);
		dev_dbg(&info->i2c_client->dev, "%s GAIN: %u err: %d\n",
			__func__, u32val, err);
		if (err)
			return err;

		data_ptr = &u32val;
		data_size = sizeof(u32val);
		break;

	case NVC_PARAM_STEREO_CAP:
		if (info->s_info != NULL)
			err = 0;
		else
			err = -ENODEV;
		dev_dbg(&info->i2c_client->dev, "%s STEREO_CAP: %d\n",
			__func__, err);
		data_ptr = &err;
		data_size = sizeof(err);
		break;

	case NVC_PARAM_STEREO:
		dev_dbg(&info->i2c_client->dev, "%s STEREO: %d\n",
			__func__, info->s_mode);
		data_ptr = &info->s_mode;
		data_size = sizeof(info->s_mode);
		break;

	case NVC_PARAM_STS:
		err = imx091_dev_id(info);
		dev_dbg(&info->i2c_client->dev, "%s STS: %d\n",
			__func__, err);
		data_ptr = &err;
		data_size = sizeof(err);
		break;

	case NVC_PARAM_DEV_ID:
		if (!info->sdata.sensor_id_minor)
			imx091_dev_id(info);
		data_ptr = &info->sdata.sensor_id;
		data_size = sizeof(info->sdata.sensor_id) * 2;
		dev_dbg(&info->i2c_client->dev, "%s DEV_ID: %x-%x\n",
			__func__, info->sdata.sensor_id,
			info->sdata.sensor_id_minor);
		break;

	case NVC_PARAM_SENSOR_TYPE:
		data_ptr = &info->sdata.sensor_type;
		data_size = sizeof(info->sdata.sensor_type);
		dev_dbg(&info->i2c_client->dev, "%s SENSOR_TYPE: %d\n",
			__func__, info->sdata.sensor_type);
		break;

	case NVC_PARAM_FOCAL_LEN:
		data_ptr = &info->sdata.focal_len;
		data_size = sizeof(info->sdata.focal_len);
		dev_dbg(&info->i2c_client->dev, "%s FOCAL_LEN: %u\n",
			__func__, info->sdata.focal_len);
		break;

	case NVC_PARAM_MAX_APERTURE:
		data_ptr = &info->sdata.max_aperture;
		data_size = sizeof(info->sdata.max_aperture);
		dev_dbg(&info->i2c_client->dev, "%s MAX_APERTURE: %u\n",
			__func__, info->sdata.max_aperture);
		break;

	case NVC_PARAM_FNUMBER:
		data_ptr = &info->sdata.fnumber;
		data_size = sizeof(info->sdata.fnumber);
		dev_dbg(&info->i2c_client->dev, "%s FNUMBER: %u\n",
			__func__, info->sdata.fnumber);
		break;

	case NVC_PARAM_VIEW_ANGLE_H:
		data_ptr = &info->sdata.view_angle_h;
		data_size = sizeof(info->sdata.view_angle_h);
		dev_dbg(&info->i2c_client->dev, "%s VIEW_ANGLE_H: %u\n",
			__func__, info->sdata.view_angle_h);
		break;

	case NVC_PARAM_VIEW_ANGLE_V:
		data_ptr = &info->sdata.view_angle_v;
		data_size = sizeof(info->sdata.view_angle_v);
		dev_dbg(&info->i2c_client->dev, "%s VIEW_ANGLE_V: %u\n",
			__func__, info->sdata.view_angle_v);
		break;

	case NVC_PARAM_I2C:
		dev_dbg(&info->i2c_client->dev, "%s I2C\n", __func__);
		if (params.sizeofvalue > IMX091_I2C_TABLE_MAX_ENTRIES) {
			dev_err(&info->i2c_client->dev,
				"%s NVC_PARAM_I2C request size too large\n",
				__func__);
			return -EINVAL;
		}
		p_i2c_table = kzalloc(sizeof(params.sizeofvalue), GFP_KERNEL);
		if (p_i2c_table == NULL) {
			pr_err("%s: kzalloc error\n", __func__);
			return -ENOMEM;
		}

		if (copy_from_user(p_i2c_table,
				   (const void __user *)params.p_value,
				   params.sizeofvalue)) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			kfree(p_i2c_table);
			return -EINVAL;
		}

		imx091_pm_dev_wr(info, NVC_PWR_COMM);
		err = imx091_i2c_rd_table(info, p_i2c_table);
		imx091_pm_dev_wr(info, NVC_PWR_OFF);
		if (copy_to_user((void __user *)params.p_value,
				 p_i2c_table,
				 params.sizeofvalue)) {
			dev_err(&info->i2c_client->dev,
				"%s copy_to_user err line %d\n",
				__func__, __LINE__);
			err = -EINVAL;
		}
		kfree(p_i2c_table);
		return err;
	default:
		dev_dbg(&info->i2c_client->dev,
			"%s unsupported parameter: %d\n",
			__func__, params.param);
		return -EINVAL;
	}

	if (params.sizeofvalue < data_size) {
		dev_err(&info->i2c_client->dev,
			"%s data size mismatch %d != %d Param: %d\n",
			__func__, params.sizeofvalue, data_size, params.param);
		return -EINVAL;
	}

	if (copy_to_user((void __user *)params.p_value,
			 data_ptr,
			 data_size)) {
		dev_err(&info->i2c_client->dev,
			"%s copy_to_user err line %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	return 0;
}

static int imx091_param_wr_s(struct imx091_info *info,
			     struct nvc_param *params,
			     u32 u32val)
{
	struct imx091_reg *p_i2c_table;
	u8 u8val;
	int err;

	u8val = (u8)u32val;
	switch (params->param) {
	case NVC_PARAM_GAIN:
		dev_dbg(&info->i2c_client->dev, "%s GAIN: %u\n",
			__func__, u32val);
		imx091_pm_dev_wr(info, NVC_PWR_COMM);
		err = imx091_gain_wr(info, u32val);
		if (err) {
			dev_err(&info->i2c_client->dev,
				"Error: %s SET GAIN ERR",
				__func__);
		}
		imx091_pm_dev_wr(info, NVC_PWR_STDBY);
		return err;

	case NVC_PARAM_RESET:
		err = imx091_reset(info, u32val);
		dev_dbg(&info->i2c_client->dev, "%s RESET=%d err=%d\n",
			__func__, u32val, err);
		return err;

	case NVC_PARAM_TESTMODE:
		dev_dbg(&info->i2c_client->dev, "%s TESTMODE: %u\n",
			__func__, (unsigned)u8val);
		if (u8val)
			u32val = info->test_pattern;
		else
			u32val = 0;
		imx091_pm_dev_wr(info, NVC_PWR_ON);
		err = imx091_test_pattern_wr(info, u32val);
		if (!u8val)
			imx091_pm_dev_wr(info, NVC_PWR_OFF);
		return err;

	case NVC_PARAM_TEST_PATTERN:
		dev_dbg(&info->i2c_client->dev, "%s TEST_PATTERN: %d\n",
			__func__, u32val);
		info->test_pattern = u32val;
		return 0;

	case NVC_PARAM_SELF_TEST:
		err = imx091_dev_id(info);
		dev_dbg(&info->i2c_client->dev, "%s SELF_TEST: %d\n",
			__func__, err);
		return err;

	case NVC_PARAM_I2C:
		dev_dbg(&info->i2c_client->dev, "%s I2C\n", __func__);
		if (params->sizeofvalue > IMX091_I2C_TABLE_MAX_ENTRIES) {
			dev_err(&info->i2c_client->dev,
				"%s NVC_PARAM_I2C request size too large\n",
				__func__);
			return -EINVAL;
		}
		p_i2c_table = kzalloc(sizeof(params->sizeofvalue), GFP_KERNEL);
		if (p_i2c_table == NULL) {
			dev_err(&info->i2c_client->dev,
				"%s kzalloc err line %d\n",
				__func__, __LINE__);
			return -ENOMEM;
		}

		if (copy_from_user(p_i2c_table,
				   (const void __user *)params->p_value,
				   params->sizeofvalue)) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			kfree(p_i2c_table);
			return -EFAULT;
		}

		imx091_pm_dev_wr(info, NVC_PWR_ON);
		err = imx091_i2c_wr_table(info, p_i2c_table);
		kfree(p_i2c_table);
		return err;

	case NVC_PARAM_SET_SENSOR_FLASH_MODE:
	{
		union nvc_imager_flash_control fm;
		if (copy_from_user(&fm,
			(const void __user *)params->p_value, sizeof(fm))) {
			pr_info("%s:fail set flash mode.\n", __func__);
			return -EFAULT;
		}
		return imx091_flash_control(info, &fm);
	}

	default:
		dev_dbg(&info->i2c_client->dev,
			"%s unsupported parameter: %d\n",
			__func__, params->param);
		return -EINVAL;
	}
}

static int imx091_param_wr(struct imx091_info *info, unsigned long arg)
{
	struct nvc_param params;
	u8 u8val;
	u32 u32val;
	int err = 0;

	if (copy_from_user(&params, (const void __user *)arg,
			   sizeof(struct nvc_param))) {
		dev_err(&info->i2c_client->dev,
			"%s copy_from_user err line %d\n", __func__, __LINE__);
		return -EFAULT;
	}

	if (copy_from_user(&u32val, (const void __user *)params.p_value,
			   sizeof(u32val))) {
		dev_err(&info->i2c_client->dev, "%s %d copy_from_user err\n",
			__func__, __LINE__);
		return -EFAULT;
	}

	u8val = (u8)u32val;
	/* parameters independent of sync mode */
	switch (params.param) {
	case NVC_PARAM_STEREO:
		dev_dbg(&info->i2c_client->dev, "%s STEREO: %d\n",
			__func__, u8val);
		if (u8val == info->s_mode)
			return 0;

		switch (u8val) {
		case NVC_SYNC_OFF:
			info->s_mode = u8val;
			if (info->s_info != NULL) {
				info->s_info->s_mode = u8val;
				imx091_pm_wr(info->s_info, NVC_PWR_OFF);
			}
			break;

		case NVC_SYNC_MASTER:
			info->s_mode = u8val;
			if (info->s_info != NULL)
				info->s_info->s_mode = u8val;
			break;

		case NVC_SYNC_SLAVE:
			if (info->s_info != NULL) {
				/* sync power */
				info->s_info->pwr_api = info->pwr_api;
				err = imx091_pm_wr(info->s_info,
						   info->pwr_dev);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
				} else {
					if (info->s_mode != NVC_SYNC_STEREO)
						imx091_pm_wr(info->s_info,
							     NVC_PWR_OFF);
					err = -EIO;
				}
			} else {
				err = -EINVAL;
			}
			break;

		case NVC_SYNC_STEREO:
			if (info->s_info != NULL) {
				/* sync power */
				info->s_info->pwr_api = info->pwr_api;
				err = imx091_pm_wr(info->s_info,
						   info->pwr_dev);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
				} else {
					if (info->s_mode != NVC_SYNC_SLAVE)
						imx091_pm_wr(info->s_info,
							     NVC_PWR_OFF);
					err = -EIO;
				}
			} else {
				err = -EINVAL;
			}
			break;

		default:
			err = -EINVAL;
		}
		if (info->pdata->cfg & NVC_CFG_NOERR)
			return 0;

		return err;

	case NVC_PARAM_GROUP_HOLD:
	{
		struct nvc_imager_ae ae;
		dev_dbg(&info->i2c_client->dev, "%s GROUP_HOLD\n",
			__func__);
		if (copy_from_user(&ae, (const void __user *)params.p_value,
				sizeof(struct nvc_imager_ae))) {
			dev_err(&info->i2c_client->dev,
				"Error: %s %d copy_from_user err\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		imx091_pm_dev_wr(info, NVC_PWR_COMM);
		err = imx091_group_hold_wr(info, &ae);
		imx091_pm_dev_wr(info, NVC_PWR_STDBY);
		return err;
	}

	default:
	/* parameters dependent on sync mode */
		switch (info->s_mode) {
		case NVC_SYNC_OFF:
		case NVC_SYNC_MASTER:
			return imx091_param_wr_s(info, &params, u32val);

		case NVC_SYNC_SLAVE:
			return imx091_param_wr_s(info->s_info, &params,
						 u32val);

		case NVC_SYNC_STEREO:
			err = imx091_param_wr_s(info, &params, u32val);
			if (!(info->pdata->cfg & NVC_CFG_SYNC_I2C_MUX))
				err |= imx091_param_wr_s(info->s_info,
							 &params, u32val);
			return err;

		default:
			dev_err(&info->i2c_client->dev, "%s %d internal err\n",
				__func__, __LINE__);
			return -EINVAL;
		}
	}
}

static int imx091_get_fuse_id(struct imx091_info *info)
{
	int ret, i;

	if (info->fuse_id.size)
		return 0;

	ret = imx091_i2c_wr8(info->i2c_client, 0x34C9, 0x10);

	for (i = 0; i < IMX091_FUSE_ID_SIZE ; i++) {
		ret |= imx091_i2c_rd8(info->i2c_client,
				0x3580 + i,
				&info->fuse_id.data[i]);
	}

	if (!ret)
		info->fuse_id.size = i;

	return ret;
}

static long imx091_ioctl(struct file *file,
			 unsigned int cmd,
			 unsigned long arg)
{
	struct imx091_info *info = file->private_data;
	struct nvc_imager_bayer mode;
	struct nvc_imager_mode_list mode_list;
	struct nvc_imager_mode mode_table[IMX091_NUM_MODES_MAX];
	struct nvc_imager_dnvc dnvc;
	unsigned int mode_table_size;
	const void *data_ptr;
	s32 num_modes;
	u32 i;
	int pwr;
	int err;

	if (tegra_platform_is_fpga()) {
		imx091_mode_table = imx091_mode_table_fpga;
		imx091_num_modes = ARRAY_SIZE(imx091_mode_table_fpga);

	} else {
		imx091_mode_table = imx091_mode_table_non_fpga;
		imx091_num_modes = ARRAY_SIZE(imx091_mode_table_non_fpga);
	}

	mode_table_size = sizeof(struct nvc_imager_mode) * imx091_num_modes;

	switch (_IOC_NR(cmd)) {
	case _IOC_NR(NVC_IOCTL_FUSE_ID):
		err = imx091_get_fuse_id(info);

		if (err) {
			pr_err("%s %d %d\n", __func__, __LINE__, err);
			return err;
		}
		if (copy_to_user((void __user *)arg,
				&info->fuse_id,
				sizeof(struct nvc_fuseid))) {
			pr_err("%s: %d: fail copy fuse id to user space\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		return 0;

	case _IOC_NR(NVC_IOCTL_PARAM_WR):
		err = imx091_param_wr(info, arg);
		return err;

	case _IOC_NR(NVC_IOCTL_PARAM_RD):
		err = imx091_param_rd(info, arg);
		return err;

	case _IOC_NR(NVC_IOCTL_DYNAMIC_RD):
		if (copy_from_user(&dnvc, (const void __user *)arg,
				   sizeof(struct nvc_imager_dnvc))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		dev_dbg(&info->i2c_client->dev, "%s DYNAMIC_RD x=%d y=%d\n",
			__func__, dnvc.res_x, dnvc.res_y);
		err = imx091_mode_rd(info, dnvc.res_x, dnvc.res_y, &i);
		if (err)
			return -EINVAL;

		if (dnvc.p_mode) {
			if (copy_to_user((void __user *)dnvc.p_mode,
					 &imx091_mode_table[i]->sensor_mode,
					 sizeof(struct nvc_imager_mode))) {
				dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
		}

		if (dnvc.p_dnvc) {
			if (copy_to_user((void __user *)dnvc.p_dnvc,
				      &imx091_mode_table[i]->sensor_dnvc,
				      sizeof(struct nvc_imager_dynamic_nvc))) {
				dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
		}

		return 0;

	case _IOC_NR(NVC_IOCTL_MODE_WR):
		if (copy_from_user(&mode, (const void __user *)arg,
				   sizeof(struct nvc_imager_bayer))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		dev_info(&info->i2c_client->dev,
			"%s MODE_WR x=%d y=%d coarse=%u frame=%u gain=%u\n",
			__func__, mode.res_x, mode.res_y,
			mode.coarse_time, mode.frame_length, mode.gain);
		err = imx091_mode_wr(info, &mode);
		return err;

	case _IOC_NR(NVC_IOCTL_MODE_RD):
		/*
		 * Return a list of modes that sensor bayer supports.
		 * If called with a NULL ptr to pModes,
		 * then it just returns the count.
		 */
		dev_dbg(&info->i2c_client->dev, "%s MODE_RD n=%d\n",
			__func__, imx091_num_modes);
		if (copy_from_user(&mode_list, (const void __user *)arg,
				   sizeof(struct nvc_imager_mode_list))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		num_modes = imx091_num_modes;
		if (mode_list.p_num_mode != NULL) {
			if (copy_to_user((void __user *)mode_list.p_num_mode,
					 &num_modes, sizeof(num_modes))) {
				dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
		}

		if (mode_list.p_modes != NULL) {
			for (i = 0; i < imx091_num_modes; i++) {
				mode_table[i] =
					     imx091_mode_table[i]->sensor_mode;
			}
			if (copy_to_user((void __user *)mode_list.p_modes,
					 (const void *)&mode_table,
					 mode_table_size)) {
				dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
		}

		return 0;

	case _IOC_NR(NVC_IOCTL_PWR_WR):
		/* This is a Guaranteed Level of Service (GLOS) call */
		pwr = (int)arg * 2;
		dev_dbg(&info->i2c_client->dev, "%s PWR_WR: %d\n",
			__func__, pwr);
		err = imx091_pm_api_wr(info, pwr);
		return err;

	case _IOC_NR(NVC_IOCTL_PWR_RD):
		if (info->s_mode == NVC_SYNC_SLAVE)
			pwr = info->s_info->pwr_api / 2;
		else
			pwr = info->pwr_api / 2;
		dev_dbg(&info->i2c_client->dev, "%s PWR_RD: %d\n",
			__func__, pwr);
		if (copy_to_user((void __user *)arg, (const void *)&pwr,
				 sizeof(pwr))) {
			dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
			return -EFAULT;
		}

		return 0;

	case _IOC_NR(NVC_IOCTL_CAPS_RD):
		dev_dbg(&info->i2c_client->dev, "%s CAPS_RD n=%d\n",
			__func__, sizeof(imx091_dflt_cap));
		data_ptr = info->cap;
		if (copy_to_user((void __user *)arg,
				 data_ptr,
				 sizeof(imx091_dflt_cap))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_to_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		return 0;

	case _IOC_NR(NVC_IOCTL_STATIC_RD):
		dev_dbg(&info->i2c_client->dev, "%s STATIC_RD n=%d\n",
			__func__, sizeof(struct nvc_imager_static_nvc));
		data_ptr = &info->sdata;
		if (copy_to_user((void __user *)arg,
				 data_ptr,
				 sizeof(struct nvc_imager_static_nvc))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_to_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		return 0;

	default:
		dev_dbg(&info->i2c_client->dev, "%s unsupported ioctl: %x\n",
			__func__, cmd);
	}

	return -EINVAL;
}

static void imx091_sdata_init(struct imx091_info *info)
{
	if (info->pdata->cap)
		info->cap = info->pdata->cap;
	else
		info->cap = &imx091_dflt_cap;
	memcpy(&info->sdata, &imx091_dflt_sdata, sizeof(info->sdata));
	if (info->pdata->lens_focal_length)
		info->sdata.focal_len = info->pdata->lens_focal_length;
	if (info->pdata->lens_max_aperture)
		info->sdata.max_aperture = info->pdata->lens_max_aperture;
	if (info->pdata->lens_fnumber)
		info->sdata.fnumber = info->pdata->lens_fnumber;
	if (info->pdata->lens_view_angle_h)
		info->sdata.view_angle_h = info->pdata->lens_view_angle_h;
	if (info->pdata->lens_view_angle_v)
		info->sdata.view_angle_v = info->pdata->lens_view_angle_v;
}

static int imx091_sync_en(unsigned num, unsigned sync)
{
	struct imx091_info *master = NULL;
	struct imx091_info *slave = NULL;
	struct imx091_info *pos = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &imx091_info_list, list) {
		if (pos->pdata->num == num) {
			master = pos;
			break;
		}
	}
	pos = NULL;
	list_for_each_entry_rcu(pos, &imx091_info_list, list) {
		if (pos->pdata->num == sync) {
			slave = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (master != NULL)
		master->s_info = NULL;
	if (slave != NULL)
		slave->s_info = NULL;
	if (!sync)
		return 0; /* no err if sync disabled */

	if (num == sync)
		return -EINVAL; /* err if sync instance is itself */

	if ((master != NULL) && (slave != NULL)) {
		master->s_info = slave;
		slave->s_info = master;
	}
	return 0;
}

static int imx091_sync_dis(struct imx091_info *info)
{
	if (info->s_info != NULL) {
		info->s_info->s_mode = 0;
		info->s_info->s_info = NULL;
		info->s_mode = 0;
		info->s_info = NULL;
		return 0;
	}

	return -EINVAL;
}

static int imx091_open(struct inode *inode, struct file *file)
{
	struct imx091_info *info = NULL;
	struct imx091_info *pos = NULL;
	int err;


	rcu_read_lock();
	list_for_each_entry_rcu(pos, &imx091_info_list, list) {
		if (pos->miscdev.minor == iminor(inode)) {
			info = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (!info) {
		pr_err("%s err @%d info is null\n", __func__, __LINE__);
		return -ENODEV;
	}

	dev_dbg(&info->i2c_client->dev, "%s +++++\n", __func__);
	err = imx091_sync_en(info->pdata->num, info->pdata->sync);
	if (err == -EINVAL)
		dev_err(&info->i2c_client->dev,
			"%s err: invalid num (%u) and sync (%u) instance\n",
			__func__, info->pdata->num, info->pdata->sync);
	if (atomic_xchg(&info->in_use, 1)) {
		dev_err(&info->i2c_client->dev, "%s err @%d device is busy\n",
			__func__, __LINE__);
		return -EBUSY;
	}
	if (info->s_info != NULL) {
		if (atomic_xchg(&info->s_info->in_use, 1)) {
			dev_err(&info->i2c_client->dev,
				"%s err @%d sync device is busy\n",
				__func__, __LINE__);
			return -EBUSY;
		}
		info->sdata.stereo_cap = 1;
	}

	file->private_data = info;
	dev_dbg(&info->i2c_client->dev, "%s -----\n", __func__);
	return 0;
}

static int imx091_release(struct inode *inode, struct file *file)
{
	struct imx091_info *info = file->private_data;

	dev_dbg(&info->i2c_client->dev, "%s +++++\n", __func__);
	imx091_pm_wr_s(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	if (info->s_info != NULL)
		WARN_ON(!atomic_xchg(&info->s_info->in_use, 0));
	imx091_sync_dis(info);
	dev_dbg(&info->i2c_client->dev, "%s -----\n", __func__);
	return 0;
}

static const struct file_operations imx091_fileops = {
	.owner = THIS_MODULE,
	.open = imx091_open,
	.unlocked_ioctl = imx091_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = imx091_ioctl,
#endif
	.release = imx091_release,
};

static void imx091_del(struct imx091_info *info)
{
	imx091_pm_exit(info);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
					     (info->s_mode == NVC_SYNC_STEREO))
		imx091_pm_exit(info->s_info);
	imx091_sync_dis(info);
	spin_lock(&imx091_spinlock);
	list_del_rcu(&info->list);
	spin_unlock(&imx091_spinlock);
	synchronize_rcu();
}

static int imx091_remove(struct i2c_client *client)
{
	struct imx091_info *info = i2c_get_clientdata(client);

	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
#ifdef CONFIG_DEBUG_FS
	nvc_debugfs_remove(&info->debugfs_info);
#endif
	misc_deregister(&info->miscdev);
	imx091_del(info);
	return 0;
}

static struct of_device_id imx091_of_match[] = {
	{ .compatible = "nvidia,imx091", },
	{ },
};

MODULE_DEVICE_TABLE(of, imx091_of_match);

static int imx091_get_extra_regulators(struct imx091_info *info)
{
	if (info->pdata->vcm_vdd && !info->imx091_ext_reg_vcm_vdd) {
		info->imx091_ext_reg_vcm_vdd = regulator_get(NULL,
							"imx091_vcm_vdd");
		if (WARN_ON(IS_ERR(info->imx091_ext_reg_vcm_vdd))) {
			pr_err("%s: imx091_ext_reg_vcm_vdd get failed %ld\n",
				__func__,
				PTR_ERR(info->imx091_ext_reg_vcm_vdd));
			info->imx091_ext_reg_vcm_vdd = NULL;
			return -ENODEV;
		}
	}

	if (info->pdata->i2c_vdd && !info->imx091_ext_reg_i2c_vdd) {
		info->imx091_ext_reg_i2c_vdd = regulator_get(NULL,
							"imx091_i2c_vdd");
		if (unlikely(WARN_ON(IS_ERR(info->imx091_ext_reg_i2c_vdd)))) {
			pr_err("%s: imx091_ext_reg_i2c_vdd get failed %ld\n",
				__func__,
				PTR_ERR(info->imx091_ext_reg_i2c_vdd));
			info->imx091_ext_reg_i2c_vdd = NULL;
			if (info->pdata->vcm_vdd)
				regulator_put(info->imx091_ext_reg_vcm_vdd);
			info->imx091_ext_reg_vcm_vdd = NULL;
			return -ENODEV;
		}
	}

	return 0;
}

static int imx091_power_on(struct nvc_regulator *vreg)
{
	int err;
	struct imx091_info *info = container_of(vreg, struct imx091_info,
						vreg[0]);

	if (unlikely(WARN_ON(!info)))
		return -EFAULT;

	if (imx091_get_extra_regulators(info))
		goto imx091_poweron_fail;

	if (info->pdata->vcm_vdd) {
		err = regulator_enable(info->imx091_ext_reg_vcm_vdd);
		if (unlikely(err))
			goto imx091_vcm_fail;
	}

	if (info->pdata->i2c_vdd) {
		err = regulator_enable(info->imx091_ext_reg_i2c_vdd);
		if (unlikely(err))
			goto imx091_i2c_fail;
	}

	imx091_gpio_wr(info, IMX091_GPIO_PWDN, 0);
	usleep_range(10, 20);

	if (vreg[IMX091_VREG_AVDD].vreg) {
		err = regulator_enable(vreg[IMX091_VREG_AVDD].vreg);
		if (unlikely(err))
			goto imx091_avdd_fail;
	}

	if (vreg[IMX091_VREG_DVDD].vreg) {
		err = regulator_enable(vreg[IMX091_VREG_DVDD].vreg);
		if (unlikely(err))
			goto imx091_dvdd_fail;
	}

	if (vreg[IMX091_VREG_IOVDD].vreg) {
		err = regulator_enable(vreg[IMX091_VREG_IOVDD].vreg);
		if (unlikely(err))
			goto imx091_iovdd_fail;
	}

	usleep_range(1, 2);
	imx091_gpio_wr(info, IMX091_GPIO_PWDN, 1);
	usleep_range(300, 310);

	return 1;

imx091_iovdd_fail:
	if (vreg[IMX091_VREG_DVDD].vreg)
		regulator_disable(vreg[IMX091_VREG_DVDD].vreg);

imx091_dvdd_fail:
	if (vreg[IMX091_VREG_AVDD].vreg)
		regulator_disable(vreg[IMX091_VREG_AVDD].vreg);

imx091_avdd_fail:
	imx091_gpio_wr(info, IMX091_GPIO_PWDN, 0);
	if (info->pdata->i2c_vdd)
		regulator_disable(info->imx091_ext_reg_i2c_vdd);
imx091_i2c_fail:
	if (info->pdata->vcm_vdd)
		regulator_disable(info->imx091_ext_reg_vcm_vdd);
imx091_vcm_fail:
imx091_poweron_fail:
	pr_err("%s FAILED\n", __func__);
	return -ENODEV;

}

static int imx091_power_off(struct nvc_regulator *vreg)
{
	struct imx091_info *info = container_of(vreg, struct imx091_info,
						vreg[0]);

	if (unlikely(WARN_ON(!info)))
		return -EFAULT;

	usleep_range(1, 2);
	imx091_gpio_wr(info, IMX091_GPIO_PWDN, 0);
	usleep_range(1, 2);

	if (vreg[IMX091_VREG_IOVDD].vreg)
		regulator_disable(vreg[IMX091_VREG_IOVDD].vreg);
	if (vreg[IMX091_VREG_DVDD].vreg)
		regulator_disable(vreg[IMX091_VREG_DVDD].vreg);
	if (vreg[IMX091_VREG_AVDD].vreg)
		regulator_disable(vreg[IMX091_VREG_AVDD].vreg);

	if (info->pdata->i2c_vdd)
		regulator_disable(info->imx091_ext_reg_i2c_vdd);
	if (info->pdata->vcm_vdd)
		regulator_disable(info->imx091_ext_reg_vcm_vdd);

	return 0;
}

static int imx091_parse_dt_gpio(struct device_node *np, const char *name,
			enum imx091_gpio type,
			struct nvc_gpio_pdata *pdata)
{
	enum of_gpio_flags gpio_flags;

	if (of_find_property(np, name, NULL)) {
		pdata->gpio = of_get_named_gpio_flags(np, name, 0, &gpio_flags);
		pdata->gpio_type = type;
		pdata->init_en = true;
		pdata->active_high = !(gpio_flags & OF_GPIO_ACTIVE_LOW);
		return 1;
	}
	return 0;
}

static struct imx091_platform_data *imx091_parse_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct imx091_platform_data *board_info_pdata;
	const struct of_device_id *match;
	struct nvc_gpio_pdata *gpio_pdata = NULL;
	struct nvc_imager_cap *imx091_cap = NULL;
	u32 temp_prop_read = 0;

	match = of_match_device(imx091_of_match, &client->dev);
	if (!match) {
		dev_err(&client->dev, "Failed to find matching dt id\n");
		return NULL;
	}

	board_info_pdata = devm_kzalloc(&client->dev, sizeof(*board_info_pdata),
			GFP_KERNEL);
	if (!board_info_pdata) {
		dev_err(&client->dev, "Failed to allocate pdata\n");
		return ERR_PTR(-ENOMEM);
	}

	gpio_pdata = devm_kzalloc(&client->dev,
		sizeof(*gpio_pdata) * ARRAY_SIZE(imx091_gpios),
		GFP_KERNEL);
	if (!gpio_pdata) {
		dev_err(&client->dev, "cannot allocate gpio data memory\n");
		return ERR_PTR(-ENOMEM);
	}

	imx091_cap = devm_kzalloc(&client->dev,
		sizeof(*imx091_cap),
		GFP_KERNEL);
	if (!imx091_cap) {
		dev_err(&client->dev, "cannot allocate imx091_cap memory\n");
		return ERR_PTR(-ENOMEM);
	}

	/* init with default platform data values */
	memcpy(board_info_pdata, &imx091_dflt_pdata, sizeof(*board_info_pdata));

	/* extra regulators info */
	board_info_pdata->vcm_vdd = of_property_read_bool(np, "nvidia,vcm_vdd");
	board_info_pdata->i2c_vdd = of_property_read_bool(np, "nvidia,i2c_vdd");

	/* MCLK clock info */
	of_property_read_string(np, "nvidia,mclk_name",
				&board_info_pdata->mclk_name);

	/* generic info */
	of_property_read_u32(np, "nvidia,num", &board_info_pdata->num);
	of_property_read_u32(np, "nvidia,cfg", &board_info_pdata->cfg);
	of_property_read_u32(np, "nvidia,sync", &board_info_pdata->sync);
	of_property_read_string(np, "nvidia,dev_name",
				&board_info_pdata->dev_name);

	/* imx091 gpios */
	board_info_pdata->gpio_count = 0;
	board_info_pdata->gpio_count += imx091_parse_dt_gpio(np,
				"reset-gpios", IMX091_GPIO_RESET,
				&gpio_pdata[board_info_pdata->gpio_count]);
	board_info_pdata->gpio_count += imx091_parse_dt_gpio(np,
				"power-gpios", IMX091_GPIO_PWDN,
				&gpio_pdata[board_info_pdata->gpio_count]);
	board_info_pdata->gpio_count += imx091_parse_dt_gpio(np,
				"gp1-gpios", IMX091_GPIO_GP1,
				&gpio_pdata[board_info_pdata->gpio_count]);
	board_info_pdata->gpio = gpio_pdata;

	/* imx091 caps */
	nvc_imager_parse_caps(np, imx091_cap);
	imx091_cap->focuser_guid = NVC_FOCUS_GUID(0);
	imx091_cap->torch_guid = NVC_TORCH_GUID(0);
	imx091_cap->cap_version = NVC_IMAGER_CAPABILITIES_VERSION2;

	board_info_pdata->cap = imx091_cap;

	/* imx091 flash caps */
	board_info_pdata->flash_cap.xvs_trigger_enabled =
		of_property_read_bool(np, "nvidia,xvs_trigger_enabled");
	board_info_pdata->flash_cap.sdo_trigger_enabled =
		of_property_read_bool(np, "nvidia,sdo_trigger_enabled");
	board_info_pdata->flash_cap.adjustable_flash_timing =
		of_property_read_bool(np, "nvidia,adjustable_flash_timing");
	of_property_read_u32(np, "nvidia,pulse_width_uS",
				&temp_prop_read);
	board_info_pdata->flash_cap.pulse_width_uS = (u16)temp_prop_read;

	/* imx091 power functions */
	board_info_pdata->power_on = imx091_power_on;
	board_info_pdata->power_off = imx091_power_off;

	return board_info_pdata;
}

static int imx091_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct imx091_info *info;
	unsigned long clock_probe_rate;
	int err;
	const char *mclk_name;

	if (tegra_platform_is_fpga()) {
		imx091_dflt_cap.discontinuous_clk_mode	= 0;
		imx091_dflt_cap.cil_threshold_settle	= 0xd;
		imx091_dflt_cap.focuser_guid	= 0;
		imx091_dflt_cap.torch_guid	= 0;
	}

	dev_dbg(&client->dev, "%s +++++\n", __func__);
	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->i2c_client = client;

	if (client->dev.of_node) {
		info->pdata = imx091_parse_dt(client);
	} else if (client->dev.platform_data) {
		info->pdata = client->dev.platform_data;
	} else {
		info->pdata = &imx091_dflt_pdata;
		dev_dbg(&client->dev,
			"%s No platform data.  Using defaults.\n", __func__);
	}

	if (!info->pdata) {
		dev_err(&client->dev, "%s: Platform data error.\n", __func__);
		return -EINVAL;
	}

	mclk_name = info->pdata->mclk_name ?
		    info->pdata->mclk_name : "default_mclk";
	info->mclk = devm_clk_get(&client->dev, mclk_name);
	if (IS_ERR(info->mclk)) {
		dev_err(&client->dev, "%s: unable to get clock %s\n",
			__func__, mclk_name);
		return PTR_ERR(info->mclk);
	}

	i2c_set_clientdata(client, info);
	INIT_LIST_HEAD(&info->list);
	spin_lock(&imx091_spinlock);
	list_add_rcu(&info->list, &imx091_info_list);
	spin_unlock(&imx091_spinlock);
	imx091_pm_init(info);
	imx091_sdata_init(info);
	imx091_get_flash_cap(info);
	info->pwr_api = NVC_PWR_OFF;
	if (info->pdata->cfg & (NVC_CFG_NODEV | NVC_CFG_BOOT_INIT)) {
		if (info->pdata->probe_clock) {
			if (info->cap->initial_clock_rate_khz)
				clock_probe_rate = info->cap->
							initial_clock_rate_khz;
			else
				clock_probe_rate = imx091_dflt_cap.
							initial_clock_rate_khz;
			clock_probe_rate *= 1000;
			info->pdata->probe_clock(clock_probe_rate);
		}
		err = imx091_dev_id(info);
		if (err < 0) {
			if (info->pdata->cfg & NVC_CFG_NODEV) {
				imx091_del(info);
				if (info->pdata->probe_clock)
					info->pdata->probe_clock(0);
				return -ENODEV;
			} else {
				dev_err(&client->dev, "%s device not found\n",
					__func__);
			}
		} else {
			dev_dbg(&client->dev, "%s device found\n", __func__);
			if (info->pdata->cfg & NVC_CFG_BOOT_INIT)
				imx091_mode_wr_full(info, info->cap->
						    preferred_mode_index);
			info->dev_id = info->sdata.sensor_id_minor;
		}
		imx091_pm_dev_wr(info, NVC_PWR_OFF);
		if (info->pdata->probe_clock)
			info->pdata->probe_clock(0);
	}

	if (info->pdata->dev_name != 0)
		strncpy(info->devname, info->pdata->dev_name,
			sizeof(info->devname) - 1);
	else
		strncpy(info->devname, "imx091", sizeof(info->devname) - 1);

	if (info->pdata->num)
		snprintf(info->devname, sizeof(info->devname), "%s.%u",
			 info->devname, info->pdata->num);
	info->miscdev.name = info->devname;
	info->miscdev.fops = &imx091_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
			__func__, info->devname);
		imx091_del(info);
		return -ENODEV;
	}

#ifdef CONFIG_DEBUG_FS
	info->debugfs_info.name = info->devname;
	info->debugfs_info.i2c_client = info->i2c_client;
	info->debugfs_info.i2c_addr_limit = 0xFFFF;
	info->debugfs_info.i2c_rd8 = imx091_i2c_rd8;
	info->debugfs_info.i2c_wr8 = imx091_i2c_wr8;
	nvc_debugfs_init(&info->debugfs_info);
#endif
	dev_dbg(&client->dev, "%s -----\n", __func__);
	return 0;
}

static const struct i2c_device_id imx091_id[] = {
	{ "imx091", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, imx091_id);

static struct i2c_driver imx091_i2c_driver = {
	.driver = {
		.name = "imx091",
		.owner = THIS_MODULE,
	},
	.id_table = imx091_id,
	.probe = imx091_probe,
	.remove = imx091_remove,
};

module_i2c_driver(imx091_i2c_driver);
MODULE_LICENSE("GPL v2");
