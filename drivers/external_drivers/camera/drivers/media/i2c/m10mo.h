/*
 * Copyright (c) 2014 Intel Corporation. All Rights Reserved.
 *
 * Partially based on m-5mols kernel driver,
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 *
 * Partially based on jc_v4l2 kernel driver from http://opensource.samsung.com
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef M10MO_REG_H
#define M10MO_REG_H
#include <linux/atomisp_platform.h>
#include <linux/atomisp.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/v4l2-mediabus.h>
#include <media/media-entity.h>
#include <media/v4l2-chip-ident.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-subdev.h>

#define M10MO_NAME			"m10mo"
#define M10MO_ADDR			0x1f
#define M10MO_I2C_RETRY			5
#define M10MO_MIPI_FREQ_0			(963000000/2)
#define M10MO_MIPI_FREQ_1			(980700000/2)
#define M10MO_INIT_TIMEOUT		500
#define M10MO_BOOT_TIMEOUT		50
#define POLL_NUM			20

#define M10MO_MIN_EV -2000
#define M10MO_MAX_EV  3000
#define M10MO_EV_STEP 500
#define M10MO_FLICKER_AUTO		0x00
#define M10MO_FLICKER_50HZ		0x01
#define M10MO_FLICKER_60HZ		0x02
#define M10MO_FLICKER_OFF		0x03
#define M10MO_METERING_CENTER		0x00
#define M10MO_METERING_SPOT		0x01
#define M10MO_METERING_AVERAGE		0x02

/* TODO These values for focal length, f-number are taken from
 * imx135 13MP. This can be changed when we get the proper values
 * for m10mo
 */

/*
 * focal length bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */

#define M10MO_FOCAL_LENGTH_DEFAULT	0x1710064

/*
 * current f-number bits definition:
 * bits 31-16: numerator, bits 15-0: denominator
 */

#define M10MO_F_NUMBER_DEFAULT		0x16000a

/*
 * f-number range bits definition:
 * bits 31-24: max f-number numerator
 * bits 23-16: max f-number denominator
 * bits 15-8: min f-number numerator
 * bits 7-0: min f-number denominator
 */

#define M10MO_F_NUMBER_RANGE		0x160a160a
#define M10MO_FOCAL_LENGTH_NUM		369
#define M10MO_FOCAL_LENGTH_DEM		100
#define M10MO_F_NUMBER_DEFAULT_NUM	22
#define M10MO_F_NUMBER_DEM		10

#define M10MO_INVALID_CHECKSUM          0xffff
#define M10MO_VALID_CHECKSUM            0

/* M10MO FW VERSION INFO ADDR */
#define M10MO_FW_VERSION_INFO_ADDR_0	0x181EF080
#define M10MO_FW_VERSION_INFO_ADDR_1	0x18000020

/* M10MO I2C commands */
#define M10MO_BYTE_READ			0x01
#define M10MO_BYTE_WRITE		0x02
#define M10MO_MEMORY_READ_8BIT		0x03
#define M10MO_MEMORY_WRITE_8BIT		0x04
#define M10MO_MEMORY_READ_16BIT		0x05
#define M10MO_MEMORY_WRITE_16BIT	0x06
#define M10MO_MEMORY_READ_32BIT		0x07
#define M10MO_MEMORY_WRITE_32BIT	0x08

#define CAPTURE_FORMAT_YUV422		0x00
#define CAPTURE_FORMAT_JPEG8		0x01

/* TODO: Fix this */
/* 4128*3096*2 + X extra bytes. Rounded to height divisible by 32 -> 25624576 */
#define M10MO_MAX_YUV422_SIZE		25624576
/* Max RAW size 26000000 + 8*2048 + 1048 (to algin to 32 bytes) */
#define M10MO_MAX_RAW_SIZE		26017792

