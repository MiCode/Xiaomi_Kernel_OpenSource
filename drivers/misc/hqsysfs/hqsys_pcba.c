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


//#define ADC_BOARD_ID_CHANNEL 3

struct board_id_information {
	int adc_channel;
	int voltage;
};

static struct board_id_information board_id;
PCBA_CONFIG huaqin_pcba_config = PCBA_UNKNOW;

//extern char *saved_command_line;
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
static bool read_pcba_config(void);
/*K19A HQ-124114 K19A charger of jeita by wangqi at 2021/4/16 start*/
int hq_selene_pcba_config;
/*K19A HQ-124114 K19A charger of jeita by wangqi at 2021/4/16 end*/
typedef struct {
	int voltage_min;
	int voltage_max;
	PCBA_CONFIG version;
} board_id_map_t;

#if defined(TARGET_PRODUCT_LANCELOT) || defined(TARGET_PRODUCT_SHIVA)
static int pcba_config;
static board_id_map_t PCBA_DETECT_LANCELOT_CN[] = {
	{130, 225, PCBA_J19_P0_1_CN},
	{226, 315, PCBA_J19_P1_CN},
	{316, 405, PCBA_J19_P2_CN},
	{406, 500, PCBA_J19_MP_CN},
};

static board_id_map_t PCBA_DETECT_LANCELOT_CN2[] = {
	{130, 225, PCBA_J19_P0_1_CN},
	{226, 315, PCBA_J19_P1_CN},
	{316, 405, PCBA_J19_P2_CN},
	{406, 500, PCBA_J19_MP_CN_SP01T},
};


static board_id_map_t PCBA_DETECT_LANCELOT_INDIA[] = {
	{130, 225, PCBA_J19_P0_1_INDIA},
	{226, 315, PCBA_J19_P1_INDIA},
	{316, 405, PCBA_J19_P2_INDIA},
	{406, 500, PCBA_J19_MP_INDIA},
};

static board_id_map_t PCBA_DETECT_LANCELOT_GLOBAL[] = {
	{130, 225, PCBA_J19_P0_1_GLOBAL},
	{226, 315, PCBA_J19_P1_GLOBAL},
	{316, 405, PCBA_J19_P2_GLOBAL},
	{406, 500, PCBA_J19_MP_GLOBAL},
};

static board_id_map_t PCBA_DETECT_LANCELOTNFC_GLOBAL[] = {
	{130, 225, PCBA_J19A_P0_1_GLOBAL},
	{226, 315, PCBA_J19A_P1_GLOBAL},
	{316, 405, PCBA_J19A_P2_GLOBAL},
	{406, 500, PCBA_J19A_MP_GLOBAL},
};

static board_id_map_t PCBA_DETECT_POCO_GLOBAL[] = {
	{316, 405, PCBA_J19P_P2_INDIA},
	{406, 500, PCBA_J19P_MP_INDIA},
};

#elif defined(TARGET_PRODUCT_SELENE)

static int selene_pcba_config;
static int selene_pcba_stage;
static int selene_pcba_count;

#else
static const board_id_map_t board_id_map[] = {
	{130, 225, PCBA_J15S_P0_CN},
	{226, 315, PCBA_J15S_P0_INDIA},
	{316, 405, PCBA_J15S_P0_GLOBAL},
	{406, 495, PCBA_J15S_P1_CN},
	{496, 585, PCBA_J15S_P1_INDIA},
	{586, 670, PCBA_J15S_P1_1_CN},
	{671, 765, PCBA_J15S_P1_1_INDIA},
	{766, 855, PCBA_J15N_P1_1_GLOBAL_NFC},
	{856, 945, PCBA_J15S_P2_CN},
	{946, 1030, PCBA_J15S_P2_INDIA},
	{1031, 1125, PCBA_J15S_P2_GLOBAL},
	{1126, 1215, PCBA_J15S_P2_1_GLOBAL},
	{1216, 1305, PCBA_J15S_MP_CN},
	{1306, 1395, PCBA_J15S_MP_INDIA},
};
static const board_id_map_t board_id_map_ext[] = {
	{130, 225, PCBA_J15S_MP_GLOBAL},
	{226, 315, PCBA_J15S_CN_NEW_PA},
};
static const board_id_map_t j15n_board_id_map[] = {
	{1031, 1125, PCBA_J15N_P2_GLOBAL_NFC},
	{1126, 1215, PCBA_J15N_P2_1_GLOBAL_NFC},
};
static const board_id_map_t j15n_board_id_map_ext[] = {
	{130, 225, PCBA_J15N_MP_GLOBAL_NFC},
};
#endif

