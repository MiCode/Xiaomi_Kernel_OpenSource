#include <linux/module.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <soc/qcom/smem.h>
#include <linux/platform_device.h>
#include <linux/qpnp/qpnp-adc.h>
#include "hqsys_pcba.h"
static PCBA_CONFIG huaqin_pcba_config;
static void read_pcba_config_form_smem(void)
{
	PCBA_CONFIG *pcba_config = NULL;
	pcba_config = (PCBA_CONFIG *)smem_find(SMEM_ID_VENDOR1,sizeof(PCBA_CONFIG),0,SMEM_ANY_HOST_FLAG);
	if (pcba_config) {
		printk(KERN_ERR "pcba config =%d.\n",*(pcba_config));
		if (*(pcba_config) > PCBA_UNKNOW && *(pcba_config) < PCBA_END)
		{
			huaqin_pcba_config = *pcba_config;
		}
		else {
			huaqin_pcba_config = PCBA_UNKNOW;
		}
	}
	else {
		printk(KERN_ERR "pcba config fail\n");
		huaqin_pcba_config = PCBA_UNKNOW;
	}
}
PCBA_CONFIG get_huaqin_pcba_config(void)
{
	return huaqin_pcba_config;
}
EXPORT_SYMBOL_GPL(get_huaqin_pcba_config);
static int __init huaqin_pcba_early_init(void)
{
	read_pcba_config_form_smem();
	return 0;
}
subsys_initcall(huaqin_pcba_early_init);
MODULE_AUTHOR("ninjia <nijiayu@huaqin.com>");
MODULE_DESCRIPTION("huaqin sys pcba");
MODULE_LICENSE("GPL");
