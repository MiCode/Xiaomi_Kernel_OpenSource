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
#include "hqsys_pcba.h"


#define ADC_BOARD_ID_CHANNEL 3

struct board_id_information {
	int adc_channel;
	int voltage;
};

static struct board_id_information board_id;
PCBA_CONFIG huaqin_pcba_config=PCBA_UNKNOW;

//extern char *saved_command_line;
extern int IMM_GetOneChannelValue(int dwChannel, int data[4], int *rawdata);
bool read_pcba_config(void);

typedef struct {
	int voltage_min;
	int voltage_max;
	PCBA_CONFIG version;
}board_id_map_t;
static const board_id_map_t board_id_map[] =
{
	{830,970,PCBA_F9_V1_CN},
	{445,565,PCBA_F9_V2_CN},
	{335,440,PCBA_F9_V3_CN},
	{250,330,PCBA_F9_V4_CN},
};

bool read_pcba_config(void)
{
	int data[4] = {0,0,0,0};
	int voltage = 0;
	int ret = 0;
	int i=0,map_size=0;
	board_id.adc_channel = ADC_BOARD_ID_CHANNEL;
	ret = IMM_GetOneChannelValue(board_id.adc_channel,data,&voltage);
	if (0 != ret)
	{
		huaqin_pcba_config=PCBA_UNKNOW;
		printk(KERN_WARNING "init_board_id get ADC error: %d\n", ret);
		return false;
	}
	board_id.voltage = (voltage * 1500) / 4096;
	/*Cause we only have just one version,so its just one version */
	map_size = sizeof(board_id_map)/sizeof(board_id_map_t);
	while(i < map_size)
	{
		if((board_id.voltage >= board_id_map[i].voltage_min)&&(board_id.voltage < board_id_map[i].voltage_max))
		{
			huaqin_pcba_config = board_id_map[i].version;
			break;
		}
		i++;
	}
	if(i >= map_size)
	{
		huaqin_pcba_config = PCBA_UNKNOW;
	}
	return true;
}
PCBA_CONFIG get_huaqin_pcba_config(void)
{
	return huaqin_pcba_config;
}
EXPORT_SYMBOL_GPL(get_huaqin_pcba_config);


static int __init huaqin_pcba_early_init(void)
{
	read_pcba_config();
	return 0;
}

module_init(huaqin_pcba_early_init);//before device_initcall

//late_initcall(huaqin_pcba_module_init);   //late initcall

MODULE_AUTHOR("wangqi<wangqi6@huaqin.com>");
MODULE_DESCRIPTION("huaqin sys pcba");
MODULE_LICENSE("GPL");