#define M10MO_GET_CLOCK_RATE_MODE(arg)	((arg >> M10MO_CLOCK_RATE_MODE_OFFSET) & M10MO_MASK)
#define M10MO_GET_MIPI_FREQ_MODE(arg)	((arg >> M10MO_MIPI_FREQ_MODE_OFFSET) & M10MO_MASK)
#define M10MO_GET_FOCUS_MODE(arg)	((arg >> M10MO_AF_MODE_OFFSET) & M10MO_MASK)
#define M10MO_GET_RESOLUTION_MODE(arg)	((arg >> M10MO_RESOLUTION_MODE_OFFSET) & M10MO_MASK)
#define M10MO_SHOT_MODES_SUPPORTED(arg)	(arg & M10MO_SHOT_MODE_SUPPORT)
#define M10MO_GET_MIPI_PACKET_SIZE_IDX(arg)	((arg >> M10MO_MIPI_PACKET_SIZE_OFFSET) & M10MO_MASK)

#define M10MO_METADATA_WIDTH	2048
#define M10MO_METADATA_HEIGHT	4
#define M10MO_METADATA_FORMAT	ATOMISP_INPUT_FORMAT_EMBEDDED

struct m10mo_spi {
	int spi_enabled;
	struct spi_device *spi_device;
	int (*write)(struct spi_device *spi, const u8 *addr,
		     const int len, const int txSize);
	int (*read)(struct spi_device *spi, u8 *buf, size_t len,
		    const int rxSize);
};

struct m10mo_version {
	int customer;
	int project;
	int firmware;
	int hardware;
	int parameter;
	int awb;
};

struct m10mo_resolution {
	u32 width;
	u32 height;
	u32 command;
	bool vdis;
};

struct m10mo_monitor_params {
	u8 af_mode;
	u8 exe_mode;
	unsigned int af_touch_posx;
	unsigned int af_touch_posy;
	u8 flash_mode;
	u8 torch;
};

/* Parameters dependent to the MIPI packet size (FW specific) */
struct m10mo_mipi_params {
	u32 jpeg_width;
	u32 jpeg_height;
	u32 raw_width;
	u32 raw_height;
};

struct m10mo_fw_ops {
	int (*set_run_mode)(struct v4l2_subdev *sd);
	int (*set_burst_mode)(struct v4l2_subdev *sd, unsigned int val);
	int (*stream_off)(struct v4l2_subdev *sd);
	int (*single_capture_process)(struct v4l2_subdev *sd);
	int (*try_mbus_fmt)(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *fmt, bool update_fmt);
	int (*set_mbus_fmt)(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *fmt);
	int (*test_pattern)(struct v4l2_subdev *sd, u8 val);
};

struct m10mo_device {
	struct v4l2_subdev sd;
	struct media_pad pad;
	struct v4l2_mbus_framefmt format;
	struct m10mo_platform_data *pdata;
	struct mutex input_lock; /* serialize sensor's ioctl */
	struct m10mo_spi *spi;
	struct m10mo_monitor_params monitor_params;
	struct m10mo_mipi_params mipi_params;
	u8 message_buffer[256]; /* Real buffer size TBD */
	int res_type;
	int power;
	u8 fps;
	u8 requested_mode;
	u8 mode;
	u8 capture_mode;
	short iso_mode;
	short iso_sensitivity;
	u8 colorfx_cr;
	u8 colorfx_cb;
	int fmt_idx;
	int capture_res_idx;
	wait_queue_head_t irq_waitq;
	unsigned int bad_fw:1;
	unsigned int isp_ready:1;
	unsigned int initialized;
	struct m10mo_version ver;
	struct v4l2_ctrl_handler ctrl_handler;
	int run_mode;
	struct v4l2_ctrl *link_freq;
	struct v4l2_ctrl *zsl_capture;
	struct v4l2_ctrl *colorfx;
	const struct m10mo_fw_ops *fw_ops;
	unsigned int num_lanes;
	const struct m10mo_resolution *curr_res_table;
	int entries_curr_table;
	int ref_clock;
	unsigned int fw_type;
	int fw_addr_id;
	u8 shot_mode;
};

