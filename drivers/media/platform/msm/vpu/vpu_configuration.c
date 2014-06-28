/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt)	"VPU, %s: " fmt, __func__

#include <linux/videodev2.h>
#include <media/v4l2-dev.h>
#include <media/v4l2-ctrls.h>
#include <linux/stddef.h>
#include <linux/sizes.h>
#include <linux/time.h>
#include <asm/div64.h>

#include "vpu_configuration.h"
#include "vpu_v4l2.h"
#include "vpu_ioctl_internal.h"
#include "vpu_translate.h"
#include "vpu_property.h"
#include "vpu_channel.h"


/*
 * Private controls support
 */
enum vpu_ctrl_type {
	CTRL_TYPE_NONE = 0,
	CTRL_TYPE_VALUE,
	CTRL_TYPE_STANDARD,
	CTRL_TYPE_AUTO_MANUAL,
	CTRL_TYPE_RESERVED,
};

struct vpu_ctrl_desc; /* forward declaration */

#define call_op(d, op, args...) (((d)->op) ? ((d)->op(args)) : 0)
typedef int (*get_custom_size)(const void *api_arg);
typedef int (*bounds_check)(const struct vpu_ctrl_desc *desc,
		const void *api_arg);
typedef void (*translate_to_hfi) (const void *api_data, void *hfi_data);
typedef void (*translate_to_api) (const void *hfi_data, void *api_data);
typedef int (*custom_set_function)(struct vpu_dev_session *session,
		const void *api_arg);
typedef int (*custom_get_function)(struct vpu_dev_session *session,
		void *ret_arg);

struct ctrl_bounds {
	s32 min;
	s32 max;
	s32 def;
	u32 step;
};

/**
 * struct vpu_ctrl_desc - describes the operation of of VPU controls
 * @name: [Optional] Control name used for logging purposes.
 * @type: Can be set if control is a generic type to avoid setting other fields.
 *		If set, then size, hfi_size, bounds_check, translate_to_hfi,
 *		and translate_to_api are automatically set accordingly.
 * @hfi_id: [Required] Identifies corresponding HFI property ID.
 *		If value is zero, then control is not sent to HFI.
 * @api_size: [Required] Size of API struct used with this control.
 * @hfi_size: [Required] Size of data sent to HFI. Auto set if type is valid.
 * @cached: Set to 1 if control cannot be read from HFI and needs to be
 *		returned from cache instead.
 * @cache_offset: [Don't set]. Calculated offset where control cache is stored.
 * @bounds: [Optional] Set if control has bounds.
 * @bounds2: [Optional] A second set of bounds if needed.
 * @custom_size: Custom function to calculate hfi_size if it is dynamic.
 * @bounds_check: [Optional] Function that performs the bound checking.
 *		Auto set if type is valid.
 * @translate_to_hfi: [Required for set control]. Translates from API to HFI.
 *		Auto set if type is valid.
 * @translate_to_api: [Required for get control]. Translates from HFI to API.
 *		Auto set if type is valid.
 * @set_function: [Optional] Custom control set function.
 * @get_function: [Optional] Custom control get function.
 */
struct vpu_ctrl_desc {
	const char *name;
	enum vpu_ctrl_type type;
	u32 hfi_id;
	u32 api_size;
	u32 hfi_size;
	u32 cached;
	u32 cache_offset;
	struct ctrl_bounds bounds;
	struct ctrl_bounds bounds2;
	/* function pointers */
	get_custom_size custom_size;
	bounds_check bounds_check;
	translate_to_hfi translate_to_hfi;
	translate_to_api translate_to_api;
	custom_set_function set_function;
	custom_get_function get_function;
};

static int __check_bounds(const struct ctrl_bounds *bounds, s32 val)
{
	if (!bounds->min && !bounds->max && !bounds->step)
		return 0; /* no bounds specified */

	if (val < bounds->min || val > bounds->max || !bounds->step)
		return -EINVAL;

	val = val - bounds->min;
	if (((val / bounds->step) * bounds->step) != val)
		return -EINVAL;

	return 0;
}

static int value_bounds_check(const struct vpu_ctrl_desc *desc,
		const void *api_arg)
{
	const s32 *value = api_arg;

	return __check_bounds(&desc->bounds, *value);
}

static int standard_bounds_check(const struct vpu_ctrl_desc *desc,
		const void *api_arg)
{
	const struct vpu_ctrl_standard *arg = api_arg;

	if (arg->enable)
		return __check_bounds(&desc->bounds, arg->value);
	else
		return 0; /* ctrl not enabled, no need to check value */
}

static int auto_manual_bounds_check(const struct vpu_ctrl_desc *desc,
		const void *api_arg)
{
	const struct vpu_ctrl_auto_manual *arg = api_arg;
	const struct ctrl_bounds *bounds;

	if (arg->enable && arg->auto_mode)
		bounds = &desc->bounds;
	else if (arg->enable && !arg->auto_mode)
		bounds = &desc->bounds2;
	else
		return 0; /* ctrl not enabled, no need to check value */

	return __check_bounds(bounds, arg->value);
}

int configure_colorspace(struct vpu_dev_session *session, int port)
{
	struct vpu_prop_session_color_space cs_param_1;
	struct vpu_prop_session_color_space cs_param_2;
	int ret;
	int colorspace;

	colorspace = session->port_info[port].format.colorspace;
	if (!colorspace)
		return 0;

	translate_colorspace_to_hfi(colorspace, &cs_param_1, &cs_param_2);
	ret = vpu_hw_session_s_property(session->id,
			VPU_PROP_SESSION_COLOR_SPACE,
			&cs_param_1, sizeof(cs_param_1));
	if (ret) {
		pr_err("Failed to set port colorspace\n");
		return ret;
	}

	if (cs_param_1.cs_config != CONFIG_RGB_RANGE) {
		ret = vpu_hw_session_s_property(session->id,
				VPU_PROP_SESSION_COLOR_SPACE,
				&cs_param_2, sizeof(cs_param_2));
		if (ret) {
			pr_err("Failed to set port colorspace\n");
			return ret;
		}
	}
	return ret;
}

