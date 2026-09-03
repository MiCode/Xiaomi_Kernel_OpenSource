// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2020 Spreadtrum Communications Inc.

#include <linux/clk.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/thermal.h>

#define SPRD_THM_CTL			0x0
#define SPRD_THM_INT_EN			0x4
#define SPRD_THM_INT_STS		0x8
#define SPRD_THM_INT_RAW_STS		0xc
#define SPRD_THM_DET_PERIOD		0x10
#define SPRD_THM_INT_CLR		0x14
#define SPRD_THM_INT_CLR_ST		0x18
#define SPRD_THM_MON_PERIOD		0x4c
#define SPRD_THM_MON_CTL		0x50
#define SPRD_THM_INTERNAL_STS1		0x54
#define SPRD_THM_RAW_READ_MSK		0x3ff

#define SPRD_THM_OFFSET(id)		((id) * 0x4)
#define SPRD_THM_TEMP(id)		(SPRD_THM_OFFSET(id) + 0x5c)
#define SPRD_THM_THRES(id)		(SPRD_THM_OFFSET(id) + 0x2c)

#define SPRD_THM_SEN(id)		BIT((id) + 2)
#define SPRD_THM_SEN_OVERHEAT_EN(id)	BIT((id) + 8)
#define SPRD_THM_SEN_OVERHEAT_ALARM_EN(id)	BIT((id) + 0)

/* bits definitions for register THM_CTL */
#define SPRD_THM_SET_RDY_ST		BIT(13)
#define SPRD_THM_SET_RDY		BIT(12)
#define SPRD_THM_MON_EN			BIT(1)
#define SPRD_THM_EN			BIT(0)

/* bits definitions for register THM_INT_CTL */
#define SPRD_THM_BIT_INT_EN		BIT(26)
#define SPRD_THM_OVERHEAT_EN		BIT(25)
#define SPRD_THM_OTP_TRIP_SHIFT		10

/* bits definitions for register SPRD_THM_INTERNAL_STS1 */
#define SPRD_THM_TEMPER_RDY		BIT(0)

#define SPRD_THM_DET_PERIOD_DATA	0x800
#define SPRD_THM_DET_PERIOD_MASK	GENMASK(19, 0)
#define SPRD_THM_MON_MODE		0x7
#define SPRD_THM_MON_MODE_MASK		GENMASK(3, 0)
#define SPRD_THM_MON_PERIOD_DATA	0x10
#define SPRD_THM_MON_PERIOD_MASK	GENMASK(15, 0)
#define SPRD_THM_THRES_MASK		GENMASK(19, 0)
#define SPRD_THM_INT_CLR_MASK		GENMASK(24, 0)

/* thermal sensor calibration parameters */
#define SPRD_THM_TEMP_LOW		-40000
#define SPRD_THM_TEMP_HIGH		120000
#define SPRD_THM_OTP_TEMP		120000
#define SPRD_THM_HOT_TEMP		75000
#define SPRD_THM_RAW_DATA_HIGH		1023
#define SPRD_THM_SEN_NUM		8
#define SPRD_THM_DT_OFFSET		24
#define SPRD_THM_RATION_OFFSET		17
#define SPRD_THM_RATION_SIGN		16

#define SPRD_THM_RDYST_POLLING_TIME	10
#define SPRD_THM_RDYST_TIMEOUT		700
#define SPRD_THM_TEMP_READY_POLL_TIME	10000
#define SPRD_THM_TEMP_READY_TIMEOUT	800000
#define SPRD_THM_MAX_SENSOR		8
#define SPRD_THM_VDD_NUM		5

struct sprd_thermal_sensor {
	struct thermal_zone_device *tzd;
	struct sprd_thermal_data *data;
	struct device *dev;
	struct list_head node;
	struct mutex lock;
	u32 rawdata;
	int cal_slope;
	int cal_offset;
	int id;
	int lasttemp;
	bool ready;
};