enum hdr_options {
	STOP_HDR_MODE,
	START_HDR_MODE,
	RESUME_PREVIEW_IN_HDR_MODE
};

enum lls_options {
	STOP_LLS_MODE,
	START_LLS_MODE,
	RESUME_PREVIEW_IN_LLS_MODE
};

#define to_m10mo_sensor(x) container_of(x, struct m10mo_device, sd)

int m10mo_memory_read(struct v4l2_subdev *sd, u16 len, u32 addr, u8 *val);
int m10mo_memory_write(struct v4l2_subdev *sd, u8 cmd,
				u16 len, u32 addr, u8 *val);
int m10mo_writeb(struct v4l2_subdev *sd, u8 category, u8 reg, u32 val);
int m10mo_writew(struct v4l2_subdev *sd, u8 category, u8 reg, u32 val);
int m10mo_writel(struct v4l2_subdev *sd, u8 category, u8 reg, u32 val);
int m10mo_readb(struct v4l2_subdev *sd, u8 category, u8 reg, u32 *val);
int m10mo_readw(struct v4l2_subdev *sd, u8 category, u8 reg, u32 *val);
int m10mo_readl(struct v4l2_subdev *sd, u8 category, u8 reg, u32 *val);
int m10mo_setup_flash_controller(struct v4l2_subdev *sd);
int m10mo_request_mode_change(struct v4l2_subdev *sd, u8 requested_mode);
int m10mo_wait_mode_change(struct v4l2_subdev *sd, u8 mode, u32 timeout);
int __m10mo_param_mode_set(struct v4l2_subdev *sd);
int __m10mo_update_stream_info(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt);
int __m10mo_try_mbus_fmt(struct v4l2_subdev *sd,
			struct v4l2_mbus_framefmt *fmt, bool update_fmt);
int __m10mo_set_mbus_fmt(struct v4l2_subdev *sd,
				struct v4l2_mbus_framefmt *fmt);
int m10mo_test_pattern_start(struct v4l2_subdev *sd);

int get_resolution_index(const struct m10mo_resolution *res,
			 int entries, int w, int h);
int m10mo_set_zsl_raw_capture(struct v4l2_subdev *sd);


void m10mo_register_spi_fw_flash_interface(struct m10mo_device *dev,
					   struct m10mo_spi *m10mo_spi_dev);

int m10mo_dump_fw(struct m10mo_device *m10mo_dev);
int m10mo_get_fw_address_count(void);
int m10mo_get_isp_fw_version_string(struct m10mo_device *dev, char *buf,
		int len, int fw_address_id);
int m10mo_fw_checksum(struct m10mo_device *dev, u16 *result);
int m10mo_program_device(struct m10mo_device *m10mo_dev);

int m10mo_get_spi_state(struct m10mo_device *m10mo_dev);
int m10mo_set_spi_state(struct m10mo_device *m10mo_dev, bool enabled);

int m10mo_update_pll_setting(struct v4l2_subdev *sd);
int m10mo_dump_string_log1(struct v4l2_subdev *sd);
int m10mo_dump_string_log2_1(struct v4l2_subdev *sd);
int m10mo_dump_string_log2_2(struct v4l2_subdev *sd);
int m10mo_dump_string_log2_3(struct v4l2_subdev *sd);
int m10mo_dump_string_log3(struct v4l2_subdev *sd);
void m10mo_dump_log(struct v4l2_subdev *sd);
int m10mo_single_capture_process(struct v4l2_subdev *sd);
int m10mo_set_still_capture(struct v4l2_subdev *sd);
int m10mo_set_panorama_monitor(struct v4l2_subdev *sd);
int m10mo_set_zsl_monitor(struct v4l2_subdev *sd);
int m10mo_set_burst_mode(struct v4l2_subdev *sd, unsigned int val);
int m10mo_streamoff(struct v4l2_subdev *sd);
int m10mo_test_pattern(struct v4l2_subdev *sd, u8 val);

