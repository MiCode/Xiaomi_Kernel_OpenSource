// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Spreadtrum Communications Inc.

#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/thermal.h>
#include <linux/of.h>
#include <linux/kernel.h>

#define THM_NAME_LENGTH		20
#define PERIOD			5000
#define DEF_THMZONE		"board-thmzone"
#define ABNORMAL_TEMP		120000
#define INIT_TEMP		25000
#define RD_TEMP_IDX		3
#define THM_ERR			-1
#define DEFAULT_TEMP_DIFF	250
#define DEFAULT_VIRT_TEMP_DIFF	200

struct shell_sensor {
	u16 sensor_id;
	int cur_temp;
	int last_temp;
	int virt_temp_diff;
	int init_flag;
	size_t nsensor;
	size_t ntemp;
	int index;
	int const_temp;
	int **coeff;
	int **hty_temp;
	int *ntc_temp_diff;
	struct sprd_thermal_zone *pzone;
	const char **sensor_names;
	struct thermal_zone_device **thm_zones;
	struct delayed_work read_temp_work;
};

struct sprd_thermal_zone {
	struct thermal_zone_device *therm_dev;
	struct device *dev;
	const struct thermal_zone_of_device_ops *ops;
	char name[THM_NAME_LENGTH];
	int id;
};

static void sprd_get_virt_temp(struct shell_sensor *psensor, int *temp, int index)
{
	int i = 0;
	int j = 0;
	int coeff_index = 0;
	int diff_temp = 0;
	int sum_temp = 0;

	for (i = 0; i < psensor->nsensor; i++) {
		for (j = 0; j < psensor->ntemp; j++) {
			if (index + 1 + j < psensor->ntemp)
				coeff_index = index + 1 + j;
			else
				coeff_index = index + 1 + j - psensor->ntemp;
			sum_temp += psensor->coeff[i][j] * psensor->hty_temp[i][coeff_index];
		}
	}
	sum_temp = sum_temp/10000 + psensor->const_temp;
	*temp = sum_temp;
	if (psensor->last_temp != ABNORMAL_TEMP) {
		diff_temp = sum_temp - psensor->last_temp;
		if ((psensor->virt_temp_diff > 0) && (abs(diff_temp) > psensor->virt_temp_diff)) {
			*temp = diff_temp > 0 ? psensor->last_temp + psensor->virt_temp_diff :
						psensor->last_temp - psensor->virt_temp_diff;
		}
	}
	psensor->last_temp = *temp;
}

static int sprd_get_temp(struct shell_sensor *psensor, int *temp)
{
	int i = 0, k = 0, ret = 0;
	int diff_temp = 0, old_temp = 0, tmp_temp = 0;
	int old_index = 0, index = psensor->index;
	struct thermal_zone_device *tz = NULL;

	for (; i < psensor->nsensor; i++) {
		tz = psensor->thm_zones[i];
		if (!tz || IS_ERR(tz) || !tz->ops->get_temp) {
			pr_err("get thermal zone failed %d\n", i);
			return -ENODEV;
		}

		do {
			ret = tz->ops->get_temp(tz, &(psensor->hty_temp[i][index]));
			if (!ret && (psensor->hty_temp[i][index] < ABNORMAL_TEMP))
				break;
			k++;
		} while (k < RD_TEMP_IDX);

		if (k == RD_TEMP_IDX) {
			pr_err("get thermal %s temp failed\n", tz->type);
			goto out1;
		}
		if (!psensor->init_flag) {
			old_index = index - 1 < 0 ? psensor->ntemp - 1 : index - 1;
			old_temp = psensor->hty_temp[i][old_index];
			diff_temp = psensor->hty_temp[i][index] - old_temp;
			if (psensor->ntc_temp_diff &&
			    (abs(diff_temp) > psensor->ntc_temp_diff[i])) {
				tmp_temp = diff_temp > 0 ? old_temp + psensor->ntc_temp_diff[i] :
							   old_temp - psensor->ntc_temp_diff[i];
				psensor->hty_temp[i][index] = tmp_temp;
			}
		}
	}

	if (psensor->init_flag)
		goto out1;

	sprd_get_virt_temp(psensor, temp, index);
	goto out2;
out1:
	tz = thermal_zone_get_zone_by_name(DEF_THMZONE);
	ret = tz->ops->get_temp(tz, temp);
	if (ret || (*temp > ABNORMAL_TEMP)) {
		pr_err("get %s temp fail\n", tz->type);
		*temp = ABNORMAL_TEMP;
		return ret;
	}

out2:
	psensor->index++;
	if (index == psensor->ntemp - 1) {
		psensor->index = 0;
		psensor->init_flag = 0;
	}
	return 0;
}

