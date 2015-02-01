/*
 * Copyright © 2014 Intel Corporation
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Author:
 * Shashank Sharma <shashank.sharma@intel.com>
 * Ramalingam C <Ramalingam.c@intel.com>
 */

#include <linux/uaccess.h>
#include <core/intel_color_manager.h>

/*
 * Array of function pointers, holding functions which loads
 * color correction capabilities of a platform. This function
 * will get all the color correction capabilities of a platfrom
 * to register with color manager
 */
typedef bool (*get_color_capabilities)(void *props_data, int object_type);
get_color_capabilities platform_color_capabilities[] = {
	vlv_get_color_correction,
	chv_get_color_correction,
};

/*
 * intel_color_manager_find
 * Find a color property based on its id for an interface and overlay engine
 * color_props = structure containing number of properties and
 *     pointer to array of so many color properties,
 *     set in interface or overlay engine
 * prop_id = property id (csc/gamma etc)
 */
struct color_property *intel_color_manager_find(
	struct color_capabilities *color_props, u8 prop_id)
{
	struct color_property *cp = NULL;
	u32 index = 0;
	u32 total_props = 0;

	if (!color_props) {
		pr_err("ADF: CM: This interface/overlay engine has no color capabilities\n");
		return NULL;
	}

	total_props = color_props->no_of_props;
	while (index < total_props) {
		cp = color_props->props[index++];
		if (cp && (cp->prop_id == prop_id)) {
			pr_info("ADF: CM: Found property %s(index=%d)\n",
				cp->name, index);
			return cp;
		}
	}
	pr_err("ADF: CM: Property Not Found: Cant find property with id=%d\n",
			(int)prop_id);
	return NULL;
}

/*
 * Disable a color manager property
 * This function assumes all the validation is done
 * by the caller.
 */
static bool intel_color_manager_disable(
	struct color_capabilities *color_props, u8 prop_id, u8 idx)
{
	struct color_property *cp;

	/* Find the propery based on id */
	cp = intel_color_manager_find(color_props, prop_id);
	if (!cp) {
		pr_err("ADF: CM: Could not find property with id=%d\n",
			(int)prop_id);
		return false;
	}

	if (!cp->validate || !cp->disable_property) {
		pr_err("ADF: CM: No function for validate/disable color property\n");
		return false;
	}

	if (!cp->validate(prop_id)) {
		pr_err("ADF: CM: Invalid input for platform (prop=%d)\n",
			(int)prop_id);
		return false;
	}

	if (!cp->disable_property(cp, idx)) {
		pr_err("ADF: CM: Disable property (%s) failed\n", cp->name);
		return false;
	}

	pr_info("ADF: CM Successfully disabled color correction (%s)\n",
								cp->name);
	return true;
}

/* Set/Apply a color correction on a pipe */
bool intel_color_manager_set(struct color_capabilities *color_props,
		u8 prop_id, u64 *data, u8 idx)
{
	struct color_property *cp;

	/* Find the propery based on id */
	cp = intel_color_manager_find(color_props, prop_id);
	if (!cp) {
		pr_err("ADF: CM: Set: Could not find property with id=%d\n",
			(int)prop_id);
		return false;
	}

	if (!cp->validate || !cp->set_property) {
		pr_err("ADF: CM: Set: No validation/set function for property %s\n",
			cp->name);
		return false;
	}

	pr_info("ADF: CM: Set: Found property %s\n", cp->name);

	if (!cp->validate(prop_id)) {
		pr_err("ADF: CM: Set: Invalid input for platform (prop=%d)\n",
			(int)prop_id);
		return false;
	}

	/* Apply color correction */
	if (!cp->set_property(cp, data, idx)) {
		pr_err("ADF: CM: Set: Set property (%s) failed\n", cp->name);
		return false;
	}

	pr_info("ADF: CM: Set: Successfully applied color correction (%s)\n",
			cp->name);
	return true;
}