extern const struct m10mo_fw_ops fw_type1_5_ops;
extern const struct m10mo_fw_ops fw_type2_ops;

/* Below contents are based on the M10MO_categoryParameter-a1.xls */

/* Category register */
#define CATEGORY_SYSTEM		0x00
#define CATEGORY_PARAM		0x01	/* Monitor & Still Parameter A */
#define CATEGORY_MONITOR	0x02  /* Monitor & Still Parameter B */
#define CATEGORY_AE		0x03
#define CATEGORY_WB		0x06
#define CATEGORY_EXIF		0x07  /* Exif Information */
#define CATEGORY_LENS		0x0a	/* AF Control */
#define CATEGORY_CAPTURE_PARAM	0x0b	/* Still Picture parameter */
#define CATEGORY_CAPTURE_CTRL	0x0c  /* Still picture control */
#define CATEGORY_LOGLEDFLASH	0x0d	/*Log Led Flash Category */
#define CATEGORY_TEST		0x0d	/* Test category for FW_TYPE_2 */
#define CATEGORY_FLASHROM	0x0f	/* FlashROM-Writer Mode only */

/* Category 0_SYSTEM mode */
#define SYSTEM_CUSTOMER_CODE	0x00
#define SYSTEM_PROJECT_CODE	0x01
#define SYSTEM_VER_FIRMWARE	0x02
#define SYSTEM_VER_HARDWARE	0x04
#define SYSTEM_VER_PARAMETER	0x06
#define SYSTEM_VER_AWB		0x08
#define SYSTEM_MASTER_SENSOR	0x17

#define SYSTEM_SYSMODE			0x0b
/* SYSTEM mode status */
#define SYSTEM_STATUS			0x0c

/* interrupt enable register */
#define SYSTEM_INT_ENABLE		0x10
#define REG_INT_EN_MODE			(1 << 0)
#define REG_INT_AF			(1 << 1)
#define REG_INT_EN_ZOOM			(1 << 2)
#define REG_INT_EN_CAPTURE		(1 << 3)
#define REG_INT_EN_FRAMESYNC		(1 << 4)
#define REG_INT_EN_FD			(1 << 5)
#define REG_INT_EN_SOUND		(1 << 7)
#define REG_REG_INT_MASK		0x0f

/* Interrupt factor (pending) register */
#define SYSTEM_INT_FACTOR	0x1c
#define REG_INT_STATUS_MODE	(1 << 0)
#define REG_INT_STATUS_ZOOM	(1 << 2)
#define REG_INT_STATUS_CAPTURE	(1 << 3)
#define REG_INT_STATUS_FRAMESYNC (1 << 4)
#define REG_INT_STATUS_FD	(1 << 5)
#define REG_INT_STATUS_SOUND	(1 << 7)

/* category 1_PARAMETER mode */
#define PARAM_MON_SIZE			0x01
#define PARAM_MON_FPS			0x02
#define REG_FPS_30			0x02
#define PARAM_OUTPUT_IF_SEL		0x00
#define REG_OUTPUT_INTERFACE_MIPI	0x02
#define PARAM_MIPI_OUT_LANE_NUM		0x3e
#define REG_OUTPUT_MIPI_4LANE		0x04

#define PARAM_VDIS			0x00
#define SHOT_MODE			0x0e
#define MPO_FORMAT_META			0x0e
#define MONITOR_TYPE			0x6e
#define MOVIE_MODE			0x3c

#define MONITOR_PREVIEW		0
#define MONITOR_BURST		1
#define MONITOR_VIDEO		3

/* Category 2_MONITOR mode */

