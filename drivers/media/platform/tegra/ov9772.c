/*
 * ov9772.c - ov9772 sensor driver
 *
 * Copyright (c) 2012-2014, NVIDIA Corporation. All Rights Reserved.
 *
 * Contributors:
 *	Phil Breczinski <pbreczinski@nvidia.com>
 *
 * This file is licensed under the terms of the GNU General Public License
 * version 2. This program is licensed "as is" without any warranty of any
 * kind, whether express or implied.
 */

/* Implementation
 * --------------
 * The board level details about the device are to be provided in the board
 * file with the <device>_platform_data structure.
 * Standard among NVC kernel drivers in this structure is:
 * .cfg = Use the NVC_CFG_ defines that are in nvc.h.
 *	Descriptions of the configuration options are with the defines.
 *	This value is typically 0.
 * .num = The number of the instance of the device.  This should start at 1 and
 *	and increment for each device on the board.  This number will be
 *	appended to the MISC driver name as "."%d, Example: /dev/camera.1
 *	If not used or 0, then nothing is appended to the name.
 * .sync = If there is a need to synchronize two devices, then this value is
 *	 the number of the device instance (.num above) this device is to
 *	 sync to.  For example:
 *	 Device 1 platform entries =
 *	 .num = 1,
 *	 .sync = 2,
 *	 Device 2 platfrom entries =
 *	 .num = 2,
 *	 .sync = 1,
 *	 The above example sync's device 1 and 2.
 *	 To disable sync, then .sync = 0.  Note that the .num = 0 device is
 *	 is not allowed to be synced to.
 *	 This is typically used for stereo applications.
 * .dev_name = The MISC driver name the device registers as.  If not used,
 *	     then the part number of the device is used for the driver name.
 *	     If using the NVC user driver then use the name found in this
 *	     driver under _default_pdata.
 * .gpio_count = The ARRAY_SIZE of the nvc_gpio_pdata table.
 * .gpio = A pointer to the nvc_gpio_pdata structure's platform GPIO data.
 *	 The GPIO mechanism works by cross referencing the .gpio_type key
 *	 among the nvc_gpio_pdata GPIO data and the driver's nvc_gpio_init
 *	 GPIO data to build a GPIO table the driver can use.  The GPIO's
 *	 defined in the device header file's _gpio_type enum are the
 *	 gpio_type keys for the nvc_gpio_pdata and nvc_gpio_init structures.
 *	 These need to be present in the board file's nvc_gpio_pdata
 *	 structure for the GPIO's that are used.
 *	 The driver's GPIO logic uses assert/deassert throughout until the
 *	 low level _gpio_wr/rd calls where the .assert_high is used to
 *	 convert the value to the correct signal level.
 *	 See the GPIO notes in nvc.h for additional information.
 *
 * The following is specific to NVC kernel sensor drivers:
 * .cap = Pointer to the nvc_imager_cap structure.  This structure needs to
 *	be defined and populated if overriding the driver defaults.  The
 *	driver defaults can be found at: default_<device>_pdata
 * .lens_focal_length = The lens focal length.  See note below.
 * .lens_view_angle_h = lens horizontal view angle.  See note below.
 * .lens_view_angle_v = lens vertical view angle.  See note below.
 * Note: The lens defines are suppose to be float values.  However, since the
 *       Linux kernel doesn't allow float data, these values are integers and
 *       will be divided by the NVC_IMAGER_INT2FLOAT_DIVISOR value when the
 *       data is in user space. For example, 12.3456 must be 123456
 * .clock_probe = The routine to call to turn on the sensor clock during the
 *		probe routine.  The routine should take one unsigned long
 *		parameter that is the clock frequency:
 *		(<probe_clock_routine>(unsigned long c)
 *		A value of 0 turns off the clock.
 *
 * Power Requirements
 * The board power file must contain the following labels for the power
 * regulator(s) of this device:
 * "avdd" = the power regulator for analog power.
 * "dvdd" = the power regulator for digital power.
 * "dovdd" = the power regulator for I/O power.
 *
 * NVC usage
 * ---------
 * The following is the expected usage method of the NVC architecture.
 * - OPEN: When opening the imager device, IOCTL's for capabilities
 * (NVC_IOCTL_CAPS_RD) and static data (NVC_IOCTL_STATIC_RD) are made to
 * populate the NVC user driver with this information.  The static data is data
 * specific to the imager device that doesn't change.  See the
 * static vs dynamic note below about static data that is really dynamic.
 * An IOCTL for dynamic data (NVC_IOCTL_DYNAMIC_RD) is also done to get the
 * data for the default mode.  This allows the NVC user driver to carry out
 * operations requiring dynamic data without a mode having been set yet.
 * This is accomplished by making the NVC_IOCTL_DYNAMIC_RD IOCTL with the mode
 * resolution set to 0 by 0 (x = 0, y = 0).  The default resolution will be
 * used which is determined by the preferred_mode_index member in the
 * capabilities structure.
 * To get a list of all the possible modes the device supports, the mode read
 * IOCTL is done (NVC_IOCTL_MODE_RD).
 * - OPERATION: The NVC_IOCTL_MODE_WR, NVC_IOCTL_DYNAMIC_RD, NVC_IOCTL_PWR_WR,
 *   and NVC_IOCTL_PARAM_(RD/WR) are used to operate the device.  See the
 *   summary of IOCTL usage for details.
 * - QUERY: Some user level functions request data about a specific mode.  The
 * NVC_IOCTL_DYNAMIC_RD serves this purpose without actually making the mode
 * switch.
 *
 * Summary of IOCTL usage:
 * - NVC_IOCTL_CAPS_RD: To read the capabilites of the device.  Board specific.
 * - NVC_IOCTL_MODE_WR: There are a number of functions with this:
 *   - If the entire nvc_imager_bayer structure is 0, the streaming is turned
 *     off and the device goes to standby.
 *   - If the x and y of the nvc_imager_bayer structure is set to 0, only the
 *     frame_length, coarse_time, and gain is written for the current set mode.
 *   - A fully populated nvc_imager_bayer structure sets the mode specified by
 *     the x and y.
 *   - Any invalid x and y other than 0,0 results in an error.
 * - NVC_IOCTL_MODE_RD: To read all the possible modes the device supports.
 * - NVC_IOCTL_STATIC_RD: To read the static data specific to the device.
 * - NVC_IOCTL_DYNAMIC_RD: To read the data specific to a mode specified by
 *   a valid x and y.  Setting the x and y to 0 allows reading this information
 *   for the default mode that is specified by the preferred_mode_index member
 *   in the capabilities structure.
 * - NVC_IOCTL_PWR_WR: This is a GLOS (Guaranteed Level Of Service) call.  The
 *   device will normally operate at the lowest possible power for its current
 *   use.  This GLOS call allows for minimum latencies during operation.
 * - NVC_IOCTL_PWR_RD: Reads the current GLOS setting.
 * - NVC_IOCTL_PARAM_WR: The IOCTL to set parameters.  Note that there is a
 *   separate GAIN parameter for when just the gain is changed.  If however,
 *   the gain should be changed along with the frame_length and coarse_time,
 *   the NVC_IOCTL_MODE_WR should be used instead.
 * - NVC_IOCTL_PARAM_RD: The IOCTL to read parameters.
 */

#include <linux/delay.h>
#include <linux/fs.h>
#include <linux/i2c.h>
#include <linux/clk.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/list.h>
#include <linux/regulator/consumer.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/tegra-soc.h>

#include <media/ov9772.h>
#include "nvc_utilities.h"

#ifdef CONFIG_DEBUG_FS
#include <media/nvc_debugfs.h>
#endif

#undef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

#define OV9772_ID			0x9772
#define OV9772_SENSOR_TYPE		NVC_IMAGER_TYPE_RAW
#define OV9772_STARTUP_DELAY_MS		50
#define OV9772_RES_CHG_WAIT_TIME_MS	100
#define OV9772_SIZEOF_I2C_BUF		16
#define OV9772_TABLE_WAIT_MS		0
#define OV9772_TABLE_END		1
#define OV9772_TABLE_RESET		2
#define OV9772_TABLE_RESET_TIMEOUT	50
#define OV9772_NUM_MODES_MAX	MAX(ARRAY_SIZE(ov9772_mode_table_non_fpga)\
					, ARRAY_SIZE(ov9772_mode_table_fpga))
/*#define OV9772_MODE_UNKNOWN		(OV9772_NUM_MODES + 1)*/
#define OV9772_LENS_MAX_APERTURE	0 /* / _INT2FLOAT_DIVISOR */
#define OV9772_LENS_FNUMBER		0 /* / _INT2FLOAT_DIVISOR */
#define OV9772_LENS_FOCAL_LENGTH	6120 /* / _INT2FLOAT_DIVISOR */
#define OV9772_LENS_VIEW_ANGLE_H	60000 /* / _INT2FLOAT_DIVISOR */
#define OV9772_LENS_VIEW_ANGLE_V	60000 /* / _INT2FLOAT_DIVISOR */
#define OV9772_I2C_TABLE_MAX_ENTRIES	400
#define OV9772_FUSE_ID_SIZE		5

