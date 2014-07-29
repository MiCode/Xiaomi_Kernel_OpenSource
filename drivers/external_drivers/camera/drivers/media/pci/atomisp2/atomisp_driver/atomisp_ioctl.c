/*
 * Support for Medifield PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
 *
 * Copyright (c) 2010 Silicon Hive www.siliconhive.com.
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include <linux/delay.h>
#ifdef CONFIG_GMIN_INTEL_MID
#include <linux/pci.h>
#endif

#include <asm/intel-mid.h>

#include <media/v4l2-ioctl.h>
#include <media/videobuf-vmalloc.h>

#include "atomisp_acc.h"
#include "atomisp_cmd.h"
#include "atomisp_common.h"
#include "atomisp_fops.h"
#include "atomisp_internal.h"
#include "atomisp_ioctl.h"
#include "atomisp-regs.h"
#include "atomisp_compat.h"

#include "sh_css_hrt.h"

#include "gp_device.h"
#include "device_access.h"
#include "irq.h"

#include "hrt/hive_isp_css_mm_hrt.h"

/* for v4l2_capability */
static const char *DRIVER = "atomisp";	/* max size 15 */
static const char *CARD = "ATOM ISP";	/* max size 31 */
static const char *BUS_INFO = "PCI-3";	/* max size 31 */
static const u32 VERSION = DRIVER_VERSION;

/*
 * FIXME: ISP should not know beforehand all CIDs supported by sensor.
 * Instead, it needs to propagate to sensor unkonwn CIDs.
 */
static struct v4l2_queryctrl ci_v4l2_controls[] = {
	{
		.id = V4L2_CID_AUTO_WHITE_BALANCE,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Automatic White Balance",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_RED_BALANCE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Red Balance",
		.minimum = 0x00,
		.maximum = 0xff,
		.step = 1,
		.default_value = 0x00,
	},
	{
		.id = V4L2_CID_BLUE_BALANCE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Blue Balance",
		.minimum = 0x00,
		.maximum = 0xff,
		.step = 1,
		.default_value = 0x00,
	},
	{
		.id = V4L2_CID_GAMMA,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Gamma",
		.minimum = 0x00,
		.maximum = 0xff,
		.step = 1,
		.default_value = 0x00,
	},
	{
		.id = V4L2_CID_POWER_LINE_FREQUENCY,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "Light frequency filter",
		.minimum = 1,
		.maximum = 2,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_COLORFX,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Image Color Effect",
		.minimum = 0,
		.maximum = 9,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_COLORFX_CBCR,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Image Color Effect CbCr",
		.minimum = 0,
		.maximum = 0xffff,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_ATOMISP_BAD_PIXEL_DETECTION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Bad Pixel Correction",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_ATOMISP_POSTPROCESS_GDC_CAC,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "GDC/CAC",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_ATOMISP_VIDEO_STABLIZATION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Video Stablization",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_ATOMISP_FIXED_PATTERN_NR,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Fixed Pattern Noise Reduction",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_ATOMISP_FALSE_COLOR_CORRECTION,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "False Color Correction",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_REQUEST_FLASH,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Request flash frames",
		.minimum = 0,
		.maximum = 10,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_ATOMISP_LOW_LIGHT,
		.type = V4L2_CTRL_TYPE_BOOLEAN,
		.name = "Low light mode",
		.minimum = 0,
		.maximum = 1,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_BIN_FACTOR_HORZ,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Horizontal binning factor",
		.minimum = 0,
		.maximum = 10,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_BIN_FACTOR_VERT,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Vertical binning factor",
		.minimum = 0,
		.maximum = 10,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_2A_STATUS,
		.type = V4L2_CTRL_TYPE_BITMASK,
		.name = "AE and AWB status",
		.minimum = 0,
		.maximum = V4L2_2A_STATUS_AE_READY | V4L2_2A_STATUS_AWB_READY,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_EXPOSURE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "exposure",
		.minimum = -4,
		.maximum = 4,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_EXPOSURE_ZONE_NUM,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "one-time exposure zone number",
		.minimum = 0x0,
		.maximum = 0xffff,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_EXPOSURE_AUTO_PRIORITY,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Exposure auto priority",
		.minimum = V4L2_EXPOSURE_AUTO,
		.maximum = V4L2_EXPOSURE_APERTURE_PRIORITY,
		.step = 1,
		.default_value = V4L2_EXPOSURE_AUTO,
	},
	{
		.id = V4L2_CID_SCENE_MODE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "scene mode",
		.minimum = 0,
		.maximum = 13,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_ISO_SENSITIVITY,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "iso",
		.minimum = -4,
		.maximum = 4,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_ISO_SENSITIVITY_AUTO,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "iso mode",
		.minimum = V4L2_ISO_SENSITIVITY_MANUAL,
		.maximum = V4L2_ISO_SENSITIVITY_AUTO,
		.step = 1,
		.default_value = V4L2_ISO_SENSITIVITY_AUTO,
	},
	{
		.id = V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "white balance",
		.minimum = 0,
		.maximum = 9,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_EXPOSURE_METERING,
		.type = V4L2_CTRL_TYPE_MENU,
		.name = "metering",
		.minimum = 0,
		.maximum = 3,
		.step = 1,
		.default_value = 1,
	},
	{
		.id = V4L2_CID_3A_LOCK,
		.type = V4L2_CTRL_TYPE_BITMASK,
		.name = "3a lock",
		.minimum = 0,
		.maximum = V4L2_LOCK_EXPOSURE | V4L2_LOCK_WHITE_BALANCE
			 | V4L2_LOCK_FOCUS,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_TEST_PATTERN,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Test Pattern",
		.minimum = 0,
		.maximum = 0xffff,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_TEST_PATTERN_COLOR_R,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Test Pattern Solid Color R",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_TEST_PATTERN_COLOR_GR,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Test Pattern Solid Color GR",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_TEST_PATTERN_COLOR_GB,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Test Pattern Solid Color GB",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
	{
		.id = V4L2_CID_TEST_PATTERN_COLOR_B,
		.type = V4L2_CTRL_TYPE_INTEGER,
		.name = "Test Pattern Solid Color B",
		.minimum = INT_MIN,
		.maximum = INT_MAX,
		.step = 1,
		.default_value = 0,
	},
};
static const u32 ctrls_num = ARRAY_SIZE(ci_v4l2_controls);

/*
 * supported V4L2 fmts and resolutions
 */
const struct atomisp_format_bridge atomisp_output_fmts[] = {
	{
		.pixelformat = V4L2_PIX_FMT_YUV420,
		.depth = 12,
		.mbus_code = V4L2_MBUS_FMT_CUSTOM_YUV420,
		.sh_fmt = CSS_FRAME_FORMAT_YUV420,
		.description = "YUV420, planar",
		.planar = true
	}, {
		.pixelformat = V4L2_PIX_FMT_YVU420,
		.depth = 12,
		.mbus_code = V4L2_MBUS_FMT_CUSTOM_YVU420,
		.sh_fmt = CSS_FRAME_FORMAT_YV12,
		.description = "YVU420, planar",
		.planar = true
	}, {
		.pixelformat = V4L2_PIX_FMT_YUV422P,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_CUSTOM_YUV422P,
		.sh_fmt = CSS_FRAME_FORMAT_YUV422,
		.description = "YUV422, planar",
		.planar = true
	}, {
		.pixelformat = V4L2_PIX_FMT_YUV444,
		.depth = 24,
		.mbus_code = V4L2_MBUS_FMT_CUSTOM_YUV444,
		.sh_fmt = CSS_FRAME_FORMAT_YUV444,
		.description = "YUV444"
	}, {
		.pixelformat = V4L2_PIX_FMT_NV12,
		.depth = 12,
		.mbus_code = V4L2_MBUS_FMT_CUSTOM_NV12,
		.sh_fmt = CSS_FRAME_FORMAT_NV12,
		.description = "NV12, Y-plane, CbCr interleaved",
		.planar = true
	}, {
		.pixelformat = V4L2_PIX_FMT_NV21,
		.depth = 12,
		.mbus_code = V4L2_MBUS_FMT_CUSTOM_NV21,
		.sh_fmt = CSS_FRAME_FORMAT_NV21,
		.description = "NV21, Y-plane, CbCr interleaved",
		.planar = true
	}, {
		.pixelformat = V4L2_PIX_FMT_NV16,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_CUSTOM_NV16,
		.sh_fmt = CSS_FRAME_FORMAT_NV16,
		.description = "NV16, Y-plane, CbCr interleaved",
		.planar = true
	}, {
		.pixelformat = V4L2_PIX_FMT_YUYV,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_CUSTOM_YUYV,
		.sh_fmt = CSS_FRAME_FORMAT_YUYV,
		.description = "YUYV, interleaved"
	}, {
		.pixelformat = V4L2_PIX_FMT_UYVY,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_UYVY8_1X16,
		.sh_fmt = CSS_FRAME_FORMAT_UYVY,
		.description = "UYVY, interleaved"
	}, { /* This one is for parallel sensors! DO NOT USE! */
		.pixelformat = V4L2_PIX_FMT_UYVY,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_UYVY8_2X8,
		.sh_fmt = CSS_FRAME_FORMAT_UYVY,
		.description = "UYVY, interleaved"
	}, {
		.pixelformat = V4L2_PIX_FMT_SBGGR16,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_CUSTOM_SBGGR16,
		.sh_fmt = CSS_FRAME_FORMAT_RAW,
		.description = "Bayer 16"
	}, {
		.pixelformat = V4L2_PIX_FMT_SBGGR8,
		.depth = 8,
		.mbus_code = V4L2_MBUS_FMT_SBGGR8_1X8,
		.sh_fmt = CSS_FRAME_FORMAT_RAW,
		.description = "Bayer 8"
	}, {
		.pixelformat = V4L2_PIX_FMT_SGBRG8,
		.depth = 8,
		.mbus_code = V4L2_MBUS_FMT_SGBRG8_1X8,
		.sh_fmt = CSS_FRAME_FORMAT_RAW,
		.description = "Bayer 8"
	}, {
		.pixelformat = V4L2_PIX_FMT_SGRBG8,
		.depth = 8,
		.mbus_code = V4L2_MBUS_FMT_SGRBG8_1X8,
		.sh_fmt = CSS_FRAME_FORMAT_RAW,
		.description = "Bayer 8"
	}, {
		.pixelformat = V4L2_PIX_FMT_SRGGB8,
		.depth = 8,
		.mbus_code = V4L2_MBUS_FMT_SRGGB8_1X8,
		.sh_fmt = CSS_FRAME_FORMAT_RAW,
		.description = "Bayer 8"
	}, {
		.pixelformat = V4L2_PIX_FMT_SBGGR10,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_SBGGR10_1X10,
		.sh_fmt = CSS_FRAME_FORMAT_RAW,
		.description = "Bayer 10"
	}, {
		.pixelformat = V4L2_PIX_FMT_SGBRG10,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_SGBRG10_1X10,
		.sh_fmt = CSS_FRAME_FORMAT_RAW,
		.description = "Bayer 10"
	}, {
		.pixelformat = V4L2_PIX_FMT_SGRBG10,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_SGRBG10_1X10,
		.sh_fmt = CSS_FRAME_FORMAT_RAW,
		.description = "Bayer 10"
	}, {
		.pixelformat = V4L2_PIX_FMT_SRGGB10,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_SRGGB10_1X10,
		.sh_fmt = CSS_FRAME_FORMAT_RAW,
		.description = "Bayer 10"
	}, {
		.pixelformat = V4L2_PIX_FMT_SBGGR12,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_SBGGR12_1X12,
		.sh_fmt = CSS_FRAME_FORMAT_RAW,
		.description = "Bayer 12"
	}, {
		.pixelformat = V4L2_PIX_FMT_SGBRG12,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_SGBRG12_1X12,
		.sh_fmt = CSS_FRAME_FORMAT_RAW,
		.description = "Bayer 12"
	}, {
		.pixelformat = V4L2_PIX_FMT_SGRBG12,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_SGRBG12_1X12,
		.sh_fmt = CSS_FRAME_FORMAT_RAW,
		.description = "Bayer 12"
	}, {
		.pixelformat = V4L2_PIX_FMT_SRGGB12,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_SRGGB12_1X12,
		.sh_fmt = CSS_FRAME_FORMAT_RAW,
		.description = "Bayer 12"
	}, {
		.pixelformat = V4L2_PIX_FMT_RGB32,
		.depth = 32,
		.mbus_code = V4L2_MBUS_FMT_CUSTOM_RGB32,
		.sh_fmt = CSS_FRAME_FORMAT_RGBA888,
		.description = "32 RGB 8-8-8-8"
	}, {
		.pixelformat = V4L2_PIX_FMT_RGB565,
		.depth = 16,
		.mbus_code = V4L2_MBUS_FMT_BGR565_2X8_LE,
		.sh_fmt = CSS_FRAME_FORMAT_RGB565,
		.description = "16 RGB 5-6-5"
	},{
		.pixelformat = V4L2_PIX_FMT_JPEG,
		.depth = 8,
		.mbus_code = V4L2_MBUS_FMT_JPEG_1X8,
		.sh_fmt = CSS_FRAME_FORMAT_BINARY_8,
		.description = "JPEG"
	},
	{
	/* This is a custom format being used by M10MO to send the RAW data */
		.pixelformat = V4L2_PIX_FMT_CUSTOM_M10MO_RAW,
		.depth = 8,
		.mbus_code = V4L2_MBUS_FMT_CUSTOM_M10MO_RAW,
		.sh_fmt = CSS_FRAME_FORMAT_BINARY_8,
		.description = "Custom RAW for M10MO"
	},
};

const struct atomisp_format_bridge *atomisp_get_format_bridge(
	unsigned int pixelformat)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(atomisp_output_fmts); i++) {
		if (atomisp_output_fmts[i].pixelformat == pixelformat)
			return &atomisp_output_fmts[i];
	}

	return NULL;
}