int configure_nr_buffers(struct vpu_dev_session *session,
		const struct vpu_ctrl_auto_manual *nr)
{
	struct vpu_controller *controller = session->controller;
	struct v4l2_pix_format_mplane *in_fmt;
	const struct vpu_format_desc *vpu_format;
	u32 buf_size = 0;
	int ret = 0, i, new_bufs = 0;

	if (nr->enable) {
		/* Get NR buffer size. NR buffers are YUV422 10bit format */
		vpu_format = query_supported_formats(PIXEL_FORMAT_YUYV10_LOOSE);
		in_fmt = &session->port_info[INPUT_PORT].format;

		if (!in_fmt->width || !in_fmt->height)
			return 0; /* input resolution not configured yet */

		buf_size = get_bytesperline(in_fmt->width,
				vpu_format->plane[0].bitsperpixel, 0,
				in_fmt->pixelformat);
		buf_size *= in_fmt->height;

		for (i = 0; i < NUM_NR_BUFFERS; i++) {
			if (vpu_mem_size(controller->nr_buffers[i]) >= buf_size)
				continue;

			ret = vpu_mem_alloc(controller->nr_buffers[i], buf_size,
				session->port_info[INPUT_PORT].secure_content);
			if (ret) {
				pr_err("Failed allocate NR buffers size (%d)\n",
						buf_size);
				goto exit_nr_bufs;
			}
			new_bufs = 1;
		}
		if (!new_bufs)
			return 0; /* no new buffers */

		/* send buffers to fw */
		ret = vpu_hw_session_nr_buffer_config(session->id,
			vpu_mem_addr(controller->nr_buffers[0], MEM_VPU_ID),
			vpu_mem_addr(controller->nr_buffers[1], MEM_VPU_ID));
		if (ret) {
			pr_err("Fail to send NR buffers to fw\n");
			goto exit_nr_bufs;
		}
	}

exit_nr_bufs:
	return ret;
}

static int range_mapping_bounds_check(const struct vpu_ctrl_desc *desc,
		const void *api_arg)
{
	int ret;
	const struct vpu_ctrl_range_mapping *arg = api_arg;

	ret = __check_bounds(&desc->bounds, arg->y_range);
	if (ret) {
		pr_err("range mapping set for y out of range %d\n",
				arg->y_range);
		return ret;
	}

	ret = __check_bounds(&desc->bounds2, arg->uv_range);
	if (ret) {
		pr_err("range mapping set for uv out of range %d\n",
				arg->uv_range);
	}
	return ret;
}

static int __active_region_param_get_size(const void *api_arg)
{
	const struct vpu_ctrl_active_region_param *arg = api_arg;

	int size = sizeof(struct vpu_prop_session_active_region_detect);
	/* add the size of exclusion regions */
	if (arg->num_exclusions <= VPU_ACTIVE_REGION_N_EXCLUSIONS)
		size += arg->num_exclusions * sizeof(struct rect);

	return size;
}

static int __active_region_param_bounds_check(const struct vpu_ctrl_desc *desc,
		const void *api_arg)
{
	const struct vpu_ctrl_active_region_param *arg = api_arg;

	return (arg->num_exclusions > VPU_ACTIVE_REGION_N_EXCLUSIONS) ?
			-EINVAL : 0;
}

static int deinterlacing_mode_bounds_check(const struct vpu_ctrl_desc *desc,
		const void *api_arg)
{
	int ret;
	const struct vpu_ctrl_deinterlacing_mode *arg = api_arg;

	ret = __check_bounds(&desc->bounds, arg->field_polarity);
	if (ret) {
		pr_err("field polarity out of range %d\n",
				arg->field_polarity);
		return ret;
	}

	ret = __check_bounds(&desc->bounds2, arg->mvp_mode);
	if (ret) {
		pr_err("mvp mode is out of range %d\n",
				arg->mvp_mode);
	}
	return ret;
}

static int hqv_bounds_check(const struct vpu_ctrl_desc *desc,
		const void *api_arg)
{
	int ret;
	const struct vpu_ctrl_hqv *arg = api_arg;

	ret = __check_bounds(&desc->bounds, arg->sharpen_strength);
	if (ret) {
		pr_err("sharpen strength is out of range %d\n",
				arg->sharpen_strength);
		return ret;
	}

	ret = __check_bounds(&desc->bounds2, arg->auto_nr_strength);
	if (ret) {
		pr_err("auto nr strength is out of range %d\n",
				arg->auto_nr_strength);
	}
	return ret;
}

static int vpu_set_content_protection(struct vpu_dev_session *session,
		const u32 *cp_arg)
{
	int i;
	u32 cp = 0;

	if (!cp_arg)
		return -EINVAL;
	if (*cp_arg)
		cp = 1;

	for (i = 0; i < NUM_VPU_PORTS; i++) {
		/* only change CP status if all ports are in reset */
		if (session->io_client[i] != NULL)
			return -EBUSY;
	}
	for (i = 0; i < NUM_VPU_PORTS; i++)
		session->port_info[i].secure_content = cp;

	return 0;
}

static int vpu_get_content_protection(struct vpu_dev_session *session, u32 *cp)
{
	if (!cp)
		return -EINVAL;
	*cp = session->port_info[INPUT_PORT].secure_content;

	return 0;
}