static int sprd_temp_sensor_read(void *devdata, int *temp)
{
	int ret = -EINVAL;
	struct sprd_thermal_zone *pzone = devdata;
	struct shell_sensor *psensor = NULL;

	psensor = (struct shell_sensor *)dev_get_drvdata(pzone->dev);

	if (!psensor || !pzone || !temp)
		return ret;
	*temp = psensor->cur_temp;
	pr_debug("shell_sensor_id:%d, temp:%d\n", pzone->id, *temp);

	return 0;
}

static void sensor_read_temp_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct shell_sensor *psensor = container_of(dwork, struct shell_sensor, read_temp_work);
	int ret;

	ret = sprd_get_temp(psensor, &psensor->cur_temp);
	if (ret)
		pr_debug("shell_sensor: %s; temp: %d", psensor->pzone->name, psensor->cur_temp);

	mod_delayed_work(system_freezable_power_efficient_wq,
			 &psensor->read_temp_work, msecs_to_jiffies(PERIOD));
}

const struct thermal_zone_of_device_ops sprd_shell_thm_ops = {
	.get_temp = sprd_temp_sensor_read,
};

static void sprd_htytemp_init(struct shell_sensor *psensor)
{
	int i = 0, j = 0;

	for (; i < psensor->nsensor; i++) {
		for (; j < psensor->ntemp; j++)
			psensor->hty_temp[i][j] = INIT_TEMP;
	}
}

static void sprd_get_temp_diff(struct device *dev, struct device_node *np,
			       struct shell_sensor *psensor)
{
	int i = 0;
	int ret = 0;
	int count = 0;

	psensor->ntc_temp_diff = devm_kmalloc_array(dev, psensor->nsensor, sizeof(int), GFP_KERNEL);
	if (!psensor->ntc_temp_diff)
		return;

	ret = of_property_read_u32(np, "virt-temp-diff", &(psensor->virt_temp_diff));
	if (ret) {
		dev_err(dev, "fail to get virt temp diff\n");
		psensor->virt_temp_diff = DEFAULT_VIRT_TEMP_DIFF;
	}

	count = (size_t)of_property_count_elems_of_size(np, "temp-diff", sizeof(u32));
	if (psensor->nsensor != count)
		goto out;

	ret = of_property_read_u32_array(np, "temp-diff", psensor->ntc_temp_diff, count);
	if (!ret)
		return;
out:
	dev_err(dev, "fail to get temp diff\n");
	for (i = 0; i < psensor->nsensor; i++)
		psensor->ntc_temp_diff[i] = DEFAULT_TEMP_DIFF;
}

