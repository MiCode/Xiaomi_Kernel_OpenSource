#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/export.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/proc_fs.h>
#include <asm/uaccess.h>
#include <mt-plat/mtk_devinfo.h>
#include <linux/of_platform.h>
#include <linux/iio/consumer.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/printk.h>
#include <linux/err.h>
#include "hqsys_pcba.h"

static int huaqin_pcba_config = 0;

static int pcba_config;
static int pcba_stage;
static int pcba_count;
char pcba_str[50];

/* get project config index */
static int __init get_pcba_config(char *p)
{
	char pcba[10];

	if (!p) return -1;

	strlcpy(pcba, p, sizeof(pcba));

	if (kstrtoint(pcba, 10, &pcba_config))
		return -1;

	pr_err("[%s]: pcba config = %d\n", __func__, pcba_config);

	return 0;
}
early_param("pcba_config", get_pcba_config);

/* get project config count */
static int __init get_pcba_count(char *p)
{
	char count[10];

	if (!p) return -1;

	strlcpy(count, p, sizeof(count));

	if (kstrtoint(count, 10, &pcba_count))
		return -1;

	printk("[%s]: pcba count = %d\n", __func__, pcba_count);

	return 0;
}
early_param("pcba_count", get_pcba_count);

/* get project stage, such as: 2->P01 3->P1 */
static int __init get_pcba_stage(char *p)
{
	char count[10];

	if (!p) return -1;

	strlcpy(count, p, sizeof(count));

	if (kstrtoint(count, 10, &pcba_stage))
		return -1;

	printk("[%s]: pcba stage = %d\n", __func__, pcba_stage);

	return 0;
}
early_param("pcba_stage", get_pcba_stage);

/* get pcba config str, such as: PCBA_C3T_P0-1_CN */
static int __init get_final_pcba_config(char *p)
{
	if (!p) {
		strlcpy(pcba_str, "PCBA_UNKONW", sizeof(pcba_str));
		return 0;
	}

	strlcpy(pcba_str, p, sizeof(pcba_str));

	printk("[%s]: pcba_str = %d\n", __func__, pcba_str);

	return 0;
}
early_param("final_pcba", get_final_pcba_config);

static int board_id_probe(struct platform_device *pdev)
{
	int ret;

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret) {
		pr_err("[%s] Failed %d!!!\n", __func__, ret);
		return ret;
	}

	huaqin_pcba_config = (pcba_stage - 1) * pcba_count + pcba_config;

	return 0;
}

static int board_id_remove(struct platform_device *pdev)
{
	pr_err("enter [%s] \n", __func__);
	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id boardId_of_match[] = {
	{.compatible = "hq,board_id",},
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

int get_huaqin_pcba_config(void)
{
	return huaqin_pcba_config;
}
EXPORT_SYMBOL_GPL(get_huaqin_pcba_config);

char *get_huaqin_pcba_str(void)
{
	return pcba_str;
}
EXPORT_SYMBOL_GPL(get_huaqin_pcba_str);

static int __init huaqin_pcba_init(void)
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

module_init(huaqin_pcba_init);
module_exit(huaqin_pcba_exit);

MODULE_DESCRIPTION("huaqin sys pcba");
MODULE_LICENSE("GPL");
