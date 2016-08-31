/*
* imx179.c - imx179 sensor driver
*
* Copyright (c) 2013-2014, NVIDIA Corporation. All Rights Reserved.
*
* This file is licensed under the terms of the GNU General Public License
* version 2. This program is licensed "as is" without any warranty of any
* kind, whether express or implied.
*/

#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/list.h>
#include <media/imx179.h>

#include "nvc_utilities.h"

#define IMX179_ID			0x0179
#define IMX179_ID_ADDRESS   0x0002
#define IMX179_STREAM_CONTROL_REG 0x0100
#define IMX179_STREAM_ENABLE 0x01
#define IMX179_STREAM_DISABLE 0x00
#define IMX179_SENSOR_TYPE		NVC_IMAGER_TYPE_RAW
#define IMX179_STARTUP_DELAY_MS		50
#define IMX179_RES_CHG_WAIT_TIME_MS	100
#define IMX179_SIZEOF_I2C_BUF		16
#define IMX179_TABLE_WAIT_MS		0
#define IMX179_TABLE_END		1
#define IMX179_NUM_MODES		ARRAY_SIZE(imx179_mode_table)
#define IMX179_MODE_UNKNOWN		(IMX179_NUM_MODES + 1)
#define IMX179_LENS_MAX_APERTURE	0 /* / _INT2FLOAT_DIVISOR */
#define IMX179_LENS_FNUMBER		0 /* / _INT2FLOAT_DIVISOR */
#define IMX179_LENS_FOCAL_LENGTH	3700 /* / _INT2FLOAT_DIVISOR */
#define IMX179_LENS_VIEW_ANGLE_H	75600 /* / _INT2FLOAT_DIVISOR */
#define IMX179_LENS_VIEW_ANGLE_V	75600 /* / _INT2FLOAT_DIVISOR */
#define IMX179_WAIT_MS 3
#define IMX179_I2C_TABLE_MAX_ENTRIES	400

static u16 imx179_ids[] = {
	0x0179,
};

static struct nvc_gpio_init imx179_gpios[] = {
	{IMX179_GPIO_RESET, GPIOF_OUT_INIT_LOW, "reset", false, true},
	{IMX179_GPIO_PWDN, GPIOF_OUT_INIT_LOW, "pwdn", false, true},
	{IMX179_GPIO_GP1, 0, "gp1", false, false},
};

static struct nvc_regulator_init imx179_vregs[] = {
	{ IMX179_VREG_DVDD, "vdig", },
	{ IMX179_VREG_AVDD, "vana", },
	{ IMX179_VREG_IOVDD, "vif", },
};

struct imx179_info {
	atomic_t in_use;
	struct i2c_client *i2c_client;
	struct imx179_platform_data *pdata;
	struct nvc_imager_cap *cap;
	struct miscdevice miscdev;
	struct list_head list;
	struct clk *mclk;
	struct nvc_gpio gpio[ARRAY_SIZE(imx179_gpios)];
	struct nvc_regulator vreg[ARRAY_SIZE(imx179_vregs)];
	int pwr_api;
	int pwr_dev;
	u8 s_mode;
	struct imx179_info *s_info;
	u32 mode_index;
	bool mode_valid;
	bool mode_enable;
	bool reset_flag;
	unsigned test_pattern;
	struct nvc_imager_static_nvc sdata;
	u8 i2c_buf[IMX179_SIZEOF_I2C_BUF];
	u8 bin_en;
#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_root;
	u16	i2c_reg;
#endif
};

struct imx179_reg {
	u16 addr;
	u16 val;
};

struct imx179_mode_data {
	struct nvc_imager_mode sensor_mode;
	struct nvc_imager_dynamic_nvc sensor_dnvc;
	struct imx179_reg *p_mode_i2c;
};

static struct nvc_imager_cap imx179_dflt_cap = {
	.identifier		= "IMX179",
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
	.focuser_guid		= NVC_FOCUS_GUID(0),
	.torch_guid		= 0, /*NVC_TORCH_GUID(0),*/
	.cap_version		= NVC_IMAGER_CAPABILITIES_VERSION2,
};

static struct imx179_platform_data imx179_dflt_pdata = {
	.cfg			= 0,
	.num			= 0,
	.sync			= 0,
	.dev_name		= "camera",
	.cap			= &imx179_dflt_cap,
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
static struct nvc_imager_static_nvc imx179_dflt_sdata = {
	.api_version		= NVC_IMAGER_API_STATIC_VER,
	.sensor_type		= IMX179_SENSOR_TYPE,
	.bits_per_pixel		= 10,
	.sensor_id		= IMX179_ID,
	.sensor_id_minor	= 0,
	.focal_len		= IMX179_LENS_FOCAL_LENGTH,
	.max_aperture		= IMX179_LENS_MAX_APERTURE,
	.fnumber		= IMX179_LENS_FNUMBER,
	.view_angle_h		= IMX179_LENS_VIEW_ANGLE_H,
	.view_angle_v		= IMX179_LENS_VIEW_ANGLE_V,
	.res_chg_wait_time	= IMX179_RES_CHG_WAIT_TIME_MS,
};

static LIST_HEAD(imx179_info_list);
static DEFINE_SPINLOCK(imx179_spinlock);


static struct imx179_reg tp_none_seq[] = {
	{IMX179_TABLE_END, 0x0000}
};

static struct imx179_reg tp_cbars_seq[] = {
	{IMX179_TABLE_END, 0x0000}
};

static struct imx179_reg tp_checker_seq[] = {
	{IMX179_TABLE_END, 0x0000}
};

static struct imx179_reg *test_patterns[] = {
	tp_none_seq,
	tp_cbars_seq,
	tp_checker_seq,
};

static struct imx179_reg imx179_3280x2464_i2c[] = {
	/*stand by*/
	{0x0100, 0x00},
	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},

	{0x0101, 0x00},
	{0x0202, 0x09},
	{0x0203, 0xCC},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0xA2},
	{0x0340, 0x09},
	{0x0341, 0xD0},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x0C},
	{0x0349, 0xCF},
	{0x034A, 0x09},
	{0x034B, 0x9F},
	{0x034C, 0x0C},
	{0x034D, 0xD0},
	{0x034E, 0x09},
	{0x034F, 0xA0},
	{0x0383, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x00},
	{0x0401, 0x00},
	{0x0405, 0x10},
	{0x3020, 0x10},
	{0x3041, 0x15},
	{0x3042, 0x87},
	{0x3089, 0x4F},
	{0x3309, 0x9A},
	{0x3344, 0x57},
	{0x3345, 0x1F},
	{0x3362, 0x0A},
	{0x3363, 0x0A},
	{0x3364, 0x00},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3370, 0x77},
	{0x3371, 0x2F},
	{0x3372, 0x4F},
	{0x3373, 0x2F},
	{0x3374, 0x2F},
	{0x3375, 0x37},
	{0x3376, 0x9F},
	{0x3377, 0x37},
	{0x33C8, 0x00},
	{0x33D4, 0x0C},
	{0x33D5, 0xD0},
	{0x33D6, 0x09},
	{0x33D7, 0xA0},
	{0x4100, 0x0E},
	{0x4108, 0x01},
	{0x4109, 0x7C},

	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},
	{IMX179_TABLE_END, 0x00}
};