/* controls description array */
static struct vpu_ctrl_desc ctrl_descriptors[VPU_CTRL_ID_MAX] = {
	[VPU_CTRL_NOISE_REDUCTION] = {
		.name = "Noise Reduction",
		.type = CTRL_TYPE_AUTO_MANUAL,
		.hfi_id = VPU_PROP_SESSION_NOISE_REDUCTION,
		.bounds = {
			.min = VPU_AUTO_NR_LEVEL_MIN,
			.max = VPU_AUTO_NR_LEVEL_MAX,
			.step = VPU_AUTO_NR_LEVEL_STEP,
		},
		.bounds2 = {
			.min = VPU_NOISE_REDUCTION_LEVEL_MIN,
			.max = VPU_NOISE_REDUCTION_LEVEL_MAX,
			.step = VPU_NOISE_REDUCTION_STEP,
		},
		.set_function = (custom_set_function) configure_nr_buffers,
	},
	[VPU_CTRL_IMAGE_ENHANCEMENT] = {
		.name = "Image Enhancement",
		.type = CTRL_TYPE_AUTO_MANUAL,
		.hfi_id = VPU_PROP_SESSION_IMAGE_ENHANCEMENT,
		.bounds = {
			.min = VPU_AUTO_IE_LEVEL_MIN,
			.max = VPU_AUTO_IE_LEVEL_MAX,
				.step = VPU_AUTO_IE_LEVEL_STEP,
		},
		.bounds2 = {
			.min = VPU_IMAGE_ENHANCEMENT_LEVEL_MIN,
			.max = VPU_IMAGE_ENHANCEMENT_LEVEL_MAX,
			.step = VPU_IMAGE_ENHANCEMENT_STEP,
		},
	},
	[VPU_CTRL_ANAMORPHIC_SCALING] = {
		.name = "Anamorphic Scaling",
		.type = CTRL_TYPE_STANDARD,
		.hfi_id = VPU_PROP_SESSION_ANAMORPHIC_SCALING,
		.bounds = {
			.min = VPU_ANAMORPHIC_SCALE_VALUE_MIN,
			.max = VPU_ANAMORPHIC_SCALE_VALUE_MAX,
			.step = VPU_ANAMORPHIC_SCALE_VALUE_STEP,
			.def = VPU_ANAMORPHIC_SCALE_VALUE_DEF,
		},
	},
	[VPU_CTRL_DIRECTIONAL_INTERPOLATION] = {
		.name = "Directional Interpolation",
		.type = CTRL_TYPE_STANDARD,
		.hfi_id = VPU_PROP_SESSION_DI,
		.bounds = {
			.min = VPU_DI_VALUE_MIN,
			.max = VPU_DI_VALUE_MAX,
			.step = 1,
		},
	},
	[VPU_CTRL_BACKGROUND_COLOR] = {
		.name = "Background Color",
		.type = CTRL_TYPE_VALUE,
		.hfi_id = VPU_PROP_SESSION_BACKGROUND_COLOR,
	},
	[VPU_CTRL_RANGE_MAPPING] = {
		.name = "Y/UV Range Mapping",
		.hfi_id = VPU_PROP_SESSION_RANGE_MAPPING,
		.api_size = sizeof(struct vpu_ctrl_range_mapping),
		.hfi_size = sizeof(struct vpu_prop_session_range_mapping),
		.bounds = {
			.min = VPU_RANGE_MAPPING_MIN,
			.max = VPU_RANGE_MAPPING_MAX,
			.step = VPU_RANGE_MAPPING_STEP,
		},
		.bounds2 = {
			.min = VPU_RANGE_MAPPING_MIN,
			.max = VPU_RANGE_MAPPING_MAX,
			.step = VPU_RANGE_MAPPING_STEP,
		},
		.bounds_check = range_mapping_bounds_check,
		.translate_to_hfi = translate_range_mapping_to_hfi,
	},
	[VPU_CTRL_DEINTERLACING_MODE] = {
		.name = "Deinterlacing Mode",
		.hfi_id = VPU_PROP_SESSION_DEINTERLACING,
		.api_size = sizeof(struct vpu_ctrl_deinterlacing_mode),
		.hfi_size = sizeof(struct vpu_prop_session_deinterlacing),
		.bounds = {
			.min = FIELD_POLARITY_AUTO,
			.max = FIELD_POLARITY_ODD,
			.step = 1,
		},
		.bounds2 = {
			.min = MVP_MODE_AUTO,
			.max = MVP_MODE_VIDEO,
			.step = 1,
		},
		.bounds_check = deinterlacing_mode_bounds_check,
	},
	[VPU_CTRL_ACTIVE_REGION_PARAM] = {
		.name = "Active Region Detection Parameters",
		.hfi_id = VPU_PROP_SESSION_ACTIVE_REGION_DETECT,
		.api_size = sizeof(struct vpu_ctrl_active_region_param),
		.custom_size = __active_region_param_get_size,
		.cached = 1,
		.bounds_check = __active_region_param_bounds_check,
		.translate_to_hfi = translate_active_region_param_to_hfi,
	},
	[VPU_CTRL_ACTIVE_REGION_RESULT] = {
		.name = "Active Region Detection Result",
		.api_size = sizeof(struct v4l2_rect),
		.hfi_size = sizeof(struct rect),
		.cached = 1,
		.translate_to_api = translate_active_region_result_to_api,
	},
	[VPU_CTRL_PRIORITY] = {
		.name = "Session Priority",
		.type = CTRL_TYPE_VALUE,
		.hfi_id = VPU_PROP_SESSION_PRIORITY,
	},
	[VPU_CTRL_CONTENT_PROTECTION] = {
		.name = "Content Protection",
		.api_size = sizeof(__s32),
		.set_function =
			(custom_set_function) vpu_set_content_protection,
		.get_function =
			(custom_get_function) vpu_get_content_protection,
	},
	[VPU_CTRL_DISPLAY_REFRESH_RATE] = {
		.name = "Display Refresh Rate",
		.type = CTRL_TYPE_VALUE,
		.hfi_id = VPU_PROP_SESSION_DISPLAY_FPS,
	},
	[VPU_CTRL_HQV] = {
		.name = "HQV",
		.hfi_id = VPU_PROP_SESSION_AUTO_HQV,
		.api_size = sizeof(struct vpu_ctrl_hqv),
		.hfi_size = sizeof(struct vpu_prop_session_auto_hqv),
		.bounds = {
			.min = VPU_SHARPEN_STRENGTH_MIN,
			.max = VPU_SHARPEN_STRENGTH_MAX,
			.step = 1,
			.def = VPU_SHARPEN_STRENGTH_MIN,
		},
		.bounds2 = {
			.min = VPU_AUTO_NR_LEVEL_MIN,
			.max = VPU_AUTO_NR_LEVEL_MAX,
			.step = VPU_AUTO_NR_LEVEL_STEP,
			.def = VPU_AUTO_NR_LEVEL_MIN,
		},
		.bounds_check = hqv_bounds_check,
		.translate_to_hfi = translate_hqv_to_hfi,
	},
	[VPU_CTRL_HQV_SHARPEN] = {
		.name = "HQV Sharpen",
		.type = CTRL_TYPE_VALUE,
		.hfi_id = VPU_PROP_SESSION_AUTO_HQV_SHARPEN_STRENGTH,
		.bounds = {
			.min = VPU_SHARPEN_STRENGTH_MIN,
			.max = VPU_SHARPEN_STRENGTH_MAX,
			.step = 1,
			.def = VPU_SHARPEN_STRENGTH_MIN,
		},
	},
	[VPU_CTRL_HQV_AUTONR] = {
		.name = "HQV AutoNR",
		.type = CTRL_TYPE_VALUE,
		.hfi_id = VPU_PROP_SESSION_AUTO_HQV_AUTONR_STRENGTH,
		.bounds = {
			.min = VPU_AUTONR_STRENGTH_MIN,
			.max = VPU_AUTONR_STRENGTH_MAX,
			.step = 1,
			.def = VPU_AUTONR_STRENGTH_MIN,
		},
	},
	[VPU_CTRL_ACE] = {
		.name = "ACE",
		.type = CTRL_TYPE_STANDARD,
		.hfi_id = VPU_PROP_SESSION_ACE,
	},
	[VPU_CTRL_ACE_BRIGHTNESS] = {
		.name = "ACE Brightness",
		.type = CTRL_TYPE_VALUE,
		.hfi_id = VPU_PROP_SESSION_ACE_BRIGHTNESS,
		.bounds = {
			.min = VPU_ACE_BRIGHTNESS_MIN,
			.max = VPU_ACE_BRIGHTNESS_MAX,
			.step = VPU_ACE_BRIGHTNESS_STEP,
			.def = VPU_ACE_BRIGHTNESS_MIN,
		},
	},
	[VPU_CTRL_ACE_CONTRAST] = {
		.name = "ACE Contrast",
		.type = CTRL_TYPE_VALUE,
		.hfi_id = VPU_PROP_SESSION_ACE_CONTRAST,
		.bounds = {
			.min = VPU_ACE_CONTRAST_MIN,
			.max = VPU_ACE_CONTRAST_MAX,
			.step = VPU_ACE_CONTRAST_STEP,
			.def = VPU_ACE_CONTRAST_MIN,
		},
	},
	[VPU_CTRL_2D3D] = {
		.name = "2D3D",
		.type = CTRL_TYPE_STANDARD,
		.hfi_id = VPU_PROP_SESSION_2D3D,
	},
	[VPU_CTRL_2D3D_DEPTH] = {
		.name = "2D3D Depth",
		.type = CTRL_TYPE_VALUE,
		.hfi_id = VPU_PROP_SESSION_2D3D_DEPTH,
		.bounds = {
			.min = VPU_2D3D_DEPTH_MIN,
			.max = VPU_2D3D_DEPTH_MAX,
			.step = VPU_2D3D_DEPTH_STEP,
			.def = VPU_2D3D_DEPTH_MIN,
		},
	},
	[VPU_INFO_TIMESTAMP] = {
		.name = "Info Timestamp",
		.hfi_id = VPU_PROP_SESSION_TIMESTAMP,
		.api_size = sizeof(struct vpu_info_frame_timestamp),
		.hfi_size = sizeof(struct vpu_prop_session_timestamp),
		.translate_to_hfi = translate_timestamp_to_hfi,
		.translate_to_api = translate_timestamp_to_api,
	},
	[VPU_CTRL_TIMESTAMP_INFO_MODE] = {
		.name = "Timestamp Info Mode",
		.type = CTRL_TYPE_VALUE,
		.hfi_id = VPU_PROP_SESSION_TIMESTAMP_AUTO_MODE,
	},
	[VPU_CTRL_FRC] = {
		.name = "FRC",
		.type = CTRL_TYPE_STANDARD,
		.hfi_id = VPU_PROP_SESSION_FRC,
	},
	[VPU_CTRL_FRC_MOTION_SMOOTHNESS] = {
		.name = "FRC Motion Smoothness",
		.type = CTRL_TYPE_VALUE,
		.hfi_id = VPU_PROP_SESSION_FRC_MOTION_SMOOTHNESS,
	},
	[VPU_CTRL_FRC_MOTION_CLEAR] = {
		.name = "FRC Motion Clear",
		.type = CTRL_TYPE_VALUE,
		.hfi_id = VPU_PROP_SESSION_FRC_MOTION_CLEAR,
		.bounds = {
			.min = VPU_FRC_MOTION_CLEAR_MIN,
			.max = VPU_FRC_MOTION_CLEAR_MAX,
			.step = VPU_FRC_MOTION_CLEAR_STEP,
			.def = VPU_FRC_MOTION_CLEAR_DEFAULT,
		},
	},
	[VPU_CTRL_LATENCY] = {
		.name = "Latency",
		.type = CTRL_TYPE_VALUE,
		.hfi_id = VPU_PROP_SESSION_LATENCY_REQUIREMENTS,
	},
	[VPU_INFO_STATISTICS] = {
		.name = "STATS",
		.type = CTRL_TYPE_RESERVED,
		.hfi_id = VPU_PROP_SESSION_STATS,
	},
};