struct sprd_thermal_data {
	const struct sprd_thm_variant_data *var_data;
	struct list_head senlist;
	struct clk *clk;
	void __iomem *base;
	struct delayed_work wait_temp_ready_work;
	u32 ratio_off;
	int ratio_sign;
	int nr_sensors;
	int vdd_cnt;
	struct regulator *vdds[SPRD_THM_VDD_NUM];
	atomic_t quit_worker_flag;
};

/*
 * The conversion between ADC and temperature is based on linear relationship,
 * and use idea_k to specify the slope and ideal_b to specify the offset.
 *
 * Since different Spreadtrum SoCs have different ideal_k and ideal_b,
 * we should save ideal_k and ideal_b in the device data structure.
 */
struct sprd_thm_variant_data {
	u32 ideal_k;
	u32 ideal_b;
};

static const struct sprd_thm_variant_data ums512_data = {
	.ideal_k = 262,
	.ideal_b = 66400,
};

static inline void sprd_thm_update_bits(void __iomem *reg, u32 mask, u32 val)
{
	u32 tmp, orig;

	orig = readl(reg);
	tmp = orig & ~mask;
	tmp |= val & mask;
	writel(tmp, reg);
}

static int sprd_thm_cal_read(struct device_node *np, const char *cell_id,
			     u32 *val)
{
	struct nvmem_cell *cell;
	void *buf;
	size_t len = 0;

	cell = of_nvmem_cell_get(np, cell_id);
	if (IS_ERR(cell))
		return PTR_ERR(cell);

	buf = nvmem_cell_read(cell, &len);
	nvmem_cell_put(cell);
	if (IS_ERR(buf))
		return PTR_ERR(buf);

	if (len > sizeof(u32)) {
		kfree(buf);
		return -EINVAL;
	}

	memcpy(val, buf, len);

	kfree(buf);
	return 0;
}

static int sprd_thm_sensor_calibration(struct device_node *np,
				       struct sprd_thermal_data *thm,
				       struct sprd_thermal_sensor *sen)
{
	int ret;
	/*
	 * According to thermal datasheet, the default calibration offset is 64,
	 * and the default ratio is 1000.
	 */
	int dt_offset = 64, ratio = 1000;

	ret = sprd_thm_cal_read(np, "sen_delta_cal", &dt_offset);
	if (ret)
		return ret;

	ratio += thm->ratio_sign * thm->ratio_off;

	/*
	 * According to the ideal slope K and ideal offset B, combined with
	 * calibration value of thermal from efuse, then calibrate the real
	 * slope k and offset b:
	 * k_cal = (k * ratio) / 1000.
	 * b_cal = b + (dt_offset - 64) * 500.
	 */
	sen->cal_slope = (thm->var_data->ideal_k * ratio) / 1000;
	sen->cal_offset = thm->var_data->ideal_b + (dt_offset - 128) * 250;
	pr_info("sen id = %d, cal =%d, offset =%d\n", sen->id, sen->cal_slope, sen->cal_offset);
	return 0;
}

static int sprd_thm_rawdata_to_temp(struct sprd_thermal_sensor *sen,
				    u32 rawdata)
{
	if (rawdata > SPRD_THM_RAW_DATA_HIGH)
		rawdata = SPRD_THM_RAW_DATA_HIGH;

	/*
	 * According to the thermal datasheet, the formula of converting
	 * adc value to the temperature value should be:
	 * T_final = k_cal * x - b_cal.
	 */
	return sen->cal_slope * rawdata - sen->cal_offset;
}

static int sprd_thm_temp_to_rawdata(int temp, struct sprd_thermal_sensor *sen)
{
	u32 val;

	clamp(temp, (int)SPRD_THM_TEMP_LOW, (int)SPRD_THM_TEMP_HIGH);

	/*
	 * According to the thermal datasheet, the formula of converting
	 * adc value to the temperature value should be:
	 * T_final = k_cal * x - b_cal.
	 */
	val = (temp + sen->cal_offset) / sen->cal_slope;

	return clamp(val, val, (u32)(SPRD_THM_RAW_DATA_HIGH - 1));
}