/* comment out definition to disable mode */
#define OV9772_ENABLE_1284x724
#define OV9772_ENABLE_960x720

static u16 ov9772_ids[] = {
	0x9772,
};

static struct nvc_gpio_init ov9772_gpio[] = {
	{ OV9772_GPIO_TYPE_SHTDN, GPIOF_OUT_INIT_LOW, "shutdn", false, true, },
	{ OV9772_GPIO_TYPE_PWRDN, GPIOF_OUT_INIT_LOW, "pwrdn", false, true, },
	{ OV9772_GPIO_TYPE_I2CMUX, 0, "i2c_mux", 0, false, },
	{ OV9772_GPIO_TYPE_GP1, 0, "gp1", 0, false, },
	{ OV9772_GPIO_TYPE_GP2, 0, "gp2", 0, false, },
	{ OV9772_GPIO_TYPE_GP3, 0, "gp3", 0, false, },
};

struct ov9772_info {
	atomic_t in_use;
	struct i2c_client *i2c_client;
	struct ov9772_platform_data *pdata;
	struct nvc_imager_cap *cap;
	struct miscdevice miscdev;
	struct list_head list;
	int pwr_api;
	int pwr_dev;
	struct clk *mclk;
	struct nvc_gpio gpio[ARRAY_SIZE(ov9772_gpio)];
	struct ov9772_power_rail regulators;
	bool power_on;
	u8 s_mode;
	struct ov9772_info *s_info;
	u32 mode_index;
	bool mode_valid;
	bool mode_enable;
	unsigned test_pattern;
	struct nvc_imager_static_nvc sdata;
	u8 i2c_buf[OV9772_SIZEOF_I2C_BUF];
	u8 bin_en;
	struct nvc_fuseid fuse_id;
#ifdef CONFIG_DEBUG_FS
	struct nvc_debugfs_info debugfs_info;
#endif
	char devname[16];
};

struct ov9772_reg {
	u16 addr;
	u16 val;
};

struct ov9772_mode_data {
	struct nvc_imager_mode sensor_mode;
	struct nvc_imager_dynamic_nvc sensor_dnvc;
	struct ov9772_reg *p_mode_i2c;
};

static struct nvc_imager_cap ov9772_dflt_cap = {
	.identifier		= "OV9772",
	/* refer to NvOdmImagerSensorInterface enum in ODM nvodm_imager.h */
	.sensor_nvc_interface	= NVC_IMAGER_SENSOR_INTERFACE_SERIAL_B,
	/* refer to NvOdmImagerPixelType enum in ODM nvodm_imager.h */
	.pixel_types[0]		= 0x103,
	/* refer to NvOdmImagerOrientation enum in ODM nvodm_imager.h */
	.orientation		= 0,
	/* refer to NvOdmImagerDirection enum in ODM nvodm_imager.h */
	.direction		= 0,
	.initial_clock_rate_khz	= 6000,
	.clock_profiles[0] = {
		.external_clock_khz	= 24000,
		.clock_multiplier	= 1162020, /* value / 1,000,000 */
	},
	.clock_profiles[1] = {
		.external_clock_khz	= 0,
		.clock_multiplier	= 0,
	},
	.h_sync_edge		= 0,
	.v_sync_edge		= 0,
	.mclk_on_vgp0		= 0,
	.csi_port		= 1,
	.data_lanes		= 1,
	.virtual_channel_id	= 0,
	.discontinuous_clk_mode	= 0, /* use continuous clock */
	.cil_threshold_settle	= 0x5,
	.min_blank_time_width	= 16,
	.min_blank_time_height	= 16,
	.preferred_mode_index	= 0,
	.focuser_guid		= 0,
	.torch_guid		= 0,
	.cap_version		= NVC_IMAGER_CAPABILITIES_VERSION2,
};

static struct ov9772_platform_data ov9772_dflt_pdata = {
	.cfg			= 0,
	.num			= 0,
	.sync			= 0,
	.dev_name		= "camera",
	.cap			= &ov9772_dflt_cap,
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
static struct nvc_imager_static_nvc ov9772_dflt_sdata = {
	.api_version		= NVC_IMAGER_API_STATIC_VER,
	.sensor_type		= OV9772_SENSOR_TYPE,
	.bits_per_pixel		= 10,
	.sensor_id		= OV9772_ID,
	.sensor_id_minor	= 0,
	.focal_len		= OV9772_LENS_FOCAL_LENGTH,
	.max_aperture		= OV9772_LENS_MAX_APERTURE,
	.fnumber		= OV9772_LENS_FNUMBER,
	.view_angle_h		= OV9772_LENS_VIEW_ANGLE_H,
	.view_angle_v		= OV9772_LENS_VIEW_ANGLE_V,
	.res_chg_wait_time	= OV9772_RES_CHG_WAIT_TIME_MS,
};

static LIST_HEAD(ov9772_info_list);
static DEFINE_SPINLOCK(ov9772_spinlock);


static struct ov9772_reg tp_none_seq[] = {
	{0x5046, 0x00},
	{OV9772_TABLE_END, 0x0000}
};

static struct ov9772_reg tp_cbars_seq[] = {
	{0x0601, 0x01},
	{0x5100, 0x80},
	{0x503D, 0xC0},
	{0x503E, 0x00},
	{0x5046, 0x01},
	{OV9772_TABLE_END, 0x0000}
};

static struct ov9772_reg tp_checker_seq[] = {
	{0x503D, 0xC0},
	{0x503E, 0x0A},
	{0x5046, 0x01},
	{OV9772_TABLE_END, 0x0000}
};

static struct ov9772_reg *test_patterns[] = {
	tp_none_seq,
	tp_cbars_seq,
	tp_checker_seq,
};

static struct ov9772_reg ov9772_1280x720_i2c[] = {
	{OV9772_TABLE_RESET, 0},
	{OV9772_TABLE_WAIT_MS, 100},
	{0x0200, 0x00},
	{0x0201, 0x00},
	{0x0301, 0x0a},
	{0x0303, 0x08},
	{0x0305, 0x02},
	{0x0307, 0x20},
	{0x0340, 0x02},
	{0x0341, 0xf8},
	{0x0342, 0x06},
	{0x0343, 0x2a},
	{0x034c, 0x05},
	{0x034d, 0x00},
	{0x034e, 0x02},
	{0x034f, 0xd0},
	{0x300c, 0x22},
	{0x300d, 0x1e},
	{0x300e, 0xc2},
	{0x3010, 0x81},
	{0x3012, 0x70},
	{0x3014, 0x0d},
	{0x3022, 0x20},
	{0x3025, 0x03},
	{0x303c, 0x23},
	{0x3103, 0x00},
	{0x3104, 0x04},
	{0x3503, 0x14},
	{0x3602, 0xc0},
	{0x3611, 0x10},
	{0x3613, 0x83},
	{0x3620, 0x24},
	{0x3622, 0x2c},
	{0x3631, 0xc2},
	{0x3634, 0x04},
	{0x3708, 0x24},
	{0x3709, 0x10},
	{0x370e, 0x00},
	{0x371b, 0x60},
	{0x3724, 0x1c},
	{0x372c, 0x00},
	{0x372d, 0x00},
	{0x3745, 0x00},
	{0x3746, 0x18},
	{0x0601, 0x00},
	{0x0101, 0x00},
	{0x3811, 0x0e},
	{0x3813, 0x08},
	{0x3a0c, 0x20},
	{0x3b01, 0x32},
	{0x3b02, 0xa4},
	{0x3c00, 0x00},
	{0x3f00, 0x2a},
	{0x3f01, 0x8c},
	{0x3f0f, 0xf5},
	{0x4000, 0x07},
	{0x4001, 0x02},
	{0x460e, 0xb1},
	{0x4800, 0x44},
	{0x4801, 0x0f},
	{0x4805, 0x10},
	{0x4815, 0x00},
	{0x4837, 0x36},
	{0x5000, 0x06},
	{0x5001, 0x31},
	{0x5005, 0x08},
	{0x5100, 0x00},
	{0x5310, 0x01},
	{0x5311, 0xff},
	{0x53b9, 0x0f},
	{0x53ba, 0x04},
	{0x53bb, 0x4a},
	{0x53bc, 0xd3},
	{0x53bd, 0x41},
	{0x53be, 0x00},
	{0x53c4, 0x03},

	{0x0100, 0x01},