static u32 controller_cache_size;

/*
 * Initializes some control descriptors if type has been set.
 * And calculates required cache size and offests for controls caches
 */
void setup_vpu_controls(void)
{
	struct vpu_ctrl_desc *desc, *prev_desc;
	int i;
	controller_cache_size = 0;

	/* First entry is an invalid control. Ensure it's zeroed out */
	memset(&ctrl_descriptors[0], 0, sizeof(struct vpu_ctrl_desc));

	for (i = 1; i < VPU_CTRL_ID_MAX; i++) {
		desc = &ctrl_descriptors[i];
		prev_desc = &ctrl_descriptors[i - 1];

		switch (desc->type) {
		case CTRL_TYPE_VALUE:
			desc->api_size = sizeof(s32);
			desc->hfi_size = sizeof(struct vpu_s_data_value);
			desc->bounds_check = value_bounds_check;
			desc->translate_to_api = translate_ctrl_value_to_api;
			desc->translate_to_hfi = translate_ctrl_value_to_hfi;
			break;
		case CTRL_TYPE_STANDARD:
			desc->api_size = sizeof(struct vpu_ctrl_standard);
			desc->hfi_size = sizeof(struct vpu_s_data_value);
			desc->bounds_check = standard_bounds_check;
			desc->translate_to_api = translate_ctrl_standard_to_api;
			desc->translate_to_hfi = translate_ctrl_standard_to_hfi;
			break;
		case CTRL_TYPE_AUTO_MANUAL:
			desc->api_size = sizeof(struct vpu_ctrl_auto_manual);
			desc->hfi_size = sizeof(struct vpu_s_data_value);
			desc->bounds_check = auto_manual_bounds_check;
			desc->translate_to_api =
					translate_ctrl_auto_manual_to_api;
			desc->translate_to_hfi =
					translate_ctrl_auto_manual_to_hfi;
			break;
		case CTRL_TYPE_RESERVED:
			desc->api_size = sizeof(union control_data);
			desc->hfi_size = sizeof(union control_data);
		default:
			break;
		}

		controller_cache_size += desc->api_size;
		desc->cache_offset = prev_desc->cache_offset +
				prev_desc->api_size;
	}
}

