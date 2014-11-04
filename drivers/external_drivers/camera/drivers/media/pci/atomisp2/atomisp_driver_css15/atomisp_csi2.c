/*
 * Support for Medifield PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2010 Intel Corporation. All Rights Reserved.
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

#include <media/v4l2-event.h>
#include <media/v4l2-mediabus.h>
#include "atomisp_internal.h"

static struct v4l2_mbus_framefmt *__csi2_get_format(
	struct atomisp_mipi_csi2_device *csi2, struct v4l2_subdev_fh *fh,
	enum v4l2_subdev_format_whence which, unsigned int pad)
{
	if (which == V4L2_SUBDEV_FORMAT_TRY)
		return v4l2_subdev_get_try_format(fh, pad);
	else
		return &csi2->formats[pad];
}

/*
 * csi2_enum_mbus_code - Handle pixel format enumeration
 * @sd     : pointer to v4l2 subdev structure
 * @fh     : V4L2 subdev file handle
 * @code   : pointer to v4l2_subdev_pad_mbus_code_enum structure
 * return -EINVAL or zero on success
*/
static int csi2_enum_mbus_code(struct v4l2_subdev *sd,
			struct v4l2_subdev_fh *fh,
			struct v4l2_subdev_mbus_code_enum *code)
{
	const struct atomisp_in_fmt_conv *ic = atomisp_in_fmt_conv;
	unsigned int i = 0;

	while (ic->code) {
		if (i == code->index) {
			code->code = ic->code;
			return 0;
		}
		i++, ic++;
	}

	return -EINVAL;
}

/*
 * csi2_get_format - Handle get format by pads subdev method
 * @sd : pointer to v4l2 subdev structure
 * @fh : V4L2 subdev file handle
 * @pad: pad num
 * @fmt: pointer to v4l2 format structure
 * return -EINVAL or zero on sucess
*/
static int csi2_get_format(struct v4l2_subdev *sd,
	struct v4l2_subdev_fh *fh, struct v4l2_subdev_format *fmt)
{
	struct atomisp_mipi_csi2_device *csi2 = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *format;

	format = __csi2_get_format(csi2, fh, fmt->which, fmt->pad);

	fmt->format = *format;

	return 0;
}

int atomisp_csi2_set_ffmt(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
			  unsigned int which, uint16_t pad,
			  struct v4l2_mbus_framefmt *ffmt)
{
	struct atomisp_mipi_csi2_device *csi2 = v4l2_get_subdevdata(sd);
	struct v4l2_mbus_framefmt *actual_ffmt =
		__csi2_get_format(csi2, fh, which, pad);

	if (pad == CSI2_PAD_SINK) {
		const struct atomisp_in_fmt_conv *ic;
		struct v4l2_mbus_framefmt tmp_ffmt;

		ic = atomisp_find_in_fmt_conv(ffmt->code);
		if (ic)
			actual_ffmt->code = ic->code;
		else
			actual_ffmt->code = atomisp_in_fmt_conv[0].code;

		actual_ffmt->width = clamp_t(
			u32, ffmt->width, ATOM_ISP_MIN_WIDTH,
			ATOM_ISP_MAX_WIDTH);
		actual_ffmt->height = clamp_t(
			u32, ffmt->height, ATOM_ISP_MIN_HEIGHT,
			ATOM_ISP_MAX_HEIGHT);

		tmp_ffmt = *ffmt = *actual_ffmt;

		return atomisp_csi2_set_ffmt(sd, fh, which, CSI2_PAD_SOURCE,
					     &tmp_ffmt);
	}

	/* FIXME: DPCM decompression */
	*actual_ffmt = *ffmt =
		*__csi2_get_format(csi2, fh, which, CSI2_PAD_SINK);

	return 0;
}

/*
 * csi2_set_format - Handle set format by pads subdev method
 * @sd : pointer to v4l2 subdev structure
 * @fh : V4L2 subdev file handle
 * @pad: pad num
 * @fmt: pointer to v4l2 format structure
 * return -EINVAL or zero on success
*/
static int csi2_set_format(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh,
		    struct v4l2_subdev_format *fmt)
{
	return atomisp_csi2_set_ffmt(sd, fh, fmt->which, fmt->pad,
				     &fmt->format);
}

/*
 * csi2_set_stream - Enable/Disable streaming on the CSI2 module
 * @sd: ISP CSI2 V4L2 subdevice
 * @enable: Enable/disable stream (1/0)
 *
 * Return 0 on success or a negative error code otherwise.
*/
static int csi2_set_stream(struct v4l2_subdev *sd, int enable)
{
	 return 0;
}

/* subdev core operations */
static const struct v4l2_subdev_core_ops csi2_core_ops = {
	.queryctrl = v4l2_subdev_queryctrl,
	.querymenu = v4l2_subdev_querymenu,
	.g_ctrl = v4l2_subdev_g_ctrl,
	.s_ctrl = v4l2_subdev_s_ctrl,
	.g_ext_ctrls = v4l2_subdev_g_ext_ctrls,
	.try_ext_ctrls = v4l2_subdev_try_ext_ctrls,
	.s_ext_ctrls = v4l2_subdev_s_ext_ctrls,
};

