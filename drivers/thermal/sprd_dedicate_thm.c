// SPDX-License-Identifier: GPL-2.0
// Copyright (C) 2019 Spreadtrum Communications Inc.

#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/module.h>
#include <linux/nvmem-consumer.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/consumer.h>
#include <linux/thermal.h>
#include <linux/slab.h>

#define SPRD_THM_CTL			0x0
#define SPRD_THM_INT_CTL		0x4
#define SPRD_THM_INT_STS		0x8
#define SPRD_THM_INT_RAW_STS		0xc
#define SPRD_THM_DET_PERIOD_L16B	0x10
#define SPRD_THM_DET_PERIOD_H4B		0x14
#define SPRD_THM_INT_CLR		0x18
#define SPRD_THM_OVERHEAT_HOT_THRES	0x24
#define SPRD_THM_HOT2NOR_HIGH_THRES	0x28
#define SPRD_THM_LOW_COLD_THRES		0x2c
#define SPRD_THM_MON_PERIOD		0x30
#define SPRD_THM_MON_CTL		0x34
#define SPRD_THM_LAST_TEMPER0_READ	0x38
#define SPRD_THM_RAW_READ_MSK		GENMASK(7, 0)

/* bits definitions for register THM_CTL */
#define SPRD_THM_SET_RDY_STATUS		BIT(5)
#define SPRD_THM_SET_RDY		BIT(4)
#define SPRD_THM_SOFT_RESET		BIT(3)
#define SPRD_THM_MON_EN			BIT(1)
#define SPRD_THM_EN			BIT(0)

/* bits definitions for register THM_INT_CTL */
#define SPRD_THM_OVERHEAT_EN            BIT(9)
#define SPRD_THM_OVERHEAT_ALARM_EN      BIT(7)
#define SPRD_THM_TRIP_THRESHOLD(val)    ((val) << 8)

#define SPRD_DET_L16B_PERIOD		0x800
#define SPRD_DET_L16B_PERIOD_MASK	GENMASK(15, 0)
#define SPRD_MON_PERIOD			0x40
#define SPRD_MON_PERIOD_MASK		GENMASK(15, 0)
#define SPRD_TRIP_THRESHOLD_MASK	GENMASK(15, 0)
#define SPRD_INT_CLR			GENMASK(8, 0)

/* definitions for register SPRD_THM_MON_CTL */
#define SPRD_THM_MON_MODE_MASK		GENMASK(3, 0)
#define SPRD_THM_MON_MODE_VAL		0x9

#define SPRD_THM_RDYST_POLLING_TIME	10
#define SPRD_THM_RDYST_TIMEOUT		700
#define SPRD_THM_TEMP_READY_POLL_TIME	10000
#define SPRD_THM_TEMP_READY_TIMEOUT	600000

#define SPRD_THM_OTPTEMP		120000
#define SPRD_THM_HOTTEMP		75000
#define SPRD_THM_HOT2NOR_TEMP		65000
#define SPRD_THM_HIGHOFF_TEMP		55000
#define SPRD_THM_LOWOFF_TEMP		45000
#define SPRD_THM_COLD_TEMP		35000

/* thm efuse cal para */
#define SPRD_TEMP_LOW			-40000
#define SPRD_TEMP_HIGH			120000
#define SPRD_RAW_DATA_LOW		0
#define SPRD_RAW_DATA_HIGH		255
#define CAL_OFFSET_MASK			GENMASK(6, 0)

#define SPRD_THM_VDD_NUM		5

/*
 * Since different sprd thermal series can have different
 * ideal_k and ideal_b, we should save ideal_k and ideal_b
 * in the device data structure.
 */
struct sprd_thm_variant_data {
	u32 ideal_k;
	u32 ideal_b;
};

struct sprd_thermal_data {
	struct thermal_zone_device *thmzone_dev;
	struct delayed_work thm_resume_work;
	struct device *dev;
	struct clk *clk;
	struct mutex lock;
	void __iomem *thm_base;
	u32 rawdata;
	u32 otp_rawdata;
	u32 hot_rawdata;
	u32 hot2nor_rawdata;
	u32 highoff_rawdata;
	u32 lowoff_rawdata;
	u32 cold_rawdata;
	u32 ratio_off;
	u32 ratio_sign;
	u32 detal_cal;
	int algor_ver;
	int cal_slope;
	int cal_offset;
	int lasttemp;
	int temp;
	int id;
	bool ready_flag;
	int vdd_cnt;
	struct regulator *vdds[SPRD_THM_VDD_NUM];
	const struct sprd_thm_variant_data *var_data;
};

static inline void sprd_thm_update_bits(void __iomem *reg, u32 mask, u32 val)
{
	u32 tmp, orig;

	orig = readl(reg);
	tmp = orig & ~mask;
	tmp |= val & mask;
	writel(tmp, reg);
}

