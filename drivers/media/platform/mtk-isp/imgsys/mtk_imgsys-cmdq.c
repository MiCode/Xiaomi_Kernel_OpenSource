// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2018 MediaTek Inc.
 *
 * Author: Daniel Huang <daniel.huang@mediatek.com>
 *
 */

#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/soc/mediatek/mtk-cmdq-ext.h>
//#include <linux/pm_opp.h>
//#include <linux/pm_runtime.h>
//#include <linux/regulator/consumer.h>
#include "mtk_imgsys-cmdq.h"
#include "mtk_imgsys-cmdq-ext.h"
//#include "mtk_imgsys-trace.h"
//#include "mtk-interconnect.h"
#include "platforms/isp71/mtk_imgsys-cmdq-plat_def.h"
#include "platforms/isp7s/mtk_imgsys-cmdq-plat_def.h"

static struct mtk_imgcmdq_dev *imgsys_cmdq_dev;


int imgsys_cmdq_ts_en;
module_param(imgsys_cmdq_ts_en, int, 0644);

int imgsys_wpe_bwlog_en;
module_param(imgsys_wpe_bwlog_en, int, 0644);

int imgsys_cmdq_ts_dbg_en;
module_param(imgsys_cmdq_ts_dbg_en, int, 0644);

int imgsys_dvfs_dbg_en;
module_param(imgsys_dvfs_dbg_en, int, 0644);

int imgsys_qos_update_freq;
module_param(imgsys_qos_update_freq, int, 0644);

int imgsys_qos_blank_int;
module_param(imgsys_qos_blank_int, int, 0644);

int imgsys_qos_factor;
module_param(imgsys_qos_factor, int, 0644);

/*#####*/
bool imgsys_cmdq_ts_enable(void)
{
	return imgsys_cmdq_ts_en;
}

u32 imgsys_wpe_bwlog_enable(void)
{
	return imgsys_wpe_bwlog_en;
}

bool imgsys_cmdq_ts_dbg_enable(void)
{
	return imgsys_cmdq_ts_dbg_en;
}

bool imgsys_dvfs_dbg_enable(void)
{
	return imgsys_dvfs_dbg_en;
}

/*#####*/
static void module_uninit(struct kref *kref)
{
	struct mtk_imgsys_dev *imgsys_dev;
	int i;
	struct mtk_imgsys_dvfs *dvfs_info;

	imgsys_dev = container_of(kref, struct mtk_imgsys_dev, init_kref);
	dvfs_info = &imgsys_dev->dvfs_info;

	for (i = 0; i < (imgsys_dev->num_mods); i++)
		if (imgsys_dev->modules[i].uninit)
			imgsys_dev->modules[i].uninit(imgsys_dev);

	if (IS_ERR_OR_NULL(dvfs_info->reg) || !regulator_is_enabled(dvfs_info->reg))
		dev_dbg(dvfs_info->dev,
			"%s: [ERROR] reg is null or disabled\n", __func__);
	else
		regulator_disable(dvfs_info->reg);

}

void mtk_imgsys_mod_put(struct mtk_imgsys_dev *imgsys_dev)
{
	struct kref *kref;

	kref = &imgsys_dev->init_kref;
	kref_put(kref, module_uninit);
}

void mtk_imgsys_mod_get(struct mtk_imgsys_dev *imgsys_dev)
{
	struct kref *kref;

	kref = &imgsys_dev->init_kref;
	kref_get(kref);
}

/*####*/

void imgsys_cmdq_init(struct mtk_imgsys_dev *imgsys_dev, const int nr_imgsys_dev)
{
	struct mtk_imgcmdq_dev *cmdq_dev = platform_get_drvdata(imgsys_dev->imgcmdq_pdev);

	cmdq_dev->cust_data->cmdq_init(imgsys_dev, nr_imgsys_dev);
}
EXPORT_SYMBOL(imgsys_cmdq_init);

void imgsys_cmdq_release(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgcmdq_dev *cmdq_dev = platform_get_drvdata(imgsys_dev->imgcmdq_pdev);

	cmdq_dev->cust_data->cmdq_release(imgsys_dev);
}
EXPORT_SYMBOL(imgsys_cmdq_release);

void imgsys_cmdq_streamon(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgcmdq_dev *cmdq_dev = platform_get_drvdata(imgsys_dev->imgcmdq_pdev);

	cmdq_dev->cust_data->cmdq_streamon(imgsys_dev);
}
EXPORT_SYMBOL(imgsys_cmdq_streamon);


void imgsys_cmdq_streamoff(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgcmdq_dev *cmdq_dev = platform_get_drvdata(imgsys_dev->imgcmdq_pdev);

	cmdq_dev->cust_data->cmdq_streamoff(imgsys_dev);
}
EXPORT_SYMBOL(imgsys_cmdq_streamoff);

int imgsys_cmdq_sendtask(struct mtk_imgsys_dev *imgsys_dev,
				struct swfrm_info_t *frm_info,
				void (*cmdq_cb)(struct cmdq_cb_data data,
					uint32_t subfidx, bool isLastTaskInReq),
				void (*cmdq_err_cb)(struct cmdq_cb_data data,
					uint32_t fail_subfidx, bool isHWhang))
{
	int ret = 0;
	struct mtk_imgcmdq_dev *cmdq_dev = platform_get_drvdata(imgsys_dev->imgcmdq_pdev);

	ret = cmdq_dev->cust_data->cmdq_sendtask(imgsys_dev, frm_info, cmdq_cb, cmdq_err_cb);
	return ret;
}
EXPORT_SYMBOL(imgsys_cmdq_sendtask);