/* subdev video operations */
static const struct v4l2_subdev_video_ops csi2_video_ops = {
	.s_stream = csi2_set_stream,
};

/* subdev pad operations */
static const struct v4l2_subdev_pad_ops csi2_pad_ops = {
	.enum_mbus_code = csi2_enum_mbus_code,
	.get_fmt = csi2_get_format,
	.set_fmt = csi2_set_format,
	.link_validate = v4l2_subdev_link_validate_default,
};

/* subdev operations */
static const struct v4l2_subdev_ops csi2_ops = {
	.core = &csi2_core_ops,
	.video = &csi2_video_ops,
	.pad = &csi2_pad_ops,
};


/*
 * csi2_link_setup - Setup CSI2 connections.
 * @entity : Pointer to media entity structure
 * @local  : Pointer to local pad array
 * @remote : Pointer to remote pad array
 * @flags  : Link flags
 * return -EINVAL or zero on success
*/
static int csi2_link_setup(struct media_entity *entity,
	    const struct media_pad *local,
	    const struct media_pad *remote, u32 flags)
{
	struct v4l2_subdev *sd = media_entity_to_v4l2_subdev(entity);
	struct atomisp_mipi_csi2_device *csi2 = v4l2_get_subdevdata(sd);
	u32 result = local->index | media_entity_type(remote->entity);

	switch (result) {
	case CSI2_PAD_SOURCE | MEDIA_ENT_T_DEVNODE:
		/* not supported yet */
		return -EINVAL;

	case CSI2_PAD_SOURCE | MEDIA_ENT_T_V4L2_SUBDEV:
		if (flags & MEDIA_LNK_FL_ENABLED) {
			if (csi2->output & ~CSI2_OUTPUT_ISP_SUBDEV)
				return -EBUSY;
			csi2->output |= CSI2_OUTPUT_ISP_SUBDEV;
		} else {
			csi2->output &= ~CSI2_OUTPUT_ISP_SUBDEV;
		}
		break;

	default:
		/* Link from camera to CSI2 is fixed... */
		return -EINVAL;
	}
	return 0;
}

/* media operations */
static const struct media_entity_operations csi2_media_ops = {
	.link_setup = csi2_link_setup,
	.link_validate = v4l2_subdev_link_validate,
};

/*
* ispcsi2_init_entities - Initialize subdev and media entity.
* @csi2: Pointer to ispcsi2 structure.
* return -ENOMEM or zero on success
*/
static int mipi_csi2_init_entities(struct atomisp_mipi_csi2_device *csi2,
					int port)
{
	struct v4l2_subdev *sd = &csi2->subdev;
	struct media_pad *pads = csi2->pads;
	struct media_entity *me = &sd->entity;
	int ret;

	v4l2_subdev_init(sd, &csi2_ops);
	snprintf(sd->name, sizeof(sd->name), "ATOM ISP CSI2-port%d", port);

	v4l2_set_subdevdata(sd, csi2);
	sd->flags |= V4L2_SUBDEV_FL_HAS_DEVNODE;

	pads[CSI2_PAD_SOURCE].flags = MEDIA_PAD_FL_SOURCE;
	pads[CSI2_PAD_SINK].flags = MEDIA_PAD_FL_SINK;

	me->ops = &csi2_media_ops;
	me->type = MEDIA_ENT_T_V4L2_SUBDEV;
	ret = media_entity_init(me, CSI2_PADS_NUM, pads, 0);
	if (ret < 0)
		return ret;

	csi2->formats[CSI2_PAD_SINK].code =
		csi2->formats[CSI2_PAD_SOURCE].code =
		atomisp_in_fmt_conv[0].code;

	return 0;
}

void
atomisp_mipi_csi2_unregister_entities(struct atomisp_mipi_csi2_device *csi2)
{
	media_entity_cleanup(&csi2->subdev.entity);
	v4l2_device_unregister_subdev(&csi2->subdev);
}

int atomisp_mipi_csi2_register_entities(struct atomisp_mipi_csi2_device *csi2,
			struct v4l2_device *vdev)
{
	int ret;

	/* Register the subdev and video nodes. */
	ret = v4l2_device_register_subdev(vdev, &csi2->subdev);
	if (ret < 0)
		goto error;

	return 0;

error:
	atomisp_mipi_csi2_unregister_entities(csi2);
	return ret;
}

/*
 * atomisp_mipi_csi2_cleanup - Routine for module driver cleanup
*/
void atomisp_mipi_csi2_cleanup(struct atomisp_device *isp)
{
}


int atomisp_mipi_csi2_init(struct atomisp_device *isp)
{
	struct atomisp_mipi_csi2_device *csi2_port;
	unsigned int i;
	int ret;

	for (i = 0; i < ATOMISP_CAMERA_NR_PORTS; i++) {
		csi2_port = &isp->csi2_port[i];
		csi2_port->isp = isp;
		ret = mipi_csi2_init_entities(csi2_port, i);
		if (ret < 0)
			goto fail;
	}

	return 0;

fail:
	atomisp_mipi_csi2_cleanup(isp);
	return ret;
}

