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
#include "hqsys_pcba.h"


PCBA_CONFIG huaqin_pcba_config = PCBA_UNKNOW;

//extern char *saved_command_line;
//extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);

typedef struct {
	int voltage_min;
	int voltage_max;
	PCBA_CONFIG version;
} board_id_map_t;

 struct board_id_information {
	int adc_channel;
	int voltage;
};
static board_id_map_t PCBA_DETECT_K19_TYPE_CN[] = {
	{130, 225, PCBA_K19_P0_1_CN},
};

static board_id_map_t PCBA_DETECT_K19_TYPE_GLOBAL[] = {
	{130, 225, PCBA_K19_P0_1_GLOBAL},
};

static board_id_map_t PCBA_DETECT_K19P_TYPE_INDIA[] = {
	{226, 315, PCBA_K19P_P1_INDIA},
	{316, 405, PCBA_K19P_P1_1_INDIA},
	{406, 485, PCBA_K19P_P2_INDIA},
	{486, 565, PCBA_K19P_MP_INDIA},
};

static board_id_map_t PCBA_DETECT_K19_TYPE_INDIA[] = {
	{406, 485, PCBA_K19_P2_INDIA},
	{486, 565, PCBA_K19_MP_INDIA},
};

static board_id_map_t PCBA_DETECT_K19_TYPE_CN_NEW[] = {
	{226, 315, PCBA_K19_P1_CN_NEW},
	{316, 405, PCBA_K19_P1_1_CN_NEW},
	{406, 485, PCBA_K19_P2_CN_NEW},
	{486, 565, PCBA_K19_MP_CN_NEW},
};

static board_id_map_t PCBA_DETECT_K19_TYPE_GLOBAL_NEW[] = {
	{226, 315, PCBA_K19_P1_GLOBAL_NEW},
	{316, 405, PCBA_K19_P1_1_GLOBAL_NEW},
	{406, 485, PCBA_K19_P2_GLOBAL_NEW},
	{486, 565, PCBA_K19_MP_GLOBAL_NEW},
};

static board_id_map_t PCBA_DETECT_K19P_TYPE_GLOBAL[] = {
	{316, 405, PCBA_K19P_P1_1_GLOBAL},
	{406, 485, PCBA_K19P_P2_GLOBAL},
	{486, 565, PCBA_K19P_MP_GLOBAL},
};

static board_id_map_t PCBA_DETECT_K19_TYPE_CN_FINAL[] = {
	{406, 485, PCBA_K19_P2_CN_FINAL},
	{486, 565, PCBA_K19_MP_CN_FINAL},
};

static struct board_id_information board_id;
static int pcba_type;

static int __init get_pcba_config(char *p)
{
	char pcba[10];

	strlcpy(pcba, p, sizeof(pcba));

	printk("[%s]: pcba = %s\n", __func__, pcba);

	pcba_type = pcba[0] - '0';

	return 0;
}

early_param("pcba_config", get_pcba_config);

