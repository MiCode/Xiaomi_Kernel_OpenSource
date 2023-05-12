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
#include <linux/soc/qcom/smem.h>
#include "hqsys_pcba.h"
#include "smem_type.h"

/*struct board_id_information {
	int adc_channel;
	int voltage;
};

static struct board_id_information board_id;
*/
static PCBA_CONFIG huaqin_pcba_config = PCBA_UNKNOW;

//extern char *saved_command_line;
//extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
//static bool read_pcba_config(void);

static void read_pcba_config_from_smem(void)
{
	PCBA_CONFIG *pcba_config = NULL;
	size_t size;
	pr_info("%s!\n", __func__);
	pr_info("SMEM_ID_VENDOR1 is %d\n", SMEM_ID_VENDOR1);

	pcba_config = (PCBA_CONFIG *)qcom_smem_get(QCOM_SMEM_HOST_ANY, SMEM_ID_VENDOR1, &size);
	if (pcba_config) {
		pr_err("pcba config = %d 0x%x.\n", *(pcba_config), *(pcba_config));
		if (*(pcba_config) > PCBA_UNKNOW && *(pcba_config) < PCBA_END) {
			huaqin_pcba_config = *pcba_config;
		} else {
			huaqin_pcba_config = PCBA_UNKNOW;
		}
	} else {
		pr_err("pcba config failed\n");
		huaqin_pcba_config = PCBA_UNKNOW;
	}
}

PCBA_CONFIG get_huaqin_pcba_config(void)
{
	return huaqin_pcba_config;
}
EXPORT_SYMBOL_GPL(get_huaqin_pcba_config);


#if 0
static int board_id_probe(struct platform_device *pdev)
{
	int ret;
	printk("board_id_probe enter\n");
	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret) {
		pr_err("[%s] Failed %d!!!\n", __func__, ret);
		return ret;
	}
	read_pcba_config_from_smem();

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
#endif

static int __init huaqin_pcba_early_init(void)
{
	pr_info("huaqin_pcba_early_init\n");
	read_pcba_config_from_smem();
	return 0;
}

subsys_initcall(huaqin_pcba_early_init); //before device_initcall

//late_initcall(huaqin_pcba_early_init);   //late initcall

MODULE_AUTHOR("lizheng<LiZheng6@huaqin.com>");
MODULE_DESCRIPTION("huaqin sys pcba");
MODULE_LICENSE("GPL");