static struct imx179_reg imx179_1640x1232_i2c[] = {
	/*stand by*/
	{0x0100, 0x00},
	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},

	{0x0101, 0x00},
	{0x0202, 0x09},
	{0x0203, 0xCC},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0xA2},
	{0x0340, 0x09},
	{0x0341, 0xD0},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x0C},
	{0x0349, 0xCF},
	{0x034A, 0x09},
	{0x034B, 0x9F},
	{0x034C, 0x06},
	{0x034D, 0x68},
	{0x034E, 0x04},
	{0x034F, 0xD0},
	{0x0383, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x01},
	{0x0401, 0x00},
	{0x0405, 0x10},
	{0x3020, 0x10},
	{0x3041, 0x15},
	{0x3042, 0x87},
	{0x3089, 0x4F},
	{0x3309, 0x9A},
	{0x3344, 0x57},
	{0x3345, 0x1F},
	{0x3362, 0x0A},
	{0x3363, 0x0A},
	{0x3364, 0x00},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3370, 0x77},
	{0x3371, 0x2F},
	{0x3372, 0x4F},
	{0x3373, 0x2F},
	{0x3374, 0x2F},
	{0x3375, 0x37},
	{0x3376, 0x9F},
	{0x3377, 0x37},
	{0x33C8, 0x00},
	{0x33D4, 0x06},
	{0x33D5, 0x68},
	{0x33D6, 0x04},
	{0x33D7, 0xD0},
	{0x4100, 0x0E},
	{0x4108, 0x01},
	{0x4109, 0x7C},

	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},
	{IMX179_TABLE_END, 0x00}
};

static struct imx179_reg imx179_1920x1080_i2c[] = {
	/*stand by*/
	{0x0100, 0x00},
	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},

	{0x0101, 0x00},
	{0x0202, 0x09},
	{0x0203, 0xCC},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0xA2},
	{0x0340, 0x09},
	{0x0341, 0xD0},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x02},
	{0x0345, 0xA8},
	{0x0346, 0x02},
	{0x0347, 0xB4},
	{0x0348, 0x0A},
	{0x0349, 0x27},
	{0x034A, 0x06},
	{0x034B, 0xEB},
	{0x034C, 0x07},
	{0x034D, 0x80},
	{0x034E, 0x04},
	{0x034F, 0x38},
	{0x0383, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x00},
	{0x0401, 0x00},
	{0x0405, 0x10},
	{0x3020, 0x10},
	{0x3041, 0x15},
	{0x3042, 0x87},
	{0x3089, 0x4F},
	{0x3309, 0x9A},
	{0x3344, 0x57},
	{0x3345, 0x1F},
	{0x3362, 0x0A},
	{0x3363, 0x0A},
	{0x3364, 0x00},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3370, 0x77},
	{0x3371, 0x2F},
	{0x3372, 0x4F},
	{0x3373, 0x2F},
	{0x3374, 0x2F},
	{0x3375, 0x37},
	{0x3376, 0x9F},
	{0x3377, 0x37},
	{0x33C8, 0x00},
	{0x33D4, 0x07},
	{0x33D5, 0x80},
	{0x33D6, 0x04},
	{0x33D7, 0x38},
	{0x4100, 0x0E},
	{0x4108, 0x01},
	{0x4109, 0x7C},

	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},
	{IMX179_TABLE_END, 0x00}
};

static struct imx179_reg imx179_1280x720_i2c[] = {
	/*stand by*/
	{0x0100, 0x00},
	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},

	{0x0101, 0x00},
	{0x0202, 0x09},
	{0x0203, 0xCC},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0xA2},
	{0x0340, 0x09},
	{0x0341, 0xD0},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x01},
	{0x0345, 0x68},
	{0x0346, 0x02},
	{0x0347, 0x00},
	{0x0348, 0x0B},
	{0x0349, 0x67},
	{0x034A, 0x07},
	{0x034B, 0x9F},
	{0x034C, 0x05},
	{0x034D, 0x00},
	{0x034E, 0x02},
	{0x034F, 0xD0},
	{0x0383, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x01},
	{0x0401, 0x00},
	{0x0405, 0x10},
	{0x3020, 0x10},
	{0x3041, 0x15},
	{0x3042, 0x87},
	{0x3089, 0x4F},
	{0x3309, 0x9A},
	{0x3344, 0x57},
	{0x3345, 0x1F},
	{0x3362, 0x0A},
	{0x3363, 0x0A},
	{0x3364, 0x00},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3370, 0x77},
	{0x3371, 0x2F},
	{0x3372, 0x4F},
	{0x3373, 0x2F},
	{0x3374, 0x2F},
	{0x3375, 0x37},
	{0x3376, 0x9F},
	{0x3377, 0x37},
	{0x33C8, 0x00},
	{0x33D4, 0x05},
	{0x33D5, 0x00},
	{0x33D6, 0x02},
	{0x33D7, 0xD0},
	{0x4100, 0x0E},
	{0x4108, 0x01},
	{0x4109, 0x7C},

	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},
	{IMX179_TABLE_END, 0x00}
};

static struct imx179_reg imx179_640x480_i2c[] = {
	/*stand by*/
	{0x0100, 0x00},
	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},

	{0x0101, 0x00},
	{0x0202, 0x09},
	{0x0203, 0xCC},
	{0x0301, 0x05},
	{0x0303, 0x01},
	{0x0305, 0x06},
	{0x0309, 0x05},
	{0x030B, 0x01},
	{0x030C, 0x00},
	{0x030D, 0xA2},
	{0x0340, 0x09},
	{0x0341, 0xD0},
	{0x0342, 0x0D},
	{0x0343, 0x70},
	{0x0344, 0x01},
	{0x0345, 0x68},
	{0x0346, 0x01},
	{0x0347, 0x10},
	{0x0348, 0x0B},
	{0x0349, 0x67},
	{0x034A, 0x08},
	{0x034B, 0x8F},
	{0x034C, 0x02},
	{0x034D, 0x80},
	{0x034E, 0x01},
	{0x034F, 0xE0},
	{0x0383, 0x01},
	{0x0387, 0x01},
	{0x0390, 0x02},
	{0x0401, 0x00},
	{0x0405, 0x10},
	{0x3020, 0x10},
	{0x3041, 0x15},
	{0x3042, 0x87},
	{0x3089, 0x4F},
	{0x3309, 0x9A},
	{0x3344, 0x57},
	{0x3345, 0x1F},
	{0x3362, 0x0A},
	{0x3363, 0x0A},
	{0x3364, 0x00},
	{0x3368, 0x18},
	{0x3369, 0x00},
	{0x3370, 0x77},
	{0x3371, 0x2F},
	{0x3372, 0x4F},
	{0x3373, 0x2F},
	{0x3374, 0x2F},
	{0x3375, 0x37},
	{0x3376, 0x9F},
	{0x3377, 0x37},
	{0x33C8, 0x00},
	{0x33D4, 0x02},
	{0x33D5, 0x80},
	{0x33D6, 0x01},
	{0x33D7, 0xE0},
	{0x4100, 0x0E},
	{0x4108, 0x01},
	{0x4109, 0x7C},

	{IMX179_TABLE_WAIT_MS, IMX179_WAIT_MS},
	{IMX179_TABLE_END, 0x00}
};
/* Each resolution requires the below data table setup and the corresponding
 * I2C data table.
 * If more NVC data is needed for the NVC driver, be sure and modify the
 * nvc_imager_nvc structure in nvc_imager.h
 * If more data sets are needed per resolution, they can be added to the
 * table format below with the imx179_mode_data structure.  New data sets
 * should conform to an already defined NVC structure.  If it's data for the
 * NVC driver, then it should be added to the nvc_imager_nvc structure.
 * Steps to add a resolution:
 * 1. Add I2C data table
 * 2. Add imx179_mode_data table
 * 3. Add entry to the imx179_mode_table
 */