#define MONITOR_ZOOM		0x01
#define ZOOM_POS_MIN		0x01
#define ZOOM_POS_MAX		0x1f /* 31 */
#define MONITOR_CFIXR		0x0a
#define MONITOR_CFIXB		0x09
#define MONITOR_COLOR_EFFECT	0x0b
#define MONITOR_ZSL_MODE_STATUS	0x5f
#define REG_NORMAL_MONITOR	0x00
#define REG_ZSL_MONITOR	0x01
#define ZSL_MODE		0x6e
#define ZSL_INTERVAL		0x6f

#define COLOR_EFFECT_NONE	0x00
#define COLOR_EFFECT_ON		0x01
#define COLOR_EFFECT_NEGATIVE	0x02
#define COLOR_EFFECT_WARM	0x04
#define COLOR_EFFECT_COLD	0x05
#define COLOR_EFFECT_WASHED	0x06

#define COLOR_CFIXB_SEPIA	0xd8
#define COLOR_CFIXR_SEPIA	0x18
#define COLOR_CFIXB_BW		0x00
#define COLOR_CFIXR_BW		0x00
#define COLOR_CFIXB_RED		0x00
#define COLOR_CFIXR_RED		0x6b
#define COLOR_CFIXB_GREEN	0xe0
#define COLOR_CFIXR_GREEN	0xe0
#define COLOR_CFIXB_BLUE	0x40
#define COLOR_CFIXR_BLUE	0x00
#define COLOR_CFIXB_PINK	0x20
#define COLOR_CFIXR_PINK	0x40
#define COLOR_CFIXB_YELLOW	0x80
#define COLOR_CFIXR_YELLOW	0x00
#define COLOR_CFIXB_PURPLE	0x50
#define COLOR_CFIXR_PURPLE	0x20
#define COLOR_CFIXB_ANTIQUE	0xd0
#define COLOR_CFIXR_ANTIQUE	0x30

/*  ZSL MODE */
#define CAPTURE_MODE		0x00
#define CAP_MODE_INFINITY_ZSL	0x0f
#define CAP_MODE_PANORAMA	0x00

/* In other type firmware movie mode is 0x00 */
#define CAP_MODE_MOVIE		0x00

#define ZSL_TRANSFER_NO	0x16
#define CAP_NV12_MODE		0x0a
#define START_DUAL_STATUS	0x1f
#define START_DUAL_CAPTURE	0x05

#define DUAL_CAPTURE_SINGLE_CAPTURE_START	0x01
#define DUAL_CAPTURE_HDR_CAPTURE_START		0x01
#define DUAL_CAPTURE_BURST_CAPTURE_START	0x04
#define DUAL_CAPTURE_BURST_CAPTURE_STOP		0x05
#define DUAL_CAPTURE_ZSL_CAPTURE_START		0x07
#define DUAL_CAPTURE_LLS_CAPTURE_START		0x08

#define DUAL_STATUS_IDLE			0x0
#define DUAL_STATUS_CAPTURE			0x1
#define DUAL_STATUS_AF_WORKING		0x2

/* Output format selection between YUV422 and NV12/NV21 */
#define OUTPUT_FMT_SELECT	0x05
#define OUTPUT_FMT_SELECT_YUV422	0x00
#define OUTPUT_FMT_SELECT_NV12NV21	0x01

/* Choose between NV12 and NV21 */
#define CHOOSE_NV12NV21_FMT		0x27
#define CHOOSE_NV12NV21_FMT_NV12	0x00
#define CHOOSE_NV12NV21_FMT_NV21	0x01

/* Enable/Disable metadata in monitor mode */
#define MON_METADATA_SUPPORT_CTRL	0x06
#define MON_METADATA_SUPPORT_CTRL_EN	0x01
#define MON_METADATA_SUPPORT_CTRL_DIS	0x00

/* MPO format */
#define MON_MPO_FMT_CTRL		0x07
#define MON_MPO_FMT_NV21		0x02
#define MON_MPO_FMT_YUV422		0x03
#define MON_MPO_FMT_YUV420		0x06

/* Category 3_Auto Exposure */

