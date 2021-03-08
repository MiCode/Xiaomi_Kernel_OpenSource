#include <linux/err.h>
#include <linux/slab.h>
#include "sde_dbg.h"

#include "clone_cooling_device.h"
#include "../dsi/dsi_display.h"
#include "../dsi/dsi_panel_mi.h"

#define BL_NODE_NAME_SIZE 32

static int bd_cdev_get_max_brightness_clone(struct thermal_cooling_device *cdev,
					unsigned long *state)
{
	struct sde_clone_cdev *cdev_clone;
	struct backlight_device *bd;

	if (!cdev)
		return -EINVAL;

	cdev_clone = (struct sde_clone_cdev *)cdev->devdata;
	if (!cdev_clone)
		return -EINVAL;

	bd = cdev_clone->bd;
	if (!bd)
		return -ENODEV;

	if (!cdev_clone->panel)
		return -ENODEV;

	*state = cdev_clone->panel->mi_cfg.max_brightness_clone;
	return 0;
}

static int bd_cdev_get_cur_brightness_clone(struct thermal_cooling_device *cdev,
                                        unsigned long *state)
{
	struct sde_clone_cdev *cdev_clone;
	struct backlight_device *bd;

	if (!cdev)
		return -EINVAL;

	cdev_clone = (struct sde_clone_cdev *)cdev->devdata;
	if (!cdev_clone)
		return -EINVAL;

	bd = cdev_clone->bd;
	if (!bd)
		return -ENODEV;

	*state = bd->thermal_brightness_clone_limit;
	return 0;
}

static int bd_cdev_set_cur_brightness_clone(struct thermal_cooling_device *cdev,
					unsigned long state)
{
	struct sde_clone_cdev *cdev_clone;
	unsigned long brightness_lvl;
	struct backlight_device *bd;
	int rc = 0;

	if (!cdev)
		return -EINVAL;

	cdev_clone = (struct sde_clone_cdev *)cdev->devdata;
	if (!cdev_clone)
		return -EINVAL;

	bd = cdev_clone->bd;
	if (!bd)
		return -ENODEV;

	if (state > cdev_clone->panel->mi_cfg.max_brightness_clone)
		return -EINVAL;

	brightness_lvl = cdev_clone->panel->mi_cfg.max_brightness_clone - state;
	if (brightness_lvl == bd->thermal_brightness_limit)
		return 0;
	bd->thermal_brightness_clone_limit = brightness_lvl;
	brightness_lvl = (bd->props.brightness_clone_backup
				<= bd->thermal_brightness_clone_limit) ?
				bd->props.brightness_clone_backup :
				bd->thermal_brightness_clone_limit;

	SDE_INFO("backup_brightness_clone[%d], thermal limit[%d]\n", bd->props.brightness_clone_backup, bd->thermal_brightness_clone_limit);
	bd->props.brightness_clone = brightness_lvl;
	sysfs_notify(&bd->dev.kobj, NULL, "brightness_clone");

	return rc;
}

static struct thermal_cooling_device_ops bd_cdev_clone_ops = {
	.get_max_state = bd_cdev_get_max_brightness_clone,
	.get_cur_state = bd_cdev_get_cur_brightness_clone,
	.set_cur_state = bd_cdev_set_cur_brightness_clone,
};

int backlight_clone_cdev_register(struct sde_clone_cdev *cdev_clone,
					struct device *parent,
					struct backlight_device *bd)
{
	static int display_clone_count;
	char bl_node_name[BL_NODE_NAME_SIZE];

	if (!bd || !parent || !cdev_clone)
		return -EINVAL;
	if (!of_find_property(parent->of_node, "#cooling-cells", NULL))
		return -ENODEV;
	snprintf(bl_node_name, BL_NODE_NAME_SIZE, "brightness%u-clone", display_clone_count++);

	cdev_clone->bd = bd;
	cdev_clone->cdev = thermal_of_cooling_device_register(parent->of_node,
				bl_node_name, cdev_clone, &bd_cdev_clone_ops);

	if (!&(cdev_clone->cdev)) {
		pr_err("Cooling device register failed\n");
		return -EINVAL;
	}
	else
		display_clone_count++;

	return 0;
}

void backlight_clone_cdev_unregister(struct sde_clone_cdev *cdev_clone)
{
	if (!cdev_clone)
		return;

	thermal_cooling_device_unregister(cdev_clone->cdev);
}

int sde_backlight_clone_setup(struct sde_connector *c_conn, struct device *parent, struct backlight_device *bd)
{
	struct dsi_display *display;
	struct sde_clone_cdev *cdev_clone = NULL;
	int rc = 0;

	if (!c_conn || !parent || !bd)
		return -ENOMEM;

	display = (struct dsi_display *) c_conn->display;
	if (!display || !display->panel)
		return -ENOMEM;

	cdev_clone = devm_kzalloc(parent, sizeof(*cdev_clone), GFP_KERNEL);
	if (!cdev_clone)
		return -ENOMEM;

	cdev_clone->panel = display->panel;
	bd->thermal_brightness_clone_limit = cdev_clone->panel->mi_cfg.max_brightness_clone;
	rc = backlight_clone_cdev_register(cdev_clone, parent, bd);
	if (rc) {
		pr_err("Failed to register backlight_clone_cdev\n");
		return -ENODEV;
	}
	c_conn->cdev_clone = cdev_clone;

	return 0;
}