static struct imx179_mode_data imx179_3280x2464 = {
	.sensor_mode = {
		.res_x			= 3280,
		.res_y			= 2464,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 11000, /* / _INT2FLOAT_DIVISOR */
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
		.coarse_time		= 0x09CC,
		.max_coarse_diff	= 5,
		.min_exposure_course	= 2,
		.max_exposure_course	= 0xFFFC,
		.diff_integration_time	= 110, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x0D70,
		.frame_length		= 0x09D0,
		.min_frame_length	= 0x09D0,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 16000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 0xA2,
		.pll_div		= 0x06,
	},
	.p_mode_i2c			= imx179_3280x2464_i2c,
};

static struct imx179_mode_data imx179_1640x1232 = {
	.sensor_mode = {
		.res_x			= 1640,
		.res_y			= 1232,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 11000, /* / _INT2FLOAT_DIVISOR */
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
		.coarse_time		= 0x09CC,
		.max_coarse_diff	= 5,
		.min_exposure_course	= 2,
		.max_exposure_course	= 0xFFFC,
		.diff_integration_time	= 110, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x0D70,
		.frame_length		= 0x09D0,
		.min_frame_length	= 0x09D0,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 16000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 0xA2,
		.pll_div		= 0x06,
	},
	.p_mode_i2c			= imx179_1640x1232_i2c,
};

static struct imx179_mode_data imx179_1920x1080 = {
	.sensor_mode = {
		.res_x			= 1920,
		.res_y			= 1080,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 11000, /* / _INT2FLOAT_DIVISOR */
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
		.coarse_time		= 0x09CC,
		.max_coarse_diff	= 5,
		.min_exposure_course	= 2,
		.max_exposure_course	= 0xFFFC,
		.diff_integration_time	= 110, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x0D70,
		.frame_length		= 0x09D0,
		.min_frame_length	= 0x09D0,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 16000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 0xA2,
		.pll_div		= 0x06,
	},
	.p_mode_i2c			= imx179_1920x1080_i2c,
};

static struct imx179_mode_data imx179_1280x720 = {
	.sensor_mode = {
		.res_x			= 1280,
		.res_y			= 720,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 11000, /* / _INT2FLOAT_DIVISOR */
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
		.coarse_time		= 0x09CC,
		.max_coarse_diff	= 5,
		.min_exposure_course	= 2,
		.max_exposure_course	= 0xFFFC,
		.diff_integration_time	= 110, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x0D70,
		.frame_length		= 0x09D0,
		.min_frame_length	= 0x09D0,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 16000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 0xA2,
		.pll_div		= 0x06,
	},
	.p_mode_i2c			= imx179_1280x720_i2c,
};

static struct imx179_mode_data imx179_640x480 = {
	.sensor_mode = {
		.res_x			= 640,
		.res_y			= 480,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 11000, /* / _INT2FLOAT_DIVISOR */
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
		.coarse_time		= 0x09CC,
		.max_coarse_diff	= 5,
		.min_exposure_course	= 2,
		.max_exposure_course	= 0xFFFC,
		.diff_integration_time	= 110, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 0x0D70,
		.frame_length		= 0x09D0,
		.min_frame_length	= 0x09D0,
		.max_frame_length	= 0xFFFF,
		.min_gain		= 1, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 16000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 0xA2,
		.pll_div		= 0x06,
	},
	.p_mode_i2c			= imx179_640x480_i2c,
};

static struct imx179_mode_data *imx179_mode_table[] = {
	&imx179_3280x2464,
	&imx179_1640x1232,
	&imx179_1920x1080,
	&imx179_1280x720,
	&imx179_640x480,
};

static int imx179_i2c_rd8(struct imx179_info *info, u16 reg, u8 *val)
{
	struct i2c_msg msg[2];
	u8 buf[3];

	buf[0] = (reg >> 8);
	buf[1] = (reg & 0x00FF);
	msg[0].addr = info->i2c_client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = &buf[0];
	msg[1].addr = info->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = &buf[2];
	*val = 0;
	if (i2c_transfer(info->i2c_client->adapter, msg, 2) != 2)
		return -EIO;

	*val = buf[2];
	return 0;
}

static int imx179_i2c_rd16(struct imx179_info *info, u16 reg, u16 *val)
{
	struct i2c_msg msg[2];
	u8 buf[4];

	buf[0] = (reg >> 8);
	buf[1] = (reg & 0x00FF);
	msg[0].addr = info->i2c_client->addr;
	msg[0].flags = 0;
	msg[0].len = 2;
	msg[0].buf = &buf[0];
	msg[1].addr = info->i2c_client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = &buf[2];
	*val = 0;
	if (i2c_transfer(info->i2c_client->adapter, msg, 2) != 2)
		return -EIO;

	*val = (((u16)buf[2] << 8) | (u16)buf[3]);
	return 0;
}

static int imx179_i2c_wr8(struct imx179_info *info, u16 reg, u8 val)
{
	struct i2c_msg msg;
	u8 buf[3];

	buf[0] = (reg >> 8);
	buf[1] = (reg & 0x00FF);
	buf[2] = val;
	msg.addr = info->i2c_client->addr;
	msg.flags = 0;
	msg.len = 3;
	msg.buf = &buf[0];
	if (i2c_transfer(info->i2c_client->adapter, &msg, 1) != 1)
		return -EIO;

	return 0;
}

static int imx179_i2c_wr16(struct imx179_info *info, u16 reg, u16 val)
{
	struct i2c_msg msg;
	u8 buf[4];

	buf[0] = (reg >> 8);
	buf[1] = (reg & 0x00FF);
	buf[2] = (val & 0x00FF);
	buf[3] = (val >> 8);
	msg.addr = info->i2c_client->addr;
	msg.flags = 0;
	msg.len = 4;
	msg.buf = &buf[0];
	if (i2c_transfer(info->i2c_client->adapter, &msg, 1) != 1)
		return -EIO;

	return 0;
}

static int imx179_i2c_rd_table(struct imx179_info *info,
			       struct imx179_reg table[])
{
	struct imx179_reg *p_table = table;
	u8 val;
	int err = 0;

	while (p_table->addr != IMX179_TABLE_END) {
		err = imx179_i2c_rd8(info, p_table->addr, &val);
		if (err)
			return err;

		p_table->val = (u16)val;
		p_table++;
	}

	return err;
}