	{OV9772_TABLE_END, 0x0000}
};

#ifdef OV9772_ENABLE_1284x724
static struct ov9772_reg ov9772_1284x724_i2c[] = {
	{OV9772_TABLE_RESET, 0},
	{0x0300, 0x00},
	{0x0301, 0x0a},
	{0x0302, 0x00},
	{0x0303, 0x02},
	{0x0304, 0x00},
	{0x0305, 0x02},
	{0x0306, 0x00},
	{0x0307, 0x3c},
	{0x0340, 0x02},
	{0x0341, 0xf8},
	{0x0342, 0x06},
	{0x0343, 0x2a},
	{0x0344, 0x00},
	{0x0345, 0x00},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x04},
	{0x0349, 0xff},
	{0x034a, 0x02},
	{0x034b, 0xcf},
	{0x034c, 0x05},
	{0x034d, 0x04},
	{0x034e, 0x02},
	{0x034f, 0xd4},
	{0x300c, 0x22},
	{0x300d, 0x1e},
	{0x300e, 0xc2},
	{0x3010, 0x81},
	{0x3012, 0x70},
	{0x3014, 0x0d},
	{0x3022, 0x20},
	{0x3025, 0x03},
	{0x303c, 0x23},
	{0x3103, 0x01},
	{0x3104, 0x00},
	{0x3503, 0x33},
	{0x3602, 0xc0},
	{0x3611, 0x10},
	{0x3613, 0x83},
	{0x3620, 0x24},
	{0x3622, 0x2c},
	{0x3631, 0xc2},
	{0x3634, 0x04},
	{0x3708, 0x24},
	{0x3709, 0x10},
	{0x370e, 0x00},
	{0x371b, 0x60},
	{0x3724, 0x1c},
	{0x372c, 0x00},
	{0x372d, 0x00},
	{0x3745, 0x00},
	{0x3746, 0x18},
	{0x3811, 0x0e},
	{0x3813, 0x08},
	{0x3a0c, 0x20},
	{0x3b01, 0x32},
	{0x3b02, 0xa4},
	{0x3c00, 0x00},
	{0x3f00, 0x2a},
	{0x3f01, 0x8c},
	{0x3f0f, 0xf5},
	{0x4000, 0x07},
	{0x4001, 0x02},
	{0x4002, 0x45},
	{0x460e, 0xb1},
	{0x4800, 0x44},
	{0x4801, 0x0f},
	{0x4805, 0x10},
	{0x4815, 0x00},
	{0x4837, 0x36},
	{0x5000, 0x06},
	{0x5001, 0x31},
	{0x5005, 0x08},
	{0x5100, 0x00},
	{0x5310, 0x01},
	{0x5311, 0xff},
	{0x53b9, 0x0f},
	{0x53ba, 0x04},
	{0x53bb, 0x4a},
	{0x53bc, 0xd3},
	{0x53bd, 0x41},
	{0x53be, 0x00},
	{0x53c4, 0x03},
	{0x301c, 0xf0},
	{0x404f, 0x8f},
	{0x0101, 0x01},
	{0x0100, 0x01},
	{OV9772_TABLE_END, 0x0000}
};
#endif
#ifdef OV9772_ENABLE_960x720
static struct ov9772_reg ov9772_960x720_i2c[] = {
	{OV9772_TABLE_RESET, 0},
	{0x3745, 0x00},
	{0x3746, 0x18},
	{0x3620, 0x36},
	{0x3622, 0x24},
	{0x3022, 0x20},
	{0x3631, 0xc2},
	{0x371b, 0x60},
	{0x3634, 0x04},
	{0x3613, 0x83},
	{0x4837, 0x36},
	{0x4805, 0x10},
	{0x3724, 0x1c},
	{0x0300, 0x00},
	{0x0301, 0x0a},
	{0x0302, 0x00},
	{0x0303, 0x02},
	{0x0304, 0x00},
	{0x0305, 0x02},
	{0x0306, 0x00},
	{0x0307, 0x3c},
	{0x303c, 0x23},
	{0x4001, 0x02},
	{0x0200, 0x00},
	{0x0201, 0x00},
	{0x372c, 0x00},
	{0x372d, 0x00},
	{0x5005, 0x08},
	{0x3a0c, 0x20},
	{0x5310, 0x01},
	{0x5311, 0xff},
	{0x53b9, 0x0f},
	{0x53ba, 0x04},
	{0x53bb, 0x4a},
	{0x53bc, 0xd3},
	{0x53bd, 0x41},
	{0x53be, 0x00},
	{0x53c4, 0x03},
	{0x3602, 0xc0},
	{0x3611, 0x10},
	{0x3c00, 0x00},
	{0x370e, 0x00},
	{0x0344, 0x00},
	{0x0345, 0xA0},
	{0x0346, 0x00},
	{0x0347, 0x00},
	{0x0348, 0x04},
	{0x0349, 0x60},
	{0x034a, 0x02},
	{0x034b, 0xd0},
	{0x034c, 0x03},
	{0x034d, 0xc0},
	{0x034e, 0x02},
	{0x034f, 0xd0},
	{0x0340, 0x02},
	{0x0341, 0xf8},
	{0x0342, 0x06},
	{0x0343, 0x2a},
	{0x3a14, 0x15},
	{0x3a15, 0x60},
	{0x3a08, 0x00},
	{0x3a09, 0xe4},
	{0x3a0e, 0x03},
	{0x3a02, 0x17},
	{0x3a03, 0xc0},
	{0x3a0a, 0x00},
	{0x3a0b, 0xbe},
	{0x3a0d, 0x04},
	{0x0303, 0x02},
	{0x0601, 0x00},
	{0x3b01, 0x32},
	{0x3b02, 0xa4},
	{0x3f00, 0x2a},
	{0x3f01, 0x8c},
	{0x3f0f, 0xf5},
	{0x4801, 0x0f},
	{0x3012, 0x70},
	{0x3014, 0x0d},
	{0x3025, 0x03},
	{0x4815, 0x00},
	{0x0307, 0x3c},
	{0x0301, 0x0a},
	{0x0101, 0x01},
	{0x3708, 0x24},
	{0x3709, 0x10},
	{0x5000, 0x06},
	{0x5001, 0x31},
	{0x5100, 0x00},
	{0x3503, 0x33},
	{0x5001, 0x31},
	{0x4002, 0x45},
	{0x0345, 0x01},
	{0x4000, 0x07},
	{0x3610, 0xc0},
	{0x3613, 0x82},
	{0x3631, 0xe2},
	{0x3634, 0x03},
	{0x373c, 0x08},
	{0x3a18, 0x00},
	{0x3a19, 0x7f},
	{0x373b, 0x01},
	{0x373c, 0x08},
	{0x0345, 0xa1},
	{0x301c, 0xf0},
	{0x404f, 0x8f},
	{0x0100, 0x01},
	{OV9772_TABLE_END, 0x0000}
};
#endif
/* Each resolution requires the below data table setup and the corresponding
 * I2C data table.
 * If more NVC data is needed for the NVC driver, be sure and modify the
 * nvc_imager_nvc structure in nvc_imager.h
 * If more data sets are needed per resolution, they can be added to the
 * table format below with the ov9772_mode_data structure.  New data sets
 * should conform to an already defined NVC structure.  If it's data for the
 * NVC driver, then it should be added to the nvc_imager_nvc structure.
 * Steps to add a resolution:
 * 1. Add I2C data table
 * 2. Add ov9772_mode_data table
 * 3. Add entry to the ov9772_mode_table_non_fpga and/or ov9772_mode_table_fpga
 */
#ifdef OV9772_ENABLE_1284x724
static struct ov9772_mode_data ov9772_1284x724 = {
	.sensor_mode = {
		.res_x			= 1280,
		.res_y			= 720,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 18000, /* / _INT2FLOAT_DIVISOR */
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
		.coarse_time		= 754,
		.max_coarse_diff	= 6,
		.min_exposure_course	= 3,
		.max_exposure_course	= 0xFFF7,
		.diff_integration_time	= 230, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 1578,
		.frame_length		= 760,
		.min_frame_length	= 760,
		.max_frame_length	= 0xFFFC,
		.min_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 15500, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 60,
		.pll_div		= 4,
		.mode_sw_wait_frames	= 1500, /* / _INT2FLOAT_DIVISOR */
	},
	.p_mode_i2c			= ov9772_1284x724_i2c,
};
#endif
#ifdef OV9772_ENABLE_960x720
static struct ov9772_mode_data ov9772_960x720 = {
	.sensor_mode = {
		.res_x			= 960,
		.res_y			= 720,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 18000, /* / _INT2FLOAT_DIVISOR */
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
		.coarse_time		= 754,
		.max_coarse_diff	= 6,
		.min_exposure_course	= 3,
		.max_exposure_course	= 0xFFF7,
		.diff_integration_time	= 230, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 1578,
		.frame_length		= 760,
		.min_frame_length	= 760,
		.max_frame_length	= 0xFFFC,
		.min_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 15500, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 60,
		.pll_div		= 4,
		.mode_sw_wait_frames	= 1500, /* / _INT2FLOAT_DIVISOR */
	},
	.p_mode_i2c			= ov9772_960x720_i2c,
};
#endif