static int sprd_temp_sen_parse_dt(struct device *dev, struct shell_sensor *psensor)
{
	int i, j, ret, offset = 0;
	int k = 0;
	size_t count;
	u32 temp_coeff[80];
	int **tmp_coeff;
	int **tmp_hty_temp;
	struct device_node *np = dev->of_node;

	if (!np) {
		dev_err(dev, "device node not found\n");
		return -EINVAL;
	}

	count = (size_t)of_property_count_strings(np, "sensor-names");
	if (count <= 0) {
		dev_err(dev, "sensor names not found\n");
		return count;
	}

	psensor->thm_zones = devm_kmalloc_array(dev, count, sizeof(struct thermal_zone_device *),
						GFP_KERNEL);
	if (!psensor->thm_zones)
		return -ENOMEM;

	psensor->sensor_names = devm_kmalloc_array(dev, count, sizeof(char *), GFP_KERNEL);
	if (!psensor->sensor_names)
		return -ENOMEM;

	psensor->nsensor = count;
	for (i = 0; i < count; i++) {
		ret = of_property_read_string_index(np, "sensor-names", i,
						  &psensor->sensor_names[i]);
		if (ret) {
			dev_err(dev, "fail to get  sensor-names\n");
			return ret;
		}
	}

	count = (size_t)of_property_count_elems_of_size(np, "temp-coeff", sizeof(u32));
	if (count <= 0) {
		dev_err(dev, "temp coeff not found\n");
		return count;
	}

	ret = of_property_read_u32_array(np, "temp-coeff", temp_coeff, count);
	if (ret) {
		dev_err(dev, "fail to get temp-coeff\n");
		return -EINVAL;
	}

	psensor->ntemp = count/psensor->nsensor;

	tmp_coeff = devm_kmalloc_array(dev, psensor->nsensor, sizeof(int *), GFP_KERNEL);
	if (!tmp_coeff)
		return -ENOMEM;

	tmp_hty_temp = devm_kmalloc_array(dev, psensor->nsensor, sizeof(int *), GFP_KERNEL);
	if (!tmp_hty_temp)
		return -ENOMEM;

	while (k < psensor->nsensor) {
		tmp_coeff[k] = devm_kmalloc_array(dev, psensor->ntemp, sizeof(int), GFP_KERNEL);
		tmp_hty_temp[k] = devm_kmalloc_array(dev, psensor->ntemp, sizeof(int), GFP_KERNEL);
		if (!tmp_coeff[k] || !tmp_hty_temp[k])
			return -ENOMEM;
		k++;
	}
	psensor->coeff = tmp_coeff;
	psensor->hty_temp = tmp_hty_temp;
	sprd_htytemp_init(psensor);
	ret = of_property_read_u32(np, "coeff-offset", &offset);
	dev_info(dev, "coeff-offset: %d, ntemp=%ld\n", offset, psensor->ntemp);
	if (ret) {
		dev_err(dev, "fail to get coeff-offset\n");
		return -EINVAL;
	}
	for (i = 0; i < psensor->nsensor; i++) {
		for (j = 0; j < psensor->ntemp; j++)
			psensor->coeff[i][j] = (int)temp_coeff[i*psensor->ntemp+j] - offset;
	}
	ret = of_property_read_u32(np, "temp-const", &psensor->const_temp);
	if (ret) {
		dev_err(dev, "fail to get temp-const\n");
		return -EINVAL;
	}
	ret = of_property_read_u32(np, "const-offset", &offset);
	if (ret) {
		dev_err(dev, "fail to get const-offset\n");
		return -EINVAL;
	}
	psensor->const_temp -= offset;
	sprd_get_temp_diff(dev, np, psensor);

	return ret;
}

static int sprd_shell_thm_resume(struct platform_device *pdev)
{
	struct shell_sensor *psensor = platform_get_drvdata(pdev);

	psensor->index = 0;
	psensor->init_flag = 1;

	return 0;
}

static int sprd_shell_thm_suspend(struct platform_device *pdev, pm_message_t
				    state)
{
	return 0;
}

int sprd_thm_init(struct sprd_thermal_zone *pzone)
{
	pzone->therm_dev = thermal_zone_of_sensor_register(pzone->dev, pzone->id,
							   pzone, &sprd_shell_thm_ops);

	if (IS_ERR_OR_NULL(pzone->therm_dev)) {
		pr_err("Register thermal zone device failed.\n");
		return PTR_ERR(pzone->therm_dev);
	}

	return 0;
}

