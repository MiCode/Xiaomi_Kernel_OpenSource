/*
 * Copyright (C) 2012-2017 InvenSense, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/device.h>
#include <linux/err.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/export.h>

#include <linux/iio/imu/mpu.h>
#include "inv_mpu_dts.h"
#include "inv_mpu_iio.h"

#ifdef CONFIG_OF

static int inv_mpu_power_on(struct mpu_platform_data *pdata)
{
	int err;

	if (!IS_ERR(pdata->vdd_ana)) {
		err = regulator_enable(pdata->vdd_ana);
		if (err)
			return err;
	}
	if (!IS_ERR(pdata->vdd_i2c)) {
		err = regulator_enable(pdata->vdd_i2c);
		if (err)
			goto error_disable_vdd_ana;
	}

	return 0;

error_disable_vdd_ana:
	regulator_disable(pdata->vdd_ana);
	return err;
}

static int inv_mpu_power_off(struct mpu_platform_data *pdata)
{
	if (!IS_ERR(pdata->vdd_ana))
		regulator_disable(pdata->vdd_ana);
	if (!IS_ERR(pdata->vdd_i2c))
		regulator_disable(pdata->vdd_i2c);

	return 0;
}

static int inv_parse_orientation_matrix(struct device *dev, s8 *orient)
{
	int rc, i;
	struct device_node *np = dev->of_node;
	u32 temp_val, temp_val2;

	for (i = 0; i < 9; i++)
		orient[i] = 0;

	/* parsing axis x orientation matrix */
	rc = of_property_read_u32(np, "axis_map_x", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read axis_map_x\n");
		return rc;
	}
	rc = of_property_read_u32(np, "negate_x", &temp_val2);
	if (rc) {
		dev_err(dev, "Unable to read negate_x\n");
		return rc;
	}
	if (temp_val2)
		orient[temp_val] = -1;
	else
		orient[temp_val] = 1;

	/* parsing axis y orientation matrix */
	rc = of_property_read_u32(np, "axis_map_y", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read axis_map_y\n");
		return rc;
	}
	rc = of_property_read_u32(np, "negate_y", &temp_val2);
	if (rc) {
		dev_err(dev, "Unable to read negate_y\n");
		return rc;
	}
	if (temp_val2)
		orient[3 + temp_val] = -1;
	else
		orient[3 + temp_val] = 1;

	/* parsing axis z orientation matrix */
	rc = of_property_read_u32(np, "axis_map_z", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read axis_map_z\n");
		return rc;
	}
	rc = of_property_read_u32(np, "negate_z", &temp_val2);
	if (rc) {
		dev_err(dev, "Unable to read negate_z\n");
		return rc;
	}
	if (temp_val2)
		orient[6 + temp_val] = -1;
	else
		orient[6 + temp_val] = 1;

	return 0;
}

static int inv_parse_secondary_orientation_matrix(struct device *dev,
						  s8 *orient)
{
	int rc, i;
	struct device_node *np = dev->of_node;
	u32 temp_val, temp_val2;

	for (i = 0; i < 9; i++)
		orient[i] = 0;

	/* parsing axis x orientation matrix */
	rc = of_property_read_u32(np, "inven,secondary_axis_map_x", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read secondary axis_map_x\n");
		return rc;
	}
	rc = of_property_read_u32(np, "inven,secondary_negate_x", &temp_val2);
	if (rc) {
		dev_err(dev, "Unable to read secondary negate_x\n");
		return rc;
	}
	if (temp_val2)
		orient[temp_val] = -1;
	else
		orient[temp_val] = 1;

	/* parsing axis y orientation matrix */
	rc = of_property_read_u32(np, "inven,secondary_axis_map_y", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read secondary axis_map_y\n");
		return rc;
	}
	rc = of_property_read_u32(np, "inven,secondary_negate_y", &temp_val2);
	if (rc) {
		dev_err(dev, "Unable to read secondary negate_y\n");
		return rc;
	}
	if (temp_val2)
		orient[3 + temp_val] = -1;
	else
		orient[3 + temp_val] = 1;

	/* parsing axis z orientation matrix */
	rc = of_property_read_u32(np, "inven,secondary_axis_map_z", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read secondary axis_map_z\n");
		return rc;
	}
	rc = of_property_read_u32(np, "inven,secondary_negate_z", &temp_val2);
	if (rc) {
		dev_err(dev, "Unable to read secondary negate_z\n");
		return rc;
	}
	if (temp_val2)
		orient[6 + temp_val] = -1;
	else
		orient[6 + temp_val] = 1;

	return 0;
}