static struct ov9772_mode_data ov9772_1280x720 = {
	.sensor_mode = {
		.res_x			= 1280,
		.res_y			= 720,
		.active_start_x		= 0,
		.active_stary_y		= 0,
		.peak_frame_rate	= 30000, /* / _INT2FLOAT_DIVISOR */
		.pixel_aspect_ratio	= 1000, /* / _INT2FLOAT_DIVISOR */
		.pll_multiplier		= 18000, /* / _INT2FLOAT_DIVISOR */
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
		.coarse_time		= 754,
		.max_coarse_diff	= 6,
		.min_exposure_course	= 3,
		.max_exposure_course	= 0xFFF7,
		.diff_integration_time	= 230, /* / _INT2FLOAT_DIVISOR */
		.line_length		= 1578,
		.frame_length		= 760,
		.min_frame_length	= 760,
		.max_frame_length	= 0xFFFC,
		.min_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.max_gain		= 15500, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain		= 1000, /* / _INT2FLOAT_DIVISOR */
		.inherent_gain_bin_en	= 1000, /* / _INT2FLOAT_DIVISOR */
		.support_bin_control	= 0,
		.support_fast_mode	= 0,
		.pll_mult		= 60,
		.pll_div		= 4,
		.mode_sw_wait_frames	= 1500, /* / _INT2FLOAT_DIVISOR */
	},
	.p_mode_i2c			= ov9772_1280x720_i2c,
};

static struct ov9772_mode_data **ov9772_mode_table;

static unsigned int ov9772_num_modes;

static struct ov9772_mode_data *ov9772_mode_table_non_fpga[] = {
	[0] =
#ifdef OV9772_ENABLE_1284x724
	&ov9772_1284x724,
#endif
#ifdef OV9772_ENABLE_960x720
	&ov9772_960x720,
#endif
};

static struct ov9772_mode_data *ov9772_mode_table_fpga[] = {
	[0] =
#ifdef OV9772_ENABLE_1284x724
	&ov9772_1284x724,
#endif
#ifdef OV9772_ENABLE_960x720
	&ov9772_960x720,
#endif
	&ov9772_1280x720,
};

static int ov9772_i2c_rd8(struct i2c_client *client, u16 reg, u8 *val)
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

static int ov9772_i2c_rd16(struct i2c_client *client, u16 reg, u16 *val)
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

static int ov9772_i2c_wr8(struct i2c_client *client, u16 reg, u8 val)
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

static int ov9772_i2c_rd_table(struct ov9772_info *info,
				struct ov9772_reg table[])
{
	struct ov9772_reg *p_table = table;
	u8 val;
	int err = 0;

	while (p_table->addr != OV9772_TABLE_END) {
		err = ov9772_i2c_rd8(info->i2c_client, p_table->addr, &val);
		if (err)
			return err;

		p_table->val = (u16)val;
		p_table++;
	}

	return err;
}

static int ov9772_i2c_wr_blk(struct ov9772_info *info, u8 *buf, int len)
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

static int ov9772_i2c_wr_table(struct ov9772_info *info,
				struct ov9772_reg table[])
{
	int err;
	const struct ov9772_reg *next;
	const struct ov9772_reg *n_next;
	u8 *b_ptr = info->i2c_buf;
	u16 buf_count = 0;
	u8 reset_status = 1;
	u8 reset_tries_left = OV9772_TABLE_RESET_TIMEOUT;

	for (next = table; next->addr != OV9772_TABLE_END; next++) {
		if (next->addr == OV9772_TABLE_WAIT_MS) {
			msleep(next->val);
			continue;
		} else if (next->addr == OV9772_TABLE_RESET) {
			err = ov9772_i2c_wr8(info->i2c_client, 0x0103, 0x01);
			if (err)
				return err;
			while (reset_status) {
				usleep_range(200, 300);
				if (reset_tries_left < 1)
					return -EIO;
				err = ov9772_i2c_rd8(info->i2c_client, 0x0103,
							&reset_status);
				if (err)
					return err;
				reset_status &= 0x01;
				reset_tries_left -= 1;
			}
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
				n_next->addr != OV9772_TABLE_WAIT_MS &&
				buf_count < OV9772_SIZEOF_I2C_BUF &&
				n_next->addr != OV9772_TABLE_RESET &&
				n_next->addr != OV9772_TABLE_END)
			continue;

		err = ov9772_i2c_wr_blk(info, info->i2c_buf, buf_count);
		if (err)
			return err;

		buf_count = 0;
	}

	return 0;
}


static inline void ov9772_frame_length_reg(struct ov9772_reg *regs,
					u32 frame_length)
{
	regs->addr = 0x340;
	regs->val = (frame_length >> 8) & 0xff;
	(regs + 1)->addr = 0x341;
	(regs + 1)->val = (frame_length) & 0xff;
}

static inline void ov9772_coarse_time_reg(struct ov9772_reg *regs,
					u32 coarse_time)
{
	regs->addr = 0x202;
	regs->val = (coarse_time >> 8) & 0xFF;
	(regs + 1)->addr = 0x203;
	(regs + 1)->val = coarse_time & 0xFF;
}

static inline void ov9772_gain_reg(struct ov9772_reg *regs, u32 gain)
{
	(regs)->addr = 0x205;
	(regs)->val = gain & 0x7F;
}

static int ov9772_bin_wr(struct ov9772_info *info, u8 enable)
{
	int err = 0;

	if (enable == info->bin_en)
		return 0;

	if (!info->mode_valid || !ov9772_mode_table[info->mode_index]->
				sensor_dnvc.support_bin_control)
		return -EINVAL;

	if (!err)
		info->bin_en = enable;
	dev_dbg(&info->i2c_client->dev, "%s bin_en=%x err=%d\n",
		__func__, info->bin_en, err);
	return err;
}

static int ov9772_exposure_wr(struct ov9772_info *info,
				struct nvc_imager_bayer *mode)
{
	struct ov9772_reg reg_list[9];
	int err;

	reg_list[0].addr = 0x3208;
	reg_list[0].val = 0x01;
	ov9772_frame_length_reg(reg_list + 1, mode->frame_length);
	ov9772_coarse_time_reg(reg_list + 3, mode->coarse_time);
	ov9772_gain_reg(reg_list + 5, mode->gain);
	reg_list[6].addr = 0x3208;
	reg_list[6].val = 0x11;
	reg_list[7].addr = 0x3208;
	reg_list[7].val = 0xe1;
	reg_list[8].addr = OV9772_TABLE_END;
	err = ov9772_i2c_wr_table(info, reg_list);
	if (!err)
		err |= ov9772_bin_wr(info, mode->bin_en);
	return err;
}

static int ov9772_gain_wr(struct ov9772_info *info, u32 gain)
{
	int err;
	err = ov9772_i2c_wr8(info->i2c_client, 0x205, (u8)(gain & 0x7F));
	return err;
}

static int ov9772_gain_rd(struct ov9772_info *info, u32 *gain)
{
	int err;

	*gain = 0;
	err = ov9772_i2c_rd16(info->i2c_client, 0x204, (u16 *)gain);
	return err;
}

static int ov9772_group_hold_wr(struct ov9772_info *info,
				struct nvc_imager_ae *ae)
{
	int err = 0;
	bool groupHoldEnable;
	struct ov9772_reg reg_list[6];
	int count = 0;
	int offset = 0;

	if (ae->gain_enable)
		count += 1;
	if (ae->coarse_time_enable)
		count += 1;
	if (ae->frame_length_enable)
		count += 1;
	groupHoldEnable = (count > 1) ? 1 : 0;

	if (groupHoldEnable)
		err |= ov9772_i2c_wr8(info->i2c_client, 0x3208, 0x01);

	if (ae->gain_enable) {
		ov9772_gain_reg(reg_list + offset, ae->gain);
		offset += 1;
	}
	if (ae->frame_length_enable) {
		ov9772_frame_length_reg(reg_list + offset, ae->frame_length);
		offset += 2;
	}
	if (ae->coarse_time_enable) {
		ov9772_coarse_time_reg(reg_list + offset, ae->coarse_time);
		offset += 2;
	}
	reg_list[offset].addr = OV9772_TABLE_END;
	err |= ov9772_i2c_wr_table(info, reg_list);

	if (groupHoldEnable) {
		err |= ov9772_i2c_wr8(info->i2c_client, 0x3208, 0x11);
		err |= ov9772_i2c_wr8(info->i2c_client, 0x3208, 0xe1);
	}

	return err;
}