struct vpu_controller *init_vpu_controller(struct vpu_platform_resources *res)
{
	int i;
	struct vpu_controller *controller =
			kzalloc(sizeof(*controller), GFP_KERNEL);

	if (!controller) {
		pr_err("Failed memory allocation\n");
		return NULL;
	}

	controller->cache_size = controller_cache_size;
	if (controller->cache_size) {
		controller->cache = kzalloc(controller->cache_size, GFP_KERNEL);
		if (!controller->cache) {
			pr_err("Failed cache allocation\n");
			deinit_vpu_controller(controller);
			return NULL;
		}
	}

	for (i = 0; i < NUM_NR_BUFFERS; i++)
		controller->nr_buffers[i] =
				vpu_mem_create_handle(res->mem_client);

	if (i < NUM_NR_BUFFERS) {
		deinit_vpu_controller(controller);
		return NULL;
	}

	return controller;
}

void deinit_vpu_controller(struct vpu_controller *controller)
{
	int i;

	if (!controller)
		return;

	for (i = 0; i < NUM_NR_BUFFERS; i++) {
		vpu_mem_destroy_handle(controller->nr_buffers[i]);
		controller->nr_buffers[i] = NULL;
	}

	kfree(controller->cache);
	controller->cache = NULL;
	kfree(controller);
}

void *get_control(struct vpu_controller *controller, u32 id)
{
	struct vpu_ctrl_desc *desc;
	if (!controller || !controller->cache  ||
		id <= VPU_CTRL_ID_MIN || id >= VPU_CTRL_ID_MAX)
		return NULL;

	desc = &ctrl_descriptors[id];

	if (!desc->api_size || desc->cache_offset > controller->cache_size ||
	    (desc->cache_offset + desc->api_size) > controller->cache_size)
		return NULL;
	else
		return controller->cache + desc->cache_offset;
}

int apply_vpu_control(struct vpu_dev_session *session, int set,
		struct vpu_control *control)
{
	struct vpu_controller *controller = session->controller;
	struct vpu_ctrl_desc *desc;
	void *cache = NULL;
	int ret = 0;

	u8 hfi_data[128]; /* temporary store of data translated to HFI */
	u32 hfi_size = 0; /* size of HFI data */
	memset(hfi_data, 0, 128);

	if (!controller || !control)
		return -ENOMEM;

	if (!control->control_id || control->control_id >= VPU_CTRL_ID_MAX) {
		pr_err("control id %d is invalid\n", control->control_id);
		return -EINVAL;
	}

	desc = &ctrl_descriptors[control->control_id];
	if (!desc->hfi_id && !desc->api_size &&
			!desc->set_function && !desc->get_function) {
		pr_err("control id %d is not supported\n", control->control_id);
		return -EINVAL;
	}
	pr_debug("%s control %s\n", set ? "Set" : "Get", desc->name);

	cache = get_control(controller, control->control_id);
	if (!cache) {
		pr_err("control %s has no cache\n", desc->name);
		return -ENOMEM;
	}

	hfi_size = (desc->custom_size) ?
		call_op(desc, custom_size, &control->data) : desc->hfi_size;

	if (set) {
		ret = call_op(desc, bounds_check, desc, &control->data);
		if (ret) {
			pr_err("bounds check failed for %s\n", desc->name);
			return ret;
		}

		if (desc->translate_to_hfi) {
			call_op(desc, translate_to_hfi,
					&control->data, hfi_data);
		} else {
			hfi_size = desc->api_size;
			memcpy(hfi_data, &control->data, desc->api_size);
		}

		mutex_lock(&session->lock);

		if (desc->hfi_id) { /* set property */
			ret = vpu_hw_session_s_property(session->id,
					desc->hfi_id, hfi_data, hfi_size);
			if (ret) {
				pr_err("%s s_property failed\n", desc->name);
				goto err_apply_control;
			}
		}


		if (desc->set_function) {/* custom set function */
			ret = call_op(desc, set_function,
					session, &control->data);
			if (ret) {
				pr_err("%s set_function failed\n", desc->name);
				goto err_apply_control;
			}
		}

		if (desc->hfi_id) { /* commit control */
			ret = commit_control(session, 0);
			if (ret)
				goto err_apply_control;
		}

		/* update controls cache */
		memcpy(cache, &control->data, desc->api_size);

		mutex_unlock(&session->lock);
	} else {
		/* If cached is set, copy control data from cache */
		if (desc->cached) {
			memcpy(&control->data, cache, desc->api_size);
			return 0;
		}

		if (desc->get_function) /* custom get function */
			return call_op(desc, get_function,
					session, &control->data);

		if (desc->hfi_id) {

			ret = vpu_hw_session_g_property(session->id,
					desc->hfi_id, hfi_data, hfi_size);
			if (ret)
				return ret;

			if (desc->translate_to_api)
				call_op(desc, translate_to_api,
						hfi_data, &control->data);
			else
				memcpy(&control->data, hfi_data,
						desc->api_size);
		}
	}

	return ret;

err_apply_control:
	mutex_unlock(&session->lock);
	return ret;
}

int apply_vpu_control_extended(struct vpu_client *client, int set,
		struct vpu_control_extended *control)
{
	struct vpu_dev_session *session = client->session;
	int ret = 0;
	if (!control)
		return -ENOMEM;

	if (control->data_len > VPU_MAX_EXT_DATA_SIZE) {
		pr_err("data_len (%d) > than max (%d)\n",
				control->data_len, VPU_MAX_EXT_DATA_SIZE);
		return -EINVAL;
	} else if (!set && control->buf_size > VPU_MAX_EXT_DATA_SIZE) {
		pr_err("buf_size (%d) > than max (%d)\n",
				control->buf_size, VPU_MAX_EXT_DATA_SIZE);
		return -EINVAL;
	}

	if (control->type == 0) { /* system */
		if (set)
			ret = vpu_hw_sys_s_property_ext(control->data_ptr,
					control->data_len);
		else
			ret = vpu_hw_sys_g_property_ext(control->data_ptr,
					control->data_len, control->buf_ptr,
					control->buf_size);
	} else if (control->type == 1) { /* session */
		if (!session)
			return -EPERM;

		if (set)
			ret = vpu_hw_session_s_property_ext(session->id,
					control->data_ptr, control->data_len);
		else
			ret = vpu_hw_session_g_property_ext(session->id,
					control->data_ptr, control->data_len,
					control->buf_ptr, control->buf_size);
	} else {
		pr_err("control type %d is invalid\n", control->type);
		return -EINVAL;
	}