static const struct sprd_thm_variant_data sharkl3_data = {
	.ideal_k = 903,
	.ideal_b = 71290,
};

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
	if (IS_ERR(buf)) {
		nvmem_cell_put(cell);
		return PTR_ERR(buf);
	}

	memcpy(val, buf, min(len, sizeof(u32)));

	kfree(buf);
	buf = NULL;
	nvmem_cell_put(cell);

	return 0;
}

static int sprd_thm_efuse_cal(struct sprd_thermal_data *thm)
{
	/*
	 * According to thermal datasheet calibration offset
	 * default val is 64, ratio val default is 1000.
	 */
	int ratio = 1000;

	if (thm->ratio_sign == 1)
		ratio = 1000 - thm->ratio_off;
	else
		ratio = 1000 + thm->ratio_off;

	/*
	 * According to the ideal slope K and ideal offset B, combined with
	 * calibration val of thermal from efuse,
	 * then confirm real slope k and offset b.
	 * k_cal =(k * ratio)/1000.
	 * b_cal = b + (dt_offset - 64) * 500.
	 */
	thm->cal_slope = (thm->var_data->ideal_k * ratio) / 1000;
	if (thm->detal_cal)
		thm->cal_offset = thm->var_data->ideal_b +
		    (thm->detal_cal - 64) * 500;
	else
		thm->cal_offset = thm->var_data->ideal_b;

	dev_info(thm->dev, "sen id = %d, cal =%d,offset =%d\n", thm->id,
		 thm->cal_slope, thm->cal_offset);

	return 0;
}

static int sprd_rawdata_to_temp_v1(struct sprd_thermal_data *thm)
{
	if (thm->rawdata < SPRD_RAW_DATA_LOW)
		thm->rawdata = SPRD_RAW_DATA_LOW;
	else if (thm->rawdata > SPRD_RAW_DATA_HIGH)
		thm->rawdata = SPRD_RAW_DATA_HIGH;

	/*
	 * According to thermal datasheet adc value conversion
	 * temperature formula T_final = k_cal * x - b_cal.
	 */
	return thm->cal_slope * thm->rawdata - thm->cal_offset;
}

static int sprd_temp_to_rawdata_v1(int temp, struct sprd_thermal_data *thm)
{
	u32 rawval;

	if (temp < SPRD_TEMP_LOW)
		temp = SPRD_TEMP_LOW;
	else if (temp > SPRD_TEMP_HIGH)
		temp = SPRD_TEMP_HIGH;

	/*
	 * Backstepping temperature convert to adc val
	 * according to the formula T_final = k_cal * x - b_cal.
	 */
	rawval = (temp + thm->cal_offset) / thm->cal_slope;

	return rawval >= SPRD_RAW_DATA_HIGH ? (SPRD_RAW_DATA_HIGH - 1) : rawval;
}

static int sprd_thm_temp_read(void *devdata, int *temp)
{
	struct sprd_thermal_data *thm = devdata;
	int sensor_temp;

	mutex_lock(&thm->lock);
	if (thm->ready_flag) {
		thm->rawdata = readl(thm->thm_base + SPRD_THM_LAST_TEMPER0_READ);
		thm->rawdata = thm->rawdata & SPRD_THM_RAW_READ_MSK;
		sensor_temp = sprd_rawdata_to_temp_v1(thm);
		thm->lasttemp = sensor_temp;
		*temp = sensor_temp;
	} else {
		*temp = thm->lasttemp;
	}
	mutex_unlock(&thm->lock);
	return 0;
}

static int sprd_thm_monitor_start(struct sprd_thermal_data *thm, bool en)
{
	u32 thm_ctl_val;
	int ret;

	/* Wait for thm ready status before config thm parameter */
	ret = readl_poll_timeout(thm->thm_base + SPRD_THM_CTL,
				 thm_ctl_val,
				 !(thm_ctl_val & SPRD_THM_SET_RDY_STATUS),
				 SPRD_THM_RDYST_POLLING_TIME,
				 SPRD_THM_RDYST_TIMEOUT);
	if (ret)
		return ret;

	if (en == true) {
		sprd_thm_update_bits(thm->thm_base + SPRD_THM_CTL,
				     SPRD_THM_MON_EN, SPRD_THM_MON_EN);
	} else {
		sprd_thm_update_bits(thm->thm_base + SPRD_THM_CTL,
				     SPRD_THM_MON_EN, 0);
	}

	sprd_thm_update_bits(thm->thm_base + SPRD_THM_CTL, SPRD_THM_SET_RDY,
			     SPRD_THM_SET_RDY);

	return 0;
}