const struct atomisp_format_bridge *atomisp_get_format_bridge_from_mbus(
	enum v4l2_mbus_pixelcode mbus_code)
{
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(atomisp_output_fmts); i++) {
		if (mbus_code == atomisp_output_fmts[i].mbus_code)
			return &atomisp_output_fmts[i];
	}

	return NULL;
}

/*
 * v4l2 ioctls
 * return ISP capabilities
 *
 * FIXME: capabilities should be different for video0/video2/video3
 */
static int atomisp_querycap(struct file *file, void *fh,
			    struct v4l2_capability *cap)
{
	int ret = 0;

	memset(cap, 0, sizeof(struct v4l2_capability));

	WARN_ON(sizeof(DRIVER) > sizeof(cap->driver) ||
		sizeof(CARD) > sizeof(cap->card) ||
		sizeof(BUS_INFO) > sizeof(cap->bus_info));

	strncpy(cap->driver, DRIVER, sizeof(cap->driver) - 1);
	strncpy(cap->card, CARD, sizeof(cap->card) - 1);
	strncpy(cap->bus_info, BUS_INFO, sizeof(cap->card) - 1);

	cap->version = VERSION;

	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE |
	    V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_OUTPUT;

	return ret;
}

#ifndef CONFIG_GMIN_INTEL_MID
/*
 * return sensor chip identification
 */
static int atomisp_g_chip_ident(struct file *file, void *fh,
	struct v4l2_dbg_chip_ident *chip)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;

	int ret = 0;

	ret = v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
			       core, g_chip_ident, chip);

	if (ret)
		dev_err(isp->dev, "failed to g_chip_ident for sensor\n");
	return ret;
}
#endif
/*
 * enum input are used to check primary/secondary camera
 */
static int atomisp_enum_input(struct file *file, void *fh,
	struct v4l2_input *input)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	int index = input->index;

	if (index >= isp->input_cnt)
		return -EINVAL;

	if (!isp->inputs[index].camera)
		return -EINVAL;

	memset(input, 0, sizeof(struct v4l2_input));
	strncpy(input->name, isp->inputs[index].camera->name,
		sizeof(input->name) - 1);

	/*
	 * HACK: append actuator's name to sensor's
	 * As currently userspace can't talk directly to subdev nodes, this
	 * ioctl is the only way to enum inputs + possible external actuators
	 * for 3A tuning purpose.
	 */
	if (isp->inputs[index].motor &&
	    strlen(isp->inputs[index].motor->name) > 0) {
		const int cur_len = strlen(input->name);
		const int max_size = sizeof(input->name) - cur_len - 1;

		if (max_size > 1) {
			input->name[cur_len] = '+';
			strncpy(&input->name[cur_len + 1],
				isp->inputs[index].motor->name, max_size - 1);
		}
	}

	input->type = V4L2_INPUT_TYPE_CAMERA;
	input->index = index;
	input->reserved[0] = isp->inputs[index].type;
	input->reserved[1] = isp->inputs[index].port;

	return 0;
}

static unsigned int atomisp_subdev_streaming_count(
					struct atomisp_sub_device *asd)
{
	return asd->video_out_preview.capq.streaming
		+ asd->video_out_capture.capq.streaming
		+ asd->video_out_video_capture.capq.streaming
		+ asd->video_out_vf.capq.streaming
		+ asd->video_in.capq.streaming;
}

unsigned int atomisp_streaming_count(struct atomisp_device *isp)
{
	unsigned int i, sum;

	for (i = 0, sum = 0; i < isp->num_of_streams; i++)
		sum += isp->asd[i].streaming ==
		    ATOMISP_DEVICE_STREAMING_ENABLED;

	return sum;
}

/*
 * get input are used to get current primary/secondary camera
 */
static int atomisp_g_input(struct file *file, void *fh, unsigned int *input)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;

	rt_mutex_lock(&isp->mutex);
	*input = asd->input_curr;
	rt_mutex_unlock(&isp->mutex);

	return 0;
}
/*
 * set input are used to set current primary/secondary camera
 */
static int atomisp_s_input(struct file *file, void *fh, unsigned int input)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;
	struct v4l2_subdev *camera = NULL;
	int ret;

	rt_mutex_lock(&isp->mutex);
	if (input >= ATOM_ISP_MAX_INPUTS || input > isp->input_cnt) {
		dev_dbg(isp->dev, "input_cnt: %d\n", isp->input_cnt);
		ret = -EINVAL;
		goto error;
	}

	/*
	 * check whether the request camera:
	 * 1: already in use
	 * 2: if in use, whether it is used by other streams
	 */
	if (isp->inputs[input].asd != NULL && isp->inputs[input].asd != asd) {
		dev_err(isp->dev,
			 "%s, camera is already used by stream: %d\n", __func__,
			 isp->inputs[input].asd->index);
		ret = -EBUSY;
		goto error;
	}

	camera = isp->inputs[input].camera;
	if (!camera) {
		dev_err(isp->dev, "%s, no camera\n", __func__);
		ret = -EINVAL;
		goto error;
	}

	if (atomisp_subdev_streaming_count(asd)) {
		dev_err(isp->dev,
			 "ISP is still streaming, stop first\n");
		ret = -EINVAL;
		goto error;
	}

	/* power off the current owned sensor, as it is not used this time */
	if (isp->inputs[asd->input_curr].asd == asd &&
	    asd->input_curr != input) {
		ret = v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
				       core, s_power, 0);
		if (ret)
			dev_warn(isp->dev,
				    "Failed to power-off sensor\n");
		/* clear the asd field to show this camera is not used */
		isp->inputs[asd->input_curr].asd = NULL;
	}

	/* powe on the new sensor */
	ret = v4l2_subdev_call(isp->inputs[input].camera, core, s_power, 1);
	if (ret) {
		dev_err(isp->dev, "Failed to power-on sensor\n");
		goto error;
	}
	/*
	 * Some sensor driver resets the run mode during power-on, thus force
	 * update the run mode to sensor after power-on.
	 */
	atomisp_update_run_mode(asd);

	/* select operating sensor */
	ret = v4l2_subdev_call(isp->inputs[input].camera, video, s_routing,
		0, isp->inputs[input].sensor_index, 0);
	if (ret && (ret != -ENOIOCTLCMD)) {
		dev_err(isp->dev, "Failed to select sensor\n");
		goto error;
	}

	if (!isp->sw_contex.file_input && isp->inputs[input].motor)
		ret = v4l2_subdev_call(isp->inputs[input].motor, core,
				       init, 1);

	asd->input_curr = input;
	/* mark this camera is used by the current stream */
	isp->inputs[input].asd = asd;
	rt_mutex_unlock(&isp->mutex);

	return 0;

error:
	rt_mutex_unlock(&isp->mutex);

	return ret;
}

static int atomisp_enum_fmt_cap(struct file *file, void *fh,
	struct v4l2_fmtdesc *f)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;
	struct v4l2_subdev_mbus_code_enum code = { 0 };
	unsigned int i, fi = 0;
	int rval;

	rt_mutex_lock(&isp->mutex);
	rval = v4l2_subdev_call(isp->inputs[asd->input_curr].camera, pad,
				enum_mbus_code, NULL, &code);
	if (rval == -ENOIOCTLCMD) {
		dev_warn(isp->dev, "enum_mbus_code pad op not supported. Please fix your sensor driver!\n");
		rval = v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
					video, enum_mbus_fmt, 0, &code.code);
	}
	rt_mutex_unlock(&isp->mutex);

	if (rval)
		return rval;

	for (i = 0; i < ARRAY_SIZE(atomisp_output_fmts); i++) {
		const struct atomisp_format_bridge *format =
			&atomisp_output_fmts[i];

		/*
		 * Is the atomisp-supported format is valid for the
		 * sensor (configuration)? If not, skip it.
		 */
		if (format->sh_fmt == CSS_FRAME_FORMAT_RAW
		    && format->mbus_code != code.code)
			continue;

		/* Found a match. Now let's pick f->index'th one. */
		if (fi < f->index) {
			fi++;
			continue;
		}

		strlcpy(f->description, format->description,
			sizeof(f->description));
		f->pixelformat = format->pixelformat;
		return 0;
	}

	return -EINVAL;
}

static int atomisp_g_fmt_cap(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);

	int ret;

	rt_mutex_lock(&isp->mutex);
	ret = atomisp_get_fmt(vdev, f);
	rt_mutex_unlock(&isp->mutex);
	return ret;
}

static int atomisp_g_fmt_file(struct file *file, void *fh,
		struct v4l2_format *f)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);

	rt_mutex_lock(&isp->mutex);
	f->fmt.pix = pipe->pix;
	rt_mutex_unlock(&isp->mutex);

	return 0;
}

/* This function looks up the closest available resolution. */
static int atomisp_try_fmt_cap(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	int ret;

	rt_mutex_lock(&isp->mutex);
	ret = atomisp_try_fmt(vdev, f, NULL);
	rt_mutex_unlock(&isp->mutex);
	return ret;
}

static int atomisp_s_fmt_cap(struct file *file, void *fh,
	struct v4l2_format *f)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	int ret;

	rt_mutex_lock(&isp->mutex);
	if (isp->isp_fatal_error) {
		ret = -EIO;
		rt_mutex_unlock(&isp->mutex);
		return ret;
	}
	ret = atomisp_set_fmt(vdev, f);
	rt_mutex_unlock(&isp->mutex);
	return ret;
}

static int atomisp_s_fmt_file(struct file *file, void *fh,
				struct v4l2_format *f)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	int ret;

	rt_mutex_lock(&isp->mutex);
	ret = atomisp_set_fmt_file(vdev, f);
	rt_mutex_unlock(&isp->mutex);
	return ret;
}

