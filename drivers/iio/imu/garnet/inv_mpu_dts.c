#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/iio/imu/mpu.h>

#include "inv_mpu_dts.h"

static int inv_parse_orient_matrix(struct device *dev, const char *name,
					s8 *orient)
{
	int ret, i;
	const struct device_node *np = dev->of_node;
	char property_name[24];
	u32 values[9];

	ret = snprintf(property_name, sizeof(property_name),
			"inven,%s-orient", name);
	if (ret >= sizeof(property_name))
		return -EINVAL;

	/* Parsing orientation matrix */
	ret = of_property_read_u32_array(np, property_name, values, 9);
	if (ret) {
		dev_err(dev, "unable to parse orientation matrix [%s]\n",
			property_name);
		return ret;
	}
	for (i = 0; i < 9; ++i) {
		orient[i] = (s8)values[i];
	}

	return 0;
}

int inv_mpu_parse_dt(struct device *dev, struct mpu_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	const s8 ident[] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };
	u32 wake_up_delays[2] = { 0, 0 };
	int rc;

	rc = inv_parse_orient_matrix(dev, "accel", pdata->accel_orient);
	if (rc) {
		dev_warn(dev, "missing accel orient matrix");
		memcpy(pdata->accel_orient, ident, sizeof(pdata->accel_orient));
	}
	rc = inv_parse_orient_matrix(dev, "magn", pdata->magn_orient);
	if (rc) {
		dev_warn(dev, "missing magn orient matrix");
		memcpy(pdata->magn_orient, ident, sizeof(pdata->magn_orient));
	}
	rc = inv_parse_orient_matrix(dev, "gyro", pdata->gyro_orient);
	if (rc) {
		dev_warn(dev, "missing gyro orient matrix");
		memcpy(pdata->gyro_orient, ident, sizeof(pdata->gyro_orient));
	}

	rc = of_get_named_gpio(np, "inven,wake-gpio", 0);
	if (!gpio_is_valid(rc)) {
		dev_warn(dev, "missing wake gpio\n");
	}
	pdata->wake_gpio = rc;

	rc = of_property_read_u32_array(np, "inven,wake-delay-us",
					wake_up_delays, 2);
	if (rc) {
		dev_warn(dev, "missing wake delays\n");
	}
	pdata->wake_delay_min = wake_up_delays[0];
	pdata->wake_delay_max = wake_up_delays[1];

	pdata->vdd_ana = regulator_get(dev, "inven,vdd_ana");
	if (IS_ERR(pdata->vdd_ana)) {
		rc = PTR_ERR(pdata->vdd_ana);
		dev_err(dev, "regulator get failed vdd_ana-supply rc=%d\n", rc);
	}
	pdata->vdd_i2c = regulator_get(dev, "inven,vcc_i2c");
	if (IS_ERR(pdata->vdd_i2c)) {
		rc = PTR_ERR(pdata->vdd_i2c);
		dev_err(dev, "regulator get failed vcc-i2c-supply rc=%d\n", rc);
	}

	return 0;
}