static int sprd_thm_read_temp(void *devdata, int *temp)
{
	struct sprd_thermal_sensor *sen = devdata;
	int data = 0;

	mutex_lock(&sen->lock);
	if (sen->ready) {
		sen->rawdata = readl(sen->data->base+SPRD_THM_TEMP(sen->id))&SPRD_THM_RAW_READ_MSK;
		data = sprd_thm_rawdata_to_temp(sen, sen->rawdata);
		sen->lasttemp = data;
		*temp = data;
	} else {
		*temp = sen->lasttemp;
	}
	mutex_unlock(&sen->lock);
	return 0;
}

static const struct thermal_zone_of_device_ops sprd_thm_ops = {
	.get_temp = sprd_thm_read_temp,
};

static int sprd_thm_get_vdds(struct platform_device *pdev, struct sprd_thermal_data *thm)
{
	int ret = 0, i = 0;
	char vdd_name[6];

	for (; i < thm->vdd_cnt; i++) {
		snprintf(vdd_name, sizeof(vdd_name), "vdd%d", i);
		thm->vdds[i] = devm_regulator_get(&pdev->dev, vdd_name);
		if (IS_ERR(thm->vdds[i])) {
			thm->vdds[i] = NULL;
			dev_err(&pdev->dev, "get vdd%d fail", i);
			ret = -EPROBE_DEFER;
			return ret;
		}
	}
	return ret;
}

static void sprd_thm_vdd_restore(struct device *dev, struct sprd_thermal_data *thm,
				 bool bind, int idx)
{
	int ret = 0;

	for (--idx; idx >= 0; idx--) {
		if (bind && regulator_disable(thm->vdds[idx]))
			ret = 1;
		else if (!bind && regulator_enable(thm->vdds[idx]))
			ret = 1;
	}
	if (ret)
		dev_err(dev, "thm vdd restore faile\n");
}

static void sprd_thm_vdd_bind(struct device *dev, struct sprd_thermal_data *thm)
{
	int ret = 0, i = 0;

	for (; i < thm->vdd_cnt; i++) {
		ret = regulator_disable(thm->vdds[i]);
		if (ret) {
			dev_err(dev, "vdd%d bind fail\n", i);
			sprd_thm_vdd_restore(dev, thm, false, i);
			return;
		}
	}
}

static void sprd_thm_vdd_unbind(struct device *dev, struct sprd_thermal_data *thm)
{
	int ret = 0, i = 0;

	for (; i < thm->vdd_cnt; i++) {
		ret = regulator_enable(thm->vdds[i]);
		if (ret) {
			dev_err(dev, "vdd%d unbind fail\n", i);
			sprd_thm_vdd_restore(dev, thm, true, i);
			return;
		}
	}
}

static int sprd_thm_poll_ready_status(struct sprd_thermal_data *thm)
{
	u32 val;
	int ret;

	/*
	 * Judge the state of SET_RDY_ST, When SET_RDY_ST is 0, SET_RDY can set to 1
	 */
	ret = readl_poll_timeout(thm->base + SPRD_THM_CTL, val,
				 !(val & SPRD_THM_SET_RDY_ST),
				 SPRD_THM_RDYST_POLLING_TIME,
				 SPRD_THM_RDYST_TIMEOUT);
	if (ret)
		return ret;

	sprd_thm_update_bits(thm->base + SPRD_THM_CTL, SPRD_THM_MON_EN, SPRD_THM_MON_EN);
	sprd_thm_update_bits(thm->base + SPRD_THM_CTL, SPRD_THM_SET_RDY, SPRD_THM_SET_RDY);
	return 0;
}