/*
 * is_resolution_supported - Check whether resolution is supported
 * @width: check resolution width
 * @height: check resolution height
 *
 * Return 1 on supported or 0 otherwise.
*/
static int is_resolution_supported(u32 width, u32 height)
{
	if ((width > ATOM_ISP_MIN_WIDTH) && (width <= ATOM_ISP_MAX_WIDTH) &&
	    (height > ATOM_ISP_MIN_HEIGHT) && (height <= ATOM_ISP_MAX_HEIGHT)) {
		if (!(width % ATOM_ISP_STEP_WIDTH) &&
		    !(height % ATOM_ISP_STEP_HEIGHT))
			return 1;
	}

	return 0;
}

/*
 * This ioctl allows applications to enumerate all frame intervals that the
 * device supports for the given pixel format and frame size.
 *
 * framerate =  1 / frameintervals
 */
static int atomisp_enum_frameintervals(struct file *file, void *fh,
	struct v4l2_frmivalenum *arg)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;
	int ret;

	if (arg->index != 0)
		return -EINVAL;

	if (!atomisp_get_format_bridge(arg->pixel_format))
		return -EINVAL;

	if (!is_resolution_supported(arg->width, arg->height))
		return -EINVAL;

	rt_mutex_lock(&isp->mutex);
	ret = v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
		video, enum_frameintervals, arg);

	if (ret) {
		/* set the FPS to default 15*/
		arg->type = V4L2_FRMIVAL_TYPE_DISCRETE;
		arg->discrete.numerator = 1;
		arg->discrete.denominator = 15;
	}
	rt_mutex_unlock(&isp->mutex);

	return 0;
}

/*
 * Free videobuffer buffer priv data
 */
void atomisp_videobuf_free_buf(struct videobuf_buffer *vb)
{
	struct videobuf_vmalloc_memory *vm_mem;

	if (vb == NULL)
		return;

	vm_mem = vb->priv;
	if (vm_mem && vm_mem->vaddr) {
		atomisp_css_frame_free(vm_mem->vaddr);
		vm_mem->vaddr = NULL;
	}
}

/*
 * this function is used to free video buffer queue
 */
static void atomisp_videobuf_free_queue(struct videobuf_queue *q)
{
	int i;

	for (i = 0; i < VIDEO_MAX_FRAME; i++) {
		atomisp_videobuf_free_buf(q->bufs[i]);
		kfree(q->bufs[i]);
		q->bufs[i] = NULL;
	}
}

int atomisp_alloc_css_stat_bufs(struct atomisp_sub_device *asd,
	uint16_t stream_id)
{
	struct atomisp_device *isp = asd->isp;
	struct atomisp_s3a_buf *s3a_buf = NULL, *_s3a_buf;
	struct atomisp_dis_buf *dis_buf = NULL, *_dis_buf;
	struct atomisp_metadata_buf *md_buf = NULL, *_md_buf;
	int count;
	unsigned int i;

	if (list_empty(&asd->s3a_stats)) {
		count = ATOMISP_CSS_Q_DEPTH +
		        ATOMISP_S3A_BUF_QUEUE_DEPTH_FOR_HAL;
		dev_dbg(isp->dev, "allocating %d 3a buffers\n", count);
		while (count--) {
			s3a_buf = kzalloc(sizeof(struct atomisp_s3a_buf), GFP_KERNEL);
			if (!s3a_buf) {
				dev_err(isp->dev, "s3a stat buf alloc failed\n");
				goto error;
			}

			if (atomisp_css_allocate_stat_buffers(
					asd, stream_id, s3a_buf, NULL, NULL)) {
				kfree(s3a_buf);
				goto error;
			}

			list_add_tail(&s3a_buf->list, &asd->s3a_stats);
		}
	}

	if (list_empty(&asd->dis_stats)) {
		count = ATOMISP_CSS_Q_DEPTH + 1;
		dev_dbg(isp->dev, "allocating %d dis buffers\n", count);
		while (count--) {
			dis_buf = kzalloc(sizeof(struct atomisp_dis_buf), GFP_KERNEL);
			if (!dis_buf) {
				dev_err(isp->dev, "dis stat buf alloc failed\n");
				kfree(s3a_buf);
				goto error;
			}
			if (atomisp_css_allocate_stat_buffers(
					asd, stream_id, NULL, dis_buf, NULL)) {
				kfree(dis_buf);
				goto error;
			}

			list_add_tail(&dis_buf->list, &asd->dis_stats);
		}
	}

	for (i = 0; i < ATOMISP_METADATA_TYPE_NUM; i++) {
		if (list_empty(&asd->metadata[i]) &&
		    list_empty(&asd->metadata_ready[i]) &&
		    list_empty(&asd->metadata_in_css[i])) {
			count = ATOMISP_CSS_Q_DEPTH +
				ATOMISP_METADATA_QUEUE_DEPTH_FOR_HAL;
			dev_dbg(isp->dev, "allocating %d metadata buffers for type %d\n",
			        count, i);
			while (count--) {
				md_buf = kzalloc(sizeof(struct atomisp_metadata_buf),
						 GFP_KERNEL);
				if (!md_buf) {
					dev_err(isp->dev, "metadata buf alloc failed\n");
					goto error;
				}

				if (atomisp_css_allocate_stat_buffers(
						asd, stream_id, NULL, NULL, md_buf)) {
					kfree(md_buf);
					goto error;
				}
				list_add_tail(&md_buf->list, &asd->metadata[i]);
			}
		}
	}
	return 0;

error:
	dev_err(isp->dev, "failed to allocate statistics buffers\n");

	list_for_each_entry_safe(dis_buf, _dis_buf, &asd->dis_stats, list) {
		atomisp_css_free_dis_buffer(dis_buf);
		list_del(&dis_buf->list);
		kfree(dis_buf);
	}

	list_for_each_entry_safe(s3a_buf, _s3a_buf, &asd->s3a_stats, list) {
		atomisp_css_free_3a_buffer(s3a_buf);
		list_del(&s3a_buf->list);
		kfree(s3a_buf);
	}

	for (i = 0; i < ATOMISP_METADATA_TYPE_NUM; i++) {
		list_for_each_entry_safe(md_buf, _md_buf, &asd->metadata[i],
		                         list) {
			atomisp_css_free_metadata_buffer(md_buf);
			list_del(&md_buf->list);
			kfree(md_buf);
		}
	}
	return -ENOMEM;
}

/*
 * Initiate Memory Mapping or User Pointer I/O
 */
int __atomisp_reqbufs(struct file *file, void *fh,
	struct v4l2_requestbuffers *req)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);
	struct atomisp_sub_device *asd = pipe->asd;
	struct atomisp_css_frame_info frame_info;
	struct atomisp_css_frame *frame;
	struct videobuf_vmalloc_memory *vm_mem;
	uint16_t source_pad = atomisp_subdev_source_pad(vdev);
	uint16_t stream_id = atomisp_source_pad_to_stream_id(asd, source_pad);
	int ret = 0, i = 0;

	if (req->count == 0) {
		mutex_lock(&pipe->capq.vb_lock);
		if (!list_empty(&pipe->capq.stream)) {
			videobuf_queue_cancel(&pipe->capq);
		}
		atomisp_videobuf_free_queue(&pipe->capq);
		mutex_unlock(&pipe->capq.vb_lock);
		/* clear request config id */
		memset(pipe->frame_request_config_id,
		       0, VIDEO_MAX_FRAME * sizeof(unsigned int));
		memset(pipe->frame_params,
		       0, VIDEO_MAX_FRAME *
		          sizeof(struct atomisp_css_params_with_list *));
		return 0;
	}

	ret = videobuf_reqbufs(&pipe->capq, req);
	if (ret)
		return ret;

	atomisp_alloc_css_stat_bufs(asd, stream_id);

	/*
	 * for user pointer type, buffers are not really allcated here,
	 * buffers are setup in QBUF operation through v4l2_buffer structure
	 */
	if (req->memory == V4L2_MEMORY_USERPTR)
		return 0;

	ret = atomisp_get_css_frame_info(asd, source_pad, &frame_info);
	if (ret)
		return ret;

	/*
	 * Allocate the real frame here for selected node using our
	 * memory management function
	 */
	for (i = 0; i < req->count; i++) {
		if (atomisp_css_frame_allocate_from_info(&frame, &frame_info))
			goto error;
		vm_mem = pipe->capq.bufs[i]->priv;
		vm_mem->vaddr = frame;
	}

	return ret;

error:
	while (i--) {
		vm_mem = pipe->capq.bufs[i]->priv;
		atomisp_css_frame_free(vm_mem->vaddr);
	}

	if (asd->vf_frame)
		atomisp_css_frame_free(asd->vf_frame);

	return -ENOMEM;
}

int atomisp_reqbufs(struct file *file, void *fh,
	struct v4l2_requestbuffers *req)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	int ret;

	rt_mutex_lock(&isp->mutex);
	ret = __atomisp_reqbufs(file, fh, req);
	rt_mutex_unlock(&isp->mutex);

	return ret;
}

static int atomisp_reqbufs_file(struct file *file, void *fh,
		struct v4l2_requestbuffers *req)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);

	if (req->count == 0) {
		mutex_lock(&pipe->outq.vb_lock);
		atomisp_videobuf_free_queue(&pipe->outq);
		mutex_unlock(&pipe->outq.vb_lock);
		return 0;
	}

	return videobuf_reqbufs(&pipe->outq, req);
}

/* application query the status of a buffer */
static int atomisp_querybuf(struct file *file, void *fh,
	struct v4l2_buffer *buf)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);

	return videobuf_querybuf(&pipe->capq, buf);
}

static int atomisp_querybuf_file(struct file *file, void *fh,
				struct v4l2_buffer *buf)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);

	return videobuf_querybuf(&pipe->outq, buf);
}

/*
 * Applications call the VIDIOC_QBUF ioctl to enqueue an empty (capturing) or
 * filled (output) buffer in the drivers incoming queue.
 */
static int atomisp_qbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	static const int NOFLUSH_FLAGS = V4L2_BUF_FLAG_NO_CACHE_INVALIDATE |
					 V4L2_BUF_FLAG_NO_CACHE_CLEAN;
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);
	struct atomisp_sub_device *asd = pipe->asd;
	struct videobuf_buffer *vb;
	struct videobuf_vmalloc_memory *vm_mem;
	struct atomisp_css_frame_info frame_info;
	struct atomisp_css_frame *handle = NULL;
	u32 length;
	u32 pgnr;
	int ret = 0;

	rt_mutex_lock(&isp->mutex);
	if (isp->isp_fatal_error) {
		ret = -EIO;
		goto error;
	}

	if (asd->streaming == ATOMISP_DEVICE_STREAMING_STOPPING) {
		dev_err(isp->dev, "%s: reject, as ISP at stopping.\n",
				__func__);
		ret = -EIO;
		goto error;
	}

	if (!buf || buf->index >= VIDEO_MAX_FRAME ||
		!pipe->capq.bufs[buf->index]) {
		dev_err(isp->dev, "Invalid index for qbuf.\n");
		ret = -EINVAL;
		goto error;
	}

	/*
	 * For userptr type frame, we convert user space address to physic
	 * address and reprograme out page table properly
	 */
	if (buf->memory == V4L2_MEMORY_USERPTR) {
		struct hrt_userbuffer_attr attributes;
		vb = pipe->capq.bufs[buf->index];
		vm_mem = vb->priv;
		if (!vm_mem) {
			ret = -EINVAL;
			goto error;
		}

		length = vb->bsize;
		pgnr = (length + (PAGE_SIZE - 1)) >> PAGE_SHIFT;

		if (vb->baddr == buf->m.userptr && vm_mem->vaddr)
			goto done;

		if (atomisp_get_css_frame_info(asd,
				atomisp_subdev_source_pad(vdev), &frame_info)) {
			ret = -EIO;
			goto error;
		}

		attributes.pgnr = pgnr;
#ifdef CONFIG_ION
		attributes.type = buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_ION
					? HRT_USR_ION : HRT_USR_PTR;
#else
		attributes.type = HRT_USR_PTR;
#endif
		ret = atomisp_css_frame_map(&handle, &frame_info,
				       (void *)buf->m.userptr,
				       0, &attributes);
		if (ret) {
			dev_err(isp->dev, "Failed to map user buffer\n");
			goto error;
		}

		if (vm_mem->vaddr) {
			mutex_lock(&pipe->capq.vb_lock);
			atomisp_css_frame_free(vm_mem->vaddr);
			vm_mem->vaddr = NULL;
			vb->state = VIDEOBUF_NEEDS_INIT;
			mutex_unlock(&pipe->capq.vb_lock);
		}

		vm_mem->vaddr = handle;

		buf->flags &= ~V4L2_BUF_FLAG_MAPPED;
		buf->flags |= V4L2_BUF_FLAG_QUEUED;
		buf->flags &= ~V4L2_BUF_FLAG_DONE;
	} else if (buf->memory == V4L2_MEMORY_MMAP) {
		buf->flags |= V4L2_BUF_FLAG_MAPPED;
		buf->flags |= V4L2_BUF_FLAG_QUEUED;
		buf->flags &= ~V4L2_BUF_FLAG_DONE;
	}