void imgsys_cmdq_clearevent(struct mtk_imgsys_dev *imgsys_dev, int event_id)
{
	struct mtk_imgcmdq_dev *cmdq_dev = platform_get_drvdata(imgsys_dev->imgcmdq_pdev);

	cmdq_dev->cust_data->cmdq_clearevent(event_id);
}
EXPORT_SYMBOL(imgsys_cmdq_clearevent);

#if DVFS_QOS_READY
void mtk_imgsys_mmdvfs_init(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgcmdq_dev *cmdq_dev = platform_get_drvdata(imgsys_dev->imgcmdq_pdev);

	cmdq_dev->cust_data->mmdvfs_init(imgsys_dev);
}
EXPORT_SYMBOL(mtk_imgsys_mmdvfs_init);

void mtk_imgsys_mmdvfs_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgcmdq_dev *cmdq_dev = platform_get_drvdata(imgsys_dev->imgcmdq_pdev);

	cmdq_dev->cust_data->mmdvfs_uninit(imgsys_dev);
}
EXPORT_SYMBOL(mtk_imgsys_mmdvfs_uninit);

void mtk_imgsys_mmqos_init(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgcmdq_dev *cmdq_dev = platform_get_drvdata(imgsys_dev->imgcmdq_pdev);

	cmdq_dev->cust_data->mmqos_init(imgsys_dev);
}
EXPORT_SYMBOL(mtk_imgsys_mmqos_init);

void mtk_imgsys_mmqos_uninit(struct mtk_imgsys_dev *imgsys_dev)
{
	struct mtk_imgcmdq_dev *cmdq_dev = platform_get_drvdata(imgsys_dev->imgcmdq_pdev);

	cmdq_dev->cust_data->mmqos_init(imgsys_dev);
}
EXPORT_SYMBOL(mtk_imgsys_mmqos_uninit);

void mtk_imgsys_power_ctrl(struct mtk_imgsys_dev *imgsys_dev, bool isPowerOn)
{
	struct mtk_imgcmdq_dev *cmdq_dev = platform_get_drvdata(imgsys_dev->imgcmdq_pdev);

	cmdq_dev->cust_data->power_ctrl(imgsys_dev, isPowerOn);
}
EXPORT_SYMBOL(mtk_imgsys_power_ctrl);

#endif

//#########

struct platform_device *mtk_imgsys_cmdq_get_plat_dev(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *cmdq_node;
	struct platform_device *cmdq_pdev;

	dev_dbg(&pdev->dev, "- E. imgsys cmdq get platform device.\n");

	cmdq_node = of_parse_phandle(dev->of_node, "mediatek,imgsys_cmdq", 0);
	if (cmdq_node == NULL) {
		dev_info(&pdev->dev, "%s can't get imgsys cmdq node.\n", __func__);
		return NULL;
	}

	cmdq_pdev = of_find_device_by_node(cmdq_node);
	if (WARN_ON(cmdq_pdev == NULL) == true) {
		dev_info(&pdev->dev, "%s imgsys cmdq pdev failed.\n", __func__);
		of_node_put(cmdq_node);
		return NULL;
	}

	return cmdq_pdev;
}
EXPORT_SYMBOL(mtk_imgsys_cmdq_get_plat_dev);

static int mtk_imgsys_cmdq_probe(struct platform_device *pdev)
{
	struct mtk_imgcmdq_dev *cmdq_dev;

	dev_info(&pdev->dev, "- E. imgsys cmdq driver probe\n");

	cmdq_dev = devm_kzalloc(&pdev->dev, sizeof(struct mtk_imgcmdq_dev), GFP_KERNEL);
	if (cmdq_dev == NULL)
		return -ENOMEM;

	imgsys_cmdq_dev = cmdq_dev;
	cmdq_dev->dev = &pdev->dev;
	cmdq_dev->cust_data = of_device_get_match_data(&pdev->dev);

	platform_set_drvdata(pdev, cmdq_dev);
	dev_set_drvdata(&pdev->dev, cmdq_dev);

	dev_dbg(&pdev->dev, "- X. imgsys cmdq driver probe success\n");
	return 0;
}


static int mtk_imgsys_cmdq_remove(struct platform_device *pdev)
{
	dev_dbg(&pdev->dev, "- E. imgsys cmdq driver remove\n");
	devm_kfree(&pdev->dev, imgsys_cmdq_dev);
	dev_dbg(&pdev->dev, "- X. imgsys cmdq driver remove success\n");
	return 0;
}

static const struct of_device_id mtk_imgsys_cmdq_of_match[] = {
	{.compatible = "mediatek,imgsys-cmdq-71", .data = (void *)&imgsys_cmdq_data_71},
	{.compatible = "mediatek,imgsys-cmdq-7s", .data = (void *)&imgsys_cmdq_data_7s},
	{}
};
MODULE_DEVICE_TABLE(of, mtk_imgsys_cmdq_of_match);

static struct platform_driver mtk_imgsys_cmdq_driver = {
	.probe  = mtk_imgsys_cmdq_probe,
	.remove = mtk_imgsys_cmdq_remove,
	.driver = {
		.name = "camera-imgsys-cmdq",
		.owner  = THIS_MODULE,
		.of_match_table = mtk_imgsys_cmdq_of_match,
	},
};

module_platform_driver(mtk_imgsys_cmdq_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mediatek imgsys cmdq driver");