static void sprd_thm_wait_temp_ready_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sprd_thermal_data *thm = container_of(dwork, struct sprd_thermal_data,
						     wait_temp_ready_work);
	u32 val = 0;
	int ret = 0;
	struct sprd_thermal_sensor *sen, *temp;

	/* Wait for first temperature data ready before reading temperature */
	ret = readl_poll_timeout(thm->base + SPRD_THM_INTERNAL_STS1, val,
				 (val & SPRD_THM_TEMPER_RDY) | atomic_read(&thm->quit_worker_flag),
				 SPRD_THM_TEMP_READY_POLL_TIME,
				 SPRD_THM_TEMP_READY_TIMEOUT);
	if (ret == 0) {
		if (atomic_read(&thm->quit_worker_flag))
			return;
		list_for_each_entry_safe(sen, temp, &thm->senlist, node)
			sen->ready = true;
	} else {
		pr_err("temp ready maybe time out 800ms\n");
		list_for_each_entry_safe(sen, temp, &thm->senlist, node)
			sen->ready = false;
	}
}

static int sprd_thm_set_ready(struct sprd_thermal_data *thm)
{
	int ret;

	ret = sprd_thm_poll_ready_status(thm);
	if (ret)
		return ret;

	/*
	 * Clear interrupt status, enable thermal interrupt and enable thermal.
	 *
	 * The SPRD thermal controller integrates a hardware interrupt signal,
	 * which means if the temperature is overheat, it will generate an
	 * interrupt and notify the event to PMIC automatically to shutdown the
	 * system. So here we should enable the interrupt bits, though we have
	 * not registered an irq handler.
	 */
	writel(SPRD_THM_INT_CLR_MASK, thm->base + SPRD_THM_INT_CLR);
	sprd_thm_update_bits(thm->base + SPRD_THM_INT_EN,
			     SPRD_THM_BIT_INT_EN, SPRD_THM_BIT_INT_EN);
	sprd_thm_update_bits(thm->base + SPRD_THM_CTL, SPRD_THM_EN, SPRD_THM_EN);
	return 0;
}

static void sprd_thm_sensor_init(struct sprd_thermal_data *thm,
				 struct sprd_thermal_sensor *sen)
{
	u32 otp_rawdata, hot_rawdata;

	otp_rawdata = sprd_thm_temp_to_rawdata(SPRD_THM_OTP_TEMP, sen);
	hot_rawdata = sprd_thm_temp_to_rawdata(SPRD_THM_HOT_TEMP, sen);

	/* Enable the sensor' overheat temperature protection interrupt */
	sprd_thm_update_bits(thm->base + SPRD_THM_INT_EN, SPRD_THM_SEN_OVERHEAT_ALARM_EN(sen->id),
			     SPRD_THM_SEN_OVERHEAT_ALARM_EN(sen->id));

	/* Set the sensor' overheat and hot threshold temperature */
	sprd_thm_update_bits(thm->base + SPRD_THM_THRES(sen->id), SPRD_THM_THRES_MASK,
			     (otp_rawdata << SPRD_THM_OTP_TRIP_SHIFT) | hot_rawdata);

	/* Enable the corresponding sensor */
	sprd_thm_update_bits(thm->base + SPRD_THM_CTL, SPRD_THM_SEN(sen->id),
			     SPRD_THM_SEN(sen->id));
}

static void sprd_thm_para_config(struct sprd_thermal_data *thm)
{
	/* Set the period of two valid temperature detection action */
	sprd_thm_update_bits(thm->base + SPRD_THM_DET_PERIOD,
			     SPRD_THM_DET_PERIOD_MASK, SPRD_THM_DET_PERIOD);

	/* Set the sensors' monitor mode */
	sprd_thm_update_bits(thm->base + SPRD_THM_MON_CTL,
			     SPRD_THM_MON_MODE_MASK, SPRD_THM_MON_MODE);

	/* Set the sensors' monitor period */
	sprd_thm_update_bits(thm->base + SPRD_THM_MON_PERIOD,
			     SPRD_THM_MON_PERIOD_MASK, SPRD_THM_MON_PERIOD);
}