static int sprd_shell_thm_probe(struct platform_device *pdev)
{
	int i = 0;
	int ret = 0, sensor_id = 0;
	struct sprd_thermal_zone *pzone = NULL;
	struct shell_sensor *psensor = NULL;
	struct device_node *np = pdev->dev.of_node;

	if (!np) {
		dev_err(&pdev->dev, "device node not found\n");
		return -EINVAL;
	}

	psensor = devm_kzalloc(&pdev->dev, sizeof(*psensor), GFP_KERNEL);
	if (!psensor)
		return -ENOMEM;

	psensor->index = 0;
	psensor->init_flag = 1;
	psensor->cur_temp = 0;
	psensor->last_temp = ABNORMAL_TEMP;
	psensor->virt_temp_diff = -1;
	ret = sprd_temp_sen_parse_dt(&pdev->dev, psensor);
	if (ret) {
		dev_err(&pdev->dev, "not found ptrips\n");
		return -EINVAL;
	}

	INIT_DELAYED_WORK(&psensor->read_temp_work, sensor_read_temp_work);
	for (i = 0; i < psensor->nsensor; i++) {
		psensor->thm_zones[i] = thermal_zone_get_zone_by_name(psensor->sensor_names[i]);
		if (IS_ERR(psensor->thm_zones[i])) {
			pr_err("get thermal zone %s failed\n", psensor->sensor_names[i]);
			return -EPROBE_DEFER;
		}
	}

	pzone = devm_kzalloc(&pdev->dev, sizeof(*pzone), GFP_KERNEL);
	if (!pzone)
		return -ENOMEM;

	pzone->dev = &pdev->dev;
	pzone->id = sensor_id;
	pzone->ops = &sprd_shell_thm_ops;
	strscpy(pzone->name, np->name, sizeof(pzone->name));

	ret = sprd_thm_init(pzone);
	if (ret < 0) {
		dev_err(&pdev->dev,
			"virtual sensor sw init error id =%d\n", pzone->id);
		return ret;
	}

	psensor->pzone = pzone;
	platform_set_drvdata(pdev, psensor);
	mod_delayed_work(system_freezable_power_efficient_wq,
			 &psensor->read_temp_work, msecs_to_jiffies(PERIOD));
	dev_info(&pdev->dev, "sprd_shell_thermal probe success\n");
	return 0;
}

static int sprd_shell_thm_remove(struct platform_device *pdev)
{
	struct shell_sensor *psensor = platform_get_drvdata(pdev);
	struct sprd_thermal_zone *pzone = psensor->pzone;

	cancel_delayed_work_sync(&psensor->read_temp_work);
	thermal_zone_device_unregister(pzone->therm_dev);
	return 0;
}

static const struct of_device_id shell_thermal_of_match[] = {
	{ .compatible = "sprd,shell-thermal" },
	{},
};
MODULE_DEVICE_TABLE(of, shell_thermal_of_match);

static struct platform_driver sprd_shell_thermal_driver = {
	.probe = sprd_shell_thm_probe,
	.suspend = sprd_shell_thm_suspend,
	.resume = sprd_shell_thm_resume,
	.remove = sprd_shell_thm_remove,
	.driver = {
		   .owner = THIS_MODULE,
		   .name = "shell-thermal",
		   .of_match_table = of_match_ptr(shell_thermal_of_match),
		   },
};

static int __init sprd_shell_thermal_init(void)
{
	return platform_driver_register(&sprd_shell_thermal_driver);
}

static void __exit sprd_shell_thermal_exit(void)
{
	platform_driver_unregister(&sprd_shell_thermal_driver);
}

device_initcall_sync(sprd_shell_thermal_init);
module_exit(sprd_shell_thermal_exit);

MODULE_DESCRIPTION("sprd thermal driver");
MODULE_LICENSE("GPL");