static int ov9772_test_pattern_wr(struct ov9772_info *info, unsigned pattern)
{
	if (pattern >= ARRAY_SIZE(test_patterns))
		return -EINVAL;

	return ov9772_i2c_wr_table(info, test_patterns[pattern]);
}

static int ov9772_gpio_rd(struct ov9772_info *info,
			enum ov9772_gpio_type type)
{
	int val = -EINVAL;

	if (info->gpio[type].gpio) {
		val = gpio_get_value_cansleep(info->gpio[type].gpio);
		dev_dbg(&info->i2c_client->dev, "%s %u %d\n",
			__func__, info->gpio[type].gpio, val);
		if (!info->gpio[type].active_high)
			val = !val;
		val &= 1;
	}
	return val; /* return read value or error */
}

static int ov9772_gpio_wr(struct ov9772_info *info,
			enum ov9772_gpio_type type,
			int val) /* val: 0=deassert, 1=assert */
{
	int err = -EINVAL;

	if (info->gpio[type].gpio) {
		if (!info->gpio[type].active_high)
			val = !val;
		val &= 1;
		err = val;
		gpio_set_value_cansleep(info->gpio[type].gpio, val);
		dev_dbg(&info->i2c_client->dev, "%s %u %d\n",
			__func__, info->gpio[type].gpio, val);
	}
	return err; /* return value written or error */
}

static void ov9772_gpio_shutdn(struct ov9772_info *info, int val)
{
	ov9772_gpio_wr(info, OV9772_GPIO_TYPE_SHTDN, val);
}

static void ov9772_gpio_pwrdn(struct ov9772_info *info, int val)
{
	int prev_val;

	prev_val = ov9772_gpio_rd(info, OV9772_GPIO_TYPE_PWRDN);
	if ((prev_val < 0) || (val == prev_val))
		return;

	ov9772_gpio_wr(info, OV9772_GPIO_TYPE_PWRDN, val);
	if (!val && prev_val)
		/* if transition from assert to deassert then delay for I2C */
		msleep(OV9772_STARTUP_DELAY_MS);
}

static void ov9772_gpio_able(struct ov9772_info *info, int val)
{
	ov9772_gpio_wr(info, OV9772_GPIO_TYPE_GP1, val);
	ov9772_gpio_wr(info, OV9772_GPIO_TYPE_GP2, val);
	ov9772_gpio_wr(info, OV9772_GPIO_TYPE_GP3, val);
}

static void ov9772_gpio_exit(struct ov9772_info *info)
{
	unsigned i;

	for (i = 0; i < ARRAY_SIZE(ov9772_gpio); i++) {
		if (info->gpio[i].gpio && info->gpio[i].own)
			gpio_free(info->gpio[i].gpio);
	}
}