done:
	if (!((buf->flags & NOFLUSH_FLAGS) == NOFLUSH_FLAGS))
		wbinvd();

	if (!atomisp_is_vf_pipe(pipe) &&
	    (buf->reserved2 & ATOMISP_BUFFER_HAS_PER_FRAME_SETTING)) {
		/* this buffer will have a per-frame parameter */
		pipe->frame_request_config_id[buf->index] = buf->reserved2 &
					~ATOMISP_BUFFER_HAS_PER_FRAME_SETTING;
		dev_dbg(isp->dev, "This buffer requires per_frame setting which has isp_config_id %d\n",
		        pipe->frame_request_config_id[buf->index]);
	} else {
		pipe->frame_request_config_id[buf->index] = 0;
	}

	pipe->frame_params[buf->index] = NULL;

	rt_mutex_unlock(&isp->mutex);

	ret = videobuf_qbuf(&pipe->capq, buf);
	rt_mutex_lock(&isp->mutex);
	if (ret)
		goto error;

	/* TODO: do this better, not best way to queue to css */
	if (asd->streaming == ATOMISP_DEVICE_STREAMING_ENABLED) {
		if (!list_empty(&pipe->buffers_waiting_for_param)) {
			atomisp_handle_parameter_and_buffer(pipe);
		} else {
			atomisp_qbuffers_to_css(asd);

			if (!atomisp_is_wdt_running(isp) && atomisp_buffers_queued(asd))
				atomisp_wdt_start(isp);
		}
	}

	/* Workaround: Due to the design of HALv3,
	 * sometimes in ZSL or SDV mode HAL needs to
	 * capture multiple images within one streaming cycle.
	 * But the capture number cannot be determined by HAL.
	 * So HAL only sets the capture number to be 1 and queue multiple
	 * buffers. Atomisp driver needs to check this case and re-trigger
	 * CSS to do capture when new buffer is queued. */
	if (asd->continuous_mode->val &&
	    atomisp_subdev_source_pad(vdev)
	    == ATOMISP_SUBDEV_PAD_SOURCE_CAPTURE &&
	    pipe->capq.streaming &&
	    !asd->enable_raw_buffer_lock->val &&
	    asd->params.offline_parm.num_captures == 1) {
		asd->pending_capture_request++;
		dev_dbg(isp->dev, "Add one pending capture request.\n");
	}
	rt_mutex_unlock(&isp->mutex);

	dev_dbg(isp->dev, "qbuf buffer %d (%s)\n", buf->index, vdev->name);

	return ret;

error:
	rt_mutex_unlock(&isp->mutex);
	return ret;
}

static int atomisp_qbuf_file(struct file *file, void *fh,
					struct v4l2_buffer *buf)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);
	int ret;

	rt_mutex_lock(&isp->mutex);
	if (isp->isp_fatal_error) {
		ret = -EIO;
		goto error;
	}

	if (!buf || buf->index >= VIDEO_MAX_FRAME ||
		!pipe->outq.bufs[buf->index]) {
		dev_err(isp->dev, "Invalid index for qbuf.\n");
		ret = -EINVAL;
		goto error;
	}

	if (buf->memory != V4L2_MEMORY_MMAP) {
		dev_err(isp->dev, "Unsupported memory method\n");
		ret = -EINVAL;
		goto error;
	}

	if (buf->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		dev_err(isp->dev, "Unsupported buffer type\n");
		ret = -EINVAL;
		goto error;
	}
	rt_mutex_unlock(&isp->mutex);

	return videobuf_qbuf(&pipe->outq, buf);

error:
	rt_mutex_unlock(&isp->mutex);

	return ret;
}

static int __get_frame_exp_id(struct atomisp_video_pipe *pipe,
		struct v4l2_buffer *buf)
{
	struct videobuf_vmalloc_memory *vm_mem;
	struct atomisp_css_frame *handle;
	int i;

	for (i = 0; pipe->capq.bufs[i]; i++) {
		vm_mem = pipe->capq.bufs[i]->priv;
		handle = vm_mem->vaddr;
		if (buf->index == pipe->capq.bufs[i]->i && handle)
			return handle->exp_id;
	}
	return -EINVAL;
}

/*
 * Applications call the VIDIOC_DQBUF ioctl to dequeue a filled (capturing) or
 * displayed (output buffer)from the driver's outgoing queue
 */
static int atomisp_dqbuf(struct file *file, void *fh, struct v4l2_buffer *buf)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);
	struct atomisp_sub_device *asd = pipe->asd;
	struct atomisp_device *isp = video_get_drvdata(vdev);
	int ret = 0;

	rt_mutex_lock(&isp->mutex);

	if (isp->isp_fatal_error) {
		rt_mutex_unlock(&isp->mutex);
		return -EIO;
	}

	if (asd->streaming == ATOMISP_DEVICE_STREAMING_STOPPING) {
		rt_mutex_unlock(&isp->mutex);
		dev_err(isp->dev, "%s: reject, as ISP at stopping.\n",
				__func__);
		return -EIO;
	}

	rt_mutex_unlock(&isp->mutex);

	ret = videobuf_dqbuf(&pipe->capq, buf, file->f_flags & O_NONBLOCK);
	if (ret) {
		dev_dbg(isp->dev, "<%s: %d\n", __func__, ret);
		return ret;
	}
	rt_mutex_lock(&isp->mutex);
	buf->bytesused = pipe->pix.sizeimage;
	buf->reserved = asd->frame_status[buf->index];
	/*
	 * Hack:
	 * Currently frame_status in the enum type which takes no more lower
	 * 8 bit.
	 * use bit[31:16] for exp_id as it is only in the range of 1~255
	 */
	buf->reserved &= 0x0000ffff;
	buf->reserved |= __get_frame_exp_id(pipe, buf) << 16;
	buf->reserved2 = pipe->frame_config_id[buf->index];
	rt_mutex_unlock(&isp->mutex);

	dev_dbg(isp->dev, "dqbuf buffer %d (%s) with exp_id %d, isp_config_id %d\n",
		buf->index, vdev->name, __get_frame_exp_id(pipe, buf),
		buf->reserved2);
	return 0;
}

enum atomisp_css_pipe_id atomisp_get_css_pipe_id(struct atomisp_sub_device *asd)
{
	if (ATOMISP_USE_YUVPP(asd))
		return CSS_PIPE_ID_YUVPP;

	if (asd->continuous_mode->val) {
		if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO)
			return CSS_PIPE_ID_VIDEO;
		else
			return CSS_PIPE_ID_PREVIEW;
	}

	/*
	 * Disable vf_pp and run CSS in video mode. This allows using ISP
	 * scaling but it has one frame delay due to CSS internal buffering.
	 */
	if (asd->vfpp->val == ATOMISP_VFPP_DISABLE_SCALER)
		return CSS_PIPE_ID_VIDEO;

	/*
	 * Disable vf_pp and run CSS in still capture mode. In this mode
	 * CSS does not cause extra latency with buffering, but scaling
	 * is not available.
	 */
	if (asd->vfpp->val == ATOMISP_VFPP_DISABLE_LOWLAT)
		return CSS_PIPE_ID_CAPTURE;

	switch (asd->run_mode->val) {
	case ATOMISP_RUN_MODE_PREVIEW:
		return CSS_PIPE_ID_PREVIEW;
	case ATOMISP_RUN_MODE_VIDEO:
		return CSS_PIPE_ID_VIDEO;
	case ATOMISP_RUN_MODE_STILL_CAPTURE:
		/* fall through */
	default:
		return CSS_PIPE_ID_CAPTURE;
	}
}

static unsigned int atomisp_sensor_start_stream(struct atomisp_sub_device *asd)
{
	struct atomisp_device *isp = asd->isp;

	if (isp->inputs[asd->input_curr].camera_caps->
	    sensor[asd->sensor_curr].stream_num > 1) {
		if (asd->high_speed_mode)
			return 1;
		else
			return 2;
	}

	if (asd->vfpp->val != ATOMISP_VFPP_ENABLE ||
	    asd->copy_mode)
		return 1;

	if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO ||
	    (asd->run_mode->val == ATOMISP_RUN_MODE_STILL_CAPTURE &&
	     !atomisp_is_mbuscode_raw(
		     asd->fmt[
			     asd->capture_pad].fmt.code) &&
	     !asd->continuous_mode->val))
		return 2;
	else
		return 1;
}
static int __stream_on_master_slave_sensor(struct atomisp_device *isp)
{
	unsigned int master = -1, slave = -1;
	int i, ret;

	/*
	 * ISP only support 2 streams now so ignore multiple master/slave
	 * case to reduce the delay between 2 stream_on calls.
	 */
	for (i = 0; i < isp->num_of_streams; i++) {
		int sensor_index = isp->asd[i].input_curr;
		if (isp->inputs[sensor_index].camera_caps->
				sensor[isp->asd[i].sensor_curr].is_slave)
			slave = sensor_index;
		else
			master = sensor_index;
	}

	if (master == -1 || slave == -1) {
		dev_err(isp->dev, "depth mode needs 2 sensors to be selected.\n");
		return -EINVAL;
	}

	ret = v4l2_subdev_call(isp->inputs[master].camera, video, s_stream, 1);
	if (ret) {
		dev_err(isp->dev, "depth mode master sensor %s stream-on failed.\n",
			isp->inputs[master].camera->name);
		return -EINVAL;
	}

	ret = v4l2_subdev_call(isp->inputs[slave].camera, video, s_stream, 1);
	if (ret) {
		dev_err(isp->dev, "depth mode slave sensor %s stream-on failed.\n",
			isp->inputs[slave].camera->name);
		v4l2_subdev_call(isp->inputs[master].camera, video, s_stream, 0);

		return -EINVAL;
	}

	return 0;
}
/*
 * This ioctl start the capture during streaming I/O.
 */