#define AE_LOCK			0x00
#define REG_AE_UNLOCK		0x00
#define REG_AE_LOCK		0x01
#define AE_MODE			0x01
#define REG_AE_OFF		0x00
#define AE_TARGET		0x02
#define AE_SPEED		0x03
#define AE_ISOMODE		0x05
#define AE_FLICKER		0x06
#define AE_FLICKER_AUTO		0x07
#define AE_EV_BIAS		0x09
#define AE_AUTO_BRACKET_EV1	0x20
#define AE_AUTO_BRACKET_EV2	0x21

#define REG_AE_ISOMODE_AUTO	0x00
#define REG_AE_ISOMODE_ISO100	0x01
#define REG_AE_ISOMODE_ISO200	0x02
#define REG_AE_ISOMODE_ISO400	0x03
#define REG_AE_ISOMODE_ISO800	0x04
#define REG_AE_ISOMODE_ISO1600	0x05


/* Category 6_White Balance */

#define AWB_MODE		0x02

#define REG_AWB_AUTO		0x01
#define REG_AWB_MANUAL		0x02

#define AWB_PRESET		0x03

#define REG_AWB_PROHIBITION	0x00
#define REG_AWB_INCANDESCENT	0x01
#define REG_AWB_FLUORESCENT_H	0x02
#define REG_AWB_FLUORESCENT_L	0x03
#define REG_AWB_DAYLIGHT	0x04
#define REG_AWB_CLOUDY		0x05
#define REG_AWB_SHADE		0x06
#define REG_AWB_HORIZON		0x07
#define REG_AWB_LEDLIGHT	0x09

/* Category 7_EXIF */
#define EXIF_INFO_EXPTIME_NU	0x00
#define EXIF_INFO_EXPTIME_DE	0x04
#define EXIF_INFO_TV_NU		0x08
#define EXIF_INFO_TV_DE		0x0c
#define EXIF_INFO_AV_NU		0x10
#define EXIF_INFO_AV_DE		0x14
#define EXIF_INFO_BV_NU		0x18
#define EXIF_INFO_BV_DE		0x1c
#define EXIF_INFO_EBV_NU	0x20
#define EXIF_INFO_EBV_DE	0x24
#define EXIF_INFO_ISO		0x28
#define EXIF_INFO_FLASH		0x2a
#define EXIF_INFO_SDR		0x2c
#define EXIF_INFO_QVAL		0x2e


/* Category A_Lens Parameter */
#define AF_MODE			0x01
#define AF_NORMAL		0x00
#define AF_MACRO		0x01
#define AF_TOUCH		0x02
#define	AF_PREVIEW_CAF		0x03
#define	AF_MOVIE_CAF		0x04
#define	AF_FACE_CAF		0x05
#define	AF_TOUCH_MACRO		0x06
#define AF_TOUCH_CAF		0x07

#define AF_EXECUTION		0x02
#define AF_STOP			0x00
#define AF_SEARCH		0x01
#define AF_PAN_FOCUSING		0x02

#define AF_RESULT			0x03
#define CAF_STATUS_RESTART_CHECK	0x01
#define CAF_STATUS_FOCUSING		0x02
#define CAF_STATUS_SUCCESS		0x03
#define CAF_STATUS_FAIL			0x04
#define AF_STATUS_INVALID		0x10
#define AF_STATUS_FOCUSING		0x20
#define AF_STATUS_SUCCESS		0x30
#define AF_STATUS_FAIL			0x40
#define AF_TOUCH_POSX			0X30
#define AF_TOUCH_POSY			0X32


/* Category B_CAPTURE Parameter */
#define CAPP_YUVOUT_MAIN	0x00
#define REG_YUV422		0x00
#define REG_BAYER10		0x07
#define REG_BAYER8		0x08
#define REG_JPEG		0x01

#define CAPP_MAIN_IMAGE_SIZE	0x01
#define CAPP_JPEG_SIZE_MAX	0x0f
#define CAPP_JPEG_RATIO		0x17
#define CAPP_JPEG_DUAL_RATIO	0x18