static void ov9772_gpio_init(struct ov9772_info *info)
{
	char label[32];
	unsigned long flags;
	unsigned type;
	unsigned i;
	unsigned j;
	int err;

	if (!info->pdata->gpio_count || !info->pdata->gpio)
		return;

	for (i = 0; i < ARRAY_SIZE(ov9772_gpio); i++) {
		type = ov9772_gpio[i].gpio_type;
		for (j = 0; j < info->pdata->gpio_count; j++) {
			if (type == info->pdata->gpio[j].gpio_type)
				break;
		}
		if (j == info->pdata->gpio_count)
			continue;

		info->gpio[type].gpio = info->pdata->gpio[j].gpio;
		if (ov9772_gpio[i].use_flags) {
			flags = ov9772_gpio[i].flags;
			info->gpio[type].active_high =
						ov9772_gpio[i].active_high;
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

		snprintf(label, sizeof(label), "ov9772_%u_%s",
			 info->pdata->num, ov9772_gpio[i].label);
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

static int ov9772_power_off(struct ov9772_info *info)
{
	struct ov9772_power_rail *pw = &info->regulators;
	int err = 0;

	if (!info->power_on)
		goto ov9772_poweroff_skip;

	dev_dbg(&info->i2c_client->dev, "%s: disable MCLK\n", __func__);
	clk_disable_unprepare(info->mclk);

	if (info->pdata && info->pdata->power_off)
		err = info->pdata->power_off(pw);
	/* if customized design handles the power off process specifically,
	* return is bigger than 0 (normally 1), otherwise 0 or error num.
	*/
	if (err > 0) {
		info->power_on = false;
		return 0;
	}

	if (!err) {
		ov9772_gpio_pwrdn(info, 1);
		ov9772_gpio_shutdn(info, 1);
		ov9772_gpio_able(info, 0);
		if (pw->avdd)
			WARN_ON(IS_ERR_VALUE(
				err = regulator_disable(pw->avdd)));
		if (pw->dvdd)
			WARN_ON(IS_ERR_VALUE(
				err |= regulator_disable(pw->dvdd)));
		if (pw->dovdd)
			WARN_ON(IS_ERR_VALUE(
				err |= regulator_disable(pw->dovdd)));
		if (pw->afvdd)
			WARN_ON(IS_ERR_VALUE(
				err |= regulator_disable(pw->afvdd)));
	}

	if (!err)
		info->power_on = false;

ov9772_poweroff_skip:
	return err;
}

static int ov9772_power_on(struct ov9772_info *info, bool standby)
{
	struct ov9772_power_rail *pw = &info->regulators;
	int err = 0;
	unsigned long mclk_init_rate;

	if (info->power_on)
		goto ov9772_poweron_skip;

	mclk_init_rate = nvc_imager_get_mclk(info->cap, &ov9772_dflt_cap, 0);
	dev_dbg(&info->i2c_client->dev, "%s: enable MCLK with %lu Hz\n",
		__func__, mclk_init_rate);
	err = clk_set_rate(info->mclk, mclk_init_rate);
	if (!err)
		err = clk_prepare_enable(info->mclk);
	if (err)
		goto ov9772_poweron_fail;

	if (info->pdata && info->pdata->power_on)
		err = info->pdata->power_on(pw);
	/* if customized design handles the power on process specifically,
	* return is bigger than 0 (normally 1), otherwise 0 or error num.
	*/
	if (!err) {
		if (pw->dvdd)
			WARN_ON(IS_ERR_VALUE(
				err = regulator_enable(pw->dvdd)));
		if (pw->avdd)
			WARN_ON(IS_ERR_VALUE(
				err |= regulator_enable(pw->avdd)));
		if (pw->dovdd)
			WARN_ON(IS_ERR_VALUE(
				err |= regulator_enable(pw->dovdd)));
		if (pw->afvdd)
			WARN_ON(IS_ERR_VALUE(
				err |= regulator_enable(pw->afvdd)));

		ov9772_gpio_able(info, 1);
		ov9772_gpio_shutdn(info, 0);
		ov9772_gpio_pwrdn(info, 0); /* PWRDN off to access I2C */
	}
	if (IS_ERR_VALUE(err))
		goto ov9772_poweron_seq_fail;
	info->power_on = true;
	err = 0;

ov9772_poweron_skip:
	if (standby) {
		/*avoid GPIO leak */
		err |= ov9772_i2c_wr8(info->i2c_client, 0x3002, 0x18);
		ov9772_gpio_pwrdn(info, 1); /* PWRDN on for standby */
	} else {
		err |= ov9772_i2c_wr8(info->i2c_client, 0x3002, 0x19);
		/* out of standby */
		err |= ov9772_i2c_wr8(info->i2c_client, 0x3025, 0x00);
		/* out of standby */
		err |= ov9772_i2c_wr8(info->i2c_client, 0x4815, 0x20);
	}

	return err;

ov9772_poweron_seq_fail:
	clk_disable_unprepare(info->mclk);
ov9772_poweron_fail:
	pr_err("%s FAILED\n", __func__);
	return err;
}

static int ov9772_pm_wr(struct ov9772_info *info, int pwr)
{
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
		err = ov9772_power_off(info);
		info->mode_valid = false;
		info->bin_en = 0;
		break;

	case NVC_PWR_STDBY:
		err = ov9772_power_on(info, true);
		break;

	case NVC_PWR_COMM:
	case NVC_PWR_ON:
		err = ov9772_power_on(info, false);
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

static int ov9772_pm_wr_s(struct ov9772_info *info, int pwr)
{
	int err1 = 0;
	int err2 = 0;

	if ((info->s_mode == NVC_SYNC_OFF) ||
			(info->s_mode == NVC_SYNC_MASTER) ||
			(info->s_mode == NVC_SYNC_STEREO))
		err1 = ov9772_pm_wr(info, pwr);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
			(info->s_mode == NVC_SYNC_STEREO))
		err2 = ov9772_pm_wr(info->s_info, pwr);
	return err1 | err2;
}

static int ov9772_pm_api_wr(struct ov9772_info *info, int pwr)
{
	int err = 0;

	if (!pwr || (pwr > NVC_PWR_ON))
		return 0;

	if (pwr > info->pwr_dev)
		err = ov9772_pm_wr_s(info, pwr);
	if (!err)
		info->pwr_api = pwr;
	else
		info->pwr_api = NVC_PWR_ERR;
	if (info->pdata->cfg & NVC_CFG_NOERR)
		return 0;

	return err;
}

static int ov9772_pm_dev_wr(struct ov9772_info *info, int pwr)
{
	if (info->mode_enable)
		pwr = NVC_PWR_ON;
	if (pwr < info->pwr_api)
		pwr = info->pwr_api;
	return ov9772_pm_wr(info, pwr);
}

static void ov9772_pm_exit(struct ov9772_info *info)
{
	struct ov9772_power_rail *pw = &info->regulators;

	ov9772_pm_wr(info, NVC_PWR_OFF_FORCE);

	if (pw->avdd)
		regulator_put(pw->avdd);
	if (pw->dvdd)
		regulator_put(pw->dvdd);
	if (pw->dovdd)
		regulator_put(pw->dovdd);
	if (pw->afvdd)
		regulator_put(pw->afvdd);

	pw->avdd = NULL;
	pw->dvdd = NULL;
	pw->dovdd = NULL;
	pw->afvdd = NULL;

	ov9772_gpio_exit(info);
}

static int ov9772_regulator_get(
	struct ov9772_info *info, struct regulator **vreg, char vreg_name[])
{
	struct regulator *reg = NULL;
	int err = 0;

	reg = regulator_get(&info->i2c_client->dev, vreg_name);
	if (IS_ERR(reg)) {
		dev_err(&info->i2c_client->dev, "%s %s ERR: %d\n",
			__func__, vreg_name, (int)reg);
		err = PTR_ERR(reg);
		reg = NULL;
	} else
		dev_dbg(&info->i2c_client->dev, "%s: %s\n",
			__func__, vreg_name);

	*vreg = reg;
	return err;
}

static void ov9772_pm_init(struct ov9772_info *info)
{
	struct ov9772_power_rail *pw = &info->regulators;

	ov9772_gpio_init(info);

	ov9772_regulator_get(info, &pw->avdd, "avdd");
	ov9772_regulator_get(info, &pw->dvdd, "dvdd");
	ov9772_regulator_get(info, &pw->dovdd, "dovdd");
	ov9772_regulator_get(info, &pw->afvdd, "vdd_af_cam1");

	info->power_on = false;
}

static int ov9772_reset(struct ov9772_info *info, int level)
{
	int err;

	if (level == NVC_RESET_SOFT) {
		err = ov9772_pm_wr(info, NVC_PWR_COMM);
		/* SW reset */
		err |= ov9772_i2c_wr8(info->i2c_client, 0x0103, 0x01);
	} else
		err = ov9772_pm_wr(info, NVC_PWR_OFF_FORCE);
	err |= ov9772_pm_wr(info, info->pwr_api);
	return err;
}

static int ov9772_dev_id(struct ov9772_info *info)
{
	u16 val = 0;
	unsigned i;
	int err;

	ov9772_pm_dev_wr(info, NVC_PWR_COMM);
	err = ov9772_i2c_rd16(info->i2c_client, 0x0000, &val);
	if (!err) {
		dev_dbg(&info->i2c_client->dev, "%s found devId: %x\n",
			__func__, val);
		info->sdata.sensor_id_minor = 0;
		for (i = 0; i < ARRAY_SIZE(ov9772_ids); i++) {
			if (val == ov9772_ids[i]) {
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
	ov9772_pm_dev_wr(info, NVC_PWR_OFF);
	return err;
}

static int ov9772_mode_enable(struct ov9772_info *info, bool mode_enable)
{
	u8 val;
	int err;

	if (mode_enable)
		val = 0x01;
	else
		val = 0x00;
	err = ov9772_i2c_wr8(info->i2c_client, 0x0100, val);
	if (!err) {
		info->mode_enable = mode_enable;
		dev_dbg(&info->i2c_client->dev, "%s streaming=%x\n",
			__func__, info->mode_enable);
		if (!mode_enable)
			ov9772_pm_dev_wr(info, NVC_PWR_STDBY);
	}
	return err;
}

static int ov9772_mode_rd(struct ov9772_info *info,
			s32 res_x,
			s32 res_y,
			u32 *index)
{
	int i;

	if (!res_x && !res_y) {
		*index = info->cap->preferred_mode_index;
		return 0;
	}

	for (i = 0; i < ov9772_num_modes; i++) {
		if ((res_x == ov9772_mode_table[i]->sensor_mode.res_x) &&
		   (res_y == ov9772_mode_table[i]->sensor_mode.res_y)) {
			break;
		}
	}

	if (i == ov9772_num_modes) {
		dev_err(&info->i2c_client->dev,
			"%s invalid resolution: %dx%d\n",
			__func__, res_x, res_y);
		return -EINVAL;
	}

	*index = i;
	return 0;
}

static int ov9772_mode_wr_full(struct ov9772_info *info, u32 mode_index)
{
	int err;

	ov9772_pm_dev_wr(info, NVC_PWR_ON);
	ov9772_bin_wr(info, 0);
	err = ov9772_i2c_wr_table(info,
				ov9772_mode_table[mode_index]->p_mode_i2c);
	if (!err) {
		info->mode_index = mode_index;
		info->mode_valid = true;
	} else {
		info->mode_valid = false;
	}
	return err;
}

static int ov9772_mode_wr(struct ov9772_info *info,
			struct nvc_imager_bayer *mode)
{
	u32 mode_index;
	int err;

#ifdef OV9772_REGISTER_DUMP
	int i;
	__u8 buf;
	__u16 bufarray[2][6];
	int col;
#endif

	err = ov9772_mode_rd(info, mode->res_x, mode->res_y, &mode_index);
	if (err < 0)
		return err;

	if (!mode->res_x && !mode->res_y) {
		if (mode->frame_length || mode->coarse_time || mode->gain) {
			/* write exposure only */
			err = ov9772_exposure_wr(info, mode);
			return err;
		} else {
			/* turn off streaming */
			err = ov9772_mode_enable(info, false);
			return err;
		}
	}

	if (!info->mode_valid || (info->mode_index != mode_index))
		err = ov9772_mode_wr_full(info, mode_index);
	else
		dev_dbg(&info->i2c_client->dev, "%s short mode\n", __func__);
	err |= ov9772_exposure_wr(info, mode);
	if (err < 0) {
		info->mode_valid = false;
		goto ov9772_mode_wr_err;
	}

	err = ov9772_mode_enable(info, true);
	if (err < 0)
		goto ov9772_mode_wr_err;

	return 0;

ov9772_mode_wr_err:
	if (!info->mode_enable)
		ov9772_pm_dev_wr(info, NVC_PWR_OFF);
	return err;
}


static int ov9772_param_rd(struct ov9772_info *info, unsigned long arg)
{
	struct nvc_param params;
	struct ov9772_reg *p_i2c_table;
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
		ov9772_pm_dev_wr(info, NVC_PWR_COMM);
		err = ov9772_gain_rd(info, &u32val);
		ov9772_pm_dev_wr(info, NVC_PWR_OFF);
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
		err = ov9772_dev_id(info);
		dev_dbg(&info->i2c_client->dev, "%s STS: %d\n",
			__func__, err);
		data_ptr = &err;
		data_size = sizeof(err);
		break;

	case NVC_PARAM_DEV_ID:
		if (!info->sdata.sensor_id_minor)
			ov9772_dev_id(info);
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
		if (params.sizeofvalue > OV9772_I2C_TABLE_MAX_ENTRIES) {
			pr_err("%s: requested size too large\n", __func__);
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

		ov9772_pm_dev_wr(info, NVC_PWR_COMM);
		err = ov9772_i2c_rd_table(info, p_i2c_table);
		ov9772_pm_dev_wr(info, NVC_PWR_OFF);
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
		dev_err(&info->i2c_client->dev,
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

static int ov9772_param_wr_s(struct ov9772_info *info,
			struct nvc_param *params,
			u32 u32val)
{
	struct ov9772_reg *p_i2c_table;
	u8 u8val;
	int err;

	u8val = (u8)u32val;
	switch (params->param) {
	case NVC_PARAM_GAIN:
		dev_dbg(&info->i2c_client->dev, "%s GAIN: %u\n",
			__func__, u32val);
		ov9772_pm_dev_wr(info, NVC_PWR_COMM);
		err = ov9772_gain_wr(info, u32val);
		ov9772_pm_dev_wr(info, NVC_PWR_STDBY);
		return err;

	case NVC_PARAM_RESET:
		err = ov9772_reset(info, u32val);
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
		ov9772_pm_dev_wr(info, NVC_PWR_ON);
		err = ov9772_test_pattern_wr(info, u32val);
		if (!u8val)
			ov9772_pm_dev_wr(info, NVC_PWR_OFF);
		return err;

	case NVC_PARAM_TEST_PATTERN:
		dev_dbg(&info->i2c_client->dev, "%s TEST_PATTERN: %d\n",
			__func__, u32val);
		info->test_pattern = u32val;
		return 0;

	case NVC_PARAM_SELF_TEST:
		err = ov9772_dev_id(info);
		dev_dbg(&info->i2c_client->dev, "%s SELF_TEST: %d\n",
			__func__, err);
		return err;

	case NVC_PARAM_I2C:
		dev_dbg(&info->i2c_client->dev, "%s I2C\n", __func__);
		if (params->sizeofvalue > OV9772_I2C_TABLE_MAX_ENTRIES) {
			pr_err("%s: requested size too large\n", __func__);
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

		ov9772_pm_dev_wr(info, NVC_PWR_ON);
		err = ov9772_i2c_wr_table(info, p_i2c_table);
		kfree(p_i2c_table);
		return err;

	default:
		dev_err(&info->i2c_client->dev,
			"%s unsupported parameter: %d\n",
			__func__, params->param);
		return -EINVAL;
	}
}

static int ov9772_param_wr(struct ov9772_info *info, unsigned long arg)
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
			ov9772_gpio_wr(info, OV9772_GPIO_TYPE_I2CMUX, 0);
			if (info->s_info != NULL) {
				info->s_info->s_mode = u8val;
				ov9772_pm_wr(info->s_info, NVC_PWR_OFF);
			}
			break;

		case NVC_SYNC_MASTER:
			info->s_mode = u8val;
			ov9772_gpio_wr(info, OV9772_GPIO_TYPE_I2CMUX, 0);
			if (info->s_info != NULL)
				info->s_info->s_mode = u8val;
			break;

		case NVC_SYNC_SLAVE:
			if (info->s_info != NULL) {
				/* sync power */
				info->s_info->pwr_api = info->pwr_api;
				err = ov9772_pm_wr(info->s_info,
						info->pwr_dev);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
					ov9772_gpio_wr(info,
							OV9772_GPIO_TYPE_I2CMUX,
							0);
				} else {
					if (info->s_mode != NVC_SYNC_STEREO)
						ov9772_pm_wr(info->s_info,
							NVC_PWR_OFF);
					err = -EIO;
				}
			}
			break;

		case NVC_SYNC_STEREO:
			if (info->s_info != NULL) {
				/* sync power */
				info->s_info->pwr_api = info->pwr_api;
				err = ov9772_pm_wr(info->s_info,
						info->pwr_dev);
				if (!err) {
					info->s_mode = u8val;
					info->s_info->s_mode = u8val;
					ov9772_gpio_wr(info,
						OV9772_GPIO_TYPE_I2CMUX,
						1);
				} else {
					if (info->s_mode != NVC_SYNC_SLAVE)
						ov9772_pm_wr(info->s_info,
								NVC_PWR_OFF);
					err = -EIO;
				}
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
				"%s %d copy_from_user err\n",
				__func__, __LINE__);
			return -EFAULT;
		}
		ov9772_pm_dev_wr(info, NVC_PWR_COMM);
		err = ov9772_group_hold_wr(info, &ae);
		ov9772_pm_dev_wr(info, NVC_PWR_STDBY);
		return err;
	}

	default:
	/* parameters dependent on sync mode */
		switch (info->s_mode) {
		case NVC_SYNC_OFF:
		case NVC_SYNC_MASTER:
			return ov9772_param_wr_s(info, &params, u32val);

		case NVC_SYNC_SLAVE:
			return ov9772_param_wr_s(info->s_info,
						 &params,
						 u32val);

		case NVC_SYNC_STEREO:
			err = ov9772_param_wr_s(info, &params, u32val);
			if (!(info->pdata->cfg & NVC_CFG_SYNC_I2C_MUX))
				err |= ov9772_param_wr_s(info->s_info,
							 &params,
							 u32val);
			return err;

		default:
			dev_err(&info->i2c_client->dev, "%s %d internal err\n",
				__func__, __LINE__);
			return -EINVAL;
		}
	}
}

static int ov9772_get_fuse_id(struct ov9772_info *info)
{
	int err, i;

	if (info->fuse_id.size)
		return 0;

	err = ov9772_i2c_wr8(info->i2c_client, 0x3d81, 0x01);

	for (i = 0; i < OV9772_FUSE_ID_SIZE; i++) {
		err |= ov9772_i2c_rd8(info->i2c_client,
				0x3d00 + i,
				&info->fuse_id.data[i]);
	}

	if (!err)
		info->fuse_id.size = OV9772_FUSE_ID_SIZE;

	return err;
}

static long ov9772_ioctl(struct file *file,
			 unsigned int cmd,
			 unsigned long arg)
{
	struct ov9772_info *info = file->private_data;
	struct nvc_imager_bayer mode;
	struct nvc_imager_mode_list mode_list;
	struct nvc_imager_mode mode_table[OV9772_NUM_MODES_MAX];
	unsigned int mode_table_size;
	struct nvc_imager_dnvc dnvc;
	const void *data_ptr;
	s32 num_modes;
	u32 i;
	int pwr;
	int err;


	if (tegra_platform_is_fpga()) {
		ov9772_mode_table = ov9772_mode_table_fpga;
		ov9772_num_modes = ARRAY_SIZE(ov9772_mode_table_fpga);
	} else {
		ov9772_mode_table = ov9772_mode_table_non_fpga;
		ov9772_num_modes = ARRAY_SIZE(ov9772_mode_table_non_fpga);
	}
	mode_table_size = sizeof(struct nvc_imager_mode) * ov9772_num_modes;
	switch (_IOC_NR(cmd)) {
	case _IOC_NR(NVC_IOCTL_FUSE_ID):
		err = ov9772_get_fuse_id(info);
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
		err = ov9772_param_wr(info, arg);
		return err;

	case _IOC_NR(NVC_IOCTL_PARAM_RD):
		err = ov9772_param_rd(info, arg);
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
		err = ov9772_mode_rd(info, dnvc.res_x, dnvc.res_y, &i);
		if (err)
			return -EINVAL;

		if (dnvc.p_mode) {
			if (copy_to_user((void __user *)dnvc.p_mode,
					 &ov9772_mode_table[i]->sensor_mode,
					 sizeof(struct nvc_imager_mode))) {
				dev_err(&info->i2c_client->dev,
					"%s copy_to_user err line %d\n",
					__func__, __LINE__);
				return -EFAULT;
			}
		}

		if (dnvc.p_dnvc) {
			if (copy_to_user((void __user *)dnvc.p_dnvc,
				    &ov9772_mode_table[i]->sensor_dnvc,
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

		dev_dbg(&info->i2c_client->dev, "%s MODE_WR x=%d y=%d ",
			__func__, mode.res_x, mode.res_y);
		dev_dbg(&info->i2c_client->dev, "coarse=%u frame=%u gain=%u\n",
			mode.coarse_time, mode.frame_length, mode.gain);
		err = ov9772_mode_wr(info, &mode);
		return err;

	case _IOC_NR(NVC_IOCTL_MODE_RD):
		/*
		 * Return a list of modes that sensor bayer supports.
		 * If called with a NULL ptr to pModes,
		 * then it just returns the count.
		 */
		dev_dbg(&info->i2c_client->dev, "%s MODE_RD n=%d\n",
			__func__, ov9772_num_modes);
		if (copy_from_user(&mode_list, (const void __user *)arg,
				sizeof(struct nvc_imager_mode_list))) {
			dev_err(&info->i2c_client->dev,
				"%s copy_from_user err line %d\n",
				__func__, __LINE__);
			return -EFAULT;
		}

		num_modes = ov9772_num_modes;
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
			for (i = 0; i < ov9772_num_modes; i++) {
				mode_table[i] =
					ov9772_mode_table[i]->sensor_mode;
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
		err = ov9772_pm_api_wr(info, pwr);
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
			__func__, sizeof(ov9772_dflt_cap));
		data_ptr = info->cap;
		if (copy_to_user((void __user *)arg,
				 data_ptr,
				 sizeof(ov9772_dflt_cap))) {
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
		dev_err(&info->i2c_client->dev, "%s unsupported ioctl: %x\n",
			__func__, cmd);
	}
	return -EINVAL;
}

static void ov9772_sdata_init(struct ov9772_info *info)
{
	memcpy(&info->sdata, &ov9772_dflt_sdata, sizeof(info->sdata));
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

static int ov9772_sync_en(unsigned num, unsigned sync)
{
	struct ov9772_info *master = NULL;
	struct ov9772_info *slave = NULL;
	struct ov9772_info *pos = NULL;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &ov9772_info_list, list) {
		if (pos->pdata->num == num) {
			master = pos;
			break;
		}
	}
	pos = NULL;
	list_for_each_entry_rcu(pos, &ov9772_info_list, list) {
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

static int ov9772_sync_dis(struct ov9772_info *info)
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

static int ov9772_open(struct inode *inode, struct file *file)
{
	struct ov9772_info *info = NULL;
	struct ov9772_info *pos = NULL;
	int err;

	rcu_read_lock();
	list_for_each_entry_rcu(pos, &ov9772_info_list, list) {
		if (pos->miscdev.minor == iminor(inode)) {
			info = pos;
			break;
		}
	}
	rcu_read_unlock();
	if (!info)
		return -ENODEV;

	err = ov9772_sync_en(info->pdata->num, info->pdata->sync);
	if (err == -EINVAL)
		dev_err(&info->i2c_client->dev,
			"%s err: invalid num (%u) and sync (%u) instance\n",
			__func__, info->pdata->num, info->pdata->sync);
	if (atomic_xchg(&info->in_use, 1))
		return -EBUSY;

	if (info->s_info != NULL) {
		if (atomic_xchg(&info->s_info->in_use, 1))
			return -EBUSY;
		info->sdata.stereo_cap = 1;
	}

	file->private_data = info;
	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	return 0;
}

int ov9772_release(struct inode *inode, struct file *file)
{
	struct ov9772_info *info = file->private_data;

	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
	ov9772_pm_wr_s(info, NVC_PWR_OFF);
	file->private_data = NULL;
	WARN_ON(!atomic_xchg(&info->in_use, 0));
	if (info->s_info != NULL)
		WARN_ON(!atomic_xchg(&info->s_info->in_use, 0));
	ov9772_sync_dis(info);
	return 0;
}

static const struct file_operations ov9772_fileops = {
	.owner = THIS_MODULE,
	.open = ov9772_open,
	.unlocked_ioctl = ov9772_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = ov9772_ioctl,
#endif
	.release = ov9772_release,
};

static void ov9772_del(struct ov9772_info *info)
{
	ov9772_pm_exit(info);
	if ((info->s_mode == NVC_SYNC_SLAVE) ||
					(info->s_mode == NVC_SYNC_STEREO))
		ov9772_pm_exit(info->s_info);
	ov9772_sync_dis(info);
	spin_lock(&ov9772_spinlock);
	list_del_rcu(&info->list);
	spin_unlock(&ov9772_spinlock);
	synchronize_rcu();
}

static int ov9772_remove(struct i2c_client *client)
{
	struct ov9772_info *info = i2c_get_clientdata(client);

	dev_dbg(&info->i2c_client->dev, "%s\n", __func__);
#ifdef CONFIG_DEBUG_FS
	nvc_debugfs_remove(&info->debugfs_info);
#endif
	misc_deregister(&info->miscdev);
	ov9772_del(info);
	return 0;
}

static struct of_device_id ov9772_of_match[] = {
	{ .compatible = "nvidia,ov9772", },
	{ },
};

MODULE_DEVICE_TABLE(of, ov9772_of_match);

static int ov9772_parse_dt_gpio(struct device_node *np, const char *name,
			enum ov9772_gpio_type type,
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

static struct ov9772_platform_data *ov9772_parse_dt(struct i2c_client *client)
{
	struct device_node *np = client->dev.of_node;
	struct ov9772_platform_data *board_info_pdata;
	struct nvc_gpio_pdata *gpio_pdata = NULL;
	const struct of_device_id *match;

	match = of_match_device(ov9772_of_match, &client->dev);
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
		sizeof(*gpio_pdata) * ARRAY_SIZE(ov9772_gpio),
		GFP_KERNEL);
	if (!gpio_pdata) {
		dev_err(&client->dev, "cannot allocate gpio data memory\n");
		return ERR_PTR(-ENOMEM);
	}

	/* init with default platform data values */
	memcpy(board_info_pdata, &ov9772_dflt_pdata, sizeof(*board_info_pdata));

	/* generic info */
	of_property_read_u32(np, "nvidia,num", &board_info_pdata->num);
	of_property_read_string(np, "nvidia,dev_name",
				&board_info_pdata->dev_name);
	board_info_pdata->vcm_vdd = of_property_read_bool(np, "nvidia,vcm_vdd");

	/* ov9772 gpios */
	board_info_pdata->gpio_count = 0;
	board_info_pdata->gpio_count += ov9772_parse_dt_gpio(np,
				"power-gpios", OV9772_GPIO_TYPE_SHTDN,
				&gpio_pdata[board_info_pdata->gpio_count]);
	board_info_pdata->gpio_count += ov9772_parse_dt_gpio(np,
				"reset-gpios", OV9772_GPIO_TYPE_PWRDN,
				&gpio_pdata[board_info_pdata->gpio_count]);

	board_info_pdata->gpio = gpio_pdata;

	/* Use driver's default power functions */
	board_info_pdata->power_on = NULL;
	board_info_pdata->power_off = NULL;

	return board_info_pdata;
}

static int ov9772_probe(
	struct i2c_client *client,
	const struct i2c_device_id *id)
{
	struct ov9772_info *info;
	unsigned long clock_probe_rate;
	const char *mclk_name;
	int err;

	dev_dbg(&client->dev, "%s\n", __func__);
	info = devm_kzalloc(&client->dev, sizeof(*info), GFP_KERNEL);
	if (info == NULL) {
		dev_err(&client->dev, "%s: kzalloc error\n", __func__);
		return -ENOMEM;
	}

	info->i2c_client = client;

	if (client->dev.of_node) {
		info->pdata = ov9772_parse_dt(client);
	} else if (client->dev.platform_data) {
		info->pdata = client->dev.platform_data;
	} else {
		info->pdata = &ov9772_dflt_pdata;
		dev_dbg(&client->dev,
			"%s No platform data.  Using defaults.\n", __func__);
	}

	if (info->pdata->cap)
		info->cap = info->pdata->cap;
	else
		info->cap = &ov9772_dflt_cap;

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
	spin_lock(&ov9772_spinlock);
	list_add_rcu(&info->list, &ov9772_info_list);
	spin_unlock(&ov9772_spinlock);
	ov9772_pm_init(info);
	ov9772_sdata_init(info);
	if (info->pdata->cfg & (NVC_CFG_NODEV | NVC_CFG_BOOT_INIT)) {
		if (info->pdata->probe_clock) {
			if (info->cap->initial_clock_rate_khz)
				clock_probe_rate = info->cap->
							initial_clock_rate_khz;
			else
				clock_probe_rate = ov9772_dflt_cap.
							initial_clock_rate_khz;
			clock_probe_rate *= 1000;
			info->pdata->probe_clock(clock_probe_rate);
		}
		err = ov9772_dev_id(info);
		if (err < 0) {
			if (info->pdata->cfg & NVC_CFG_NODEV) {
				ov9772_del(info);
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
				ov9772_mode_wr_full(info, info->cap->
						preferred_mode_index);
		}
		ov9772_pm_dev_wr(info, NVC_PWR_OFF);
		if (info->pdata->probe_clock)
			info->pdata->probe_clock(0);
	}
	if (info->pdata->dev_name != 0)
		strncpy(info->devname, info->pdata->dev_name,
			sizeof(info->devname) - 1);
	else
		strncpy(info->devname, "ov9772", sizeof(info->devname) - 1);

	if (info->pdata->num)
		snprintf(info->devname, sizeof(info->devname), "%s.%u",
			 info->devname, info->pdata->num);

	info->miscdev.name = info->devname;
	info->miscdev.fops = &ov9772_fileops;
	info->miscdev.minor = MISC_DYNAMIC_MINOR;
	if (misc_register(&info->miscdev)) {
		dev_err(&client->dev, "%s unable to register misc device %s\n",
			__func__, info->devname);
		ov9772_del(info);
		return -ENODEV;
	}
#ifdef CONFIG_DEBUG_FS
	info->debugfs_info.name = info->devname;
	info->debugfs_info.i2c_client = info->i2c_client;
	info->debugfs_info.i2c_addr_limit = 0xFFFF;
	info->debugfs_info.i2c_rd8 = ov9772_i2c_rd8;
	info->debugfs_info.i2c_wr8 = ov9772_i2c_wr8;
	nvc_debugfs_init(&(info->debugfs_info));
#endif

	return 0;
}

static const struct i2c_device_id ov9772_id[] = {
	{ "ov9772", 0 },
	{ },
};

MODULE_DEVICE_TABLE(i2c, ov9772_id);

static struct i2c_driver ov9772_i2c_driver = {
	.driver = {
		.name = "ov9772",
		.owner = THIS_MODULE,
	},
	.id_table = ov9772_id,
	.probe = ov9772_probe,
	.remove = ov9772_remove,
};

module_i2c_driver(ov9772_i2c_driver);
MODULE_LICENSE("GPL v2");