static int atomisp_streamon(struct file *file, void *fh,
	enum v4l2_buf_type type)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);
	struct atomisp_sub_device *asd = pipe->asd;
	struct atomisp_device *isp = video_get_drvdata(vdev);
	enum atomisp_css_pipe_id css_pipe_id;
	unsigned int sensor_start_stream;
	unsigned int wdt_duration = ATOMISP_ISP_TIMEOUT_DURATION;
	int ret = 0;
	unsigned long irqflags;

	if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		dev_dbg(isp->dev, "unsupported v4l2 buf type\n");
		return -EINVAL;
	}

	rt_mutex_lock(&isp->mutex);
	if (isp->isp_fatal_error) {
		ret = -EIO;
		goto out;
	}

	if (asd->streaming == ATOMISP_DEVICE_STREAMING_STOPPING) {
		ret = -EBUSY;
		goto out;
	}

	if (pipe->capq.streaming)
		goto out;

	/*
	 * The number of streaming video nodes is based on which
	 * binary is going to be run.
	 */
	sensor_start_stream = atomisp_sensor_start_stream(asd);

	spin_lock_irqsave(&pipe->irq_lock, irqflags);
	if (list_empty(&(pipe->capq.stream))) {
		spin_unlock_irqrestore(&pipe->irq_lock, irqflags);
		dev_dbg(isp->dev, "no buffer in the queue\n");
		ret = -EINVAL;
		goto out;
	}
	spin_unlock_irqrestore(&pipe->irq_lock, irqflags);

	ret = videobuf_streamon(&pipe->capq);
	if (ret)
		goto out;

	/* Reset pending capture request count. */
	asd->pending_capture_request = 0;

	if ((atomisp_subdev_streaming_count(asd) > sensor_start_stream) &&
	    (!isp->inputs[asd->input_curr].camera_caps->multi_stream_ctrl)) {
		/* trigger still capture */
		if (asd->continuous_mode->val &&
		    atomisp_subdev_source_pad(vdev)
		    == ATOMISP_SUBDEV_PAD_SOURCE_CAPTURE) {
			if (asd->run_mode->val == ATOMISP_RUN_MODE_VIDEO)
			    dev_dbg(isp->dev,
				"SDV last video raw buffer id: %u\n",
				asd->latest_preview_exp_id);
			else
			    dev_dbg(isp->dev,
				"ZSL last preview raw buffer id: %u\n",
				asd->latest_preview_exp_id);

			if (asd->delayed_init == ATOMISP_DELAYED_INIT_QUEUED) {
				flush_work(&asd->delayed_init_work);
				rt_mutex_unlock(&isp->mutex);
				if (wait_for_completion_interruptible(
						&asd->init_done) != 0)
					return -ERESTARTSYS;
				rt_mutex_lock(&isp->mutex);
			}

			/* handle per_frame_setting parameter and buffers */
			atomisp_handle_parameter_and_buffer(pipe);

			/*
			 * only ZSL/SDV capture request will be here, raise
			 * the ISP freq to the highest possible to minimize
			 * the S2S latency.
			 */
			atomisp_freq_scaling(isp, ATOMISP_DFS_MODE_MAX, false);
			/*
			 * When asd->enable_raw_buffer_lock->val is true,
			 * An extra IOCTL is needed to call
			 * atomisp_css_exp_id_capture and trigger real capture
			 */
			if (!asd->enable_raw_buffer_lock->val) {
				ret = atomisp_css_offline_capture_configure(asd,
					asd->params.offline_parm.num_captures,
					asd->params.offline_parm.skip_frames,
					asd->params.offline_parm.offset);
				if (ret) {
					ret = -EINVAL;
					goto out;
				}
			}
		}
		atomisp_qbuffers_to_css(asd);
		goto out;
	}

	if (asd->streaming == ATOMISP_DEVICE_STREAMING_ENABLED) {
		atomisp_qbuffers_to_css(asd);
		goto start_sensor;
	}

	css_pipe_id = atomisp_get_css_pipe_id(asd);

	ret = atomisp_acc_load_extensions(asd);
	if (ret < 0) {
		dev_err(isp->dev, "acc extension failed to load\n");
		goto out;
	}
	ret = atomisp_css_start(asd, css_pipe_id, false);
	if (ret)
		goto out;


	/* Make sure that update_isp_params is called at least once.*/
	asd->params.css_update_params_needed = true;
	asd->streaming = ATOMISP_DEVICE_STREAMING_ENABLED;
	atomic_set(&asd->sof_count, -1);
	atomic_set(&asd->sequence, -1);
	atomic_set(&asd->sequence_temp, -1);
	if (isp->sw_contex.file_input)
		wdt_duration = ATOMISP_ISP_FILE_TIMEOUT_DURATION;

	isp->sw_contex.invalid_frame = false;
	asd->params.dis_proj_data_valid = false;
	asd->latest_preview_exp_id = 0;

	/* handle per_frame_setting parameter and buffers */
	atomisp_handle_parameter_and_buffer(pipe);

	atomisp_qbuffers_to_css(asd);

	/* Only start sensor when the last streaming instance started */
	if (atomisp_subdev_streaming_count(asd) < sensor_start_stream)
		goto out;

start_sensor:
	if (isp->flash) {
		asd->params.num_flash_frames = 0;
		asd->params.flash_state = ATOMISP_FLASH_IDLE;
		atomisp_setup_flash(asd);
	}

	if (!isp->sw_contex.file_input) {
		atomisp_css_irq_enable(isp, CSS_IRQ_INFO_CSS_RECEIVER_SOF,
				atomisp_css_valid_sof(isp));
		atomisp_csi2_configure(asd);

		if (atomisp_freq_scaling(isp, ATOMISP_DFS_MODE_AUTO, false) < 0)
			dev_dbg(isp->dev, "dfs failed!\n");
	} else {
		if (atomisp_freq_scaling(isp, ATOMISP_DFS_MODE_MAX, false) < 0)
			dev_dbg(isp->dev, "dfs failed!\n");
	}

	if (asd->depth_mode->val && atomisp_streaming_count(isp) ==
			ATOMISP_DEPTH_SENSOR_STREAMON_COUNT) {
		ret = __stream_on_master_slave_sensor(isp);
		if (ret) {
			dev_err(isp->dev, "master slave sensor stream on failed!\n");
			goto out;
		}
		goto wdt_start;
	} else if (asd->depth_mode->val && (atomisp_streaming_count(isp) <
		   ATOMISP_DEPTH_SENSOR_STREAMON_COUNT)) {
		goto out;
	}

	/* Enable the CSI interface on ANN B0/K0 */
	if (isp->media_dev.hw_revision >= ((ATOMISP_HW_REVISION_ISP2401 <<
	    ATOMISP_HW_REVISION_SHIFT) | ATOMISP_HW_STEPPING_B0)) {
		pci_write_config_word(isp->pdev, MRFLD_PCI_CSI_CONTROL,
				      isp->saved_regs.csi_control |
				      MRFLD_PCI_CSI_CONTROL_CSI_READY);
	}

	/* stream on the sensor */
	ret = v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
			       video, s_stream, 1);
	if (ret) {
		asd->streaming = ATOMISP_DEVICE_STREAMING_DISABLED;
		ret = -EINVAL;
		goto out;
	}
wdt_start:
	if (asd->continuous_mode->val) {
		struct v4l2_mbus_framefmt *sink;

		sink = atomisp_subdev_get_ffmt(&asd->subdev, NULL,
				       V4L2_SUBDEV_FORMAT_ACTIVE,
				       ATOMISP_SUBDEV_PAD_SINK);

#ifndef CONFIG_GMIN_INTEL_MID
		INIT_COMPLETION(asd->init_done);
#else
		reinit_completion(&asd->init_done);
#endif
		asd->delayed_init = ATOMISP_DELAYED_INIT_QUEUED;
		queue_work(asd->delayed_init_workq, &asd->delayed_init_work);
		atomisp_css_set_cont_prev_start_time(isp,
				ATOMISP_CALC_CSS_PREV_OVERLAP(sink->height));
	} else {
		asd->delayed_init = ATOMISP_DELAYED_INIT_NOT_QUEUED;
	}

	if (atomisp_buffers_queued(asd))
		atomisp_wdt_refresh(isp, wdt_duration);
out:
	rt_mutex_unlock(&isp->mutex);
	return ret;
}

int __atomisp_streamoff(struct file *file, void *fh, enum v4l2_buf_type type)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_video_pipe *pipe = atomisp_to_video_pipe(vdev);
	struct atomisp_sub_device *asd = pipe->asd;
	struct atomisp_video_pipe *capture_pipe = NULL;
	struct atomisp_video_pipe *vf_pipe = NULL;
	struct atomisp_video_pipe *preview_pipe = NULL;
	struct atomisp_video_pipe *video_pipe = NULL;
	struct videobuf_buffer *vb, *_vb;
	enum atomisp_css_pipe_id css_pipe_id;
	int ret;
	unsigned long flags;
	bool first_streamoff = false;

	BUG_ON(!rt_mutex_is_locked(&isp->mutex));
	BUG_ON(!mutex_is_locked(&isp->streamoff_mutex));

	if (type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		dev_dbg(isp->dev, "unsupported v4l2 buf type\n");
		return -EINVAL;
	}

	/*
	 * do only videobuf_streamoff for capture & vf pipes in
	 * case of continuous capture
	 */
	if ((asd->continuous_mode->val ||
	    isp->inputs[asd->input_curr].camera_caps->multi_stream_ctrl) &&
	    atomisp_subdev_source_pad(vdev) !=
		ATOMISP_SUBDEV_PAD_SOURCE_PREVIEW &&
	    atomisp_subdev_source_pad(vdev) !=
		ATOMISP_SUBDEV_PAD_SOURCE_VIDEO) {

		if (isp->inputs[asd->input_curr].camera_caps->multi_stream_ctrl) {
			v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
				video, s_stream, 0);
		} else if (atomisp_subdev_source_pad(vdev)
		    == ATOMISP_SUBDEV_PAD_SOURCE_CAPTURE &&
		    asd->params.offline_parm.num_captures == -1) {
			/* stop continuous still capture if needed */
			atomisp_css_offline_capture_configure(asd, 0, 0, 0);
			atomisp_freq_scaling(isp, ATOMISP_DFS_MODE_AUTO, false);
		}
		/*
		 * Currently there is no way to flush buffers queued to css.
		 * When doing videobuf_streamoff, active buffers will be
		 * marked as VIDEOBUF_NEEDS_INIT. HAL will be able to use
		 * these buffers again, and these buffers might be queued to
		 * css more than once! Warn here, if HAL has not dequeued all
		 * buffers back before calling streamoff.
		 */
		if (pipe->buffers_in_css != 0) {
			WARN(1, "%s: buffers of vdev %s still in CSS!\n",
			     __func__, pipe->vdev.name);

			/*
			 * Buffers remained in css maybe dequeued out in the
			 * next stream on, while this will causes serious
			 * issues as buffers already get invalid after
			 * previous stream off.
			 *
			 * No way to flush buffers but to reset the whole css
			 */
			dev_warn(isp->dev, "Reset CSS to clean up css buffers.\n");
			atomisp_css_flush(isp);
		}

		return videobuf_streamoff(&pipe->capq);
	}

	if (!pipe->capq.streaming)
		return 0;

	spin_lock_irqsave(&isp->lock, flags);
	if (asd->streaming == ATOMISP_DEVICE_STREAMING_ENABLED) {
		asd->streaming = ATOMISP_DEVICE_STREAMING_STOPPING;
		first_streamoff = true;
	}
	spin_unlock_irqrestore(&isp->lock, flags);

	if (first_streamoff) {
		/* if other streams are running, should not disable watch dog */
		rt_mutex_unlock(&isp->mutex);
		if (!atomisp_streaming_count(isp)) {
			atomisp_wdt_stop(isp, true);
		}

		/*
		 * must stop sending pixels into GP_FIFO before stop
		 * the pipeline.
		 */
		if (isp->sw_contex.file_input)
			v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
					video, s_stream, 0);

		rt_mutex_lock(&isp->mutex);
		atomisp_acc_unload_extensions(asd);
	}

	spin_lock_irqsave(&isp->lock, flags);
	if (atomisp_subdev_streaming_count(asd) == 1)
		asd->streaming = ATOMISP_DEVICE_STREAMING_DISABLED;
	spin_unlock_irqrestore(&isp->lock, flags);

	if (!first_streamoff) {
		ret = videobuf_streamoff(&pipe->capq);
		if (ret)
			return ret;
		goto stopsensor;
	}

	atomisp_clear_css_buffer_counters(asd);

	if (!isp->sw_contex.file_input)
		atomisp_css_irq_enable(isp, CSS_IRQ_INFO_CSS_RECEIVER_SOF,
					false);

	if (asd->delayed_init == ATOMISP_DELAYED_INIT_QUEUED) {
		cancel_work_sync(&asd->delayed_init_work);
		asd->delayed_init = ATOMISP_DELAYED_INIT_NOT_QUEUED;
	}

	css_pipe_id = atomisp_get_css_pipe_id(asd);
	ret = atomisp_css_stop(asd, css_pipe_id, false);

	/* cancel work queue*/
	if (asd->video_out_capture.users) {
		capture_pipe = &asd->video_out_capture;
		wake_up_interruptible(&capture_pipe->capq.wait);
	}
	if (asd->video_out_vf.users) {
		vf_pipe = &asd->video_out_vf;
		wake_up_interruptible(&vf_pipe->capq.wait);
	}
	if (asd->video_out_preview.users) {
		preview_pipe = &asd->video_out_preview;
		wake_up_interruptible(&preview_pipe->capq.wait);
	}
	if (asd->video_out_video_capture.users) {
		video_pipe = &asd->video_out_video_capture;
		wake_up_interruptible(&video_pipe->capq.wait);
	}
	ret = videobuf_streamoff(&pipe->capq);
	if (ret)
		return ret;

	/* cleanup css here */
	/* no need for this, as ISP will be reset anyway */
	/*atomisp_flush_bufs_in_css(isp);*/

	spin_lock_irqsave(&pipe->irq_lock, flags);
	list_for_each_entry_safe(vb, _vb, &pipe->activeq, queue) {
		vb->state = VIDEOBUF_PREPARED;
		list_del(&vb->queue);
	}
	list_for_each_entry_safe(vb, _vb, &pipe->buffers_waiting_for_param, queue) {
		vb->state = VIDEOBUF_PREPARED;
		list_del(&vb->queue);
		pipe->frame_request_config_id[vb->i] = 0;
	}
	spin_unlock_irqrestore(&pipe->irq_lock, flags);

	atomisp_subdev_cleanup_pending_events(asd);
