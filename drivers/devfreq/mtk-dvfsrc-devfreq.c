// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 MediaTek Inc.
 */
#include <linux/module.h>
#include <linux/device.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/devfreq.h>
#include <linux/pm_opp.h>
#include <linux/clk.h>
#include <linux/clk-provider.h>
#include <linux/arm-smccc.h>
#include <linux/soc/mediatek/mtk_dvfsrc.h>
#include <linux/soc/mediatek/mtk_sip_svc.h>

/* Query available frequencies. */
#define DVFSRC_DDR_DVFS_GET_FREQ_COUNT	0x7
#define DVFSRC_DDR_DVFS_GET_FREQ_INFO	0x5

#define MAX_FREQ_COUNT 12

struct dvfsrc_devfreq {
	struct devfreq_dev_profile profile;
	struct devfreq *devfreq;
	struct device *ctrl_dev;
	int freq_count;
	unsigned long freq_table[MAX_FREQ_COUNT];
	unsigned long rate;
};

static void dvfsrc_set_freq_level(struct dvfsrc_devfreq *dvfsrc,
				  u32 level)
{
	mtk_dvfsrc_send_request(dvfsrc->ctrl_dev,
		MTK_DVFSRC_CMD_DRAM_REQUEST,
		level);
}

static unsigned long dvfsrc_get_cur_freq(struct dvfsrc_devfreq *dvfsrc)
{
	u32 val;

	mtk_dvfsrc_query_info(dvfsrc->ctrl_dev,
		MTK_DVFSRC_CMD_DRAM_LEVEL_QUERY, &val);

	if (val > MAX_FREQ_COUNT - 1)
		return 0;

	return dvfsrc->freq_table[val];
}

static u32 dvfsrc_find_freq_level(struct dvfsrc_devfreq *dvfsrc,
				  unsigned long rate)
{
	u32 index;

	for (index = 0; index < dvfsrc->freq_count - 1; index++) {
		if (dvfsrc->freq_table[index] >= rate)
			break;
	}

	return index;
}

static int dvfsrc_devfreq_target(struct device *dev, unsigned long *freq,
				 u32 flags)
{
	struct dvfsrc_devfreq *dvfsrc = dev_get_drvdata(dev);
	struct dev_pm_opp *new_opp;
	unsigned long new_freq;
	u32 level;
	int ret = 0;

	new_opp = devfreq_recommended_opp(dev, freq, flags);
	if (IS_ERR(new_opp)) {
		ret = PTR_ERR(new_opp);
		dev_err(dev, "failed to get recommended opp: %d\n", ret);
		return ret;
	}
	new_freq = dev_pm_opp_get_freq(new_opp);
	dev_pm_opp_put(new_opp);

	if (dvfsrc->rate == new_freq)
		return 0;

	level = dvfsrc_find_freq_level(dvfsrc, new_freq);
	dvfsrc_set_freq_level(dvfsrc, level);

	dvfsrc->rate = dvfsrc_get_cur_freq(dvfsrc);
	*freq = new_freq;

	return ret;
}

static int dvfsrc_devfreq_get_cur_freq(struct device *dev, unsigned long *freq)
{
	struct dvfsrc_devfreq *dvfsrc = dev_get_drvdata(dev);

	*freq = dvfsrc->rate;

	return 0;
}

static int dvfsrc_devfreq_get_dev_status(struct device *dev, struct devfreq_dev_status *stat)
{
	struct dvfsrc_devfreq *dvfsrc = dev_get_drvdata(dev);

	stat->busy_time = 0;
	stat->total_time = 0;
	stat->current_frequency = dvfsrc->rate;

	return 0;
}

static int dvfsrc_init_freq_info(struct device *dev)
{
	struct dvfsrc_devfreq *dvfsrc = dev_get_drvdata(dev);
	struct arm_smccc_res res;
	int index, err;
	unsigned long rate;

	arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL, DVFSRC_DDR_DVFS_GET_FREQ_COUNT,
			0, 0, 0, 0, 0, 0, &res);
	if (res.a0)
		return -ENODEV;

	dvfsrc->freq_count = res.a1;
	if (dvfsrc->freq_count <= 0 || dvfsrc->freq_count > MAX_FREQ_COUNT)
		return -EINVAL;

	for (index = 0; index < dvfsrc->freq_count; ++index) {
		arm_smccc_smc(MTK_SIP_VCOREFS_CONTROL,
				DVFSRC_DDR_DVFS_GET_FREQ_INFO,
				index, 0, 0, 0, 0, 0, &res);
		if ((res.a0) || (long)res.a1 <= 0)
			return -EINVAL;
		/* return freq as khz*/
		rate = res.a1 * 1000;

		err = dev_pm_opp_add(dev, rate, 0);
		if (err) {
			dev_err(dev, "failed to add opp: %d\n", err);
			return err;
		}

		dvfsrc->freq_table[index] = rate;
	}

	return 0;
}

static int dvfsrc_devfreq_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct dvfsrc_devfreq *dvfsrc;
	const char *gov = DEVFREQ_GOV_USERSPACE;
	int ret;

	dvfsrc = devm_kzalloc(dev, sizeof(*dvfsrc), GFP_KERNEL);
	if (!dvfsrc)
		return -ENOMEM;

	platform_set_drvdata(pdev, dvfsrc);
	ret = dvfsrc_init_freq_info(dev);
	if (ret) {
		dev_err(dev, "failed to init dram freq info: %d\n", ret);
		goto err_free_opp;
	}
	dvfsrc->ctrl_dev = dev->parent;
	dvfsrc->profile.target = dvfsrc_devfreq_target;
	dvfsrc->profile.get_cur_freq = dvfsrc_devfreq_get_cur_freq;
	dvfsrc->profile.get_dev_status = dvfsrc_devfreq_get_dev_status;
	dvfsrc->profile.initial_freq = 0;
	dvfsrc->devfreq = devm_devfreq_add_device(dev,
		&dvfsrc->profile, gov, NULL);

	if (IS_ERR(dvfsrc->devfreq)) {
		ret = PTR_ERR(dvfsrc->devfreq);
		dev_err(dev, "failed to add devfreq device: %d\n", ret);
		goto err_free_opp;
	}
	return 0;

err_free_opp:
	dev_pm_opp_remove_all_dynamic(dev);

	return ret;
}

static int dvfsrc_devfreq_remove(struct platform_device *pdev)
{
	struct dvfsrc_devfreq *dvfsrc = platform_get_drvdata(pdev);
	int ret;

	ret = devfreq_remove_device(dvfsrc->devfreq);
	if (ret)
		dev_info(&pdev->dev, "failed to remove devfreq device: %d\n", ret);

	dev_pm_opp_remove_all_dynamic(&pdev->dev);

	return 0;
}

static struct platform_driver dvfsrc_devfreq_platdrv = {
	.probe	= dvfsrc_devfreq_probe,
	.remove = dvfsrc_devfreq_remove,
	.driver = {
		.name	= "mtk-dvfsrc-devfreq",
	},
};
module_platform_driver(dvfsrc_devfreq_platdrv);
MODULE_DESCRIPTION("MTK DVFSRC devfreq driver");
MODULE_AUTHOR("Arvin wang <arvin.wang@mediatek.com>");
MODULE_LICENSE("GPL v2");