	return ret;
}

/*
 * Descriptions of supported formats
 */
static const struct vpu_format_desc vpu_port_formats[] = {
	[PIXEL_FORMAT_RGB888] = {
		.description = "RGB-8-8-8",
		.fourcc = V4L2_PIX_FMT_RGB24,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 24, .heightfactor = 1},
	},
	[PIXEL_FORMAT_XRGB8888] = {
		.description = "ARGB-8-8-8-8",
		.fourcc = V4L2_PIX_FMT_RGB32,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 32, .heightfactor = 1},
	},
	[PIXEL_FORMAT_XRGB2] = {
		.description = "ARGB-2-10-10-10",
		.fourcc = V4L2_PIX_FMT_XRGB2,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 32, .heightfactor = 1},
	},
	[PIXEL_FORMAT_BGR888] = {
		.description = "BGR-8-8-8",
		.fourcc = V4L2_PIX_FMT_BGR24,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 24, .heightfactor = 1},
	},
	[PIXEL_FORMAT_BGRX8888] = {
		.description = "BGRX-8-8-8-8",
		.fourcc = V4L2_PIX_FMT_BGR32,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 32, .heightfactor = 1},
	},
	[PIXEL_FORMAT_XBGR2] = {
		.description = "XBGR-2-10-10-10",
		.fourcc = V4L2_PIX_FMT_XBGR2,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 32, .heightfactor = 1},
	},
	[PIXEL_FORMAT_NV12] = {
		.description = "YUV 4:2:0 semi-planar",
		.fourcc = V4L2_PIX_FMT_NV12,
		.num_planes = 2,
		.plane[0] = { .bitsperpixel = 8, .heightfactor = 1},
		.plane[1] = { .bitsperpixel = 8, .heightfactor = 2},
	},
	[PIXEL_FORMAT_NV21] = {
		.description = "YVU 4:2:0 semi-planar",
		.fourcc = V4L2_PIX_FMT_NV21,
		.num_planes = 2,
		.plane[0] = { .bitsperpixel = 8, .heightfactor = 1},
		.plane[1] = { .bitsperpixel = 8, .heightfactor = 2},
	},
	[PIXEL_FORMAT_YUYV] = {
		.description = "YUYV 4:2:2 intlvd",
		.fourcc = V4L2_PIX_FMT_YUYV,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 16, .heightfactor = 1},
	},
	[PIXEL_FORMAT_YVYU] = {
		.description = "YVYU 4:2:2 intlvd",
		.fourcc = V4L2_PIX_FMT_YVYU,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 16, .heightfactor = 1},
	},
	[PIXEL_FORMAT_VYUY] = {
		.description = "VYUY 4:2:2 intlvd",
		.fourcc = V4L2_PIX_FMT_VYUY,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 16, .heightfactor = 1},
	},
	[PIXEL_FORMAT_UYVY] = {
		.description = "UYVY 4:2:2 intlvd",
		.fourcc = V4L2_PIX_FMT_UYVY,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 16, .heightfactor = 1},
	},
	[PIXEL_FORMAT_YUYV10_LOOSE] = {
		.description = "YUYV 4:2:2 10bit intlvd loose",
		.fourcc = V4L2_PIX_FMT_YUYV10,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 24, .heightfactor = 1},
	},
	[PIXEL_FORMAT_YUV_8BIT_INTERLEAVED_DENSE] = {
		.description = "YUV 4:4:4 8bit intlvd dense",
		.fourcc = V4L2_PIX_FMT_YUV8,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 24, .heightfactor = 1},
	},
	[PIXEL_FORMAT_YUV_10BIT_INTERLEAVED_LOOSE] = {
		.description = "YUV 4:4:4 10bit intlvd loose",
		.fourcc = V4L2_PIX_FMT_YUV10,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 32, .heightfactor = 1},
	},
	[PIXEL_FORMAT_COMPRESSED_YUYV422] = {
		.description = "YUYV422 Compressed",
		.fourcc = V4L2_PIX_FMT_YUYV10BWC,
		.num_planes = 1,
		.plane[0] = { .bitsperpixel = 21, .heightfactor = 1},
	},
};

#define PADDING 128
#define CEIL(x, y) (((x) + ((y)-1)) / (y))
u32 get_bytesperline(u32 width, u32 bitsperpixel, u32 input_bytesperline,
		u32 pixelformat)
{
	u32 bytesperline, padding_factor, min_bytesperline;

	if (pixelformat == V4L2_PIX_FMT_YUYV10BWC)
		bytesperline = CEIL(width * 64, 24);
	else
		bytesperline = CEIL(width * bitsperpixel, 8);

	padding_factor = CEIL(bytesperline, PADDING);
	min_bytesperline = PADDING * padding_factor;

	if (!input_bytesperline) {
		return min_bytesperline;
	} else if (input_bytesperline < min_bytesperline ||
			input_bytesperline % PADDING ||
			input_bytesperline > SZ_16K) {
		pr_err("Invalid input bytesperline %d\n", input_bytesperline);
		return 0;
	} else {
		return input_bytesperline;
	}
}

int is_format_valid(struct v4l2_format *fmt)
{
#define VPU_DIM_MINIMUM 72
	u32 height = fmt->fmt.pix_mp.height;
	u32 width = fmt->fmt.pix_mp.width;

	if (width < VPU_DIM_MINIMUM || height < VPU_DIM_MINIMUM)
		return 0;
	if ((height & 1) || (width & 1)) /* must be even */
		return 0;

	if (fmt->fmt.pix_mp.colorspace <= VPU_CS_MIN
		|| fmt->fmt.pix_mp.colorspace >= VPU_CS_MAX)
		fmt->fmt.pix_mp.colorspace = 0;

	return 1;
}

const struct vpu_format_desc *query_supported_formats(int index)
{
	if (index < 0 || index >= ARRAY_SIZE(vpu_port_formats))
		return NULL;
	if (vpu_port_formats[index].fourcc == 0)
		return NULL; /* fourcc of 0 indicates no support in API */
	else
		return &vpu_port_formats[index];
}

/*
 * Configuration commit handling
 */