stopsensor:
	if (atomisp_subdev_streaming_count(asd) + 1
	    != atomisp_sensor_start_stream(asd))
		return 0;

	if (!isp->sw_contex.file_input)
		ret = v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
				       video, s_stream, 0);

	if (isp->flash) {
		asd->params.num_flash_frames = 0;
		asd->params.flash_state = ATOMISP_FLASH_IDLE;
	}

	/* if other streams are running, isp should not be powered off */
	if (atomisp_streaming_count(isp)) {
		atomisp_css_flush(isp);
		return 0;
	}

	/* Disable the CSI interface on ANN B0/K0 */
	if (isp->media_dev.hw_revision >= ((ATOMISP_HW_REVISION_ISP2401 <<
	    ATOMISP_HW_REVISION_SHIFT) | ATOMISP_HW_STEPPING_B0)) {
		pci_write_config_word(isp->pdev, MRFLD_PCI_CSI_CONTROL,
				      isp->saved_regs.csi_control &
				      ~MRFLD_PCI_CSI_CONTROL_CSI_READY);
	}

	if (atomisp_freq_scaling(isp, ATOMISP_DFS_MODE_LOW, false))
		dev_warn(isp->dev, "DFS failed.\n");
	/*
	 * ISP work around, need to reset isp
	 * Is it correct time to reset ISP when first node does streamoff?
	 */
	if (isp->sw_contex.power_state == ATOM_ISP_POWER_UP) {
		unsigned int i;
		if (isp->isp_timeout)
			dev_err(isp->dev, "%s: Resetting with WA activated",
				__func__);
		/*
		 * It is possible that the other asd stream is in the stage
		 * that v4l2_setfmt is just get called on it, which will
		 * create css stream on that stream. But at this point, there
		 * is no way to destroy the css stream created on that stream.
		 *
		 * So force stream destroy here.
		 */
		for (i = 0; i < isp->num_of_streams; i++) {
			if (isp->asd[i].stream_prepared)
				atomisp_destroy_pipes_stream_force(&isp->
						asd[i]);
		}

		/* disable  PUNIT/ISP acknowlede/handshake - SRSE=3 */
		pci_write_config_dword(isp->pdev, PCI_I_CONTROL, isp->saved_regs.i_control |
				MRFLD_PCI_I_CONTROL_SRSE_RESET_MASK);
		dev_err(isp->dev, "atomisp_reset");
		atomisp_reset(isp);
		isp->isp_timeout = false;
	}
	return ret;
}

static int atomisp_streamoff(struct file *file, void *fh,
			     enum v4l2_buf_type type)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	int rval;

	mutex_lock(&isp->streamoff_mutex);
	rt_mutex_lock(&isp->mutex);
	rval = __atomisp_streamoff(file, fh, type);
	rt_mutex_unlock(&isp->mutex);
	mutex_unlock(&isp->streamoff_mutex);

	return rval;
}

/*
 * To get the current value of a control.
 * applications initialize the id field of a struct v4l2_control and
 * call this ioctl with a pointer to this structure
 */
static int atomisp_g_ctrl(struct file *file, void *fh,
	struct v4l2_control *control)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;
	struct atomisp_device *isp = video_get_drvdata(vdev);
	int i, ret = -EINVAL;

	for (i = 0; i < ctrls_num; i++) {
		if (ci_v4l2_controls[i].id == control->id) {
			ret = 0;
			break;
		}
	}

	if (ret)
		return ret;

	rt_mutex_lock(&isp->mutex);

	switch (control->id) {
	case V4L2_CID_IRIS_ABSOLUTE:
	case V4L2_CID_EXPOSURE_ABSOLUTE:
	case V4L2_CID_FNUMBER_ABSOLUTE:
	case V4L2_CID_2A_STATUS:
	case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
	case V4L2_CID_EXPOSURE:
	case V4L2_CID_SCENE_MODE:
	case V4L2_CID_ISO_SENSITIVITY:
	case V4L2_CID_ISO_SENSITIVITY_AUTO:
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
	case V4L2_CID_SHARPNESS:
	case V4L2_CID_3A_LOCK:
	case V4L2_CID_EXPOSURE_ZONE_NUM:
	case V4L2_CID_TEST_PATTERN:
	case V4L2_CID_TEST_PATTERN_COLOR_R:
	case V4L2_CID_TEST_PATTERN_COLOR_GR:
	case V4L2_CID_TEST_PATTERN_COLOR_GB:
	case V4L2_CID_TEST_PATTERN_COLOR_B:
		rt_mutex_unlock(&isp->mutex);
		return v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
				       core, g_ctrl, control);
	case V4L2_CID_COLORFX:
		ret = atomisp_color_effect(asd, 0, &control->value);
		break;
	case V4L2_CID_ATOMISP_BAD_PIXEL_DETECTION:
		ret = atomisp_bad_pixel(asd, 0, &control->value);
		break;
	case V4L2_CID_ATOMISP_POSTPROCESS_GDC_CAC:
		ret = atomisp_gdc_cac(asd, 0, &control->value);
		break;
	case V4L2_CID_ATOMISP_VIDEO_STABLIZATION:
		ret = atomisp_video_stable(asd, 0, &control->value);
		break;
	case V4L2_CID_ATOMISP_FIXED_PATTERN_NR:
		ret = atomisp_fixed_pattern(asd, 0, &control->value);
		break;
	case V4L2_CID_ATOMISP_FALSE_COLOR_CORRECTION:
		ret = atomisp_false_color(asd, 0, &control->value);
		break;
	case V4L2_CID_ATOMISP_LOW_LIGHT:
		ret = atomisp_low_light(asd, 0, &control->value);
		break;
	default:
		ret = -EINVAL;
		break;
	}

	rt_mutex_unlock(&isp->mutex);
	return ret;
}

/*
 * To change the value of a control.
 * applications initialize the id and value fields of a struct v4l2_control
 * and call this ioctl.
 */
static int atomisp_s_ctrl(struct file *file, void *fh,
			  struct v4l2_control *control)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;
	struct atomisp_device *isp = video_get_drvdata(vdev);
	int i, ret = -EINVAL;

	for (i = 0; i < ctrls_num; i++) {
		if (ci_v4l2_controls[i].id == control->id) {
			ret = 0;
			break;
		}
	}

	if (ret)
		return ret;

	rt_mutex_lock(&isp->mutex);
	switch (control->id) {
	case V4L2_CID_AUTO_N_PRESET_WHITE_BALANCE:
	case V4L2_CID_EXPOSURE:
	case V4L2_CID_EXPOSURE_AUTO_PRIORITY:
	case V4L2_CID_SCENE_MODE:
	case V4L2_CID_ISO_SENSITIVITY:
	case V4L2_CID_ISO_SENSITIVITY_AUTO:
	case V4L2_CID_POWER_LINE_FREQUENCY:
	case V4L2_CID_EXPOSURE_METERING:
	case V4L2_CID_CONTRAST:
	case V4L2_CID_SATURATION:
	case V4L2_CID_SHARPNESS:
	case V4L2_CID_3A_LOCK:
	case V4L2_CID_COLORFX_CBCR:
	case V4L2_CID_TEST_PATTERN:
	case V4L2_CID_TEST_PATTERN_COLOR_R:
	case V4L2_CID_TEST_PATTERN_COLOR_GR:
	case V4L2_CID_TEST_PATTERN_COLOR_GB:
	case V4L2_CID_TEST_PATTERN_COLOR_B:
		rt_mutex_unlock(&isp->mutex);
		return v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
				       core, s_ctrl, control);
	case V4L2_CID_COLORFX:
		ret = atomisp_color_effect(asd, 1, &control->value);
		break;
	case V4L2_CID_ATOMISP_BAD_PIXEL_DETECTION:
		ret = atomisp_bad_pixel(asd, 1, &control->value);
		break;
	case V4L2_CID_ATOMISP_POSTPROCESS_GDC_CAC:
		ret = atomisp_gdc_cac(asd, 1, &control->value);
		break;
	case V4L2_CID_ATOMISP_VIDEO_STABLIZATION:
		ret = atomisp_video_stable(asd, 1, &control->value);
		break;
	case V4L2_CID_ATOMISP_FIXED_PATTERN_NR:
		ret = atomisp_fixed_pattern(asd, 1, &control->value);
		break;
	case V4L2_CID_ATOMISP_FALSE_COLOR_CORRECTION:
		ret = atomisp_false_color(asd, 1, &control->value);
		break;
	case V4L2_CID_REQUEST_FLASH:
		ret = atomisp_flash_enable(asd, control->value);
		break;
	case V4L2_CID_ATOMISP_LOW_LIGHT:
		ret = atomisp_low_light(asd, 1, &control->value);
		break;
	default:
		ret = -EINVAL;
		break;
	}
	rt_mutex_unlock(&isp->mutex);
	return ret;
}
/*
 * To query the attributes of a control.
 * applications set the id field of a struct v4l2_queryctrl and call the
 * this ioctl with a pointer to this structure. The driver fills
 * the rest of the structure.
 */
static int atomisp_queryctl(struct file *file, void *fh,
			    struct v4l2_queryctrl *qc)
{
	int i, ret = -EINVAL;

	if (qc->id & V4L2_CTRL_FLAG_NEXT_CTRL)
		return ret;

	for (i = 0; i < ctrls_num; i++) {
		if (ci_v4l2_controls[i].id == qc->id) {
			memcpy(qc, &ci_v4l2_controls[i],
			       sizeof(struct v4l2_queryctrl));
			qc->reserved[0] = 0;
			ret = 0;
			break;
		}
	}
	if (ret != 0)
		qc->flags = V4L2_CTRL_FLAG_DISABLED;

	return ret;
}

