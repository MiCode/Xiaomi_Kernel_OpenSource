/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "core/common/backlight_dev.h"
#include "intel_adf.h"

static int get_brightness(struct backlight_device *bl_dev)
{
	if (!bl_dev) {
		pr_err("%s: invalid argument\n", __func__);
		return -EINVAL;
	}

	pr_debug("brightness = 0x%x\n", bl_dev->props.brightness);

	return bl_dev->props.brightness;
}

static int set_brightness(struct backlight_device *bl_dev)
{
	int level = 0, i = 0, ret = 0;
	struct intel_adf_context *adf_ctx;
	struct intel_adf_interface *intf;
	struct intel_pipe *pipe;

	if (!bl_dev) {
		pr_err("%s: invalid argument\n", __func__);
		return -EINVAL;
	}

	adf_ctx = bl_get_data(bl_dev);
	if (!adf_ctx)
		return -EINVAL;

	level = bl_dev->props.brightness;
	/* Perform value bounds checking */
	if (level < BRIGHTNESS_MIN_LEVEL)
		level = BRIGHTNESS_MIN_LEVEL;

	if (level > BRIGHTNESS_MAX_LEVEL)
		level = BRIGHTNESS_MAX_LEVEL;

	pr_debug("%s: level is %d\n", __func__, level);

	for (i = 0; i < adf_ctx->n_intfs; i++) {
		intf = &adf_ctx->intfs[i];
		if (!intf)
			continue;

		pipe = intf->pipe;
		if (!pipe || !pipe->ops)
			continue;

		/*
		 * Primary display should support brightness setting,
		 * but external display may not.
		 */
		if (!pipe->ops->set_brightness) {
			pr_debug("%s: pipe %s doesn't support brightness",
				 __func__, pipe->base.name);
			continue;
		}

		ret = pipe->ops->set_brightness(pipe, level);
		if (ret)
			return ret;
	}

	return ret;
}

const struct backlight_ops bl_ops = {
	.get_brightness = get_brightness,
	.update_status = set_brightness,
};

int backlight_init(struct intel_adf_context *adf_ctx)
{
	struct backlight_properties props;

	if (!adf_ctx) {
		pr_err("%s: invalid argument\n", __func__);
		return -EINVAL;
	}

	memset(&props, 0, sizeof(struct backlight_properties));
	props.max_brightness = BRIGHTNESS_MAX_LEVEL;
	props.type = BACKLIGHT_RAW;

	/* Use the legacy backlight device name 'psb-bl'... */
	adf_ctx->bl_dev = backlight_device_register("psb-bl",
			NULL, (void *)adf_ctx, &bl_ops, &props);
	if (IS_ERR(adf_ctx->bl_dev))
		return PTR_ERR(adf_ctx->bl_dev);

	adf_ctx->bl_dev->props.brightness = BRIGHTNESS_INIT_LEVEL;
	adf_ctx->bl_dev->props.max_brightness = BRIGHTNESS_MAX_LEVEL;

	return 0;
}

void backlight_exit(struct backlight_device *bl_dev)
{
	if (bl_dev)
		backlight_device_unregister(bl_dev);
}