static int sprd_thm_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	struct device_node *sen_child;
	struct sprd_thermal_data *thm;
	struct sprd_thermal_sensor *sen, *tmp;
	const struct sprd_thm_variant_data *pdata;
	int ret = 0;
	u32 val;

	pdata = of_device_get_match_data(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "No matching driver data found\n");
		return -EINVAL;
	}

	thm = devm_kzalloc(&pdev->dev, sizeof(*thm), GFP_KERNEL);
	if (!thm)
		return -ENOMEM;

	thm->var_data = pdata;
	thm->base = devm_platform_ioremap_resource(pdev, 0);
	if (IS_ERR(thm->base))
		return PTR_ERR(thm->base);

	thm->nr_sensors = of_get_child_count(np);
	if (thm->nr_sensors == 0 || thm->nr_sensors > SPRD_THM_MAX_SENSOR) {
		dev_err(&pdev->dev, "incorrect sensor count\n");
		return -EINVAL;
	}

	thm->clk = devm_clk_get(&pdev->dev, "enable");
	if (IS_ERR(thm->clk)) {
		dev_err(&pdev->dev, "failed to get enable clock\n");
		return PTR_ERR(thm->clk);
	}

	ret = clk_prepare_enable(thm->clk);
	if (ret)
		return ret;

	if (of_property_read_u32(np, "vdd_cnt", &thm->vdd_cnt)) {
		dev_info(&pdev->dev, "get vdd cnt fail, check it is configured or not\n");
		thm->vdd_cnt = 0;
	}

	if (thm->vdd_cnt > 0) {
		ret = sprd_thm_get_vdds(pdev, thm);
		if (ret)
			return -EPROBE_DEFER;
		sprd_thm_vdd_unbind(&pdev->dev, thm);
	}

	INIT_LIST_HEAD(&thm->senlist);
	sprd_thm_para_config(thm);

	ret = sprd_thm_cal_read(np, "thm_sign_cal", &val);
	if (ret)
		goto disable_clk;

	if (val > 0)
		thm->ratio_sign = -1;
	else
		thm->ratio_sign = 1;

	ret = sprd_thm_cal_read(np, "thm_ratio_cal", &thm->ratio_off);
	if (ret)
		goto disable_clk;

	for_each_child_of_node(np, sen_child) {
		sen = devm_kzalloc(&pdev->dev, sizeof(*sen), GFP_KERNEL);
		if (!sen) {
			ret = -ENOMEM;
			goto of_put;
		}

		sen->ready = false;
		sen->data = thm;
		sen->dev = &pdev->dev;
		sen->lasttemp = 25000;


		ret = of_property_read_u32(sen_child, "reg", &sen->id);
		if (ret) {
			dev_err(&pdev->dev, "get sensor reg failed");
			goto of_put;
		}

		ret = sprd_thm_sensor_calibration(sen_child, thm, sen);
		if (ret) {
			dev_err(&pdev->dev, "efuse cal analysis failed");
			goto of_put;
		}

		sprd_thm_sensor_init(thm, sen);
		mutex_init(&sen->lock);

		sen->tzd = devm_thermal_zone_of_sensor_register(sen->dev, sen->id,
								sen, &sprd_thm_ops);
		if (IS_ERR(sen->tzd)) {
			dev_err(&pdev->dev, "register thermal zone failed %d\n", sen->id);
			ret = PTR_ERR(sen->tzd);
			mutex_destroy(&sen->lock);
			goto of_put;
		}

		list_add_tail(&sen->node, &thm->senlist);
	}
	platform_set_drvdata(pdev, thm);

	/* sen_child set to NULL at this point */
	ret = sprd_thm_set_ready(thm);
	if (ret)
		goto of_put;

	INIT_DELAYED_WORK(&thm->wait_temp_ready_work, sprd_thm_wait_temp_ready_work);
	schedule_delayed_work(&thm->wait_temp_ready_work, 0);

	return 0;

of_put:
	of_node_put(sen_child);
	list_for_each_entry_safe(sen, tmp, &thm->senlist, node)
		mutex_destroy(&sen->lock);