/* Get color correction data */
bool intel_color_manager_get(struct color_capabilities *color_props,
	struct color_cmd __user *ubuf, u8 idx)
{
	struct color_cmd cmd;
	struct color_property *cp;

	/* Clean command buf */
	memset((void *)&cmd, 0, sizeof(cmd));

	/* Extract the command */
	if (copy_from_user((void *)&cmd, (const void *)ubuf,
			sizeof(struct color_cmd))) {
		pr_err("ADF: CM: Get: Copy failed\n");
		return false;
	}

	/* Find the propery based on id */
	cp = intel_color_manager_find(color_props, cmd.property);
	if (!cp) {
		pr_err("ADF: CM: Could not find property with id=%d\n",
			(int)cmd.property);
		return false;
	}

	/* Load status */
	if (copy_to_user((void __user *)&(ubuf->action),
			(const void *)&(cp->status), sizeof(ubuf->action))) {
		pr_err("ADF: CM: Get status, copy status failed\n");
		return false;
	}

	/* Load the current values */
	if (copy_to_user((void __user *)to_user_ptr(ubuf->data_ptr),
		(const void *)cp->lut, ubuf->size * sizeof(uint64_t))) {
		pr_err("ADF: CM: Get status, copy data failed\n");
		return false;
	}

	pr_info("ADF: CM: Successfully got color correction status(%s)\n",
			cp->name);
	return true;
}

/*
 * intel_color_manager_apply:
 * Parse, decode and Apply a change on an interface or overlay engine.
 */
bool intel_color_manager_apply(struct color_capabilities *color_props,
	struct color_cmd *ubuf, u8 idx)
{
	bool ret = false;
	struct color_cmd cmd;
	u64 *raw_data = NULL;

	if (!ubuf) {
		pr_err("ADF: CM: Apply: insufficient data\n");
		return false;
	}

	/* Clean command buf */
	memset((void *)&cmd, 0, sizeof(cmd));

	/* Extract the command */
	if (copy_from_user((void *)&cmd, (const void *)ubuf,
			sizeof(struct color_cmd))) {
		pr_err("ADF: CM: Apply: Copy failed\n");
		return false;
	}

	/* Validate command */
	if ((int)cmd.action < color_set ||
			(int)cmd.action > color_disable) {
		pr_err("ADF: CM: Invalid action %d\n", (int)cmd.action);
		return false;
	}

	if (cmd.action == color_set) {
		pr_info("ADF: CM: Enabling property\n");

		/* Validite size, min 1 block of data is required */
		if (cmd.size < COLOR_MANAGER_SIZE_MIN) {
			pr_err("ADF: CM: Invalid size=%d\n",
					(int)cmd.size);
			return false;
		}

		if (!cmd.data_ptr) {
			pr_err("ADF: CM: Expecting %d coeff data but got NULL\n",
					cmd.size);
			return false;
		}

		raw_data = kzalloc(cmd.size * sizeof(uint64_t), GFP_KERNEL);
		if (!raw_data) {
			pr_err("ADF: CM: Out of memory\n");
			return false;
		}

		/* Get the data */
		if (copy_from_user((void *)raw_data,
			(const void *)to_user_ptr(cmd.data_ptr),
				cmd.size * sizeof(uint64_t))) {
			pr_err("ADF: CM: copy from user data failed\n");
			ret = false;
			goto FREE_AND_RETURN;
		}

		/* Data loaded, now do changes in property */
		if (!intel_color_manager_set(color_props, cmd.property,
					raw_data, idx)) {
			pr_err("ADF: CM: Color correction failed\n");
			ret = false;
			goto FREE_AND_RETURN;
		}

		ret = true;
		pr_info("ADF: CM: Apply color correction success\n");
	} else {
		pr_info("ADF: CM: Disabling property\n");
		if (!intel_color_manager_disable(color_props,
						cmd.property, idx)) {
			pr_err("ADF: CM: Disabling color property failed\n");
			ret = false;
			goto FREE_AND_RETURN;
		}
		ret = true;
		pr_info("ADF: CM: Disable color correction success\n");
	}

FREE_AND_RETURN:
	kfree(raw_data);
	return ret;
}