static int atomisp_camera_g_ext_ctrls(struct file *file, void *fh,
	struct v4l2_ext_controls *c)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct v4l2_control ctrl;
	int i;
	int ret = 0;

	for (i = 0; i < c->count; i++) {
		ctrl.id = c->controls[i].id;
		ctrl.value = c->controls[i].value;
		switch (ctrl.id) {
		case V4L2_CID_EXPOSURE_ABSOLUTE:
		case V4L2_CID_IRIS_ABSOLUTE:
		case V4L2_CID_FNUMBER_ABSOLUTE:
		case V4L2_CID_BIN_FACTOR_HORZ:
		case V4L2_CID_BIN_FACTOR_VERT:
		case V4L2_CID_3A_LOCK:
		case V4L2_CID_TEST_PATTERN:
		case V4L2_CID_TEST_PATTERN_COLOR_R:
		case V4L2_CID_TEST_PATTERN_COLOR_GR:
		case V4L2_CID_TEST_PATTERN_COLOR_GB:
		case V4L2_CID_TEST_PATTERN_COLOR_B:
			/*
			 * Exposure related control will be handled by sensor
			 * driver
			 */
			ret = v4l2_subdev_call(isp->inputs
					       [asd->input_curr].camera,
					       core, g_ctrl, &ctrl);
			break;
		case V4L2_CID_FOCUS_ABSOLUTE:
		case V4L2_CID_FOCUS_RELATIVE:
		case V4L2_CID_FOCUS_STATUS:
		case V4L2_CID_FOCUS_AUTO:
			if (isp->inputs[asd->input_curr].motor)
				ret = v4l2_subdev_call(
					isp->inputs[asd->input_curr].motor,
					core, g_ctrl, &ctrl);
			else
				ret = v4l2_subdev_call(
					isp->inputs[asd->input_curr].camera,
					core, g_ctrl, &ctrl);
			break;
		case V4L2_CID_FLASH_STATUS:
		case V4L2_CID_FLASH_INTENSITY:
		case V4L2_CID_FLASH_TORCH_INTENSITY:
		case V4L2_CID_FLASH_INDICATOR_INTENSITY:
		case V4L2_CID_FLASH_TIMEOUT:
		case V4L2_CID_FLASH_STROBE:
		case V4L2_CID_FLASH_MODE:
		case V4L2_CID_FLASH_STATUS_REGISTER:
			if (isp->flash)
				ret = v4l2_subdev_call(
					isp->flash, core, g_ctrl, &ctrl);
			break;
		case V4L2_CID_ZOOM_ABSOLUTE:
			rt_mutex_lock(&isp->mutex);
			ret = atomisp_digital_zoom(asd, 0, &ctrl.value);
			rt_mutex_unlock(&isp->mutex);
			break;
		case V4L2_CID_G_SKIP_FRAMES:
			ret = v4l2_subdev_call(
				isp->inputs[asd->input_curr].camera,
				sensor, g_skip_frames, (u32 *)&ctrl.value);
			break;
		default:
			ret = -EINVAL;
		}

		if (ret) {
			c->error_idx = i;
			break;
		}
		c->controls[i].value = ctrl.value;
	}
	return ret;
}

/* This ioctl allows the application to get multiple controls by class */
static int atomisp_g_ext_ctrls(struct file *file, void *fh,
	struct v4l2_ext_controls *c)
{
	struct v4l2_control ctrl;
	int i, ret = 0;

	/* input_lock is not need for the Camera releated IOCTLs
	 * The input_lock downgrade the FPS of 3A*/
	ret = atomisp_camera_g_ext_ctrls(file, fh, c);
	if (ret != -EINVAL)
		return ret;

	for (i = 0; i < c->count; i++) {
		ctrl.id = c->controls[i].id;
		ctrl.value = c->controls[i].value;
		ret = atomisp_g_ctrl(file, fh, &ctrl);
		c->controls[i].value = ctrl.value;
		if (ret) {
			c->error_idx = i;
			break;
		}
	}
	return ret;
}

static int atomisp_camera_s_ext_ctrls(struct file *file, void *fh,
	struct v4l2_ext_controls *c)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct v4l2_control ctrl;
	int i;
	int ret = 0;

	for (i = 0; i < c->count; i++) {
		struct v4l2_ctrl *ctr;

		ctrl.id = c->controls[i].id;
		ctrl.value = c->controls[i].value;
		switch (ctrl.id) {
		case V4L2_CID_EXPOSURE_ABSOLUTE:
		case V4L2_CID_EXPOSURE_METERING:
		case V4L2_CID_IRIS_ABSOLUTE:
		case V4L2_CID_FNUMBER_ABSOLUTE:
		case V4L2_CID_VCM_TIMEING:
		case V4L2_CID_VCM_SLEW:
		case V4L2_CID_3A_LOCK:
		case V4L2_CID_TEST_PATTERN:
		case V4L2_CID_TEST_PATTERN_COLOR_R:
		case V4L2_CID_TEST_PATTERN_COLOR_GR:
		case V4L2_CID_TEST_PATTERN_COLOR_GB:
		case V4L2_CID_TEST_PATTERN_COLOR_B:
			ret = v4l2_subdev_call(
				isp->inputs[asd->input_curr].camera,
				core, s_ctrl, &ctrl);
			break;
		case V4L2_CID_FOCUS_ABSOLUTE:
		case V4L2_CID_FOCUS_RELATIVE:
		case V4L2_CID_FOCUS_STATUS:
		case V4L2_CID_FOCUS_AUTO:
			if (isp->inputs[asd->input_curr].motor)
				ret = v4l2_subdev_call(
					isp->inputs[asd->input_curr].motor,
					core, s_ctrl, &ctrl);
			else
				ret = v4l2_subdev_call(
					isp->inputs[asd->input_curr].camera,
					core, s_ctrl, &ctrl);
			break;
		case V4L2_CID_FLASH_STATUS:
		case V4L2_CID_FLASH_INTENSITY:
		case V4L2_CID_FLASH_TORCH_INTENSITY:
		case V4L2_CID_FLASH_INDICATOR_INTENSITY:
		case V4L2_CID_FLASH_TIMEOUT:
		case V4L2_CID_FLASH_STROBE:
		case V4L2_CID_FLASH_MODE:
		case V4L2_CID_FLASH_STATUS_REGISTER:
			rt_mutex_lock(&isp->mutex);
			if (isp->flash) {
				ret = v4l2_subdev_call(isp->flash,
					core, s_ctrl, &ctrl);
				/* When flash mode is changed we need to reset
				 * flash state */
				if (ctrl.id == V4L2_CID_FLASH_MODE) {
					asd->params.flash_state =
						ATOMISP_FLASH_IDLE;
					asd->params.num_flash_frames = 0;
				}
			}
			rt_mutex_unlock(&isp->mutex);
			break;
		case V4L2_CID_ZOOM_ABSOLUTE:
			rt_mutex_lock(&isp->mutex);
			ret = atomisp_digital_zoom(asd, 1, &ctrl.value);
			rt_mutex_unlock(&isp->mutex);
			break;
		default:
			ctr = v4l2_ctrl_find(&asd->ctrl_handler, ctrl.id);
			if (ctr)
				ret = v4l2_ctrl_s_ctrl(ctr, ctrl.value);
			else
				ret = -EINVAL;
		}

		if (ret) {
			c->error_idx = i;
			break;
		}
		c->controls[i].value = ctrl.value;
	}
	return ret;
}

/* This ioctl allows the application to set multiple controls by class */
static int atomisp_s_ext_ctrls(struct file *file, void *fh,
	struct v4l2_ext_controls *c)
{
	struct v4l2_control ctrl;
	int i, ret = 0;

	/* input_lock is not need for the Camera releated IOCTLs
	 * The input_lock downgrade the FPS of 3A*/
	ret = atomisp_camera_s_ext_ctrls(file, fh, c);
	if (ret != -EINVAL)
		return ret;

	for (i = 0; i < c->count; i++) {
		ctrl.id = c->controls[i].id;
		ctrl.value = c->controls[i].value;
		ret = atomisp_s_ctrl(file, fh, &ctrl);
		c->controls[i].value = ctrl.value;
		if (ret) {
			c->error_idx = i;
			break;
		}
	}
	return ret;
}

/*
 * vidioc_g/s_param are used to switch isp running mode
 */
static int atomisp_g_parm(struct file *file, void *fh,
	struct v4l2_streamparm *parm)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;
	struct atomisp_device *isp = video_get_drvdata(vdev);

	if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		dev_err(isp->dev, "unsupport v4l2 buf type\n");
		return -EINVAL;
	}

	rt_mutex_lock(&isp->mutex);
	parm->parm.capture.capturemode = asd->run_mode->val;
	rt_mutex_unlock(&isp->mutex);

	return 0;
}

static int atomisp_s_parm(struct file *file, void *fh,
	struct v4l2_streamparm *parm)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;
	int mode;
	int rval;
	int fps;

	if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE) {
		dev_err(isp->dev, "unsupport v4l2 buf type\n");
		return -EINVAL;
	}

	rt_mutex_lock(&isp->mutex);

	asd->high_speed_mode = false;
	switch (parm->parm.capture.capturemode) {
	case CI_MODE_NONE: {
		struct v4l2_subdev_frame_interval fi = {0};

		fi.interval = parm->parm.capture.timeperframe;

		rval = v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
					video, s_frame_interval, &fi);
		if (!rval)
			parm->parm.capture.timeperframe = fi.interval;

		if (fi.interval.numerator != 0) {
			fps = fi.interval.denominator / fi.interval.numerator;
			if (fps > 30)
				asd->high_speed_mode = true;
		}

		goto out;
	}
	case CI_MODE_VIDEO:
		mode = ATOMISP_RUN_MODE_VIDEO;
		break;
	case CI_MODE_STILL_CAPTURE:
		mode = ATOMISP_RUN_MODE_STILL_CAPTURE;
		break;
	case CI_MODE_CONTINUOUS:
		mode = ATOMISP_RUN_MODE_CONTINUOUS_CAPTURE;
		break;
	case CI_MODE_PREVIEW:
		mode = ATOMISP_RUN_MODE_PREVIEW;
		break;
	default:
		rval = -EINVAL;
		goto out;
	}

	rval = v4l2_ctrl_s_ctrl(asd->run_mode, mode);

out:
	rt_mutex_unlock(&isp->mutex);

	return rval == -ENOIOCTLCMD ? 0 : rval;
}

static int atomisp_s_parm_file(struct file *file, void *fh,
				struct v4l2_streamparm *parm)
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);

	if (parm->type != V4L2_BUF_TYPE_VIDEO_OUTPUT) {
		dev_err(isp->dev, "unsupport v4l2 buf type for output\n");
		return -EINVAL;
	}

	rt_mutex_lock(&isp->mutex);
	isp->sw_contex.file_input = 1;
	rt_mutex_unlock(&isp->mutex);

	return 0;
}

static long atomisp_vidioc_default(struct file *file, void *fh,
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 10, 0))
	bool valid_prio, unsigned int cmd, void *arg)
#else
	bool valid_prio, int cmd, void *arg)