static int imx179_i2c_wr_blk(struct imx179_info *info, u8 *buf, int len)
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

static int imx179_i2c_wr_table(struct imx179_info *info,
			       struct imx179_reg table[])
{
	int err;
	const struct imx179_reg *next;
	const struct imx179_reg *n_next;
	u8 *b_ptr = info->i2c_buf;
	u16 buf_count = 0;

	for (next = table; next->addr != IMX179_TABLE_END; next++) {
		if (next->addr == IMX179_TABLE_WAIT_MS) {
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
				n_next->addr != IMX179_TABLE_WAIT_MS &&
				buf_count < IMX179_SIZEOF_I2C_BUF &&
				n_next->addr != IMX179_TABLE_END)
			continue;

		err = imx179_i2c_wr_blk(info, info->i2c_buf, buf_count);
		if (err)
			return err;

		buf_count = 0;
	}

	return 0;
}

static inline void imx179_frame_length_reg(struct imx179_reg *regs,
					   u32 frame_length)
{
	regs->addr = 0x0340;
	regs->val = (frame_length >> 8) & 0xFF;
	(regs + 1)->addr = 0x0341;
	(regs + 1)->val = (frame_length) & 0xFF;
}

static inline void imx179_coarse_time_reg(struct imx179_reg *regs,
					  u32 coarse_time)
{
	regs->addr = 0x0202;
	regs->val = (coarse_time >> 8) & 0xFF;
	(regs + 1)->addr = 0x0203;
	(regs + 1)->val = (coarse_time) & 0xFF;
}

static inline void imx179_gain_reg(struct imx179_reg *regs, u32 gain)
{
	regs->addr = 0x0205;
	regs->val = gain & 0xFF;
}

static int imx179_bin_wr(struct imx179_info *info, u8 enable)
{
	int err = 0;

	if (enable == info->bin_en)
		return 0;

	if (!info->mode_valid || !imx179_mode_table[info->mode_index]->
				  sensor_dnvc.support_bin_control)
		return -EINVAL;

	if (!err)
		info->bin_en = enable;
	dev_dbg(&info->i2c_client->dev, "%s bin_en=%x err=%d\n",
		__func__, info->bin_en, err);
	return err;
}

static int imx179_exposure_wr(struct imx179_info *info,
			      struct nvc_imager_bayer *mode)
{
	struct imx179_reg reg_list[8];
	int err;

	reg_list[0].addr = 0x0104;
	reg_list[0].val = 0x01;
	imx179_frame_length_reg(reg_list+1, mode->frame_length);
	imx179_coarse_time_reg(reg_list + 3, mode->coarse_time);
	imx179_gain_reg(reg_list + 5, mode->gain);
	reg_list[6].addr = 0x0104;
	reg_list[6].val = 0x00;
	reg_list[7].addr = IMX179_TABLE_END;
	err = imx179_i2c_wr_table(info, reg_list);
	if (!err)
		err = imx179_bin_wr(info, mode->bin_en);
	return err;
}

static int imx179_gain_wr(struct imx179_info *info, u32 gain)
{
	int err;

	gain &= 0xFF;
	err = imx179_i2c_wr16(info, 0x0205, (u16)gain);
	return err;
}

static int imx179_gain_rd(struct imx179_info *info, u32 *gain)
{
	int err;

	*gain = 0;
	err = imx179_i2c_rd8(info, 0x0205, (u8 *)gain);
	return err;
}

static int imx179_group_hold_wr(struct imx179_info *info,
				struct nvc_imager_ae *ae)
{
	int err;
	bool groupHoldEnable;
	struct imx179_reg reg_list[6];
	int count = 0;

	groupHoldEnable = ae->gain_enable |
					ae->frame_length_enable |
					ae->coarse_time_enable;

	if (groupHoldEnable) {
		err = imx179_i2c_wr8(info, 0x0104, 1);
		if (err) {
			dev_err(&info->i2c_client->dev,
				"Error: %s fail to enable grouphold\n",
				__func__);
			return err;
		}
	}

	if (ae->gain_enable) {
		imx179_gain_reg(reg_list + count, ae->gain);
		count += 1;
	}
	if (ae->coarse_time_enable) {
		imx179_coarse_time_reg(reg_list + count, ae->coarse_time);
		count += 2;
	}
	if (ae->frame_length_enable) {
		imx179_frame_length_reg(reg_list + count, ae->frame_length);
		count += 2;
	}
	reg_list[count].addr = IMX179_TABLE_END;
	err = imx179_i2c_wr_table(info, reg_list);
	if (err) {
		dev_err(&info->i2c_client->dev, "Error: %s i2c wr_table fail\n",
			__func__);
	}

	if (groupHoldEnable) {
		err = imx179_i2c_wr8(info, 0x0104, 0);
		if (err) {
			dev_err(&info->i2c_client->dev,
				"Error: %s fail to release grouphold\n",
				__func__);
		}
	}
	return err;
}

static int imx179_test_pattern_wr(struct imx179_info *info, unsigned pattern)
{
	if (pattern >= ARRAY_SIZE(test_patterns))
		return -EINVAL;

	return imx179_i2c_wr_table(info, test_patterns[pattern]);
}

static int imx179_set_flash_output(struct imx179_info *info)
{
	struct imx179_flash_config *fcfg;
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
	ret = imx179_i2c_wr8(info, 0x304A, 0);
	/* config XVS/SDO pin output mode */
	ret |= imx179_i2c_wr8(info, 0x3240, val);
	/* set the control pulse width settings - Gain + Step
	 * Pulse width(sec) = 64 * 2^(Gain) * (Step + 1) / Logic Clk
	 * Logic Clk = ExtClk * PLL Multipiler / Pre_Div / Post_Div
	 * / Logic Clk Division Ratio
	 * Logic Clk Division Ratio = 5 @4lane, 10 @2lane, 20 @1lane
	 */
	ret |= imx179_i2c_wr8(info, 0x307C, 0x07);
	ret |= imx179_i2c_wr8(info, 0x307D, 0x3F);
	return ret;
}

static void imx179_get_flash_cap(struct imx179_info *info)
{
	struct nvc_imager_cap *fcap = info->cap;
	struct imx179_flash_config *fcfg;

	if (!info->pdata)
		return;

	fcfg = &info->pdata->flash_cap;
	fcap->flash_control_enabled =
		fcfg->xvs_trigger_enabled | fcfg->sdo_trigger_enabled;
	fcap->adjustable_flash_timing = fcfg->adjustable_flash_timing;
}

static int imx179_flash_control(
	struct imx179_info *info, union nvc_imager_flash_control *fm)
{
	int ret;
	u8 f_cntl;
	u8 f_tim;

	if (!info->pdata)
		return -EFAULT;

	ret = imx179_i2c_wr8(info, 0x304A, 0);
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
	ret |= imx179_i2c_wr8(info, 0x307B, f_tim);
	ret |= imx179_i2c_wr8(info, 0x304A, f_cntl);

	dev_dbg(&info->i2c_client->dev,
		"%s: %04x %02x %02x\n", __func__, fm->mode, f_tim, f_cntl);
	return ret;
}

static int imx179_gpio_rd(struct imx179_info *info,
			  enum imx179_gpio i)
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