static int __get_bits_per_pixel(u32 api_pix_fmt)
{
	int i, bpp = 0;
	const struct vpu_format_desc *vpu_format;
	u32 hfi_pix_fmt = translate_pixelformat_to_hfi(api_pix_fmt);

	vpu_format = query_supported_formats(hfi_pix_fmt);
	if (!vpu_format)
		return -EINVAL;

	for (i = 0; i < vpu_format->num_planes; i++)
		bpp  += vpu_format->plane[i].bitsperpixel /
			vpu_format->plane[i].heightfactor;

	return bpp;
}

/*
 * Power mode / Bus load computation
 */

#define SESSION_IS_ACTIVE(s, cur)	(((s) == (cur)) ? true : \
					(s)->streaming_state == ALL_STREAMING)
#define TO_KILO(val)			((val) / 1000)

#define SW_OVERHEAD_MS			1
#define STRIPE_OVERHEAD_PERCENT		5
#define MAX_FPS				60

enum {
	VIP_MIP = 0,
	VIP_SIP,
	VIP_TNR,
	VOP_TNR,
	VOP_SOP,
	VIP_SCL,
	VOP_MOP,

	NUM_STAGES,
};

/*
 * __get_vpu_load() - compute the average load value for all sessions (in kbps)
 *
 * 1) Pick the highest session's load (say "load_kbps")
 * 2) Increase it by "software overhead factor"
 *    with "software overhead factor"
 *    = (1 / max_fps) / (1 / max_fps / num_sessions - SW_OVERHEAD_MS / 1000)
 *    = (1000*num_sessions) / (1000 - (num_sessions*SW_OVERHEAD_MS*max_fps))
 * 3) Increase the result by "stripe overhead"
 *    with "stripe overhead"
 *    = 1 + (STRIPE_OVERHEAD_PERCENT / 100)
 *    = (100 + STRIPE_OVERHEAD_PERCENT) / 100
 * => load_kbps *= "software overhead factor" * "stripe overhead"
 */
static u32 __get_vpu_load(struct vpu_dev_session *cur_session)
{
	struct vpu_dev_core *core = cur_session->core;
	struct vpu_dev_session *session = NULL;
	u32 num_sessions = 0;
	u32 denominator = 1;
	u64 load_kbps = 0;
	int i;

	for (i = 0; i < VPU_NUM_SESSIONS; i++) {
		session = core->sessions[i];
		if (!session || !SESSION_IS_ACTIVE(session, cur_session))
			continue;

		pr_debug("session %d load: %dkbps\n", i, session->load);
		load_kbps = max_t(u32, load_kbps, session->load);

		num_sessions++;
	}

	/* software overhead factor */
	load_kbps *= MSEC_PER_SEC * num_sessions;
	denominator *= MSEC_PER_SEC - (num_sessions * SW_OVERHEAD_MS * MAX_FPS);

	/* stripe overhead */
	load_kbps *= 100 + STRIPE_OVERHEAD_PERCENT;
	denominator *= 100;

	do_div(load_kbps, denominator);

	pr_debug("Calculated load = %dkbps\n", (u32)load_kbps);
	return (u32)load_kbps;
}

/* returns the session load in kilo bits per second */
static u32 __calculate_session_load(struct vpu_dev_session *session)
{
	struct vpu_controller *controller = session->controller;
	struct vpu_port_info *in = &session->port_info[INPUT_PORT];
	struct vpu_port_info *out = &session->port_info[OUTPUT_PORT];
	struct vpu_ctrl_auto_manual *nr_ctrl;
	u32 fps, in_bpp, out_bpp, nrwb_bpp;
	bool interlaced, nr, bbroi;
	u32 stage_load_factor[NUM_STAGES];
	u32 load_kbps = 0;
	int i;

	/* get required info before computation */
	fps = max(in->framerate, out->framerate) >> 16;
	fps = fps ? fps : 30;
	nrwb_bpp = __get_bits_per_pixel(V4L2_PIX_FMT_YUYV);
	in_bpp = __get_bits_per_pixel(in->format.pixelformat);
	out_bpp = __get_bits_per_pixel(out->format.pixelformat);

	interlaced = in->scan_mode == LINESCANINTERLACED;
	nr_ctrl = get_control(controller, VPU_CTRL_NOISE_REDUCTION);
	nr = nr_ctrl ? (nr_ctrl->enable == PROP_TRUE) : false;
	bbroi = false; /* TODO: how to calculate? */

	/* compute the current session's load at each stage */
	stage_load_factor[VIP_MIP] = in_bpp;
	stage_load_factor[VIP_SIP] = !interlaced ? 0 : in_bpp;
	stage_load_factor[VIP_TNR] = !nr ? 0 : nrwb_bpp;
	stage_load_factor[VOP_TNR] = !nr ? 0 : nrwb_bpp;
	stage_load_factor[VOP_SOP] = !bbroi ? 0 : nrwb_bpp;
	stage_load_factor[VIP_SCL] = !bbroi ? 0 : nrwb_bpp;
	stage_load_factor[VOP_MOP] = out_bpp;

	/* final bus load for the session (before software overhead factor) */
	for (i = 0; i <= VIP_SCL; i++)
		load_kbps += stage_load_factor[i] *
			TO_KILO(in->format.width * in->format.height * fps);
	for (i = VOP_MOP; i < NUM_STAGES; i++)
		load_kbps += stage_load_factor[i] *
			TO_KILO(out->format.width * out->format.height * fps);

	/* approximately a 25% BW increase if dual output is enabled */
	if (session->dual_output)
		load_kbps = (load_kbps * 5) / 4;

	return load_kbps;
}

/* __get_power_mode() - returns the VPU frequency mode (enum vpu_power_mode)
 *
 * 1) Determine the pixel rate of each session:
 *    (max(in_h, out_h) * max (in_v, out_v) * frame_rate
 * 2) If the resolution is higher than 2MP, then use turbo frequency
 * 3) For each frequency mode less than turbo:
 *	3a) if single session and pixel rate (pr) is less than threshold, then
 *	    use this frequency mode
 *	3b) if dual session and each session's pixel rate is less than
 *	    threshold/2, then use this frequency mode
 * 4) If no frequency is chosen, choose turbo
 */
