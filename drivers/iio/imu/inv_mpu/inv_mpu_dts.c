#include <linux/err.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>

#include <linux/i2c.h>
#include <linux/mpu.h>
#include "inv_mpu_dts.h"

int inv_mpu_power_on(struct mpu_platform_data *pdata)
{
	int err = 0;

	if (pdata->vdd_ana) {
		err = regulator_enable(pdata->vdd_ana);
		if (err < 0)
			return err;
	}
	if (pdata->vdd_i2c) {
		err = regulator_enable(pdata->vdd_i2c);
		if (err < 0)
			return err;
	}
	pr_debug(KERN_INFO "inv_mpu_power_on call");

	return err ;
}

int inv_mpu_power_off(struct mpu_platform_data *pdata)
{
	int err = 0;

	if (pdata->vdd_ana) {
		err = regulator_disable(pdata->vdd_ana);
		if (err < 0)
			return err;
	}
	if (pdata->vdd_i2c) {
		err = regulator_disable(pdata->vdd_i2c);
		if (err < 0)
			return err;
	}

	pr_debug(KERN_INFO "inv_mpu_power_off call");

	return err;
}

int inv_parse_orientation_matrix(struct device *dev, s8 *orient)
{
	int rc, i;
	struct device_node *np = dev->of_node;
	u32 temp_val, temp_val2;

	for (i = 0; i < 9; i++)
		orient[i] = 0;

	/* parsing axis x orientation matrix*/
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

	/* parsing axis y orientation matrix*/
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

	/* parsing axis z orientation matrix*/
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

int inv_parse_secondary_orientation_matrix(struct device *dev,
							s8 *orient)
{
	int rc, i;
	struct device_node *np = dev->of_node;
	u32 temp_val, temp_val2;

	for (i = 0; i < 9; i++)
		orient[i] = 0;

	/* parsing axis x orientation matrix*/
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

	/* parsing axis y orientation matrix*/
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

	/* parsing axis z orientation matrix*/
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

int inv_parse_secondary(struct device *dev, struct mpu_platform_data *pdata)
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
	else
		return -EINVAL;
	rc = of_property_read_u32(np, "inven,secondary_reg", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read secondary register\n");
		return rc;
	}
	pdata->secondary_i2c_addr = temp_val;
	rc = inv_parse_secondary_orientation_matrix(dev,
						pdata->secondary_orientation);

	return rc;
}

int inv_parse_aux(struct device *dev, struct mpu_platform_data *pdata)
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

int inv_parse_regulator_config(struct device *dev, struct mpu_platform_data *pdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	u32 temp_val;

	/* parsing regulator config */
	rc = of_property_read_u32(np, "regulator_config", &temp_val);
	if (rc) {
		dev_err(dev, "Unable to read regulator config\n");
		return rc;
	}

	pdata->regulator_config = temp_val;

	return rc;
}

int invensense_mpu_parse_dt(struct device *dev, struct mpu_platform_data *pdata)
{
	int rc;
	pr_debug("Invensense MPU parse_dt started.\n");

	rc = inv_parse_regulator_config(dev, pdata);
	if (rc)
		return rc;

	rc = inv_parse_orientation_matrix(dev, pdata->orientation);
	if (rc)
		return rc;

	rc = inv_parse_secondary(dev, pdata);
	if (rc)
		return rc;

	inv_parse_aux(dev, pdata);

	pdata->vdd_ana = NULL;
	pdata->vdd_i2c = NULL;
	if (pdata->regulator_config & REGULATOR_CONFIG_ANA) {
		pdata->vdd_ana = regulator_get(dev, "inven,vdd_ana");
		if (IS_ERR(pdata->vdd_ana)) {
			rc = PTR_ERR(pdata->vdd_ana);
			dev_err(dev, "Regulator get failed vdd_ana-supply rc=%d\n", rc);
			return rc;
		}
	}

	if (pdata->regulator_config & REGULATOR_CONFIG_I2C) {
		pdata->vdd_i2c = regulator_get(dev, "inven,vcc_i2c");
		if (IS_ERR(pdata->vdd_i2c)) {
			rc = PTR_ERR(pdata->vdd_i2c);
			dev_err(dev, "Regulator get failed vcc-i2c-supply rc=%d\n", rc);
			return rc;
		}
	}

	pdata->power_on = inv_mpu_power_on;
	pdata->power_off = inv_mpu_power_off;
	pr_debug("Invensense MPU parse_dt complete.\n");
	return rc;
}

