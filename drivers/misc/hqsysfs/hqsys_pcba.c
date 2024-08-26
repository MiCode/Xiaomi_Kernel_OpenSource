#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <linux/of_platform.h>
#include <linux/iio/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/printk.h>
#include <linux/err.h>
#include <linux/libfdt.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/types.h>
#include "hqsys_pcba.h"

#define SKU_CMDLINE	"androidboot.product.hardware.sku"
#define RSC_CMDLINE	"androidboot.rsc"

#define PCBA_CONFIG_CMDLINE	"pcba_config"
#define PCBA_COUNT_CMDLINE	"pcba_config_count"
#define PROJECT_STAGE_CMDLINE  "project_stage"

struct PCBA_MSG pcba_msg = {PCBA_INFO_UNKNOW, STAGE_UNKNOW, 0, 0, NULL, NULL};

static int board_id_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct device_node *pcba_node = pdev->dev.of_node;

	ret = of_property_read_u32(pcba_node, PCBA_CONFIG_CMDLINE, &(pcba_msg.pcba_config));
	if (ret) {
		pr_err("%s: get pcba_config fail\n", __func__);
		return -1;
	}

	ret = of_property_read_u32(pcba_node, PCBA_COUNT_CMDLINE, &(pcba_msg.pcba_config_count));
	if (ret) {
		pr_err("%s: get pcba_config_count fail\n", __func__);
		return -1;
	}

	ret = of_property_read_string(pdev->dev.of_node, RSC_CMDLINE, &(pcba_msg.rsc));
	if (ret) {
		pr_err("%s: get rsc fail\n", __func__);
		return -1;
	}
	pr_err("rsc = %s\n", pcba_msg.rsc);

	ret = of_property_read_string(pdev->dev.of_node, SKU_CMDLINE, &(pcba_msg.sku));
	if (ret) {
		pr_err("%s: get sku fail\n", __func__);
		return -1;
	}
	pr_err("sku = %s\n", pcba_msg.sku);

	ret = of_property_read_u32(pcba_node, PROJECT_STAGE_CMDLINE, &(pcba_msg.pcba_stage));
    if (ret) {
        pr_err("%s: get pcba_stage fail\n", __func__);
        return -1;
    }

	pr_info("pcba_config = %d\n", pcba_msg.pcba_config);
	pr_info("pcba_config_count = %d\n", pcba_msg.pcba_config_count);
	pr_info("pcba_stage = %d\n", pcba_msg.pcba_stage);

	pcba_msg.huaqin_pcba_config = (pcba_msg.pcba_stage - 1) * (pcba_msg.pcba_config_count) + pcba_msg.pcba_config;
	pr_info("[%s]: huaqin_pcba_config = %d\n", __func__, pcba_msg.huaqin_pcba_config);

	if (pcba_msg.huaqin_pcba_config < PCBA_INFO_UNKNOW || pcba_msg.huaqin_pcba_config >= PCBA_INFO_END) {
		pcba_msg.huaqin_pcba_config = PCBA_INFO_UNKNOW;
	}

	return 0;
}

static int board_id_remove(struct platform_device *pdev)
{
	pr_err("enter [%s] \n", __func__);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id boardId_of_match[] = {
	{.compatible = "mediatek,board_id",},
	{},
};
#endif

static struct platform_driver boardId_driver = {
	.probe = board_id_probe,
	.remove = board_id_remove,
	.driver = {
		.name = "board_id",
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = boardId_of_match,
#endif
	},
};

struct PCBA_MSG* get_pcba_msg(void)
{
	return &pcba_msg;
}
EXPORT_SYMBOL_GPL(get_pcba_msg);

static int __init huaqin_pcba_early_init(void)
{
	int ret;
	pr_err("[%s]start to register boardId driver\n", __func__);

	ret = platform_driver_register(&boardId_driver);
	if (ret) {
		pr_err("[%s]Failed to register boardId driver\n", __func__);
		return ret;
	}
	return 0;
}

static void __exit huaqin_pcba_exit(void)
{
	platform_driver_unregister(&boardId_driver);
}

module_init(huaqin_pcba_early_init);//before device_initcall
module_exit(huaqin_pcba_exit);
MODULE_DESCRIPTION("huaqin sys pcba");
MODULE_LICENSE("GPL");