#if defined(TARGET_PRODUCT_LANCELOT) || defined(TARGET_PRODUCT_SHIVA)

static int __init get_pcba_config(char *p)
{
	char pcba[10];

	strlcpy(pcba, p, sizeof(pcba));

	printk("[%s]: pcba = %s\n", __func__, pcba);

	pcba_config = pcba[0] - '0';

	return 0;
}

early_param("pcba_config", get_pcba_config);

static bool read_pcba_config_j19(void)
{
	int ret = 0;
	int i = 0, map_size = 0;
	int auxadc_voltage = 0;

	struct iio_channel *channel;
	struct device_node *board_id_node;
	struct platform_device *board_id_dev;
	board_id_map_t *board_id_map;

	board_id_node = of_find_node_by_name(NULL, "board_id");
	if (board_id_node == NULL) {
		pr_err("[%s] find board_id node fail \n", __func__);
		return false;
	} else
		pr_err("[%s] find board_id node success %s \n", __func__, board_id_node->name);

	board_id_dev = of_find_device_by_node(board_id_node);
	if (board_id_dev == NULL) {
		pr_err("[%s] find board_id dev fail \n", __func__);
		return false;
	} else
		pr_err("[%s] find board_id dev success %s \n", __func__, board_id_dev->name);

	channel = iio_channel_get(&(board_id_dev->dev), "board_id-channel");
	if (IS_ERR(channel)) {
		ret = PTR_ERR(channel);
		pr_err("[%s] iio channel not found %d\n", __func__, ret);
		return false;
	} else
		pr_err("[%s] get channel success\n", __func__);

	if (channel != NULL)
		ret = iio_read_channel_processed(channel, &auxadc_voltage);
	else {
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
	board_id_map = PCBA_DETECT_LANCELOT_CN;

	pr_err("[%s] read_pcba_config board_id.voltage: %d\n", __func__, board_id.voltage);
	if (pcba_config == PCBA_J19_CN) {
		board_id_map = PCBA_DETECT_LANCELOT_CN;
		map_size = sizeof(PCBA_DETECT_LANCELOT_CN)/sizeof(board_id_map_t);
	} else if (pcba_config == PCBA_J19_CN_SP10T) {
		board_id_map = PCBA_DETECT_LANCELOT_CN2;
		map_size = sizeof(PCBA_DETECT_LANCELOT_CN2)/sizeof(board_id_map_t);
	} else if (pcba_config == PCBA_J19_INDIA) {
		board_id_map = PCBA_DETECT_LANCELOT_INDIA;
		map_size = sizeof(PCBA_DETECT_LANCELOT_INDIA)/sizeof(board_id_map_t);
	} else if (pcba_config == PCBA_J19_GLOBAL) {
		board_id_map = PCBA_DETECT_LANCELOT_GLOBAL;
		map_size = sizeof(PCBA_DETECT_LANCELOT_GLOBAL)/sizeof(board_id_map_t);
	} else if (pcba_config == PCBA_J19A_GLOBAL) {
		board_id_map = PCBA_DETECT_LANCELOTNFC_GLOBAL;
		map_size = sizeof(PCBA_DETECT_LANCELOTNFC_GLOBAL)/sizeof(board_id_map_t);
	} else if (pcba_config == PCBA_J19P_INDIA) {
		board_id_map = PCBA_DETECT_POCO_GLOBAL;
		map_size = sizeof(PCBA_DETECT_POCO_GLOBAL)/sizeof(board_id_map_t);
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
	pr_err("[%s] read_pcba_config huaqin_pcba_config: 0x%x\n", __func__, huaqin_pcba_config);
	return true;
}

#elif defined(TARGET_PRODUCT_SELENE)
/* Huaqin modify for HQ-147481 by liunianliang at 2021/07/27 start */
static int __init get_selene_pcba_config(char *p)
{
	char pcba[10];

	strlcpy(pcba, p, sizeof(pcba));

	if (kstrtoint(pcba, 10, &selene_pcba_config))
		return -1;

	printk("[%s]: pcba config = %d\n", __func__, selene_pcba_config);

	/*K19A HQ-124114 K19A charger of jeita by wangqi at 2021/4/16 start*/
	hq_selene_pcba_config = selene_pcba_config;
	/*K19A HQ-124114 K19A charger of jeita by wangqi at 2021/4/16 end*/

	return 0;
}
early_param("pcba_config", get_selene_pcba_config);

static int __init get_selene_pcba_stage(char *p)
{
	char stage[10];

	strlcpy(stage, p, sizeof(stage));

	if (kstrtoint(stage, 10, &selene_pcba_stage))
		return -1;

	printk("[%s]: pcba stage = %d\n", __func__, selene_pcba_stage);

	return 0;
}
early_param("pcba_stage", get_selene_pcba_stage);

static int __init get_selene_pcba_count(char *p)
{
	char count[10];

	strlcpy(count, p, sizeof(count));

	if (kstrtoint(count, 10, &selene_pcba_count))
		return -1;

	printk("[%s]: pcba count = %d\n", __func__, selene_pcba_count);

	return 0;
}
early_param("pcba_count", get_selene_pcba_count);
/* Huaqin modify for HQ-147481 by liunianliang at 2021/07/27 end */

static bool read_pcba_config_k19a(void)
{
	if (selene_pcba_config == PCBA_UNKNOW) {
		huaqin_pcba_config = PCBA_UNKNOW;
		return false;
	}
	huaqin_pcba_config = (selene_pcba_stage - 1) * selene_pcba_count + selene_pcba_config;
	printk("[%s]: huaqin_pcba_config = %d\n", __func__, huaqin_pcba_config);
	return true;
}

#else

static bool read_pcba_config(void)
{
	int ret = 0;
	int i = 0, map_size = 0;
	int auxadc_voltage;
	int hw_id_gpio, board_id3_gpio;
	/* Huaqin modify for HQ-140352/140366 by liunianliang at 2021/06/15 start */
	int hw_id_gpio_value = 0, board_id3_gpio_value = 0;
	/* Huaqin modify for HQ-140352/140366 by liunianliang at 2021/06/15 end */
	struct iio_channel *channel;
	struct device_node *board_id_node;
	struct platform_device *board_id_dev;

	board_id_node = of_find_node_by_name(NULL, "board_id");
	if (board_id_node == NULL) {
		pr_err("[%s] find board_id node fail \n", __func__);
		return false;
	} else
		pr_err("[%s] find board_id node success %s \n", __func__, board_id_node->name);

	board_id_dev = of_find_device_by_node(board_id_node);
	if (board_id_dev == NULL) {
		pr_err("[%s] find board_id dev fail \n", __func__);
		return false;
	} else
		pr_err("[%s] find board_id dev success %s \n", __func__, board_id_dev->name);

	hw_id_gpio = of_get_named_gpio(board_id_node, "hw_id-gpios", 0);
	if (gpio_is_valid(hw_id_gpio)) {
		ret = devm_gpio_request_one(&board_id_dev->dev, hw_id_gpio, GPIOF_IN, "hw_id");
		if (!ret) {
			hw_id_gpio_value = gpio_get_value(hw_id_gpio);
			pr_err("[%s] get hw_id_gpio %d value: %d\n", __func__, hw_id_gpio, hw_id_gpio_value);
		} else
			pr_err("[%s] Can not request hw_id_gpio : %d\n", __func__, ret);
	}
#if 0
	board_id2_gpio = of_get_named_gpio(board_id_node, "board_id2-gpios", 0);
	if (gpio_is_valid(board_id2_gpio)) {
		ret = devm_gpio_request_one(&board_id_dev->dev, board_id2_gpio, GPIOF_IN, "board_id2");
		if (!ret) {
			ret = gpio_get_value(board_id2_gpio);
			pr_err("[%s] get board_id2_gpio %d value: %d\n", __func__, board_id2_gpio, ret);
		} else
			pr_err("[%s] Can not request board_id2_gpio : %d\n", __func__, ret);
	}
#endif
	board_id3_gpio = of_get_named_gpio(board_id_node, "board_id3-gpios", 0);
	if (gpio_is_valid(board_id3_gpio)) {
		ret = devm_gpio_request_one(&board_id_dev->dev, board_id3_gpio, GPIOF_IN, "board_id3");
		if (!ret) {
			board_id3_gpio_value = gpio_get_value(board_id3_gpio);
			pr_err("[%s] get board_id3_gpio %d value: %d\n", __func__, board_id3_gpio, board_id3_gpio_value);
		} else
			pr_err("[%s] Can not request board_id3_gpio : %d\n", __func__, ret);
	}

	channel = iio_channel_get(&(board_id_dev->dev), "board_id-channel");
	if (IS_ERR(channel)) {
		ret = PTR_ERR(channel);
		pr_err("[%s] iio channel not found %d\n", __func__, ret);
		return false;
	} else
		pr_err("[%s] get channel success\n", __func__);

	if (channel != NULL)
		ret = iio_read_channel_processed(channel, &auxadc_voltage);
	else {
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
	/*Cause we only have just one version,so its just one version */
	if (0 == hw_id_gpio_value && 1 == board_id3_gpio_value) {
		map_size = sizeof(board_id_map)/sizeof(board_id_map_t);
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
	} else if (0 == hw_id_gpio_value && 0 == board_id3_gpio_value) {
		map_size = sizeof(board_id_map_ext)/sizeof(board_id_map_t);
		while (i < map_size) {
			if ((board_id.voltage >= board_id_map_ext[i].voltage_min) && (board_id.voltage < board_id_map_ext[i].voltage_max)) {
				huaqin_pcba_config = board_id_map_ext[i].version;
				break;
			}
			i++;
		}
		if (i >= map_size) {
			huaqin_pcba_config = PCBA_UNKNOW;
		}
	} else if (1 == hw_id_gpio_value && 1 == board_id3_gpio_value) {
		map_size = sizeof(j15n_board_id_map)/sizeof(board_id_map_t);
		while (i < map_size) {
			if ((board_id.voltage >= j15n_board_id_map[i].voltage_min) && (board_id.voltage < j15n_board_id_map[i].voltage_max)) {
				huaqin_pcba_config = j15n_board_id_map[i].version;
				break;
			}
			i++;
		}
		if (i >= map_size) {
			huaqin_pcba_config = PCBA_UNKNOW;
		}
	} else if (1 == hw_id_gpio_value && 0 == board_id3_gpio_value) {
		map_size = sizeof(j15n_board_id_map_ext)/sizeof(board_id_map_t);
		while (i < map_size) {
			if ((board_id.voltage >= j15n_board_id_map_ext[i].voltage_min) && (board_id.voltage < j15n_board_id_map_ext[i].voltage_max)) {
				huaqin_pcba_config = j15n_board_id_map_ext[i].version;
				break;
			}
			i++;
		}
		if (i >= map_size) {
			huaqin_pcba_config = PCBA_UNKNOW;
		}
	}
	pr_err("[%s] huaqin_pcba_config huaqin_pcba_config: 0x%x\n", __func__, huaqin_pcba_config);
	return true;
}
#endif

static int board_id_probe(struct platform_device *pdev)
{
	int ret;

	ret = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (ret) {
		pr_err("[%s] Failed %d!!!\n", __func__, ret);
		return ret;
	}

#if defined(TARGET_PRODUCT_LANCELOT) || defined(TARGET_PRODUCT_SHIVA)
	read_pcba_config_j19();
#elif defined(TARGET_PRODUCT_SELENE)
	read_pcba_config_k19a();
#else
	read_pcba_config();
#endif
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