/* Category C_CAPTURE Control */
#define CAPC_MODE		0x00
#define REG_CAP_NONE		0x00
#define REG_CAP_ANTI_SHAKE	0x02
#define CAPC_SEL_FRAME_MAIN	0x06
#define CAPC_TRANSFER_START	0x09
#define REG_CAP_START_MAIN	0x01

/* Category D LED Flash Control */
#define FLASH_MODE              0xB6
#define FLASH_MODE_OFF          0x00
#define FLASH_MODE_ON           0X01
#define FLASH_MODE_AUTO         0X02
#define LED_TORCH               0x29
#define LED_TORCH_OFF           0x00
#define LED_TORCH_ON            0x01
#define LOG_ADD_SHOW		0x06
#define LOG_ADD_SHOW_INIT_VALUE	0x00
#define LOG_STR_LEN		0x07
#define LOG_STR_ADD3		0x08
#define LOG_STR_ADD2		0x09
#define LOG_STR_ADD1		0x0A
#define LOG_STR_ADD0		0x0B
#define LOG_SEL1		0x0C
#define LOG_SEL0		0x0D
#define LOG_ACT		0x0E
#define LOG_ACT_ENABLE		0x01
#define LOG_ACT_DISABLE	0x02
#define LOG_ACT_OUTPUT_STR	0x03
#define LOG_ACT_CLEAR		0x04
#define LOG_MODE		0x0F
#define LOG_STANDARD_MODE	0x00
#define LOG_ANALYZE_MODE0	0x01
#define LOG_ANALYZE_MODE1	0x02
#define LOG_ANALYZE_MODE2	0x03
#define LOG_TRACE_MODE		0x04
#define LOG_DATA_LEN1		0x14
#define LOG_DATA_LEN0		0x15

#define I2C_MEM_READ_SIZE	128
#define MAX_LOG_STR_LEN	0xFF
#define MIN_LOG_STR_LEN	0x00
#define MAX_LOG_STR_LEN_LOG2	0xFFFF
#define MIN_LOG_STR_LEN_LOG2	0x0000
#define MAX_MEM_DUMP_NUM	10000
#define MAX_MEM_DUMP_NUM_LOG3	20000

/* Category D Test */
#define TEST_PATTERN_SENSOR	0x1d

/* Category F_Flash */
#define REG_FLASH_ADD           0x00
#define REG_FLASH_BYTE          0x04
#define REG_FLASH_ERASE         0x06
#define REG_FLASH_WRITE         0x07
#define REG_FLASH_CHECK         0x09
#define REG_FLASH_SUM           0x0a
#define REG_CAM_START_ADD       0x0c
#define REG_CAM_START           0x12
#define REG_DATA_RAM_ADDR       0x14
#define REG_DATA_TRANS_SIZE     0x18
#define REG_PLL_VALUES          0x1c /* 2 dividers, 2 multipliers */
#define REG_SDRAM_CFG           0x48
#define REG_RAM_START           0x4a
#define REG_SIO_MODE            0x4b
#define REG_FW_READ             0x57
#define REG_CHECK_SUM_SIZE      0x5c

/* Shot modes (cat 0x01 byte 0x0e) */
#define SHOT_MODE_AUTO			0x01
#define SHOT_MODE_BEAUTY_FACE		0X02
#define SHOT_MODE_BEST_PHOTO		0X03
#define SHOT_MODE_DRAMA			0X04
#define SHOT_MODE_BEST_FACE		0X05
#define SHOT_MODE_ERASER		0X06
#define SHOT_MODE_PANORAMA		0x07
#define SHOT_MODE_RICH_TONE_HDR		0X09
#define SHOT_MODE_NIGHT			0X0A
#define SHOT_MODE_SOUND_SHOT            0X0B
#define SHOT_MODE_ANIMATED_PHOTO	0X0F
#define SHOT_MODE_SPORTS		0X11