#endif
{
	struct video_device *vdev = video_devdata(file);
	struct atomisp_device *isp = video_get_drvdata(vdev);
	struct atomisp_sub_device *asd = atomisp_to_video_pipe(vdev)->asd;
	int err;

	switch (cmd) {
	case ATOMISP_IOC_G_MOTOR_PRIV_INT_DATA:
	case ATOMISP_IOC_S_EXPOSURE:
	case ATOMISP_IOC_G_SENSOR_CALIBRATION_GROUP:
	case ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA:
	case ATOMISP_IOC_EXT_ISP_CTRL:
	case ATOMISP_IOC_G_SENSOR_AE_BRACKETING_INFO:
	case ATOMISP_IOC_S_SENSOR_AE_BRACKETING_MODE:
	case ATOMISP_IOC_G_SENSOR_AE_BRACKETING_MODE:
	case ATOMISP_IOC_S_SENSOR_AE_BRACKETING_LUT:
		/* we do not need take isp->mutex for these IOCTLs */
		break;
	default:
		rt_mutex_lock(&isp->mutex);
		break;
	}
	switch (cmd) {
	case ATOMISP_IOC_G_XNR:
		err = atomisp_xnr(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_XNR:
		err = atomisp_xnr(asd, 1, arg);
		break;

	case ATOMISP_IOC_G_NR:
		err = atomisp_nr(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_NR:
		err = atomisp_nr(asd, 1, arg);
		break;

	case ATOMISP_IOC_G_TNR:
		err = atomisp_tnr(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_TNR:
		err = atomisp_tnr(asd, 1, arg);
		break;

	case ATOMISP_IOC_G_BLACK_LEVEL_COMP:
		err = atomisp_black_level(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_BLACK_LEVEL_COMP:
		err = atomisp_black_level(asd, 1, arg);
		break;

	case ATOMISP_IOC_G_EE:
		err = atomisp_ee(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_EE:
		err = atomisp_ee(asd, 1, arg);
		break;

	case ATOMISP_IOC_G_DIS_STAT:
		err = atomisp_get_dis_stat(asd, arg);
		break;

	case ATOMISP_IOC_G_DVS2_BQ_RESOLUTIONS:
		err = atomisp_get_dvs2_bq_resolutions(asd, arg);
		break;
	case ATOMISP_IOC_S_DIS_COEFS:
		err = atomisp_set_dis_coefs(asd, arg);
		if (!err && arg) {
			atomisp_css_update_isp_params(asd);
			asd->params.css_update_params_needed = false;
		}
		break;

	case ATOMISP_IOC_S_DIS_VECTOR:
		err = atomisp_cp_dvs_6axis_config(asd, arg, &asd->params.css_param);
		if (!err && arg) {
			atomisp_css_set_dvs_6axis(asd, asd->params.css_param.dvs_6axis);
			atomisp_css_update_isp_params(asd);
			asd->params.css_update_params_needed = false;
		}
		break;

	case ATOMISP_IOC_G_ISP_PARM:
		err = atomisp_param(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_ISP_PARM:
		err = atomisp_param(asd, 1, arg);
		break;

	case ATOMISP_IOC_G_3A_STAT:
		err = atomisp_3a_stat(asd, 0, arg);
		break;

	case ATOMISP_IOC_G_ISP_GAMMA:
		err = atomisp_gamma(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_ISP_GAMMA:
		err = atomisp_gamma(asd, 1, arg);
		break;

	case ATOMISP_IOC_G_ISP_GDC_TAB:
		err = atomisp_gdc_cac_table(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_ISP_GDC_TAB:
		err = atomisp_gdc_cac_table(asd, 1, arg);
		break;

	case ATOMISP_IOC_G_ISP_MACC:
		err = atomisp_macc_table(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_ISP_MACC:
		err = atomisp_macc_table(asd, 1, arg);
		break;

	case ATOMISP_IOC_G_ISP_BAD_PIXEL_DETECTION:
		err = atomisp_bad_pixel_param(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_ISP_BAD_PIXEL_DETECTION:
		err = atomisp_bad_pixel_param(asd, 1, arg);
		break;

	case ATOMISP_IOC_G_ISP_FALSE_COLOR_CORRECTION:
		err = atomisp_false_color_param(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_ISP_FALSE_COLOR_CORRECTION:
		err = atomisp_false_color_param(asd, 1, arg);
		break;

	case ATOMISP_IOC_G_ISP_CTC:
		err = atomisp_ctc(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_ISP_CTC:
		err = atomisp_ctc(asd, 1, arg);
		break;

	case ATOMISP_IOC_G_ISP_WHITE_BALANCE:
		err = atomisp_white_balance_param(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_ISP_WHITE_BALANCE:
		err = atomisp_white_balance_param(asd, 1, arg);
		break;

	case ATOMISP_IOC_G_3A_CONFIG:
		err = atomisp_3a_config_param(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_3A_CONFIG:
		err = atomisp_3a_config_param(asd, 1, arg);
		break;

	case ATOMISP_IOC_S_ISP_FPN_TABLE:
		err = atomisp_fixed_pattern_table(asd, arg);
		break;

	case ATOMISP_IOC_ISP_MAKERNOTE:
		err = atomisp_exif_makernote(asd, arg);
		break;

	case ATOMISP_IOC_G_SENSOR_MODE_DATA:
		err = atomisp_get_sensor_mode_data(asd, arg);
		break;

	case ATOMISP_IOC_G_MOTOR_PRIV_INT_DATA:
		if (isp->inputs[asd->input_curr].motor)
			err = v4l2_subdev_call(
					isp->inputs[asd->input_curr].motor,
					core, ioctl, cmd, arg);
		else
			err = v4l2_subdev_call(
					isp->inputs[asd->input_curr].camera,
					core, ioctl, cmd, arg);
		break;

	case ATOMISP_IOC_S_EXPOSURE:
	case ATOMISP_IOC_G_SENSOR_CALIBRATION_GROUP:
	case ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA:
	case ATOMISP_IOC_G_SENSOR_AE_BRACKETING_INFO:
	case ATOMISP_IOC_S_SENSOR_AE_BRACKETING_MODE:
	case ATOMISP_IOC_G_SENSOR_AE_BRACKETING_MODE:
	case ATOMISP_IOC_S_SENSOR_AE_BRACKETING_LUT:
		err = v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
					core, ioctl, cmd, arg);
		break;

	case ATOMISP_IOC_ACC_LOAD:
		err = atomisp_acc_load(isp, arg);
		break;

	case ATOMISP_IOC_ACC_LOAD_TO_PIPE:
		err = atomisp_acc_load_to_pipe(isp, arg);
		break;

	case ATOMISP_IOC_ACC_UNLOAD:
		err = atomisp_acc_unload(isp, arg);
		break;

	case ATOMISP_IOC_ACC_START:
		err = atomisp_acc_start(asd, arg);
		break;

	case ATOMISP_IOC_ACC_WAIT:
		err = atomisp_acc_wait(asd, arg);
		break;

	case ATOMISP_IOC_ACC_MAP:
		err = atomisp_acc_map(isp, arg);
		break;

	case ATOMISP_IOC_ACC_UNMAP:
		err = atomisp_acc_unmap(isp, arg);
		break;

	case ATOMISP_IOC_ACC_S_MAPPED_ARG:
		err = atomisp_acc_s_mapped_arg(isp, arg);
		break;

	case ATOMISP_IOC_S_ISP_SHD_TAB:
		err = atomisp_set_shading_table(asd, arg);
		break;

	case ATOMISP_IOC_G_ISP_GAMMA_CORRECTION:
		err = atomisp_gamma_correction(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_ISP_GAMMA_CORRECTION:
		err = atomisp_gamma_correction(asd, 1, arg);
		break;

	case ATOMISP_IOC_S_PARAMETERS:
		err = atomisp_set_parameters(vdev, arg);
		break;

	case ATOMISP_IOC_S_CONT_CAPTURE_CONFIG:
		err = atomisp_offline_capture_configure(asd, arg);
		break;
	case ATOMISP_IOC_G_METADATA:
		err = atomisp_get_metadata(asd, 0, arg);
		break;
	case ATOMISP_IOC_G_METADATA_BY_TYPE:
		err = atomisp_get_metadata_by_type(asd, 0, arg);
		break;
	case ATOMISP_IOC_EXT_ISP_CTRL:
		err = v4l2_subdev_call(isp->inputs[asd->input_curr].camera,
					core, ioctl, cmd, arg);
		break;
	case ATOMISP_IOC_EXP_ID_UNLOCK:
		err = atomisp_exp_id_unlock(asd, arg);
		break;
	case ATOMISP_IOC_EXP_ID_CAPTURE:
		err = atomisp_exp_id_capture(asd, arg);
		break;
	case ATOMISP_IOC_S_ENABLE_DZ_CAPT_PIPE:
		err = atomisp_enable_dz_capt_pipe(asd, arg);
		break;
	case ATOMISP_IOC_G_FORMATS_CONFIG:
		err = atomisp_formats(asd, 0, arg);
		break;

	case ATOMISP_IOC_S_FORMATS_CONFIG:
		err = atomisp_formats(asd, 1, arg);
		break;
	case ATOMISP_IOC_S_EXPOSURE_WINDOW:
		err = atomisp_s_ae_window(asd, arg);
		break;
	case ATOMISP_IOC_S_ACC_STATE:
		err = atomisp_acc_set_state(asd, arg);
		break;
	case ATOMISP_IOC_G_ACC_STATE:
		err = atomisp_acc_get_state(asd, arg);
		break;
	case ATOMISP_IOC_INJECT_A_FAKE_EVENT:
		err = atomisp_inject_a_fake_event(asd, arg);
		break;
	case ATOMISP_IOC_G_INVALID_FRAME_NUM:
		err = atomisp_get_invalid_frame_num(vdev, arg);
		break;
	case ATOMISP_IOC_G_EFFECTIVE_RESOLUTION:
		err = atomisp_get_effective_res(asd, arg);
		break;
	default:
		err = -EINVAL;
		break;
	}

	switch (cmd) {
	case ATOMISP_IOC_G_MOTOR_PRIV_INT_DATA:
	case ATOMISP_IOC_S_EXPOSURE:
	case ATOMISP_IOC_G_SENSOR_CALIBRATION_GROUP:
	case ATOMISP_IOC_G_SENSOR_PRIV_INT_DATA:
	case ATOMISP_IOC_EXT_ISP_CTRL:
	case ATOMISP_IOC_G_SENSOR_AE_BRACKETING_INFO:
	case ATOMISP_IOC_S_SENSOR_AE_BRACKETING_MODE:
	case ATOMISP_IOC_G_SENSOR_AE_BRACKETING_MODE:
	case ATOMISP_IOC_S_SENSOR_AE_BRACKETING_LUT:
		break;
	default:
		rt_mutex_unlock(&isp->mutex);
		break;
	}
	return err;
}

const struct v4l2_ioctl_ops atomisp_ioctl_ops = {
	.vidioc_querycap = atomisp_querycap,
#ifndef CONFIG_GMIN_INTEL_MID
	.vidioc_g_chip_ident = atomisp_g_chip_ident,
#endif
	.vidioc_enum_input = atomisp_enum_input,
	.vidioc_g_input = atomisp_g_input,
	.vidioc_s_input = atomisp_s_input,
	.vidioc_queryctrl = atomisp_queryctl,
	.vidioc_s_ctrl = atomisp_s_ctrl,
	.vidioc_g_ctrl = atomisp_g_ctrl,
	.vidioc_s_ext_ctrls = atomisp_s_ext_ctrls,
	.vidioc_g_ext_ctrls = atomisp_g_ext_ctrls,
	.vidioc_enum_fmt_vid_cap = atomisp_enum_fmt_cap,
	.vidioc_try_fmt_vid_cap = atomisp_try_fmt_cap,
	.vidioc_g_fmt_vid_cap = atomisp_g_fmt_cap,
	.vidioc_s_fmt_vid_cap = atomisp_s_fmt_cap,
	.vidioc_reqbufs = atomisp_reqbufs,
	.vidioc_querybuf = atomisp_querybuf,
	.vidioc_qbuf = atomisp_qbuf,
	.vidioc_dqbuf = atomisp_dqbuf,
	.vidioc_streamon = atomisp_streamon,
	.vidioc_streamoff = atomisp_streamoff,
	.vidioc_default = atomisp_vidioc_default,
	.vidioc_enum_frameintervals = atomisp_enum_frameintervals,
	.vidioc_s_parm = atomisp_s_parm,
	.vidioc_g_parm = atomisp_g_parm,
};

const struct v4l2_ioctl_ops atomisp_file_ioctl_ops = {
	.vidioc_querycap = atomisp_querycap,
	.vidioc_g_fmt_vid_out = atomisp_g_fmt_file,
	.vidioc_s_fmt_vid_out = atomisp_s_fmt_file,
	.vidioc_s_parm = atomisp_s_parm_file,
	.vidioc_reqbufs = atomisp_reqbufs_file,
	.vidioc_querybuf = atomisp_querybuf_file,
	.vidioc_qbuf = atomisp_qbuf_file,
};