static u32 __get_power_mode(struct vpu_dev_session *cur_sess)
{
	struct vpu_dev_core *core = cur_sess->core;
	struct vpu_dev_session *session = NULL;
	enum vpu_power_mode m, mode = VPU_POWER_TURBO;
	u32 max_w = 0, max_h = 0, fps, pr[VPU_NUM_SESSIONS] = {0, };
	int i, num_sessions = 0;
	const u32 pr_threshold[VPU_POWER_MAX] = {
		[VPU_POWER_SVS]     =  67000000,
		[VPU_POWER_NOMINAL] = 134000000,
		[VPU_POWER_TURBO]   = 260000000,
	};

	for (i = 0; i < VPU_NUM_SESSIONS; i++) {
		struct vpu_port_info *in, *out;
		session = core->sessions[i];
		if (!session || !SESSION_IS_ACTIVE(session, cur_sess))
			continue;

		in  = &session->port_info[INPUT_PORT];
		out = &session->port_info[OUTPUT_PORT];

		max_w = max(in->format.width,  out->format.width);
		max_h = max(in->format.height, out->format.height);
		if ((max_w * max_h) > SZ_2M)
			goto exit_and_return_mode;

		fps = max(in->framerate, out->framerate) >> 16;
		fps = fps ? fps : 30;

		pr[i] = max_w * max_h * fps;
		pr_debug("session %d's pixel rate is %d\n", i, pr[i]);

		num_sessions++;
	}

	for (m = VPU_POWER_SVS; m <= VPU_POWER_NOMINAL; m++) {
		bool eligible = true;

		for (i = 0; i < VPU_NUM_SESSIONS; i++) {
			session = core->sessions[i];
			if (!session || !SESSION_IS_ACTIVE(session, cur_sess))
				continue;

			eligible &= (pr[i] < (pr_threshold[m] / num_sessions));
		}
		if (eligible) {
			mode = m;
			break;
		}
	}

exit_and_return_mode:
	pr_debug("eligible for %s mode\n", (mode == VPU_POWER_SVS) ? "svs" :
			((mode == VPU_POWER_NOMINAL) ? "nominal" : "turbo"));
	return (u32)mode;
}

static int __configure_input_port(struct vpu_dev_session *session)
{
	struct vpu_prop_session_input in_param;
	struct vpu_ctrl_auto_manual *nr;
	int ret = 0;

	nr = get_control(session->controller, VPU_CTRL_NOISE_REDUCTION);
	ret = configure_nr_buffers(session, nr);
	if (ret) {
		pr_err("Failed to configure nr\n");
		return ret;
	}

	translate_input_format_to_hfi(&session->port_info[INPUT_PORT],
			&in_param);
	ret = vpu_hw_session_s_input_params(session->id,
			translate_port_id(INPUT_PORT), &in_param);
	if (ret) {
		pr_err("Failed to set input port config\n");
		return ret;
	}

	if (session->port_info[INPUT_PORT].source
					!= VPU_INPUT_TYPE_HOST) {
		struct vpu_data_value in_source_ch;
		memset(&in_source_ch, 0, sizeof(in_source_ch));
		in_source_ch.value = translate_input_source_ch(
				session->port_info[INPUT_PORT].source);
		ret = vpu_hw_session_s_property(session->id,
				VPU_PROP_SESSION_SOURCE_CONFIG,
				&in_source_ch, sizeof(in_source_ch));
		if (ret) {
			pr_err("Failed to set port 0 source ch\n");
			return ret;
		}
	}

	return 0;
}

static int __configure_output_port(struct vpu_dev_session *session, int port)
{
	struct vpu_prop_session_output out_param;
	int ret = 0;

	translate_output_format_to_hfi(&session->port_info[port],
			&out_param);
	ret = vpu_hw_session_s_output_params(session->id,
			translate_port_id(port), &out_param);
	if (ret) {
		pr_err("Failed to set output port %d config\n", port);
		return ret;
	}

	if (session->port_info[port].destination
					!= VPU_OUTPUT_TYPE_HOST) {
		struct vpu_data_pkt out_dest_ch;
		memset(&out_dest_ch, 0, sizeof(out_dest_ch));
		out_dest_ch.payload[0] = translate_output_destination_ch(
				session->port_info[port].destination);
		out_dest_ch.size = sizeof(out_dest_ch);
		ret = vpu_hw_session_s_property(session->id,
				VPU_PROP_SESSION_SINK_CONFIG,
				&out_dest_ch, sizeof(out_dest_ch));
		if (ret) {
			pr_err("Failed to set port %d dest ch\n", port);
			return ret;
		}
	}

	return 0;
}

static int __do_commit(struct vpu_dev_session *session,
		enum commit_type commit_type, int new_load)
{
	int ret;

	if (new_load)
		session->load = __calculate_session_load(session);

	ret = vpu_hw_session_commit(session->id, commit_type,
			__get_vpu_load(session), __get_power_mode(session));
	if (ret)
		pr_err("Commit Failed\n");
	else
		pr_debug("Commit successful\n");

	return ret;
}

int commit_initial_config(struct vpu_dev_session *session)
{
	int ret = 0;

	if (session->commit_state == COMMITED)
		return 0;

	ret = __configure_input_port(session);
	if (ret)
		return ret;

	ret = __configure_output_port(session, OUTPUT_PORT);
	if (ret)
		return ret;

	if (session->dual_output) {
		ret = __configure_output_port(session, OUTPUT_PORT2);
		if (ret)
			return ret;
	}

	ret = __do_commit(session, CH_COMMIT_AT_ONCE, 1);
	if (ret)
		return ret;

	session->commit_state = COMMITED;
	return 0;
}

int commit_port_config(struct vpu_dev_session *session,	int port, int new_load)
{
	int ret = 0;

	/* defer to initial session commit if not streaming */
	if (session->streaming_state != ALL_STREAMING) {
		session->commit_state = 0;
		return 0;
	}

	if (port == INPUT_PORT) {
		ret = __configure_input_port(session);
		if (ret)
			return ret;

	} else if (port == OUTPUT_PORT || port == OUTPUT_PORT2) {
		ret = __configure_output_port(session, port);
		if (ret)
			return ret;
	} else {
		return -EINVAL;
	}

	return __do_commit(session, CH_COMMIT_IN_ORDER, new_load);
}

int commit_control(struct vpu_dev_session *session, int new_load)
{
	/* defer to initial session commit if not streaming */
	if (session->streaming_state != ALL_STREAMING) {
		session->commit_state = 0;
		return 0;
	}

	return __do_commit(session, CH_COMMIT_AT_ONCE, new_load);
}