disable_clk:
	clk_disable_unprepare(thm->clk);
	return ret;
}

#ifdef CONFIG_PM_SLEEP
static int sprd_thm_suspend(struct device *dev)
{
	struct sprd_thermal_data *thm = dev_get_drvdata(dev);
	struct sprd_thermal_sensor *sen, *temp;

	atomic_set(&thm->quit_worker_flag, 1);
	cancel_delayed_work_sync(&thm->wait_temp_ready_work);

	list_for_each_entry_safe(sen, temp, &thm->senlist, node) {
		mutex_lock(&sen->lock);
		sen->ready = false;
		sprd_thm_update_bits(thm->base + SPRD_THM_CTL, SPRD_THM_SEN(sen->id), 0);
		mutex_unlock(&sen->lock);
	}

	/* Bug 1550142 Deepsleep AVDD1V8 power consumption is too high */
	writel(0x00, thm->base + SPRD_THM_CTL);
	if (thm->vdd_cnt > 0)
		sprd_thm_vdd_bind(dev, thm);

	return 0;
}

static int sprd_thm_hw_resume(struct sprd_thermal_data *thm)
{
	struct sprd_thermal_sensor *sen, *temp;
	int ret = 0;

	list_for_each_entry_safe(sen, temp, &thm->senlist, node) {
		sprd_thm_update_bits(thm->base + SPRD_THM_CTL,
				     SPRD_THM_SEN(sen->id),
				     SPRD_THM_SEN(sen->id));
	}

	ret = sprd_thm_poll_ready_status(thm);
	if (ret)
		return ret;

	writel(SPRD_THM_INT_CLR_MASK, thm->base + SPRD_THM_INT_CLR);
	sprd_thm_update_bits(thm->base + SPRD_THM_CTL, SPRD_THM_EN, SPRD_THM_EN);
	return 0;
}

static int sprd_thm_resume(struct device *dev)
{
	struct sprd_thermal_data *thm = dev_get_drvdata(dev);
	int ret = 0;

	if (thm->vdd_cnt > 0)
		sprd_thm_vdd_unbind(dev, thm);

	ret = sprd_thm_hw_resume(thm);
	if (ret)
		return ret;
	atomic_set(&thm->quit_worker_flag, 0);
	schedule_delayed_work(&thm->wait_temp_ready_work, 0);
	return 0;
}
#endif

static int sprd_thm_remove(struct platform_device *pdev)
{
	struct sprd_thermal_data *thm = platform_get_drvdata(pdev);
	struct sprd_thermal_sensor *sen, *tmp;

	atomic_set(&thm->quit_worker_flag, 1);
	cancel_delayed_work_sync(&thm->wait_temp_ready_work);

	list_for_each_entry_safe(sen, tmp, &thm->senlist, node) {
		devm_thermal_zone_of_sensor_unregister(&pdev->dev, sen->tzd);
		mutex_destroy(&sen->lock);
	}

	clk_disable_unprepare(thm->clk);
	return 0;
}

static const struct of_device_id sprd_thermal_of_match[] = {
	{ .compatible = "sprd,ums512-thermal", .data = &ums512_data },
	{ },
};
MODULE_DEVICE_TABLE(of, sprd_thermal_of_match);

static const struct dev_pm_ops sprd_thermal_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sprd_thm_suspend, sprd_thm_resume)
};

static struct platform_driver sprd_thermal_driver = {
	.probe = sprd_thm_probe,
	.remove = sprd_thm_remove,
	.driver = {
		.name = "sprd-thermal",
		.pm = &sprd_thermal_pm_ops,
		.of_match_table = sprd_thermal_of_match,
	},
};

module_platform_driver(sprd_thermal_driver);

MODULE_AUTHOR("Freeman Liu <freeman.liu@unisoc.com>");
MODULE_DESCRIPTION("Spreadtrum thermal driver");
MODULE_LICENSE("GPL v2");