static int inv_parse_secondary(struct device *dev,
			       struct mpu_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	u32 temp_val;
	const char *name;

	if (of_property_read_string(np, "inven,secondary_type", &name)) {
		dev_err(dev, "Missing secondary type.\n");
		return -EINVAL;
	}
	if (!strcmp(name, "compass")) {
		pdata->sec_slave_type = SECONDARY_SLAVE_TYPE_COMPASS;
	} else if (!strcmp(name, "none")) {
		pdata->sec_slave_type = SECONDARY_SLAVE_TYPE_NONE;
		return 0;
	} else {
		return -EINVAL;
	}

	if (of_property_read_string(np, "inven,secondary_name", &name)) {
		dev_err(dev, "Missing secondary name.\n");
		return -EINVAL;
	}
	if (!strcmp(name, "ak8963"))
		pdata->sec_slave_id = COMPASS_ID_AK8963;
	else if (!strcmp(name, "ak8975"))
		pdata->sec_slave_id = COMPASS_ID_AK8975;
	else if (!strcmp(name, "ak8972"))
		pdata->sec_slave_id = COMPASS_ID_AK8972;
	else if (!strcmp(name, "ak09911"))
		pdata->sec_slave_id = COMPASS_ID_AK09911;
	else if (!strcmp(name, "ak09912"))
		pdata->sec_slave_id = COMPASS_ID_AK09912;
	else if (!strcmp(name, "ak09916"))
		pdata->sec_slave_id = COMPASS_ID_AK09916;
	else
		return -EINVAL;
	rc = of_property_read_u32(np, "inven,secondary_reg", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read secondary register\n");
		return rc;
	}
	pdata->secondary_i2c_addr = temp_val;
	rc = inv_parse_secondary_orientation_matrix(dev,
						    pdata->
						    secondary_orientation);

	return rc;
}

static int inv_parse_aux(struct device *dev, struct mpu_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	u32 temp_val;
	const char *name;

	if (of_property_read_string(np, "inven,aux_type", &name)) {
		dev_err(dev, "Missing aux type.\n");
		return -EINVAL;
	}
	if (!strcmp(name, "pressure")) {
		pdata->aux_slave_type = SECONDARY_SLAVE_TYPE_PRESSURE;
	} else if (!strcmp(name, "none")) {
		pdata->aux_slave_type = SECONDARY_SLAVE_TYPE_NONE;
		return 0;
	} else {
		return -EINVAL;
	}

	if (of_property_read_string(np, "inven,aux_name", &name)) {
		dev_err(dev, "Missing aux name.\n");
		return -EINVAL;
	}
	if (!strcmp(name, "bmp280"))
		pdata->aux_slave_id = PRESSURE_ID_BMP280;
	else
		return -EINVAL;

	rc = of_property_read_u32(np, "inven,aux_reg", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read aux register\n");
		return rc;
	}
	pdata->aux_i2c_addr = temp_val;

	return 0;
}

static int inv_parse_readonly_secondary(struct device *dev,
				 	struct mpu_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	u32 temp_val;
	const char *name;

	if (of_property_read_string(np, "inven,read_only_slave_type", &name)) {
		dev_err(dev, "Missing read only slave type type.\n");
		return -EINVAL;
	}
	if (!strcmp(name, "als")) {
		pdata->read_only_slave_type = SECONDARY_SLAVE_TYPE_ALS;
	} else if (!strcmp(name, "none")) {
		pdata->read_only_slave_type = SECONDARY_SLAVE_TYPE_NONE;
		return 0;
	} else {
		return -EINVAL;
	}

	if (of_property_read_string(np, "inven,read_only_slave_name", &name)) {
		dev_err(dev, "Missing read only slave type name.\n");
		return -EINVAL;
	}
	if (!strcmp(name, "apds9930"))
		pdata->read_only_slave_id = ALS_ID_APDS_9930;
	else
		return -EINVAL;

	rc = of_property_read_u32(np, "inven,read_only_slave_reg", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read read only slave reg register\n");
		return rc;
	}
	pdata->read_only_i2c_addr = temp_val;

	return 0;
}

int invensense_mpu_parse_dt(struct device *dev, struct mpu_platform_data *pdata)
{
	int rc;

	rc = inv_parse_orientation_matrix(dev, pdata->orientation);
	if (rc)
		return rc;
	rc = inv_parse_secondary(dev, pdata);
	if (rc)
		return rc;
	inv_parse_aux(dev, pdata);

	inv_parse_readonly_secondary(dev, pdata);

	pdata->vdd_ana = regulator_get(dev, "inven,vdd_ana");
	if (IS_ERR(pdata->vdd_ana)) {
		rc = PTR_ERR(pdata->vdd_ana);
		dev_warn(dev, "regulator get failed vdd_ana-supply rc=%d\n", rc);
	}
	pdata->vdd_i2c = regulator_get(dev, "inven,vcc_i2c");
	if (IS_ERR(pdata->vdd_i2c)) {
		rc = PTR_ERR(pdata->vdd_i2c);
		dev_warn(dev, "regulator get failed vcc-i2c-supply rc=%d\n", rc);
	}
	pdata->power_on = inv_mpu_power_on;
	pdata->power_off = inv_mpu_power_off;
	dev_dbg(dev, "parse dt complete\n");

	return 0;
}
EXPORT_SYMBOL_GPL(invensense_mpu_parse_dt);

#endif /* CONFIG_OF */