static int sprd_thm_enable(struct sprd_thermal_data *thm, bool en)
{
	int ret;

	ret = sprd_thm_monitor_start(thm, en);
	if (ret)
		return ret;

	writel(SPRD_INT_CLR, thm->thm_base + SPRD_THM_INT_CLR);
	if (en == true) {
		sprd_thm_update_bits((thm->thm_base + SPRD_THM_INT_CTL),
				     SPRD_THM_OVERHEAT_EN |
				     SPRD_THM_OVERHEAT_ALARM_EN,
				     SPRD_THM_OVERHEAT_EN |
				     SPRD_THM_OVERHEAT_ALARM_EN);
		sprd_thm_update_bits((thm->thm_base + SPRD_THM_CTL),
				     SPRD_THM_EN, SPRD_THM_EN);
	} else {
		sprd_thm_update_bits((thm->thm_base + SPRD_THM_INT_CTL),
				     SPRD_THM_OVERHEAT_EN |
				     SPRD_THM_OVERHEAT_ALARM_EN, 0);
		sprd_thm_update_bits((thm->thm_base + SPRD_THM_CTL),
				     SPRD_THM_EN, 0);
	}

	return 0;
}

static void sprd_thm_trip_config(struct sprd_thermal_data *thm)
{
	thm->otp_rawdata = sprd_temp_to_rawdata_v1(SPRD_THM_OTPTEMP, thm);
	thm->hot_rawdata = sprd_temp_to_rawdata_v1(SPRD_THM_HOTTEMP, thm);
	thm->hot2nor_rawdata =
	    sprd_temp_to_rawdata_v1(SPRD_THM_HOT2NOR_TEMP, thm);
	thm->highoff_rawdata =
	    sprd_temp_to_rawdata_v1(SPRD_THM_HIGHOFF_TEMP, thm);
	thm->lowoff_rawdata =
	    sprd_temp_to_rawdata_v1(SPRD_THM_LOWOFF_TEMP, thm);
	thm->cold_rawdata = sprd_temp_to_rawdata_v1(SPRD_THM_COLD_TEMP, thm);
}

static int sprd_thm_hw_init(struct sprd_thermal_data *thm)
{
	sprd_thm_update_bits((thm->thm_base + SPRD_THM_DET_PERIOD_L16B),
			     SPRD_DET_L16B_PERIOD_MASK,
			     SPRD_DET_L16B_PERIOD);
	sprd_thm_update_bits((thm->thm_base + SPRD_THM_MON_CTL),
			     SPRD_THM_MON_MODE_MASK,  SPRD_THM_MON_MODE_VAL);
	sprd_thm_update_bits((thm->thm_base + SPRD_THM_MON_PERIOD),
			     SPRD_MON_PERIOD_MASK,
			     SPRD_MON_PERIOD);
	sprd_thm_update_bits((thm->thm_base + SPRD_THM_OVERHEAT_HOT_THRES),
			     SPRD_TRIP_THRESHOLD_MASK,
			     (SPRD_THM_TRIP_THRESHOLD(thm->otp_rawdata) |
			      thm->hot_rawdata));
	sprd_thm_update_bits((thm->thm_base + SPRD_THM_HOT2NOR_HIGH_THRES),
			     SPRD_TRIP_THRESHOLD_MASK,
			     (SPRD_THM_TRIP_THRESHOLD(thm->hot2nor_rawdata) |
			      thm->highoff_rawdata));
	sprd_thm_update_bits((thm->thm_base + SPRD_THM_LOW_COLD_THRES),
			     SPRD_TRIP_THRESHOLD_MASK,
			     (SPRD_THM_TRIP_THRESHOLD(thm->lowoff_rawdata) |
			      thm->cold_rawdata));
	return sprd_thm_enable(thm, true);
}

static int sprd_hw_thm_suspend(struct sprd_thermal_data *thm)
{
	sprd_thm_enable(thm, false);

	return 0;
}

static int sprd_hw_thm_resume(struct sprd_thermal_data *thm)
{
	sprd_thm_enable(thm, true);

	return 0;
}

static void sprd_thm_resume_work(struct work_struct *work)
{
	struct sprd_thermal_data *thm = container_of(work,
						     struct sprd_thermal_data,
						     thm_resume_work.work);
	thm->ready_flag = 1;
}