static int imx179_gpio_wr(struct imx179_info *info,
			  enum imx179_gpio i,
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

static int imx179_gpio_pwrdn(struct imx179_info *info, int val)
{
	int prev_val;

	prev_val = imx179_gpio_rd(info, IMX179_GPIO_PWDN);
	if (prev_val < 0)
		return 1; /* assume PWRDN hardwired deasserted */

	if (val == prev_val)
		return 0; /* no change */

	imx179_gpio_wr(info, IMX179_GPIO_PWDN, val);
	return 1; /* return state change */
}

static int imx179_gpio_reset(struct imx179_info *info, int val)
{
	int err = 0;

	if (val) {
		if (!info->reset_flag) {
			info->reset_flag = true;
			err = imx179_gpio_wr(info, IMX179_GPIO_RESET, 1);
			if (err < 0)
				return 0; /* flag no reset */

			usleep_range(1000, 1500);
			imx179_gpio_wr(info, IMX179_GPIO_RESET, 0);
			msleep(IMX179_STARTUP_DELAY_MS); /* startup delay */
			err = 1; /* flag that a reset was done */
		}
	} else {
		info->reset_flag = false;
	}
	return err;
}

static void imx179_gpio_able(struct imx179_info *info, int val)
{
	if (val)
		imx179_gpio_wr(info, IMX179_GPIO_GP1, val);
	else
		imx179_gpio_wr(info, IMX179_GPIO_GP1, val);
}

static void imx179_gpio_exit(struct imx179_info *info)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(imx179_gpios); i++) {
		if (info->gpio[i].flag && info->gpio[i].own) {
			gpio_free(info->gpio[i].gpio);
			info->gpio[i].own = false;
		}
	}
}

static void imx179_gpio_init(struct imx179_info *info)
{
	char label[32];
	unsigned long flags;
	unsigned type;
	unsigned i;
	unsigned j;
	int err;

	for (i = 0; i < ARRAY_SIZE(imx179_gpios); i++)
		info->gpio[i].flag = false;
	if (!info->pdata->gpio_count || !info->pdata->gpio)
		return;

	for (i = 0; i < ARRAY_SIZE(imx179_gpios); i++) {
		type = imx179_gpios[i].gpio_type;
		for (j = 0; j < info->pdata->gpio_count; j++) {
			if (type == info->pdata->gpio[j].gpio_type)
				break;
		}
		if (j == info->pdata->gpio_count)
			continue;

		info->gpio[type].gpio = info->pdata->gpio[j].gpio;
		info->gpio[type].flag = true;
		if (imx179_gpios[i].use_flags) {
			flags = imx179_gpios[i].flags;
			info->gpio[type].active_high =
						   imx179_gpios[i].active_high;
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

		snprintf(label, sizeof(label), "imx179_%u_%s",
			 info->pdata->num, imx179_gpios[i].label);
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

static void imx179_mclk_disable(struct imx179_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);
}

static int imx179_mclk_enable(struct imx179_info *info)
{
	int err;
	unsigned long mclk_init_rate =
		nvc_imager_get_mclk(info->cap, &imx179_dflt_cap, 0);

	dev_dbg(&info->i2c_client->dev, "%s: enable MCLK with %lu Hz\n",
		__func__, mclk_init_rate);

	err = clk_set_rate(info->mclk, mclk_init_rate);
	if (!err)
		err = clk_prepare_enable(info->mclk);

	return err;
}

static int imx179_vreg_dis_all(struct imx179_info *info)
{
	int err;

	if (!info->pdata || !info->pdata->power_off)
		return -EFAULT;

	err = info->pdata->power_off(info->vreg);

	imx179_mclk_disable(info);

    return err;
}

static int imx179_vreg_en_all(struct imx179_info *info)
{
	int err = 0;
	if (!info->pdata || !info->pdata->power_on)
		return -EFAULT;

	err = imx179_mclk_enable(info);
	if (err)
		return err;

	err = info->pdata->power_on(info->vreg);
	if (err < 0)
		imx179_mclk_disable(info);

	return err;
}

static void imx179_vreg_exit(struct imx179_info *info)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(imx179_vregs); i++) {
		regulator_put(info->vreg[i].vreg);
		info->vreg[i].vreg = NULL;
	}
}

