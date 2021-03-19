#include <linux/device.h>
#include <linux/backlight.h>
#include <linux/thermal.h>
#include <linux/notifier.h>

#include "sde_connector.h"
#include "dsi_panel.h"

struct sde_connector;
struct sde_clone_cdev {
	struct thermal_cooling_device *cdev;
	struct backlight_device *bd;
	struct dsi_panel *panel;
};

int sde_backlight_clone_setup(struct sde_connector *c_conn,
			struct device *dev,
			struct backlight_device *bd);
void backlight_clone_cdev_unregister(struct sde_clone_cdev *cdev_clone);