/* Allocate an LUT for any color property to copy data from userspace */
u64 *intel_color_manager_create_lut(int size)
{
	u64 *lut;
	lut = kzalloc(size * sizeof(uint64_t), GFP_KERNEL);
	if (!lut)
		pr_err("ADF: CM: OOM in create LUT\n");
	return lut;
}

/* Get the color correction capabilities of a platform */
bool intel_get_color_capabilities(int platform_id,
	void *props_data, int object_type)
{
	/* Get the platform specific init data */
	if (platform_color_capabilities[platform_id]) {
		if (!platform_color_capabilities[platform_id](props_data,
								object_type)) {
			pr_err("ADF: CM: Getting color correction capabilities failed\n");
			return false;
		}
		pr_info("ADF: CM: Got platform color correction data\n");
	} else {
		pr_err("ADF: CM: No platform init function for platform id %d\n",
				platform_id);
		return false;
	}

	/* Check data validity */
	if (!props_data) {
		pr_err("ADF: CM: Invalid platform color correction capabilities\n");
		return false;
	}

	pr_info("ADF: CM: Got color correction capabilities for platform %d\n",
		platform_id);
	return true;
}

/*
 * intel_color_manager_register_pipe_props
 * Register Pipe level color properties by getting from pipe_props struct
 * Returns :
 *	Number of pipe properties registered
 *	-1 if any error
 */
int intel_color_manager_register_pipe_props(struct intel_pipe *pipe,
					struct pipe_properties *pipe_props)
{
	int no_of_properties = pipe_props->no_of_pipe_props;
	int count = 0;

	struct color_property *cp;

	while (count < no_of_properties) {
		cp = kzalloc(sizeof(struct color_property), GFP_KERNEL);
		if (!cp) {
			pr_err("ADF: CM: OOM whie loading %d property\n",
					count);

			goto OOM;
		}

		memcpy((void *) cp, (const void *)(pipe_props->props[count]),
				sizeof(struct color_property));

		pr_info("ADF: CM: CP[%d] len=%u prop(%s,%d)\n",
			count, cp->len,
				cp->name, cp->prop_id);

		cp->lut = intel_color_manager_create_lut(cp->len);
		if (!cp->lut) {
			pr_err("ADF: CM: Cant create LUT for prop %s\n",
					cp->name);

			kfree(cp);
			goto OOM;
		}

		pipe->color_ctx->props[count++] = cp;
		pr_info("ADF: CM: Registered %d property for Pipe\n", count);
	}

	pipe->color_ctx->no_of_props = count;

	return count;
OOM:
	while (count--) {
		kfree(pipe_props->props[count]->lut);
		kfree(pipe_props->props[count]);
	}

	pr_err("ADF: CM: Error loading pipe level color property\n");
	return -1;
}


/*
 * intel_color_manager_register_plane_props
 * Register Plane level color properties by getting from plane_properties struct
 * Returns :
 *	Number of plane properties registered
 *	-1 if any error
 */
int intel_color_manager_register_plane_props(struct intel_plane *plane,
					struct plane_properties *plane_props)
{
	int no_of_properties = plane_props->no_of_plane_props;
	int count = 0;

	struct color_property *cp;