static int imx179_vreg_init(struct imx179_info *info)
{
	unsigned i;
	unsigned j;
	int err = 0;

	for (i = 0; i < ARRAY_SIZE(imx179_vregs); i++) {
		j = imx179_vregs[i].vreg_num;
		info->vreg[j].vreg_name = imx179_vregs[i].vreg_name;
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

static int imx179_pm_wr(struct imx179_info *info, int pwr)
{
	int ret;
	int err = 0;

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
		imx179_gpio_pwrdn(info, 1);
		err = imx179_vreg_dis_all(info);
		imx179_gpio_able(info, 0);
		imx179_gpio_reset(info, 0);
		info->mode_valid = false;
		info->bin_en = 0;
		break;

	case NVC_PWR_STDBY:
		imx179_gpio_pwrdn(info, 1);
		err = imx179_vreg_en_all(info);
		imx179_gpio_able(info, 1);
		break;

	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		if (info->pwr_dev != NVC_PWR_ON &&
				info->pwr_dev != NVC_PWR_COMM)
			imx179_gpio_pwrdn(info, 1);
		err = imx179_vreg_en_all(info);
		imx179_gpio_able(info, 1);
		ret = imx179_gpio_pwrdn(info, 0);
		ret &= !imx179_gpio_reset(info, 1);
		if (ret) /* if no reset && pwrdn changed states then delay */
			msleep(IMX179_STARTUP_DELAY_MS);
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

static int imx179_pm_wr_s(struct imx179_info *info, int pwr)
{
	int err1 = 0;
	int err2 = 0;

	if ((info->s_mode == NVC_SYNC_OFF) ||
			(info->s_mode == NVC_SYNC_MASTER) ||
			(info->s_mode == NVC_SYNC_STEREO))
		err1 = imx179_pm_wr(info, pwr);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
			(info->s_mode == NVC_SYNC_STEREO))
		err2 = imx179_pm_wr(info->s_info, pwr);
	return err1 | err2;
}

static int imx179_pm_api_wr(struct imx179_info *info, int pwr)
{
	int err = 0;

	if (!pwr || (pwr > NVC_PWR_ON))
		return 0;

	if (pwr > info->pwr_dev)
		err = imx179_pm_wr_s(info, pwr);
	if (!err)
		info->pwr_api = pwr;
	else
		info->pwr_api = NVC_PWR_ERR;
	if (info->pdata->cfg & NVC_CFG_NOERR)
		return 0;

	return err;
}

static int imx179_pm_dev_wr(struct imx179_info *info, int pwr)
{
	if (pwr < info->pwr_api)
		pwr = info->pwr_api;
	if (info->mode_enable)
		pwr = NVC_PWR_ON;
	return imx179_pm_wr(info, pwr);
}

static void imx179_pm_exit(struct imx179_info *info)
{
	imx179_pm_wr(info, NVC_PWR_OFF_FORCE);
	imx179_vreg_exit(info);
	imx179_gpio_exit(info);
}

static void imx179_pm_init(struct imx179_info *info)
{
	imx179_gpio_init(info);
	imx179_vreg_init(info);
}

static int imx179_reset(struct imx179_info *info, u32 level)
{
	int err;

	if (level == NVC_RESET_SOFT) {
		err = imx179_pm_wr(info, NVC_PWR_COMM);
		err |= imx179_i2c_wr8(info, 0x0103, 0x01); /* SW reset */
	} else {
		err = imx179_pm_wr(info, NVC_PWR_OFF_FORCE);
	}
	err |= imx179_pm_wr(info, info->pwr_api);
	return err;
}

static int imx179_dev_id(struct imx179_info *info)
{
	u16 val = 0;
	unsigned i;
	int err;

	dev_dbg(&info->i2c_client->dev, "%s +++++\n",
			__func__);
	imx179_pm_dev_wr(info, NVC_PWR_COMM);
	dev_dbg(&info->i2c_client->dev, "DUCK:%s:%d\n",
			__func__, __LINE__);
	err = imx179_i2c_rd16(info, IMX179_ID_ADDRESS, &val);
	if (!err) {
		dev_dbg(&info->i2c_client->dev, "%s found devId: %x\n",
			__func__, val);
		info->sdata.sensor_id_minor = 0;
		for (i = 0; i < ARRAY_SIZE(imx179_ids); i++) {
			if (val == imx179_ids[i]) {
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
	imx179_pm_dev_wr(info, NVC_PWR_OFF);
	dev_dbg(&info->i2c_client->dev, "%s -----\n",
			__func__);
	return err;
}

static int imx179_mode_able(struct imx179_info *info, bool mode_enable)
{
	u8 val;
	int err;

	if (mode_enable)
		val = IMX179_STREAM_ENABLE;
	else
		val = IMX179_STREAM_DISABLE;
	err = imx179_i2c_wr8(info, IMX179_STREAM_CONTROL_REG, val);
	if (!err) {
		info->mode_enable = mode_enable;
		dev_dbg(&info->i2c_client->dev, "%s streaming=%x\n",
			__func__, info->mode_enable);
		if (!mode_enable)
			imx179_pm_dev_wr(info, NVC_PWR_OFF);
	}
	msleep(IMX179_WAIT_MS);
	return err;
}

static int imx179_mode_rd(struct imx179_info *info,
			  s32 res_x,
			  s32 res_y,
			  u32 *index)
{
	int i;

	if (!res_x && !res_y) {
		*index = info->cap->preferred_mode_index;
		return 0;
	}

	for (i = 0; i < IMX179_NUM_MODES; i++) {
		if ((res_x == imx179_mode_table[i]->sensor_mode.res_x) &&
		    (res_y == imx179_mode_table[i]->sensor_mode.res_y)) {
			break;
		}
	}

	if (i == IMX179_NUM_MODES) {
		dev_err(&info->i2c_client->dev,
			"%s invalid resolution: %dx%d\n",
			__func__, res_x, res_y);
		return -EINVAL;
	}

	*index = i;
	return 0;
}

static int imx179_mode_wr_full(struct imx179_info *info, u32 mode_index)
{
	int err;

	imx179_pm_dev_wr(info, NVC_PWR_ON);
	imx179_bin_wr(info, 0);
	err = imx179_i2c_wr_table(info,
				  imx179_mode_table[mode_index]->p_mode_i2c);
	if (!err) {
		info->mode_index = mode_index;
		info->mode_valid = true;
	} else {
		info->mode_valid = false;
	}

	return err;
}

static int imx179_mode_wr(struct imx179_info *info,
			  struct nvc_imager_bayer *mode)
{
	u32 mode_index;
	int err;

	err = imx179_mode_rd(info, mode->res_x, mode->res_y, &mode_index);
	if (err < 0)
		return err;

	if (!mode->res_x && !mode->res_y) {
		if (mode->frame_length || mode->coarse_time || mode->gain) {
			/* write exposure only */
			err = imx179_exposure_wr(info, mode);
			return err;
		} else {
			/* turn off streaming */
			err = imx179_mode_able(info, false);
			return err;
		}
	}

	if (!info->mode_valid || (info->mode_index != mode_index))
		err = imx179_mode_wr_full(info, mode_index);
	else
		dev_dbg(&info->i2c_client->dev, "%s short mode\n", __func__);
	err |= imx179_exposure_wr(info, mode);
	if (err < 0) {
		info->mode_valid = false;
		goto imx179_mode_wr_err;
	}

	err = imx179_set_flash_output(info);

	err |= imx179_mode_able(info, true);
	if (err < 0)
		goto imx179_mode_wr_err;

	return 0;

imx179_mode_wr_err:
	if (!info->mode_enable)
		imx179_pm_dev_wr(info, NVC_PWR_STDBY);
	return err;
}


static int imx179_param_rd(struct imx179_info *info, unsigned long arg)
{
	struct nvc_param params;
	struct imx179_reg *p_i2c_table;
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
		imx179_pm_dev_wr(info, NVC_PWR_COMM);
		err = imx179_gain_rd(info, &u32val);
		imx179_pm_dev_wr(info, NVC_PWR_OFF);
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
		err = imx179_dev_id(info);
		dev_dbg(&info->i2c_client->dev, "%s STS: %d\n",
			__func__, err);
		data_ptr = &err;
		data_size = sizeof(err);
		break;

	case NVC_PARAM_DEV_ID:
		if (!info->sdata.sensor_id_minor)
			imx179_dev_id(info);
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
		if (params.sizeofvalue > IMX179_I2C_TABLE_MAX_ENTRIES) {
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

		imx179_pm_dev_wr(info, NVC_PWR_COMM);
		err = imx179_i2c_rd_table(info, p_i2c_table);
		imx179_pm_dev_wr(info, NVC_PWR_OFF);
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

static int imx179_param_wr_s(struct imx179_info *info,
			     struct nvc_param *params,
			     u32 u32val)
{
	struct imx179_reg *p_i2c_table;
	u8 u8val;
	int err;

	u8val = (u8)u32val;
	switch (params->param) {
	case NVC_PARAM_GAIN:
		dev_dbg(&info->i2c_client->dev, "%s GAIN: %u\n",
			__func__, u32val);
		imx179_pm_dev_wr(info, NVC_PWR_COMM);
		err = imx179_gain_wr(info, u32val);
		if (err) {
			dev_err(&info->i2c_client->dev, "Error: %s SET GAIN ERR",
							__func__);
		}
		imx179_pm_dev_wr(info, NVC_PWR_STDBY);
		return err;

	case NVC_PARAM_RESET:
		err = imx179_reset(info, u32val);
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
		imx179_pm_dev_wr(info, NVC_PWR_ON);
		err = imx179_test_pattern_wr(info, u32val);
		if (!u8val)
			imx179_pm_dev_wr(info, NVC_PWR_OFF);
		return err;

	case NVC_PARAM_TEST_PATTERN:
		dev_dbg(&info->i2c_client->dev, "%s TEST_PATTERN: %d\n",
			__func__, u32val);
		info->test_pattern = u32val;
		return 0;

	case NVC_PARAM_SELF_TEST:
		err = imx179_dev_id(info);
		dev_dbg(&info->i2c_client->dev, "%s SELF_TEST: %d\n",
			__func__, err);
		return err;

	case NVC_PARAM_I2C:
		dev_dbg(&info->i2c_client->dev, "%s I2C\n", __func__);
		if (params->sizeofvalue > IMX179_I2C_TABLE_MAX_ENTRIES) {
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

		imx179_pm_dev_wr(info, NVC_PWR_ON);
		err = imx179_i2c_wr_table(info, p_i2c_table);
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
		return imx179_flash_control(info, &fm);
	}

	default:
		dev_dbg(&info->i2c_client->dev,
			"%s unsupported parameter: %d\n",
			__func__, params->param);
		return -EINVAL;
	}
}

static int imx179_param_wr(struct imx179_info *info, unsigned long arg)
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
				imx179_pm_wr(info->s_info, NVC_PWR_OFF);
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
				err = imx179_pm_wr(info->s_info,
						   info->pwr_dev);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
				} else {
					if (info->s_mode != NVC_SYNC_STEREO)
						imx179_pm_wr(info->s_info,
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
				err = imx179_pm_wr(info->s_info,
						   info->pwr_dev);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
				} else {
					if (info->s_mode != NVC_SYNC_SLAVE)
						imx179_pm_wr(info->s_info,
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
			dev_err(&info->i2c_client->dev, "Error: %s %d copy_from_user err\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		imx179_pm_dev_wr(info, NVC_PWR_COMM);
		err = imx179_group_hold_wr(info, &ae);
		imx179_pm_dev_wr(info, NVC_PWR_STDBY);
		return err;
	}

	default:
	/* parameters dependent on sync mode */
		switch (info->s_mode) {
		case NVC_SYNC_OFF:
		case NVC_SYNC_MASTER:
			return imx179_param_wr_s(info, &params, u32val);

		case NVC_SYNC_SLAVE:
			return imx179_param_wr_s(info->s_info, &params,
						 u32val);

		case NVC_SYNC_STEREO:
			err = imx179_param_wr_s(info, &params, u32val);
			if (!(info->pdata->cfg & NVC_CFG_SYNC_I2C_MUX))
				err |= imx179_param_wr_s(info->s_info,
							 &params, u32val);
			return err;

		default:
			dev_err(&info->i2c_client->dev, "%s %d internal err\n",
				__func__, __LINE__);
			return -EINVAL;
		}
	}
}

static long imx179_ioctl(struct file *file,
			 unsigned int cmd,
			 unsigned long arg)
{
	struct imx179_info *info = file->private_data;
	struct nvc_imager_bayer mode;
	struct nvc_imager_mode_list mode_list;
	struct nvc_imager_mode mode_table[IMX179_NUM_MODES];
	struct nvc_imager_dnvc dnvc;
	const void *data_ptr;
	s32 num_modes;
	u32 i;
	int pwr;
	int err;

	switch (cmd) {
	case NVC_IOCTL_PARAM_WR:
		err = imx179_param_wr(info, arg);
		return err;

	case NVC_IOCTL_PARAM_RD:
		err = imx179_param_rd(info, arg);
		return err;

	case NVC_IOCTL_DYNAMIC_RD:
		if (copy_from_user(&dnvc, (const void __user *)arg,
				   sizeof(struct nvc_imager_dnvc))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		dev_dbg(&info->i2c_client->dev, "%s DYNAMIC_RD x=%d y=%d\n",
			__func__, dnvc.res_x, dnvc.res_y);
		err = imx179_mode_rd(info, dnvc.res_x, dnvc.res_y, &i);
		if (err)
			return -EINVAL;

		if (dnvc.p_mode) {
			if (copy_to_user((void __user *)dnvc.p_mode,
					 &imx179_mode_table[i]->sensor_mode,
					 sizeof(struct nvc_imager_mode))) {
				dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
		}

		if (dnvc.p_dnvc) {
			if (copy_to_user((void __user *)dnvc.p_dnvc,
				      &imx179_mode_table[i]->sensor_dnvc,
				      sizeof(struct nvc_imager_dynamic_nvc))) {
				dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
		}

		return 0;

	case NVC_IOCTL_MODE_WR:
		if (copy_from_user(&mode, (const void __user *)arg,
				   sizeof(struct nvc_imager_bayer))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		dev_dbg(&info->i2c_client->dev,
			"%s MODE_WR x=%d y=%d coarse=%u frame=%u gain=%u\n",
			__func__, mode.res_x, mode.res_y,
			mode.coarse_time, mode.frame_length, mode.gain);

		err = imx179_mode_wr(info, &mode);
		return err;

	case NVC_IOCTL_MODE_RD:
		/*
		 * Return a list of modes that sensor bayer supports.
		 * If called with a NULL ptr to pModes,
		 * then it just returns the count.
		 */
		dev_dbg(&info->i2c_client->dev, "%s MODE_RD n=%d\n",
			__func__, IMX179_NUM_MODES);
		if (copy_from_user(&mode_list, (const void __user *)arg,
				   sizeof(struct nvc_imager_mode_list))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		num_modes = IMX179_NUM_MODES;
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
			for (i = 0; i < IMX179_NUM_MODES; i++) {
				mode_table[i] =
					     imx179_mode_table[i]->sensor_mode;
			}
			if (copy_to_user((void __user *)mode_list.p_modes,
					 (const void *)&mode_table,
					 sizeof(mode_table))) {
				dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
		}

		return 0;

	case NVC_IOCTL_PWR_WR:
		/* This is a Guaranteed Level of Service (GLOS) call */
		pwr = (int)arg * 2;
		dev_dbg(&info->i2c_client->dev, "%s PWR_WR: %d\n",
			__func__, pwr);
		err = imx179_pm_api_wr(info, pwr);
		return err;

	case NVC_IOCTL_PWR_RD:
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

	case NVC_IOCTL_CAPS_RD:
		dev_dbg(&info->i2c_client->dev, "%s CAPS_RD n=%d\n",
			__func__, sizeof(imx179_dflt_cap));
		data_ptr = info->cap;
		if (copy_to_user((void __user *)arg,
				 data_ptr,
				 sizeof(imx179_dflt_cap))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_to_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		return 0;

	case NVC_IOCTL_STATIC_RD:
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

static void imx179_sdata_init(struct imx179_info *info)
{
	if (info->pdata->cap)
		info->cap = info->pdata->cap;
	else
		info->cap = &imx179_dflt_cap;
	memcpy(&info->sdata, &imx179_dflt_sdata, sizeof(info->sdata));
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

static int imx179_sync_en(unsigned num, unsigned sync)
{
	struct imx179_info *master = NULL;
	struct imx179_info *slave = NULL;
	struct imx179_info *pos = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &imx179_info_list, list) {
		if (pos->pdata->num == num) {
			master = pos;
			break;
		}
	}
	pos = NULL;
	list_for_each_entry_rcu(pos, &imx179_info_list, list) {
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

static int imx179_sync_dis(struct imx179_info *info)
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

static int imx179_open(struct inode *inode, struct file *file)
{
	struct imx179_info *info = NULL;
	struct imx179_info *pos = NULL;
	int err;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &imx179_info_list, list) {
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
	err = imx179_sync_en(info->pdata->num, info->pdata->sync);
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
			dev_err(&info->i2c_client->dev, "%s err @%d sync device is busy\n",
					__func__, __LINE__);
			return -EBUSY;
		}
		info->sdata.stereo_cap = 1;
	}

	file->private_data = info;
	dev_dbg(&info->i2c_client->dev, "%s -----\n", __func__);
	return 0;
}

static int imx179_release(struct inode *inode, struct file *file)
{
	struct imx179_info *info = file->private_data;

	dev_dbg(&info->i2c_client->dev, "%s +++++\n", __func__);
	imx179_pm_wr_s(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	if (info->s_info != NULL)
		WARN_ON(!atomic_xchg(&info->s_info->in_use, 0));
	imx179_sync_dis(info);
	dev_dbg(&info->i2c_client->dev, "%s -----\n", __func__);
	return 0;
}

static const struct file_operations imx179_fileops = {
	.owner = THIS_MODULE,
	.open = imx179_open,
	.unlocked_ioctl = imx179_ioctl,
	.release = imx179_release,
};

static void imx179_del(struct imx179_info *info)
{
	imx179_pm_exit(info);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
					     (info->s_mode == NVC_SYNC_STEREO))
		imx179_pm_exit(info->s_info);
	imx179_sync_dis(info);
	spin_lock(&imx179_spinlock);
	list_del_rcu(&info->list);
	spin_unlock(&imx179_spinlock);
	synchronize_rcu();
}

static int imx179_remove(struct i2c_client *client)
{
	struct imx179_info *info = i2c_get_clientdata(client);

	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
#ifdef CONFIG_DEBUG_FS
	if (info->debugfs_root)
		debugfs_remove_recursive(info->debugfs_root);
#endif
	misc_deregister(&info->miscdev);
	imx179_del(info);
	return 0;
}

#ifdef CONFIG_DEBUG_FS
static int i2ca_get(void *data, u64 *val)
{
	struct imx179_info *info = (struct imx179_info *)(data);
	*val = (u64)info->i2c_reg;
	return 0;
}

static int i2ca_set(void *data, u64 val)
{
	struct imx179_info *info = (struct imx179_info *)(data);

	if (val > 0x36FF) {
		dev_err(&info->i2c_client->dev, "ERR:%s out of range\n",
				__func__);
		return -EIO;
	}

	info->i2c_reg = (u16) val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i2ca_fops, i2ca_get, i2ca_set, "0x%02llx\n");

static int i2cr_get(void *data, u64 *val)
{
	u8 temp = 0;
	struct imx179_info *info = (struct imx179_info *)(data);

	if (imx179_i2c_rd8(info, info->i2c_reg, &temp)) {
		dev_err(&info->i2c_client->dev, "ERR:%s failed\n", __func__);
		return -EIO;
	}
	*val = (u64)temp;
	return 0;
}

static int i2cr_set(void *data, u64 val)
{
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i2cr_fops, i2cr_get, i2cr_set, "0x%02llx\n");

static int i2cw_get(void *data, u64 *val)
{
	return 0;
}

static int i2cw_set(void *data, u64 val)
{
	struct imx179_info *info = (struct imx179_info *)(data);

	val &= 0xFF;
	if (imx179_i2c_wr8(info, info->i2c_reg, val)) {
		dev_err(&info->i2c_client->dev, "ERR:%s failed\n", __func__);
		return -EIO;
	}
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(i2cw_fops, i2cw_get, i2cw_set, "0x%02llx\n");

static int imx179_debug_init(struct imx179_info *info)
{
	dev_dbg(&info->i2c_client->dev, "%s", __func__);

	info->i2c_reg = 0;
	info->debugfs_root = debugfs_create_dir(info->miscdev.name, NULL);

	if (!info->debugfs_root)
		goto err_out;

	if (!debugfs_create_file("i2ca", S_IRUGO | S_IWUSR,
				info->debugfs_root, info, &i2ca_fops))
		goto err_out;

	if (!debugfs_create_file("i2cr", S_IRUGO,
				info->debugfs_root, info, &i2cr_fops))
		goto err_out;

	if (!debugfs_create_file("i2cw", S_IWUSR,
				info->debugfs_root, info, &i2cw_fops))
		goto err_out;

	return 0;

err_out:
	dev_err(&info->i2c_client->dev, "ERROR:%s failed", __func__);
	if (info->debugfs_root)
		debugfs_remove_recursive(info->debugfs_root);
	return -ENOMEM;
}
#endif

static int imx179_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct imx179_info *info;
	char dname[16];
	unsigned long clock_probe_rate;
	int err;
	const char *mclk_name;

	dev_dbg(&client->dev, "%s +++++\n", __func__);
	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->i2c_client = client;
	if (client->dev.platform_data) {
		info->pdata = client->dev.platform_data;
	} else {
		info->pdata = &imx179_dflt_pdata;
		dev_dbg(&client->dev,
			"%s No platform data.  Using defaults.\n", __func__);
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
	spin_lock(&imx179_spinlock);
	list_add_rcu(&info->list, &imx179_info_list);
	spin_unlock(&imx179_spinlock);
	imx179_pm_init(info);
	imx179_sdata_init(info);
	imx179_get_flash_cap(info);
	if (info->pdata->cfg & (NVC_CFG_NODEV | NVC_CFG_BOOT_INIT)) {
		if (info->pdata->probe_clock) {
			if (info->cap->initial_clock_rate_khz)
				clock_probe_rate = info->cap->
							initial_clock_rate_khz;
			else
				clock_probe_rate = imx179_dflt_cap.
							initial_clock_rate_khz;
			clock_probe_rate *= 1000;
			info->pdata->probe_clock(clock_probe_rate);
		}
		err = imx179_dev_id(info);
		if (err < 0) {
			if (info->pdata->cfg & NVC_CFG_NODEV) {
				imx179_del(info);
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
				imx179_mode_wr_full(info, info->cap->
						    preferred_mode_index);
		}
		imx179_pm_dev_wr(info, NVC_PWR_OFF);
		if (info->pdata->probe_clock)
			info->pdata->probe_clock(0);
	}

	if (info->pdata->dev_name != 0)
		strcpy(dname, info->pdata->dev_name);
	else
		strcpy(dname, "imx179");
	if (info->pdata->num)
		snprintf(dname, sizeof(dname), "%s.%u",
			 dname, info->pdata->num);
	info->miscdev.name = dname;
	info->miscdev.fops = &imx179_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
			__func__, dname);
		imx179_del(info);
		return -ENODEV;
	}

#ifdef CONFIG_DEBUG_FS
	imx179_debug_init(info);
#endif
	dev_dbg(&client->dev, "%s -----\n", __func__);
	return 0;
}

static const struct i2c_device_id imx179_id[] = {
	{ "imx179", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, imx179_id);

static struct i2c_driver imx179_i2c_driver = {
	.driver = {
		.name = "imx179",
		.owner = THIS_MODULE,
	},
	.id_table = imx179_id,
	.probe = imx179_probe,
	.remove = imx179_remove,
};

module_i2c_driver(imx179_i2c_driver);
MODULE_LICENSE("GPL v2");