/* Still capture modes for NV12 */
#define REG_CAP_NV12_MODE	0x0a
#define NORMAL_CAPTURE		0x00
#define HDR_CAPTURE		0x02
#define LLS_CAPTURE		0x04
#define RAW_CAPTURE		0x10

#define PREVIEW_IN_NV12_MODE	0x02

#define REG_RAM_START_SRAM      0x01
#define REG_RAM_START_SDRAM     0x02

#define REG_FLASH_WRITE_START_PRG      0x01

#define REG_FLASH_ERASE_SECTOR_ERASE   0x01
#define REG_FLASH_ERASE_CHIP_ERASE     0x02
#define REG_FLASH_ERASE_BLOCK64k_ERASE 0x04
#define REG_FLASH_ERASE_BLOCK32k_ERASE 0x08

#define REG_FW_READ_CMD_READ    0x01
#define REG_FW_READ_CMD_NONE    0x00

#define REG_SIO_MODE_RISING_LATCH 0x4c
#define REG_SIO_MODE_FALLING_LATCH 0x44

/* Starts internal ARM core, 1st command to be sent to ISP */
#define FLASH_CAM_START		REG_CAM_START

/* Internal modes of M10MO */
enum M10MO_MODES {
	M10MO_NO_MODE_REQUEST,
	M10MO_POWERED_OFF,
	M10MO_POWERING_ON,
	M10MO_FLASH_WRITE_MODE,
	M10MO_PARAM_SETTING_MODE,
	M10MO_PARAMETER_MODE,
	M10MO_MONITOR_MODE,
	M10MO_MONITOR_MODE_ZSL,
	M10MO_MONITOR_MODE_PANORAMA,
	M10MO_SINGLE_CAPTURE_MODE,
	M10MO_BURST_CAPTURE_MODE,
	M10MO_MONITOR_MODE_HIGH_SPEED
};

/* Internal modes of M10MO */
enum M10MO_CAPTURE_MODES {
	M10MO_CAPTURE_MODE_ZSL_NORMAL = 0,
	M10MO_CAPTURE_MODE_ZSL_HDR,
	M10MO_CAPTURE_MODE_ZSL_LLS,
	M10MO_CAPTURE_MODE_ZSL_BURST,
	M10MO_CAPTURE_MODE_ZSL_RAW
};

#define M10MO_MODE_PREVIEW_INDEX	0
#define M10MO_MODE_CAPTURE_INDEX	1
#define M10MO_MODE_VIDEO_INDEX		2

#define M10MO_ZSL_JPEG_VIRTUAL_CHANNEL	1
#define M10MO_ZSL_NV12_VIRTUAL_CHANNEL	0

#define M10MO_NORMAL_FPS		30
#define M10MO_HIGH_SPEED_FPS	60
/* FIXME It should be in tables */
#define MON_SIZE_FHD_60FPS		0x2b

extern const struct m10mo_resolution *resolutions[][3];
extern const ssize_t resolutions_sizes[][3];

struct M10MO_AF_Parameters {
	u8 af_mode;
	u8 af_normal;
	u8 af_macro;
	u8 af_touch;
	u8 af_preview_caf;
	u8 af_movie_caf;
	u8 af_face_caf;
	u8 af_touch_macro;
	u8 af_touch_caf;

	u8 af_execution;
	u8 af_stop;
	u8 af_search;
	u8 af_pan_focusing;

	u8 af_result;
	u8 caf_status_restart_check;
	u8 caf_status_focusing;
	u8 caf_status_success;
	u8 caf_status_fail;
	u8 af_status_invalid;
	u8 af_status_focusing;
	u8 af_status_success;
	u8 af_status_fail;

	u8 af_touch_posx;
	u8 af_touch_posy;
};
extern const struct M10MO_AF_Parameters m10m0_af_parameters[];

#endif	/* M10MO_H */




