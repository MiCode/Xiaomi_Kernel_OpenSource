/*
 * Copyright (c) 2014, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/of.h>
#include <linux/iio/light/ls_sysfs.h>

static const char *propname[NUM_PROP] = {
	"vendor"
};

static const char *dt_chan_sysfs_lut[MAX_CHAN][MAX_CHAN_PROP] = {
	{
		"illuminance,max-range",
		"illuminance,integration-time",
		"illuminance,resolution",
		"illuminance,power-consumed"
	},
	{
		"proximity,max-range",
		"proximity,integration-time",
		"proximity,power-consumed"
	}
};

/*
 * 1. create instance of lightsensor_spec,
 * 2. parse the DT node for the dev * and fill the lightsensor_spec
 * 3. return the filled lightsensor_spec if success.
 */
struct lightsensor_spec *of_get_ls_spec(struct device *dev)
{
	const char *prop_value;
	int i, j, ret;
	struct lightsensor_spec *ls_spec;
	bool is_empty = true;

	if (!dev->of_node)
		return NULL;

	ls_spec = devm_kzalloc(dev,
				sizeof(struct lightsensor_spec), GFP_KERNEL);
	if (!ls_spec)
		return NULL;

	/* fill values of dt properties in propname -> ls_spec->propname */
	for (i = 0; i < NUM_PROP; i++) {
		ret = of_property_read_string(dev->of_node, propname[i],
						&prop_value);
		if (!ret) {
			ls_spec->prop[i] = prop_value;
			is_empty = false;
		}
	}

	/*
	 * fill values of dt properties in dt_chan_sysfs_lut to
	 * ls_spec->chan_prop
	 */
	for (i = 0; i < MAX_CHAN; i++)
		for (j = 0; j < MAX_CHAN_PROP; j++)
			if (dt_chan_sysfs_lut[i][j]) {
				ret = of_property_read_string(dev->of_node,
					dt_chan_sysfs_lut[i][j], &prop_value);
				if (!ret) {
					ls_spec->chan_prop[i][j] = prop_value;
					is_empty = false;
				}
			}

	if (is_empty) {
		devm_kfree(dev, ls_spec);
		ls_spec = NULL;
	}

	return ls_spec;
}
