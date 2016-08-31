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
#include <linux/iio/light/ls_sysfs.h>

static const char *propname[NUM_PROP] = {
	"vendor"
};

/* look up table for light sensor's per channel sysfs */
static const char *sysfs_ls_lut[MAX_CHAN][MAX_CHAN_PROP] = {
	{
		"in_illuminance_max_range",
		"in_illuminance_integration_time",
		"in_illuminance_resolution",
		"in_illuminance_power_consumed"
	},
	{
		"in_proximity_max_range",
		"in_proximity_integration_time",
		"in_proximity_power_consumed"
	}
};

static const char *get_ls_spec_val(struct lightsensor_spec *ls_spec,
				const char *sysfs_name)
{
	int i, j;

	if (sysfs_name == NULL)
		return NULL;

	if (!ls_spec)
		return NULL;

	for (i = 0; i < NUM_PROP; i++)
		if (strstr(sysfs_name, propname[i]))
			return ls_spec->prop[i];

	for (i = 0; i < MAX_CHAN; i++)
		for (j = 0; j < MAX_CHAN_PROP; j++)
			if (sysfs_ls_lut[i][j] &&
				strstr(sysfs_name, sysfs_ls_lut[i][j]))
				return ls_spec->chan_prop[i][j];

	return NULL;
}

void fill_ls_attrs(struct lightsensor_spec *ls_spec,
			struct attribute **attrs)
{
	struct attribute *attr;
	struct device_attribute *dev_attr;
	struct iio_const_attr *iio_const_attr;
	int i;
	const char *val;

	if (!ls_spec || !attrs)
		return;

	for (i = 0, attr = attrs[i]; attr; attr = attrs[i++]) {
		dev_attr = container_of(attr, struct device_attribute, attr);
		iio_const_attr = to_iio_const_attr(dev_attr);
		val = get_ls_spec_val(ls_spec, attr->name);
		if (val)
			iio_const_attr->string = val;
	}
}