static bool read_pcba_config(void)
{
	int ret = 0;
	int i = 0, map_size = 0;
	int auxadc_voltage;
	struct iio_channel *channel;
	struct device_node *board_id_node;
	struct platform_device *board_id_dev;
	board_id_map_t *board_id_map;

	board_id_node = of_find_node_by_name(NULL, "board_id");
	if (board_id_node == NULL) {
		pr_err("[%s] find board_id node fail \n", __func__);
		return false;
	} else {
		pr_err("[%s] find board_id node success %s \n", __func__, board_id_node->name);
	}

	board_id_dev = of_find_device_by_node(board_id_node);
	if (board_id_dev == NULL) {
		pr_err("[%s] find board_id dev fail \n", __func__);
		return false;
	} else {
		pr_err("[%s] find board_id dev success %s \n", __func__, board_id_dev->name);
	}

	channel = iio_channel_get(&(board_id_dev->dev), "board_id-channel");
	if (IS_ERR(channel)) {
		ret = PTR_ERR(channel);
		pr_err("[%s] iio channel not found %d\n", __func__, ret);
		return false;
	} else {
		pr_err("[%s] get channel success\n", __func__);
	}

	if (channel != NULL) {
		ret = iio_read_channel_processed(channel, &auxadc_voltage);
	} else {
		pr_err("[%s] no channel to processed \n", __func__);
		return false;
	}

	if (ret < 0) {
		pr_err("[%s] IIO channel read failed %d \n", __func__, ret);
		return false;
	} else {
		pr_err("[%s] auxadc_voltage is %d\n", __func__, auxadc_voltage);
		board_id.voltage = auxadc_voltage * 1500 / 4096;
		pr_err("[%s] board_id_voltage is %d\n", __func__, board_id.voltage);
	}

	pr_err("[%s] read_pcba_config board_id.voltage: %d\n", __func__, board_id.voltage);
	if (pcba_type ==  PCBA_K19_TYPE_CN) {
		board_id_map = PCBA_DETECT_K19_TYPE_CN;
		map_size = sizeof(PCBA_DETECT_K19_TYPE_CN)/sizeof(board_id_map_t);
	} else if (pcba_type ==  PCBA_K19_TYPE_GLOBAL) {
		board_id_map = PCBA_DETECT_K19_TYPE_GLOBAL;
		map_size = sizeof(PCBA_DETECT_K19_TYPE_GLOBAL)/sizeof(board_id_map_t);
	} else if (pcba_type ==  PCBA_K19P_TYPE_INDIA) {
		board_id_map = PCBA_DETECT_K19P_TYPE_INDIA;
		map_size = sizeof(PCBA_DETECT_K19P_TYPE_INDIA)/sizeof(board_id_map_t);
	} else if (pcba_type ==  PCBA_K19_TYPE_CN_NEW) {
		board_id_map = PCBA_DETECT_K19_TYPE_CN_NEW;
		map_size = sizeof(PCBA_DETECT_K19_TYPE_CN_NEW)/sizeof(board_id_map_t);
	} else if (pcba_type ==  PCBA_K19_TYPE_GLOBAL_NEW) {
		board_id_map = PCBA_DETECT_K19_TYPE_GLOBAL_NEW;
		map_size = sizeof(PCBA_DETECT_K19_TYPE_GLOBAL_NEW)/sizeof(board_id_map_t);
	} else if (pcba_type ==  PCBA_K19P_TYPE_GLOBAL) {
		board_id_map = PCBA_DETECT_K19P_TYPE_GLOBAL;
		map_size = sizeof(PCBA_DETECT_K19P_TYPE_GLOBAL)/sizeof(board_id_map_t);
	} else if (pcba_type ==  PCBA_K19_TYPE_CN_FINAL) {
		board_id_map = PCBA_DETECT_K19_TYPE_CN_FINAL;
		map_size = sizeof(PCBA_DETECT_K19_TYPE_CN_FINAL)/sizeof(board_id_map_t);
	} else if (pcba_type ==  PCBA_K19_TYPE_INDIA) {
		board_id_map = PCBA_DETECT_K19_TYPE_INDIA;
		map_size = sizeof(PCBA_DETECT_K19_TYPE_INDIA)/sizeof(board_id_map_t);
	}

	/*Cause we only have just one version,so its just one version */
	while (i < map_size) {
		if ((board_id.voltage >= board_id_map[i].voltage_min) && (board_id.voltage < board_id_map[i].voltage_max)) {
			huaqin_pcba_config = board_id_map[i].version;
			break;
		}
		i++;
	}
	if (i >= map_size) {
		huaqin_pcba_config = PCBA_UNKNOW;
	}
	pr_err("[%s] huaqin_pcba_config: 0x%x\n", __func__, huaqin_pcba_config);
	return true;

}


static int board_id_probe(struct platform_device *pdev)
{
	int ret;

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret) {
		pr_err("[%s] Failed %d!!!\n", __func__, ret);
		return ret;
	}
	read_pcba_config();
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

PCBA_CONFIG get_huaqin_pcba_config(void)
{
	return huaqin_pcba_config;
}
EXPORT_SYMBOL_GPL(get_huaqin_pcba_config);


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

//late_initcall(huaqin_pcba_module_init);   //late initcall

MODULE_AUTHOR("wangqi<wangqi6@huaqin.com>");
MODULE_DESCRIPTION("huaqin sys pcba");
MODULE_LICENSE("GPL");