static const struct thermal_zone_of_device_ops sprd_thm_ops = {
	.get_temp = sprd_thm_temp_read,
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

static int sprd_thm_probe(struct platform_device *pdev)
{
	struct sprd_thermal_data *thm = NULL;
	struct resource *res;
	int ret;
	int sensor_id = 0;
	struct device_node *np = pdev->dev.of_node;
	const struct sprd_thm_variant_data *pdata;

	pdata = of_device_get_match_data(&pdev->dev);
	if (!pdata) {
		dev_err(&pdev->dev, "No matching driver data found\n");
		return -EINVAL;
	}

	thm = devm_kzalloc(&pdev->dev, sizeof(*thm), GFP_KERNEL);
	if (!thm)
		return -ENOMEM;

	thm->var_data = pdata;
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	thm->thm_base = devm_ioremap_resource(&pdev->dev, res);
	if (!thm->thm_base)
		return -ENOMEM;

	thm->clk = devm_clk_get(&pdev->dev, "enable");
	if (IS_ERR(thm->clk)) {
		dev_err(&pdev->dev, "get the clock failed\n");
		return PTR_ERR(thm->clk);
	}

	ret = clk_prepare_enable(thm->clk);
	if (ret) {
		dev_err(&pdev->dev, "clock prepare enable failed\n");
		return ret;
	}

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

	ret = sprd_thm_cal_read(np, "thm_sign_cal", &thm->ratio_sign);
	if (ret)
		thm->ratio_sign = 0;

	ret = sprd_thm_cal_read(np, "thm_ratio_cal", &thm->ratio_off);
	if (ret)
		thm->ratio_off = 0;

	ret = sprd_thm_cal_read(np, "thm_delta_cal", &thm->detal_cal);
	if (ret)
		goto disable_clk;

	thm->id = sensor_id;
	thm->dev = &pdev->dev;

	INIT_DELAYED_WORK(&thm->thm_resume_work, sprd_thm_resume_work);
	sprd_thm_efuse_cal(thm);
	sprd_thm_trip_config(thm);

	ret = sprd_thm_hw_init(thm);
	if (ret) {
		dev_err(&pdev->dev, "sprd thm hw init failed\n");
		goto disable_clk;
	}
	mutex_init(&thm->lock);
	thm->thmzone_dev =
	    devm_thermal_zone_of_sensor_register(thm->dev, thm->id,
						 thm, &sprd_thm_ops);
	if (IS_ERR_OR_NULL(thm->thmzone_dev)) {
		dev_err(&pdev->dev, "register thermal zone failed %d\n",
			thm->id);
		ret = PTR_ERR(thm->thmzone_dev);
		mutex_destroy(&thm->lock);
		goto disable_clk;
	}

	thm->ready_flag = 1;
	platform_set_drvdata(pdev, thm);
	return 0;

disable_clk:
	clk_disable_unprepare(thm->clk);
	return ret;
}

static int sprd_thm_remove(struct platform_device *pdev)
{
	struct sprd_thermal_data *thm = platform_get_drvdata(pdev);

	thermal_zone_device_unregister(thm->thmzone_dev);
	cancel_delayed_work_sync(&thm->thm_resume_work);
	mutex_destroy(&thm->lock);

	return 0;
}

static int sprd_thm_suspend(struct device *dev)
{
	struct sprd_thermal_data *thm = dev_get_drvdata(dev);

	flush_delayed_work(&thm->thm_resume_work);
	mutex_lock(&thm->lock);
	thm->ready_flag = 0;
	sprd_hw_thm_suspend(thm);
	mutex_unlock(&thm->lock);
	if (thm->vdd_cnt > 0)
		sprd_thm_vdd_bind(dev, thm);
	clk_disable_unprepare(thm->clk);

	return 0;
}

static int sprd_thm_resume(struct device *dev)
{
	int ret;
	struct sprd_thermal_data *thm = dev_get_drvdata(dev);

	ret = clk_prepare_enable(thm->clk);
	if (ret) {
		dev_err(dev, "resume clock prepare enable failed\n");
		return ret;
	}

	if (thm->vdd_cnt > 0)
		sprd_thm_vdd_unbind(dev, thm);

	sprd_hw_thm_resume(thm);
	queue_delayed_work(system_power_efficient_wq,
			   &thm->thm_resume_work, msecs_to_jiffies(500));

	return 0;
}

static const struct dev_pm_ops sprd_thm_dev_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sprd_thm_suspend, sprd_thm_resume)
};

static const struct of_device_id sprd_thermal_of_match[] = {
	{.compatible = "sprd,sharkl3-thermal", .data = &sharkl3_data},
	{},
};

static struct platform_driver sprd_thermal_driver = {
	.probe = sprd_thm_probe,
	.remove = sprd_thm_remove,
	.driver = {
		.name = "sprd-dedicate-thermal",
		.pm = &sprd_thm_dev_pm_ops,
		.of_match_table = sprd_thermal_of_match,
	},
};

module_platform_driver(sprd_thermal_driver);

MODULE_AUTHOR("Freeman Liu <freeman.liu@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum dedicated thermal driver");
MODULE_LICENSE("GPL v2");
