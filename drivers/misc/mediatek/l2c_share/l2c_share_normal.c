#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/cpu.h>
#include <linux/sched.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <mach/mt_hotplug_strategy.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/mt_secure_api.h>
#include "l2c_share.h"


static struct device_driver mt_l2c_drv =
{
    .name = "l2c_share",
    .bus = &platform_bus_type,
    .owner = THIS_MODULE,
};


static char *log[] = {
    "borrow L2$",
    "return L2$",   
    "wrong option"
};


void __iomem *mp0_cache_config;
void __iomem *mp1_cache_config;

l2c_share_info g_l2c_share_info;
int is_l2_borrowed;


int IS_L2_BORROWED(void)
{
    return is_l2_borrowed;
}

static int enable_secondary_clusters_pwr(void)
{
	int err = 0;
	
	if(g_l2c_share_info.share_cluster_num == 1)
	{
		pr_notice("L2$ share cluster num is only 1, no needs to enable other cluster's pwr.\n");
	}
	else if(g_l2c_share_info.share_cluster_num == 2)
	{
		spm_mtcmos_ctrl_cpusys1(STA_POWER_ON, 1);
	}
	//else if(TBD...)
	else
	{
		pr_err("[ERROR] Inllegal L2$ share_cluster_num!\n");
		err = -1;
	}

	return err;
}

int disable_secondary_clusters_pwr(void)
{
	int err = 0;
	
	if(g_l2c_share_info.share_cluster_num == 1)
	{
		pr_notice("L2$ share cluster num is only 1, no needs to disable other cluster's pwr.\n");
	}
	else if(g_l2c_share_info.share_cluster_num == 2)
	{
		spm_mtcmos_ctrl_cpusys1(STA_POWER_DOWN, 1);
	}
	//else if(TBD...)
	else
	{
		pr_err("[ERROR] Inllegal L2$ share_cluster_num!\n");
		err = -1;
	}

	return err;
}

int config_L2_size(enum options option)
{
	int ret = 0;
	unsigned long flag;
	
	local_irq_save(flag);
	ret = mt_secure_call(MTK_SIP_KERNEL_L2_SHARING, option, g_l2c_share_info.share_cluster_num, 
		(g_l2c_share_info.cluster_borrow<<16) | (g_l2c_share_info.cluster_return));
	local_irq_restore(flag);

	return ret;
}

int switch_L2(enum options option)
{
	int i, cpu;
	int err = 0;
	int retry=0;    
	u64 t1;
	u64 t2;
	unsigned long mask = (1<<0);

	if(option >= BORROW_NONE) {
		pr_err("wrong option %d\n", option);
		return -1;
	}

	t1 = sched_clock();

	/* bind this process to main cpu */    
	while(sched_setaffinity(0, (struct cpumask*) &mask) < 0)
	{
		pr_err("Could not set cpu 0 affinity for current process(%d).\n", retry);
		retry++;
		if(retry > 100)
		{
			return -1;
		}
	}

	/*disable hot-plug*/
	hps_set_enabled(0);

	is_l2_borrowed = 0;

	for(i=1; i<NR_CPUS; i++)
	{
		if(cpu_online(i))
		{
			err = cpu_down(i);
			if(err < 0)
			{
				pr_err("[L2$ sharing] disable cpu %d failed!\n", i);
				
				hps_set_enabled(1);
				return -1;
			}
		}
	}

	/* disable preemption */
	cpu = get_cpu();
	
	/* enable other clusters' power */
	enable_secondary_clusters_pwr();

	config_L2_size(option);

	if(option == BORROW_L2)
	{
		is_l2_borrowed = 1;        
	}
	else // if(option == RETURN_L2)
	{
		is_l2_borrowed = 0;
		/* Disable other clusters' power */
		disable_secondary_clusters_pwr();
	}

	/*enable hot-plug*/
	hps_set_enabled(1);	
	put_cpu();

	t2 = sched_clock();
    
	if(option == BORROW_L2)
	{
		pr_notice("[%s]: borrow L2$ cost %llu ns\n", __func__, t2 - t1);
	}
	else
	{
		pr_notice("[%s]: return L2$ cost %llu ns\n", __func__, t2 - t1);
	}

	return err;
}

static ssize_t cur_l2c_show(struct device_driver *driver, char *buf)
{
	return 0;
}

static ssize_t cur_l2c_store(struct device_driver *driver, const char *buf,
			     size_t count)
{
	char *p = (char *)buf;
	int option, ret;

	option = simple_strtoul(p, &p, 10);

	if(option >= BORROW_NONE) {
		pr_err("wrong option %d\n", option);
		return count;
	}

	pr_notice("config L2 option: %s\n", log[option]);

	ret = switch_L2(option);

	if (ret < 0)
		pr_err("Config L2 error ret:%d by %s\n", ret, log[option]);
	return count;
}

DRIVER_ATTR(current_l2c, 0644, cur_l2c_show, cur_l2c_store);

/*
 * mt_l2c_init: initialize l2c driver.
 * Always return 0.
 */
int mt_l2c_init(void)
{
	struct device_node *node;
	int ret = 0;
    
	node = of_find_compatible_node(NULL, NULL, "mediatek,l2c_share");
	if(node)
	{
		mp0_cache_config = of_iomap(node, 0);
		mp1_cache_config = of_iomap(node, 1);
		of_property_read_u32(node, "mediatek,l2c-share-cluster-num", &g_l2c_share_info.share_cluster_num);
		of_property_read_u32(node, "mediatek,l2c-cluster-borrow", &g_l2c_share_info.cluster_borrow);
		of_property_read_u32(node, "mediatek,l2c-cluster-return", &g_l2c_share_info.cluster_return);

		ret = driver_register(&mt_l2c_drv);
		if (ret){
			printk("fail to register mt_l2c_drv\n");
		}

		ret = driver_create_file(&mt_l2c_drv, &driver_attr_current_l2c);
		if (ret) {
			pr_err("fail to create l2$ sharing sysfs files\n");
		}
	}
	else
	{
		pr_err("[L2$ sharing] failed to find l2$ node in DT!\n");
		ret =  -1;
	}

	return ret;
}

module_init(mt_l2c_init);

EXPORT_SYMBOL(switch_L2);
EXPORT_SYMBOL(IS_L2_BORROWED);