	while (count < no_of_properties) {
		cp = kzalloc(sizeof(struct color_property), GFP_KERNEL);
		if (!cp) {
			pr_err("ADF: CM: OOM whie loading %d property\n",
					count);

			goto OOM;
		}

		memcpy((void *) cp,
			(const void *)(plane_props->props[count]),
				sizeof(struct color_property));

		pr_info("ADF: CM: CP[%d] len=%u prop(%s,%d)\n",
			count, cp->len,
				cp->name, cp->prop_id);

		cp->lut = intel_color_manager_create_lut(cp->len);
		if (!cp->lut) {
			pr_err("ADF: CM: Cant create LUT for prop %s\n",
					cp->name);
			kfree(cp);
			goto OOM;
		}

		plane->color_ctx->props[count++] = cp;
		pr_info("ADF: CM: Registered %d property for Plane\n", count);
	}
	plane->color_ctx->no_of_props = count;

	return count;

OOM:
	while (count--) {
		kfree(plane_props->props[count]->lut);
		kfree(plane_props->props[count]);
	}

	pr_err("ADF: CM: Error loading plane level color property\n");
	return -1;
}

bool intel_color_manager_pipe_init(struct intel_pipe *pipe, int platform_id)
{
	struct pipe_properties *pipe_props;

	/* Sanity */
	if (platform_id < 0 || !pipe) {
		pr_err("ADF: CM: Color correction init for pipe failed, %s\n",
			pipe ? " Invalid platform_id" : "Null pipe");
		return false;
	}

	pipe_props = kzalloc(sizeof(struct pipe_properties), GFP_KERNEL);

	if (!pipe_props) {
		pr_err("ADF: CM: No memory available for initializing pipe properties\n");
		return false;
	}

	pipe->color_ctx = kzalloc(sizeof(struct color_capabilities),
							GFP_KERNEL);
	if (!pipe->color_ctx) {
		pr_err("ADF: CM: No memory available for pipe Color Context\n");
		kfree(pipe_props);
		return false;
	}

	/* Get the color correction capabilites of a platform's pipe */
	if (!intel_get_color_capabilities(platform_id, (void *) pipe_props,
					CLRMGR_REQUEST_FROM_PIPE)) {
		pr_err("ADF: CM: Color correction init failed, no valid capabilities found\n");
		kfree(pipe_props);
		return false;
	}

	/* Got the pipe capabilities, Now register it */
	if (!intel_color_manager_register_pipe_props(pipe, pipe_props)) {
		pr_err("ADF: CM: Registering pipe color correction capabilities failed\n");
		kfree(pipe_props);
		return false;
	}

	pr_info("ADF: CM: Color correction init success\n");
	kfree(pipe_props);
	return true;
}

bool intel_color_manager_plane_init(struct intel_plane *plane, int platform_id)
{
	struct plane_properties *plane_props;

	/* Sanity */
	if (platform_id < 0 || !plane) {
		pr_err("ADF: CM: Color correction init for plane failed, %s\n",
			plane ? " Invalid platform_id" : "Null plane");
		return false;
	}

	plane_props = kzalloc(sizeof(struct plane_properties), GFP_KERNEL);

	if (!plane_props) {
		pr_err("ADF: CM: No memory available for initializing plane properties\n");
		return false;
	}

	plane->color_ctx = kzalloc(sizeof(struct color_capabilities),
							GFP_KERNEL);
	if (!plane->color_ctx) {
		pr_err("ADF: CM: No memory available for plane color context\n");
		kfree(plane_props);
		return false;
	}

	/* Get the color correction capabilites of a platform's plane */
	if (!intel_get_color_capabilities(platform_id, (void *) plane_props,
					CLRMGR_REQUEST_FROM_PLANE)) {
		pr_err("ADF: CM: Color correction init failed, no valid capabilities found\n");
		kfree(plane_props);
		return false;
	}

	/* Got the plane capabilities, Now register it */
	if (!intel_color_manager_register_plane_props(plane, plane_props)) {
		pr_err("ADF: CM: Registering plane color correction capabilities failed\n");
		kfree(plane_props);
		return false;
	}

	pr_info("ADF: CM: Color correction init success\n");
	kfree(plane_props);
	return true;
}
